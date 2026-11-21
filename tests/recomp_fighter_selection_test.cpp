#include "fighter_selection.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
constexpr uint32_t Main=0x598430,Characters=0x598DF0,Popup=0x596810,Scene=0x700000;
constexpr uint32_t Progress=0x5A375C,Keys=0x4E5F98,Masks=0x4FC348,Roster=0x598C28,Count=0x598C54;
std::array<uint8_t,8<<20> memory{};
unsigned failures=0,native_entries=0,versus_rosters=0,connected_sources=1,portrait_refreshes=0;
unsigned native_portrait_mask=0,native_cursor_roster_count=0;
unsigned hint_frames=0;
std::string_view hint_message;
uint32_t pressed_port=0;
uint32_t entry_mode=~0u,logic_mode=~0u,confirming_port=0;
uint32_t native_input_mode=~0u,native_update_mode=~0u,native_navigation_span=~0u;
bool kontent_unlocked=false,write_unrelated_progress=false;
void Check(bool ok,const char* why){if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",why);}}
bool NativeVersusEligible(unsigned fighter){
    const auto key=kernel::R32(Keys+fighter*12);
    return (kernel::R32(Progress+(key>>5)*4)&kernel::R32(Masks+(key&31)*4))!=0;
}
void InitializeNativeRoster(){
    ++versus_rosters;memory[Count]=0;native_portrait_mask=0;
    // 0x4E3D0 builds all eight scenes but navigates only unlocked fighters; later filtering cannot restore heroes.
    for(unsigned i=0;i<8;++i){
        const bool available=NativeVersusEligible(i);
        kernel::W32(Characters+0x48+i*12,available?1:0);
        if(available){
            native_portrait_mask|=1u<<i;
            memory[Roster+memory[Count]++]=uint8_t(i);
        }
    }
    // Native 0x1B6400 lays out both cursors using the count before Main returns.
    native_cursor_roster_count=memory[Count];
    kernel::W32(Characters+0x3C,4);kernel::W32(Characters+0x40,3);
    kernel::W32(Characters+0x48+4*12,2);kernel::W32(Characters+0x48+3*12,3);
}
bool RosterEquals(std::initializer_list<uint8_t> expected){
    return memory[Count]==expected.size() && std::equal(expected.begin(),expected.end(),memory.data()+Roster);
}
X86Context Context(uint32_t object=Main,uint32_t held=0,uint32_t pressed=0x40){
    X86Context ctx{};ctx.ecx=object;ctx.esp=0x8000;ctx.esi=0x1234;ctx.edi=0x4321;
    kernel::W32(ctx.esp,0xDEADBEEF);kernel::W32(ctx.esp+4,0x8100);kernel::W32(ctx.esp+8,0x8104);
    kernel::W32(0x8100,held);kernel::W32(0x8104,pressed);return ctx;
}
void Reset(unsigned mode=0,uint8_t versus=0){
    openshao::fighter_selection::FinishStartup();memory.fill(0);
    native_entries=versus_rosters=portrait_refreshes=0;entry_mode=logic_mode=~0u;
    hint_frames=pressed_port=0;hint_message={};
    kontent_unlocked=write_unrelated_progress=false;connected_sources=1;confirming_port=0;
    for(unsigned bit=0;bit<32;++bit)kernel::W32(Masks+bit*4,1u<<bit);
    for(unsigned fighter=0;fighter<8;++fighter)kernel::W32(Keys+fighter*12,128+fighter);
    kernel::W32(Progress+16,versus);
    kernel::W32(Main+0xC,Scene);memory[Scene+0x340]=1;kernel::W32(Main+0x38,mode);
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,Main);
}
std::array<uint8_t,52> SavedProgress(){std::array<uint8_t,52> result{};std::copy_n(memory.data()+Progress,52,result.begin());return result;}
}
X86_EXTERN_FUNC(sub_0001313E);
X86_EXTERN_FUNC(sub_000127BB);
X86_EXTERN_FUNC(sub_00011703);
X86_EXTERN_FUNC(sub_00015010);
X86_EXTERN_FUNC(sub_000159E3);
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count){
    if(fn==0x1B5070 && count==0){ctx.eax=ctx.ecx=ctx.esp=0xBAD;return connected_sources;}
    if(fn==0x1B4E60){W32(ctx.ecx+0xC,confirming_port);ctx.eax=ctx.ecx=ctx.esp=0xBAD;return 0;}
    if(fn==0x11703){++portrait_refreshes;ctx.eax=ctx.ecx=ctx.esp=0xBAD;return 0;}
    if(fn==0x12E5A){W32(Progress+16,0xFF);return 0;}
    if(fn==0x1B4E80)return 0;
    if(fn==0x15AD10)return 0;
    if(fn==0x15ABF0)return args[0]==pressed_port?R32(0x8104):0;
    if(fn==0x1B1270){
        W32(ctx.ecx+0x18,args[0]);W32(ctx.ecx+0x1C,args[1]);W32(args[0]+0x1C,0);
        return 0;
    }
    if(fn==0x14F84){++hint_frames;W32(Popup+0x10,0x720000);return 0;}
    std::fprintf(stderr,"Unexpected selector call %X\n",fn);std::abort();
}
}
namespace logx {void Write(Level,const char*,...) {}}
namespace openshao::ed_nexus {bool KontentUnlocked(){return kontent_unlocked;}}
namespace openshao::recomp::raw {void NativeEnterAccept(bool) {}}
X86_FUNC(__imp__sub_00015C86){
    ++native_entries;entry_mode=kernel::R32(Main+0x38);
    Check(kernel::R32(0x8100)==0 && kernel::R32(0x8104)==0x40,"full native entry receives only Accept");
    if(entry_mode==2){sub_000127BB(ctx,base);}
    if(write_unrelated_progress)kernel::W32(Progress+16,kernel::R32(Progress+16)|0x80000000u);
    kernel::W32(Main+0x18,Characters);kernel::W32(Main+0x1C,2);kernel::W32(Characters+0x30,~0u);
    ctx.esi=ctx.edi=0xBAD;kernel::Return(ctx,77,2);
}
X86_FUNC(__imp__sub_000127BB){InitializeNativeRoster();kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_00011703){
    for(unsigned slot=0;slot<8;++slot)
        kernel::W32(0x58FB40+slot*0x348,kernel::R32(Characters+0x48+slot*12));
    kernel::Return(ctx,0,0);
}
X86_FUNC(__imp__sub_0001313E){kernel::W32(Main+0x38,0);kernel::W32(Popup+0x1C,0);kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_00015010){
    native_input_mode=kernel::R32(Main+0x38);native_navigation_span=kernel::R32(0x58E4D0);
    if(kernel::R32(0x8100)&0x2000){
        Check(kernel::R32(Characters+0x48+5*12)!=0,"native navigation can reach a locked portrait");
        kernel::W32(Characters+0x48+5*12,2);
        kernel::W32(Characters+0x3C,5);
        auto refresh=ctx;
        sub_00011703(refresh,base);
        kernel::Return(ctx,0x1234,2);
        return;
    }
    kernel::W32(Characters+0x3C,6);kernel::Return(ctx,0x1234,2);
}
X86_FUNC(__imp__sub_000159E3){
    native_update_mode=kernel::R32(Main+0x38);
    kernel::W32(Characters+0x40,7);kernel::Return(ctx,0x5678,0);
}
#define UNUSED_IMPORT(name) X86_FUNC(__imp__##name){kernel::Return(ctx,0,0);}
UNUSED_IMPORT(sub_000132A6) UNUSED_IMPORT(sub_0001498F) UNUSED_IMPORT(sub_000117AD)
UNUSED_IMPORT(sub_00012829) UNUSED_IMPORT(sub_000167DF)
UNUSED_IMPORT(sub_00013B57) UNUSED_IMPORT(sub_0001566E)
UNUSED_IMPORT(sub_00015EF2) UNUSED_IMPORT(sub_00012720) UNUSED_IMPORT(sub_000166DB)
UNUSED_IMPORT(sub_00014291)
#undef UNUSED_IMPORT
namespace openshao::difficulty {
void DrawFighterHint(X86Context&,std::string_view,std::string_view message){hint_message=message;}
void DrawPopup(X86Context& ctx,uint8_t*){
    logic_mode=kernel::R32(Main+0x38);
    const auto pressed=kernel::R32(kernel::R32(ctx.esp+8));
    if(pressed&0x40){kernel::W32(Popup+0x1C,3);kernel::W32(Popup+8,logic_mode?0x11DB1:0x122D4);}
    else if(pressed&0x20)kernel::W32(Popup+0x1C,1);
    kernel::Return(ctx,0,2);
}
}
int main(){
    using namespace openshao::fighter_selection;
    Reset();auto ctx=Context();
    Check(MainClip(11)==11,"ordinary Versus keeps its own entry clip");
    Check(CampaignInput(ctx,memory.data()),"original Solo entry is consumed");
    Check(MainClip(3)==1 && MainClip(11)==7 && MainClip(12)==8,
          "campaign Solo keeps its own Main highlight through entry and return");
    Check(native_entries==1 && versus_rosters==1 && entry_mode==2,"all portraits initialize exactly once through native Versus entry");
    Check(RosterEquals({3,4}),"clean campaign retains both monks even with clear Versus bits");
    Check(!StartingFor(Purpose::Survival),"campaign selection cannot activate Survival");

    // This direct boundary regression isolates eligibility from Main interception.
    Reset(0,0xE1);memory[0x5A378D]=1;write_unrelated_progress=true;
    const auto saved=SavedProgress();ctx=Context();Begin(ctx,memory.data(),Purpose::Campaign,0,0);
    Check(RosterEquals({0,1,2,3,4}),"Story Sub-Zero opens Baraka and Kitana alongside the monks");
    Check(native_portrait_mask==0x1F && native_cursor_roster_count==5,
          "native portraits and cursor layout include both left-side extras");
    auto expected=saved;expected[19]|=0x80;
    Check(SavedProgress()==expected,"temporary eligibility restores exact saved bits while preserving unrelated native progress writes");
    Check(versus_rosters==1 && portrait_refreshes==1 && kernel::R32(Characters+0x48+3*12)==1,
          "Solo neutralizes the native P2 highlight during the only initialization");
    Check(kernel::R32(Characters+0x48+2*12)==1 && kernel::R32(Characters+0x48+5*12)==0,
          "Story-only hero has an initialized active slot while locked Scorpion stays disabled");

    Reset(0,1);memory[0x5A378D]=1;ctx=Context();Begin(ctx,memory.data(),Purpose::Survival,0,0);
    Check(RosterEquals({0}),"Survival uniformly follows Versus unlocks despite the Story flag");
    for(unsigned mode:{0u,1u}){
        Reset(mode,0xFF);const auto versus_saved=SavedProgress();ctx=Context();
        Begin(ctx,memory.data(),Purpose::Campaign,mode,mode);
        Check(RosterEquals({3,4}),"Versus coins alone unlock no campaign fighters in Solo or Ko-op");
        Check(native_portrait_mask==0x18 && native_cursor_roster_count==2 && SavedProgress()==versus_saved,
              "campaign initialization retains the monks and restores the original Versus unlocks");
    }
    Reset();memory[0x5A378E]=1;ctx=Context();Begin(ctx,memory.data(),Purpose::Campaign,0,0);
    Check(RosterEquals({3,4,5,6,7}),"Story Scorpion opens Johnny Cage and Reptile without Versus unlocks");
    Reset(1);memory[0x5A378E]=1;ctx=Context();Begin(ctx,memory.data(),Purpose::Campaign,1,1);
    Check(MainClip(3)==2 && MainClip(11)==9 && MainClip(12)==10,
          "campaign Ko-op keeps its own Main highlight through entry and return");
    Check(RosterEquals({3,4,5,6,7}) && native_portrait_mask==0xF8,
          "Ko-op uses the same Scorpion unlock pair and initializes both portraits");
    Reset(1);memory[0x5A378D]=memory[0x5A378E]=1;ctx=Context();Begin(ctx,memory.data(),Purpose::Survival,1,0);
    Check(MainClip(3)==3 && MainClip(11)==11 && MainClip(12)==12,"Survival retains its existing clip mapping");
    Check(RosterEquals({}),"neither story unlock adds portraits in Survival");
    Reset();kontent_unlocked=true;const auto clean=SavedProgress();ctx=Context();Begin(ctx,memory.data(),Purpose::Campaign,0,0);
    Check(RosterEquals({0,1,2,3,4,5,6,7}) && native_portrait_mask==0xFF && native_cursor_roster_count==8 && SavedProgress()==clean,
          "session unlock-all initializes every campaign fighter without saving unlock bits");

    for(unsigned mode:{0u,1u}){
        Reset(mode,0xFF);memory[0x5A378D]=memory[0x5A378E]=1;connected_sources=2;confirming_port=2;
        ctx=Context();Check(CampaignInput(ctx,memory.data()) && PlayerMode()==mode,"accepted original row stores its real campaign mode");
        if(!CharacterSelection())continue;
        Check(ctx.esp==0x800C && ctx.esi==0x1234 && ctx.edi==0x4321 && kernel::R32(0x8104)==0x40,
              "campaign consumption preserves native return ABI and input buffers");
        Check(kernel::R32(Characters+0x30)==2,"campaign entry retains the confirming port");
        ctx=Context();Check(!CampaignInput(ctx,memory.data()) && native_entries==1,"repeated acceptance cannot initialize another roster");
        Check(kernel::R32(Characters+0x48+3*12)==(mode?3:1),"only Solo suppresses the native second highlight");
        kernel::W32(0x6C9308,2);kernel::W32(0x6C9310,Characters);kernel::W32(0x58E4D0,1);
        const auto native_progress=SavedProgress();
        ctx=Context(Characters);sub_00015010(ctx,memory.data());
        Check(native_input_mode==mode && native_navigation_span==(mode?1u:3u) &&
              kernel::R32(0x58E4D0)==1 && kernel::R32(Main+0x38)==2 &&
              kernel::R32(Characters+0x3C)==6 && ctx.eax==0x1234 && ctx.esp==0x800C,
              "native fighter input receives real campaign mode and retains its selection result and return ABI");
        ctx=Context(Characters);sub_000159E3(ctx,memory.data());
        Check(native_update_mode==mode && kernel::R32(Main+0x38)==2 &&
              kernel::R32(Characters+0x40)==7 && ctx.eax==0x5678 && ctx.esp==0x8004 && SavedProgress()==native_progress,
              "native asynchronous selection receives campaign mode and preserves selected state, ABI and saved policy flags");
        kernel::W32(0x6C9308,1);
        kernel::W32(Main+0x18,0);kernel::W32(Main+0x1C,0);memory[Scene+0x340]=0;
        Check(ReturnToMain(ctx,memory.data()) && CharacterSelection(),"campaign Back waits for the native return movie");
        memory[Scene+0x340]=1;
        Check(ReturnToMain(ctx,memory.data()) && !CharacterSelection() && kernel::R32(Main+0x38)==mode,
              "campaign Back restores the original Main row");
        ctx=Context();Check(CampaignInput(ctx,memory.data()),"campaign can reopen after Back");
        for(unsigned attempt=0;attempt<2;++attempt){
            ctx=Context(Popup,0,0);kernel::W32(ctx.esp,mode?0x50381:0x4FEF3);sub_0001313E(ctx,memory.data());
            Check(SetupPopup() && kernel::R32(Main+0x38)==2,"both campaign modes retain all portraits when difficulty opens");
            ctx=Context(Popup,0,0);Check(PopupInput(ctx,memory.data()) && logic_mode==2 && kernel::R32(Main+0x38)==2,
                                      "idle difficulty redraw retains Versus portrait presentation");
            ctx=Context(Popup,0,attempt?0x40:0x20);Check(PopupInput(ctx,memory.data()) && logic_mode==mode,
                                      "difficulty acceptance and Back use the real campaign mode");
            if(!attempt)Check(CharacterSelection() && kernel::R32(Main+0x38)==2,"difficulty Back returns to the full selector");
        }
        Check(StartingFor(Purpose::Campaign) && !StartingFor(Purpose::Survival) &&
              kernel::R32(Popup+8)==(mode?0x11DB1:0x122D4),"campaign difficulty schedules only the original mode's startup");
        Check(RosterBackdrop() && kernel::R32(Main+0x38)==2,
              "after Accept the roster keeps every portrait until the game starts");
        ApplyStartMode();
        Check(kernel::R32(Main+0x38)==mode,"the start callback receives the real Solo or Ko-op mode");
        FinishStartup();
        ApplyStartMode();
        Check(kernel::R32(Main+0x38)==mode,"outside a start the mode is left alone");
    }
    Reset(1);ctx=Context();Check(!CampaignInput(ctx,memory.data()) && native_entries==0 && !CharacterSelection(),
                               "Ko-op with one source defers to the native rejection");
    connected_sources=2;ctx=Context();Check(CampaignInput(ctx,memory.data()) && PlayerMode()==1,"Ko-op enters after a second source appears");
    Reset();connected_sources=0;confirming_port=~0u;ctx=Context();
    Check(CampaignInput(ctx,memory.data()) && PlayerMode()==0 && kernel::R32(Characters+0x30)==0,"keyboard-only Solo uses input port zero");
    for(unsigned row:{2u,3u,4u,5u,9u}){
        Reset(row);ctx=Context();Check(!CampaignInput(ctx,memory.data()) && native_entries==0 && kernel::R32(Main+0x38)==row,
                                     "ordinary Versus, Load and other original rows remain unintercepted");
    }
    Reset(2,0xFF);kernel::W32(0x6C9308,2);kernel::W32(0x6C9310,Characters);kernel::W32(0x58E4D0,1);
    ctx=Context(Characters);sub_00015010(ctx,memory.data());
    Check(native_input_mode==2 && native_navigation_span==1 && ctx.eax==0x1234,"ordinary Versus input retains native mode, navigation and result");
    ctx=Context(Characters);sub_000159E3(ctx,memory.data());
    Check(native_update_mode==2 && ctx.eax==0x5678,"ordinary Versus updates retain their native behavior");
    Reset();ctx=Context(Main,0x4000);Check(!CampaignInput(ctx,memory.data()),"navigation takes precedence over campaign Accept");
    Reset();ctx=Context(Main,0,0);Check(!CampaignInput(ctx,memory.data()),"idle Main does not enter campaign");
    Reset();kernel::W32(Scene+0x2E4,3);ctx=Context();Check(!CampaignInput(ctx,memory.data()),"Main focus animation guards campaign entry");
    Reset();kernel::W32(0x6C930C,Characters);ctx=Context();Check(!CampaignInput(ctx,memory.data()),"background Main cannot accept campaign entry");
    for(unsigned mode:{0u,1u,2u}){
        Reset(mode,0x18);connected_sources=2;ctx=Context();
        if(mode!=2)Check(CampaignInput(ctx,memory.data()),"campaign roster opens");
        kernel::W32(0x6C9308,2);kernel::W32(0x6C9310,Characters);
        kernel::W32(Characters+0xC,Scene);kernel::W32(Characters+0x3C,4);
        kernel::W32(Characters+0x30,0);kernel::W32(Characters+0x34,1);
        ctx=Context(Characters,0x2000,0);sub_00015010(ctx,memory.data());
        Check(hint_frames==0 && kernel::R32(Characters+0x3C)==5,"hover alone opens no hint");
        Check(kernel::R32(0x58FB40+5*0x348)==2 && kernel::R32(0x58FB40+6*0x348)==0,
              "locked hover plays the native focus clip and other locked fighters stay idle");
        if(mode!=0){
            memory[Characters+0x44]=1;kernel::W32(Characters+0x40,5);pressed_port=1;
        }
        const auto saved=SavedProgress();
        ctx=Context(Characters);sub_00015010(ctx,memory.data());
        Check(hint_frames==1 && kernel::R32(Characters+0x18)==Popup,"Accept opens the framed hint for a locked fighter");
        Check(memory[Characters+0x44]==unsigned(mode!=0) && !memory[Characters+0x45] && SavedProgress()==saved,
              "a hint preserves the other player's confirmation and unlock progress");
        if(hint_frames){
            kernel::W32(0x6C9308,3);kernel::W32(0x6C9314,Popup);
            ctx=Context(Popup,0,0x40);
            Check(PopupInput(ctx,memory.data()) && kernel::R32(Popup+0x1C)==0,
                  "the press that opens a hint cannot also close it");
            ctx=Context(Popup,0,0);PopupInput(ctx,memory.data());
            Check(hint_message==(mode==2?"fighter.find_coin":"fighter.finish_liu_kang"),
                  "Scorpion shows the red coin requirement in Versus and Story completion in campaign");
            ctx=Context(Popup,0,0x40);
            Check(PopupInput(ctx,memory.data()) && kernel::R32(Popup+0x1C)==0,
                  "a new Accept press leaves the hint open");
            ctx=Context(Popup,0,0x20);
            Check(PopupInput(ctx,memory.data()) && kernel::R32(Popup+0x18)==Characters,
                  "closing the hint returns to the same selector");
            Check(kernel::R32(Characters+0x3C)==5 && memory[Characters+0x44]==unsigned(mode!=0),
                  "closing the hint preserves cursor and confirmation");
        }
    }
    std::printf("Fighter selection: %u failures\n",failures);return failures?1:0;
}
