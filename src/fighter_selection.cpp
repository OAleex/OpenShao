#include "fighter_selection.h"
#include "ed_nexus.h"
#include "difficulty.h"
#include "recomp_input.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <array>

X86_EXTERN_FUNC(__imp__sub_0001313E);
X86_EXTERN_FUNC(__imp__sub_00015C86);
namespace openshao::fighter_selection {
namespace {
constexpr uint32_t Main=0x598430,Popup=0x596810,Characters=0x598DF0;
constexpr uint32_t SavedProgress=0x5A375C,UnlockKeys=0x4E5F98,UnlockMasks=0x4FC348;
constexpr uint32_t SavedProgressWords=13;
constexpr uint32_t VersusRoster=0x598C28,VersusRosterCount=0x598C54;
enum class Phase { Idle, Characters, Difficulty, Starting };
Phase phase=Phase::Idle;
Purpose purpose=Purpose::Survival;
uint32_t players=0,return_row=0;
uint32_t hint=8;
constexpr uint32_t Accept=0x40, Back=0x20, Horizontal=0xA000;
constexpr std::array<const char*,8> Names{
    "Baraka","Kitana","Sub-Zero","Kung Lao","Liu Kang","Scorpion","Johnny Cage","Reptile"};
uint32_t TopMenu(){
    const auto depth=kernel::R32(0x6C9308);
    return depth && depth<32?kernel::R32(0x6C9308+depth*4):0;
}
uint32_t Call(X86Context& ctx,uint32_t object,uint32_t fn,std::initializer_list<uint32_t> args){
    const auto saved=ctx;ctx.ecx=object;
    const auto result=kernel::CallGuest(ctx,fn,args);ctx=saved;return result;
}
uint32_t ConfirmingPort(X86Context& ctx){
    // Save the A owner before the return movie consumes the edge.
    Call(ctx,0x598DF0+0x24,0x1B4E60,{kernel::R32(ctx.esp+8)});
    const auto port=kernel::R32(0x598DF0+0x30);
    if(port<4)return port;
    const auto connected=kernel::G<uint8_t>(0x67C674);
    for(unsigned i=0;i<4;++i)if(connected&(1u<<i))return i;
    // Keyboard-only solo uses input port 0.
    return 0;
}
bool SavedVersusFighter(uint32_t fighter) {
    if(fighter>=8)return false;
    if(ed_nexus::KontentUnlocked())return true;
    // Same saved progress bits as retail Versus (0x4E3D0).
    const uint32_t key=kernel::R32(UnlockKeys+fighter*12);
    if(key>=SavedProgressWords*32)return false;
    return (kernel::R32(SavedProgress+(key>>5)*4)&kernel::R32(UnlockMasks+(key&31)*4))!=0;
}
bool Eligible(uint32_t fighter){
    if(fighter>=8)return false;
    if(purpose==Purpose::Survival || ed_nexus::KontentUnlocked())return SavedVersusFighter(fighter);
    if(fighter==3 || fighter==4)return true;
    // Story completion and the original character cheats set these same flags.
    const bool subzero=kernel::G<uint8_t>(0x5A378D)==1;
    const bool scorpion=kernel::G<uint8_t>(0x5A378E)==1;
    if(fighter==2)return subzero;
    if(fighter==5)return scorpion;
    return fighter<2 ? subzero : scorpion;
}
bool CampaignRoster(){return phase==Phase::Characters && purpose==Purpose::Campaign;}
bool HintRoster(){
    if(phase==Phase::Characters)return purpose==Purpose::Campaign;
    return phase==Phase::Idle && kernel::R32(Main+0x38)==2;
}
bool Playable(uint32_t fighter){
    return CampaignRoster()?Eligible(fighter):fighter==3 || fighter==4 || SavedVersusFighter(fighter);
}
void Sound(X86Context& ctx,uint32_t address){
    if(const auto fn=kernel::R32(address))Call(ctx,0,fn,{});
}
struct ScopedCampaignRoster {
    struct ProgressWord {
        uint32_t changed=0;
        uint32_t original=0;
    };
    std::array<ProgressWord,SavedProgressWords> progress{};

