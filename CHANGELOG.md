# vDoom Changelog

## 3.3-alpha-vdoom.79 - 2026-06-28

- Reintroduced near-plane clipping for temporary Vulkan flat triangles so
  floor/ceiling geometry crossing the camera does not leave undrawn Hall-effect
  wedges in the native world pass.
- Restored UZDoom-style flat triangulation policy for ordinary subsectors:
  convex flats use the subsector fan order, with ear clipping reserved for
  non-convex flat point loops.

## 3.3-alpha-vdoom.78 - 2026-06-28

- Changed the temporary Vulkan world pass to submit floor and ceiling flats
  before textured walls, so early wall quads cannot pre-fill depth over flat
  coverage while the native geometry path is still being brought up.
- Keeps `.77` triangle-list flat submission and the `.76` masked-texel discard.

## 3.3-alpha-vdoom.77 - 2026-06-28

- Changed active Vulkan floor/ceiling submission back to triangle-list drawing
  and enabled ear-clipping triangulation for subsector flat polygons.
- Keeps the `.76` masked-texel discard and view-local BSP collection while
  removing the assumption that each subsector can be safely drawn as a fan.

## 3.3-alpha-vdoom.76 - 2026-06-28

- Added alpha-test discard to the Vulkan world texture fragment shader so
  transparent texels no longer draw as opaque black depth-writing pixels.
- Restored active flat collection to the view-local BSP path from `.74` after
  the sector scan proved too broad for the temporary renderer budget.

## 3.3-alpha-vdoom.75 - 2026-06-28

- Changed the temporary Vulkan flat collector to gather floor and ceiling fans
  from sector-owned subsector lists instead of the bespoke BSP leaf walk.
- This follows the established GL flat pass more closely while keeping the
  Vulkan fan pipeline from `.74`.

## 3.3-alpha-vdoom.74 - 2026-06-28

- Added a dedicated Vulkan flat texture pipeline using `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN`.
- Changed active Vulkan floor/ceiling submission to record one GPU triangle fan
  per subsector plane, matching the established GL flat draw primitive more
  closely than CPU-expanded triangle lists.

## 3.3-alpha-vdoom.73 - 2026-06-28

- Changed the temporary Vulkan world draw order to submit textured walls before
  floor and ceiling flats.
- This uses wall depth as the stable occluder while diagnosing whether flat
  wedges are caused by depth-order interactions rather than polygon coverage.

## 3.3-alpha-vdoom.72 - 2026-06-28

- Removed the temporary CPU near-plane clipping from Vulkan flat triangles.
- Restored flat triangle emission to direct three-vertex fan conversion so
  floor/ceiling geometry reaches Vulkan like the established GL/UZDoom flat
  path, leaving clipping to the GPU projection.

## 3.3-alpha-vdoom.71 - 2026-06-28

- Returned the temporary Vulkan flat polygon source to the proven
  GL/UZDoom-style subsector vertex order, using each seg's `v1` directly.
- Disabled the speculative flat loop reconstruction and ear-clipping path from
  active floor/ceiling submission so wedge diagnosis follows the established
  subsector fan model.

## 3.3-alpha-vdoom.70 - 2026-06-28

- Made `vk_hide_software_frame` fully suppress the old software framebuffer
  whenever Vulkan world geometry is available, including while menus are open.
- This keeps menu and old-renderer pixels from showing through missing Vulkan
  floor/ceiling coverage while diagnosing the temporary world renderer.

## 3.3-alpha-vdoom.69 - 2026-06-28

- Added near-plane clipping for temporary Vulkan floor and ceiling triangles
  before submission, matching the wall path's need to avoid camera-crossing
  geometry in the provisional projection.
- Raised the temporary flat vertex budget estimate because a clipped flat
  triangle can become two submitted triangles.

## 3.3-alpha-vdoom.68 - 2026-06-28

- Extended temporary Vulkan flat loop reconstruction so subsector edge chains
  can continue through reversed segs when a stored seg points back toward the
  current vertex.
