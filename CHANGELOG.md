# vDoom Changelog

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
