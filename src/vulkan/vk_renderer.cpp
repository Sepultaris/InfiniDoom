/*
** vk_renderer.cpp
** Vulkan renderer bootstrap interface.
**
** This backend is intentionally InfiniDoom-shaped: it proves Vulkan runtime,
** instance, physical device, and graphics queue availability while continuing
** to use the existing software renderer until the swapchain/render path is
** built in small steps.
*/

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define USE_WINDOWS_DWORD
#include <windows.h>
#endif

#include <vulkan/vulkan.h>

#include "vulkan/vk_renderer.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef _WIN32
#include "win32/hardware.h"
#endif

#include "r_swrenderer.h"
#include "version.h"
#include "c_cvars.h"
#include "v_text.h"
#include "v_palette.h"
#include "m_fixed.h"
#include "r_utility.h"
#include "vulkan/vk_palette_present_shaders.h"
#ifdef _WIN32
#include "win32/i_system.h"
#else
#include "sdl/i_system.h"
#endif

void DoBlending(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

#ifdef _WIN32
extern HINSTANCE g_hInst;
extern HWND Window;
#endif

CUSTOM_CVAR(Int, vk_present_filter, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}

CUSTOM_CVAR(Float, vk_render_scale, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0.25f)
	{
		self = 0.25f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}

CUSTOM_CVAR(Float, vk_present_sharpness, 0.5f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0.f)
	{
		self = 0.f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}

CUSTOM_CVAR(Int, vk_present_scale_mode, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}

CUSTOM_CVAR(Float, vk_present_aspect, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0.f)
	{
		self = 0.f;
	}
	else if (self > 4.f)
	{
		self = 4.f;
	}
}