- Restored ear-clipping triangulation for ordered non-convex flat loops, with
  a center-fan fallback if the diagnostic triangulator cannot find a valid ear.

## 3.3-alpha-vdoom.67 - 2026-06-28

- Rebuilt the temporary Vulkan flat polygon source so subsector vertices are
  ordered by following each seg's `v1 -> v2` edge loop before triangulation.
- Returned active flat triangulation to a boundary fan after the center-fan
  test proved the missing floor and ceiling wedges are tied to polygon
  construction rather than texture lookup.

## 3.3-alpha-vdoom.66 - 2026-06-28

- Changed the temporary Vulkan flat triangulation from a boundary-vertex fan
  to a center fan per BSP subsector so every subsector edge contributes a
  triangle during floor and ceiling coverage testing.
- Raised the flat vertex budget check to match the center-fan triangle count,
  avoiding accidental under-budgeting while diagnosing missing flat wedges.

## 3.3-alpha-vdoom.65 - 2026-06-28

- Restored the temporary Vulkan flat submission path to BSP-visible subsector
  order after the sector sweep checkpoint suppressed floors and ceilings
  completely on test maps.
- Kept the sector-scan helper code available for comparison, but removed it
  from the active draw path so flat bring-up returns to the last visible
  baseline before rebuilding the clipping/triangulation step.

## 3.3-alpha-vdoom.64 - 2026-06-28

- Loosened the temporary Vulkan sector-flat checkpoint so sector floors and
  ceilings are submitted without the overly broad camera-XY sector cull that
  could suppress the entire flat pass.
- Removed a redundant sector-level flat texture pre-check; texture and sky
  rejection now happens at the actual submitted plane.

## 3.3-alpha-vdoom.63 - 2026-06-28

- Changed temporary Vulkan floor and ceiling submission from BSP-leaf flat
  emission to a sector-based pass that draws each visible sector plane through
  its sector-owned subsector list, matching the shape of the established
  OpenGL flat batching path more closely.
- Kept the existing BSP wall submission path unchanged so this checkpoint
  isolates flat coverage from wall rendering.

## 3.3-alpha-vdoom.62 - 2026-06-28

- Expanded the temporary Vulkan world texture atlas from 64 to 256 slots so
  the combined wall, floor, and ceiling bring-up path has enough room for
  larger visible texture sets.
- Added world-atlas occupancy to `stat renderer` so black or missing Vulkan
  world regions can be checked against atlas pressure during testing.

## 3.3-alpha-vdoom.61 - 2026-06-28

- Changed temporary Vulkan flat placement to use the subsector's real sector
  for floor and ceiling plane height, while preserving the render sector for
  flat texture selection, offsets, scale, and lighting.
- This follows the established hardware flat geometry shape more closely and
  avoids letting render-sector effects move the base floor/ceiling mesh.

## 3.3-alpha-vdoom.60 - 2026-06-28

- Raised the temporary Vulkan flat subsector submission cap from 1024 to 4096
  so dense maps do not leave later BSP floor and ceiling leaves unsubmitted.

## 3.3-alpha-vdoom.59 - 2026-06-28

- Removed the temporary camera-position floor/ceiling eligibility test from
  Vulkan flat submission. Visible BSP subsectors now submit textured floor and
  ceiling planes directly instead of dropping planes based on the viewer's XY
  position against the sector plane.

## 3.3-alpha-vdoom.58 - 2026-06-28

- Added `vk_debug_flat_colors`, a Vulkan flat diagnostic mode that tints
  submitted floor and ceiling subsectors with stable solid colors.
- Split Vulkan flat rejection stats into degenerate, texture, build, budget,
  range, and oversized counts so missing floor/ceiling regions can be traced
  to skipped subsectors versus malformed submitted triangles.

## 3.3-alpha-vdoom.57 - 2026-06-28

- Tightened the temporary Vulkan flat builder to match the established
  subsector fan order for normal convex floor and ceiling planes.
