#include "recomp_input.h"
#include <iostream>
int main() {
    using namespace openshao::recomp::raw;
    using namespace std::chrono_literals;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    Active(true);Key(13,true);Begin(Device::Keyboard);Key('Z',true);
    check(Take().device==Device::None,"The opening key must not leak into capture");
    Key('Z',false);Key(13,false);Key('Z',true);
    auto captured=Take();check(captured.device==Device::Keyboard && captured.key=='Z',"Capture must accept a previously unmapped key after release");
    check(Navigation()==0,"Captured input must not select another menu action");
    Begin(Device::Controller);Controller("a",0x40);
    check(Take().device==Device::None,"Controller capture must wait for neutral");
    Controller("",0);Controller("righty-",0);
    captured=Take();check(captured.source=="righty-","Raw capture must retain analog direction");
    const auto start=Clock::now();
    Begin(Device::Controller);Controller("",0,start);Controller("b",0x20,start);
    check(Take().device==Device::None && Capturing()==Device::Controller,
          "Pressing B must wait to distinguish an assignment from a hold to cancel");
    Controller("",0,start+130ms);
    check(Take().source=="b","A short B press must assign B on release");
    Begin(Device::Controller);Controller("b",0x20,start);
    Controller("b",0x20,start+700ms);
    check(!Take().cancelled && Capturing()==Device::Controller,"A B held before capture must wait for neutral");
    Controller("",0,start+710ms);
    Controller("b",0x20,start+720ms);
    Controller("a",0x60,start+1219ms);
    check(!Take().cancelled && Capturing()==Device::Controller,"A pending B must keep waiting until the hold threshold");
    Controller("a",0x60,start+1220ms);
    captured=Take();
    check(captured.cancelled && captured.device==Device::None && ControllerCancelHeld(),"Holding B cancels without emitting a binding");
    Controller("b",0x20,start+1250ms);
    Controller("a",0x60,start+1280ms);
    check(Navigation()==0 && !Take().cancelled,"A held cancel must not leak another navigation action or cancel twice");
    Controller("",0,start+1350ms);
    Controller("b",0x20,start+1400ms);
    check(!ControllerCancelHeld() && Navigation()==0x20,"A fresh press after release can navigate Back normally");
    Begin(Device::Controller);Controller("",0,start+1440ms);
    Controller("b",0x20,start+1450ms);
    Controller("",0,start+2050ms);
    check(Take().cancelled && !ControllerCancelHeld(),"A long press released between polls must still cancel");
    Begin(Device::Keyboard);FocusLost();FocusLost();
    check(Capturing()==Device::None && Navigation()==0,"Focus loss must cancel capture and clear pending navigation");
    check(Take().cancelled,"The open settings list must receive the cancellation when focus is lost");
    Active(true);Key(38,true);Key(13,true);
    check(Navigation()==0x1040 && Navigation()==0,"Fixed recovery navigation must be consumed exactly once");
    Begin(Device::Keyboard);Key(38,false);Key(13,false);Key(0x78,true);
    check(Take().device==Device::None,"F9 must remain reserved for the performance overlay");
    Cancel();Active(false);check(!Key('Z',true),"Closed menus must not consume gameplay keys");
    return failures?1:0;
}
