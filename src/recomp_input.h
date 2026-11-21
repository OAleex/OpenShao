#pragma once
#include "input_bindings.h"
#include <bitset>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
namespace openshao::recomp::raw {
enum class Device {None,Controller,Keyboard};
struct Result {Device device=Device::None;std::string source;int key=0;bool cancelled=false;};
using Clock=std::chrono::steady_clock;
inline constexpr auto CancelHold=std::chrono::milliseconds(500);
inline constexpr uint32_t PhysicalB=0x20;
inline std::mutex mutex;
inline bool menu=false,neutral=false;
inline bool cancel_held=false;
inline std::optional<Clock::time_point> b_pressed;
inline Device capture=Device::None;
inline std::bitset<256> held;
inline bool pending_enter=false;
inline std::atomic<bool> native_enter_accept{false};
inline void NativeEnterAccept(bool enabled){native_enter_accept.store(enabled,std::memory_order_relaxed);}
inline bool NativeEnterAccept(){return native_enter_accept.load(std::memory_order_relaxed);}
inline Clock::time_point began;
inline Result result;
inline uint32_t navigation=0,last_controller=0;
inline void Active(bool value){std::lock_guard guard(mutex);menu=value;if(!value){capture=Device::None;result={};navigation=0;b_pressed.reset();}}
inline bool Active(){std::lock_guard guard(mutex);return menu;}
inline void Begin(Device device){std::lock_guard guard(mutex);capture=device;result={};neutral=device==Device::Keyboard && held.none();began=Clock::now();navigation=0;b_pressed.reset();}
inline void Cancel(){std::lock_guard guard(mutex);capture=Device::None;result={};navigation=0;b_pressed.reset();}
inline Device Capturing(){std::lock_guard guard(mutex);return capture;}
inline Result Take(){std::lock_guard guard(mutex);auto value=std::move(result);result={};return value;}
inline bool Expired(){std::lock_guard guard(mutex);if(capture==Device::None || Clock::now()-began<std::chrono::seconds(15))return false;capture=Device::None;cancel_held|=b_pressed.has_value();b_pressed.reset();return true;}
inline uint32_t Navigation(){std::lock_guard guard(mutex);const auto value=navigation;navigation=0;return value;}
inline uint32_t ControllerButtons(){std::lock_guard guard(mutex);return last_controller;}
inline bool ControllerCancelHeld(){std::lock_guard guard(mutex);return cancel_held;}
inline bool Key(unsigned key,bool down) {
    if(key>=256)return false;
    std::lock_guard guard(mutex);
    if(key==13 && down && !held[key] && !menu && NativeEnterAccept())pending_enter=true;
    held[key]=down;
    if(capture==Device::Keyboard) {
        if(held.none())neutral=true;
        if(neutral && down && bindings::ValidKey(int(key))){result={Device::Keyboard,{},int(key)};capture=Device::None;}
        return bindings::ValidKey(int(key));
    }
    if(!menu)return false;
    if(down) {
        if(key==38)navigation|=0x1000;if(key==40)navigation|=0x4000;if(key==37)navigation|=0x8000;if(key==39)navigation|=0x2000;
        if(key==13 || key==32)navigation|=0x40;if(key==27 || key==8)navigation|=0x20;
        if(key==46)navigation|=0x80;
    }
    return key==38 || key==40 || key==37 || key==39 || key==13 || key==32 || key==27 || key==8 || key==46;
}
inline bool TakeEnter() {std::lock_guard guard(mutex);const bool pressed=pending_enter;pending_enter=false;return pressed;}
inline void Controller(std::string_view source,uint32_t buttons,Clock::time_point now=Clock::now()) {
    std::lock_guard guard(mutex);
    const bool b_down=(buttons&PhysicalB)!=0;
    if(!b_down)cancel_held=false;
    if(capture==Device::Controller) {
        if(source.empty() && !b_down)neutral=true;
        if(b_pressed) {
            if(now-*b_pressed>=CancelHold) {
                result.cancelled=true;capture=Device::None;cancel_held=b_down;b_pressed.reset();
            } else if(!b_down) {
                result={Device::Controller,"b",0};capture=Device::None;b_pressed.reset();
            }
        } else if(neutral && b_down) {
            b_pressed=now;
        } else if(neutral && !source.empty() && bindings::ValidSource(std::string(source))) {
            result={Device::Controller,std::string(source),0};capture=Device::None;
        }
    } else if(menu && !cancel_held)navigation|=buttons&~last_controller;
    last_controller=buttons;
}
inline void FocusLost() {
    std::lock_guard guard(mutex);
    const bool capture_interrupted=capture!=Device::None || result.device!=Device::None || result.cancelled;
    held.reset();pending_enter=false;native_enter_accept.store(false,std::memory_order_relaxed);
    capture=Device::None;result={};result.cancelled=capture_interrupted;
    navigation=0;last_controller=0;b_pressed.reset();cancel_held=false;
}
}