- Enabled the existing ear-clipping fallback only for non-convex flat point
  sequences, keeping the UZDoom-style normal path while handling pathological
  subsector shapes more robustly.

## 3.3-alpha-vdoom.56 - 2026-06-28

- Changed the temporary Vulkan wall submission to walk BSP subsector segs
  instead of scanning global linedefs, so wall and flat geometry now come from
  the same map-space traversal used by the established hardware renderer shape.
- Kept the older linedef wall scan only as a fallback for maps without
  subsector data.

## 3.3-alpha-vdoom.55 - 2026-06-28

- Changed temporary Vulkan floor/ceiling submission from per-plane
  `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN` draws to explicit triangle-list geometry
  using the same subsector fan index order that UZDoom's flat vertex builder
  emits into its indexed flat batches.
- Removed the temporary BSP clipper gate from Vulkan flat submission because
  walls are not yet submitted from the same visible seg walk; using that
  incomplete clipper caused visible movement artifacts.

## 3.3-alpha-vdoom.54 - 2026-06-28

- Fixed `Clipper` member initialization so non-global clipper instances start
  with a null silhouette list instead of relying on static zero-initialization.

## 3.3-alpha-vdoom.53 - 2026-06-28

- Hardened the temporary Vulkan BSP clipper experiment against partial seg and
  subsector state encountered during map startup.

## 3.3-alpha-vdoom.52 - 2026-06-28

- Added an experimental BSP clipper pass for Vulkan flat visibility using the
  existing OpenGL clipper shape. This was later removed from the temporary flat
  path because the wall renderer was not yet driven by the same visible seg
  traversal.

## 3.3-alpha-vdoom.51 - 2026-06-28

- Moved temporary Vulkan flat rendering to a dedicated `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN`
  pipeline with one draw range per submitted floor or ceiling plane, matching
  the established OpenGL/UZDoom flat submission model much more directly.
- Split GPU world submission so flats draw through the fan pipeline and wall
  sections continue to draw through the existing triangle-list texture pipeline.

## 3.3-alpha-vdoom.50 - 2026-06-28

- Kept the temporary Vulkan flat path on the UZDoom/OpenGL-style triangle fan
  for convex subsectors, but now detects non-convex subsector polygons and uses
  the local triangulator only for those fallback cases.
- Made flat triangle append failures propagate back to the flat draw accounting
  instead of counting a flat as drawn when the shared Vulkan vertex budget
  prevented one of its triangles from being emitted.

## 3.3-alpha-vdoom.49 - 2026-06-28

- Changed the temporary Vulkan flat builder to preserve each subsector's
  ordered `firstline[].v1` vertex stream exactly, matching the established
  OpenGL/UZDoom normal-subsector triangle-fan path instead of removing
  duplicate points before fan emission.
- Kept this as a focused geometry submission change so remaining flat holes can
  be tested against the renderer's real subsector data rather than a cleaned-up
  polygon approximation.

## 3.3-alpha-vdoom.48 - 2026-06-28

- Replaced the temporary Vulkan flat pass's map-array-order subsector scan with
  a front-to-back BSP subsector walk, matching the way the established hardware
  renderers discover flat geometry more closely.
- Kept the old distance-limited flat scan only as a fallback for maps without
  BSP nodes.

## 3.3-alpha-vdoom.47 - 2026-06-28

- Matched the temporary Vulkan flat submission more closely to the proven
  UZDoom/OpenGL hardware path by drawing floors only when the viewpoint is above
  the floor plane, and ceilings only when the viewpoint is below the ceiling
  plane.
- Removed the speculative CPU-side flat frustum clipper so ordinary Vulkan flats
  are submitted as ordered triangle-fan triangles again.

## 3.3-alpha-vdoom.46 - 2026-06-28

- Extended Vulkan flat clipping to the left and right view-frustum planes, so
  large floor and ceiling triangles are clipped in the same horizontal space as
  the temporary wall draw path before upload.
- Expanded the temporary Vulkan flat vertex budget again for flats split by
  side-frustum clipping.

