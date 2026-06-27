# vDoom Changelog

## 3.3-alpha-vdoom.23 - 2026-06-27

- Clipped `vk_world_probe` wall segments against the left and right view
  frustum planes instead of only rejecting fully off-screen walls.
- Tightened the probe FOV padding so partially visible debug wall quads no
  longer spill across the foreground.

## 3.3-alpha-vdoom.22 - 2026-06-27

- Clipped `vk_world_probe` wall segments against the camera near plane before
  submitting Vulkan debug geometry.
- Added conservative horizontal FOV rejection so off-screen world-probe walls
  do not become giant foreground slabs.
- Counted only emitted world-probe walls toward the debug wall budget.

## 3.3-alpha-vdoom.21 - 2026-06-27

- Split Vulkan debug geometry into separate scene-marker and world-wall probe
  pipelines.
- Made `vk_world_probe` use an opaque, depth-tested pipeline while keeping
  `vk_scene_probe` as a translucent debug marker.
- Updated `stat renderer` to report scene-probe and world-probe vertex counts
  separately.

## 3.3-alpha-vdoom.20 - 2026-06-27

- Added a Vulkan depth attachment to the GPU presentation render pass and
  swapchain framebuffers.
- Enabled depth testing and depth writes for the Vulkan probe/world geometry
  pipeline while keeping the fullscreen palette presenter depth-neutral.
- Updated the probe projection to emit camera-distance depth values so debug
  wall batches can be depth-tested against each other.

## 3.3-alpha-vdoom.19 - 2026-06-27

- Added `vk_world_probe`, an opt-in Vulkan debug draw that extracts nearby
  one-sided map linedefs into a host-visible vertex buffer as wall quads.
- Reused the Vulkan probe pipeline for the first actual Doom map geometry
  batch while keeping the software-rendered frame as the reference underneath.
- Renamed the renderer stats probe line to `GPU probe` because it can now
  represent scene-marker or map-wall debug geometry.

## 3.3-alpha-vdoom.18 - 2026-06-26

- Changed `vk_scene_probe` to feed 3D Doom-world positions into Vulkan and
  transform them in the vertex shader with a per-frame world-to-clip push
  constant.
- The probe triangle is generated near the active software-renderer camera so
  it can validate Vulkan geometry against the current view.

## 3.3-alpha-vdoom.17 - 2026-06-26

- Changed `vk_scene_probe` from hardcoded shader vertices to a host-visible
  Vulkan vertex buffer populated by the backend and submitted with
  `vkCmdBindVertexBuffers`.
- Updated `stat renderer` to show the active probe vertex count.

## 3.3-alpha-vdoom.16 - 2026-06-26

- Moved the `vk_scene_probe` triangle to a more visible right-center screen
  position, increased its opacity, and print a console confirmation when the
  probe pipeline is created.

## 3.3-alpha-vdoom.15 - 2026-06-26

- Added `vk_scene_probe`, an opt-in Vulkan-owned geometry draw after the
  palette presentation pass. This uses a separate Vulkan graphics pipeline and
  draw call, proving that the backend can draw independent GPU geometry on top
  of the uploaded software frame.
- Updated `stat renderer` to report whether the GPU scene probe pipeline is
  active.

## 3.3-alpha-vdoom.14 - 2026-06-26

- Added `vk_present_sharpness` for the `vk_present_filter 2` sharp-color
  scaler. Lower values behave closer to smooth color bilinear filtering; higher
  values preserve crisper pixel edges.
- Updated `stat renderer` to show the active sharpness value when the
  sharp-color presentation filter is selected.

## 3.3-alpha-vdoom.13 - 2026-06-26

- Added `vk_render_scale`, a Vulkan presentation-source scale control for
  testing upscale quality. Values below `1.0` downsample the completed
  software frame before the Vulkan palette presenter upscales it to the
  swapchain, making `vk_present_filter` differences visible without changing
  the selected window/output resolution.
- Updated `stat renderer` to show the Vulkan present source dimensions in
  addition to the framebuffer, swapchain, and viewport sizes.

## 3.3-alpha-vdoom.12 - 2026-06-26

- Added `vk_present_filter 2`, a Vulkan palette-present smoothing mode that
  blends final palette colors instead of raw 8-bit palette indices. This makes
  smoothing changes more visible while preserving saner color behavior than the
  experimental linear-index sampler.
- Updated `stat renderer` to distinguish `nearest`, `linear index`, and
  `sharp color` presentation filters.

## 3.3-alpha-vdoom.11 - 2026-06-26

