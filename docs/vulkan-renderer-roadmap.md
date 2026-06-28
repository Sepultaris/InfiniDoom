# vDoom Vulkan Renderer Roadmap

This note records the working plan for moving vDoom from the current Vulkan
presentation/probe path toward a real GPU hardware renderer for Doom maps.
UZDoom is the primary reference for renderer architecture, but direct code
copying should be avoided unless the copied file's license is audited and its
notices are preserved.

## Current vDoom State

The current Vulkan path is a useful bring-up scaffold, not a mature Doom
renderer:

- `src/vulkan/vk_renderer.cpp` dynamically loads Vulkan, creates the Win32
  surface/swapchain, uploads the software framebuffer and palette, and presents
  through a fullscreen shader.
- `vk_draw_world` adds diagnostic GPU-owned walls/flats, but those are extracted
  directly from Doom map data inside the Vulkan backend.
- Textures are packed into a temporary per-frame atlas.
- Plane rendering still lacks the complete hardware-renderer scene model:
  render-sector fake flats, exact wall opening logic, flood planes, sector-stack
  portals, 3D floors, draw-list ordering, material state, and the full
  missing-texture render-hack pipeline.

The repeated floor/ceiling holes are a symptom of this architecture. We have
been asking a backend bootstrapper to infer Doom hardware-renderer visibility
rules on its own.

## What UZDoom Does Differently

UZDoom's Vulkan renderer is not a standalone Vulkan-specific Doom extractor.
It has two major layers:

1. Hardware scene construction

   The `hwrenderer` code walks the BSP, clips visible segs, processes walls,
   flats, sprites, portals, decals, dynamic lights, render hacks, and 3D floors,
   then stores work in renderer-independent draw lists.

   Key reference files:

   - `src/rendering/hwrenderer/hw_entrypoint.cpp`
   - `src/rendering/hwrenderer/scene/hw_drawinfo.*`
   - `src/rendering/hwrenderer/scene/hw_bsp.cpp`
   - `src/rendering/hwrenderer/scene/hw_walls.cpp`
   - `src/rendering/hwrenderer/scene/hw_flats.cpp`
   - `src/rendering/hwrenderer/scene/hw_renderhacks.cpp`
   - `src/rendering/hwrenderer/hw_vertexbuilder.cpp`
   - `src/common/rendering/hwrenderer/data/flatvertices.*`

2. Render backend execution

   The hardware scene layer emits state changes and draw calls through an
   abstract `FRenderState`. Vulkan implements that abstraction with pipeline
   caches, descriptor managers, stream buffers, render passes, texture objects,
   and hardware buffers.

   Key reference files:

   - `src/common/rendering/hwrenderer/data/hw_renderstate.h`
   - `src/common/rendering/vulkan/system/vk_renderdevice.*`
   - `src/common/rendering/vulkan/renderer/vk_renderstate.*`
   - `src/common/rendering/vulkan/renderer/vk_renderpass.*`
   - `src/common/rendering/vulkan/renderer/vk_descriptorset.*`
   - `src/common/rendering/vulkan/textures/vk_hwtexture.*`
   - `libraries/ZVulkan/*`

The important lesson: Vulkan receives already-correct draw lists. It does not
rediscover Doom's rendering rules from raw sectors every frame.

## Target Architecture For vDoom

vDoom should grow a hardware-renderer layer that can feed Vulkan first and other
future GPU backends later.

### 1. Split Backend From Scene Building

Create a renderer-owned scene path separate from `src/vulkan/vk_renderer.cpp`.
The Vulkan file should stop being responsible for Doom map interpretation.

Proposed modules:

- `src/hwrenderer/` or `src/vdoom/render/`
  - `vd_hw_drawinfo.*`
  - `vd_hw_bsp.*`
  - `vd_hw_walls.*`
  - `vd_hw_flats.*`
  - `vd_hw_renderhacks.*`
  - `vd_hw_drawlist.*`
  - `vd_hw_material.*`
- `src/vulkan/`
  - Vulkan instance/device/swapchain
  - render pass/pipeline/cache
  - buffers/textures/descriptors
  - implementation of the backend draw API

The first API boundary can be deliberately small:

- set viewport/projection
- set material/texture
- set depth/blend/alpha/cull state
- bind vertex/index buffers
- draw arrays / draw indexed

### 2. Reuse Doom's Visibility Model Instead Of Rebuilding It In Vulkan

