#include "survival_mode.h"
#include "survival_session.h"
#include "survival_unlock.h"
#include "ed_nexus.h"
#include "difficulty.h"
#include "launcher.h"
#include "menu_renderer.h"
#include "menu_text.h"
#include "recomp_input.h"
#include "mods/languages.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <array>
#include <span>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

X86_EXTERN_FUNC(__imp__sub_0001313E);
X86_EXTERN_FUNC(__imp__sub_00015C86);
namespace openshao::survival {
namespace {
constexpr uint32_t Main=0x598430,Popup=0x596810,Characters=0x598DF0;
constexpr uint32_t SavedProgress=0x5A375C,UnlockKeys=0x4E5F98,UnlockMasks=0x4FC348;
constexpr uint32_t VersusRoster=0x598C28,VersusRosterCount=0x598C54;
constexpr uint32_t TableBytes=68,TextBytes=128,RulesBytes=512;
uint32_t TopMenu(){
    const auto depth=kernel::R32(0x6C9308);
    return depth && depth<32?kernel::R32(0x6C9308+depth*4):0;
}
constexpr std::array<std::string_view,4> Keys{
    "survival.complete","survival.unlock_new","survival.unlock_pending","survival.unlock_saved"};
static_assert(PortalSavedLabel-LabelBase==Keys.size()-1);
enum class Page {Idle,Modes,Online,Players,Characters,Difficulty,Starting};
Page page=Page::Idle,view=Page::Idle;
// Game Mode lists: add a mode here and handle its row in AcceptMode().
enum ModeRow : unsigned {SurvivalRow,OnlineRow};
constexpr std::array<std::string_view,2> ModeOptions{"game_mode.survival","game_mode.online"};
// Online modes reuse the main menu's labels so each name matches its offline mode.
enum OnlineModeRow : unsigned {OnlineKoopRow,OnlineVersusRow,OnlineSurvivalRow};
constexpr std::array<std::string_view,3> OnlineOptions{"main.koop","main.versus","game_mode.survival"};
constexpr std::array<std::string_view,2> PlayerOptions{"main.single_player","main.koop"};
std::span<const std::string_view> Options(Page shown){
    if(shown==Page::Modes)return ModeOptions;
    if(shown==Page::Online)return OnlineOptions;
    if(shown==Page::Players)return PlayerOptions;
    return {};
}
// Online modes are always fully unlocked for balance, so only offline Survival can be locked.
bool Locked(Page shown,unsigned row){return shown==Page::Modes && row==SurvivalRow && !unlock::Available();}
bool release=true;
bool direction_held=true;
unsigned players=0;
uint32_t confirming_port=4;

uint32_t storage=0;
std::string_view Text(std::string_view key){return menu::Catalog::Builtin().Get(launcher::Current().language,key);}
bool EnsureStorage(){
    if(!storage)storage=mem::AllocSystem(TableBytes+uint32_t(Keys.size())*TextBytes+RulesBytes);
    if(!storage)return false;
    const bool pt=mods::SameLanguage(launcher::Current().language,"pt-BR");
    for(unsigned i=0;i<Keys.size();++i){
        const auto encoded=menu::EncodeNativeMultiline(Text(Keys[i]),pt,TextBytes-1);
        const auto at=storage+TableBytes+i*TextBytes;
        std::memset(kernel::HostPtr(at),0,TextBytes);
        std::memcpy(kernel::HostPtr(at),encoded.data(),encoded.size());
    }
    const auto encoded=menu::EncodeNative(Text("survival.rules_1"),pt)+"\n"+
        menu::EncodeNative(Text("survival.rules_2"),pt);
    const auto at=storage+TableBytes+uint32_t(Keys.size())*TextBytes;
    std::memset(kernel::HostPtr(at),0,RulesBytes);
    std::memcpy(kernel::HostPtr(at),encoded.data(),encoded.size());
    return true;
}
uint32_t Call(X86Context& ctx,uint32_t object,uint32_t fn,std::initializer_list<uint32_t> args){
    const auto saved=ctx;ctx.ecx=object;
    const auto result=kernel::CallGuest(ctx,fn,args);ctx=saved;return result;
}
void Sound(X86Context& ctx,uint32_t slot){if(const auto fn=kernel::R32(slot))Call(ctx,0,fn,{});}
void Clip(X86Context& ctx,uint32_t object,unsigned clip){
    const auto scratch=ctx.esp-128,old=kernel::R32(scratch);
    kernel::W32(scratch,clip);Call(ctx,object,0x1B1240,{scratch});kernel::W32(scratch,old);
}
void Select(X86Context& ctx,unsigned row){
    kernel::W32(Main+0x38,row);Clip(ctx,Main,std::min(row+1,6u));
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
    if(key>=0x34*8)return false;
    return (kernel::R32(SavedProgress+(key>>5)*4)&kernel::R32(UnlockMasks+(key&31)*4))!=0;
}
void MatchSavedVersusRoster(){
    const uint32_t count=std::min<uint32_t>(kernel::G<uint8_t>(VersusRosterCount),8);
    uint32_t visible=0;
    for(uint32_t i=0;i<count;++i) {
        const uint8_t fighter=kernel::G<uint8_t>(VersusRoster+i);
        if(SavedVersusFighter(fighter))kernel::G<uint8_t>(VersusRoster+visible++)=fighter;
    }
    kernel::G<uint8_t>(VersusRosterCount)=uint8_t(visible);
    for(uint32_t fighter=0;fighter<8;++fighter)
        if(!SavedVersusFighter(fighter))kernel::W32(Characters+0x48+fighter*12,0);
}
void NativeAccept(X86Context& ctx,uint8_t* base,unsigned row){
    const auto original=ctx;
    const auto held_at=kernel::R32(ctx.esp+4),pressed_at=kernel::R32(ctx.esp+8);
    const auto held=kernel::R32(held_at),pressed=kernel::R32(pressed_at);
    ctx.ecx=Main;kernel::W32(Main+0x38,row);
    kernel::W32(held_at,0);kernel::W32(pressed_at,0x40);
    __imp__sub_00015C86(ctx,base);
    if(row==2)MatchSavedVersusRoster();
    kernel::W32(held_at,held);kernel::W32(pressed_at,pressed);ctx=original;
}
void ShowMenu(X86Context& ctx,Page next,unsigned selected=0){
    page=view=next;release=true;direction_held=true;
    Select(ctx,selected);
    LOGI("Game Mode menu: %s",next==Page::Modes?"modes":next==Page::Online?"online":"players");
}
}
void Open(X86Context& ctx,uint8_t*){if(EnsureStorage())ShowMenu(ctx,Page::Modes);}
uint32_t Label(uint32_t id){
    if(!storage && !EnsureStorage())return 0;
    if(id==RulesLabel)return storage+TableBytes+uint32_t(Keys.size())*TextBytes;
    return id>=LabelBase && id<LabelBase+Keys.size()?storage+TableBytes+(id-LabelBase)*TextBytes:0;
}
bool MedallionMenu(){return page==Page::Modes || page==Page::Online || page==Page::Players;}
bool OverlayShown(){return view==Page::Modes || view==Page::Online || view==Page::Players;}
// Emblem shortened 28 canvas pixels per row, as in Konfigure Recompilation.
float OrnamentExtension(){
    const auto rows=Options(view).size();
    return rows?-184.f+28.f*float(rows):0.f;
}
bool Transitioning(){
    if(!MedallionMenu())return false;
    const auto scene=kernel::R32(Main+0xC);
    return TopMenu()!=Main || !scene || !kernel::G<uint8_t>(scene+0x340);
}
void DrawMenu(X86Context& ctx,unsigned opacity){
    if(!OverlayShown() || TopMenu()!=Main)return;
    // Options are Main's own rows, so they keep its native animation and sparkle.
    const auto rows=Options(view);
    for(unsigned i=0;i<rows.size();++i)
        menu::OverrideMainRow(i,Text(rows[i]),Locked(view,i)?menu::MainIdleAlpha:menu::MainSelectedAlpha);
    opacity=std::min(opacity,255u);
    if(opacity && Locked(view,kernel::R32(Main+0x38)))
        menu::NativeLabel(ctx,55.f,453.f,Text("survival.locked_hint"),.72f,(254*opacity+127)/255,false,true,550);
}
bool HideMainCaption(std::string_view id){
    if(!OverlayShown() || TopMenu()!=Main || !id.starts_with("main."))return false;
    constexpr std::array<std::string_view,6> MainRows{"main.single_player","main.koop","main.versus",
        "main.load_game","main.options","main.kontent"};
    const auto rows=Options(view).size();
    for(unsigned i=0;i<rows && i<MainRows.size();++i)if(id==MainRows[i])return false;
    return true;
}
bool MenuInput(X86Context& ctx,uint8_t* base,bool keyboard_enter){
    if(ctx.ecx!=Main || TopMenu()!=Main || !MedallionMenu())return false;
    const auto held=kernel::R32(kernel::R32(ctx.esp+4)),pressed=kernel::R32(kernel::R32(ctx.esp+8));
    const auto selected=kernel::R32(Main+0x38);
    // Replaces Main's "two controllers required" notice (text 0xDE, drawn by 0x48EB0).
    if(page==Page::Players && selected==1 && Call(ctx,0,0x1B5070,{})<2){
        const auto text=Call(ctx,0,0x16992,{0xDE});
        menu::MainControllerNotice(ctx,text,0x3C,.8f);
    }
    if(!(pressed&0x60))release=false;
    if(!(held&0x5000))direction_held=false;
    if(Transitioning()){kernel::Return(ctx,0,2);return true;}
    if(!release && (pressed&0x20)){
        StopAnnouncement(ctx);
        if(page==Page::Players){Sound(ctx,0x6CA3BC);ShowMenu(ctx,Page::Modes,SurvivalRow);kernel::Return(ctx,0,2);}
        else if(page==Page::Online){Sound(ctx,0x6CA3BC);ShowMenu(ctx,Page::Modes,OnlineRow);kernel::Return(ctx,0,2);}
        else {page=view=Page::Idle;Sound(ctx,0x6CA3BC);Select(ctx,menu::MainGameModeRow);kernel::Return(ctx,0,2);}
        return true;
    }
    if((!release || keyboard_enter) && ((pressed&0x40) || keyboard_enter)){
        if(Locked(page,selected)){
            Sound(ctx,0x6CA3BC);release=true;kernel::Return(ctx,0,2);return true;
        }
        if(page==Page::Players){
            if(selected==1 && Call(ctx,0,0x1B5070,{})<2){
                Sound(ctx,0x6CA3BC);release=true;kernel::Return(ctx,0,2);return true;
            }
            confirming_port=ConfirmingPort(ctx);
            players=selected;StopAnnouncement(ctx);recomp::raw::NativeEnterAccept(false);
            page=Page::Characters;release=true;
            NativeAccept(ctx,base,2);
            kernel::W32(0x598DF0+0x30,confirming_port);
            kernel::Return(ctx,0,2);
        }else if(page==Page::Online){
            // Online sessions are not implemented yet; the rows only reserve the menu layout.
            Sound(ctx,0x6CA3BC);release=true;kernel::Return(ctx,0,2);
        }else if(selected==SurvivalRow){
            Sound(ctx,0x6CA3B8);AnnounceMode(ctx);ShowMenu(ctx,Page::Players);kernel::Return(ctx,0,2);
        }else if(selected==OnlineRow){
            Sound(ctx,0x6CA3B8);ShowMenu(ctx,Page::Online);kernel::Return(ctx,0,2);
        }else kernel::Return(ctx,0,2);
        return true;
    }
    const auto count=unsigned(Options(page).size());
    if(count>1 && (held&0x5000) && !direction_held){
        direction_held=true;
        Sound(ctx,0x6CA3C0);Select(ctx,(held&0x1000)?(selected+count-1)%count:(selected+1)%count);
    }
    kernel::Return(ctx,0,2);return true;
}
bool PopupInput(X86Context& ctx,uint8_t* base){
    if(ctx.ecx!=Popup)return false;
    if(page==Page::Difficulty){
        // Accept/Back need the real Solo/Ko-op mode; otherwise keep the Versus portraits.
        const auto pressed_at=kernel::R32(ctx.esp+8);
        const auto pressed=pressed_at?kernel::R32(pressed_at):0;
        if(pressed&0x60)kernel::W32(Main+0x38,PlayerMode());
        difficulty::DrawPopup(ctx,base);
        if(kernel::R32(Popup+0x1C)==3)page=Page::Starting;
        else if(kernel::R32(Popup+0x1C)==1){
            page=Page::Characters;
            kernel::W32(Main+0x38,2);
        }else kernel::W32(Main+0x38,2);
        return true;
    }
    return false;
}
bool Starting(){return page==Page::Starting;}
void Started(){page=view=Page::Idle;}
bool CharacterSelection(){return page==Page::Characters;}
uint32_t PlayerMode(){return players;}
bool SetupPopup(){return page==Page::Difficulty;}
bool MainMenu(X86Context& ctx){
    if(TopMenu()!=Main)return false;
    if(page==Page::Characters){
        // The child push may still be queued.
        if(kernel::R32(Main+0x18)==0x598DF0 && kernel::R32(Main+0x1C)==2)return true;
        // Wait for Main's return clip from the roster.
        const auto scene=kernel::R32(Main+0xC);
        if(scene && !kernel::G<uint8_t>(scene+0x340))return true;
        ShowMenu(ctx,Page::Players,players);return true;
    }
    return false;
}
}
X86_FUNC(sub_0001313E){
    const auto caller=kernel::R32(ctx.esp);
    const bool survival_difficulty=openshao::survival::CharacterSelection() && (caller==0x4FEF3 || caller==0x50381);
    if(survival_difficulty)openshao::survival::page=openshao::survival::Page::Difficulty;
    __imp__sub_0001313E(ctx,base);
    // Keep the Versus portrait view; the popup applies the real mode on Accept or Back.
    if(survival_difficulty)kernel::W32(0x598468,2);
}
