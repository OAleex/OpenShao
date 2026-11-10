#include "survival_mode.h"
#include "survival_unlock.h"
#include "survival_session.h"
#include "difficulty.h"
#include "menu_text.h"
#include "launcher.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr uint32_t Main=0x598430,Popup=0x596810,Characters=0x598DF0,RetailKontent=0x598470;
constexpr uint32_t MainScene=0x700000,PopupScene=0x701000,List=0x703000,KontentScene=0x704000;
constexpr uint32_t Progress=0x5A375C,Keys=0x4E5F98,Masks=0x4FC348,Roster=0x598C28,RosterCount=0x598C54;
std::array<uint8_t,8<<20> memory{};
openshao::launcher::Settings settings;
unsigned failures=0,announcements=0,stops=0,native_entries=0,popup_pushes=0,versus_rosters=0,native_mode_entries=0,unexpected_native_entries=0,connected_players=2,controller_notices=0;
int confirming_source=0;
uint32_t difficulty_logic_mode=~0u;
bool globally_unlocked=true,kontent_unlocked=false;
std::vector<std::string> captions;
void Check(bool value,const char* reason){if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",reason);}}
void SavedUnlocks(uint8_t bits){
    for(unsigned bit=0;bit<32;++bit)kernel::W32(Masks+bit*4,1u<<bit);
    for(unsigned fighter=0;fighter<8;++fighter)kernel::W32(Keys+fighter*12,128+fighter);
    kernel::W32(Progress+4*4,uint32_t(bits));
}
bool RosterEquals(std::initializer_list<uint8_t> expected){
    const auto count=memory[RosterCount];
    if(count!=expected.size())return false;
    return std::equal(expected.begin(),expected.end(),memory.data()+Roster);
}
}
X86_EXTERN_FUNC(sub_0001313E);
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count) {
    if(fn==0x1B5070)return connected_players;
    if(fn==0x16992 && count==1 && args[0]==0xDE)return 0x7400;
    if(fn==0x1B4E60){W32(ctx.ecx+0xC,uint32_t(confirming_source));return 0;}
    if(fn==0x1313E){auto nested=ctx;nested.esp-=4;W32(nested.esp,0x12345678);sub_0001313E(nested,g_base);}
    else if((fn==0x1B1240 || fn==0x1B2200) && count==1){
        const auto scene=R32(ctx.ecx+0xC),clip=R32(args[0]);
        W32(scene+0x2A0,clip);W32(scene+0x33C,R32(scene+0x24+clip*16));memory[scene+0x340]=0;
    }
    else if(fn==0x1B1270 && count==2){if(args[0]==Popup)++popup_pushes;W32(ctx.ecx+0x18,args[0]);W32(ctx.ecx+0x1C,args[1]);}
    else if(fn==0x1B0F70 && count==10){W32(List+8,R32(args[0]));W32(List+0xC,0);}
    else if(fn==0x1B5990 && count==4)W32(List+0x18,R32(args[0]));
    else if(fn==0x152DC0 && count==2)return uint32_t(std::strlen(static_cast<const char*>(HostPtr(args[0]))))*10;
    else if(fn==0x1169F && count==3){}
    else if(fn==0x1B2390 || fn==0x1B29B0){}
    else if(fn!=0x100100 && fn!=0x100200 && fn!=0x100300){std::fprintf(stderr,"Unexpected frontend call %X\n",fn);std::abort();}
    ctx.ecx=ctx.esp=ctx.eax=0xBAD;return 0;
}
}
namespace mem {uint32_t AllocSystem(uint32_t,uint32_t){return 0x710000;}}
namespace logx {void Write(Level,const char*,...) {}}
namespace openshao::launcher {const Settings& Current(){return settings;}}
namespace openshao::mods {bool SameLanguage(std::string_view a,std::string_view b){return a==b;}}
namespace openshao::menu {
float NativeLabel(X86Context&,float,float,std::string_view text,float,unsigned,bool,bool,float){captions.emplace_back(text);return 100;}
void TitleWidth(float){}
void OverrideMainRow(unsigned,std::string_view text,unsigned){captions.emplace_back(text);}
void MainControllerNotice(X86Context&,uint32_t text,unsigned,float){if(text==0x7400)++controller_notices;}
}
namespace openshao::survival {
void AnnounceMode(X86Context&){++announcements;}
void AnnounceRules(X86Context&) {Check(false,"rules must be announced inside the arena, never from character confirmation");}
void StopAnnouncement(X86Context&){++stops;}
}
namespace openshao::survival::unlock {bool Available(){return globally_unlocked;}}
namespace openshao::ed_nexus {bool KontentUnlocked(){return kontent_unlocked;}}
X86_FUNC(__imp__sub_0001313E){kernel::W32(0x598468,0);kernel::W32(Popup+0x10,List);kernel::W32(Popup+0x3C,0);kernel::W32(Popup+0x1C,0);kernel::W32(List+8,4);kernel::W32(List+0x18,0x5A7);kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_00015C86){
    if(kernel::R32(kernel::R32(ctx.esp+4))&0x5000){kernel::Return(ctx,0,2);return;}
    if(kernel::R32(kernel::R32(ctx.esp+8))&0x40){
        if(kernel::R32(Main+0x38)==5){++native_mode_entries;kernel::W32(MainScene+0x2A0,17);memory[MainScene+0x340]=0;kernel::W32(Main+0x18,RetailKontent);kernel::W32(Main+0x1C,2);kernel::Return(ctx,0,2);return;}
        ++native_entries;kernel::W32(Characters+0x30,0xFFFFFFFF);
        if(kernel::R32(Main+0x38)==2){
            ++versus_rosters;
            memory[RosterCount]=8;
            for(unsigned i=0;i<8;++i){memory[Roster+i]=uint8_t(i);kernel::W32(Characters+0x48+i*12,1);}
        }else ++unexpected_native_entries;
        kernel::W32(0x594F4C,0);kernel::W32(0x58E4DC,0);
        kernel::W32(0x592D20+0x18,0);
        kernel::W32(Main+0x18,Characters);kernel::W32(Main+0x1C,2);
    }
    kernel::Return(ctx,0,2);
}
namespace openshao::difficulty {
void Select(X86Context&,Mode){Check(false,"difficulty must keep its original input and selection policy");}
void DrawPopup(X86Context& ctx,uint8_t*){
    difficulty_logic_mode=kernel::R32(0x598468);
    const auto pressed=kernel::R32(kernel::R32(ctx.esp+8));
    if(pressed&0x40){kernel::W32(Popup+0x1C,3);kernel::W32(Popup+8,kernel::R32(0x598468)?0x11DB1:0x122D4);}
    else if(pressed&0x20)kernel::W32(Popup+0x1C,1);
    kernel::Return(ctx,0,2);
}
}
static X86Context Context(uint32_t object=Main,uint32_t directions=0,uint32_t pressed=0){
    X86Context ctx{};ctx.ecx=object;ctx.esp=0x8000;ctx.esi=0xABCD;ctx.edi=0xDCBA;
    kernel::W32(ctx.esp,0xDEADBEEF);kernel::W32(ctx.esp+4,0x8100);kernel::W32(ctx.esp+8,0x8104);
    kernel::W32(0x8100,directions);kernel::W32(0x8104,pressed);return ctx;
}
namespace openshao::recomp::raw {void NativeEnterAccept(bool){} bool TakeEnter(){return false;}}
static uint32_t Top(){return kernel::R32(0x6C9308+kernel::R32(0x6C9308)*4);}
static void Input(uint32_t directions=0,uint32_t pressed=0,bool enter=false){
    auto ctx=Context(Top(),directions,pressed);
    if(Top()==Main && openshao::survival::MainMenu(ctx)){kernel::Return(ctx,0,2);}
    else Check(openshao::survival::MenuInput(ctx,memory.data(),enter),"Survival consumes input on its current native page");
    Check(ctx.esp==0x800C && ctx.esi==0xABCD && ctx.edi==0xDCBA,"native menu return ABI is preserved");
    Check(kernel::R32(0x8100)==directions && kernel::R32(0x8104)==pressed,"input pointers are restored");
}
static void Settle(uint32_t directions=0){
    memory[MainScene+0x340]=1;
    if(Top()==Main && kernel::R32(Main+0x1C)==2){
        Check(kernel::R32(Main+0x18)==Characters,"only the native fighter scene can be queued from the Main overlay");
        kernel::W32(0x6C9308,2);kernel::W32(0x6C9310,Characters);kernel::W32(Main+0x1C,0);
    }
    if(openshao::survival::MedallionMenu())Input(directions);
    Check(!openshao::survival::Transitioning(),"native movie completion settles the page");
}
static void Accept(){Input();Input(0,0x40);}
static void Back(){Input();Input(0,0x20);}
static void Graphics(){
    captions.clear();
    auto ctx=Context();openshao::survival::DrawMenu(ctx);
    Check(!openshao::survival::HideMainCaption("main.single_player") &&
          openshao::survival::HideMainCaption("main.versus") &&
          openshao::survival::HideMainCaption("main.kontent") &&
          !openshao::survival::HideMainCaption("kontent.title"),
          "the Survival pages keep Main's first rows for their options and hide the other Main captions");
}
int main(){
    using namespace openshao::survival;
    settings.language="en";settings.improved_difficulty=true;
    SavedUnlocks((1u<<0)|(1u<<3)|(1u<<4)|(1u<<6));
    kernel::W32(RetailKontent+0xC,KontentScene);kernel::W32(Main+0xC,MainScene);kernel::W32(Popup+0xC,PopupScene);memory[PopupScene+0x340]=1;
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,Main);memory[0x67C674]=1;
    kernel::W32(0x6CA3B8,0x100100);kernel::W32(0x6CA3BC,0x100200);kernel::W32(0x6CA3C0,0x100300);
    auto ctx=Context();Open(ctx,memory.data());
    Check(MedallionMenu() && Top()==Main && kernel::R32(0x6C9308)==1 &&
          kernel::R32(Main+0x38)==0 && native_mode_entries==0 && native_entries==0 &&
          kernel::R32(Main+0x18)!=RetailKontent && !SetupPopup(),
          "Game Mode opens the Survival overlay on Main without any Kontent transition or extra menu stack entry");
    ctx=Context(Top());Check(MenuInput(ctx,memory.data(),true) && Transitioning() && announcements==0 &&
          kernel::R32(Main+0x18)!=Characters,"Accept during the Main movie cannot skip directly to the player screen");
    Settle();
    const auto rules=std::string(static_cast<const char*>(kernel::HostPtr(Label(RulesLabel))));
    Check(std::count(rules.begin(),rules.end(),'\n')==1 && rules.find("difficulty")==std::string::npos,
          "Survival rules use only the two native gameplay instructions, without redundant difficulty text");
    Graphics();Check(captions==std::vector<std::string>{"Survival Mode"},"the Modes overlay draws only the Survival option without a redundant heading");
    Check(OrnamentExtension()==-156.f,"a one-row page uses the shortest native emblem");
    Input(0x4000);Settle();
    Check(kernel::R32(Main+0x38)==0 && MedallionMenu(),"a one-row page keeps its focus when Down is pressed");
    Input();
    Accept();Check(MedallionMenu() && Top()==Main && native_entries==0 &&
                   kernel::R32(Main+0x18)!=RetailKontent,"Modes to Players changes only the Main overlay");
    Settle();Graphics();Check(announcements==1 && captions==std::vector<std::string>{"Single Player","Ko-op"},
                            "the Players overlay draws both choices without an extra heading");
    Check(OrnamentExtension()==-128.f,"each additional row lengthens the emblem by 28 canvas pixels");
    Input(0x1000);Settle();
    Check(kernel::R32(Main+0x38)==1,"Up from the first row wraps to the last row");
    Input();Input(0x4000);Settle();
    Check(kernel::R32(Main+0x38)==0,"Down from the last row wraps to the first row");
    Input();
    Back();Settle();Graphics();
    Check(stops>0 && MedallionMenu() && Top()==Main && captions==std::vector<std::string>{"Survival Mode"},
          "Back from Players restores Modes over the same Main background");
    Accept();Settle();Input(0x4000);
    Check(Transitioning(),"the native focus animation blocks another selection until it finishes");
    Settle(0x4000);
    Check(kernel::R32(Main+0x38)==1 && Top()==Main && native_entries==0,
          "holding Down through a focus animation must not immediately bounce Ko-op back to Solo");
    Input();
    kernel::W32(0x594F4C,42);kernel::W32(0x58E4DC,0x1234);kernel::W32(0x592D20+0x18,1);
    connected_players=1;controller_notices=0;Input();
    Check(controller_notices==1,"focused Ko-op with one controller shows Main's two-controller notice");
    Accept();
    Check(MedallionMenu() && !Transitioning() && Top()==Main && !CharacterSelection() && native_entries==0,
          "Ko-op without two connected players stays on the same Main overlay");
    connected_players=2;controller_notices=0;Input();
    Check(controller_notices==0,"two controllers hide the Ko-op notice");
    Input(0x4000,0x40);
    Check(CharacterSelection() && !MedallionMenu() && native_entries==1 && versus_rosters==1 && !popup_pushes && native_mode_entries==0 && unexpected_native_entries==0,
          "only confirming a player invokes the full native Versus roster callback");
    Check(kernel::R32(0x8100)==0x4000 && kernel::R32(0x8104)==0x40,
          "native character entry restores both original held directions and the Accept input buffer");
    Check(OverlayShown() && HideMainCaption("main.versus") && OrnamentExtension()==-128.f,
          "Main's exit movie keeps the compact Players overlay instead of the full main list and emblem");
    captions.clear();ctx=Context(Main);DrawMenu(ctx,128);
    Check(captions==std::vector<std::string>{"Single Player","Ko-op"},
          "the Players options keep Main's native rows while the fighter screen is queued");
    ctx=Context(Main,0x4000);Check(MainMenu(ctx) && kernel::R32(Main+0x18)==Characters && kernel::R32(Main+0x1C)==2,
        "the native Main-to-fighters transition cannot accidentally reopen Players while queued");
    Check(kernel::R32(Characters+0x30)==0,"delayed native character entry retains the real confirming input source instead of -1");
    Check(kernel::R32(0x598468)==2 && PlayerMode()==1 && RosterEquals({0,3,4,6}),
          "Survival offers exactly the fighters unlocked by the saved Versus progress, not all eight");
    Check(kernel::R32(Characters+0x48+2*12)==0 && kernel::R32(Characters+0x48+5*12)==0 &&
          kernel::R32(Characters+0x48+4*12)==1,
          "locked fighters cannot retain native highlights while unlocked portraits remain available");
    Check(!kernel::R32(0x594F4C) && !kernel::R32(0x58E4DC) && !kernel::R32(0x592D20+0x18),"native character entry releases frontend state and hides the previous selector");
    Settle();Check(Top()==Characters && kernel::R32(0x6C9308)==2,"the first native stack transition happens only at fighter selection");
    captions.clear();ctx=Context(Characters);DrawMenu(ctx);
    Check(captions.empty() && !HideMainCaption("main.single_player"),
          "Survival's Main-only captions cannot leak onto the native Versus fighter screen");
    ctx=Context();Check(!MainMenu(ctx) && CharacterSelection(),
        "a background main-menu callback must never mistake character selection for Back");
    kernel::W32(0x6C9308,1);kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);
    memory[MainScene+0x340]=0;
    ctx=Context(Main);Check(MainMenu(ctx) && !MedallionMenu() && CharacterSelection(),
                            "Main finishes its native return clip from the roster before Players takes focus");
    memory[MainScene+0x340]=1;
    ctx=Context(Main);Check(MainMenu(ctx) && MedallionMenu() && Top()==Main && kernel::R32(Main+0x38)==1,
                            "cancelling the native fighter roster returns to Players over the Main background");
    kontent_unlocked=true;
    Settle();Accept();Settle();Check(CharacterSelection() && native_entries==2 && RosterEquals({0,1,2,3,4,5,6,7}),
          "with the debug menu's unlock-all Kontent, reentering the roster offers every Versus fighter like Versus");
    kontent_unlocked=false;
    ctx=Context(Popup);kernel::W32(ctx.esp,0x50381);sub_0001313E(ctx,memory.data());
    Check(!CharacterSelection() && SetupPopup() && kernel::R32(0x598468)==2 && kernel::R32(List+8)==4 && !Starting(),
          "native difficulty builder restores the Versus portrait mode immediately even after its own Solo/Ko-op write");
    ctx=Context(Popup);Check(PopupInput(ctx,memory.data()) && kernel::R32(0x598468)==2 && difficulty_logic_mode==2,
          "idle difficulty redraw keeps the Versus background mode for all eight portraits");
    ctx=Context(Popup,0x4000);Check(PopupInput(ctx,memory.data()) && kernel::R32(0x598468)==2 && difficulty_logic_mode==2,
          "difficulty row navigation cannot switch native portrait modes on successive frames");
    ctx=Context(Popup,0,0x20);Check(PopupInput(ctx,memory.data()) && CharacterSelection() && kernel::R32(0x598468)==2 && difficulty_logic_mode==1,
        "native difficulty Back restores the full Versus portrait scene while retaining the chosen Ko-op session");
    ctx=Context(Popup);kernel::W32(ctx.esp,0x50381);sub_0001313E(ctx,memory.data());
    Check(kernel::R32(0x598468)==2 && SetupPopup(),
          "native difficulty reentry immediately restores all Versus portraits before any popup input tick");
    ctx=Context(Popup,0,0x40);Check(PopupInput(ctx,memory.data()) && Starting() && kernel::R32(Popup+8)==0x11DB1,"native difficulty Accept schedules Ko-op directly, with no frontend rules popup");
    Check(kernel::R32(0x598468)==1 && difficulty_logic_mode==1,
          "only beginning the session restores the real Ko-op gameplay mode");
    Started();Check(!Starting() && !MedallionMenu(),"startup releases the frontend view");
    kernel::W32(0x6C9308,1);kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);
    ctx=Context();Open(ctx,memory.data());Settle();Back();
    Check(!MedallionMenu() && Top()==Main && kernel::R32(0x598468)==9 && native_mode_entries==0 &&
          !HideMainCaption("main.koop"),"Back from Modes restores the ordinary Main list without a Kontent return movie");
    captions.clear();ctx=Context(Main);DrawMenu(ctx);
    Check(captions.empty(),"returning from the overlay must not leave custom captions over the regular Main list");
    ctx=Context();Open(ctx,memory.data());Settle();
    ctx=Context(Top());Check(MenuInput(ctx,memory.data(),true) && MedallionMenu() && Top()==Main,
                           "keyboard Enter confirms Survival Mode without a Kontent transition");
    Settle();confirming_source=-1;ctx=Context(Top());Check(MenuInput(ctx,memory.data(),true) && CharacterSelection() && native_entries==3,
                                                         "keyboard Enter confirms Single Player through the full native Versus roster entry");
    Check(kernel::R32(Characters+0x30)==0,"synthetic Enter without a native A edge chooses an actually connected player source");
    Check(kernel::R32(0x8104)==0,"synthetic menu A must not modify the native input buffer after the callback");
    Started();kernel::W32(0x67C674,0);kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);
    ctx=Context();Open(ctx,memory.data());Settle();Accept();Settle();
    confirming_source=-1;Accept();
    Check(CharacterSelection() && kernel::R32(Characters+0x30)==0 && native_entries==4 && native_mode_entries==0,
          "keyboard-only Solo selects P1 even when the retail physical-controller mask is empty");
    Started();
    // A different save can unlock the entire retail Versus roster. The
    // Survival filter must follow those bits rather than freeze a subset.
    SavedUnlocks(0xFF);
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,Main);
    kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);memory[0x67C674]=1;
    ctx=Context();Open(ctx,memory.data());Settle();Accept();Settle();Accept();Settle();
    Check(CharacterSelection() && RosterEquals({0,1,2,3,4,5,6,7}) && kernel::R32(0x598468)==2,
          "a fully unlocked save keeps every native Versus fighter selectable in Survival");
    Started();
    SavedUnlocks((1u<<2)|(1u<<3)|(1u<<4));
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,Main);
    kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);
    ctx=Context();Open(ctx,memory.data());Settle();Accept();Settle();Accept();Settle();
    Check(CharacterSelection() && RosterEquals({2,3,4}),
          "changing to a partially unlocked save removes fighter options from the next Survival entry");
    Started();kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,Main);
    kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);
    globally_unlocked=false;
    const auto old_announcements=announcements,old_entries=native_entries;
    ctx=Context();Open(ctx,memory.data());Settle();
    captions.clear();ctx=Context(Main);DrawMenu(ctx);
    Check(captions.size()==2 && captions[0]=="Survival Mode" &&
          captions[1]=="Complete the secret challenge in Story Mode to unlock Survival Mode.",
          "locked Survival option shows the localized bottom description without a modal");
    Accept();
    Check(MedallionMenu() && !CharacterSelection() && Top()==Main &&
          announcements==old_announcements && native_entries==old_entries,
          "accepting locked Survival never enters Players or the Versus character roster");
    globally_unlocked=true;Input();Accept();Settle();
    Check(announcements==old_announcements+1 && MedallionMenu(),
          "saving a global unlock makes the same menu entry available immediately");
    std::printf("Survival frontend: %u failures\n",failures);return failures?1:0;
}
