#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif
#include "linux/vk/vk_api.h"

#include "core/log.h"

namespace vkapi {

Table vk;
VkDevice device = VK_NULL_HANDLE;

bool LoadGlobal() {
    if (vk.vkGetInstanceProcAddr) return true;
#ifdef _WIN32
    static HMODULE loader=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!loader){LOGE("vulkan: system Vulkan loader unavailable (%lu)",GetLastError());return false;}
    vk.vkGetInstanceProcAddr=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader,"vkGetInstanceProcAddr"));
#else
    // the window was made with SDL_WINDOW_VULKAN, which loads the library; this only adds a reference
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        LOGE("vulkan: no Vulkan loader (%s)", SDL_GetError());
        return false;
    }
    vk.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
#endif
    if (!vk.vkGetInstanceProcAddr) return false;
#define OPENSHAO_VK_LOAD(name) vk.name = reinterpret_cast<PFN_##name>(vk.vkGetInstanceProcAddr(nullptr, #name));
    OPENSHAO_VK_GLOBAL(OPENSHAO_VK_LOAD)
#undef OPENSHAO_VK_LOAD
    return vk.vkCreateInstance && vk.vkEnumerateInstanceExtensionProperties;
}

bool LoadInstance(VkInstance instance) {
    bool ok = true;
#define OPENSHAO_VK_LOAD(name)                                                                \
    vk.name = reinterpret_cast<PFN_##name>(vk.vkGetInstanceProcAddr(instance, #name));       \
    if (!vk.name) {                                                                          \
        LOGE("vulkan: missing %s", #name);                                                   \
        ok = false;                                                                          \
    }
    OPENSHAO_VK_INSTANCE(OPENSHAO_VK_LOAD)
#undef OPENSHAO_VK_LOAD
    return ok;
}

bool LoadDevice(VkDevice dev) {
    device = dev;
    bool ok = true;
#define OPENSHAO_VK_LOAD(name)                                                       \
    vk.name = reinterpret_cast<PFN_##name>(vk.vkGetDeviceProcAddr(dev, #name));     \
    if (!vk.name) {                                                                 \
        LOGE("vulkan: missing %s", #name);                                          \
        ok = false;                                                                 \
    }
    OPENSHAO_VK_DEVICE(OPENSHAO_VK_LOAD)
#undef OPENSHAO_VK_LOAD
#define OPENSHAO_VK_LOAD2(name, alt)                                                  \
    vk.name = reinterpret_cast<PFN_##name>(vk.vkGetDeviceProcAddr(dev, #name));      \
    if (!vk.name) vk.name = reinterpret_cast<PFN_##name>(vk.vkGetDeviceProcAddr(dev, alt)); \
    if (!vk.name) {                                                                  \
        LOGE("vulkan: missing %s", #name);                                           \
        ok = false;                                                                  \
    }
    OPENSHAO_VK_PROMOTED(OPENSHAO_VK_LOAD2)
#undef OPENSHAO_VK_LOAD2
    return ok;
}

const char* ResultName(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        default: return "VkResult";
    }
}

}
