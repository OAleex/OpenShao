#include "survival_mode.h"
#include "campaign_finishers.h"
#include "fighter_selection.h"
#include "survival_session.h"
#include "survival_unlock.h"
#include "launcher.h"
#include "menu_renderer.h"
#include "menu_text.h"
#include "mods/languages.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <array>
#include <span>
#include <algorithm>
#include <cstring>

namespace openshao::survival {
namespace {
constexpr uint32_t Main=0x598430;
constexpr uint32_t TableBytes=68,TextBytes=128,RulesBytes=512;
uint32_t TopMenu(){
    const auto depth=kernel::R32(0x6C9308);
    return depth && depth<32?kernel::R32(0x6C9308+depth*4):0;
}
constexpr std::array<std::string_view,6> Keys{
    "survival.complete","survival.unlock_new","survival.unlock_pending","survival.unlock_saved",
    "campaign.fatality_compartment","campaign.ally_fatality_compartment"};
static_assert(campaign_finishers::AcquiredLabel == PortalSavedLabel + 1);
static_assert(campaign_finishers::AllyAcquiredLabel - LabelBase == Keys.size() - 1);
enum class Page {Idle,Modes,Players};
Page page=Page::Idle,view=Page::Idle;
// Game Mode lists: add a mode here and handle its row in AcceptMode().
enum ModeRow : unsigned {SurvivalRow};
constexpr std::array<std::string_view,1> ModeOptions{"game_mode.survival"};
constexpr std::array<std::string_view,2> PlayerOptions{"main.single_player","main.koop"};
std::span<const std::string_view> Options(Page shown){
    if(shown==Page::Modes)return ModeOptions;
    if(shown==Page::Players)return PlayerOptions;
    return {};
}
bool Locked(Page shown,unsigned row){return shown==Page::Modes && row==SurvivalRow && !unlock::Available();}
bool release=true;
bool direction_held=true;

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
void ShowMenu(X86Context& ctx,Page next,unsigned selected=0){
    page=view=next;release=true;direction_held=true;
    Select(ctx,selected);
    LOGI("Survival menu: %s",next==Page::Modes?"modes":"players");
}
}
void Open(X86Context& ctx,uint8_t*){if(EnsureStorage())ShowMenu(ctx,Page::Modes);}
uint32_t Label(uint32_t id){
    if(!storage && !EnsureStorage())return 0;
    if(id==RulesLabel)return storage+TableBytes+uint32_t(Keys.size())*TextBytes;
    return id>=LabelBase && id<LabelBase+Keys.size()?storage+TableBytes+(id-LabelBase)*TextBytes:0;
}
bool MedallionMenu(){return page==Page::Modes || page==Page::Players;}
bool OverlayShown(){return view==Page::Modes || view==Page::Players;}
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
            StopAnnouncement(ctx);page=Page::Idle;release=true;
            fighter_selection::Begin(ctx,base,fighter_selection::Purpose::Survival,selected,selected);
            kernel::Return(ctx,0,2);
        }else if(selected==SurvivalRow){
            Sound(ctx,0x6CA3B8);AnnounceMode(ctx);ShowMenu(ctx,Page::Players);kernel::Return(ctx,0,2);
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
// This clears only the Survival overlay. The shared selector owns startup state.
void Started(){page=view=Page::Idle;}
bool MainMenu(X86Context& ctx){
    if(view!=Page::Players || !fighter_selection::ReturnToMain(ctx,kernel::g_base))return false;
    if(!fighter_selection::CharacterSelection())ShowMenu(ctx,Page::Players,kernel::R32(Main+0x38));
    return true;
}
}
