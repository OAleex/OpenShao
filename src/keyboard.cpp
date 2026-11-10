#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include "linux/virtual_keys.h"
#endif
#include <algorithm>
#include <bitset>
#include <mutex>
#include "keyboard.h"
#include "cheat_menu.h"
#include "recomp_input.h"
#include "controller.h"
#include "mouse.h"
#include "core/input.h"
#include "core/log.h"

namespace openshao::keyboard {
namespace {
std::mutex lock;
std::bitset<256> keys;
bindings::Keyboard settings;
}
void Configure(const bindings::Keyboard& value) {
    std::lock_guard guard(lock);settings=value;keys.reset();
}
bool Mapped(unsigned key) {
    if(!key || key>=256)return false;
    for(const auto& pair:settings.keys)for(int bound:pair)
        if(unsigned(bound)==key || (bound==VK_SHIFT && (key==VK_LSHIFT || key==VK_RSHIFT)) ||
           (bound==VK_CONTROL && (key==VK_LCONTROL || key==VK_RCONTROL)) ||
           (bound==18 && (key==164 || key==165)))return true;
    return false;
}

void Key(unsigned virtual_key, bool down) {
    const bool consumed=recomp::raw::Key(virtual_key,down);
    std::lock_guard guard(lock);
    if (virtual_key < keys.size() && (!down || (!consumed && Mapped(virtual_key)))) keys.set(virtual_key, down);
}
void ReleaseAll() {
    std::lock_guard guard(lock);
    keys.reset();
}
#ifdef _WIN32
bool Message(void*, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    std::lock_guard guard(lock);
    if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE)) {
        keys.reset();
        return false;
    }
    if(wp==VK_SHIFT)if(const auto side=MapVirtualKeyW((unsigned(lp)>>16)&255,MAPVK_VSC_TO_VK_EX))wp=side;
    if(wp==VK_CONTROL)wp=(lp&(intptr_t(1)<<24))?VK_RCONTROL:VK_LCONTROL;
    if(wp==VK_MENU)wp=(lp&(intptr_t(1)<<24))?VK_RMENU:VK_LMENU;
    if((msg==WM_KEYDOWN || msg==WM_KEYUP) && cheat::Message(msg==WM_KEYDOWN?0x100:0x101,wp,lp)){*result=0;return true;}
    if((msg==WM_KEYDOWN || msg==WM_KEYUP) && cheat::HandleKey(unsigned(wp),msg==WM_KEYDOWN)){*result=0;return true;}
    // A key pressed before opening the custom menu must release even when that menu consumes key-up.
    if((msg==WM_KEYUP || msg==WM_SYSKEYUP) && wp<keys.size())keys.reset(wp);
    if((msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP) &&
       recomp::raw::Key(unsigned(wp),msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN)){*result=0;return true;}
    if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp < keys.size() && Mapped(unsigned(wp))) {
        keys.set(wp, msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        LOGI("openshao input: window key=%u down=%u", unsigned(wp), (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? 1u : 0u);
        *result = 0;
        return true;
    }
    return false;
}
#else
bool Message(void*, uint32_t, uintptr_t, intptr_t, intptr_t*) { return false; }
#endif

void Poll(int port, input::Pad& pad) {
    if (port != controller::KeyboardPort()) return;
    std::lock_guard guard(lock);
    pad.connected = true;
    bindings::Values values{};
    const bool native_enter_accept=recomp::raw::NativeEnterAccept();
    for(unsigned i=0;i<values.size();++i)for(int key:settings.keys[i]) {
        if(key<=0 || key>=256 || (native_enter_accept && key==VK_RETURN))continue;
        bool down=keys[key];
        if(key==VK_SHIFT)down=down || keys[VK_LSHIFT] || keys[VK_RSHIFT];
        if(key==VK_CONTROL)down=down || keys[VK_LCONTROL] || keys[VK_RCONTROL];
        if(key==18)down=down || keys[164] || keys[165];
        if(down)values[i]=1;
    }
    // Enter is Start in gameplay but Accept on the medallion pages.
    if(native_enter_accept && keys[VK_RETURN]){
        values[0]=1;
    }
    bindings::Apply(pad,values);
    mouse::Poll(pad);
}
}
