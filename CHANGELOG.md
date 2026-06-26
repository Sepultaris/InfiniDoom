# vDoom Changelog

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
