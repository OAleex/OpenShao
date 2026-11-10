#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "platform_compat.h"
#include <SDL3/SDL.h>
#include <toml++/toml.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include "controller.h"
#include "launcher_settings.h"
#include "recomp_input.h"
#include "recomp_settings.h"
#include "survival_session.h"
#include "input_bindings.h"
#include "host.h"
#include "keyboard.h"
#include "mouse.h"
#include "netplay/session.h"
#include "app/app.h"
#include "core/input.h"
#include "core/log.h"

namespace {
struct Slot {
    SDL_Gamepad* device=nullptr;
    input::Pad state{};
    bool rumble=false, motor_warning=false;
    uint16_t desired[2]{}, sent[2]{};
    uint64_t refreshed=0;
    uint64_t activity_logged=0;
};
std::mutex lock;
std::array<Slot,4> slots;
bool initialized=false, focused=false;
unsigned long owner_thread=0;
#ifdef _WIN32
HWND game_window=nullptr;
#else
void* game_window=nullptr;  // SDL_Window; the Linux host calls Update from its event loop
#endif
#ifdef _WIN32
constexpr UINT_PTR timer=0x534D4354;
#endif
uint64_t last_device_scan=0;
float deadzone=.15f;
bool rumble_enabled=true;
bool multiplayer=false;
int auto_keyboard=0;
std::array<SDL_Joystick*,4> fixtures{};
bool Enabled() { return !openshao::options.background || openshao::options.virtual_gamepads; }
int PhysicalPort(int port) {
    return port>=0 && port<int(slots.size())?port:-1;
}
void UpdateKeyboardPort() {
    const int port=multiplayer && slots[0].device?-1:0;
    if(auto_keyboard!=port)LOGI("input: keyboard=%d after controller topology change",port+1);
    auto_keyboard=port;
}
bool StandbyPort(int port) {
    return port==1 && !multiplayer && slots[0].device;
}
auto controller_bindings=openshao::bindings::controller_defaults;
void Sticks(int16_t raw_x,int16_t raw_y,int16_t& x,int16_t& y) {
    float fx=raw_x/32768.0f, fy=-raw_y/32768.0f, length=std::hypot(fx,fy);
    if (length<=deadzone) { x=y=0; return; }
    float gain=(std::min(length,1.0f)-deadzone)/((1-deadzone)*length);
    auto encode=[](float v) { return int16_t(std::clamp(std::lround(v*32768),-32768l,32767l)); };
    x=encode(fx*gain); y=encode(fy*gain);
}
void Add(SDL_JoystickID id) {
    for (auto& s:slots) if (s.device && SDL_GetGamepadID(s.device)==id) return;
    auto it=std::find_if(slots.begin(),slots.end(),[](const Slot& s){return !s.device;});
    // A new Ko-op session starts with keyboard P1; later devices fill the missing player.
    if(multiplayer && !slots[0].device && !slots[1].device)it=slots.begin()+1;
    if (it==slots.end()) return;
    auto* pad=SDL_OpenGamepad(id);
    if (!pad) { LOGW("input: cannot open controller: %s",SDL_GetError()); return; }
    it->device=pad;
    it->rumble=SDL_GetBooleanProperty(SDL_GetGamepadProperties(pad),SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,false);
    UpdateKeyboardPort();
    LOGI("input: port=%d name=%s type=%s connection=%d vendor=%04X product=%04X rumble=%d",
         int(it-slots.begin()),SDL_GetGamepadName(pad),SDL_GetGamepadStringForType(SDL_GetGamepadType(pad)),
         int(SDL_GetGamepadConnectionState(pad)),SDL_GetGamepadVendor(pad),SDL_GetGamepadProduct(pad),it->rumble);
}
void Remove(SDL_JoystickID id) {
    for (auto& s:slots) if (s.device && SDL_GetGamepadID(s.device)==id) {
        SDL_CloseGamepad(s.device); s={};
        LOGI("input: port=%d disconnected",int(&s-slots.data()));
    }
    UpdateKeyboardPort();
}
void AttachFixture(unsigned index) {
    if (fixtures[index]) return;
    SDL_VirtualJoystickDesc desc; SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.name="OpenShao isolated diagnostic pad";
    desc.Rumble=[](void*,Uint16,Uint16)->bool{return true;};
    const auto id=SDL_AttachVirtualJoystick(&desc);
    fixtures[index]=id?SDL_OpenJoystick(id):nullptr;
    if (!fixtures[index]) {if(id)SDL_DetachVirtualJoystick(id);LOGE("input: cannot create diagnostic gamepad");return;}
    SDL_SetJoystickVirtualAxis(fixtures[index],SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768);
    SDL_SetJoystickVirtualAxis(fixtures[index],SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
}
void DetachFixture(unsigned index) {
    if (!fixtures[index]) return;
    const auto id=SDL_GetJoystickID(fixtures[index]);
    SDL_CloseJoystick(fixtures[index]);SDL_DetachVirtualJoystick(id);fixtures[index]=nullptr;
}
}
namespace openshao::controller {
void SetMultiplayer(bool enabled) {
    std::lock_guard guard(lock);
    if (multiplayer==enabled) return;
    multiplayer=enabled;
    const auto count=std::count_if(slots.begin(),slots.end(),[](const Slot& s){return s.device!=nullptr;});
    // One controller enters Ko-op on P2 and returns to solo on P1.
    std::array<Slot,4> assigned{};
    int port=enabled && count<2?1:0;
    for(const auto& s:slots)if(s.device)assigned[port++]=s;
    slots=assigned;
    UpdateKeyboardPort();
    // Mode changes run on the guest thread; defer SDL motor updates to the window thread.
    for (auto& s:slots) s.desired[0]=s.desired[1]=0;
    LOGI("input: multiplayer=%d keyboard=%d reserved_port=%d",enabled,auto_keyboard+1,enabled && auto_keyboard==0?0:-1);
}
int KeyboardPort() {
    std::lock_guard guard(lock);
    return auto_keyboard;
}
void Configure(const launcher::Settings& settings) {
    std::lock_guard guard(lock);
    deadzone=float(settings.deadzone);rumble_enabled=settings.rumble;controller_bindings=settings.bindings;
    for(auto& source:controller_bindings)source=openshao::bindings::CanonicalSource(std::move(source));
    if(!rumble_enabled)for(auto& s:slots)s.desired[0]=s.desired[1]=0;
}
void Load(const void* raw) {
    const auto c=(*static_cast<const toml::table*>(raw))["controller"];
    const double value=c["deadzone"].value_or(.15);
    deadzone=std::isfinite(value)?float(std::clamp(value,0.0,.5)):.15f;
    rumble_enabled=c["rumble"].value_or(true);
    if(!rumble_enabled)for(auto& s:slots)s.desired[0]=s.desired[1]=0;
    multiplayer=false;auto_keyboard=0;
    controller_bindings=openshao::bindings::controller_defaults;
    for(size_t i=0;i<controller_bindings.size();++i)if(auto name=c["bindings"][openshao::bindings::keys[i]].value<std::string>()) {
        if(openshao::bindings::ValidSource(*name))controller_bindings[i]=openshao::bindings::CanonicalSource(*name);
        else LOGW("input: ignored unknown binding %s=%s",openshao::bindings::keys[i],name->c_str());
    }
}
void Save(void* raw) {
    auto& table=*static_cast<toml::table*>(raw);
    toml::table mapped;
    for (size_t i=0;i<controller_bindings.size();++i) mapped.insert(openshao::bindings::keys[i],controller_bindings[i]);
    table.insert_or_assign("controller",toml::table{{"deadzone",double(deadzone)},{"rumble",rumble_enabled},
        {"bindings",std::move(mapped)}});
}
void SetFocused(bool value) {
    if(!value)recomp::raw::FocusLost();
    std::lock_guard guard(lock);
    if (focused!=value) LOGI("input: game window focus=%d",value);
    focused=value;
    if (!focused) for (auto& s:slots) {
        bool connected=s.state.connected; s.state={}; s.state.connected=connected;
        s.desired[0]=s.desired[1]=0;
    }
}
bool HasRumble(int port) {
    std::lock_guard guard(lock);
    const int index=PhysicalPort(port);
    if (index>=0 && slots[index].device) return slots[index].rumble;
    // The guest caches capabilities before confirming Ko-op and binding P2.
    return StandbyPort(port) && app::kGame.pad && slots[0].rumble;
}
bool RumbleEnabled(){std::lock_guard guard(lock);return rumble_enabled;}
void Update() {
    if (!Enabled() || !initialized || GetCurrentThreadId()!=owner_thread) return;
    SDL_PumpEvents();
    std::lock_guard guard(lock);
    SDL_Event event;
    while (SDL_PeepEvents(&event,1,SDL_GETEVENT,SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)==1) {
        if (event.type==SDL_EVENT_GAMEPAD_ADDED) Add(event.gdevice.which);
        else if (event.type==SDL_EVENT_GAMEPAD_REMOVED) Remove(event.gdevice.which);
    }
    const uint64_t now=SDL_GetTicks();
    // Reconcile SDL devices because Bluetooth reconnects may omit a usable gamepad event.
    if (now-last_device_scan>=750) {
        int count=0;
        if (SDL_JoystickID* ids=SDL_GetGamepads(&count)) {
            // Release stale ports before opening replacements from the same scan.
            for (auto& s:slots) if (s.device && !SDL_GamepadConnected(s.device)) {
                const auto index=int(&s-slots.data());
                SDL_CloseGamepad(s.device); s={};
                LOGI("input: port=%d disconnected (device scan)",index);
            }
            UpdateKeyboardPort();
            for (int i=0;i<count;++i) Add(ids[i]);
            SDL_free(ids);
        }
        last_device_scan=now;
    }
    std::string raw_source;
    uint32_t raw_buttons=0;
    for (auto& s:slots) {
        const auto previous=s.state;s.state={}; auto* pad=s.device;
        if (!pad || !SDL_GamepadConnected(pad)) continue;
        auto& p=s.state; p.connected=true;
        if (focused) {
            for(int button=0;button<SDL_GAMEPAD_BUTTON_COUNT;++button)if(SDL_GetGamepadButton(pad,SDL_GamepadButton(button))) {
                if(raw_source.empty())if(const auto* name=SDL_GetGamepadStringForButton(SDL_GamepadButton(button)))raw_source=name;
                switch(button) {
                case SDL_GAMEPAD_BUTTON_SOUTH:raw_buttons|=0x40;break;case SDL_GAMEPAD_BUTTON_EAST:raw_buttons|=0x20;break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:raw_buttons|=0x1000;break;case SDL_GAMEPAD_BUTTON_DPAD_DOWN:raw_buttons|=0x4000;break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:raw_buttons|=0x8000;break;case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:raw_buttons|=0x2000;break;
                }
            }
            for(int axis=0;axis<SDL_GAMEPAD_AXIS_COUNT;++axis) {
                const int value=SDL_GetGamepadAxis(pad,SDL_GamepadAxis(axis));
                if(axis<4?std::abs(value)>20000:value>16000) {
                    const char* names[]={"leftx","lefty","rightx","righty","lefttrigger","righttrigger"};
                    if(raw_source.empty()){raw_source=names[axis];if(axis<4)raw_source+=value<0?'-':'+';}
                    if(axis==0)raw_buttons|=value<0?0x8000:0x2000;
                    if(axis==1)raw_buttons|=value<0?0x1000:0x4000;
                }
            }
            int16_t lx=0,ly=0,rx=0,ry=0;
            Sticks(SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX),SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTY),lx,ly);
            Sticks(SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHTX),SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHTY),rx,ry);
            const float axes[]={lx/32768.f,-ly/32768.f,rx/32768.f,-ry/32768.f};
            openshao::bindings::Values values{};
            for(unsigned i=0;i<values.size();++i) {
                const auto& source=controller_bindings[i];
                if(source=="lefttrigger" || source=="righttrigger")
                    values[i]=std::max(0,int(SDL_GetGamepadAxis(pad,source=="lefttrigger"?SDL_GAMEPAD_AXIS_LEFT_TRIGGER:SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)))/32767.f;
                else if(source.size()==6 && (source.back()=='+' || source.back()=='-')) {
                    const int axis=source.starts_with("left")?(source[4]=='x'?0:1):(source[5]=='x'?2:3);
                    values[i]=std::max(0.f,axes[axis]*(source.back()=='+'?1.f:-1.f));
                } else if(source.size()==7 && (source.back()=='+' || source.back()=='-')) {
                    const int axis=source[5]=='x'?2:3;
                    values[i]=std::max(0.f,axes[axis]*(source.back()=='+'?1.f:-1.f));
                } else {
                    const auto button=SDL_GetGamepadButtonFromString(source.c_str());
                    if(button!=SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(pad,button))values[i]=1;
                }
            }
            openshao::bindings::Apply(p,values);
            bool changed=(p.buttons&~previous.buttons)!=0;
            for (int i=0;i<8;++i) changed|=p.analog[i]>previous.analog[i];
            changed|=std::abs(int(p.lx)-previous.lx)>4000 || std::abs(int(p.ly)-previous.ly)>4000 ||
                     std::abs(int(p.rx)-previous.rx)>4000 || std::abs(int(p.ry)-previous.ry)>4000;
            if (changed) {
                if (now-s.activity_logged>=200) {
                    LOGI("input: gamepad port=%d buttons=%04X A=%u B=%u left=%d,%d",
                        int(&s-slots.data()),p.buttons,p.analog[0],p.analog[1],p.lx,p.ly);
                    s.activity_logged=now;
                }
            }
        }
        const uint16_t low=focused && rumble_enabled?s.desired[0]:0, high=focused && rumble_enabled?s.desired[1]:0;
        if (s.rumble && (low!=s.sent[0] || high!=s.sent[1] || ((low||high) && now-s.refreshed>=100))) {
            if (!SDL_RumbleGamepad(pad,low,high,(low||high)?300:0) && !s.motor_warning) {
                LOGW("input: rumble unavailable: %s",SDL_GetError()); s.motor_warning=true;
            }
            s.sent[0]=low; s.sent[1]=high; s.refreshed=now;
        }
    }
    // One combined snapshot, so idle pads cannot release another pad's buttons.
    recomp::raw::Controller(raw_source,raw_buttons);
}
void Shutdown() {
    if (!initialized || GetCurrentThreadId()!=owner_thread) return;
#ifdef _WIN32
    if (game_window) KillTimer(game_window,timer);
#endif
    std::lock_guard guard(lock);
    for (auto& s:slots) if (s.device) {
        if (s.sent[0]||s.sent[1]) SDL_RumbleGamepad(s.device,0,0,0);
        SDL_CloseGamepad(s.device); s={};
    }
    for (unsigned i=0;i<fixtures.size();++i) DetachFixture(i);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD); initialized=false; game_window=nullptr;
    multiplayer=false;auto_keyboard=0;last_device_scan=0;
}
void Window(void* hwnd) {
    mouse::Window(hwnd);
    if (!Enabled() || !initialized) return;
#ifndef _WIN32
    game_window=hwnd;
    SetFocused(true);
#else
    game_window=static_cast<HWND>(hwnd);
    SetFocused(options.background || GetForegroundWindow()==game_window);
    if (!SetTimer(game_window,timer,8,nullptr)) LOGE("input: controller timer failed");
#endif
    Update();
}
bool Message(void* hwnd,uint32_t message,uintptr_t wp,intptr_t lp,intptr_t* result) {
#ifndef _WIN32
    (void)hwnd;(void)message;(void)wp;(void)lp;(void)result;
    return false;  // Linux delivers SDL events through linux_input.cpp
#else
    if (mouse::Message(hwnd, message, wp, lp, result)) return true;
    // Accept fixture messages only in the owned hidden window with physical backends disabled.
    if (options.background && options.virtual_gamepads && hwnd==game_window &&
        (message==WM_APP+0x51 || message==WM_APP+0x52 || message==WM_APP+0x53)) {
        unsigned index=HIWORD(wp), control=LOWORD(wp);
        if (message==WM_APP+0x53 && index<options.virtual_gamepads) {
            if (lp) AttachFixture(index);else DetachFixture(index);
            Update();*result=0;return true;
        }
        if (index<fixtures.size() && fixtures[index]) {
            if (message==WM_APP+0x51 && control<SDL_GAMEPAD_BUTTON_COUNT)
                SDL_SetJoystickVirtualButton(fixtures[index],int(control),lp!=0);
            if (message==WM_APP+0x52 && control<SDL_GAMEPAD_AXIS_COUNT && lp>=-32768 && lp<=32767)
                SDL_SetJoystickVirtualAxis(fixtures[index],int(control),Sint16(lp));
            Update();
        }
        *result=0; return true;
    }
    if (Enabled()) {
        if (message==WM_TIMER && wp==timer) {
            if (!options.background && game_window && IsWindowVisible(game_window)) {
                const HWND active=GetForegroundWindow();
                SetFocused(active==game_window || (active && GetAncestor(active,GA_ROOTOWNER)==game_window));
            }
            Update(); *result=0; return true;
        }
        if (message==WM_SETFOCUS || (message==WM_ACTIVATE && LOWORD(wp)!=WA_INACTIVE)) SetFocused(true);
        if (message==WM_KILLFOCUS || (message==WM_ACTIVATE && LOWORD(wp)==WA_INACTIVE)) SetFocused(false);
        if (message==WM_ACTIVATEAPP) SetFocused(wp!=0);
        if (message==WM_DESTROY) Shutdown();
    }
    return keyboard::Message(hwnd,message,wp,lp,result);
#endif
}
}
namespace input {
void Init() {
    if (!Enabled() || initialized) return;
    if (openshao::options.background) {
        for (const char* hint : {SDL_HINT_JOYSTICK_HIDAPI,SDL_HINT_JOYSTICK_WGI,SDL_HINT_XINPUT_ENABLED,
                SDL_HINT_JOYSTICK_DIRECTINPUT,SDL_HINT_JOYSTICK_RAWINPUT,SDL_HINT_JOYSTICK_GAMEINPUT,
                SDL_HINT_JOYSTICK_HIDAPI_PS4,SDL_HINT_JOYSTICK_HIDAPI_PS5})
            SDL_SetHintWithPriority(hint,"0",SDL_HINT_OVERRIDE);
    }
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS,"auto");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) { LOGE("input: SDL initialization failed: %s",SDL_GetError()); return; }
    owner_thread=GetCurrentThreadId(); initialized=true;
    for (unsigned i=0;i<openshao::options.virtual_gamepads;++i) AttachFixture(i);
    const auto db=(openshao::options.data_dir.empty()?app::ExeDir():openshao::options.data_dir)/"gamecontrollerdb.txt";
    if (std::filesystem::is_regular_file(db)) LOGI("input: local mappings added=%d",SDL_AddGamepadMappingsFromFile(db.string().c_str()));
    int count=0;
    if (auto* ids=SDL_GetGamepads(&count)) {
        std::lock_guard guard(lock);
        for (int i=0;i<count;++i) Add(ids[i]);
        SDL_free(ids);
    }
    LOGI("input: SDL %d, %d gamepads; PS4 Bluetooth/USB via HIDAPI",SDL_GetVersion(),count);
}
namespace {
// The physical devices and keyboard, before session samples and pause filtering.
Pad LivePoll(int port) {
    Pad p{};bool standby=false;
    if (Enabled()) {
        std::lock_guard guard(lock);const int index=PhysicalPort(port);if (index>=0) p=slots[index].state;
        standby=StandbyPort(port);
    }
    if (app::kGame.pad) app::kGame.pad(port,p);
    if (standby && !p.connected && app::kGame.pad) {
        // Ko-op requires two advertised sources; never copy P1 input into P2.
        Pad keyboard{};app::kGame.pad(0,keyboard);p.connected=keyboard.connected;
    }
    return p;
}
[[maybe_unused]] const bool live_source=(openshao::netplay::session::SetLiveSource(LivePoll),true);
}
Pad Poll(int port) {
    // A recording or replay hands every game thread the same sample for the whole frame.
    Pad p=openshao::netplay::session::Poll(port);
    if(p.connected)p.buttons=openshao::survival::PauseButtons(p.buttons,openshao::survival::ActiveSession());
    return p;
}
void Rumble(int port,uint16_t left,uint16_t right) {
    if(left || right)LOG_FIRST(8,"input: game vibration port=%d low=%u high=%u",port,unsigned(left),unsigned(right));
    if (openshao::options.background) return;
    if (app::kGame.rumble) app::kGame.rumble(port,left,right);
    std::lock_guard guard(lock);
    const int index=PhysicalPort(port);
    if (index>=0 && slots[index].device && focused && rumble_enabled) { slots[index].desired[0]=left; slots[index].desired[1]=right; }
}
}