Build visible scene data through the same concepts UZDoom uses:

- BSP traversal decides visible subsectors and segs.
- Wall processing owns upper/lower/mid wall rules.
- Flat processing owns floor/ceiling plane visibility.
- Render hacks run after normal BSP collection:
  - missing upper/lower texture tracking
  - other floor/ceiling planes
  - flood planes for unresolved missing textures
  - hacked subsectors/deep-water style cases
- Draw lists sort and split solid, masked, translucent, portal, model, sprite,
  and decal work.

This should replace the current `vk_draw_world` extraction path, not extend it
forever.

### 3. Build Persistent Geometry Buffers

The renderer should have persistent level geometry buffers instead of
rebuilding large host-visible vertex streams every frame.

Initial geometry buffers:

- Static flat vertex/index buffer:
  - one floor and one ceiling section per sector/subsector
  - GL/UZDoom fan triangulation for ordinary subsectors
  - non-convex/hole support only where the node data requires it
- Static wall segment base data:
  - endpoints, sidedef ids, line flags, sector refs
  - dynamic top/bottom heights evaluated per frame or updated when sectors move
- Dynamic stream buffer:
  - moving sector planes
  - temporary render-hack vertices
  - sprites, decals, particles
  - sky and portal helper geometry

The goal is to make per-frame work mostly "select and draw ranges", not
"rebuild the world".

### 4. Replace The Temporary Texture Atlas With Materials

The current atlas is fine for proof-of-life, but it cannot support modern Doom
content.

Needed material system:

- Texture cache keyed by `FTextureID` / game texture object.
- GPU images with mipmaps, sampler state, clamp/wrap state, alpha mode, and
  invalidation.
- Separate paths for:
  - wall textures
  - flats
  - sprites
  - brightmaps
  - fullbright textures
  - translations/colormaps
  - masked textures
  - sky textures
- Descriptor set strategy that supports many materials without rebuilding
  descriptors per wall.

The simplest Vulkan milestone is one texture array or descriptor table for
world materials, then a material id in vertices/draw constants.

### 5. Implement Correct Pass Ordering

Target pass order should resemble UZDoom's hardware renderer:

1. Clear scene color/depth/stencil.
2. Solid walls.
3. Solid flats.
4. Masked walls/flats with alpha threshold.
5. Decals.
6. Models and sprites.
7. Translucent sorted draw list.
8. Portals/mirrors/sky special passes where applicable.
9. Postprocess.
10. 2D HUD/menu composition.
11. Swapchain present.

The current palette-presenter can remain as a fallback/debug overlay, but the
Vulkan hardware renderer should eventually render the scene directly into an
RGBA scene target.

### 6. Bring Over Render Hacks As Concepts, Not Backend Guesswork

The immediate floor/ceiling HOM issue should be handled by a real render-hack
collection stage. UZDoom's order is the model:

1. BSP scene creation collects ordinary visible walls/flats and records missing
   upper/lower texture candidates.
2. `HandleMissingTextures` tries to create other ceiling/floor planes when a
   connected fake plane can close the hole.
3. `PrepareUnhandledMissingTextures` creates flood-plane helper geometry for
   gaps that cannot be closed as normal other planes.
4. `DispatchRenderHacks` submits those plane-hack/flood-hack draw lists through
   the normal flat drawing path.

vDoom's current `.87` approximation only scans raw segs in the Vulkan backend.
The correct next step is to move those data structures into a hardware
draw-info stage and have Vulkan merely draw the resulting ranges.

### 7. Add Diagnostics That Match The New Layers

Keep `stat renderer`, but split counters by layer:

- scene collection:
  - visible subsectors
  - visible segs
  - wall pieces
  - normal flat sections
  - missing texture candidates
  - other planes
  - flood planes
  - sprites/decals/models
- backend:
  - draw calls
  - pipeline switches
  - descriptor writes
  - uploaded bytes
  - GPU frame time
  - VRAM usage/budget

This will make failures obvious: either the scene layer did not produce a
surface, or the backend failed to draw it.

## Execution Plan

### Phase 0: Freeze The Current Diagnostic Path

Keep `vk_draw_world`, `vk_world_probe`, and `vk_hide_software_frame` as debug
tools. Stop trying to make the one-file Vulkan extractor mature. It has done
its job: proving Vulkan presentation, GPU geometry, textures, depth, and basic
stats.

