#ifndef __VK_RENDERER_H
#define __VK_RENDERER_H

#include "doomtype.h"
#include "r_renderer.h"

class IVideo;

FRenderer *vk_CreateInterface();
IVideo *vk_CreateVideo();

#endif
