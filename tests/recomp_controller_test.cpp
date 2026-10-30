// Synthetic SDL devices only: never opens or vibrates the user's controller.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <toml++/toml.hpp>
#include "app/app.h"
#include "core/input.h"
#include "host.h"
#include "controller.h"
#include "keyboard.h"
#include "input_keys_sdl.h"
#include "launcher_input_capture.h"
#include "launcher_settings.h"
#include "recomp_input.h"
static bool keyboard_active=false;
static void TestKeyboard(int port,input::Pad& pad) {if(keyboard_active) openshao::keyboard::Poll(port,pad);}
namespace app { const GameInfo kGame{.pad=TestKeyboard}; std::filesystem::path ExeDir() { return std::filesystem::current_path(); } }
namespace openshao { Options options; }
static void Check(bool value,const char* description) {
    if (!value) { std::fprintf(stderr,"FAIL: %s (%s)\n",description,SDL_GetError()); std::exit(1); }
}
// The real game only calls controller::Update from its Win32 timer.
// Never refresh SDL separately here: that would hide an input pump bug.
static void Tick() { openshao::controller::Update(); }
static Uint16 low_motor=0, high_motor=0;
static bool SDLCALL Motor(void*,Uint16 low,Uint16 high) { low_motor=low; high_motor=high; return true; }
static LRESULT CALLBACK TestWindow(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    intptr_t result=0;
    if (openshao::controller::Message(hwnd,msg,wp,lp,&result)) return result;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static SDL_JoystickID Attach(const char* name) {
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes=SDL_GAMEPAD_AXIS_COUNT; d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    d.name=name; d.Rumble=Motor;
    return SDL_AttachVirtualJoystick(&d);
}
static void TestControllerHandoff() {
    const toml::table automatic;
    unsigned failures=0;
    auto expect=[&](bool ok,const char* text) {
        if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",text);}
    };
    keyboard_active=true;
    for(bool before_entry:{true,false}) {
        openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
        auto host=Attach("OpenShao departing host");auto guest=Attach("OpenShao remaining guest");
        auto* guest_joy=SDL_OpenJoystick(guest);
        Check(host && guest && guest_joy,"attach host and guest handoff devices");Tick();
        SDL_SetJoystickVirtualButton(guest_joy,SDL_GAMEPAD_BUTTON_SOUTH,true);
        SDL_SetJoystickVirtualButton(guest_joy,SDL_GAMEPAD_BUTTON_DPAD_LEFT,true);
        SDL_SetJoystickVirtualAxis(guest_joy,SDL_GAMEPAD_AXIS_LEFTX,32767);
        SDL_SetJoystickVirtualAxis(guest_joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);Tick();
        if(!before_entry)openshao::controller::SetMultiplayer(true);
        SDL_DetachVirtualJoystick(host);Tick();
        if(before_entry)openshao::controller::SetMultiplayer(true);
        const auto guest_state=input::Poll(1);
        expect(openshao::controller::KeyboardPort()==0 && input::Poll(0).connected,
               before_entry?"host unplug before Ko-op keeps keyboard P1":
                            "host unplug during Ko-op restores keyboard P1");
        expect(guest_state.connected && guest_state.analog[0]==255 && guest_state.analog[7]==255 &&
               (guest_state.buttons&4) && guest_state.lx>32000 && !input::Poll(2).connected,
               before_entry?"remaining controller enters Ko-op as P2 with all buttons and axes":
                            "host unplug preserves the guest's P2 buttons and axes");
        expect(!openshao::controller::HasRumble(0) && openshao::controller::HasRumble(1),
               "handoff keeps vibration capabilities on the guest controller");
        openshao::keyboard::Key('W',true);
        expect(input::Poll(0).ly==32767 && !input::Poll(0).analog[0] && !input::Poll(1).ly,
               "keyboard recovery controls only P1 while the guest keeps P2");
        openshao::keyboard::ReleaseAll();
        host=Attach("OpenShao returning host");Tick();
        expect(openshao::controller::KeyboardPort()==-1 && input::Poll(0).connected &&
               !input::Poll(0).analog[0] && input::Poll(1).analog[0]==255 && !input::Poll(2).connected,
               "returning host fills P1 without moving the guest to another player");
        SDL_DetachVirtualJoystick(host);Tick();
        openshao::controller::SetMultiplayer(false);
        expect(input::Poll(0).analog[0]==255 && !input::Poll(1).analog[0],
               "returning to solo shares the remaining controller with keyboard P1");
        SDL_CloseJoystick(guest_joy);SDL_DetachVirtualJoystick(guest);Tick();
        openshao::controller::Shutdown();
    }
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    openshao::controller::SetMultiplayer(true);
    auto guest=Attach("OpenShao first late Ko-op controller");auto* guest_joy=SDL_OpenJoystick(guest);
    Check(guest && guest_joy,"attach late guest for handoff");
    SDL_SetJoystickVirtualButton(guest_joy,SDL_GAMEPAD_BUTTON_EAST,true);Tick();
    auto host=Attach("OpenShao second late Ko-op controller");Tick();
    expect(input::Poll(0).connected && input::Poll(1).analog[1]==255 && !input::Poll(2).connected &&
           openshao::controller::KeyboardPort()==-1,
           "second late controller fills P1 while the first controller retains P2");
    SDL_DetachVirtualJoystick(host);SDL_CloseJoystick(guest_joy);SDL_DetachVirtualJoystick(guest);Tick();
    expect(openshao::controller::KeyboardPort()==0 && input::Poll(0).connected && !input::Poll(1).connected,
           "disconnecting both controllers preserves keyboard P1");
    guest=Attach("OpenShao guest after both disconnect");guest_joy=SDL_OpenJoystick(guest);
    Check(guest && guest_joy,"reconnect guest after both disconnect");
    SDL_SetJoystickVirtualButton(guest_joy,SDL_GAMEPAD_BUTTON_NORTH,true);Tick();
    expect(input::Poll(1).analog[3]==255 && !input::Poll(0).analog[3],
           "first reconnect after both disconnect restores keyboard P1 and controller P2");
    SDL_CloseJoystick(guest_joy);SDL_DetachVirtualJoystick(guest);Tick();openshao::controller::Shutdown();
    // Reconcile a host replacement when both SDL hot-plug events were missed.
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    host=Attach("OpenShao missed-event host");guest=Attach("OpenShao missed-event guest");
    guest_joy=SDL_OpenJoystick(guest);Check(host && guest && guest_joy,"attach missed-event handoff controllers");
    SDL_SetJoystickVirtualButton(guest_joy,SDL_GAMEPAD_BUTTON_SOUTH,true);Tick();
    openshao::controller::SetMultiplayer(true);
    SDL_DetachVirtualJoystick(host);host=Attach("OpenShao missed-event host replacement");
    Check(host!=0,"attach host replacement before device reconciliation");
    SDL_PumpEvents();SDL_Event missed{};
    while(SDL_PeepEvents(&missed,1,SDL_GETEVENT,SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)==1) {}
    SDL_Delay(800);Tick();
    expect(openshao::controller::KeyboardPort()==-1 && input::Poll(0).connected &&
           input::Poll(1).analog[0]==255 && !input::Poll(2).connected,
           "device reconciliation releases the stale host slot before assigning its replacement");
    SDL_DetachVirtualJoystick(host);SDL_CloseJoystick(guest_joy);SDL_DetachVirtualJoystick(guest);Tick();
    openshao::controller::Shutdown();
    Check(failures==0,"automatic controller handoff regressions");
}
static void TestMenuControllers() {
    using namespace openshao::recomp::raw;
    unsigned failures=0;
    auto expect=[&](bool ok,const char* text) {
        if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",text);}
    };
    const toml::table automatic;
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    auto first=Attach("OpenShao first menu controller");auto second=Attach("OpenShao second menu controller");
    auto* first_joy=SDL_OpenJoystick(first);auto* second_joy=SDL_OpenJoystick(second);
    Check(first && second && first_joy && second_joy,"attach both menu controllers");
    FocusLost();Active(true);Tick();Navigation();
    SDL_SetJoystickVirtualButton(second_joy,SDL_GAMEPAD_BUTTON_SOUTH,true);Tick();
    expect(Navigation()==0x40 && ControllerButtons()==0x40,
           "second controller can confirm in the custom menu");
    Tick();Tick();
    expect(Navigation()==0,"idle first controller cannot repeat a held second-controller confirm");
    SDL_SetJoystickVirtualButton(first_joy,SDL_GAMEPAD_BUTTON_DPAD_UP,true);Tick();
    expect(Navigation()==0x1000 && ControllerButtons()==0x1040,
           "menu combines buttons from both controllers without losing held state");
    SDL_SetJoystickVirtualButton(first_joy,SDL_GAMEPAD_BUTTON_DPAD_UP,false);Tick();Navigation();
    SDL_SetJoystickVirtualButton(second_joy,SDL_GAMEPAD_BUTTON_SOUTH,false);Tick();Navigation();
    Begin(Device::Controller);Tick();
    SDL_SetJoystickVirtualAxis(second_joy,SDL_GAMEPAD_AXIS_RIGHTX,32767);Tick();
    const auto captured=Take();
    expect(captured.device==Device::Controller && captured.source=="rightx+",
           "controller binding capture accepts the second controller's stick");
    Cancel();SDL_SetJoystickVirtualAxis(second_joy,SDL_GAMEPAD_AXIS_RIGHTX,0);Tick();Navigation();
    SDL_CloseJoystick(first_joy);SDL_DetachVirtualJoystick(first);Tick();
    SDL_SetJoystickVirtualButton(second_joy,SDL_GAMEPAD_BUTTON_EAST,true);Tick();
    expect(Navigation()==0x20 && ControllerButtons()==0x20,
           "remaining controller can go back after the first controller disconnects");
    openshao::controller::SetFocused(false);
    expect(ControllerButtons()==0 && Navigation()==0,"focus loss clears buttons from every menu controller");
    SDL_SetJoystickVirtualButton(second_joy,SDL_GAMEPAD_BUTTON_EAST,false);
    openshao::controller::SetFocused(true);Tick();Navigation();
    auto replacement=Attach("OpenShao replacement menu controller");auto* replacement_joy=SDL_OpenJoystick(replacement);
    Check(replacement && replacement_joy,"attach replacement menu controller");Tick();
    SDL_SetJoystickVirtualButton(replacement_joy,SDL_GAMEPAD_BUTTON_SOUTH,true);Tick();Navigation();
    SDL_CloseJoystick(replacement_joy);SDL_DetachVirtualJoystick(replacement);Tick();
    expect(ControllerButtons()==0 && Navigation()==0,"disconnect clears a held menu button even with another idle controller");
    replacement=Attach("OpenShao neutral menu reconnect");Tick();
    expect(ControllerButtons()==0 && Navigation()==0,"menu reconnection cannot replay the disconnected controller's button");
    SDL_DetachVirtualJoystick(replacement);SDL_CloseJoystick(second_joy);SDL_DetachVirtualJoystick(second);Tick();
    Cancel();Active(false);FocusLost();openshao::controller::Shutdown();
    Check(failures==0,"all-controller menu input regressions");
}
int main() {
    // Override normal backend hints before SDL_Init: hardware cannot be touched.
    for (const char* hint : {SDL_HINT_JOYSTICK_HIDAPI,SDL_HINT_JOYSTICK_WGI,SDL_HINT_XINPUT_ENABLED,
                            SDL_HINT_JOYSTICK_DIRECTINPUT,SDL_HINT_JOYSTICK_RAWINPUT,SDL_HINT_JOYSTICK_GAMEINPUT,
                            SDL_HINT_JOYSTICK_HIDAPI_PS4,SDL_HINT_JOYSTICK_HIDAPI_PS5})
        SDL_SetHintWithPriority(hint,"0",SDL_HINT_OVERRIDE);
    input::Init();
    openshao::controller::SetFocused(true);
    int initial=0; auto* initial_ids=SDL_GetGamepads(&initial); SDL_free(initial_ids);
    Check(initial==0,"virtual tests must not enumerate any physical gamepad");
    auto first=Attach("OpenShao synthetic pad"); Check(first!=0,"attach virtual gamepad");
    auto* joy=SDL_OpenJoystick(first); Check(joy!=nullptr,"open virtual joystick");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTX,900);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-700);
    Tick();
    auto p=input::Poll(0);
    Check(p.connected,"hot-plug connected");
    Check(p.lx==0 && p.ly==0,"stick drift inside default deadzone is neutral");
    Check(p.rx==0 && p.ry==0 && p.analog[6]==0 && p.analog[7]==0,"neutral right stick and triggers");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_SOUTH,true);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,true);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_START,true);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTX,32767);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-32768);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,0);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHTX,-32768);
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHTY,32767);
    Tick(); p=input::Poll(0);
    Check(p.analog[0]==255 && p.analog[4]==255 && p.analog[7]==255,"face, shoulder and trigger mapping");
    Check((p.buttons&0x10)!=0 && p.lx>20000 && p.ly>20000,"START and sticks delivered");
    Check(p.rx<-20000 && p.ry<-20000,"camera stick preserves sign and magnitude");
    Check(p.analog[6]>=126 && p.analog[6]<=128,"trigger retains intermediate pressure");
    input::Rumble(0,12345,23456); Tick();
    Check(low_motor==12345 && high_motor==23456,"rumble has a nonzero duration");
    for (int n=0;n<4;++n) {SDL_Delay(100);Tick();}
    Check(low_motor==12345 && high_motor==23456,"motors refreshed beyond their lease");
    openshao::controller::SetFocused(false); p=input::Poll(0);
    Check(p.connected && !p.buttons && !p.lx && !p.ly && !p.analog[0],"focus loss releases input while keeping connection");
    Tick(); Check(!low_motor && !high_motor,"focus loss stops motors");
    openshao::controller::SetFocused(true); Tick();
    Check(!low_motor && !high_motor,"focus return does not replay stale rumble");
    input::Pad threaded;
    std::thread reader([&]{threaded=input::Poll(0);openshao::controller::Update();});reader.join();
    Check(threaded.analog[0]==255,"game thread reads a snapshot without pumping SDL");
    auto config=toml::parse("[controller]\ndeadzone=0.15\nrumble=false\n[controller.bindings]\na='east'\n");
    openshao::controller::Load(&config); Tick();
    Check(!input::Poll(0).analog[0],"custom mapping changes the controller input");
    Check(openshao::controller::HasRumble(0),"Disabling vibration must not remove the hardware motor capabilities cached by the original game");
    input::Rumble(0,12345,23456);Tick();
    Check(!low_motor && !high_motor,"Disabled vibration must suppress motor output while retaining hardware capabilities");
    openshao::launcher::Settings enabled;enabled.rumble=true;openshao::controller::Configure(enabled);
    Tick();Check(!low_motor && !high_motor,"Reenabling vibration must not replay a motor effect requested while disabled");
    input::Rumble(0,12345,23456);Tick();
    Check(low_motor==12345 && high_motor==23456,"Vibration can be enabled live without reconnecting the controller");
    openshao::controller::Load(&config);Tick();
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,true);Tick();
    Check(input::Poll(0).analog[0]==255,"remapped face button reaches Xbox A");
    toml::table saved;openshao::controller::Save(&saved);
    Check(saved["controller"]["bindings"]["a"].value_or(std::string{})=="b","mapping saved in SDL TOML format");
    Check(saved["controller"].as_table()->size()==3,"controller settings contain only deadzone, rumble, and bindings");
    const auto extended=toml::parse("[controller.bindings]\na='none'\nleft_trigger='a'\ndpad_up='righttrigger'\nmove_left='rightx-'\ncamera_right='lefty-'\ncamera_left='none'\nmove_right='none'\n");
    openshao::controller::Load(&extended);Tick();p=input::Poll(0);
    Check(!p.analog[0] && p.analog[6]==255 && (p.buttons&1) && p.lx< -20000 && p.rx>20000,
          "unbinding, button-to-trigger, trigger-to-dpad and stick direction remaps reach the guest");
    openshao::controller::Load(&config);Tick();
    {
        openshao::launcher::ControllerCapture capture;
        Check(!capture.Start(false),"background controller capture cannot initialize hardware");
        Check(capture.Start(true),"interactive capture initializes against the synthetic backend");
        Check(capture.Poll().empty(),"capture waits for previously held controls to be released");
        for(int button=0;button<SDL_GAMEPAD_BUTTON_COUNT;++button)SDL_SetJoystickVirtualButton(joy,button,false);
        for(int axis=0;axis<SDL_GAMEPAD_AXIS_COUNT;++axis)SDL_SetJoystickVirtualAxis(joy,axis,axis<4?0:-32768);
        Check(capture.Poll().empty(),"neutral input only arms capture");
        SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_WEST,true);
        Check(capture.Poll()=="x" && !capture.Active(),"controller capture returns a newly pressed button and closes its handles");
        SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_WEST,false);
        Check(capture.Start(true),"a second capture can start without restarting SDL");capture.Poll();
        SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHTX,-32768);
        Check(capture.Poll()=="rightx-","controller capture identifies stick direction and sign");
        SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHTX,0);
    }
    auto second=Attach("OpenShao synthetic second pad"); Check(second!=0,"second gamepad");
    Tick(); Check(input::Poll(1).connected,"second player has a separate port");
    SDL_CloseJoystick(joy); Check(SDL_DetachVirtualJoystick(first),"detach first");
    Tick(); Check(!input::Poll(0).connected && input::Poll(1).connected,"disconnect preserves second port");
    auto third=Attach("OpenShao synthetic replacement"); Check(third!=0,"reconnect replacement");
    Tick(); Check(input::Poll(0).connected && input::Poll(1).connected,"reconnect fills free port");
    SDL_DetachVirtualJoystick(second); SDL_DetachVirtualJoystick(third); Tick();
    // Exercise the real HWND timer bridge: Poll must receive the new state
    // without the game thread or test calling Update directly.
    WNDCLASSW wc{};wc.lpfnWndProc=TestWindow;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"OpenShaoControllerTest";
    Check(RegisterClassW(&wc)!=0,"register owned hidden test window");
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP,0,0,16,16,nullptr,nullptr,wc.hInstance,nullptr);
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    Check(window && pid==GetCurrentProcessId() && !IsWindowVisible(window),"hidden timer window belongs to this test");
    openshao::controller::Window(window);
    auto timed=Attach("OpenShao timer pad");auto* timed_joy=SDL_OpenJoystick(timed);
    Check(timed && timed_joy,"attach timer device");
    SDL_SetJoystickVirtualButton(timed_joy,SDL_GAMEPAD_BUTTON_START,true);
    SendMessageW(window,WM_SETFOCUS,0,0);
    for (int n=0;n<30;++n) {
        MSG msg;while(PeekMessageW(&msg,window,0,0,PM_REMOVE)) DispatchMessageW(&msg);
        SDL_Delay(2);
    }
    Check((input::Poll(0).buttons&0x10)!=0,"owned window timer refreshes input without guest SDL calls");
    SendMessageW(window,WM_KILLFOCUS,0,0);
    Check(!input::Poll(0).buttons,"window focus message releases the controller snapshot");
    SDL_CloseJoystick(timed_joy);SDL_DetachVirtualJoystick(timed);
    DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    openshao::controller::Shutdown();
    keyboard_active=true;
    const toml::table automatic;
    openshao::controller::Load(&automatic);
    input::Init();openshao::controller::SetFocused(true);
    auto coop=Attach("OpenShao coop pad");auto* coop_joy=SDL_OpenJoystick(coop);
    Check(coop && coop_joy,"attach co-op gamepad");
    SDL_SetJoystickVirtualButton(coop_joy,SDL_GAMEPAD_BUTTON_EAST,true);
    Tick();
    openshao::controller::SetMultiplayer(true);
    Check(input::Poll(0).connected && input::Poll(1).connected,"keyboard and first gamepad occupy separate player ports");
    openshao::launcher::Settings live;live.deadzone=.21;live.rumble=false;
    openshao::controller::Configure(live);
    Check(openshao::controller::KeyboardPort()==0 && input::Poll(1).analog[1]==255,"Live settings must preserve established Ko-op player ownership");
    intptr_t key_result=0;
    openshao::keyboard::Message(nullptr,WM_KEYDOWN,'W',0,&key_result);
    Check(input::Poll(0).ly==32767 && !input::Poll(1).ly,"keyboard movement reaches only player one");
    Check(!input::Poll(0).analog[1] && input::Poll(1).analog[1]==255,"gamepad attack reaches only player two");
    openshao::keyboard::Message(nullptr,WM_KILLFOCUS,0,0,&key_result);
    SDL_CloseJoystick(coop_joy);SDL_DetachVirtualJoystick(coop);Tick();
    Check(input::Poll(0).connected && !input::Poll(1).connected,"gamepad unplug does not disconnect keyboard player");
    coop=Attach("OpenShao coop replacement");Tick();
    Check(input::Poll(0).connected && input::Poll(1).connected,"gamepad reconnect returns to second player");
    SDL_DetachVirtualJoystick(coop);Tick();openshao::controller::Shutdown();
    openshao::controller::Load(&automatic);
    openshao::options.background=true;openshao::options.virtual_gamepads=1;
    input::Init();openshao::controller::SetFocused(true);Tick();
    openshao::controller::SetMultiplayer(true);
    Check(input::Poll(0).connected && input::Poll(1).connected,"isolated hidden fixture supplies second player");
    Check(!input::Poll(2).connected && !input::Poll(3).connected,"hidden fixture enumerates no physical controllers");
    Check(RegisterClassW(&wc)!=0,"register hidden fixture window");
    window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP,0,0,16,16,nullptr,nullptr,wc.hInstance,nullptr);
    Check(window && !IsWindowVisible(window),"fixture window is hidden");
    openshao::controller::Window(window);
    SendMessageW(window,WM_APP+0x51,SDL_GAMEPAD_BUTTON_START,1);
    SendMessageW(window,WM_APP+0x52,SDL_GAMEPAD_AXIS_LEFTX,32767);
    Check(!input::Poll(0).buttons && !input::Poll(0).lx && (input::Poll(1).buttons&0x10) && input::Poll(1).lx>32000,
          "hidden fixture messages pass through SDL to player two only");
    SendMessageW(window,WM_APP+0x51,SDL_GAMEPAD_BUTTON_START,0);
    SendMessageW(window,WM_APP+0x52,SDL_GAMEPAD_AXIS_LEFTX,0);
    Check(!input::Poll(1).buttons && !input::Poll(1).lx,"fixture releases its buttons and axes");
    SendMessageW(window,WM_APP+0x53,0,0);
    Check(input::Poll(0).connected && !input::Poll(1).connected,"hidden fixture can disconnect before the game enumerates devices");
    SendMessageW(window,WM_APP+0x53,0,1);
    Check(input::Poll(0).connected && input::Poll(1).connected,"hidden fixture can connect after the game is already running");
    DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    openshao::controller::Shutdown();
    // Reproduce launching normally, navigating on keyboard, then connecting a
    // gamepad after selecting Ko-op. No dedicated launch flag is involved.
    openshao::options.background=false;openshao::options.virtual_gamepads=0;
    openshao::controller::Load(&automatic);
    input::Init();openshao::controller::SetFocused(true);
    Check(input::Poll(0).connected && !input::Poll(1).connected,"normal keyboard-only launch has one player");
    openshao::keyboard::Message(nullptr,WM_KEYDOWN,'W',0,&key_result);
    openshao::keyboard::Message(nullptr,WM_KEYUP,'W',0,&key_result);
    openshao::controller::SetMultiplayer(true);
    auto late=Attach("OpenShao late co-op pad");auto* late_joy=SDL_OpenJoystick(late);
    Check(late && late_joy,"connect gamepad after choosing Ko-op");
    SDL_SetJoystickVirtualButton(late_joy,SDL_GAMEPAD_BUTTON_START,true);Tick();
    Check(input::Poll(0).connected && input::Poll(1).connected,"normal launch recognizes late gamepad as second player in Ko-op");
    Check(!input::Poll(0).buttons && (input::Poll(1).buttons&0x10),"late gamepad START reaches only player two");
    Check(!openshao::controller::HasRumble(0) && openshao::controller::HasRumble(1),"co-op motor capabilities follow the real controller port");
    SDL_CloseJoystick(late_joy);SDL_DetachVirtualJoystick(late);Tick();
    Check(input::Poll(0).connected && !input::Poll(1).connected,"late unplug preserves keyboard player");
    late=Attach("OpenShao late co-op reconnect");Tick();
    Check(input::Poll(0).connected && input::Poll(1).connected,"late reconnect restores second player");
    openshao::controller::SetMultiplayer(false);
    Check(input::Poll(0).connected && input::Poll(1).connected && !input::Poll(1).buttons,
          "solo advertises keyboard plus controller to unlock Ko-op while keeping player two neutral");
    SDL_DetachVirtualJoystick(late);Tick();openshao::controller::Shutdown();
    // The input used to navigate the menu must not change the fixed PC rule.
    // Even when START was pressed on the controller, keyboard is P1 in co-op.
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    late=Attach("OpenShao gamepad primary");late_joy=SDL_OpenJoystick(late);
    SDL_SetJoystickVirtualButton(late_joy,SDL_GAMEPAD_BUTTON_START,true);Tick();
    Check((input::Poll(0).buttons&0x10) && !input::Poll(1).buttons,
          "solo gamepad and keyboard share P1 before Ko-op selection");
    openshao::controller::SetMultiplayer(true);
    openshao::keyboard::Message(nullptr,WM_KEYDOWN,'W',0,&key_result);
    Check(!(input::Poll(0).buttons&0x10) && input::Poll(0).ly==32767 &&
          (input::Poll(1).buttons&0x10) && !input::Poll(1).ly,
          "Ko-op always uses keyboard P1 and single gamepad P2, regardless of menu navigation");
    openshao::keyboard::Message(nullptr,WM_KILLFOCUS,0,0,&key_result);
    SDL_CloseJoystick(late_joy);SDL_DetachVirtualJoystick(late);Tick();openshao::controller::Shutdown();
    // Two actual gamepads retain their original logical ports on disconnect.
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    first=Attach("OpenShao automatic first");second=Attach("OpenShao automatic second");Tick();
    openshao::controller::SetMultiplayer(true);
    Check(input::Poll(0).connected && input::Poll(1).connected && !input::Poll(2).connected,
          "two gamepads use P1 and P2 in automatic co-op");
    openshao::keyboard::Message(nullptr,WM_KEYDOWN,'W',0,&key_result);
    Check(!input::Poll(0).ly && !input::Poll(1).ly,
          "keyboard cannot unexpectedly control either player when two gamepads own Ko-op");
    openshao::keyboard::Message(nullptr,WM_KILLFOCUS,0,0,&key_result);
    SDL_DetachVirtualJoystick(second);Tick();
    Check(input::Poll(0).connected && !input::Poll(1).connected,"second gamepad unplug never moves first gamepad to P2");
    second=Attach("OpenShao automatic second reconnect");Tick();
    Check(input::Poll(0).connected && input::Poll(1).connected,"second gamepad reconnect restores its original player");
    SDL_DetachVirtualJoystick(first);SDL_DetachVirtualJoystick(second);Tick();openshao::controller::Shutdown();
    // A keyboard-only solo session must accept a newly connected gamepad on
    // P1 immediately, then give it P2 when the user enters co-op by keyboard.
    keyboard_active=true;
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    auto switched=Attach("OpenShao solo hotplug pad");auto* switched_joy=SDL_OpenJoystick(switched);
    Check(switched && switched_joy,"attach gamepad after keyboard solo startup");
    SDL_SetJoystickVirtualButton(switched_joy,SDL_GAMEPAD_BUTTON_EAST,true);Tick();
    Check(input::Poll(0).analog[1]==255 && !input::Poll(1).analog[1],
          "gamepad connected during keyboard solo immediately controls player one");
    openshao::keyboard::Message(nullptr,WM_KEYDOWN,'W',0,&key_result);
    openshao::keyboard::Message(nullptr,WM_KEYUP,'W',0,&key_result);
    openshao::controller::SetMultiplayer(true);
    Check(!input::Poll(0).analog[1] && input::Poll(1).analog[1]==255,
          "keyboard-selected Ko-op moves the live controller to player two");
    SDL_CloseJoystick(switched_joy);SDL_DetachVirtualJoystick(switched);Tick();
    openshao::controller::Shutdown();
    // Simulate a driver that changes its device list while its SDL add/remove
    // notifications are consumed by another component of the event loop.
    keyboard_active=false;
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    auto unnoticed=Attach("OpenShao missed hotplug event");
    SDL_PumpEvents();
    SDL_Event missed{};
    while(SDL_PeepEvents(&missed,1,SDL_GETEVENT,SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)==1) {}
    SDL_Delay(800);Tick();
    Check(input::Poll(0).connected,"device scan detects a gamepad even if its added event was missed");
    SDL_DetachVirtualJoystick(unnoticed);
    SDL_PumpEvents();
    while(SDL_PeepEvents(&missed,1,SDL_GETEVENT,SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)==1) {}
    SDL_Delay(800);Tick();
    Check(!input::Poll(0).connected,"device scan releases a gamepad even if its removed event was missed");
    openshao::controller::Shutdown();
    openshao::bindings::Keyboard custom;for(auto& pair:custom.keys)pair={0,0};
    custom.keys[0]={'Z','V'};custom.keys[1]={VK_RCONTROL,0};custom.keys[16]={'I',0};custom.keys[17]={'M',0};
    custom.keys[22]={'N',0};custom.keys[23]={'O',0};
    openshao::keyboard::Configure(custom);openshao::controller::SetMultiplayer(false);
    openshao::keyboard::Key('Z',true);p={};openshao::keyboard::Poll(0,p);
    Check(p.analog[0]==255,"a remapped primary keyboard key reaches A");
    openshao::keyboard::Key('Z',false);openshao::keyboard::Key('V',true);p={};openshao::keyboard::Poll(0,p);
    Check(p.analog[0]==255,"an alternate keyboard key reaches the same input");
    openshao::keyboard::ReleaseAll();openshao::keyboard::Key(VK_SPACE,true);p={};openshao::keyboard::Poll(0,p);
    Check(!p.analog[0],"a removed default key cannot continue to activate its old input");
    openshao::keyboard::Key('I',true);openshao::keyboard::Key('M',true);openshao::keyboard::Key('N',true);p={};openshao::keyboard::Poll(0,p);
    Check(!p.ly && p.rx==-32768,"opposing movement keys cancel and camera direction keys work");
    openshao::keyboard::ReleaseAll();openshao::keyboard::Key(VK_LCONTROL,true);p={};openshao::keyboard::Poll(0,p);
    Check(!p.analog[1],"left Ctrl cannot activate a right Ctrl binding");
    openshao::keyboard::Key(VK_RCONTROL,true);p={};openshao::keyboard::Poll(0,p);Check(p.analog[1]==255,"right Ctrl is independently bindable");
    openshao::keyboard::Message(nullptr,WM_KILLFOCUS,0,0,&key_result);p={};openshao::keyboard::Poll(0,p);
    Check(!p.analog[1] && !p.rx,"focus loss clears remapped keyboard input");
    openshao::keyboard::Configure({});
    // The opening key belongs to gameplay, but its release can arrive after
    // the custom menu starts consuming keyboard input. It must still release P1.
    keyboard_active=true;
    openshao::controller::Load(&automatic);input::Init();openshao::controller::SetFocused(true);
    for(bool messages:{true,false}) {
        auto key=[&](unsigned code,bool down) {
            if(messages)openshao::keyboard::Message(nullptr,down?WM_KEYDOWN:WM_KEYUP,code,0,&key_result);
            else openshao::keyboard::Key(code,down);
        };
        for(unsigned code:{unsigned(VK_SPACE),unsigned(VK_RETURN),unsigned(VK_ESCAPE),unsigned(VK_UP),unsigned('J')}) {
            openshao::recomp::raw::Active(false);openshao::recomp::raw::FocusLost();
            openshao::keyboard::ReleaseAll();key(code,true);
            p=input::Poll(0);
            Check(p.buttons || p.analog[0] || p.analog[1] || p.analog[2],"opening key reaches P1 before custom menu");
            openshao::recomp::raw::Active(true);
            if(code=='J')openshao::recomp::raw::Begin(openshao::recomp::raw::Device::Keyboard);
            key(code,false);p=input::Poll(0);
            Check(!p.buttons && !p.analog[0] && !p.analog[1] && !p.analog[2],
                  messages?"Win32 key release consumed by custom menu must clear gameplay input":
                           "SDL key release consumed by custom menu must clear gameplay input");
            openshao::recomp::raw::Cancel();openshao::recomp::raw::Active(false);
            key(code,true);p=input::Poll(0);
            Check(p.buttons || p.analog[0] || p.analog[1] || p.analog[2],"same key can activate again after leaving custom menu");
            key(code,false);
        }
        key(VK_SPACE,true);openshao::recomp::raw::Active(true);key(VK_SPACE,false);
        auto id=Attach("OpenShao keyboard menu handoff");auto* handoff=SDL_OpenJoystick(id);
        Check(id && handoff,"connect controller after keyboard entered custom menu");Tick();
        Check(!input::Poll(0).analog[0],"late controller starts without a keyboard A stuck down");
        for(int button=SDL_GAMEPAD_BUTTON_SOUTH;button<=SDL_GAMEPAD_BUTTON_NORTH;++button) {
            SDL_SetJoystickVirtualButton(handoff,button,true);Tick();
            Check(input::Poll(0).analog[button]==255,"late controller face button reaches player one");
            SDL_SetJoystickVirtualButton(handoff,button,false);Tick();
            Check(!input::Poll(0).analog[button],"late controller face button returns to neutral");
        }
        openshao::recomp::raw::Active(false);
        SDL_CloseJoystick(handoff);SDL_DetachVirtualJoystick(id);Tick();
    }
    openshao::recomp::raw::FocusLost();openshao::controller::Shutdown();
    TestControllerHandoff();
    TestMenuControllers();
    Check(openshao::bindings::VirtualKey(SDL_SCANCODE_Z)=='Z' && openshao::bindings::VirtualKey(SDL_SCANCODE_RCTRL)==VK_RCONTROL &&
          openshao::bindings::VirtualKey(SDL_SCANCODE_KP_7)==0x67,"SDL keyboard translation includes remapped letters, modifiers and numpad");
    std::puts("PASS: virtual controller mapping, deadzone, rumble and independent hot-plug ports");
}