Deliverables:

- Add comments/docs labeling current world draw as experimental.
- Keep it available for A/B testing.

### Phase 1: Minimal Hardware Scene Layer

Create a small vDoom hardware scene collector that initially supports Doom 1
style maps:

- BSP traversal.
- Visible one-sided and two-sided wall pieces.
- Sector floor/ceiling flats.
- Draw lists for solid walls and solid flats.
- A backend-agnostic vertex format.

Vulkan consumes those lists and draws them with existing simple pipelines.

Success criteria:

- E1M1 renders walls/floors/ceilings without `vk_hide_software_frame` showing
  HOM holes.
- `stat renderer` reports scene-layer wall/flat counts separately from Vulkan
  draw calls.

### Phase 2: UZDoom-Style Missing Texture And Flood Plane Handling

Move missing upper/lower texture tracking out of Vulkan and into the scene
collector.

Deliverables:

- `MissingUpperTextures`, `MissingLowerTextures`, `MissingUpperSegs`,
  `MissingLowerSegs` equivalents.
- Other floor/ceiling plane lists keyed by sector.
- Flood plane helper geometry for unresolved gaps.
- Dedicated counters for other/flood planes.

Success criteria:

- The current black/HOM floor and ceiling wedges disappear with the software
  frame hidden.
- Menus no longer show through missing world pixels.

### Phase 3: Persistent Flat Mesh And Indexed Draws

Create a persistent flat mesh at map load, closer to UZDoom's
`hw_vertexbuilder`/`flatvertices` path.

Deliverables:

- Per-sector/subsector flat ranges.
- Static vertex/index buffers.
- Dynamic update path for moving/sloped sectors.
- Vulkan indexed draw submission.

Success criteria:

- Flat vertices are not rebuilt wholesale every frame.
- Animated/moving floors still update correctly.

### Phase 4: Wall Piece Parity

Replace temporary wall section extraction with a wall processor that understands
the full wall rules:

- top/bottom/mid textures
- pegging
- wrap flags
- missing texture gap rules
- slopes
- fake flats
- 3D floors
- masked midtextures
- fog boundaries
- mirror/portal lines

Success criteria:

- Doom, Doom II, and common Boom/ZDoom maps render structurally correctly.

### Phase 5: Materials And Texture Cache

Replace the temporary atlas with persistent GPU materials.

Deliverables:

- Texture upload/cache/invalidation.
- Sampler state.
- Alpha/masked handling.
- Brightmap/fullbright support.
- Texture animations.
- Material ids or descriptor indexing.

Success criteria:

- Large maps/mods no longer hit atlas slot limits.
- Texture filtering and smoothing settings visibly affect native GPU-rendered
  world textures, not just the software-frame presenter.

### Phase 6: Sprites, Decals, Models, And Translucency

Add non-wall/non-flat scene objects:

- actor sprites
- weapon sprites
- particles
- decals
- models if supported by this fork
- sorted translucent draw list

Success criteria:

- Gameplay visuals are complete enough to play without the software renderer
  behind the scene.

### Phase 7: Portals, Mirrors, Skies, 3D Floors, Dynamic Lights

Add the higher-end hardware renderer features:

- sky dome / skybox handling
- mirrors and line portals
- sector portals
- stacked sectors
- 3D floors
- dynamic lights
- glow/gradient/split planes
- shadow/light buffers later

Success criteria:

- Hardware-rendered scenes behave like established GZDoom/UZDoom maps rather
  than just vanilla Doom maps.

### Phase 8: Modern Rendering Features

Once correctness is no longer fragile, add vDoom-specific modern features:

- high-quality internal resolution scaling
- postprocess chain
- TAA/FSR-like upscaling experiments
- GPU timing/profiling UI
- bindless or descriptor indexing if available
- optional lightmaps or clustered lighting experiments
- renderer feature presets

## Immediate Next Task

The next code step should be Phase 1, not another `.87`-style patch. Build a
minimal `HWDrawInfo`-like scene collection layer in vDoom and have Vulkan draw
from that layer. The current floor/ceiling bug should then be fixed in Phase 2
by implementing missing-texture and flood-plane lists in the scene collector.

This is slower than patching one more wedge case, but it puts the work on the
same axis as UZDoom's proven renderer: collect correct Doom surfaces first,
then make Vulkan draw them efficiently.