## 3.3-alpha-vdoom.45 - 2026-06-28

- Added Vulkan near-plane clipping for floor and ceiling triangles so flat
  polygons crossing the camera plane are split before projection instead of
  relying on backend clipping.
- Expanded the temporary Vulkan flat vertex budget to cover clipped flat
  triangles that split into two triangles.

## 3.3-alpha-vdoom.44 - 2026-06-28

- Added `vk_debug_solid_flats` as a Vulkan flat diagnostic mode. When enabled,
  floors and ceilings use constant atlas tiles and stable UVs while walls stay
  textured, making it easier to tell texture/UV artifacts from real geometry
  holes.
- `stat renderer` now reports whether Vulkan flats are using atlas textures or
  the solid diagnostic mode.

## 3.3-alpha-vdoom.43 - 2026-06-27

- Expanded the temporary Vulkan flat draw coverage from 256 nearby subsectors
  to 1024 subsectors, allowed larger subsector polygons, and widened the
  debug pass range so floors and ceilings are less likely to be omitted by the
  bring-up sampling limits.
- Added `stat renderer` flat diagnostics for drawn flats plus range, large
  polygon, and vertex-budget skips.

## 3.3-alpha-vdoom.42 - 2026-06-27

- Fixed the Vulkan flat triangulator leaving partial floor/ceiling polygons
  behind when ear clipping stalled on awkward or collinear subsector vertices.
- Matched the established OpenGL and UZDoom behavior more closely: normal
  subsectors now draw as ordered triangle fans, with the local ear-clipping
  helper kept all-or-nothing for future hole-subsector support.

## 3.3-alpha-vdoom.41 - 2026-06-27

- Reworked the Vulkan `vk_draw_world` flat pass to follow the established
  hardware renderer model: ordered subsector vertices are triangulated into
  floor and ceiling polygons instead of drawing center-to-seg wedges.
- Added a small ear-clipping fallback for non-convex subsector polygons,
  mirroring the UZDoom/GZDoom approach used for rare hole subsectors without
  importing their renderer code.
- Increased the temporary Vulkan flat debug budget so nearby floors and
  ceilings are less likely to disappear while the GPU-owned world path grows.

## 3.3-alpha-vdoom.40 - 2026-06-27

- Fixed `vk_hide_software_frame` hiding the menu when no Vulkan world is
  available or when the menu is active.
- Changed first-pass Vulkan flat triangulation from a subsector vertex fan to
  center-to-seg triangles, reducing missing floor and ceiling pieces caused by
  mixed seg ordering.

## 3.3-alpha-vdoom.39 - 2026-06-27

- Added first-pass Vulkan floor and ceiling flat rendering to `vk_draw_world`.
- Nearby subsectors now emit depth-tested floor and indoor ceiling triangles
  using their sector flat textures in the temporary Vulkan atlas.
- Sky flats are skipped for now so outdoor ceilings stay black in
  `vk_hide_software_frame` isolation mode until a Vulkan sky path exists.

## 3.3-alpha-vdoom.38 - 2026-06-27

- Added `vk_hide_software_frame` for Vulkan renderer bring-up.
- When enabled, Vulkan presentation clears the swapchain but skips drawing the
  old paletted software framebuffer, making GPU-owned world geometry visible
  against black for easier renderer debugging.
- `stat renderer` now reports whether the software frame is visible or hidden.

## 3.3-alpha-vdoom.37 - 2026-06-27

- Added two-sided wall section extraction to the Vulkan `vk_draw_world` path.
- The Vulkan wall draw now emits solid one-sided middle walls plus upper and
  lower sections for two-sided height differences, using top/bottom sidedef
  textures when present.
- Added first-pass wall pegging for Vulkan wall UVs so upper, lower, and
  one-sided middle textures anchor more like Doom walls.

## 3.3-alpha-vdoom.36 - 2026-06-27

- Fixed camera-dependent wall texture seams in the Vulkan `vk_draw_world`
  path by moving texture repeat wrapping into the wall fragment shader.
