#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
struct X86Context {uint32_t esp=0x1100,eax=0,ecx=0;};
namespace openshao::preferences { void Apply(X86Context&, uint8_t*) {} }
namespace openshao::ed_nexus { void FrontendInput(X86Context&, uint8_t*) {} void ResetPlayer2() {} }
namespace openshao::recomp {bool MainInput(X86Context&,uint8_t*){return false;} void RestoreMainSelector() {}}
std::array<uint8_t,4096> bytes{};
uint32_t selected=0,top_menu=0x598430,top_depth=1,rumble_mask=0,load_popup_mode=0;
bool survival_characters=false,survival_popup=false;
uint32_t survival_players=0;
namespace openshao::survival {
bool CharacterSelection(){return survival_characters;}
bool SetupPopup(){return survival_popup;}
uint32_t PlayerMode(){return survival_players;}
}
namespace kernel {
uint32_t R32(uint32_t p) {
    if(p==0x639674)return rumble_mask;
    if(p==0x598468)return selected;
    if(p==0x6C9308)return top_depth;
    if(p==0x6C9308+top_depth*4)return top_menu;
    if(p==0x59684C)return load_popup_mode;
    uint32_t v;std::memcpy(&v,bytes.data()+p-0x1000,4);return v;
}
void W32(uint32_t p,uint32_t v) {if(p==0x639674){rumble_mask=v;return;}std::memcpy(bytes.data()+p-0x1000,&v,4);}
}
std::vector<bool> modes;
bool original_called=false;
bool frontend_poll=false;
uint32_t connected_devices=1;
namespace openshao::controller {
bool vibration=true;
bool RumbleEnabled(){return vibration;}
void SetMultiplayer(bool value) {if(!original_called && !frontend_poll)std::abort();modes.push_back(value);}
}
#define X86_FUNC(name) void name(X86Context& ctx,uint8_t* base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
#define LOG_FIRST(...) ((void)0)
#define LOGI(...) ((void)0)
X86_FUNC(__imp__sub_0001670D) {original_called=true;ctx.eax=kernel::R32(ctx.esp+4)+10;ctx.esp+=4;}
X86_FUNC(__imp__sub_00015C86) {original_called=true;top_menu=kernel::R32(ctx.esp+4);ctx.eax=0xCAFE;ctx.ecx=0xDEADBEEF;ctx.esp+=12;}
X86_FUNC(__imp__sub_001B5070) {ctx.eax=connected_devices;ctx.esp+=4;}
#include "controller_mode.inl"
void Check(bool ok,const char* text) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main() {
    X86Context menu;menu.ecx=0x598430;selected=1;kernel::W32(menu.esp+4,0x598DF0);
    sub_00015C86(menu,nullptr);
    Check(!modes.empty() && modes.back(),"entering original retail Ko-op selector separates players before confirming their characters");
    Check(menu.eax==0xCAFE && menu.ecx==0xDEADBEEF && menu.esp==0x110C,"retail menu hook preserves registers and RET8");
    for (uint32_t selection:{0u,1u,2u}) {
        modes.clear();original_called=false;menu={};menu.ecx=0x598430;selected=selection;
        kernel::W32(menu.esp+4,0x598DF0);sub_00015C86(menu,nullptr);
        Check(modes.size()==1 && modes.back()==(selection!=0),"character selector uses the confirmed main-menu game type");
    }
    modes.clear();menu={};menu.ecx=0x598430;selected=1;kernel::W32(menu.esp+4,0x598430);sub_00015C86(menu,nullptr);
    Check(modes.size()==1 && !modes.back(),"highlighting Ko-op keeps shared P1 input before confirmation");
    modes.clear();menu={};menu.ecx=0x598430;selected=0;kernel::W32(menu.esp+4,0x598430);sub_00015C86(menu,nullptr);
    Check(modes.size()==1 && !modes.back(),"selecting Single Player restores shared input");
    modes.clear();menu={};menu.ecx=0x123456;kernel::W32(menu.esp+4,0x598DF0);sub_00015C86(menu,nullptr);
    Check(modes.empty(),"other menu instances do not change player assignment");
    // The original main-menu callback does not run again after the character
    // panel opens. XInputGetState must still switch to two independent ports.
    frontend_poll=true;
    original_called=false;
    modes.clear();selected=1;top_menu=0x598430;
    SyncRetailFrontendMode();
    Check(modes.size()==1 && !modes.back(),"highlighting Ko-op does not split ports");
    modes.clear();top_menu=0x598DF0;
    SyncRetailFrontendMode();
    Check(modes.size()==1 && modes.back(),"first character-screen state poll separates keyboard P1 and gamepad P2 without menu callback");
    modes.clear();selected=0;SyncRetailFrontendMode();
    Check(modes.size()==1 && !modes.back(),"single-player selector retains shared P1");
    survival_characters=true;selected=2;survival_players=0;
    modes.clear();SyncRetailFrontendMode();
    Check(modes.size()==1 && !modes.back(),"Survival's Versus portrait scene keeps solo input shared");
    survival_players=1;modes.clear();SyncRetailFrontendMode();
    Check(modes.size()==1 && modes.back(),"Survival's Versus portrait scene retains Ko-op controller routing");
    survival_characters=false;survival_popup=true;survival_players=0;
    modes.clear();SyncRetailFrontendMode();
    Check(modes.size()==1 && !modes.back(),"opening Survival's difficulty above the roster keeps solo input shared");
    survival_popup=false;
    for(uint32_t count:{0u,1u,2u,4u}) {
        connected_devices=count;survival_characters=true;survival_players=0;
        top_menu=0x598430;selected=2;X86Context entry;
        kernel::W32(entry.esp,0x48EBD);sub_001B5070(entry,nullptr);
        Check(entry.eax==(count<2?2:count) && entry.esp==0x1104,
              "Survival solo enters the full fighter selector with keyboard alone and preserves the native query ABI");
        for(unsigned scenario=0;scenario<4;++scenario) {
            survival_characters=scenario!=0;survival_players=scenario==1?1:0;
            top_menu=scenario==2?0x598DF0:0x598430;entry={};
            kernel::W32(entry.esp,scenario==3?0x123456:0x48EBD);sub_001B5070(entry,nullptr);
            Check(entry.eax==count,"campaign, Ko-op, character updates and unrelated device queries retain the true device count");
        }
    }
    top_depth=2;top_menu=0x598430;selected=2;survival_characters=true;survival_players=0;
    for(uint32_t count:{0u,1u}) {
        connected_devices=count;X86Context entry;
        kernel::W32(entry.esp,0x48EBD);sub_001B5070(entry,nullptr);
        Check(entry.eax==2 && entry.esp==0x1104,
              "keyboard-only Survival must accept the native two-level main-menu stack during fighter handoff");
    }
    top_depth=1;
    survival_characters=false;top_menu=0x598DF0;selected=0;
    modes.clear();selected=1;top_menu=0x123456;SyncRetailFrontendMode();
    Check(modes.empty(),"other menu stacks do not override routing");

    modes.clear();top_menu=0x596810;load_popup_mode=6;SyncRetailFrontendMode();
    Check(modes.size()==1 && modes.back(),"loading a Ko-op save separates the controller as P2 before its Start prompt is polled");
    modes.clear();load_popup_mode=1;SyncRetailFrontendMode();
    Check(modes.empty(),"the load confirmation after P2 joins must keep the established split");
    modes.clear();top_menu=0x5967C0;SyncRetailFrontendMode();
    Check(modes.size()==1 && !modes.back(),"cancelling Ko-op load returns the controller to shared P1 on the save list");
    for(uint32_t popup_mode:{0u,1u,2u,3u,4u,5u}) {
        modes.clear();top_menu=0x596810;load_popup_mode=popup_mode;SyncRetailFrontendMode();
        Check(modes.empty(),"solo load and unrelated confirmations must not turn on multiplayer");
    }
    rumble_mask=0x80;openshao::controller::vibration=true;SyncRetailFrontendMode();
    Check(rumble_mask==0x8F,"Enabled host vibration must enable the original game's four player vibration gates while preserving unrelated flags");
    openshao::controller::vibration=false;SyncRetailFrontendMode();
    Check(rumble_mask==0x80,"Disabling host vibration must close the original game vibration gates");
    openshao::controller::vibration=true;SyncRetailFrontendMode();
    Check(rumble_mask==0x8F,"Vibration can be reenabled after an in-game setting change without restarting or reconnecting");
    frontend_poll=false;
    for (uint32_t mode:{0u,1u,2u,7u}) {
        modes.clear();original_called=false;X86Context ctx;
        kernel::W32(ctx.esp+4,mode);sub_0001670D(ctx,nullptr);
        Check(original_called,"original mode setter is preserved");
        if (mode<=2) Check(modes.size()==1 && modes.back()==(mode!=0),"actual game mode selects shared or split input");
        else Check(modes.empty(),"unknown mode does not invent an input assignment");
        Check(ctx.eax==mode+10 && ctx.esp==0x1104,"cdecl mode wrapper preserves return value and stack");
    }
    std::puts("PASS: original mode setter, solo/Ko-op/versus routing and preserved ABI");
}