CVAR(Bool, vk_present_force_aspect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
CVAR(Bool, vk_draw_world, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
CVAR(Bool, vk_scene_probe, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
CVAR(Bool, vk_world_probe, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
CUSTOM_CVAR(Int, vk_world_yaw_sign, -1, CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = -1;
	}
	else
	{
		self = 1;
	}
}
CUSTOM_CVAR(Int, vk_clip_yaw_sign, -1, CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = -1;
	}
	else
	{
		self = 1;
	}
}
CUSTOM_CVAR(Int, vk_clip_side_sign, 1, CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = -1;
	}
	else
	{
		self = 1;
	}
}

namespace
{
#ifdef _WIN32
	typedef HMODULE VulkanLoaderModule;
#else
	typedef void *VulkanLoaderModule;
#endif

	struct VulkanFunctions
	{
		PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
		PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
		PFN_vkCreateInstance CreateInstance;
		PFN_vkDestroyInstance DestroyInstance;
		PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
		PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
		PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
		PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
		PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2;
		PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
		PFN_vkCreateDevice CreateDevice;
		PFN_vkDestroyDevice DestroyDevice;
		PFN_vkDeviceWaitIdle DeviceWaitIdle;
		PFN_vkGetDeviceQueue GetDeviceQueue;
		PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
		PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
		PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
		PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
		PFN_vkCreateImageView CreateImageView;
		PFN_vkDestroyImageView DestroyImageView;
		PFN_vkCreateCommandPool CreateCommandPool;
		PFN_vkDestroyCommandPool DestroyCommandPool;
		PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
		PFN_vkBeginCommandBuffer BeginCommandBuffer;
		PFN_vkEndCommandBuffer EndCommandBuffer;
		PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
		PFN_vkCmdClearColorImage CmdClearColorImage;
		PFN_vkCreateSemaphore CreateSemaphore;
		PFN_vkDestroySemaphore DestroySemaphore;
		PFN_vkCreateFence CreateFence;
		PFN_vkDestroyFence DestroyFence;
		PFN_vkWaitForFences WaitForFences;
		PFN_vkResetFences ResetFences;
		PFN_vkResetCommandBuffer ResetCommandBuffer;
		PFN_vkQueueSubmit QueueSubmit;
		PFN_vkQueuePresentKHR QueuePresentKHR;
		PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
		PFN_vkCreateBuffer CreateBuffer;
		PFN_vkDestroyBuffer DestroyBuffer;
		PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
		PFN_vkAllocateMemory AllocateMemory;
		PFN_vkFreeMemory FreeMemory;
		PFN_vkBindBufferMemory BindBufferMemory;
		PFN_vkMapMemory MapMemory;
		PFN_vkUnmapMemory UnmapMemory;
		PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
		PFN_vkCreateImage CreateImage;
		PFN_vkDestroyImage DestroyImage;
		PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
		PFN_vkBindImageMemory BindImageMemory;
		PFN_vkCreateSampler CreateSampler;
		PFN_vkDestroySampler DestroySampler;
		PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
		PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
		PFN_vkCreateDescriptorPool CreateDescriptorPool;
		PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
		PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
		PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
		PFN_vkCreatePipelineLayout CreatePipelineLayout;
		PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
		PFN_vkCreateRenderPass CreateRenderPass;
		PFN_vkDestroyRenderPass DestroyRenderPass;
		PFN_vkCreateFramebuffer CreateFramebuffer;
		PFN_vkDestroyFramebuffer DestroyFramebuffer;
		PFN_vkCreateShaderModule CreateShaderModule;
		PFN_vkDestroyShaderModule DestroyShaderModule;
		PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
		PFN_vkDestroyPipeline DestroyPipeline;
		PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
		PFN_vkCmdEndRenderPass CmdEndRenderPass;
		PFN_vkCmdBindPipeline CmdBindPipeline;
		PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
		PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
		PFN_vkCmdDraw CmdDraw;
		PFN_vkCmdSetViewport CmdSetViewport;
		PFN_vkCmdSetScissor CmdSetScissor;
		PFN_vkCmdPushConstants CmdPushConstants;
		PFN_vkCreateQueryPool CreateQueryPool;
		PFN_vkDestroyQueryPool DestroyQueryPool;
		PFN_vkCmdResetQueryPool CmdResetQueryPool;
		PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
		PFN_vkGetQueryPoolResults GetQueryPoolResults;
#ifdef _WIN32
		PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
#endif
	};

	class VulkanRuntime;
	static FVulkanBackendStats VulkanStats;
	static void ResetVulkanStats();
	static void PublishVulkanStats(const VulkanRuntime *runtime);
	static const char *CopyVulkanString(char *to, unsigned int toSize, const char *from);

	struct PresentPushConstants
	{
		float UvOffset[2];
		float UvScale[2];
		float SourceOffset[2];
		float SourceScale[2];
		float BorderColor[4];
		float FilterParams[4];
	};

	struct SceneProbeVertex
	{
		float Position[3];
		float Color[3];
	};

	struct SceneProbePushConstants
	{
		float Row0[4];
		float Row1[4];
		float Row2[4];
		float Row3[4];
	};

	enum
	{
		SceneProbeVertexCount = 3,
		WorldProbeMaxWalls = 96,
		WorldDrawMaxWalls = 192,
		ProbeVertexMaxCount = SceneProbeVertexCount + (WorldProbeMaxWalls + WorldDrawMaxWalls) * 6
	};

	static int ClampPresentFilterMode()
	{
		return vk_present_filter < 0 ? 0 : (vk_present_filter > 2 ? 2 : vk_present_filter);
	}

	static float ClampRenderScale()
	{
		return vk_render_scale < 0.25f ? 0.25f : (vk_render_scale > 1.f ? 1.f : vk_render_scale);
	}

	static float ClampPresentSharpness()
	{
		return vk_present_sharpness < 0.f ? 0.f : (vk_present_sharpness > 1.f ? 1.f : vk_present_sharpness);
	}

	class VulkanRuntime
	{
	public:
		VulkanRuntime()
			: Module(NULL), Instance(NULL), Surface(VK_NULL_HANDLE), PhysicalDevice(NULL), Device(NULL), GraphicsQueue(NULL),
			  Swapchain(VK_NULL_HANDLE), CommandPool(VK_NULL_HANDLE), CommandBuffer(VK_NULL_HANDLE),
			  ImageAvailableSemaphore(VK_NULL_HANDLE), RenderFinishedSemaphore(VK_NULL_HANDLE), RenderFence(VK_NULL_HANDLE),
			  UploadBuffer(VK_NULL_HANDLE), UploadMemory(VK_NULL_HANDLE), UploadPtr(NULL), UploadSize(0),
			  ProbeVertexBuffer(VK_NULL_HANDLE), ProbeVertexMemory(VK_NULL_HANDLE), ProbeVertexPtr(NULL), ProbeVertexBufferSize(0),
			  ProbeVertexDrawCount(0), WorldDrawFirstVertex(0), WorldDrawDrawCount(0), SceneProbeFirstVertex(0), SceneProbeDrawCount(0), WorldProbeFirstVertex(0), WorldProbeDrawCount(0),
			  SourceImage(VK_NULL_HANDLE), SourceImageMemory(VK_NULL_HANDLE), SourceImageView(VK_NULL_HANDLE),
			  PaletteImage(VK_NULL_HANDLE), PaletteImageMemory(VK_NULL_HANDLE), PaletteImageView(VK_NULL_HANDLE),
			  DepthImage(VK_NULL_HANDLE), DepthImageMemory(VK_NULL_HANDLE), DepthImageView(VK_NULL_HANDLE),
			  NearestSampler(VK_NULL_HANDLE), DescriptorSetLayout(VK_NULL_HANDLE), DescriptorPool(VK_NULL_HANDLE),
			  DescriptorSet(VK_NULL_HANDLE), PipelineLayout(VK_NULL_HANDLE), RenderPass(VK_NULL_HANDLE),
			  GraphicsPipeline(VK_NULL_HANDLE), ProbePipelineLayout(VK_NULL_HANDLE), ProbePipeline(VK_NULL_HANDLE),
			  WorldProbePipelineLayout(VK_NULL_HANDLE), WorldProbePipeline(VK_NULL_HANDLE),
			  SourceImageWidth(0), SourceImageHeight(0), GpuPresentationReady(false),
			  PresentFilterMode(-1), TimestampQueryPool(VK_NULL_HANDLE), TimestampQueriesSupported(false),
			  TimestampQueryPending(false), TimestampPeriod(0.0), LastGpuFrameMS(0.0), MemoryBudgetSupported(false),
			  DeviceLocalMemoryBudgetBytes(0), DeviceLocalMemoryUsageBytes(0),
			  PresentScaleMode(1), PresentViewportX(0), PresentViewportY(0), PresentViewportWidth(0), PresentViewportHeight(0),
			  GraphicsQueueFamily(~0u), DeviceCount(0), SwapchainImageCount(0), SwapchainViewCount(0),
			  SwapchainFormat(VK_FORMAT_UNDEFINED), SwapchainColorSpace(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
			  SwapchainRecreateCount(0), OutOfDateCount(0), WindowMinimized(false), Ready(false)
		{
			SwapchainExtent.width = 0;
			SwapchainExtent.height = 0;
			for (unsigned int i = 0; i < MaxSwapchainImages; ++i)
			{
				SwapchainImages[i] = VK_NULL_HANDLE;
				SwapchainImageViews[i] = VK_NULL_HANDLE;
				SwapchainFramebuffers[i] = VK_NULL_HANDLE;
			}
			memset(&Vk, 0, sizeof(Vk));
			memset(&DeviceProperties, 0, sizeof(DeviceProperties));
			memset(&MemoryProperties, 0, sizeof(MemoryProperties));
		}

		~VulkanRuntime()
		{
			Shutdown();
		}

		bool Init()
		{
			if (!LoadLoader())
			{
				return false;
			}
			if (!LoadGlobalFunctions())
			{
				return false;
			}
			if (!CreateInstance())
			{
				return false;
			}
			if (!CreateSurface())
			{
				return false;
			}
			if (!ChoosePhysicalDevice())
			{
				return false;
			}
			if (!CreateDevice())
			{
				return false;
			}
			if (!CreateSwapchain())
			{
				return false;
			}
			if (!CreateImageViews())
			{
				return false;
			}
			if (!CreateCommandResources())
			{
				return false;
			}
			Ready = true;
			PublishVulkanStats(this);
			return true;
		}

		void Shutdown()
		{
			if (Device != NULL && Vk.DestroyDevice != NULL)
			{
				if (Vk.DeviceWaitIdle != NULL)
				{
					Vk.DeviceWaitIdle(Device);
				}
				if (RenderFence != VK_NULL_HANDLE && Vk.DestroyFence != NULL)
				{
					Vk.DestroyFence(Device, RenderFence, NULL);
				}
				RenderFence = VK_NULL_HANDLE;
				DestroyTimestampQueries();
				if (RenderFinishedSemaphore != VK_NULL_HANDLE && Vk.DestroySemaphore != NULL)
				{
					Vk.DestroySemaphore(Device, RenderFinishedSemaphore, NULL);
				}
				RenderFinishedSemaphore = VK_NULL_HANDLE;
				if (ImageAvailableSemaphore != VK_NULL_HANDLE && Vk.DestroySemaphore != NULL)
				{
					Vk.DestroySemaphore(Device, ImageAvailableSemaphore, NULL);
				}
				ImageAvailableSemaphore = VK_NULL_HANDLE;
				if (CommandPool != VK_NULL_HANDLE && Vk.DestroyCommandPool != NULL)
				{
					Vk.DestroyCommandPool(Device, CommandPool, NULL);
				}
				CommandPool = VK_NULL_HANDLE;
				CommandBuffer = VK_NULL_HANDLE;
				DestroyPresentationResources();
				DestroyProbeVertexBuffer();
				DestroyUploadBuffer();
				DestroySwapchainResources();
				Vk.DestroyDevice(Device, NULL);
			}
			Device = NULL;
			GraphicsQueue = NULL;
			SwapchainImageCount = 0;
			for (unsigned int i = 0; i < MaxSwapchainImages; ++i)
			{
				SwapchainImages[i] = VK_NULL_HANDLE;
			}
			SwapchainFormat = VK_FORMAT_UNDEFINED;
			SwapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			SwapchainExtent.width = 0;
			SwapchainExtent.height = 0;

			if (Instance != NULL && Vk.DestroyInstance != NULL)
			{
				if (Surface != VK_NULL_HANDLE && Vk.DestroySurfaceKHR != NULL)
				{
					Vk.DestroySurfaceKHR(Instance, Surface, NULL);
				}
				Surface = VK_NULL_HANDLE;
				Vk.DestroyInstance(Instance, NULL);
			}
			Instance = NULL;
			PhysicalDevice = NULL;
			GraphicsQueueFamily = ~0u;
			DeviceCount = 0;
			Ready = false;
			TimestampQueriesSupported = false;
			TimestampQueryPending = false;
			TimestampPeriod = 0.0;
			LastGpuFrameMS = 0.0;
			MemoryBudgetSupported = false;
			DeviceLocalMemoryBudgetBytes = 0;
			DeviceLocalMemoryUsageBytes = 0;
			memset(&DeviceProperties, 0, sizeof(DeviceProperties));
			memset(&MemoryProperties, 0, sizeof(MemoryProperties));

#ifdef _WIN32
			if (Module != NULL)
			{
				FreeLibrary(Module);
			}
#else
			if (Module != NULL)
			{
				dlclose(Module);
			}
#endif
			Module = NULL;
			memset(&Vk, 0, sizeof(Vk));
			ResetVulkanStats();
		}

		bool IsReady() const
		{
			return Ready;
		}

		const VkPhysicalDeviceProperties &GetDeviceProperties() const
		{
			return DeviceProperties;
		}

		const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const
		{
			return MemoryProperties;
		}

		unsigned int GetDeviceCount() const
		{
			return DeviceCount;
		}

		unsigned int GetGraphicsQueueFamily() const
		{
			return GraphicsQueueFamily;
		}

		unsigned int GetSwapchainImageCount() const
		{
			return SwapchainImageCount;
		}

		VkFormat GetSwapchainFormat() const
		{
			return SwapchainFormat;
		}

		VkExtent2D GetSwapchainExtent() const
		{
			return SwapchainExtent;
		}

		VkColorSpaceKHR GetSwapchainColorSpace() const
		{
			return SwapchainColorSpace;
		}

		VkDeviceSize GetUploadSize() const
		{
			return UploadSize;
		}

		unsigned int GetSwapchainRecreateCount() const
		{
			return SwapchainRecreateCount;
		}

		unsigned int GetOutOfDateCount() const
		{
			return OutOfDateCount;
		}

		bool IsWindowMinimized() const
		{
			return WindowMinimized;
		}

		bool IsGpuPresentationReady() const
		{
			return GpuPresentationReady;
		}

		bool IsSceneProbeActive() const
		{
			return vk_scene_probe && ProbePipeline != VK_NULL_HANDLE && ProbeVertexBuffer != VK_NULL_HANDLE && SceneProbeDrawCount > 0;
		}

		unsigned int GetSceneProbeVertexCount() const
		{
			return IsSceneProbeActive() ? SceneProbeDrawCount : 0;
		}

		bool IsWorldProbeActive() const
		{
			return vk_world_probe && WorldProbePipeline != VK_NULL_HANDLE && ProbeVertexBuffer != VK_NULL_HANDLE && WorldProbeDrawCount > 0;
		}

		unsigned int GetWorldProbeVertexCount() const
		{
			return IsWorldProbeActive() ? WorldProbeDrawCount : 0;
		}

		bool IsWorldDrawActive() const
		{
			return vk_draw_world && WorldProbePipeline != VK_NULL_HANDLE && ProbeVertexBuffer != VK_NULL_HANDLE && WorldDrawDrawCount > 0;
		}

		unsigned int GetWorldDrawVertexCount() const
		{
			return IsWorldDrawActive() ? WorldDrawDrawCount : 0;
		}

		bool WantsProbeDraw() const
		{
			return vk_draw_world || vk_scene_probe || vk_world_probe;
		}

		bool AreTimestampQueriesAvailable() const
		{
			return TimestampQueriesSupported && TimestampQueryPool != VK_NULL_HANDLE;
		}

		double GetLastGpuFrameMS() const
		{
			return LastGpuFrameMS;
		}

		bool IsMemoryBudgetAvailable() const
		{
			return MemoryBudgetSupported && DeviceLocalMemoryBudgetBytes != 0;
		}

		unsigned long long GetDeviceLocalMemoryBudgetBytes() const
		{
			return DeviceLocalMemoryBudgetBytes;
		}

		unsigned long long GetDeviceLocalMemoryUsageBytes() const
		{
			return DeviceLocalMemoryUsageBytes;
		}

		unsigned int GetPresentFilterMode() const
		{
			return (unsigned int)(PresentFilterMode < 0 ? 0 : PresentFilterMode);
		}

		unsigned int GetPresentScaleMode() const
		{
			return PresentScaleMode;
		}

		unsigned int GetPresentViewportX() const
		{
			return PresentViewportX;
		}

		unsigned int GetPresentViewportY() const
		{
			return PresentViewportY;
		}

		unsigned int GetPresentViewportWidth() const
		{
			return PresentViewportWidth;
		}

		unsigned int GetPresentViewportHeight() const
		{
			return PresentViewportHeight;
		}

		unsigned int GetPresentSourceWidth() const
		{
			return SourceImageWidth;
		}

		unsigned int GetPresentSourceHeight() const
		{
			return SourceImageHeight;
		}

		float GetPresentSharpness() const
		{
			return ClampPresentSharpness();
		}

		float GetPresentAspect() const
		{
			return vk_present_force_aspect && vk_present_aspect > 0.f ? vk_present_aspect : 0.f;
		}

		bool RecreateSwapchainForWindow()
		{
			if (Device == NULL)
			{
				return false;
			}

			Ready = false;
			if (Vk.DeviceWaitIdle != NULL)
			{
				Vk.DeviceWaitIdle(Device);
			}

			DestroyPresentationResources();
			DestroyUploadBuffer();
			DestroySwapchainResources();

			if (IsClientMinimized())
			{
				WindowMinimized = true;
				PublishVulkanStats(this);
				return true;
			}
			WindowMinimized = false;

			if (!CreateSwapchain() || !CreateImageViews())
			{
				PublishVulkanStats(this);
				return false;
			}
			Ready = true;
			SwapchainRecreateCount++;
			PublishVulkanStats(this);
			return true;
		}

		bool PresentPalettedFrame(const BYTE *pixels, int pitch, int width, int height, const PalEntry *palette)
		{
			unsigned int startMS = I_FPSTime();
			if (IsClientMinimized())
			{
				WindowMinimized = true;
				VulkanStats.LastPresentSucceeded = false;
				PublishVulkanStats(this);
				return true;
			}
			WindowMinimized = false;

			if (!EnsureSwapchainMatchesWindow())
			{
				VulkanStats.LastPresentSucceeded = false;
				PublishVulkanStats(this);
				return false;
			}

			if (!Ready || pixels == NULL || palette == NULL || width <= 0 || height <= 0)
			{
				VulkanStats.LastPresentSucceeded = false;
				return false;
			}

			const int presentWidth = (int)SwapchainExtent.width;
			const int presentHeight = (int)SwapchainExtent.height;
			if (presentWidth <= 0 || presentHeight <= 0)
			{
				VulkanStats.LastPresentSucceeded = false;
				return false;
			}

			bool useGpuPresentation = EnsureGpuPresentationResources(width, height);
			VkDeviceSize uploadNeeded = useGpuPresentation ?
				((VkDeviceSize)width * (VkDeviceSize)height + 256 * 4) :
				((VkDeviceSize)presentWidth * (VkDeviceSize)presentHeight * 4);
			if (!EnsureUploadBuffer(uploadNeeded))
			{
				VulkanStats.LastPresentSucceeded = false;
				PublishVulkanStats(this);
				return false;
			}

			BYTE *dst = reinterpret_cast<BYTE *>(UploadPtr);
			if (useGpuPresentation)
			{
				for (int y = 0; y < height; ++y)
				{
					memcpy(dst + y * width, pixels + y * pitch, width);
				}
				BYTE *paletteDst = dst + width * height;
				for (int i = 0; i < 256; ++i)
				{
					paletteDst[i * 4 + 0] = palette[i].r;
					paletteDst[i * 4 + 1] = palette[i].g;
					paletteDst[i * 4 + 2] = palette[i].b;
					paletteDst[i * 4 + 3] = 255;
				}
			}
			else
			{
				for (int y = 0; y < presentHeight; ++y)
				{
					const BYTE *src = pixels + ((y * height) / presentHeight) * pitch;
					for (int x = 0; x < presentWidth; ++x)
					{
						PalEntry color = palette[src[(x * width) / presentWidth]];
						dst[0] = color.b;
						dst[1] = color.g;
						dst[2] = color.r;
						dst[3] = 255;
						dst += 4;
					}
				}
			}

			VkResult result = Vk.WaitForFences(Device, 1, &RenderFence, VK_TRUE, ~(uint64_t)0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkWaitForFences failed (%d).\n", (int)result);
				return false;
			}
			ReadGpuFrameTime();
			RefreshMemoryBudgetStats();
			Vk.ResetFences(Device, 1, &RenderFence);
			Vk.ResetCommandBuffer(CommandBuffer, 0);

			unsigned int imageIndex = 0;
			result = Vk.AcquireNextImageKHR(Device, Swapchain, ~(uint64_t)0, ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				OutOfDateCount++;
				VulkanStats.LastPresentSucceeded = false;
				RecreateSwapchainForWindow();
				return false;
			}
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			{
				VulkanStats.LastPresentSucceeded = false;
				return false;
			}
			if (imageIndex >= SwapchainImageCount)
			{
				VulkanStats.LastPresentSucceeded = false;
				return false;
			}
			if (useGpuPresentation)
			{
				if (!RecordGpuPresentationCommands(imageIndex, width, height))
				{
					VulkanStats.LastPresentSucceeded = false;
					return false;
				}
			}
			else if (!RecordUploadCommands(imageIndex, presentWidth, presentHeight))
			{
				VulkanStats.LastPresentSucceeded = false;
				return false;
			}

			VkPipelineStageFlags waitStage = useGpuPresentation ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo submitInfo;
			memset(&submitInfo, 0, sizeof(submitInfo));
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
			submitInfo.pWaitDstStageMask = &waitStage;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &CommandBuffer;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &RenderFinishedSemaphore;

			result = Vk.QueueSubmit(GraphicsQueue, 1, &submitInfo, RenderFence);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkQueueSubmit failed (%d).\n", (int)result);
				VulkanStats.LastPresentSucceeded = false;
				TimestampQueryPending = false;
				return false;
			}
			TimestampQueryPending = AreTimestampQueriesAvailable();

			VkPresentInfoKHR presentInfo;
			memset(&presentInfo, 0, sizeof(presentInfo));
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &RenderFinishedSemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &Swapchain;
			presentInfo.pImageIndices = &imageIndex;

			result = Vk.QueuePresentKHR(GraphicsQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				OutOfDateCount++;
				VulkanStats.LastPresentMS = I_FPSTime() - startMS;
				VulkanStats.LastPresentSucceeded = false;
				RecreateSwapchainForWindow();
				return false;
			}
			PublishVulkanStats(this);
			VulkanStats.LastPresentMS = I_FPSTime() - startMS;
			VulkanStats.LastPresentSucceeded = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
			if (result == VK_SUBOPTIMAL_KHR)
			{
				RecreateSwapchainForWindow();
			}
			return VulkanStats.LastPresentSucceeded;
		}

	private:
		enum
		{
			MaxSwapchainImages = 16
		};

		bool LoadLoader()
		{
#ifdef _WIN32
			Module = LoadLibraryA("vulkan-1.dll");
#else
			Module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
			if (Module == NULL)
			{
				Module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
			}
#endif
			if (Module == NULL)
			{
				Printf(TEXTCOLOR_RED "Vulkan: loader library not found.\n");
				return false;
			}
			return true;
		}

		PFN_vkVoidFunction GetLoaderProc(const char *name)
		{
#ifdef _WIN32
			return reinterpret_cast<PFN_vkVoidFunction>(GetProcAddress(Module, name));
#else
			return reinterpret_cast<PFN_vkVoidFunction>(dlsym(Module, name));
#endif
		}

		bool LoadGlobalFunctions()
		{
			Vk.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetLoaderProc("vkGetInstanceProcAddr"));
			if (Vk.GetInstanceProcAddr == NULL)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkGetInstanceProcAddr is unavailable.\n");
				return false;
			}
			Vk.CreateInstance = reinterpret_cast<PFN_vkCreateInstance>(Vk.GetInstanceProcAddr(NULL, "vkCreateInstance"));
			if (Vk.CreateInstance == NULL)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateInstance is unavailable.\n");
				return false;
			}
			return true;
		}

		bool LoadInstanceFunctions()
		{
			Vk.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(Vk.GetInstanceProcAddr(Instance, "vkDestroyInstance"));
			Vk.EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(Vk.GetInstanceProcAddr(Instance, "vkEnumeratePhysicalDevices"));
			Vk.GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
			Vk.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceProperties"));
			Vk.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceMemoryProperties"));
			Vk.GetPhysicalDeviceMemoryProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceMemoryProperties2"));
			if (Vk.GetPhysicalDeviceMemoryProperties2 == NULL)
			{
				Vk.GetPhysicalDeviceMemoryProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceMemoryProperties2KHR"));
			}
			Vk.EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(Vk.GetInstanceProcAddr(Instance, "vkEnumerateDeviceExtensionProperties"));
			Vk.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(Vk.GetInstanceProcAddr(Instance, "vkCreateDevice"));
			Vk.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(Vk.GetInstanceProcAddr(Instance, "vkGetDeviceProcAddr"));
			Vk.DestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(Vk.GetInstanceProcAddr(Instance, "vkDestroySurfaceKHR"));
			Vk.GetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
			Vk.GetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
			Vk.GetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
			Vk.GetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(Vk.GetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
#ifdef _WIN32
			Vk.CreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(Vk.GetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
#endif
			return Vk.DestroyInstance != NULL &&
				Vk.EnumeratePhysicalDevices != NULL &&
				Vk.GetPhysicalDeviceQueueFamilyProperties != NULL &&
				Vk.GetPhysicalDeviceProperties != NULL &&
				Vk.GetPhysicalDeviceMemoryProperties != NULL &&
				Vk.EnumerateDeviceExtensionProperties != NULL &&
				Vk.CreateDevice != NULL &&
				Vk.GetDeviceProcAddr != NULL &&
				Vk.DestroySurfaceKHR != NULL &&
				Vk.GetPhysicalDeviceSurfaceSupportKHR != NULL &&
				Vk.GetPhysicalDeviceSurfaceCapabilitiesKHR != NULL &&
				Vk.GetPhysicalDeviceSurfaceFormatsKHR != NULL &&
				Vk.GetPhysicalDeviceSurfacePresentModesKHR != NULL
#ifdef _WIN32
				&& Vk.CreateWin32SurfaceKHR != NULL
#endif
				;
		}

		bool LoadDeviceFunctions()
		{
			Vk.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(Vk.GetDeviceProcAddr(Device, "vkDestroyDevice"));
			Vk.DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(Vk.GetDeviceProcAddr(Device, "vkDeviceWaitIdle"));
			Vk.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(Vk.GetDeviceProcAddr(Device, "vkGetDeviceQueue"));
			Vk.CreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(Vk.GetDeviceProcAddr(Device, "vkCreateSwapchainKHR"));
			Vk.DestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(Vk.GetDeviceProcAddr(Device, "vkDestroySwapchainKHR"));
			Vk.GetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(Vk.GetDeviceProcAddr(Device, "vkGetSwapchainImagesKHR"));
			Vk.CreateImageView = reinterpret_cast<PFN_vkCreateImageView>(Vk.GetDeviceProcAddr(Device, "vkCreateImageView"));
			Vk.DestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(Vk.GetDeviceProcAddr(Device, "vkDestroyImageView"));
			Vk.CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(Vk.GetDeviceProcAddr(Device, "vkCreateCommandPool"));
			Vk.DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(Vk.GetDeviceProcAddr(Device, "vkDestroyCommandPool"));
			Vk.AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(Vk.GetDeviceProcAddr(Device, "vkAllocateCommandBuffers"));
			Vk.BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(Vk.GetDeviceProcAddr(Device, "vkBeginCommandBuffer"));
			Vk.EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(Vk.GetDeviceProcAddr(Device, "vkEndCommandBuffer"));
			Vk.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(Vk.GetDeviceProcAddr(Device, "vkCmdPipelineBarrier"));
			Vk.CmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(Vk.GetDeviceProcAddr(Device, "vkCmdClearColorImage"));
			Vk.CreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(Vk.GetDeviceProcAddr(Device, "vkCreateSemaphore"));
			Vk.DestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(Vk.GetDeviceProcAddr(Device, "vkDestroySemaphore"));
			Vk.CreateFence = reinterpret_cast<PFN_vkCreateFence>(Vk.GetDeviceProcAddr(Device, "vkCreateFence"));
			Vk.DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(Vk.GetDeviceProcAddr(Device, "vkDestroyFence"));
			Vk.WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(Vk.GetDeviceProcAddr(Device, "vkWaitForFences"));
			Vk.ResetFences = reinterpret_cast<PFN_vkResetFences>(Vk.GetDeviceProcAddr(Device, "vkResetFences"));
			Vk.ResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(Vk.GetDeviceProcAddr(Device, "vkResetCommandBuffer"));
			Vk.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(Vk.GetDeviceProcAddr(Device, "vkQueueSubmit"));
			Vk.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(Vk.GetDeviceProcAddr(Device, "vkQueuePresentKHR"));
			Vk.AcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(Vk.GetDeviceProcAddr(Device, "vkAcquireNextImageKHR"));
			Vk.CreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(Vk.GetDeviceProcAddr(Device, "vkCreateBuffer"));
			Vk.DestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(Vk.GetDeviceProcAddr(Device, "vkDestroyBuffer"));
			Vk.GetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(Vk.GetDeviceProcAddr(Device, "vkGetBufferMemoryRequirements"));
			Vk.AllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(Vk.GetDeviceProcAddr(Device, "vkAllocateMemory"));
			Vk.FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(Vk.GetDeviceProcAddr(Device, "vkFreeMemory"));
			Vk.BindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(Vk.GetDeviceProcAddr(Device, "vkBindBufferMemory"));
			Vk.MapMemory = reinterpret_cast<PFN_vkMapMemory>(Vk.GetDeviceProcAddr(Device, "vkMapMemory"));
			Vk.UnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(Vk.GetDeviceProcAddr(Device, "vkUnmapMemory"));
			Vk.CmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(Vk.GetDeviceProcAddr(Device, "vkCmdCopyBufferToImage"));
			Vk.CreateImage = reinterpret_cast<PFN_vkCreateImage>(Vk.GetDeviceProcAddr(Device, "vkCreateImage"));
			Vk.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(Vk.GetDeviceProcAddr(Device, "vkDestroyImage"));
			Vk.GetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(Vk.GetDeviceProcAddr(Device, "vkGetImageMemoryRequirements"));
			Vk.BindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(Vk.GetDeviceProcAddr(Device, "vkBindImageMemory"));
			Vk.CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(Vk.GetDeviceProcAddr(Device, "vkCreateSampler"));
			Vk.DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(Vk.GetDeviceProcAddr(Device, "vkDestroySampler"));
			Vk.CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(Vk.GetDeviceProcAddr(Device, "vkCreateDescriptorSetLayout"));
			Vk.DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(Vk.GetDeviceProcAddr(Device, "vkDestroyDescriptorSetLayout"));
			Vk.CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(Vk.GetDeviceProcAddr(Device, "vkCreateDescriptorPool"));
			Vk.DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(Vk.GetDeviceProcAddr(Device, "vkDestroyDescriptorPool"));
			Vk.AllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(Vk.GetDeviceProcAddr(Device, "vkAllocateDescriptorSets"));
			Vk.UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(Vk.GetDeviceProcAddr(Device, "vkUpdateDescriptorSets"));
			Vk.CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(Vk.GetDeviceProcAddr(Device, "vkCreatePipelineLayout"));
			Vk.DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(Vk.GetDeviceProcAddr(Device, "vkDestroyPipelineLayout"));
			Vk.CreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(Vk.GetDeviceProcAddr(Device, "vkCreateRenderPass"));
			Vk.DestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(Vk.GetDeviceProcAddr(Device, "vkDestroyRenderPass"));
			Vk.CreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(Vk.GetDeviceProcAddr(Device, "vkCreateFramebuffer"));
			Vk.DestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(Vk.GetDeviceProcAddr(Device, "vkDestroyFramebuffer"));
			Vk.CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(Vk.GetDeviceProcAddr(Device, "vkCreateShaderModule"));
			Vk.DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(Vk.GetDeviceProcAddr(Device, "vkDestroyShaderModule"));
			Vk.CreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(Vk.GetDeviceProcAddr(Device, "vkCreateGraphicsPipelines"));
			Vk.DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(Vk.GetDeviceProcAddr(Device, "vkDestroyPipeline"));
			Vk.CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(Vk.GetDeviceProcAddr(Device, "vkCmdBeginRenderPass"));
			Vk.CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(Vk.GetDeviceProcAddr(Device, "vkCmdEndRenderPass"));
			Vk.CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(Vk.GetDeviceProcAddr(Device, "vkCmdBindPipeline"));
			Vk.CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(Vk.GetDeviceProcAddr(Device, "vkCmdBindDescriptorSets"));
			Vk.CmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(Vk.GetDeviceProcAddr(Device, "vkCmdBindVertexBuffers"));
			Vk.CmdDraw = reinterpret_cast<PFN_vkCmdDraw>(Vk.GetDeviceProcAddr(Device, "vkCmdDraw"));
			Vk.CmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(Vk.GetDeviceProcAddr(Device, "vkCmdSetViewport"));
			Vk.CmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(Vk.GetDeviceProcAddr(Device, "vkCmdSetScissor"));
			Vk.CmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(Vk.GetDeviceProcAddr(Device, "vkCmdPushConstants"));
			Vk.CreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(Vk.GetDeviceProcAddr(Device, "vkCreateQueryPool"));
			Vk.DestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(Vk.GetDeviceProcAddr(Device, "vkDestroyQueryPool"));
			Vk.CmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(Vk.GetDeviceProcAddr(Device, "vkCmdResetQueryPool"));
			Vk.CmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(Vk.GetDeviceProcAddr(Device, "vkCmdWriteTimestamp"));
			Vk.GetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(Vk.GetDeviceProcAddr(Device, "vkGetQueryPoolResults"));
			return Vk.DestroyDevice != NULL &&
				Vk.DeviceWaitIdle != NULL &&
				Vk.GetDeviceQueue != NULL &&
				Vk.CreateSwapchainKHR != NULL &&
				Vk.DestroySwapchainKHR != NULL &&
				Vk.GetSwapchainImagesKHR != NULL &&
				Vk.CreateImageView != NULL &&
				Vk.DestroyImageView != NULL &&
				Vk.CreateCommandPool != NULL &&
				Vk.DestroyCommandPool != NULL &&
				Vk.AllocateCommandBuffers != NULL &&
				Vk.BeginCommandBuffer != NULL &&
				Vk.EndCommandBuffer != NULL &&
				Vk.CmdPipelineBarrier != NULL &&
				Vk.CmdClearColorImage != NULL &&
				Vk.CreateSemaphore != NULL &&
				Vk.DestroySemaphore != NULL &&
				Vk.CreateFence != NULL &&
				Vk.DestroyFence != NULL &&
				Vk.WaitForFences != NULL &&
				Vk.ResetFences != NULL &&
				Vk.ResetCommandBuffer != NULL &&
				Vk.QueueSubmit != NULL &&
				Vk.QueuePresentKHR != NULL &&
				Vk.AcquireNextImageKHR != NULL &&
				Vk.CreateBuffer != NULL &&
				Vk.DestroyBuffer != NULL &&
				Vk.GetBufferMemoryRequirements != NULL &&
				Vk.AllocateMemory != NULL &&
				Vk.FreeMemory != NULL &&
				Vk.BindBufferMemory != NULL &&
				Vk.MapMemory != NULL &&
				Vk.UnmapMemory != NULL &&
				Vk.CmdCopyBufferToImage != NULL &&
				Vk.CreateImage != NULL &&
				Vk.DestroyImage != NULL &&
				Vk.GetImageMemoryRequirements != NULL &&
				Vk.BindImageMemory != NULL &&
				Vk.CreateSampler != NULL &&
				Vk.DestroySampler != NULL &&
				Vk.CreateDescriptorSetLayout != NULL &&
				Vk.DestroyDescriptorSetLayout != NULL &&
				Vk.CreateDescriptorPool != NULL &&
				Vk.DestroyDescriptorPool != NULL &&
				Vk.AllocateDescriptorSets != NULL &&
				Vk.UpdateDescriptorSets != NULL &&
				Vk.CreatePipelineLayout != NULL &&
				Vk.DestroyPipelineLayout != NULL &&
				Vk.CreateRenderPass != NULL &&
				Vk.DestroyRenderPass != NULL &&
				Vk.CreateFramebuffer != NULL &&
				Vk.DestroyFramebuffer != NULL &&
				Vk.CreateShaderModule != NULL &&
				Vk.DestroyShaderModule != NULL &&
				Vk.CreateGraphicsPipelines != NULL &&
				Vk.DestroyPipeline != NULL &&
				Vk.CmdBeginRenderPass != NULL &&
				Vk.CmdEndRenderPass != NULL &&
				Vk.CmdBindPipeline != NULL &&
				Vk.CmdBindDescriptorSets != NULL &&
				Vk.CmdBindVertexBuffers != NULL &&
				Vk.CmdDraw != NULL &&
				Vk.CmdSetViewport != NULL &&
				Vk.CmdSetScissor != NULL &&
				Vk.CmdPushConstants != NULL;
		}

		bool CreateInstance()
		{
			VkApplicationInfo appInfo;
			memset(&appInfo, 0, sizeof(appInfo));
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = GAMENAME;
			appInfo.applicationVersion = 1;
			appInfo.pEngineName = "InfiniDoom";
			appInfo.engineVersion = 1;
			appInfo.apiVersion = VK_API_VERSION_1_0;

			const char *extensions[] =
			{
				VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
			};

			VkInstanceCreateInfo createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
			createInfo.ppEnabledExtensionNames = extensions;

			VkResult result = Vk.CreateInstance(&createInfo, NULL, &Instance);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateInstance failed (%d).\n", (int)result);
				return false;
			}
			if (!LoadInstanceFunctions())
			{
				Printf(TEXTCOLOR_RED "Vulkan: required instance functions are unavailable.\n");
				return false;
			}
			return true;
		}

		bool CreateSurface()
		{
#ifdef _WIN32
			if (Window == NULL || g_hInst == NULL)
			{
				Printf(TEXTCOLOR_RED "Vulkan: Win32 window is unavailable for surface creation.\n");
				return false;
			}

			VkWin32SurfaceCreateInfoKHR createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			createInfo.hinstance = g_hInst;
			createInfo.hwnd = Window;

			VkResult result = Vk.CreateWin32SurfaceKHR(Instance, &createInfo, NULL, &Surface);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateWin32SurfaceKHR failed (%d).\n", (int)result);
				return false;
			}
			return true;
#else
			Printf(TEXTCOLOR_ORANGE "Vulkan: platform surface creation is not implemented for this target yet.\n");
			return false;
#endif
		}

		bool ChoosePhysicalDevice()
		{
			unsigned int count = 0;
			VkResult result = Vk.EnumeratePhysicalDevices(Instance, &count, NULL);
			if (result != VK_SUCCESS || count == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no physical devices found (%d).\n", (int)result);
				return false;
			}

			VkPhysicalDevice devices[16];
			unsigned int queryCount = count;
			if (queryCount > 16)
			{
				queryCount = 16;
			}
			result = Vk.EnumeratePhysicalDevices(Instance, &queryCount, devices);
			if (result != VK_SUCCESS || queryCount == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: failed to enumerate physical devices (%d).\n", (int)result);
				return false;
			}
			DeviceCount = count;

			for (unsigned int i = 0; i < queryCount; ++i)
			{
				unsigned int queueCount = 0;
				Vk.GetPhysicalDeviceQueueFamilyProperties(devices[i], &queueCount, NULL);
				if (queueCount == 0)
				{
					continue;
				}

				VkQueueFamilyProperties queues[32];
				unsigned int queueQueryCount = queueCount;
				if (queueQueryCount > 32)
				{
					queueQueryCount = 32;
				}
				Vk.GetPhysicalDeviceQueueFamilyProperties(devices[i], &queueQueryCount, queues);
				for (unsigned int q = 0; q < queueQueryCount; ++q)
				{
					VkBool32 presentSupported = VK_FALSE;
					VkResult presentResult = Vk.GetPhysicalDeviceSurfaceSupportKHR(devices[i], q, Surface, &presentSupported);
					if ((queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
						queues[q].queueCount > 0 &&
						presentResult == VK_SUCCESS &&
						presentSupported)
					{
						PhysicalDevice = devices[i];
						GraphicsQueueFamily = q;
						Vk.GetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);
						Vk.GetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);
						TimestampQueriesSupported = queues[q].timestampValidBits > 0 && DeviceProperties.limits.timestampPeriod > 0.f;
						TimestampPeriod = DeviceProperties.limits.timestampPeriod;
						MemoryBudgetSupported = IsDeviceExtensionSupported(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
						PublishVulkanStats(this);
						return true;
					}
				}
			}

			Printf(TEXTCOLOR_RED "Vulkan: no graphics-capable queue family found.\n");
			return false;
		}

		bool CreateDevice()
		{
			const float queuePriority = 1.f;
			const char *deviceExtensions[2];
			unsigned int deviceExtensionCount = 0;
			deviceExtensions[deviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			if (MemoryBudgetSupported)
			{
				deviceExtensions[deviceExtensionCount++] = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
			}

			VkDeviceQueueCreateInfo queueInfo;
			memset(&queueInfo, 0, sizeof(queueInfo));
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = GraphicsQueueFamily;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &queuePriority;

			VkDeviceCreateInfo createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.queueCreateInfoCount = 1;
			createInfo.pQueueCreateInfos = &queueInfo;
			createInfo.enabledExtensionCount = deviceExtensionCount;
			createInfo.ppEnabledExtensionNames = deviceExtensions;

			VkResult result = Vk.CreateDevice(PhysicalDevice, &createInfo, NULL, &Device);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateDevice failed (%d).\n", (int)result);
				return false;
			}
			if (!LoadDeviceFunctions())
			{
				Printf(TEXTCOLOR_RED "Vulkan: required device functions are unavailable.\n");
				return false;
			}
			Vk.GetDeviceQueue(Device, GraphicsQueueFamily, 0, &GraphicsQueue);
			RefreshMemoryBudgetStats();
			return GraphicsQueue != NULL;
		}

		bool IsDeviceExtensionSupported(const char *extensionName) const
		{
			if (Vk.EnumerateDeviceExtensionProperties == NULL || PhysicalDevice == NULL || extensionName == NULL)
			{
				return false;
			}

			unsigned int count = 0;
			if (Vk.EnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &count, NULL) != VK_SUCCESS || count == 0)
			{
				return false;
			}

			VkExtensionProperties extensions[128];
			unsigned int queryCount = count;
			if (queryCount > 128)
			{
				queryCount = 128;
			}
			if (Vk.EnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &queryCount, extensions) != VK_SUCCESS)
			{
				return false;
			}

			for (unsigned int i = 0; i < queryCount; ++i)
			{
				if (strcmp(extensions[i].extensionName, extensionName) == 0)
				{
					return true;
				}
			}
			return false;
		}

		VkSurfaceFormatKHR ChooseSurfaceFormat(const VkSurfaceFormatKHR *formats, unsigned int count)
		{
			if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
			{
				VkSurfaceFormatKHR format;
				format.format = VK_FORMAT_B8G8R8A8_UNORM;
				format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				return format;
			}

			for (unsigned int i = 0; i < count; ++i)
			{
				if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
					formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return formats[i];
				}
			}
			return formats[0];
		}

		VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities)
		{
			if (capabilities.currentExtent.width != 0xffffffffu)
			{
				return capabilities.currentExtent;
			}

			VkExtent2D extent;
#ifdef _WIN32
			RECT rect;
			if (GetClientRect(Window, &rect))
			{
				extent.width = (unsigned int)(rect.right - rect.left);
				extent.height = (unsigned int)(rect.bottom - rect.top);
			}
			else
#endif
			{
				extent.width = 640;
				extent.height = 480;
			}

			if (extent.width < capabilities.minImageExtent.width)
			{
				extent.width = capabilities.minImageExtent.width;
			}
			if (extent.height < capabilities.minImageExtent.height)
			{
				extent.height = capabilities.minImageExtent.height;
			}
			if (extent.width > capabilities.maxImageExtent.width)
			{
				extent.width = capabilities.maxImageExtent.width;
			}
			if (extent.height > capabilities.maxImageExtent.height)
			{
				extent.height = capabilities.maxImageExtent.height;
			}
			return extent;
		}

		VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported)
		{
			const VkCompositeAlphaFlagBitsKHR choices[] =
			{
				VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
			};
			for (unsigned int i = 0; i < sizeof(choices) / sizeof(choices[0]); ++i)
			{
				if (supported & choices[i])
				{
					return choices[i];
				}
			}
			return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		}

		bool CreateSwapchain()
		{
			if (IsClientMinimized())
			{
				WindowMinimized = true;
				return false;
			}
			WindowMinimized = false;

			VkSurfaceCapabilitiesKHR capabilities;
			VkResult result = Vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &capabilities);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (%d).\n", (int)result);
				return false;
			}

			unsigned int formatCount = 0;
			result = Vk.GetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, NULL);
			if (result != VK_SUCCESS || formatCount == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no surface formats found (%d).\n", (int)result);
				return false;
			}

			VkSurfaceFormatKHR formats[64];
			unsigned int queryFormatCount = formatCount;
			if (queryFormatCount > 64)
			{
				queryFormatCount = 64;
			}
			result = Vk.GetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &queryFormatCount, formats);
			if (result != VK_SUCCESS || queryFormatCount == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: failed to enumerate surface formats (%d).\n", (int)result);
				return false;
			}

			VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats, queryFormatCount);
			VkExtent2D extent = ChooseSwapchainExtent(capabilities);
			if (extent.width == 0 || extent.height == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: swapchain extent is empty.\n");
				return false;
			}

			unsigned int imageCount = capabilities.minImageCount + 1;
			if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			{
				imageCount = capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = Surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.preTransform = capabilities.currentTransform;
			createInfo.compositeAlpha = ChooseCompositeAlpha(capabilities.supportedCompositeAlpha);
			createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			createInfo.clipped = VK_TRUE;
			createInfo.oldSwapchain = VK_NULL_HANDLE;

			result = Vk.CreateSwapchainKHR(Device, &createInfo, NULL, &Swapchain);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateSwapchainKHR failed (%d).\n", (int)result);
				return false;
			}

			SwapchainImageCount = 0;
			result = Vk.GetSwapchainImagesKHR(Device, Swapchain, &SwapchainImageCount, NULL);
			if (result != VK_SUCCESS || SwapchainImageCount == 0)
			{
				Printf(TEXTCOLOR_RED "Vulkan: failed to query swapchain images (%d).\n", (int)result);
				return false;
			}
			if (SwapchainImageCount > MaxSwapchainImages)
			{
				Printf(TEXTCOLOR_RED "Vulkan: swapchain returned too many images (%u).\n", SwapchainImageCount);
				return false;
			}
			result = Vk.GetSwapchainImagesKHR(Device, Swapchain, &SwapchainImageCount, SwapchainImages);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: failed to enumerate swapchain images (%d).\n", (int)result);
				return false;
			}

			SwapchainFormat = surfaceFormat.format;
			SwapchainColorSpace = surfaceFormat.colorSpace;
			SwapchainExtent = extent;
			return true;
		}

		bool CreateImageViews()
		{
			for (unsigned int i = 0; i < SwapchainImageCount; ++i)
			{
				VkImageViewCreateInfo createInfo;
				memset(&createInfo, 0, sizeof(createInfo));
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = SwapchainImages[i];
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = SwapchainFormat;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				VkResult result = Vk.CreateImageView(Device, &createInfo, NULL, &SwapchainImageViews[i]);
				if (result != VK_SUCCESS)
				{
					Printf(TEXTCOLOR_RED "Vulkan: vkCreateImageView failed (%d).\n", (int)result);
					return false;
				}
				SwapchainViewCount++;
			}
			return true;
		}

		bool CreateRenderPass()
		{
			VkAttachmentDescription attachments[2];
			memset(attachments, 0, sizeof(attachments));
			attachments[0].format = SwapchainFormat;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			attachments[1].format = VK_FORMAT_D32_SFLOAT;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorRef;
			memset(&colorRef, 0, sizeof(colorRef));
			colorRef.attachment = 0;
			colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthRef;
			memset(&depthRef, 0, sizeof(depthRef));
			depthRef.attachment = 1;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass;
			memset(&subpass, 0, sizeof(subpass));
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef;
			subpass.pDepthStencilAttachment = &depthRef;

			VkRenderPassCreateInfo createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = 2;
			createInfo.pAttachments = attachments;
			createInfo.subpassCount = 1;
			createInfo.pSubpasses = &subpass;

			VkResult result = Vk.CreateRenderPass(Device, &createInfo, NULL, &RenderPass);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateRenderPass failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		bool CreateSwapchainFramebuffers()
		{
			if (DepthImageView == VK_NULL_HANDLE && !CreateDepthResources())
			{
				return false;
			}
			for (unsigned int i = 0; i < SwapchainImageCount; ++i)
			{
				VkImageView attachments[] = { SwapchainImageViews[i], DepthImageView };
				VkFramebufferCreateInfo createInfo;
				memset(&createInfo, 0, sizeof(createInfo));
				createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				createInfo.renderPass = RenderPass;
				createInfo.attachmentCount = 2;
				createInfo.pAttachments = attachments;
				createInfo.width = SwapchainExtent.width;
				createInfo.height = SwapchainExtent.height;
				createInfo.layers = 1;
				VkResult result = Vk.CreateFramebuffer(Device, &createInfo, NULL, &SwapchainFramebuffers[i]);
				if (result != VK_SUCCESS)
				{
					Printf(TEXTCOLOR_RED "Vulkan: vkCreateFramebuffer failed (%d).\n", (int)result);
					return false;
				}
			}
			return true;
		}

		VkShaderModule CreateShaderModule(const unsigned int *code, unsigned int size)
		{
			VkShaderModuleCreateInfo createInfo;
			memset(&createInfo, 0, sizeof(createInfo));
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = size;
			createInfo.pCode = code;

			VkShaderModule module = VK_NULL_HANDLE;
			VkResult result = Vk.CreateShaderModule(Device, &createInfo, NULL, &module);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateShaderModule failed (%d).\n", (int)result);
				return VK_NULL_HANDLE;
			}
			return module;
		}

		bool CreateDescriptorResources()
		{
			VkDescriptorSetLayoutBinding bindings[2];
			memset(bindings, 0, sizeof(bindings));
			for (unsigned int i = 0; i < 2; ++i)
			{
				bindings[i].binding = i;
				bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				bindings[i].descriptorCount = 1;
				bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			VkDescriptorSetLayoutCreateInfo layoutInfo;
			memset(&layoutInfo, 0, sizeof(layoutInfo));
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = 2;
			layoutInfo.pBindings = bindings;
			VkResult result = Vk.CreateDescriptorSetLayout(Device, &layoutInfo, NULL, &DescriptorSetLayout);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateDescriptorSetLayout failed (%d).\n", (int)result);
				return false;
			}

			VkDescriptorPoolSize poolSize;
			memset(&poolSize, 0, sizeof(poolSize));
			poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSize.descriptorCount = 2;

			VkDescriptorPoolCreateInfo poolInfo;
			memset(&poolInfo, 0, sizeof(poolInfo));
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.maxSets = 1;
			poolInfo.poolSizeCount = 1;
			poolInfo.pPoolSizes = &poolSize;
			result = Vk.CreateDescriptorPool(Device, &poolInfo, NULL, &DescriptorPool);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateDescriptorPool failed (%d).\n", (int)result);
				return false;
			}

			VkDescriptorSetAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &DescriptorSetLayout;
			result = Vk.AllocateDescriptorSets(Device, &allocInfo, &DescriptorSet);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkAllocateDescriptorSets failed (%d).\n", (int)result);
				return false;
			}

			VkSamplerCreateInfo samplerInfo;
			memset(&samplerInfo, 0, sizeof(samplerInfo));
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			PresentFilterMode = ClampPresentFilterMode();
			VkFilter filter = PresentFilterMode == 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			samplerInfo.magFilter = filter;
			samplerInfo.minFilter = filter;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.maxLod = 0.f;
			result = Vk.CreateSampler(Device, &samplerInfo, NULL, &NearestSampler);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateSampler failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		bool CreateGraphicsPipeline()
		{
			if (RenderPass == VK_NULL_HANDLE && !CreateRenderPass())
			{
				return false;
			}
			if (DescriptorSetLayout == VK_NULL_HANDLE && !CreateDescriptorResources())
			{
				return false;
			}

			VkShaderModule vert = CreateShaderModule(PalettePresentVertSpv, PalettePresentVertSpvSize);
			VkShaderModule frag = CreateShaderModule(PalettePresentFragSpv, PalettePresentFragSpvSize);
			if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
			{
				if (vert != VK_NULL_HANDLE) Vk.DestroyShaderModule(Device, vert, NULL);
				if (frag != VK_NULL_HANDLE) Vk.DestroyShaderModule(Device, frag, NULL);
				return false;
			}

			VkPipelineShaderStageCreateInfo stages[2];
			memset(stages, 0, sizeof(stages));
			stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			stages[0].module = vert;
			stages[0].pName = "main";
			stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			stages[1].module = frag;
			stages[1].pName = "main";

			VkPipelineVertexInputStateCreateInfo vertexInput;
			memset(&vertexInput, 0, sizeof(vertexInput));
			vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			VkPipelineInputAssemblyStateCreateInfo inputAssembly;
			memset(&inputAssembly, 0, sizeof(inputAssembly));
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkPipelineViewportStateCreateInfo viewportState;
			memset(&viewportState, 0, sizeof(viewportState));
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rasterizer;
			memset(&rasterizer, 0, sizeof(rasterizer));
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.cullMode = VK_CULL_MODE_NONE;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.lineWidth = 1.f;

			VkPipelineMultisampleStateCreateInfo multisampling;
			memset(&multisampling, 0, sizeof(multisampling));
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState blendAttachment;
			memset(&blendAttachment, 0, sizeof(blendAttachment));
			blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VkPipelineColorBlendStateCreateInfo blending;
			memset(&blending, 0, sizeof(blending));
			blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blending.attachmentCount = 1;
			blending.pAttachments = &blendAttachment;

			VkPipelineDepthStencilStateCreateInfo depthStencil;
			memset(&depthStencil, 0, sizeof(depthStencil));
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_FALSE;
			depthStencil.depthWriteEnable = VK_FALSE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

			VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicState;
			memset(&dynamicState, 0, sizeof(dynamicState));
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = 2;
			dynamicState.pDynamicStates = dynamicStates;

			VkPipelineLayoutCreateInfo pipelineLayoutInfo;
			memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &DescriptorSetLayout;

			VkPushConstantRange pushConstantRange;
			memset(&pushConstantRange, 0, sizeof(pushConstantRange));
			pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(PresentPushConstants);
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

			VkResult result = Vk.CreatePipelineLayout(Device, &pipelineLayoutInfo, NULL, &PipelineLayout);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreatePipelineLayout failed (%d).\n", (int)result);
				Vk.DestroyShaderModule(Device, vert, NULL);
				Vk.DestroyShaderModule(Device, frag, NULL);
				return false;
			}

			VkGraphicsPipelineCreateInfo pipelineInfo;
			memset(&pipelineInfo, 0, sizeof(pipelineInfo));
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = stages;
			pipelineInfo.pVertexInputState = &vertexInput;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pColorBlendState = &blending;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = PipelineLayout;
			pipelineInfo.renderPass = RenderPass;
			pipelineInfo.subpass = 0;
			result = Vk.CreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &GraphicsPipeline);

			Vk.DestroyShaderModule(Device, vert, NULL);
			Vk.DestroyShaderModule(Device, frag, NULL);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateGraphicsPipelines failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		bool CreateProbePipeline(VkPipelineLayout &pipelineLayout, VkPipeline &pipeline, bool enableBlend, bool enableDepthTest, bool enableDepthWrite, const char *label)
		{
			if (RenderPass == VK_NULL_HANDLE && !CreateRenderPass())
			{
				return false;
			}

			VkShaderModule vert = CreateShaderModule(SceneProbeVertSpv, SceneProbeVertSpvSize);
			VkShaderModule frag = CreateShaderModule(SceneProbeFragSpv, SceneProbeFragSpvSize);
			if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
			{
				if (vert != VK_NULL_HANDLE) Vk.DestroyShaderModule(Device, vert, NULL);
				if (frag != VK_NULL_HANDLE) Vk.DestroyShaderModule(Device, frag, NULL);
				return false;
			}

			VkPipelineShaderStageCreateInfo stages[2];
			memset(stages, 0, sizeof(stages));
			stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			stages[0].module = vert;
			stages[0].pName = "main";
			stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			stages[1].module = frag;
			stages[1].pName = "main";

			VkPipelineVertexInputStateCreateInfo vertexInput;
			memset(&vertexInput, 0, sizeof(vertexInput));
			vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			VkVertexInputBindingDescription vertexBinding;
			memset(&vertexBinding, 0, sizeof(vertexBinding));
			vertexBinding.binding = 0;
			vertexBinding.stride = sizeof(SceneProbeVertex);
			vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			VkVertexInputAttributeDescription vertexAttributes[2];
			memset(vertexAttributes, 0, sizeof(vertexAttributes));
			vertexAttributes[0].location = 0;
			vertexAttributes[0].binding = 0;
			vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributes[0].offset = offsetof(SceneProbeVertex, Position);
			vertexAttributes[1].location = 1;
			vertexAttributes[1].binding = 0;
			vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributes[1].offset = offsetof(SceneProbeVertex, Color);
			vertexInput.vertexBindingDescriptionCount = 1;
			vertexInput.pVertexBindingDescriptions = &vertexBinding;
			vertexInput.vertexAttributeDescriptionCount = 2;
			vertexInput.pVertexAttributeDescriptions = vertexAttributes;

			VkPipelineInputAssemblyStateCreateInfo inputAssembly;
			memset(&inputAssembly, 0, sizeof(inputAssembly));
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkPipelineViewportStateCreateInfo viewportState;
			memset(&viewportState, 0, sizeof(viewportState));
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rasterizer;
			memset(&rasterizer, 0, sizeof(rasterizer));
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.cullMode = VK_CULL_MODE_NONE;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.lineWidth = 1.f;

			VkPipelineMultisampleStateCreateInfo multisampling;
			memset(&multisampling, 0, sizeof(multisampling));
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState blendAttachment;
			memset(&blendAttachment, 0, sizeof(blendAttachment));
			blendAttachment.blendEnable = enableBlend ? VK_TRUE : VK_FALSE;
			blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
			blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VkPipelineColorBlendStateCreateInfo blending;
			memset(&blending, 0, sizeof(blending));
			blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blending.attachmentCount = 1;
			blending.pAttachments = &blendAttachment;

			VkPipelineDepthStencilStateCreateInfo depthStencil;
			memset(&depthStencil, 0, sizeof(depthStencil));
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
			depthStencil.depthWriteEnable = enableDepthWrite ? VK_TRUE : VK_FALSE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.stencilTestEnable = VK_FALSE;

			VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicState;
			memset(&dynamicState, 0, sizeof(dynamicState));
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = 2;
			dynamicState.pDynamicStates = dynamicStates;

			VkPipelineLayoutCreateInfo layoutInfo;
			memset(&layoutInfo, 0, sizeof(layoutInfo));
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			VkPushConstantRange pushConstantRange;
			memset(&pushConstantRange, 0, sizeof(pushConstantRange));
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(SceneProbePushConstants);
			layoutInfo.pushConstantRangeCount = 1;
			layoutInfo.pPushConstantRanges = &pushConstantRange;

			VkResult result = Vk.CreatePipelineLayout(Device, &layoutInfo, NULL, &pipelineLayout);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: %s vkCreatePipelineLayout failed (%d).\n", label, (int)result);
				Vk.DestroyShaderModule(Device, vert, NULL);
				Vk.DestroyShaderModule(Device, frag, NULL);
				return false;
			}

			VkGraphicsPipelineCreateInfo pipelineInfo;
			memset(&pipelineInfo, 0, sizeof(pipelineInfo));
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = stages;
			pipelineInfo.pVertexInputState = &vertexInput;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pColorBlendState = &blending;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.renderPass = RenderPass;
			pipelineInfo.subpass = 0;
			result = Vk.CreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline);

			Vk.DestroyShaderModule(Device, vert, NULL);
			Vk.DestroyShaderModule(Device, frag, NULL);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: %s vkCreateGraphicsPipelines failed (%d).\n", label, (int)result);
				if (pipelineLayout != VK_NULL_HANDLE)
				{
					Vk.DestroyPipelineLayout(Device, pipelineLayout, NULL);
					pipelineLayout = VK_NULL_HANDLE;
				}
				return false;
			}
			Printf(TEXTCOLOR_GREEN "Vulkan %s pipeline active.\n", label);
			return true;
		}

		bool CreateSceneProbePipeline()
		{
			return CreateProbePipeline(ProbePipelineLayout, ProbePipeline, true, false, false, "scene probe");
		}

		bool CreateWorldProbePipeline()
		{
			return CreateProbePipeline(WorldProbePipelineLayout, WorldProbePipeline, false, true, true, "world probe");
		}

		void DestroyProbeVertexBuffer()
		{
			if (Device == NULL)
			{
				ProbeVertexBuffer = VK_NULL_HANDLE;
				ProbeVertexMemory = VK_NULL_HANDLE;
				ProbeVertexPtr = NULL;
				ProbeVertexBufferSize = 0;
				ProbeVertexDrawCount = 0;
				WorldDrawFirstVertex = 0;
				WorldDrawDrawCount = 0;
				SceneProbeFirstVertex = 0;
				SceneProbeDrawCount = 0;
				WorldProbeFirstVertex = 0;
				WorldProbeDrawCount = 0;
				return;
			}
			if (ProbeVertexMemory != VK_NULL_HANDLE && ProbeVertexPtr != NULL && Vk.UnmapMemory != NULL)
			{
				Vk.UnmapMemory(Device, ProbeVertexMemory);
			}
			ProbeVertexPtr = NULL;
			if (ProbeVertexBuffer != VK_NULL_HANDLE && Vk.DestroyBuffer != NULL)
			{
				Vk.DestroyBuffer(Device, ProbeVertexBuffer, NULL);
			}
			ProbeVertexBuffer = VK_NULL_HANDLE;
			if (ProbeVertexMemory != VK_NULL_HANDLE && Vk.FreeMemory != NULL)
			{
				Vk.FreeMemory(Device, ProbeVertexMemory, NULL);
			}
			ProbeVertexMemory = VK_NULL_HANDLE;
			ProbeVertexBufferSize = 0;
			ProbeVertexDrawCount = 0;
			WorldDrawFirstVertex = 0;
			WorldDrawDrawCount = 0;
			SceneProbeFirstVertex = 0;
			SceneProbeDrawCount = 0;
			WorldProbeFirstVertex = 0;
			WorldProbeDrawCount = 0;
		}

		void AppendProbeVertex(SceneProbeVertex *vertices, unsigned int &count, float x, float y, float z, float r, float g, float b)
		{
			if (count >= ProbeVertexMaxCount)
			{
				return;
			}
			vertices[count].Position[0] = x;
			vertices[count].Position[1] = y;
			vertices[count].Position[2] = z;
			vertices[count].Color[0] = r;
			vertices[count].Color[1] = g;
			vertices[count].Color[2] = b;
			++count;
		}

		bool AppendWorldProbeWall(SceneProbeVertex *vertices, unsigned int &count, const line_t *line, float r, float g, float b)
		{
			if (line == NULL || line->v1 == NULL || line->v2 == NULL || line->frontsector == NULL)
			{
				return false;
			}
			if (line->backsector != NULL)
			{
				return false;
			}
			if (count + 6 > ProbeVertexMaxCount)
			{
				return false;
			}

			float x1 = FIXED2FLOAT(line->v1->x);
			float y1 = FIXED2FLOAT(line->v1->y);
			float x2 = FIXED2FLOAT(line->v2->x);
			float y2 = FIXED2FLOAT(line->v2->y);
			float floor1 = FIXED2FLOAT(line->frontsector->floorplane.ZatPoint(line->v1));
			float floor2 = FIXED2FLOAT(line->frontsector->floorplane.ZatPoint(line->v2));
			float ceiling1 = FIXED2FLOAT(line->frontsector->ceilingplane.ZatPoint(line->v1));
			float ceiling2 = FIXED2FLOAT(line->frontsector->ceilingplane.ZatPoint(line->v2));

			const double pi = 3.14159265358979323846;
			const double yawRadians = (180.0 + (double)vk_world_yaw_sign * ANGLE2DBL(viewangle)) * (pi / 180.0);
			const double forwardX = cos(yawRadians);
			const double forwardY = sin(yawRadians);
			const double rightX = forwardY;
			const double rightY = -forwardX;
			const double camX = FIXED2FLOAT(viewx);
			const double camY = FIXED2FLOAT(viewy);
			const double nearDepth = 8.0;

			double relX1 = x1 - camX;
			double relY1 = y1 - camY;
			double relX2 = x2 - camX;
			double relY2 = y2 - camY;
			double depth1 = relX1 * forwardX + relY1 * forwardY;
			double depth2 = relX2 * forwardX + relY2 * forwardY;
			if (depth1 <= nearDepth && depth2 <= nearDepth)
			{
				return false;
			}
			if (depth1 <= nearDepth || depth2 <= nearDepth)
			{
				const double denom = depth2 - depth1;
				if (fabs(denom) < 0.0001)
				{
					return false;
				}
				const float t = (float)((nearDepth - depth1) / denom);
				if (depth1 <= nearDepth)
				{
					x1 += (x2 - x1) * t;
					y1 += (y2 - y1) * t;
					floor1 += (floor2 - floor1) * t;
					ceiling1 += (ceiling2 - ceiling1) * t;
				}
				else
				{
					x2 = x1 + (x2 - x1) * t;
					y2 = y1 + (y2 - y1) * t;
					floor2 = floor1 + (floor2 - floor1) * t;
					ceiling2 = ceiling1 + (ceiling2 - ceiling1) * t;
				}
				relX1 = x1 - camX;
				relY1 = y1 - camY;
				relX2 = x2 - camX;
				relY2 = y2 - camY;
				depth1 = relX1 * forwardX + relY1 * forwardY;
				depth2 = relX2 * forwardX + relY2 * forwardY;
			}

			double fovDegrees = (double)R_GetFOV();
			if (fovDegrees < 5.0)
			{
				fovDegrees = 5.0;
			}
			else if (fovDegrees > 170.0)
			{
				fovDegrees = 170.0;
			}
			double side1 = relX1 * rightX + relY1 * rightY;
			double side2 = relX2 * rightX + relY2 * rightY;
			const double tanX = tan(fovDegrees * (pi / 360.0));
			const double padding = 4.0;
			for (unsigned int plane = 0; plane < 2; ++plane)
			{
				double value1 = plane == 0 ? side1 + depth1 * tanX + padding : -side1 + depth1 * tanX + padding;
				double value2 = plane == 0 ? side2 + depth2 * tanX + padding : -side2 + depth2 * tanX + padding;
				if (value1 < 0.0 && value2 < 0.0)
				{
					return false;
				}
				if (value1 < 0.0 || value2 < 0.0)
				{
					const double denom = value1 - value2;
					if (fabs(denom) < 0.0001)
					{
						return false;
					}
					const float t = (float)(value1 / denom);
					if (value1 < 0.0)
					{
						x1 += (x2 - x1) * t;
						y1 += (y2 - y1) * t;
						floor1 += (floor2 - floor1) * t;
						ceiling1 += (ceiling2 - ceiling1) * t;
						depth1 += (depth2 - depth1) * t;
						side1 += (side2 - side1) * t;
					}
					else
					{
						x2 = x1 + (x2 - x1) * t;
						y2 = y1 + (y2 - y1) * t;
						floor2 = floor1 + (floor2 - floor1) * t;
						ceiling2 = ceiling1 + (ceiling2 - ceiling1) * t;
						depth2 = depth1 + (depth2 - depth1) * t;
						side2 = side1 + (side2 - side1) * t;
					}
				}
			}
			if (fabs(x2 - x1) + fabs(y2 - y1) < 0.001)
			{
				return false;
			}

			AppendProbeVertex(vertices, count, x1, y1, ceiling1, r, g, b);
			AppendProbeVertex(vertices, count, x2, y2, ceiling2, r, g, b);
			AppendProbeVertex(vertices, count, x2, y2, floor2, r * 0.65f, g * 0.65f, b * 0.65f);
			AppendProbeVertex(vertices, count, x1, y1, ceiling1, r, g, b);
			AppendProbeVertex(vertices, count, x2, y2, floor2, r * 0.65f, g * 0.65f, b * 0.65f);
			AppendProbeVertex(vertices, count, x1, y1, floor1, r * 0.65f, g * 0.65f, b * 0.65f);
			return true;
		}

		void AppendWorldProbeVertices(SceneProbeVertex *vertices, unsigned int &count)
		{
			if (lines == NULL || numlines <= 0)
			{
				return;
			}
			const double camX = FIXED2FLOAT(viewx);
			const double camY = FIXED2FLOAT(viewy);
			const double maxDistance = 768.0;
			const double maxDistanceSquared = maxDistance * maxDistance;
			unsigned int walls = 0;
			for (int i = 0; i < numlines && walls < WorldProbeMaxWalls; ++i)
			{
				const line_t *line = &lines[i];
				if (line->v1 == NULL || line->v2 == NULL || line->frontsector == NULL || line->backsector != NULL)
				{
					continue;
				}
				const double midX = (FIXED2FLOAT(line->v1->x) + FIXED2FLOAT(line->v2->x)) * 0.5;
				const double midY = (FIXED2FLOAT(line->v1->y) + FIXED2FLOAT(line->v2->y)) * 0.5;
				const double dx = midX - camX;
				const double dy = midY - camY;
				if (dx * dx + dy * dy > maxDistanceSquared)
				{
					continue;
				}
				const float tint = (walls & 1) ? 0.85f : 1.0f;
				if (AppendWorldProbeWall(vertices, count, line, 0.05f * tint, 0.90f * tint, 0.35f * tint))
				{
					++walls;
				}
			}
		}

		void AppendWorldDrawVertices(SceneProbeVertex *vertices, unsigned int &count)
		{
			if (lines == NULL || numlines <= 0)
			{
				return;
			}
			const double camX = FIXED2FLOAT(viewx);
			const double camY = FIXED2FLOAT(viewy);
			const double maxDistance = 1024.0;
			const double maxDistanceSquared = maxDistance * maxDistance;
			unsigned int walls = 0;
			for (int i = 0; i < numlines && walls < WorldDrawMaxWalls; ++i)
			{
				const line_t *line = &lines[i];
				if (line->v1 == NULL || line->v2 == NULL || line->frontsector == NULL || line->backsector != NULL)
				{
					continue;
				}
				const double midX = (FIXED2FLOAT(line->v1->x) + FIXED2FLOAT(line->v2->x)) * 0.5;
				const double midY = (FIXED2FLOAT(line->v1->y) + FIXED2FLOAT(line->v2->y)) * 0.5;
				const double dx = midX - camX;
				const double dy = midY - camY;
				if (dx * dx + dy * dy > maxDistanceSquared)
				{
					continue;
				}
				const float tint = (walls & 1) ? 0.82f : 1.0f;
				if (AppendWorldProbeWall(vertices, count, line, 0.50f * tint, 0.43f * tint, 0.32f * tint))
				{
					++walls;
				}
			}
		}

		void AppendSceneProbeVertices(SceneProbeVertex *vertices, unsigned int &count)
		{
			if (count + SceneProbeVertexCount > ProbeVertexMaxCount)
			{
				return;
			}
			const double pi = 3.14159265358979323846;
			const double angleRadians = (180.0 + (double)vk_world_yaw_sign * ANGLE2DBL(viewangle)) * (pi / 180.0);
			const double forwardX = cos(angleRadians);
			const double forwardY = sin(angleRadians);
			const double rightX = forwardY;
			const double rightY = -forwardX;
			const double camX = FIXED2FLOAT(viewx);
			const double camY = FIXED2FLOAT(viewy);
			const double camZ = FIXED2FLOAT(viewz);
			const double centerForward = 128.0;
			const double centerRight = 56.0;
			const double halfWidth = 32.0;
			const double baseZ = camZ - 20.0;
			const double topZ = camZ + 42.0;
			const double centerX = camX + forwardX * centerForward + rightX * centerRight;
			const double centerY = camY + forwardY * centerForward + rightY * centerRight;
			SceneProbeVertex sceneVertices[SceneProbeVertexCount] =
			{
				{ { (float)centerX, (float)centerY, (float)topZ }, { 1.0f, 0.0f, 1.0f } },
				{ { (float)(centerX - rightX * halfWidth), (float)(centerY - rightY * halfWidth), (float)baseZ }, { 0.0f, 1.0f, 1.0f } },
				{ { (float)(centerX + rightX * halfWidth), (float)(centerY + rightY * halfWidth), (float)baseZ }, { 1.0f, 1.0f, 0.0f } }
			};
			memcpy(&vertices[count], sceneVertices, sizeof(sceneVertices));
			count += SceneProbeVertexCount;
		}

		void UpdateProbeVertices()
		{
			if (ProbeVertexPtr == NULL)
			{
				ProbeVertexDrawCount = 0;
				WorldDrawFirstVertex = 0;
				WorldDrawDrawCount = 0;
				SceneProbeFirstVertex = 0;
				SceneProbeDrawCount = 0;
				WorldProbeFirstVertex = 0;
				WorldProbeDrawCount = 0;
				return;
			}

			SceneProbeVertex *vertices = static_cast<SceneProbeVertex *>(ProbeVertexPtr);
			unsigned int count = 0;
			WorldDrawFirstVertex = 0;
			WorldDrawDrawCount = 0;
			SceneProbeFirstVertex = 0;
			SceneProbeDrawCount = 0;
			WorldProbeFirstVertex = 0;
			WorldProbeDrawCount = 0;
			if (vk_draw_world)
			{
				WorldDrawFirstVertex = count;
				AppendWorldDrawVertices(vertices, count);
				WorldDrawDrawCount = count - WorldDrawFirstVertex;
			}
			if (vk_world_probe)
			{
				WorldProbeFirstVertex = count;
				AppendWorldProbeVertices(vertices, count);
				WorldProbeDrawCount = count - WorldProbeFirstVertex;
			}
			if (vk_scene_probe)
			{
				SceneProbeFirstVertex = count;
				AppendSceneProbeVertices(vertices, count);
				SceneProbeDrawCount = count - SceneProbeFirstVertex;
			}
			ProbeVertexDrawCount = count;
		}

		bool EnsureProbeVertexBuffer()
		{
			const VkDeviceSize needed = sizeof(SceneProbeVertex) * ProbeVertexMaxCount;
			if (ProbeVertexBuffer != VK_NULL_HANDLE && ProbeVertexPtr != NULL && ProbeVertexBufferSize >= needed)
			{
				UpdateProbeVertices();
				return true;
			}

			DestroyProbeVertexBuffer();

			VkBufferCreateInfo bufferInfo;
			memset(&bufferInfo, 0, sizeof(bufferInfo));
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = needed;
			bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkResult result = Vk.CreateBuffer(Device, &bufferInfo, NULL, &ProbeVertexBuffer);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: probe vkCreateBuffer failed (%d).\n", (int)result);
				return false;
			}

			VkMemoryRequirements requirements;
			Vk.GetBufferMemoryRequirements(Device, ProbeVertexBuffer, &requirements);
			unsigned int memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (memoryType == ~0u)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no host-visible probe vertex memory type found.\n");
				DestroyProbeVertexBuffer();
				return false;
			}

			VkMemoryAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = requirements.size;
			allocInfo.memoryTypeIndex = memoryType;

			result = Vk.AllocateMemory(Device, &allocInfo, NULL, &ProbeVertexMemory);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: probe vkAllocateMemory failed (%d).\n", (int)result);
				DestroyProbeVertexBuffer();
				return false;
			}
			result = Vk.BindBufferMemory(Device, ProbeVertexBuffer, ProbeVertexMemory, 0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: probe vkBindBufferMemory failed (%d).\n", (int)result);
				DestroyProbeVertexBuffer();
				return false;
			}
			result = Vk.MapMemory(Device, ProbeVertexMemory, 0, needed, 0, &ProbeVertexPtr);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: probe vkMapMemory failed (%d).\n", (int)result);
				DestroyProbeVertexBuffer();
				return false;
			}
			ProbeVertexBufferSize = needed;
			UpdateProbeVertices();
			PublishVulkanStats(this);
			return true;
		}

		void DestroyUploadBuffer()
		{
			if (Device == NULL)
			{
				UploadBuffer = VK_NULL_HANDLE;
				UploadMemory = VK_NULL_HANDLE;
				UploadPtr = NULL;
				UploadSize = 0;
				return;
			}
			if (UploadMemory != VK_NULL_HANDLE && UploadPtr != NULL && Vk.UnmapMemory != NULL)
			{
				Vk.UnmapMemory(Device, UploadMemory);
			}
			UploadPtr = NULL;
			if (UploadBuffer != VK_NULL_HANDLE && Vk.DestroyBuffer != NULL)
			{
				Vk.DestroyBuffer(Device, UploadBuffer, NULL);
			}
			UploadBuffer = VK_NULL_HANDLE;
			if (UploadMemory != VK_NULL_HANDLE && Vk.FreeMemory != NULL)
			{
				Vk.FreeMemory(Device, UploadMemory, NULL);
			}
			UploadMemory = VK_NULL_HANDLE;
			UploadSize = 0;
		}

		void DestroySwapchainResources()
		{
			if (Device == NULL)
			{
				Swapchain = VK_NULL_HANDLE;
				SwapchainImageCount = 0;
				SwapchainViewCount = 0;
				return;
			}
			for (unsigned int i = 0; i < MaxSwapchainImages; ++i)
			{
				if (SwapchainFramebuffers[i] != VK_NULL_HANDLE && Vk.DestroyFramebuffer != NULL)
				{
					Vk.DestroyFramebuffer(Device, SwapchainFramebuffers[i], NULL);
				}
				SwapchainFramebuffers[i] = VK_NULL_HANDLE;
			}
			DestroyDepthResources();
			for (unsigned int i = 0; i < SwapchainViewCount; ++i)
			{
				if (SwapchainImageViews[i] != VK_NULL_HANDLE && Vk.DestroyImageView != NULL)
				{
					Vk.DestroyImageView(Device, SwapchainImageViews[i], NULL);
				}
				SwapchainImageViews[i] = VK_NULL_HANDLE;
			}
			SwapchainViewCount = 0;
			if (Swapchain != VK_NULL_HANDLE && Vk.DestroySwapchainKHR != NULL)
			{
				Vk.DestroySwapchainKHR(Device, Swapchain, NULL);
			}
			Swapchain = VK_NULL_HANDLE;
			SwapchainImageCount = 0;
			for (unsigned int i = 0; i < MaxSwapchainImages; ++i)
			{
				SwapchainImages[i] = VK_NULL_HANDLE;
			}
			SwapchainFormat = VK_FORMAT_UNDEFINED;
			SwapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			SwapchainExtent.width = 0;
			SwapchainExtent.height = 0;
		}

		void DestroyPresentationResources()
		{
			DestroySourceImages();
			for (unsigned int i = 0; i < MaxSwapchainImages; ++i)
			{
				if (SwapchainFramebuffers[i] != VK_NULL_HANDLE && Vk.DestroyFramebuffer != NULL)
				{
					Vk.DestroyFramebuffer(Device, SwapchainFramebuffers[i], NULL);
				}
				SwapchainFramebuffers[i] = VK_NULL_HANDLE;
			}
			DestroyDepthResources();
			if (GraphicsPipeline != VK_NULL_HANDLE && Vk.DestroyPipeline != NULL)
			{
				Vk.DestroyPipeline(Device, GraphicsPipeline, NULL);
			}
			GraphicsPipeline = VK_NULL_HANDLE;
			if (ProbePipeline != VK_NULL_HANDLE && Vk.DestroyPipeline != NULL)
			{
				Vk.DestroyPipeline(Device, ProbePipeline, NULL);
			}
			ProbePipeline = VK_NULL_HANDLE;
			if (WorldProbePipeline != VK_NULL_HANDLE && Vk.DestroyPipeline != NULL)
			{
				Vk.DestroyPipeline(Device, WorldProbePipeline, NULL);
			}
			WorldProbePipeline = VK_NULL_HANDLE;
			if (PipelineLayout != VK_NULL_HANDLE && Vk.DestroyPipelineLayout != NULL)
			{
				Vk.DestroyPipelineLayout(Device, PipelineLayout, NULL);
			}
			PipelineLayout = VK_NULL_HANDLE;
			if (ProbePipelineLayout != VK_NULL_HANDLE && Vk.DestroyPipelineLayout != NULL)
			{
				Vk.DestroyPipelineLayout(Device, ProbePipelineLayout, NULL);
			}
			ProbePipelineLayout = VK_NULL_HANDLE;
			if (WorldProbePipelineLayout != VK_NULL_HANDLE && Vk.DestroyPipelineLayout != NULL)
			{
				Vk.DestroyPipelineLayout(Device, WorldProbePipelineLayout, NULL);
			}
			WorldProbePipelineLayout = VK_NULL_HANDLE;
			if (RenderPass != VK_NULL_HANDLE && Vk.DestroyRenderPass != NULL)
			{
				Vk.DestroyRenderPass(Device, RenderPass, NULL);
			}
			RenderPass = VK_NULL_HANDLE;
			if (DescriptorPool != VK_NULL_HANDLE && Vk.DestroyDescriptorPool != NULL)
			{
				Vk.DestroyDescriptorPool(Device, DescriptorPool, NULL);
			}
			DescriptorPool = VK_NULL_HANDLE;
			DescriptorSet = VK_NULL_HANDLE;
			if (DescriptorSetLayout != VK_NULL_HANDLE && Vk.DestroyDescriptorSetLayout != NULL)
			{
				Vk.DestroyDescriptorSetLayout(Device, DescriptorSetLayout, NULL);
			}
			DescriptorSetLayout = VK_NULL_HANDLE;
			if (NearestSampler != VK_NULL_HANDLE && Vk.DestroySampler != NULL)
			{
				Vk.DestroySampler(Device, NearestSampler, NULL);
			}
			NearestSampler = VK_NULL_HANDLE;
			GpuPresentationReady = false;
			PresentFilterMode = -1;
		}

		bool GetClientExtent(unsigned int &width, unsigned int &height) const
		{
#ifdef _WIN32
			if (Window == NULL)
			{
				width = 0;
				height = 0;
				return false;
			}
			RECT rect;
			if (!GetClientRect(Window, &rect))
			{
				width = 0;
				height = 0;
				return false;
			}
			int clientWidth = rect.right - rect.left;
			int clientHeight = rect.bottom - rect.top;
			if (clientWidth <= 0 || clientHeight <= 0)
			{
				width = 0;
				height = 0;
				return true;
			}
			width = (unsigned int)clientWidth;
			height = (unsigned int)clientHeight;
			return true;
#else
			width = SwapchainExtent.width;
			height = SwapchainExtent.height;
			return width > 0 && height > 0;
#endif
		}

		bool IsClientMinimized() const
		{
			unsigned int width = 0;
			unsigned int height = 0;
			return GetClientExtent(width, height) && (width == 0 || height == 0);
		}

		bool EnsureSwapchainMatchesWindow()
		{
			unsigned int clientWidth = 0;
			unsigned int clientHeight = 0;
			if (!GetClientExtent(clientWidth, clientHeight))
			{
				return false;
			}
			if (clientWidth == 0 || clientHeight == 0)
			{
				WindowMinimized = true;
				return true;
			}
			if (Swapchain == VK_NULL_HANDLE ||
				SwapchainExtent.width != clientWidth ||
				SwapchainExtent.height != clientHeight)
			{
				return RecreateSwapchainForWindow();
			}
			return true;
		}

		bool CreateCommandResources()
		{
			VkCommandPoolCreateInfo poolInfo;
			memset(&poolInfo, 0, sizeof(poolInfo));
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = GraphicsQueueFamily;

			VkResult result = Vk.CreateCommandPool(Device, &poolInfo, NULL, &CommandPool);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateCommandPool failed (%d).\n", (int)result);
				return false;
			}

			VkCommandBufferAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = CommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			result = Vk.AllocateCommandBuffers(Device, &allocInfo, &CommandBuffer);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkAllocateCommandBuffers failed (%d).\n", (int)result);
				return false;
			}

			VkSemaphoreCreateInfo semaphoreInfo;
			memset(&semaphoreInfo, 0, sizeof(semaphoreInfo));
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			result = Vk.CreateSemaphore(Device, &semaphoreInfo, NULL, &ImageAvailableSemaphore);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateSemaphore image-available failed (%d).\n", (int)result);
				return false;
			}
			result = Vk.CreateSemaphore(Device, &semaphoreInfo, NULL, &RenderFinishedSemaphore);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateSemaphore render-finished failed (%d).\n", (int)result);
				return false;
			}

			VkFenceCreateInfo fenceInfo;
			memset(&fenceInfo, 0, sizeof(fenceInfo));
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			result = Vk.CreateFence(Device, &fenceInfo, NULL, &RenderFence);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateFence failed (%d).\n", (int)result);
				return false;
			}
			CreateTimestampQueries();
			return true;
		}

		void CreateTimestampQueries()
		{
			if (!TimestampQueriesSupported || Vk.CreateQueryPool == NULL || Vk.DestroyQueryPool == NULL ||
				Vk.CmdResetQueryPool == NULL || Vk.CmdWriteTimestamp == NULL || Vk.GetQueryPoolResults == NULL)
			{
				TimestampQueriesSupported = false;
				return;
			}

			VkQueryPoolCreateInfo queryInfo;
			memset(&queryInfo, 0, sizeof(queryInfo));
			queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
			queryInfo.queryCount = 2;

			VkResult result = Vk.CreateQueryPool(Device, &queryInfo, NULL, &TimestampQueryPool);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_ORANGE "Vulkan: timestamp query pool unavailable (%d).\n", (int)result);
				TimestampQueriesSupported = false;
				TimestampQueryPool = VK_NULL_HANDLE;
			}
		}

		void DestroyTimestampQueries()
		{
			if (TimestampQueryPool != VK_NULL_HANDLE && Vk.DestroyQueryPool != NULL)
			{
				Vk.DestroyQueryPool(Device, TimestampQueryPool, NULL);
			}
			TimestampQueryPool = VK_NULL_HANDLE;
			TimestampQueryPending = false;
		}

		unsigned int FindMemoryType(unsigned int typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties;
			Vk.GetPhysicalDeviceMemoryProperties(PhysicalDevice, &memProperties);
			for (unsigned int i = 0; i < memProperties.memoryTypeCount; ++i)
			{
				if ((typeFilter & (1u << i)) &&
					(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			return ~0u;
		}

		bool CreateSampledImage(unsigned int width, unsigned int height, VkFormat format, VkImage &image, VkDeviceMemory &memory)
		{
			VkImageCreateInfo imageInfo;
			memset(&imageInfo, 0, sizeof(imageInfo));
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = format;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VkResult result = Vk.CreateImage(Device, &imageInfo, NULL, &image);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateImage failed (%d).\n", (int)result);
				return false;
			}

			VkMemoryRequirements requirements;
			Vk.GetImageMemoryRequirements(Device, image, &requirements);
			unsigned int memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (memoryType == ~0u)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no device-local image memory type found.\n");
				return false;
			}

			VkMemoryAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = requirements.size;
			allocInfo.memoryTypeIndex = memoryType;
			result = Vk.AllocateMemory(Device, &allocInfo, NULL, &memory);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkAllocateMemory image failed (%d).\n", (int)result);
				return false;
			}
			result = Vk.BindImageMemory(Device, image, memory, 0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkBindImageMemory failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		bool CreateDepthResources()
		{
			if (SwapchainExtent.width == 0 || SwapchainExtent.height == 0)
			{
				return false;
			}

			DestroyDepthResources();
			VkImageCreateInfo imageInfo;
			memset(&imageInfo, 0, sizeof(imageInfo));
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = VK_FORMAT_D32_SFLOAT;
			imageInfo.extent.width = SwapchainExtent.width;
			imageInfo.extent.height = SwapchainExtent.height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VkResult result = Vk.CreateImage(Device, &imageInfo, NULL, &DepthImage);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: depth vkCreateImage failed (%d).\n", (int)result);
				return false;
			}

			VkMemoryRequirements requirements;
			Vk.GetImageMemoryRequirements(Device, DepthImage, &requirements);
			unsigned int memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (memoryType == ~0u)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no device-local depth memory type found.\n");
				DestroyDepthResources();
				return false;
			}

			VkMemoryAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = requirements.size;
			allocInfo.memoryTypeIndex = memoryType;
			result = Vk.AllocateMemory(Device, &allocInfo, NULL, &DepthImageMemory);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: depth vkAllocateMemory failed (%d).\n", (int)result);
				DestroyDepthResources();
				return false;
			}
			result = Vk.BindImageMemory(Device, DepthImage, DepthImageMemory, 0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: depth vkBindImageMemory failed (%d).\n", (int)result);
				DestroyDepthResources();
				return false;
			}

			VkImageViewCreateInfo viewInfo;
			memset(&viewInfo, 0, sizeof(viewInfo));
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = DepthImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = VK_FORMAT_D32_SFLOAT;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			result = Vk.CreateImageView(Device, &viewInfo, NULL, &DepthImageView);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: depth vkCreateImageView failed (%d).\n", (int)result);
				DestroyDepthResources();
				return false;
			}
			return true;
		}

		void DestroyDepthResources()
		{
			if (DepthImageView != VK_NULL_HANDLE && Vk.DestroyImageView != NULL)
			{
				Vk.DestroyImageView(Device, DepthImageView, NULL);
			}
			DepthImageView = VK_NULL_HANDLE;
			if (DepthImage != VK_NULL_HANDLE && Vk.DestroyImage != NULL)
			{
				Vk.DestroyImage(Device, DepthImage, NULL);
			}
			DepthImage = VK_NULL_HANDLE;
			if (DepthImageMemory != VK_NULL_HANDLE && Vk.FreeMemory != NULL)
			{
				Vk.FreeMemory(Device, DepthImageMemory, NULL);
			}
			DepthImageMemory = VK_NULL_HANDLE;
		}

		bool CreateSampledImageView(VkImage image, VkFormat format, VkImageView &view)
		{
			VkImageViewCreateInfo viewInfo;
			memset(&viewInfo, 0, sizeof(viewInfo));
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			VkResult result = Vk.CreateImageView(Device, &viewInfo, NULL, &view);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateImageView sampled failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		void DestroySourceImages()
		{
			if (SourceImageView != VK_NULL_HANDLE && Vk.DestroyImageView != NULL)
			{
				Vk.DestroyImageView(Device, SourceImageView, NULL);
			}
			SourceImageView = VK_NULL_HANDLE;
			if (SourceImage != VK_NULL_HANDLE && Vk.DestroyImage != NULL)
			{
				Vk.DestroyImage(Device, SourceImage, NULL);
			}
			SourceImage = VK_NULL_HANDLE;
			if (SourceImageMemory != VK_NULL_HANDLE && Vk.FreeMemory != NULL)
			{
				Vk.FreeMemory(Device, SourceImageMemory, NULL);
			}
			SourceImageMemory = VK_NULL_HANDLE;
			if (PaletteImageView != VK_NULL_HANDLE && Vk.DestroyImageView != NULL)
			{
				Vk.DestroyImageView(Device, PaletteImageView, NULL);
			}
			PaletteImageView = VK_NULL_HANDLE;
			if (PaletteImage != VK_NULL_HANDLE && Vk.DestroyImage != NULL)
			{
				Vk.DestroyImage(Device, PaletteImage, NULL);
			}
			PaletteImage = VK_NULL_HANDLE;
			if (PaletteImageMemory != VK_NULL_HANDLE && Vk.FreeMemory != NULL)
			{
				Vk.FreeMemory(Device, PaletteImageMemory, NULL);
			}
			PaletteImageMemory = VK_NULL_HANDLE;
			SourceImageWidth = 0;
			SourceImageHeight = 0;
			GpuPresentationReady = false;
		}

		void UpdateDescriptorSet()
		{
			VkDescriptorImageInfo imageInfos[2];
			memset(imageInfos, 0, sizeof(imageInfos));
			imageInfos[0].sampler = NearestSampler;
			imageInfos[0].imageView = SourceImageView;
			imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[1].sampler = NearestSampler;
			imageInfos[1].imageView = PaletteImageView;
			imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkWriteDescriptorSet writes[2];
			memset(writes, 0, sizeof(writes));
			for (unsigned int i = 0; i < 2; ++i)
			{
				writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[i].dstSet = DescriptorSet;
				writes[i].dstBinding = i;
				writes[i].descriptorCount = 1;
				writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writes[i].pImageInfo = &imageInfos[i];
			}
			Vk.UpdateDescriptorSets(Device, 2, writes, 0, NULL);
		}

		bool EnsureGpuPresentationResources(int width, int height)
		{
			if (width <= 0 || height <= 0)
			{
				return false;
			}
			if (PresentFilterMode != -1 && PresentFilterMode != ClampPresentFilterMode())
			{
				DestroyPresentationResources();
			}
			if (RenderPass == VK_NULL_HANDLE && !CreateRenderPass())
			{
				return false;
			}
			if (GraphicsPipeline == VK_NULL_HANDLE && !CreateGraphicsPipeline())
			{
				return false;
			}
			if (vk_scene_probe && ProbePipeline == VK_NULL_HANDLE && !CreateSceneProbePipeline())
			{
				return false;
			}
			if ((vk_draw_world || vk_world_probe) && WorldProbePipeline == VK_NULL_HANDLE && !CreateWorldProbePipeline())
			{
				return false;
			}
			if (WantsProbeDraw() && !EnsureProbeVertexBuffer())
			{
				return false;
			}
			if (SwapchainFramebuffers[0] == VK_NULL_HANDLE && !CreateSwapchainFramebuffers())
			{
				return false;
			}
			if (SourceImage != VK_NULL_HANDLE && SourceImageWidth == (unsigned int)width && SourceImageHeight == (unsigned int)height)
			{
				GpuPresentationReady = true;
				return true;
			}

			DestroySourceImages();
			if (!CreateSampledImage((unsigned int)width, (unsigned int)height, VK_FORMAT_R8_UNORM, SourceImage, SourceImageMemory) ||
				!CreateSampledImage(256, 1, VK_FORMAT_R8G8B8A8_UNORM, PaletteImage, PaletteImageMemory) ||
				!CreateSampledImageView(SourceImage, VK_FORMAT_R8_UNORM, SourceImageView) ||
				!CreateSampledImageView(PaletteImage, VK_FORMAT_R8G8B8A8_UNORM, PaletteImageView))
			{
				DestroySourceImages();
				return false;
			}
			SourceImageWidth = (unsigned int)width;
			SourceImageHeight = (unsigned int)height;
			UpdateDescriptorSet();
			GpuPresentationReady = true;
			return true;
		}

		bool EnsureUploadBuffer(VkDeviceSize needed)
		{
			if (UploadBuffer != VK_NULL_HANDLE && UploadSize >= needed)
			{
				return true;
			}

			if (UploadMemory != VK_NULL_HANDLE && UploadPtr != NULL)
			{
				Vk.UnmapMemory(Device, UploadMemory);
			}
			UploadPtr = NULL;
			if (UploadBuffer != VK_NULL_HANDLE)
			{
				Vk.DestroyBuffer(Device, UploadBuffer, NULL);
			}
			UploadBuffer = VK_NULL_HANDLE;
			if (UploadMemory != VK_NULL_HANDLE)
			{
				Vk.FreeMemory(Device, UploadMemory, NULL);
			}
			UploadMemory = VK_NULL_HANDLE;
			UploadSize = 0;

			VkBufferCreateInfo bufferInfo;
			memset(&bufferInfo, 0, sizeof(bufferInfo));
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = needed;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkResult result = Vk.CreateBuffer(Device, &bufferInfo, NULL, &UploadBuffer);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkCreateBuffer failed (%d).\n", (int)result);
				return false;
			}

			VkMemoryRequirements requirements;
			Vk.GetBufferMemoryRequirements(Device, UploadBuffer, &requirements);
			unsigned int memoryType = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (memoryType == ~0u)
			{
				Printf(TEXTCOLOR_RED "Vulkan: no host-visible upload memory type found.\n");
				return false;
			}

			VkMemoryAllocateInfo allocInfo;
			memset(&allocInfo, 0, sizeof(allocInfo));
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = requirements.size;
			allocInfo.memoryTypeIndex = memoryType;

			result = Vk.AllocateMemory(Device, &allocInfo, NULL, &UploadMemory);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkAllocateMemory failed (%d).\n", (int)result);
				return false;
			}
			result = Vk.BindBufferMemory(Device, UploadBuffer, UploadMemory, 0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkBindBufferMemory failed (%d).\n", (int)result);
				return false;
			}
			result = Vk.MapMemory(Device, UploadMemory, 0, needed, 0, &UploadPtr);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkMapMemory failed (%d).\n", (int)result);
				return false;
			}
			UploadSize = needed;
			PublishVulkanStats(this);
			return true;
		}

		bool RecordClearCommands(unsigned int imageIndex)
		{
			VkCommandBufferBeginInfo beginInfo;
			memset(&beginInfo, 0, sizeof(beginInfo));
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VkResult result = Vk.BeginCommandBuffer(CommandBuffer, &beginInfo);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkBeginCommandBuffer failed (%d).\n", (int)result);
				return false;
			}

			VkImageSubresourceRange colorRange;
			memset(&colorRange, 0, sizeof(colorRange));
			colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorRange.baseMipLevel = 0;
			colorRange.levelCount = 1;
			colorRange.baseArrayLayer = 0;
			colorRange.layerCount = 1;

			VkImageMemoryBarrier toClear;
			memset(&toClear, 0, sizeof(toClear));
			toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toClear.image = SwapchainImages[imageIndex];
			toClear.subresourceRange = colorRange;
			toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			Vk.CmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, NULL,
				0, NULL,
				1, &toClear);

			VkClearColorValue clearColor;
			clearColor.float32[0] = 0.02f;
			clearColor.float32[1] = 0.02f;
			clearColor.float32[2] = 0.05f;
			clearColor.float32[3] = 1.f;
			Vk.CmdClearColorImage(CommandBuffer, SwapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &colorRange);

			VkImageMemoryBarrier toPresent;
			memset(&toPresent, 0, sizeof(toPresent));
			toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toPresent.image = SwapchainImages[imageIndex];
			toPresent.subresourceRange = colorRange;
			toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			Vk.CmdPipelineBarrier(CommandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0,
				0, NULL,
				0, NULL,
				1, &toPresent);

			result = Vk.EndCommandBuffer(CommandBuffer);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkEndCommandBuffer failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		bool RecordUploadCommands(unsigned int imageIndex, int width, int height)
		{
			VkCommandBufferBeginInfo beginInfo;
			memset(&beginInfo, 0, sizeof(beginInfo));
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VkResult result = Vk.BeginCommandBuffer(CommandBuffer, &beginInfo);
			if (result != VK_SUCCESS)
			{
				return false;
			}
			WriteTimestampStart();

			VkImageSubresourceRange colorRange;
			memset(&colorRange, 0, sizeof(colorRange));
			colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorRange.baseMipLevel = 0;
			colorRange.levelCount = 1;
			colorRange.baseArrayLayer = 0;
			colorRange.layerCount = 1;

			VkImageMemoryBarrier toTransfer;
			memset(&toTransfer, 0, sizeof(toTransfer));
			toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toTransfer.image = SwapchainImages[imageIndex];
			toTransfer.subresourceRange = colorRange;
			toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			Vk.CmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toTransfer);

			VkBufferImageCopy copyRegion;
			memset(&copyRegion, 0, sizeof(copyRegion));
			copyRegion.bufferRowLength = (unsigned int)width;
			copyRegion.bufferImageHeight = (unsigned int)height;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent.width = (unsigned int)MIN<int>(width, (int)SwapchainExtent.width);
			copyRegion.imageExtent.height = (unsigned int)MIN<int>(height, (int)SwapchainExtent.height);
			copyRegion.imageExtent.depth = 1;
			Vk.CmdCopyBufferToImage(CommandBuffer, UploadBuffer, SwapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			VkImageMemoryBarrier toPresent;
			memset(&toPresent, 0, sizeof(toPresent));
			toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			toPresent.image = SwapchainImages[imageIndex];
			toPresent.subresourceRange = colorRange;
			toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			Vk.CmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &toPresent);

			WriteTimestampEnd();
			return Vk.EndCommandBuffer(CommandBuffer) == VK_SUCCESS;
		}

		bool RecordGpuPresentationCommands(unsigned int imageIndex, int width, int height)
		{
			VkCommandBufferBeginInfo beginInfo;
			memset(&beginInfo, 0, sizeof(beginInfo));
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VkResult result = Vk.BeginCommandBuffer(CommandBuffer, &beginInfo);
			if (result != VK_SUCCESS)
			{
				return false;
			}
			WriteTimestampStart();

			VkImageSubresourceRange colorRange;
			memset(&colorRange, 0, sizeof(colorRange));
			colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorRange.baseMipLevel = 0;
			colorRange.levelCount = 1;
			colorRange.baseArrayLayer = 0;
			colorRange.layerCount = 1;

			VkImageMemoryBarrier imageBarriers[2];
			memset(imageBarriers, 0, sizeof(imageBarriers));
			imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].image = SourceImage;
			imageBarriers[0].subresourceRange = colorRange;
			imageBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarriers[1] = imageBarriers[0];
			imageBarriers[1].image = PaletteImage;
			Vk.CmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, imageBarriers);

			VkBufferImageCopy copyRegions[2];
			memset(copyRegions, 0, sizeof(copyRegions));
			copyRegions[0].bufferOffset = 0;
			copyRegions[0].bufferRowLength = (unsigned int)width;
			copyRegions[0].bufferImageHeight = (unsigned int)height;
			copyRegions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegions[0].imageSubresource.layerCount = 1;
			copyRegions[0].imageExtent.width = (unsigned int)width;
			copyRegions[0].imageExtent.height = (unsigned int)height;
			copyRegions[0].imageExtent.depth = 1;
			copyRegions[1].bufferOffset = (VkDeviceSize)width * (VkDeviceSize)height;
			copyRegions[1].bufferRowLength = 256;
			copyRegions[1].bufferImageHeight = 1;
			copyRegions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegions[1].imageSubresource.layerCount = 1;
			copyRegions[1].imageExtent.width = 256;
			copyRegions[1].imageExtent.height = 1;
			copyRegions[1].imageExtent.depth = 1;
			Vk.CmdCopyBufferToImage(CommandBuffer, UploadBuffer, SourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegions[0]);
			Vk.CmdCopyBufferToImage(CommandBuffer, UploadBuffer, PaletteImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegions[1]);

			imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarriers[1] = imageBarriers[0];
			imageBarriers[1].image = PaletteImage;
			Vk.CmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, imageBarriers);

			VkRenderPassBeginInfo renderPassInfo;
			memset(&renderPassInfo, 0, sizeof(renderPassInfo));
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = RenderPass;
			renderPassInfo.framebuffer = SwapchainFramebuffers[imageIndex];
			renderPassInfo.renderArea.extent = SwapchainExtent;
			VkClearValue clearValues[2];
			memset(clearValues, 0, sizeof(clearValues));
			clearValues[1].depthStencil.depth = 1.0f;
			clearValues[1].depthStencil.stencil = 0;
			renderPassInfo.clearValueCount = 2;
			renderPassInfo.pClearValues = clearValues;
			Vk.CmdBeginRenderPass(CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport;
			memset(&viewport, 0, sizeof(viewport));
			viewport.width = (float)SwapchainExtent.width;
			viewport.height = (float)SwapchainExtent.height;
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;
			VkRect2D scissor;
			memset(&scissor, 0, sizeof(scissor));
			scissor.extent = SwapchainExtent;
			Vk.CmdSetViewport(CommandBuffer, 0, 1, &viewport);
			Vk.CmdSetScissor(CommandBuffer, 0, 1, &scissor);
			Vk.CmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);
			Vk.CmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet, 0, NULL);
			PresentPushConstants constants = BuildPresentPushConstants(width, height);
			Vk.CmdPushConstants(CommandBuffer, PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
			Vk.CmdDraw(CommandBuffer, 3, 1, 0, 0);
			if (WantsProbeDraw())
			{
				UpdateProbeVertices();
			}
			if (ProbeVertexBuffer != VK_NULL_HANDLE && ProbeVertexDrawCount > 0)
			{
				VkDeviceSize vertexOffsets[1] = { 0 };
				SceneProbePushConstants probeConstants = BuildSceneProbePushConstants();
				if (vk_draw_world && WorldProbePipeline != VK_NULL_HANDLE && WorldDrawDrawCount > 0)
				{
					Vk.CmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, WorldProbePipeline);
					Vk.CmdPushConstants(CommandBuffer, WorldProbePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeConstants), &probeConstants);
					Vk.CmdBindVertexBuffers(CommandBuffer, 0, 1, &ProbeVertexBuffer, vertexOffsets);
					Vk.CmdDraw(CommandBuffer, WorldDrawDrawCount, 1, WorldDrawFirstVertex, 0);
				}
				if (vk_world_probe && WorldProbePipeline != VK_NULL_HANDLE && WorldProbeDrawCount > 0)
				{
					Vk.CmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, WorldProbePipeline);
					Vk.CmdPushConstants(CommandBuffer, WorldProbePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeConstants), &probeConstants);
					Vk.CmdBindVertexBuffers(CommandBuffer, 0, 1, &ProbeVertexBuffer, vertexOffsets);
					Vk.CmdDraw(CommandBuffer, WorldProbeDrawCount, 1, WorldProbeFirstVertex, 0);
				}
				if (vk_scene_probe && ProbePipeline != VK_NULL_HANDLE && SceneProbeDrawCount > 0)
				{
					Vk.CmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ProbePipeline);
					Vk.CmdPushConstants(CommandBuffer, ProbePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(probeConstants), &probeConstants);
					Vk.CmdBindVertexBuffers(CommandBuffer, 0, 1, &ProbeVertexBuffer, vertexOffsets);
					Vk.CmdDraw(CommandBuffer, SceneProbeDrawCount, 1, SceneProbeFirstVertex, 0);
				}
			}
			Vk.CmdEndRenderPass(CommandBuffer);

			WriteTimestampEnd();
			return Vk.EndCommandBuffer(CommandBuffer) == VK_SUCCESS;
		}

		SceneProbePushConstants BuildSceneProbePushConstants()
		{
			SceneProbePushConstants constants;
			memset(&constants, 0, sizeof(constants));

			const double pi = 3.14159265358979323846;
			double fovDegrees = (double)R_GetFOV();
			if (fovDegrees < 5.0)
			{
				fovDegrees = 5.0;
			}
			else if (fovDegrees > 170.0)
			{
				fovDegrees = 170.0;
			}
			double aspect = 4.0 / 3.0;
			if (PresentViewportWidth > 0 && PresentViewportHeight > 0)
			{
				aspect = (double)PresentViewportWidth / (double)PresentViewportHeight;
			}
			else if (SwapchainExtent.width > 0 && SwapchainExtent.height > 0)
			{
				aspect = (double)SwapchainExtent.width / (double)SwapchainExtent.height;
			}
			const double tanX = tan(fovDegrees * (pi / 360.0));
			const double tanY = tanX / aspect;
			const double invTanX = tanX > 0.0001 ? 1.0 / tanX : 1.0;
			const double invTanY = tanY > 0.0001 ? 1.0 / tanY : 1.0;
			const double yawRadians = (180.0 + (double)vk_clip_yaw_sign * ANGLE2DBL(viewangle)) * (pi / 180.0);
			const double pitchRadians = ((double)viewpitch * (90.0 / (double)ANGLE_90)) * (pi / 180.0);
			const double yawCos = cos(yawRadians);
			const double yawSin = sin(yawRadians);
			const double pitchCos = cos(pitchRadians);
			const double pitchSin = sin(pitchRadians);
			const double camX = FIXED2FLOAT(viewx);
			const double camY = FIXED2FLOAT(viewy);
			const double camZ = FIXED2FLOAT(viewz);

			const double forwardRow[4] =
			{
				yawCos * pitchCos,
				yawSin * pitchCos,
				pitchSin,
				-((yawCos * camX + yawSin * camY) * pitchCos + camZ * pitchSin)
			};
			const double unpitchedForwardRow[4] =
			{
				yawCos,
				yawSin,
				0.0,
				-(yawCos * camX + yawSin * camY)
			};
			const double upRow[4] =
			{
				0.0,
				0.0,
				1.0,
				-camZ
			};

			const double sideSign = (double)vk_clip_side_sign;
			constants.Row0[0] = (float)(sideSign * yawSin * invTanX);
			constants.Row0[1] = (float)(sideSign * -yawCos * invTanX);
			constants.Row0[2] = 0.0f;
			constants.Row0[3] = (float)(sideSign * (-yawSin * camX + yawCos * camY) * invTanX);

			const double nearDepth = 4.0;
			for (unsigned int i = 0; i < 4; ++i)
			{
				constants.Row1[i] = (float)((unpitchedForwardRow[i] * pitchSin - upRow[i] * pitchCos) * invTanY);
				constants.Row2[i] = (float)forwardRow[i];
				constants.Row3[i] = (float)forwardRow[i];
			}
			constants.Row2[3] -= (float)nearDepth;
			return constants;
		}

		PresentPushConstants BuildPresentPushConstants(int sourceWidth, int sourceHeight)
		{
			PresentPushConstants constants;
			memset(&constants, 0, sizeof(constants));
			constants.UvScale[0] = 1.f;
			constants.UvScale[1] = 1.f;
			constants.SourceScale[0] = 1.f;
			constants.SourceScale[1] = 1.f;
			constants.BorderColor[0] = 0.02f;
			constants.BorderColor[1] = 0.02f;
			constants.BorderColor[2] = 0.05f;
			constants.BorderColor[3] = 1.f;
			constants.FilterParams[0] = (float)ClampPresentFilterMode();
			constants.FilterParams[1] = (float)(sourceWidth > 0 ? sourceWidth : 1);
			constants.FilterParams[2] = (float)(sourceHeight > 0 ? sourceHeight : 1);
			constants.FilterParams[3] = ClampPresentSharpness();

			PresentScaleMode = (unsigned int)(vk_present_scale_mode < 0 ? 0 : (vk_present_scale_mode > 2 ? 2 : vk_present_scale_mode));
			PresentViewportX = 0;
			PresentViewportY = 0;
			PresentViewportWidth = SwapchainExtent.width;
			PresentViewportHeight = SwapchainExtent.height;

			if (PresentScaleMode == 0 || sourceWidth <= 0 || sourceHeight <= 0 || SwapchainExtent.width == 0 || SwapchainExtent.height == 0)
			{
				return constants;
			}

			const double sourceAspect = (double)sourceWidth / (double)sourceHeight;
			const double targetAspect = vk_present_force_aspect && vk_present_aspect > 0.0f ? (double)vk_present_aspect : sourceAspect;

			unsigned int targetWidth = SwapchainExtent.width;
			unsigned int targetHeight = (unsigned int)((double)targetWidth / targetAspect + 0.5);
			if (targetHeight > SwapchainExtent.height)
			{
				targetHeight = SwapchainExtent.height;
				targetWidth = (unsigned int)((double)targetHeight * targetAspect + 0.5);
			}

			unsigned int displayWidth = targetWidth;
			unsigned int displayHeight = targetHeight;
			if (PresentScaleMode == 2)
			{
				const unsigned int virtualWidth = 320;
				unsigned int virtualHeight = (unsigned int)((double)virtualWidth / targetAspect + 0.5);
				if (virtualHeight == 0)
				{
					virtualHeight = 1;
				}
				unsigned int scaleX = SwapchainExtent.width / virtualWidth;
				unsigned int scaleY = SwapchainExtent.height / virtualHeight;
				unsigned int integerScale = scaleX < scaleY ? scaleX : scaleY;
				if (integerScale == 0)
				{
					integerScale = 1;
				}
				displayWidth = virtualWidth * integerScale;
				displayHeight = virtualHeight * integerScale;
			}

			if (displayWidth > SwapchainExtent.width)
			{
				displayWidth = SwapchainExtent.width;
			}
			if (displayHeight > SwapchainExtent.height)
			{
				displayHeight = SwapchainExtent.height;
			}
			if (displayWidth == 0 || displayHeight == 0)
			{
				return constants;
			}

			PresentViewportWidth = displayWidth;
			PresentViewportHeight = displayHeight;
			PresentViewportX = (SwapchainExtent.width - displayWidth) / 2;
			PresentViewportY = (SwapchainExtent.height - displayHeight) / 2;

			constants.UvOffset[0] = (float)((double)PresentViewportX / (double)SwapchainExtent.width);
			constants.UvOffset[1] = (float)((double)PresentViewportY / (double)SwapchainExtent.height);
			constants.UvScale[0] = (float)((double)SwapchainExtent.width / (double)displayWidth);
			constants.UvScale[1] = (float)((double)SwapchainExtent.height / (double)displayHeight);
			return constants;
		}

		void WriteTimestampStart()
		{
			if (!AreTimestampQueriesAvailable())
			{
				return;
			}
			Vk.CmdResetQueryPool(CommandBuffer, TimestampQueryPool, 0, 2);
			Vk.CmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, 0);
		}

		void WriteTimestampEnd()
		{
			if (!AreTimestampQueriesAvailable())
			{
				return;
			}
			Vk.CmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, 1);
		}

		void ReadGpuFrameTime()
		{
			if (!TimestampQueryPending || !AreTimestampQueriesAvailable())
			{
				return;
			}

			uint64_t timestamps[2] = { 0, 0 };
			VkResult result = Vk.GetQueryPoolResults(Device, TimestampQueryPool, 0, 2, sizeof(timestamps), timestamps,
				sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			TimestampQueryPending = false;
			if (result == VK_SUCCESS && timestamps[1] >= timestamps[0])
			{
				LastGpuFrameMS = ((double)(timestamps[1] - timestamps[0]) * TimestampPeriod) / 1000000.0;
			}
		}

		void RefreshMemoryBudgetStats()
		{
			DeviceLocalMemoryBudgetBytes = 0;
			DeviceLocalMemoryUsageBytes = 0;
			if (!MemoryBudgetSupported || Vk.GetPhysicalDeviceMemoryProperties2 == NULL || PhysicalDevice == NULL)
			{
				return;
			}

			VkPhysicalDeviceMemoryBudgetPropertiesEXT budget;
			memset(&budget, 0, sizeof(budget));
			budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

			VkPhysicalDeviceMemoryProperties2 memory;
			memset(&memory, 0, sizeof(memory));
			memory.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
			memory.pNext = &budget;

			Vk.GetPhysicalDeviceMemoryProperties2(PhysicalDevice, &memory);
			for (unsigned int i = 0; i < memory.memoryProperties.memoryHeapCount; ++i)
			{
				if (memory.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
				{
					DeviceLocalMemoryBudgetBytes += (unsigned long long)budget.heapBudget[i];
					DeviceLocalMemoryUsageBytes += (unsigned long long)budget.heapUsage[i];
				}
			}
		}

		bool PresentStartupFrame()
		{
			unsigned int imageIndex = 0;
			VkResult result = Vk.AcquireNextImageKHR(Device, Swapchain, ~(uint64_t)0, ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkAcquireNextImageKHR failed (%d).\n", (int)result);
				return false;
			}
			if (imageIndex >= SwapchainImageCount)
			{
				Printf(TEXTCOLOR_RED "Vulkan: acquired invalid swapchain image %u.\n", imageIndex);
				return false;
			}
			if (!RecordClearCommands(imageIndex))
			{
				return false;
			}

			VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo submitInfo;
			memset(&submitInfo, 0, sizeof(submitInfo));
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
			submitInfo.pWaitDstStageMask = &waitStage;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &CommandBuffer;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &RenderFinishedSemaphore;

			result = Vk.QueueSubmit(GraphicsQueue, 1, &submitInfo, RenderFence);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkQueueSubmit failed (%d).\n", (int)result);
				return false;
			}

			result = Vk.WaitForFences(Device, 1, &RenderFence, VK_TRUE, ~(uint64_t)0);
			if (result != VK_SUCCESS)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkWaitForFences failed (%d).\n", (int)result);
				return false;
			}

			VkPresentInfoKHR presentInfo;
			memset(&presentInfo, 0, sizeof(presentInfo));
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &RenderFinishedSemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &Swapchain;
			presentInfo.pImageIndices = &imageIndex;

			result = Vk.QueuePresentKHR(GraphicsQueue, &presentInfo);
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			{
				Printf(TEXTCOLOR_RED "Vulkan: vkQueuePresentKHR failed (%d).\n", (int)result);
				return false;
			}
			return true;
		}

		VulkanLoaderModule Module;
		VulkanFunctions Vk;
		VkInstance Instance;
		VkSurfaceKHR Surface;
		VkPhysicalDevice PhysicalDevice;
		VkDevice Device;
		VkQueue GraphicsQueue;
		VkSwapchainKHR Swapchain;
		VkImage SwapchainImages[MaxSwapchainImages];
		VkImageView SwapchainImageViews[MaxSwapchainImages];
		VkFramebuffer SwapchainFramebuffers[MaxSwapchainImages];
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
		VkSemaphore ImageAvailableSemaphore;
		VkSemaphore RenderFinishedSemaphore;
		VkFence RenderFence;
		VkBuffer UploadBuffer;
		VkDeviceMemory UploadMemory;
		void *UploadPtr;
		VkDeviceSize UploadSize;
		VkBuffer ProbeVertexBuffer;
		VkDeviceMemory ProbeVertexMemory;
		void *ProbeVertexPtr;
		VkDeviceSize ProbeVertexBufferSize;
		unsigned int ProbeVertexDrawCount;
		unsigned int WorldDrawFirstVertex;
		unsigned int WorldDrawDrawCount;
		unsigned int SceneProbeFirstVertex;
		unsigned int SceneProbeDrawCount;
		unsigned int WorldProbeFirstVertex;
		unsigned int WorldProbeDrawCount;
		VkImage SourceImage;
		VkDeviceMemory SourceImageMemory;
		VkImageView SourceImageView;
		VkImage PaletteImage;
		VkDeviceMemory PaletteImageMemory;
		VkImageView PaletteImageView;
		VkImage DepthImage;
		VkDeviceMemory DepthImageMemory;
		VkImageView DepthImageView;
		VkSampler NearestSampler;
		VkDescriptorSetLayout DescriptorSetLayout;
		VkDescriptorPool DescriptorPool;
		VkDescriptorSet DescriptorSet;
		VkPipelineLayout PipelineLayout;
		VkRenderPass RenderPass;
		VkPipeline GraphicsPipeline;
		VkPipelineLayout ProbePipelineLayout;
		VkPipeline ProbePipeline;
		VkPipelineLayout WorldProbePipelineLayout;
		VkPipeline WorldProbePipeline;
		unsigned int SourceImageWidth;
		unsigned int SourceImageHeight;
		bool GpuPresentationReady;
		int PresentFilterMode;
		VkQueryPool TimestampQueryPool;
		bool TimestampQueriesSupported;
		bool TimestampQueryPending;
		double TimestampPeriod;
		double LastGpuFrameMS;
		bool MemoryBudgetSupported;
		unsigned long long DeviceLocalMemoryBudgetBytes;
		unsigned long long DeviceLocalMemoryUsageBytes;
		unsigned int PresentScaleMode;
		unsigned int PresentViewportX;
		unsigned int PresentViewportY;
		unsigned int PresentViewportWidth;
		unsigned int PresentViewportHeight;
		unsigned int GraphicsQueueFamily;
		unsigned int DeviceCount;
		unsigned int SwapchainImageCount;
		unsigned int SwapchainViewCount;
		VkFormat SwapchainFormat;
		VkColorSpaceKHR SwapchainColorSpace;
		VkExtent2D SwapchainExtent;
		VkPhysicalDeviceProperties DeviceProperties;
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		unsigned int SwapchainRecreateCount;
		unsigned int OutOfDateCount;
		bool WindowMinimized;
		bool Ready;
	};

	static void ResetVulkanStats()
	{
		memset(&VulkanStats, 0, sizeof(VulkanStats));
		VulkanStats.GraphicsQueueFamily = ~0u;
		VulkanStats.LastPresentSucceeded = false;
	}

	static void PublishVulkanStats(const VulkanRuntime *runtime)
	{
		unsigned int lastPresentMS = VulkanStats.LastPresentMS;
		bool lastPresentSucceeded = VulkanStats.LastPresentSucceeded;

		ResetVulkanStats();
		VulkanStats.LastPresentMS = lastPresentMS;
		VulkanStats.LastPresentSucceeded = lastPresentSucceeded;
		if (runtime == NULL)
		{
			return;
		}

		VulkanStats.Available = true;
		VulkanStats.Ready = runtime->IsReady();
		VulkanStats.DeviceCount = runtime->GetDeviceCount();
		VulkanStats.GraphicsQueueFamily = runtime->GetGraphicsQueueFamily();
		VulkanStats.SwapchainImageCount = runtime->GetSwapchainImageCount();
		VulkanStats.SwapchainFormat = (unsigned int)runtime->GetSwapchainFormat();
		VulkanStats.SwapchainColorSpace = (unsigned int)runtime->GetSwapchainColorSpace();
		VkExtent2D extent = runtime->GetSwapchainExtent();
		VulkanStats.SwapchainWidth = extent.width;
		VulkanStats.SwapchainHeight = extent.height;
		VulkanStats.UploadBufferBytes = (unsigned long long)runtime->GetUploadSize();
		VulkanStats.SwapchainRecreateCount = runtime->GetSwapchainRecreateCount();
		VulkanStats.OutOfDateCount = runtime->GetOutOfDateCount();
		VulkanStats.GpuPresentationActive = runtime->IsGpuPresentationReady();
		VulkanStats.WindowMinimized = runtime->IsWindowMinimized();
		VulkanStats.TimestampQueriesAvailable = runtime->AreTimestampQueriesAvailable();
		VulkanStats.LastGpuFrameMS = runtime->GetLastGpuFrameMS();
		VulkanStats.MemoryBudgetAvailable = runtime->IsMemoryBudgetAvailable();
		VulkanStats.DeviceLocalMemoryBudgetBytes = runtime->GetDeviceLocalMemoryBudgetBytes();
		VulkanStats.DeviceLocalMemoryUsageBytes = runtime->GetDeviceLocalMemoryUsageBytes();
		VulkanStats.PresentFilterMode = runtime->GetPresentFilterMode();
		VulkanStats.PresentScaleMode = runtime->GetPresentScaleMode();
		VulkanStats.PresentViewportX = runtime->GetPresentViewportX();
		VulkanStats.PresentViewportY = runtime->GetPresentViewportY();
		VulkanStats.PresentViewportWidth = runtime->GetPresentViewportWidth();
		VulkanStats.PresentViewportHeight = runtime->GetPresentViewportHeight();
		VulkanStats.PresentSourceWidth = runtime->GetPresentSourceWidth();
		VulkanStats.PresentSourceHeight = runtime->GetPresentSourceHeight();
		VulkanStats.PresentSharpness = runtime->GetPresentSharpness();
		VulkanStats.PresentAspect = runtime->GetPresentAspect();
		VulkanStats.WorldDrawActive = runtime->IsWorldDrawActive();
		VulkanStats.WorldDrawVertexCount = runtime->GetWorldDrawVertexCount();
		VulkanStats.SceneProbeActive = runtime->IsSceneProbeActive();
		VulkanStats.SceneProbeVertexCount = runtime->GetSceneProbeVertexCount();
		VulkanStats.WorldProbeActive = runtime->IsWorldProbeActive();
		VulkanStats.WorldProbeVertexCount = runtime->GetWorldProbeVertexCount();

		const VkPhysicalDeviceProperties &properties = runtime->GetDeviceProperties();
		CopyVulkanString(VulkanStats.DeviceName, sizeof(VulkanStats.DeviceName), properties.deviceName);
		VulkanStats.ApiVersion = properties.apiVersion;
		VulkanStats.DriverVersion = properties.driverVersion;
		VulkanStats.VendorID = properties.vendorID;
		VulkanStats.DeviceID = properties.deviceID;

		const VkPhysicalDeviceMemoryProperties &memory = runtime->GetMemoryProperties();
		for (unsigned int i = 0; i < memory.memoryHeapCount; ++i)
		{
			if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				VulkanStats.DeviceLocalMemoryBytes += (unsigned long long)memory.memoryHeaps[i].size;
			}
		}
	}

	static const char *CopyVulkanString(char *to, unsigned int toSize, const char *from)
	{
		if (toSize == 0)
		{
			return to;
		}
		strncpy(to, from != NULL ? from : "", toSize - 1);
		to[toSize - 1] = 0;
		return to;
	}
}

