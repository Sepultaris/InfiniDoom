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
	unsigned long long UploadBufferBytes;
	unsigned int LastPresentMS;
	bool LastPresentSucceeded;
};

FRenderer *vk_CreateInterface();
IVideo *vk_CreateVideo();
const FVulkanBackendStats &vk_GetBackendStats();

#endif
