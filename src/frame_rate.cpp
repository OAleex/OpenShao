#include "frame_rate.h"
#include "app/app.h"
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif

namespace openshao::frames {
int Target() {
    if(!synchronize.load(std::memory_order_relaxed))return target_rate.load(std::memory_order_relaxed);
    using Clock=std::chrono::steady_clock;
    static thread_local auto next=Clock::time_point{};
    static thread_local double refresh=60;
    const auto now=Clock::now();
    if(now>=next) {
        next=now+std::chrono::seconds(1);
        refresh=60;
#ifdef _WIN32
        MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);
        DEVMODEW mode{};mode.dmSize=sizeof(mode);
        if(GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(app::Window()),MONITOR_DEFAULTTONEAREST),&monitor) &&
           EnumDisplaySettingsW(monitor.szDevice,ENUM_CURRENT_SETTINGS,&mode))refresh=mode.dmDisplayFrequency;
#else
        const auto display=SDL_GetDisplayForWindow(static_cast<SDL_Window*>(app::Window()));
        if(const auto* mode=SDL_GetCurrentDisplayMode(display?display:SDL_GetPrimaryDisplay()))refresh=mode->refresh_rate;
#endif
    }
    return EffectiveRate(target_rate.load(std::memory_order_relaxed),true,refresh);
}
}
