#ifndef __VK_RENDERER_H
#define __VK_RENDERER_H

#include "doomtype.h"
#include "r_renderer.h"

class IVideo;

struct FVulkanBackendStats
{
	bool Available;
	bool Ready;
	char DeviceName[256];
	unsigned int ApiVersion;
	unsigned int DriverVersion;
	unsigned int VendorID;
	unsigned int DeviceID;
	unsigned int DeviceCount;
	unsigned int GraphicsQueueFamily;
	unsigned int SwapchainImageCount;
	unsigned int SwapchainFormat;
	unsigned int SwapchainColorSpace;
	unsigned int SwapchainWidth;
	unsigned int SwapchainHeight;
	unsigned long long DeviceLocalMemoryBytes;
	unsigned long long DeviceLocalMemoryBudgetBytes;
	unsigned long long DeviceLocalMemoryUsageBytes;
	unsigned long long UploadBufferBytes;
	unsigned int LastPresentMS;
	double LastGpuFrameMS;
	unsigned int SwapchainRecreateCount;
	unsigned int OutOfDateCount;
	unsigned int PresentFilterMode;
	unsigned int PresentScaleMode;
	unsigned int PresentViewportX;
	unsigned int PresentViewportY;
	unsigned int PresentViewportWidth;
	unsigned int PresentViewportHeight;
	unsigned int PresentSourceWidth;
	unsigned int PresentSourceHeight;
	float PresentSharpness;
	float PresentAspect;
	unsigned int SceneProbeVertexCount;
	bool SceneProbeActive;
	bool GpuPresentationActive;
	bool TimestampQueriesAvailable;
	bool MemoryBudgetAvailable;
	bool LastPresentSucceeded;
	bool WindowMinimized;
};

FRenderer *vk_CreateInterface();
IVideo *vk_CreateVideo();
const FVulkanBackendStats &vk_GetBackendStats();

#endif
