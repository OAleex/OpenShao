// Native HWND ownership and client sizes, with only the driver call substituted.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "linux/vk/vk_window.h"

namespace { HWND window; bool procedure=true; unsigned created=0; }
namespace app { void* Window(){return window;} }
namespace vkapi { Table vk; }
namespace {
void Require(bool good,const char* message){if(!good)throw std::runtime_error(message);}
VKAPI_ATTR VkResult VKAPI_CALL Create(VkInstance instance,const VkWin32SurfaceCreateInfoKHR* info,
                                      const VkAllocationCallbacks*,VkSurfaceKHR* surface){
    Require(instance==(VkInstance)uintptr_t(1) && info->sType==VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR &&
            info->hwnd==window && info->hinstance==GetModuleHandleW(nullptr),
            "the Vulkan surface must use the owned Win32 window and application instance");
    ++created;*surface=(VkSurfaceKHR)uint64_t(2);return VK_SUCCESS;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Get(VkInstance,const char* name){
    return procedure && !std::strcmp(name,"vkCreateWin32SurfaceKHR")?reinterpret_cast<PFN_vkVoidFunction>(Create):nullptr;
}
}
int main(){
    try {
        WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"OpenShaoVulkanWindowTest";
        Require(RegisterClassW(&cls)!=0,"the isolated native fixture must register");
        window=CreateWindowExW(0,cls.lpszClassName,L"",WS_POPUP,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
        Require(window && !IsWindowVisible(window),"the fixture must stay hidden");
        int width=0,height=0;vkplatform::WindowSize(width,height);
        Require(width==640 && height==480,"Vulkan must query the native client size instead of treating HWND as SDL_Window");
        Require(SetWindowPos(window,nullptr,0,0,960,720,SWP_NOACTIVATE|SWP_NOZORDER),"the hidden fixture must resize");
        vkplatform::WindowSize(width,height);
        Require(width==960 && height==720,"Vulkan swapchain sizing must observe native resizes");
        const auto extensions=vkplatform::SurfaceExtensions();
        bool surface_ext=false,win32_ext=false;
        for(auto name:extensions){surface_ext|=!std::strcmp(name,"VK_KHR_surface");win32_ext|=!std::strcmp(name,"VK_KHR_win32_surface");}
        Require(surface_ext && win32_ext,"Windows instance creation must request the native surface extensions");
        vkapi::vk.vkGetInstanceProcAddr=Get;
        VkSurfaceKHR surface=VK_NULL_HANDLE;
        Require(vkplatform::CreateSurface((VkInstance)uintptr_t(1),surface) && created==1 && surface==(VkSurfaceKHR)uint64_t(2),
                "native surface creation must report the driver result");
        procedure=false;surface=VK_NULL_HANDLE;
        Require(!vkplatform::CreateSurface((VkInstance)uintptr_t(1),surface) && !surface && created==1,
                "a missing Win32 surface entry point must fail without calling a null procedure");
        DestroyWindow(window);window=nullptr;UnregisterClassW(cls.lpszClassName,cls.hInstance);
        std::puts("PASS: native Vulkan window ownership, resize and failure handling");
    }catch(const std::exception& e){if(window)DestroyWindow(window);std::fprintf(stderr,"%s\n",e.what());return 1;}
}
