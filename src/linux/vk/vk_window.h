#pragma once
#include <vector>
#include "linux/vk/vk_api.h"

namespace vkplatform {
std::vector<const char*> SurfaceExtensions();
bool CreateSurface(VkInstance instance,VkSurfaceKHR& surface);
void WindowSize(int& width,int& height);
}