- Wall vertices now carry continuous wall-space UVs plus their atlas rectangle,
  so clipping the visible wall segment no longer decides where atlas seams
  appear while turning.

## 3.3-alpha-vdoom.35 - 2026-06-27

- Changed `vk_draw_world` from CPU-colored wall cells to a sampled Vulkan wall
  texture path backed by a temporary texture atlas.
- Added a dedicated wall-texture shader pair, descriptor set, atlas image
  upload, and depth-tested draw pipeline separate from the green
  `vk_world_probe` diagnostic path.
- Kept the first atlas milestone intentionally simple: nearby one-sided wall
  mid textures are packed into 128x128 slots each frame, with repeated UVs
  approximated through subdivided wall cells while the renderer grows toward a
  persistent material cache.

## 3.3-alpha-vdoom.32 - 2026-06-27

- Removed the remaining live Vulkan world-basis calibration CVars after
  `vk_clip_yaw_sign 1` could also crash the client.
- Restored the Vulkan diagnostic world/probe basis to the last non-crashing
  fixed setup while we continue rotation correction in code-reviewed builds.

## 3.3-alpha-vdoom.31 - 2026-06-27

- Removed the live `vk_clip_side_sign` calibration CVar after it could crash
  the Vulkan diagnostic world path when changed in-game.
- Restored the projection side row to a fixed value and left only the safer
  yaw-sign controls exposed through `stat renderer`.

## 3.3-alpha-vdoom.30 - 2026-06-27

- Added temporary Vulkan world-basis calibration CVars:
  `vk_world_yaw_sign`, `vk_clip_yaw_sign`, and `vk_clip_side_sign`.
- Defaulted the calibration signs to the last known front-visible setup while
  exposing the active values through `stat renderer` for faster test feedback.
- Restored matching default extraction/projection yaw signs so the probe should
  start in front again before trying alternate turn-direction combinations.

## 3.3-alpha-vdoom.29 - 2026-06-27

- Restored the CPU-side Vulkan world/probe extraction yaw to the known-good
  `180 - viewangle` path so forward/back and strafe selection no longer render
  the world behind the player.
- Kept the shared Vulkan world-to-clip transform on `180 + viewangle`, limiting
  the turn-direction correction to projection instead of world extraction.

## 3.3-alpha-vdoom.28 - 2026-06-27

- Switched the Vulkan world/probe yaw mapping from `180 - viewangle` to
  `180 + viewangle` so GPU-drawn world geometry follows the software renderer
  when turning the camera.
- Kept the corrected lateral handedness from `.27`, preserving forward/back and
  strafe alignment while fixing camera rotation direction.

## 3.3-alpha-vdoom.27 - 2026-06-27

- Flipped the Vulkan probe/world right-vector basis so strafing left and right
  moves GPU-drawn world geometry in the same lateral direction as the software
  renderer.
- Updated the shared world-to-clip side row to match the corrected lateral
  handedness.

## 3.3-alpha-vdoom.26 - 2026-06-27

- Rotated the Vulkan world/probe yaw basis by 180 degrees so GPU world
  geometry renders in front of the player instead of behind the camera.
- Applied the corrected basis to world draw, world probe, scene probe, and the
  shared probe world-to-clip transform.

## 3.3-alpha-vdoom.25 - 2026-06-27

- Added `vk_draw_world`, an opt-in Vulkan world-wall batch separate from the
  green `vk_world_probe` diagnostic overlay.
- Added independent world-draw vertex ranges and `stat renderer` reporting so
  the real world path can be tested without conflating it with probe geometry.
- Reused the existing clipped one-sided wall extraction for the first opaque
  GPU-owned world draw pass.

## 3.3-alpha-vdoom.24 - 2026-06-27

- Flipped the Vulkan probe yaw convention to match Doom's camera turn
  direction.
- Applied the corrected yaw consistently to world-probe culling, scene-probe
  marker placement, and the probe world-to-clip transform.

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