    ScopedCampaignRoster(){
        if(purpose!=Purpose::Campaign || ed_nexus::KontentUnlocked())return;
        // Native mode 2 lays out portraits and cursors from the eligible roster: give it the Story choices first.
        for(uint32_t fighter=0;fighter<8;++fighter){
            const auto key=kernel::R32(UnlockKeys+fighter*12);
            if(key>=SavedProgressWords*32)continue;
            const auto word=key>>5,address=SavedProgress+word*4;
            const auto mask=kernel::R32(UnlockMasks+(key&31)*4);
            const auto saved=kernel::R32(address);
            const auto desired=Eligible(fighter)?saved|mask:saved&~mask;
            if(saved==desired)continue;
            progress[word].changed|=saved^desired;
            progress[word].original|=saved&(saved^desired);
            kernel::W32(address,desired);
        }
    }
    ~ScopedCampaignRoster(){
        // Restore only the changed eligibility bits, including those cleared above.
        for(uint32_t word=0;word<progress.size();++word){
            const auto [changed,original]=progress[word];
            if(!changed)continue;
            const auto address=SavedProgress+word*4;
            kernel::W32(address,(kernel::R32(address)&~changed)|original);
        }
    }
};
void MatchEligibleRoster(){
    const uint32_t count=std::min<uint32_t>(kernel::G<uint8_t>(VersusRosterCount),8);
    uint32_t visible=0;
    for(uint32_t i=0;i<count;++i) {
        const uint8_t fighter=kernel::G<uint8_t>(VersusRoster+i);
        if(Eligible(fighter))kernel::G<uint8_t>(VersusRoster+visible++)=fighter;
    }
    kernel::G<uint8_t>(VersusRosterCount)=uint8_t(visible);
    for(uint32_t fighter=0;fighter<8;++fighter)
        if(!Eligible(fighter))kernel::W32(Characters+0x48+fighter*12,0);
}
void NativeAccept(X86Context& ctx,uint8_t* base,unsigned row){
    const auto original=ctx;
    const auto held_at=kernel::R32(ctx.esp+4),pressed_at=kernel::R32(ctx.esp+8);
    const auto held=kernel::R32(held_at),pressed=kernel::R32(pressed_at);
    ctx.ecx=Main;kernel::W32(Main+0x38,row);
    kernel::W32(held_at,0);kernel::W32(pressed_at,0x40);
    {
        ScopedCampaignRoster roster;
        __imp__sub_00015C86(ctx,base);
    }
    if(row==2)MatchEligibleRoster();
    kernel::W32(held_at,held);kernel::W32(pressed_at,pressed);ctx=original;
}
}
bool CampaignInput(X86Context& ctx,uint8_t* base){
    if(ctx.ecx!=Main || TopMenu()!=Main || phase!=Phase::Idle)return false;
    const auto row=kernel::R32(Main+0x38),scene=kernel::R32(Main+0xC);
    const auto status=scene?kernel::R32(scene+0x2E4):~0u;
    if(row>1 || (status!=0 && status!=2))return false;
    const auto held=kernel::R32(kernel::R32(ctx.esp+4)),pressed=kernel::R32(kernel::R32(ctx.esp+8));
    if((held&0x5000) || !(pressed&0x40))return false;
    if(row==1 && Call(ctx,0,0x1B5070,{})<2)return false;
    Begin(ctx,base,Purpose::Campaign,row,row);
    kernel::Return(ctx,0,2);
    return true;
}
void Begin(X86Context& ctx,uint8_t* base,Purpose next,uint32_t player_mode,uint32_t row){
    const auto confirming_port=ConfirmingPort(ctx);
    purpose=next;players=player_mode;return_row=row;
    recomp::raw::NativeEnterAccept(false);
    phase=Phase::Characters;
    NativeAccept(ctx,base,2);
    kernel::W32(Characters+0x30,confirming_port);
}
bool CharacterSelection(){return phase==Phase::Characters;}
bool SetupPopup(){return phase==Phase::Difficulty;}
bool RosterBackdrop(){return phase==Phase::Difficulty || phase==Phase::Starting;}
bool HintActive(){
    if(hint>=Names.size())return false;
    if(TopMenu()==Popup)return true;
    if(TopMenu()==Characters && kernel::R32(Characters+0x18)==Popup &&
       kernel::R32(Characters+0x1C)==2)return true;
    hint=Names.size();
    return false;
}
bool LockedPortrait(uint32_t slot){return slot<Names.size() && HintRoster() && !Playable(slot);}
bool LockedInput(X86Context& ctx){
    if(ctx.ecx!=Characters || TopMenu()!=Characters || !HintRoster())return false;
    const auto scene=kernel::R32(Characters+0xC);
    if(!scene || !kernel::G<uint8_t>(scene+0x340) || kernel::G<uint8_t>(Characters+0xA8))return false;
    const auto held_at=kernel::R32(ctx.esp+4),pressed_at=kernel::R32(ctx.esp+8);
    if(!held_at || !pressed_at)return false;
    const auto pressed=kernel::R32(pressed_at);
    if(!(pressed&Accept))return false;
    const bool solo=CampaignRoster() && players==0;
    if(!solo)Call(ctx,Characters+0x24,0x1B4E80,{});
    for(unsigned player=0;player<(solo?1u:2u);++player){
        if(kernel::G<uint8_t>(Characters+0x44+player))continue;
        const auto fighter=kernel::R32(Characters+0x3C+player*4);
        if(!LockedPortrait(fighter))continue;
        if(kernel::G<uint8_t>(solo?0x4E5FF0:0x598C58+player*0x1C))continue;
        const auto port=kernel::R32(Characters+0x30+player*4);
        const auto held=solo?kernel::R32(held_at):port<4?Call(ctx,0x67C658,0x15AD10,{port}):0;
        const auto buttons=solo?pressed:port<4?Call(ctx,0x67C658,0x15ABF0,{port}):0;
        if((held&Horizontal) || !(buttons&Accept) || (buttons&0x10))continue;
        Sound(ctx,0x6CA3B8);
        Call(ctx,Characters,0x1B1270,{Popup,2});
        Call(ctx,Popup,0x14F84,{});
        const auto list=kernel::R32(Popup+0x10);
        if(list){
            // The native confirmation supplies the frame only. Its rows and prompt stay hidden.
            kernel::W32(list+0x28,0);
            kernel::W32(list+0x30,0);
        }
        hint=fighter;
        kernel::Return(ctx,0,2);
        return true;
    }
    return false;
}
uint32_t PlayerMode(){return players;}
uint32_t MainClip(uint32_t clip){
    if(phase==Phase::Idle || purpose!=Purpose::Campaign)return clip;
    // The shared roster uses mode 2; Main's highlight and transition clips still follow the Solo or Ko-op entry.
    if(clip==3)return return_row+1;
    if(clip==11 || clip==12)return clip-4+return_row*2;
    return clip;
}
bool StartingFor(Purpose requested){return phase==Phase::Starting && purpose==requested;}
bool PopupInput(X86Context& ctx,uint8_t* base){
    if(ctx.ecx==Popup && HintActive()){
        const auto pressed_at=kernel::R32(ctx.esp+8);
        const auto pressed=pressed_at?kernel::R32(pressed_at):0;
        if(pressed&Back){
            Sound(ctx,0x6CA3BC);
            Call(ctx,Popup,0x1B1270,{Characters,1});
        }else{
            const char* message="fighter.find_coin";
            if(CampaignRoster()){
                if(hint==2)message="fighter.finish_kung_lao";
                else if(hint==5)message="fighter.finish_liu_kang";
                else message=hint<2?"fighter.unlock_sub_zero":"fighter.unlock_scorpion";
            }
            difficulty::DrawFighterHint(ctx,Names[hint],message);
        }
        kernel::Return(ctx,0,2);
        return true;
    }
    if(ctx.ecx!=Popup || phase!=Phase::Difficulty)return false;
    // Accept/Back need the real Solo/Ko-op mode; otherwise keep the Versus portraits.
    const auto pressed_at=kernel::R32(ctx.esp+8);
    const auto pressed=pressed_at?kernel::R32(pressed_at):0;
    if(pressed&0x60)kernel::W32(Main+0x38,PlayerMode());
    difficulty::DrawPopup(ctx,base);
    if(kernel::R32(Popup+0x1C)==3){
        // The roster draws from this mode until the start callback; Solo/Ko-op would show only story fighters.
        phase=Phase::Starting;
        kernel::W32(Main+0x38,2);
    }
    else if(kernel::R32(Popup+0x1C)==1){
        phase=Phase::Characters;
        kernel::W32(Main+0x38,2);
    }else kernel::W32(Main+0x38,2);
    return true;
}
bool ReturnToMain(X86Context&,uint8_t*){
    if(TopMenu()!=Main || phase!=Phase::Characters)return false;
    // The child push may still be queued.
    if(kernel::R32(Main+0x18)==Characters && kernel::R32(Main+0x1C)==2)return true;
    // Wait for Main's return clip from the roster.
    const auto scene=kernel::R32(Main+0xC);
    if(scene && !kernel::G<uint8_t>(scene+0x340))return true;
    phase=Phase::Idle;
    kernel::W32(Main+0x38,return_row);
    return true;
}
void ApplyStartMode(){if(phase==Phase::Starting)kernel::W32(Main+0x38,players);}
void FinishStartup(){phase=Phase::Idle;hint=Names.size();}
}
X86_FUNC(sub_0001313E){
    const auto caller=kernel::R32(ctx.esp);
    const bool shared_difficulty=openshao::fighter_selection::CharacterSelection() && (caller==0x4FEF3 || caller==0x50381);
    if(shared_difficulty)openshao::fighter_selection::phase=openshao::fighter_selection::Phase::Difficulty;
    __imp__sub_0001313E(ctx,base);
    // Keep the Versus portrait view; the popup applies the real mode on Accept or Back.
    if(shared_difficulty)kernel::W32(0x598468,2);
}