- Changed `vk_present_aspect` default to `0`, meaning source-framebuffer
  aspect. This avoids damaging/cropping already-composed widescreen menus at
  the presentation layer. Explicit aspect overrides remain available for
  experiments behind the new `vk_present_force_aspect` opt-in, so previously
  archived 4:3 values do not silently keep breaking the menu.

## 3.3-alpha-vdoom.10 - 2026-06-26

- Fixed Vulkan aspect/integer scale modes squeezing the full widescreen
  software framebuffer into a narrower output rectangle. The palette presenter
  now uses a centered source crop that matches the requested presentation
  aspect before scaling.

## 3.3-alpha-vdoom.9 - 2026-06-26

- Changed Vulkan presentation scale modes to use an explicit configurable
  presentation aspect instead of the already-window-sized software framebuffer.
  This makes `vk_present_scale_mode` visibly affect normal Vulkan fullscreen
  and windowed modes.
- Added `vk_present_aspect`, defaulting to classic 4:3 presentation.

## 3.3-alpha-vdoom.8 - 2026-06-26

- Added aspect-aware Vulkan presentation scaling through
  `vk_present_scale_mode`: `0` stretches, `1` preserves aspect ratio, and `2`
  uses integer scaling when possible.
- Updated the Vulkan palette presenter shader to use push constants for
  source-to-swapchain mapping and border fill.
- Expanded `stat renderer` with the active Vulkan scale mode and presentation
  viewport.

## 3.3-alpha-vdoom.7 - 2026-06-26

- Added Vulkan timestamp-query instrumentation so `stat renderer` can report
  measured GPU frame time for the current presentation path.
- Enabled `VK_EXT_memory_budget` when supported and report live local VRAM
  usage/budget alongside the physical local heap size.
- Added `vk_present_filter`: `0` keeps the default nearest presentation,
  while `1` enables linear filtering for the Vulkan palette presenter.

## 3.3-alpha-vdoom.6 - 2026-06-26

- Fixed the Vulkan GPU palette presenter drawing the software framebuffer
  upside-down by correcting the fullscreen shader texture coordinates.

## 3.3-alpha-vdoom.5 - 2026-06-26

- Added the first Vulkan GPU presentation path: the software framebuffer is
  uploaded as an 8-bit source image plus RGBA palette image, then presented by
  a fullscreen Vulkan shader.
- Kept the previous CPU-expanded transfer presenter as a fallback if shader
  presentation resources cannot be created.
- Added readable GLSL shader sources and embedded SPIR-V for the Vulkan palette
  presenter.
- `stat renderer` now reports whether Vulkan presentation is using the GPU
  palette shader or the transfer fallback.

## 3.3-alpha-vdoom.4 - 2026-06-26

- Fixed Vulkan framebuffer creation so fresh modes preserve the requested
  fullscreen state instead of falling back to windowed after resolution changes.
- Hardened Vulkan swapchain recreation around client-size changes, minimized
  windows, and out-of-date/suboptimal acquire/present results.
- Expanded `stat renderer` with Vulkan swapchain recreation and out-of-date
  counters.

## 3.3-alpha-vdoom.3 - 2026-06-26

- Added `stat renderer`, a backend-neutral renderer diagnostics overlay.
- The renderer overlay reports the selected renderer, engine version,
  framebuffer mode, process RAM, and Vulkan device/swapchain/upload-buffer
  details when Vulkan is active.
- Vulkan true GPU frame time and live VRAM usage are documented as future
  instrumentation work rather than estimated values.

## 3.3-alpha-vdoom.2 - 2026-06-26

- Fixed Vulkan fullscreen state drift where the menu/config could report
  fullscreen while the Win32 window was still running as a framed window.
- Vulkan fullscreen currently uses borderless fullscreen and rebuilds the
  swapchain when toggling between fullscreen and windowed mode.

## 3.3-alpha-vdoom.1 - 2026-06-26

- Renamed the user-facing source port identity from Zandronum to vDoom while
  keeping the repository name as InfiniDoom.
- Changed the built executable and base data package names to `vdoom.exe` and
  `vdoom.pk3`.
- Added an OpenAL sound backend with FluidSynth MIDI support for the long-term
  native audio path.
- Added an initial Vulkan renderer/video backend selected with `vid_renderer 2`.
  The current Vulkan path presents the existing software framebuffer through a
  Win32 Vulkan swapchain.
- Made Vulkan the default renderer for new configurations and added its renderer
  menu label.
- Vulkan fullscreen is experimental and implemented as borderless fullscreen.
