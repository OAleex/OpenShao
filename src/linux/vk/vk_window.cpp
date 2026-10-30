#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif
#include "linux/vk/vk_window.h"
#include "app/app.h"
#include "core/log.h"
namespace vkplatform {
std::vector<const char*> SurfaceExtensions(){
#ifdef _WIN32
    return {VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
#else
    Uint32 count=0;
    const auto extensions=SDL_Vulkan_GetInstanceExtensions(&count);
    if(!extensions){LOGE("vulkan: no surface extensions (%s)",SDL_GetError());return {};}
    return {extensions,extensions+count};
#endif
}
bool CreateSurface(VkInstance instance,VkSurfaceKHR& surface){
    surface=VK_NULL_HANDLE;
#ifdef _WIN32
    const auto create=vkapi::vk.vkGetInstanceProcAddr?reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkapi::vk.vkGetInstanceProcAddr(instance,"vkCreateWin32SurfaceKHR")):nullptr;
    if(!create){LOGE("vulkan: missing vkCreateWin32SurfaceKHR");return false;}
    VkWin32SurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    info.hinstance=GetModuleHandleW(nullptr);
    info.hwnd=static_cast<HWND>(app::Window());
    const VkResult result=create(instance,&info,nullptr,&surface);
    if(result!=VK_SUCCESS)LOGE("vulkan: Win32 surface creation failed (%d)",int(result));
    return result==VK_SUCCESS;
#else
    if(SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(app::Window()),instance,nullptr,&surface))return true;
    LOGE("vulkan: no window surface (%s)",SDL_GetError());return false;
#endif
}
void WindowSize(int& width,int& height){
    width=height=0;
#ifdef _WIN32
    RECT rect{};
    if(GetClientRect(static_cast<HWND>(app::Window()),&rect)){width=rect.right-rect.left;height=rect.bottom-rect.top;}
#else
    SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(app::Window()),&width,&height);
#endif
}
}