#ifdef _WIN32

static void ResizeVulkanWindow(int clientWidth, int clientHeight, bool fullscreen)
{
	if (Window == NULL || clientWidth <= 0 || clientHeight <= 0)
	{
		return;
	}

	if (fullscreen)
	{
		I_SaveWindowedPos();

		HMONITOR monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo;
		memset(&monitorInfo, 0, sizeof(monitorInfo));
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (!GetMonitorInfo(monitor, &monitorInfo))
		{
			return;
		}

		SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowLong(Window, GWL_EXSTYLE, 0);
		SetWindowPos(Window, HWND_TOP,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		ShowWindow(Window, SW_SHOW);
		UpdateWindow(Window);
		return;
	}

	SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);

	RECT windowRect;
	GetWindowRect(Window, &windowRect);

	RECT clientRect;
	clientRect.left = 0;
	clientRect.top = 0;
	clientRect.right = clientWidth;
	clientRect.bottom = clientHeight;
	AdjustWindowRectEx(&clientRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_WINDOWEDGE);

	int windowWidth = clientRect.right - clientRect.left;
	int windowHeight = clientRect.bottom - clientRect.top;
	SetWindowPos(Window, NULL, windowRect.left, windowRect.top, windowWidth, windowHeight,
		SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	I_RestoreWindowedPos();
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
}

class VulkanFrameBuffer : public DFrameBuffer
{
	DECLARE_CLASS(VulkanFrameBuffer, DFrameBuffer)
public:
	VulkanFrameBuffer()
		: Runtime(NULL), Fullscreen(false), PresentBuffer(NULL), PresentBufferSize(0),
		  GammaValue(1.f), FlashColor(0), FlashAmount(0)
	{
	}

	VulkanFrameBuffer(int width, int height, bool fullscreen)
		: DFrameBuffer(width, height), Runtime(NULL), Fullscreen(fullscreen),
		  PresentBuffer(NULL), PresentBufferSize(0), GammaValue(1.f), FlashColor(0), FlashAmount(0)
	{
		memcpy(SourcePalette, GPalette.BaseColors, sizeof(SourcePalette));
		ResizeVulkanWindow(width, height, fullscreen);
		Runtime = new VulkanRuntime;
		if (!Runtime->Init())
		{
			delete Runtime;
			Runtime = NULL;
		}
		else
		{
			VkExtent2D extent = Runtime->GetSwapchainExtent();
			Printf("%s Vulkan framebuffer active: %u swapchain images, format %d, %ux%u.\n",
				GAMENAME,
				Runtime->GetSwapchainImageCount(),
				(int)Runtime->GetSwapchainFormat(),
				extent.width,
				extent.height);
		}
	}

	~VulkanFrameBuffer()
	{
		delete Runtime;
		Runtime = NULL;
		delete[] PresentBuffer;
		PresentBuffer = NULL;
		PresentBufferSize = 0;
	}

	bool IsValid()
	{
		return MemBuffer != NULL && Runtime != NULL && Runtime->IsReady();
	}

	bool Lock(bool buffered)
	{
		return DSimpleCanvas::Lock(buffered);
	}

	void Unlock()
	{
		DSimpleCanvas::Unlock();
	}

	void Update()
	{
		if (LockCount != 1)
		{
			if (LockCount > 0)
			{
				--LockCount;
			}
			return;
		}

		DrawRateStuff();

		PalEntry palette[256];
		GetFlashedPalette(palette);
		const BYTE *presentSource = MemBuffer;
		int presentPitch = Pitch;
		int presentWidth = Width;
		int presentHeight = Height;
		BuildPresentSource(presentSource, presentPitch, presentWidth, presentHeight);
		Runtime->PresentPalettedFrame(presentSource, presentPitch, presentWidth, presentHeight, palette);

		LockCount = 0;
		Buffer = NULL;
	}

	PalEntry *GetPalette()
	{
		return SourcePalette;
	}

	void GetFlashedPalette(PalEntry palette[256])
	{
		memcpy(palette, SourcePalette, sizeof(SourcePalette));
		if (FlashAmount != 0)
		{
			DoBlending(palette, palette, 256, FlashColor.r, FlashColor.g, FlashColor.b, FlashAmount);
		}
	}

	void UpdatePalette()
	{
	}

	bool SetGamma(float gamma)
	{
		GammaValue = gamma;
		return true;
	}

	bool SetFlash(PalEntry rgb, int amount)
	{
		FlashColor = rgb;
		FlashAmount = amount;
		return true;
	}

	void GetFlash(PalEntry &rgb, int &amount)
	{
		rgb = FlashColor;
		amount = FlashAmount;
	}

	int GetPageCount()
	{
		return 1;
	}

	bool IsFullscreen()
	{
		return Fullscreen;
	}

	bool SetFullscreenMode(bool fullscreen)
	{
		if (Fullscreen == fullscreen)
		{
			return true;
		}
		ResizeVulkanWindow(Width, Height, fullscreen);
		if (Runtime != NULL && Runtime->RecreateSwapchainForWindow())
		{
			Fullscreen = fullscreen;
			return true;
		}
		return false;
	}

	void PaletteChanged()
	{
	}

	int QueryNewPalette()
	{
		return 0;
	}

	bool Is8BitMode()
	{
		return false;
	}

private:
	void BuildPresentSource(const BYTE *&source, int &pitch, int &width, int &height)
	{
		const float scale = ClampRenderScale();
		if (scale >= 0.999f)
		{
			return;
		}

		const int scaledWidth = MAX(1, (int)((float)Width * scale + 0.5f));
		const int scaledHeight = MAX(1, (int)((float)Height * scale + 0.5f));
		const unsigned int needed = (unsigned int)(scaledWidth * scaledHeight);
		if (PresentBufferSize < needed)
		{
			delete[] PresentBuffer;
			PresentBuffer = new BYTE[needed];
			PresentBufferSize = needed;
		}

		for (int y = 0; y < scaledHeight; ++y)
		{
			const int srcY = (int)(((long long)y * Height) / scaledHeight);
			const BYTE *srcRow = MemBuffer + srcY * Pitch;
			BYTE *dstRow = PresentBuffer + y * scaledWidth;
			for (int x = 0; x < scaledWidth; ++x)
			{
				const int srcX = (int)(((long long)x * Width) / scaledWidth);
				dstRow[x] = srcRow[srcX];
			}
		}

		source = PresentBuffer;
		pitch = scaledWidth;
		width = scaledWidth;
		height = scaledHeight;
	}

	VulkanRuntime *Runtime;
	bool Fullscreen;
	BYTE *PresentBuffer;
	unsigned int PresentBufferSize;
	float GammaValue;
	PalEntry SourcePalette[256];
	PalEntry FlashColor;
	int FlashAmount;
};

IMPLEMENT_CLASS(VulkanFrameBuffer)

class Win32VulkanVideo : public IVideo
{
public:
	Win32VulkanVideo()
		: Modes(NULL), Iterator(NULL)
	{
		I_SetWndProc();
		MakeModesList();
	}

	~Win32VulkanVideo()
	{
		FreeModes();
	}

	EDisplayType GetDisplayType()
	{
		return DISPLAY_Both;
	}

	void SetWindowedScale(float scale)
	{
	}

	DFrameBuffer *CreateFrameBuffer(int width, int height, bool fs, DFrameBuffer *old)
	{
		if (old != NULL)
		{
			if (old->GetWidth() == width && old->GetHeight() == height)
			{
				VulkanFrameBuffer *fb = static_cast<VulkanFrameBuffer *>(old);
				if (fb->SetFullscreenMode(fs))
				{
					return old;
				}
			}
			old->ObjectFlags |= OF_YesReallyDelete;
			if (old == screen)
			{
				screen = NULL;
			}
			delete old;
		}
		return new VulkanFrameBuffer(width, height, fs);
	}

	void StartModeIterator(int bits, bool fs)
	{
		Iterator = Modes;
	}

	bool NextMode(int *width, int *height, bool *letterbox)
	{
		if (Iterator == NULL)
		{
			return false;
		}
		*width = Iterator->Width;
		*height = Iterator->Height;
		if (letterbox != NULL)
		{
			*letterbox = false;
		}
		Iterator = Iterator->Next;
		return true;
	}

private:
	struct ModeInfo
	{
		ModeInfo(int width, int height)
			: Width(width), Height(height), Next(NULL)
		{
		}

		int Width;
		int Height;
		ModeInfo *Next;
	};

	void AddMode(int width, int height)
	{
		for (ModeInfo *mode = Modes; mode != NULL; mode = mode->Next)
		{
			if (mode->Width == width && mode->Height == height)
			{
				return;
			}
		}
		ModeInfo *mode = new ModeInfo(width, height);
		mode->Next = Modes;
		Modes = mode;
	}

	void MakeModesList()
	{
		DEVMODE dm;
		memset(&dm, 0, sizeof(dm));
		dm.dmSize = sizeof(dm);
		for (int i = 0; EnumDisplaySettings(NULL, i, &dm); ++i)
		{
			if (dm.dmBitsPerPel >= 24)
			{
				AddMode((int)dm.dmPelsWidth, (int)dm.dmPelsHeight);
			}
		}
		AddMode(640, 480);
		AddMode(800, 600);
		AddMode(1280, 720);
		AddMode(1600, 900);
	}

	void FreeModes()
	{
		while (Modes != NULL)
		{
			ModeInfo *next = Modes->Next;
			delete Modes;
			Modes = next;
		}
		Iterator = NULL;
	}

	ModeInfo *Modes;
	ModeInfo *Iterator;
};

#endif

class FVulkanRenderer : public FSoftwareRenderer
{
public:
	FVulkanRenderer()
		: Runtime(NULL)
	{
	}

	~FVulkanRenderer()
	{
		delete Runtime;
	}

	void Init()
	{
		Printf("%s Vulkan renderer active; software draw path will present through Vulkan when Vulkan video is active.\n", GAMENAME);
		FSoftwareRenderer::Init();
	}

	bool IsVulkanReady() const
	{
		return Runtime != NULL && Runtime->IsReady();
	}

private:
	VulkanRuntime *Runtime;
};

FRenderer *vk_CreateInterface()
{
	return new FVulkanRenderer;
}

IVideo *vk_CreateVideo()
{
#ifdef _WIN32
	return new Win32VulkanVideo;
#else
	return NULL;
#endif
}

const FVulkanBackendStats &vk_GetBackendStats()
{
	return VulkanStats;
}
