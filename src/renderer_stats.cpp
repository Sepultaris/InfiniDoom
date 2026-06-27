/*
** renderer_stats.cpp
** vDoom renderer diagnostics exposed through the existing stat command.
*/

#include "stats.h"
#include "c_cvars.h"
#include "v_video.h"
#include "version.h"
#include "vulkan/vk_renderer.h"

#include <string.h>

#ifdef _WIN32
typedef void *VDoomProcessHandle;

struct VDoomProcessMemoryCounters
{
	unsigned long cb;
	unsigned long PageFaultCount;
	size_t PeakWorkingSetSize;
	size_t WorkingSetSize;
	size_t QuotaPeakPagedPoolUsage;
	size_t QuotaPagedPoolUsage;
	size_t QuotaPeakNonPagedPoolUsage;
	size_t QuotaNonPagedPoolUsage;
	size_t PagefileUsage;
	size_t PeakPagefileUsage;
};

extern "C"
{
	__declspec(dllimport) VDoomProcessHandle __stdcall GetCurrentProcess();
	__declspec(dllimport) int __stdcall K32GetProcessMemoryInfo(VDoomProcessHandle Process, VDoomProcessMemoryCounters *Counters, unsigned long Size);
}
#endif

EXTERN_CVAR(Int, vid_renderer)

static const char *RendererName(int renderer)
{
	switch (renderer)
	{
	case 0:
		return "Software";
	case 1:
		return "OpenGL";
	case 2:
		return "Vulkan";
	default:
		return "Unknown";
	}
}

static FString FormatBytes(unsigned long long bytes)
{
	FString out;
	if (bytes >= 1024ull * 1024ull * 1024ull)
	{
		out.Format("%.1f GiB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
	}
	else if (bytes >= 1024ull * 1024ull)
	{
		out.Format("%.1f MiB", (double)bytes / (1024.0 * 1024.0));
	}
	else
	{
		out.Format("%.0f bytes", (double)bytes);
	}
	return out;
}

static FString FormatVulkanVersion(unsigned int version)
{
	FString out;
	out.Format("%u.%u.%u", version >> 22, (version >> 12) & 0x3ff, version & 0xfff);
	return out;
}

static const char *VulkanScaleModeName(unsigned int mode)
{
	switch (mode)
	{
	case 0:
		return "stretch";
	case 1:
		return "preserve aspect";
	case 2:
		return "integer scale";
	default:
		return "unknown";
	}
}

static const char *VulkanPresentFilterName(unsigned int mode)
{
	switch (mode)
	{
	case 0:
		return "nearest";
	case 1:
		return "linear index";
	case 2:
		return "sharp color";
	default:
		return "unknown";
	}
}

static FString FormatPresentAspect(float aspect)
{
	FString out;
	if (aspect > 0.f)
	{
		out.Format("%.3f", aspect);
	}
	else
	{
		out = "source";
	}
	return out;
}

static unsigned long long GetProcessRamBytes()
{
#ifdef _WIN32
	VDoomProcessMemoryCounters counters;
	memset(&counters, 0, sizeof(counters));
	counters.cb = sizeof(counters);
	if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
	{
		return (unsigned long long)counters.WorkingSetSize;
	}
#endif
	return 0;
}

ADD_STAT(renderer)
{
	FString out;
	out.Format("Renderer: %s (%d)\nEngine: %s\n", RendererName(vid_renderer), (int)vid_renderer, GetVersionString());

	if (screen != NULL)
	{
		out.AppendFormat("Framebuffer: %dx%d, %s\n",
			screen->GetWidth(),
			screen->GetHeight(),
			screen->IsFullscreen() ? "fullscreen" : "windowed");
	}
	else
	{
		out += "Framebuffer: unavailable\n";
	}

	if (vid_renderer == 2)
	{
		const FVulkanBackendStats &vk = vk_GetBackendStats();
		if (vk.Available)
		{
			out.AppendFormat("Vulkan: %s, API %s\n",
				vk.Ready ? "ready" : "initializing",
				FormatVulkanVersion(vk.ApiVersion).GetChars());
			out.AppendFormat("Device: %s\n", vk.DeviceName[0] != 0 ? vk.DeviceName : "unknown");
			out.AppendFormat("Device IDs: vendor %04x, device %04x, driver %u\n",
				vk.VendorID,
				vk.DeviceID,
				vk.DriverVersion);
			out.AppendFormat("Swapchain: %ux%u, %u images, format %u\n",
				vk.SwapchainWidth,
				vk.SwapchainHeight,
				vk.SwapchainImageCount,
				vk.SwapchainFormat);
			out.AppendFormat("Swapchain state: %u recreate(s), %u out-of-date event(s)%s\n",
				vk.SwapchainRecreateCount,
				vk.OutOfDateCount,
				vk.WindowMinimized ? ", minimized" : "");
			out.AppendFormat("Presentation: %s, %s filter, %s, aspect %s\n",
				vk.GpuPresentationActive ? "GPU palette shader" : "transfer fallback",
				VulkanPresentFilterName(vk.PresentFilterMode),
				VulkanScaleModeName(vk.PresentScaleMode),
				FormatPresentAspect(vk.PresentAspect).GetChars());
			if (vk.PresentFilterMode == 2)
			{
				out.AppendFormat("Present sharpness: %.2f\n", (double)vk.PresentSharpness);
			}
			if (vk.GpuPresentationActive)
			{
				out.AppendFormat("Present source: %ux%u\n",
					vk.PresentSourceWidth,
					vk.PresentSourceHeight);
				out.AppendFormat("Present viewport: %ux%u at %u,%u\n",
					vk.PresentViewportWidth,
					vk.PresentViewportHeight,
					vk.PresentViewportX,
					vk.PresentViewportY);
				if (vk.SceneProbeActive)
				{
					out.AppendFormat("GPU scene probe: active, %u vertices\n", vk.SceneProbeVertexCount);
				}
				else
				{
					out.AppendFormat("GPU scene probe: off\n");
				}
				if (vk.WorldProbeActive)
				{
					out.AppendFormat("GPU world probe: active, %u vertices\n", vk.WorldProbeVertexCount);
				}
				else
				{
					out.AppendFormat("GPU world probe: off\n");
				}
			}
			out.AppendFormat("Queue: graphics/present family %u of %u device(s)\n",
				vk.GraphicsQueueFamily,
				vk.DeviceCount);
			out.AppendFormat("Upload buffer: %s\n", FormatBytes(vk.UploadBufferBytes).GetChars());
			if (vk.MemoryBudgetAvailable)
			{
				out.AppendFormat("VRAM/local heap: %s used, %s budget, %s total\n",
					FormatBytes(vk.DeviceLocalMemoryUsageBytes).GetChars(),
					FormatBytes(vk.DeviceLocalMemoryBudgetBytes).GetChars(),
					FormatBytes(vk.DeviceLocalMemoryBytes).GetChars());
			}
			else
			{
				out.AppendFormat("VRAM/local heap: %s total, live budget unavailable\n", FormatBytes(vk.DeviceLocalMemoryBytes).GetChars());
			}
			out.AppendFormat("Vulkan present CPU: %u ms, %s\n",
				vk.LastPresentMS,
				vk.LastPresentSucceeded ? "ok" : "not presented");
			if (vk.TimestampQueriesAvailable)
			{
				out.AppendFormat("Vulkan GPU frame: %.3f ms\n", vk.LastGpuFrameMS);
			}
			else
			{
				out += "Vulkan GPU frame: timestamp queries unavailable\n";
			}
		}
		else
		{
			out += "Vulkan: not initialized\n";
		}
	}

	unsigned long long processRam = GetProcessRamBytes();
	if (processRam != 0)
	{
		out.AppendFormat("Process RAM: %s working set\n", FormatBytes(processRam).GetChars());
	}
	else
	{
		out += "Process RAM: unavailable\n";
	}

	out += "Frame timing: use vid_fps 1 or stat fps";
	return out;
}
