#include "survival_session.h"
#include "survival_mode.h"
#include "survival_levels.h"
#include "survival_unlock.h"
#include "difficulty.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <array>
#include <bit>
#include <cstring>

namespace openshao::survival {
namespace {
constexpr uint32_t MessageManager=0x4E6E20, CurrentMessage=MessageManager+0x6C0;
constexpr uint32_t MessageTemplate=0x4E6A98, WorldPaused=0x5AE5B0;
enum class RulesStage { None, Pending, Shown, Fading, Done };
RulesStage rules=RulesStage::None;
constexpr unsigned IntroFadeTicks=36;
unsigned intro_fade_tick=0;
bool intro_pause_owned=false;
bool intro_pause_release_pending=false;
uint32_t pending_music=0;
bool active=false,challenge_played=false,completion_shown=false;
uint32_t level_one_voice=0,announcement_handle=0,announcement_sound=0;
// Level announced this load (0 = none), keyed by banner text: the script may post it before the room variable changes.
unsigned banner_level=0;
bool emitting_banner=false;
uint32_t upgraded_world=0,upgraded_room=0,upgraded_state=0;
bool upgraded=false,upgrade_pending=false,repeated_load_pending=false;
constexpr uint32_t Heroes=0x5CE880,HeroStride=0x3238;
constexpr uint32_t FatalityLevel=0x4928,FatalityPlayerStride=0x50;
// Story health pickups: native base-health row at state+0xE0 (0..4), separate from the debug health tiers.
constexpr uint32_t CampaignHealthPickups=0xE0,MaxCampaignHealthPickups=4;

constexpr bool CampaignSkill(uint32_t skill) {
    // 11/12 are Versus moves; 23..26 are Ko-op-only.
    return skill<=10 || (skill>=13 && skill<=22);
}

void DrainIntroPause() {
    if(!intro_pause_release_pending)return;
    if(!kernel::G<uint8_t>(WorldPaused)){intro_pause_release_pending=false;return;}
    if(kernel::G<uint8_t>(CurrentMessage) || kernel::R32(0x6C92F8) ||
       kernel::R32(0x6C9308))return;
    kernel::G<uint8_t>(WorldPaused)=0;
    intro_pause_release_pending=false;
}
void ReleaseIntroPause() {
    if(intro_pause_owned){intro_pause_owned=false;intro_pause_release_pending=true;}
    DrainIntroPause();
}

bool SameUpgradedHeroes(uint32_t world,uint32_t room,uint32_t state,uint32_t mode,
                        uint32_t first,uint32_t second) {
    if(!upgraded || upgraded_world!=world || upgraded_room!=room || upgraded_state!=state ||
       !state || mode>1 || kernel::R32(state+CampaignHealthPickups)!=MaxCampaignHealthPickups ||
       kernel::R32(state+0x90)!=1)return false;
    const uint32_t expected=kernel::R32(state+0x94)/(mode==1?2u:1u);
    if(!expected || !first || kernel::R32(first+0x370)!=expected ||
       (mode==1 && (!second || kernel::R32(second+0x370)!=expected)))return false;
    for(uint32_t player=0;player<2;++player)
        for(uint32_t skill=0;skill<27;++skill) {
            if(!CampaignSkill(skill))continue;
            if(kernel::G<uint8_t>(state+0x1C4+player*0x1C+skill)!=
               kernel::G<uint8_t>(0x4F3390+skill))return false;
        }
    for(uint32_t player=1;player<=(mode==1?2u:1u);++player)
        if(kernel::R32(state+FatalityLevel+player*FatalityPlayerStride)<3)return false;
    return true;
}

void UpgradePlayers(X86Context& ctx,unsigned level) {
    if(!active || !level)return;
    const auto world=kernel::R32(0x639224),state=kernel::R32(0x5ACEBC);
    if(!world || !state)return;
    const auto mode=kernel::R32(state+0x88);
    if(mode>1)return;
    const auto room=kernel::R32(0x505970);
    const auto first=kernel::R32(Heroes+HeroStride+0x200);
    const auto second=kernel::R32(Heroes+2*HeroStride+0x200);
    // Ko-op P2 may get its health component a frame after room-ready.
    if(!first || !kernel::R32(first+0x370) ||
       (mode==1 && (!second || !kernel::R32(second+0x370))))return;
    if(SameUpgradedHeroes(world,room,state,mode,first,second)){
        upgrade_pending=false;
        return;
    }

    // Story specials and kombos only; Versus and Ko-op-only ranks stay native.
    const auto saved=ctx;
    for(uint32_t player=0;player<2;++player){
        for(uint32_t skill=0;skill<27;++skill){
            if(!CampaignSkill(skill))continue;
            ctx.eax=skill;
            kernel::CallGuest(ctx,0xB06F0,{player,kernel::G<uint8_t>(0x4F3390+skill),0});
            ctx=saved;
        }
    }
    // Unlock all three Fatality levels; 0x14903 sets the level without charging the meter.
    for(uint32_t player=1;player<=(mode==1?2u:1u);++player){
        if(kernel::R32(state+FatalityLevel+player*FatalityPlayerStride)>=3)continue;
        kernel::CallGuest(ctx,0x14903,{Heroes+player*HeroStride,3});
        ctx=saved;
    }
    // Max story health: last native pickup row, base HP via 0x11636, then refill at tier 1 (keeps the native bar).
    kernel::W32(state+CampaignHealthPickups,MaxCampaignHealthPickups);
    // 0x11636 thunk: its target 0xD4760 cannot be called indirectly in this port.
    kernel::CallGuest(ctx,0x11636,{});
    ctx=saved;
    difficulty::SetDebugRefill(true);
    kernel::CallGuest(ctx,0x12D65,{1});
    difficulty::SetDebugRefill(false);
    ctx=saved;
    upgraded=true;upgrade_pending=false;
    upgraded_world=world;upgraded_room=room;upgraded_state=state;
    LOGI("Survival level %u: campaign skills, three Fatality levels and all native story health pickups applied",level);
}

constexpr unsigned BannerText=155;
unsigned BannerLevel(uint32_t type,uint32_t text){
    return type==2 && text>BannerText && text<=BannerText+9?text-BannerText:0;
}
bool BannerShowing(unsigned level){
    return kernel::G<uint8_t>(CurrentMessage) &&
        BannerLevel(kernel::R32(CurrentMessage+0x14),kernel::R32(CurrentMessage+0x18))==level;
}

uint32_t Call(X86Context& ctx,uint32_t fn,std::initializer_list<uint32_t> args) {
    const auto saved=ctx;
    ctx.ecx=fn==0x152360 || fn==0x152380?0x653C64:0x653BC0;
    const auto result=kernel::CallGuest(ctx,fn,args);
    ctx=saved;
    return result;
}

uint32_t Play(X86Context& ctx,uint32_t sound) {
    const auto handle=Call(ctx,0x151560,{sound,0x3F800000,0,0,0,0xBF800000,0,0});
    LOGI("Survival voice %08X handle %08X",sound,handle);
    return handle;
}

void Announcement(X86Context& ctx,uint32_t sound) {
    StopAnnouncement(ctx);
    announcement_sound=sound;
    announcement_handle=Play(ctx,sound);
}

enum class Frame{NativeTemplate,Centered,Story};
void Message(X86Context& ctx,uint32_t type,uint32_t text,Frame frame) {
    const bool native_template=frame==Frame::NativeTemplate;
    const auto saved=ctx;
    ctx.esp-=64;
    const auto info=ctx.esp;
    std::array<uint8_t,64> previous{};
    std::memcpy(previous.data(),kernel::HostPtr(info),previous.size());
    if(native_template)std::memset(kernel::HostPtr(info),0,previous.size());
    else {
        // Template 0: native framed, A-confirmed message (border, fade, footer).
        std::memcpy(kernel::HostPtr(info),kernel::HostPtr(MessageTemplate),previous.size());
        kernel::G<uint8_t>(info)=1;
        // Story frames keep the template anchor (320,370); only the rules are centered.
        if(frame==Frame::Centered){kernel::W32(info+0xC,320);kernel::W32(info+0x10,240);}
        kernel::G<uint8_t>(info+0x28)=1;
        kernel::G<uint8_t>(info+0x29)=1;
        kernel::G<uint8_t>(info+0x2A)=1;
    }
    kernel::W32(info+0x14,type);
    kernel::W32(info+0x18,text);
    ctx.ecx=MessageManager;
    kernel::CallGuest(ctx,0x135A3,{info,uint32_t(native_template)});
    std::memcpy(kernel::HostPtr(info),previous.data(),previous.size());
    ctx=saved;
}

void LevelBanner(X86Context& ctx,unsigned level) {
    if(banner_level==level)return;
    banner_level=level;
    if(BannerShowing(level))return;
    emitting_banner=true;
    Message(ctx,2,BannerText+level,Frame::NativeTemplate);
    emitting_banner=false;
}
// Room 64 script (PWF 2192, CRC-32 names): the statues enable _d_exit02, the portal's button trigger (same name in every arena).
constexpr uint32_t PortalRoom=64,ExitButtonTrigger=0xBF0B9329;
bool portal_release_pending=false;
void FoundryPortal(X86Context& ctx);
void PortalReleaseFrame(X86Context& ctx) {
    if(!portal_release_pending)return;
    if(active || kernel::R32(0x505970)!=PortalRoom){portal_release_pending=false;return;}
    if(kernel::G<uint8_t>(CurrentMessage) || kernel::G<uint8_t>(WorldPaused) ||
       kernel::R32(0x6C92F8) || kernel::R32(0x6C9308) || !kernel::R32(0x639224))return;
    portal_release_pending=false;
    FoundryPortal(ctx);
}
void FoundryPortal(X86Context& ctx) {
    // Repeated portal requests before the frame pauses the world are ignored.
    if(kernel::G<uint8_t>(CurrentMessage))return;
    const auto encounter=unlock::Discover();
    const uint32_t text=encounter==unlock::Encounter::New?PortalNewLabel:
        encounter==unlock::Encounter::PendingSave?PortalPendingLabel:PortalSavedLabel;
    // Host text ID, resolved by 0x16992.
    Message(ctx,0,text,Frame::Story);
    LOGI("Foundry Survival portal: %s",encounter==unlock::Encounter::New?"unlocked pending save":
        encounter==unlock::Encounter::PendingSave?"already unlocked, save pending":"already saved globally");
}

void ShowRules(X86Context& ctx) {
    const auto paused_before=kernel::G<uint8_t>(WorldPaused);
    rules=RulesStage::Shown;
    Message(ctx,0,RulesLabel,Frame::Centered);
    kernel::W32(MessageManager+0x68C,0x40);
    // Pause the world now so the first tick cannot attack or advance waves.
    kernel::G<uint8_t>(WorldPaused)=1;
    intro_pause_owned|=!paused_before;
    LOGI("Survival rules pause before native frame: %u",unsigned(paused_before));
    AnnounceRules(ctx);
    LOGI("Survival rules ready: waiting for native A confirmation");
}

// Ninth level: framed congratulations over a paused world, then the main menu (16BD6).
void ShowCompletion(X86Context& ctx) {
    StopAnnouncement(ctx);
    const auto paused_before=kernel::G<uint8_t>(WorldPaused);
    Message(ctx,0,CompleteLabel,Frame::Story);
    kernel::W32(MessageManager+0x68C,0x40);
    kernel::G<uint8_t>(WorldPaused)=1;
    intro_pause_owned|=!paused_before;
    completion_shown=true;
    LOGI("Survival session completed: nine levels, waiting for A");
}
bool Queued(uint32_t text) {
    // 15-slot message queue; the exit's one-tick "^ Interact" prompt can hold the head.
    for(uint32_t slot=0;slot<15;++slot){
        const auto at=CurrentMessage+slot*0x40;
        if(!kernel::G<uint8_t>(at))break;
        if(kernel::R32(at+0x18)==text)return true;
    }
    return false;
}
bool UpdateCompletion(X86Context& ctx) {
    if(!completion_shown)return false;
    if(Queued(CompleteLabel)){
        kernel::G<uint8_t>(WorldPaused)=1;
        return true;
    }
    ResetSession();
    Call(ctx,0x16BD6,{1});
    LOGI("Survival completion confirmed: returning to the main menu");
    return true;
}

void Start(X86Context& ctx) {
    StopAnnouncement(ctx);
    ReleaseIntroPause();
    if(!Starting()){ResetSession();return;}
    completion_shown=false;
    active=true;challenge_played=false;level_one_voice=0;
    intro_fade_tick=0;pending_music=0;
    upgraded=upgrade_pending=repeated_load_pending=false;upgraded_world=upgraded_room=upgraded_state=0;
    rules=RulesStage::Pending;
    kernel::W32(0x505970,RoomForLevel(1));
    kernel::W32(0x5CABAC,RoomForLevel(1));
    Started();
    kernel::W32(0x4EF42C,1);
    LOGI("Survival session started (nine levels)");
}
}

void AnnounceMode(X86Context& ctx){Announcement(ctx,0x6B3A3469);}
void AnnounceRules(X86Context& ctx){Announcement(ctx,0x5AFBC61B);}
void StopAnnouncement(X86Context& ctx) {
    const auto handle=announcement_handle;
    announcement_handle=announcement_sound=0;
    if(handle){Call(ctx,0x152380,{handle});LOGI("Survival announcement stopped: %08X",handle);}
}

void VoicePlayed(uint32_t sound,uint32_t handle) {
    // Sound objects are pooled: ignore a handle reused by another sound.
    if(handle && handle==announcement_handle && sound!=announcement_sound)
        announcement_handle=announcement_sound=0;
    if(active && rules==RulesStage::Done && !challenge_played && handle &&
       kernel::R32(0x505970)==RoomForLevel(1) && sound==0xD5FEA8B2)
        level_one_voice=handle;
}

void UpdateVoice(X86Context& ctx) {
    // Runs outside Survival too, so an abandoned fade can release its pause.
    DrainIntroPause();
    PortalReleaseFrame(ctx);
    if(!active || UpdateCompletion(ctx))return;
    if(upgrade_pending)UpgradePlayers(ctx,LevelForRoom(kernel::R32(0x505970)));
    if(rules==RulesStage::Shown) {
        if(kernel::G<uint8_t>(CurrentMessage) && kernel::R32(CurrentMessage+0x18)==RulesLabel)return;
        StopAnnouncement(ctx);
        rules=RulesStage::Fading;
        intro_fade_tick=0;
        kernel::G<uint8_t>(WorldPaused)=1;
        LOGI("Survival rules dismissed: beginning black fade");
        return;
    }
    if(rules==RulesStage::Fading) {
        kernel::G<uint8_t>(WorldPaused)=1;
        if(++intro_fade_tick<IntroFadeTicks)return;
        rules=RulesStage::Done;
        ReleaseIntroPause();
        if(pending_music) {
            const auto track=pending_music;
            pending_music=0;
            const auto saved=ctx;
            kernel::CallGuest(ctx,0x1698D,{track});
            ctx=saved;
            LOGI("Survival music started after rules fade: %08X",track);
        }
        LevelBanner(ctx,1);
        LOGI("Survival rules confirmed: Level One released");
    }
    if(rules!=RulesStage::Done || challenge_played || !level_one_voice)return;
    if(kernel::R32(0x505970)!=RoomForLevel(1)){level_one_voice=0;return;}
    if(Call(ctx,0x152360,{level_one_voice}))return;
    challenge_played=true;level_one_voice=0;
    Play(ctx,0x396797D8);
}

bool InterceptFoundryPortal(X86Context& ctx,uint32_t destination){
    if(!unlock::FoundrySurvivalPortal(kernel::R32(0x505970),destination,active))return false;
    FoundryPortal(ctx);
    return true;
}
bool ActiveSession(){return active;}
void TriggerActivated(X86Context& ctx,uint32_t target,bool enable){
    // Ninth arena: congratulate as soon as the exit portal opens.
    if(enable && active && !completion_shown && target==ExitButtonTrigger &&
       LevelForRoom(kernel::R32(0x505970))==9)
        ShowCompletion(ctx);
    if(enable && !active && target==ExitButtonTrigger && kernel::R32(0x505970)==PortalRoom &&
       !unlock::Available())
        portal_release_pending=true;
}
bool HoldRulesWorld(){
    return active && kernel::R32(0x505970)==RoomForLevel(1) &&
        (rules==RulesStage::Shown || rules==RulesStage::Fading);
}
bool DeferMusic(uint32_t track){
    if(!active || kernel::R32(0x505970)!=RoomForLevel(1) ||
       (rules!=RulesStage::Pending && rules!=RulesStage::Shown && rules!=RulesStage::Fading))return false;
    pending_music=track;
    return true;
}
uint8_t RulesBackdropOpacity(){
    if(!HoldRulesWorld())return 0;
    if(rules==RulesStage::Shown)return 255;
    return uint8_t(((IntroFadeTicks-intro_fade_tick)*255+IntroFadeTicks/2)/IntroFadeTicks);
}
void DrawRulesBackdrop(X86Context& ctx) {
    if(!HoldRulesWorld())return;
    const auto saved=ctx;
    // Drawn after the world, before screen rectangles, frames and fonts.
    static uint32_t panel=0;
    if(!panel)panel=mem::AllocSystem(44);
    if(!panel)return;
    std::memset(kernel::HostPtr(panel),0,44);
    kernel::G<uint8_t>(panel)=1;
    kernel::G<uint8_t>(panel+1)=1;
    for(const auto [offset,value]:{std::pair{4u,0.f},{8u,640.f},{12u,0.f},{16u,480.f},{36u,.5f}})
        kernel::W32(panel+offset,std::bit_cast<uint32_t>(value));
    const uint32_t color=uint32_t(RulesBackdropOpacity())<<24;
    for(unsigned offset=20;offset<=32;offset+=4)kernel::W32(panel+offset,color);
    kernel::CallGuest(ctx,0x12FCC0,{});
    ctx=saved;
    ctx.ecx=panel;
    kernel::CallGuest(ctx,0x12FDE0,{});
    ctx=saved;
}
void ResetSession() {
    ReleaseIntroPause();
    active=false;challenge_played=false;level_one_voice=0;
    banner_level=0;rules=RulesStage::None;completion_shown=false;
    intro_fade_tick=0;pending_music=0;
    upgraded=upgrade_pending=repeated_load_pending=false;upgraded_world=upgraded_room=upgraded_state=0;
}
void RoomLoading() {
    const auto world=kernel::R32(0x639224),room=kernel::R32(0x505970),state=kernel::R32(0x5ACEBC);
    const auto mode=state?kernel::R32(state+0x88):~0u;
    const auto first=kernel::R32(Heroes+HeroStride+0x200);
    const auto second=kernel::R32(Heroes+2*HeroStride+0x200);
    const bool repeated=active && SameUpgradedHeroes(world,room,state,mode,first,second);
    upgrade_pending=false;repeated_load_pending=repeated;
    if(repeated)return;
    banner_level=0;level_one_voice=0;
    if(rules==RulesStage::Shown || rules==RulesStage::Fading)rules=RulesStage::Pending;
}
void RoomReady(X86Context& ctx) {
    if(!active || !kernel::R32(0x639224))return;
    const auto level=LevelForRoom(kernel::R32(0x505970));
    if(!level)return;
    if(repeated_load_pending){
        repeated_load_pending=false;
        const auto state=kernel::R32(0x5ACEBC),mode=state?kernel::R32(state+0x88):~0u;
        const auto first=kernel::R32(Heroes+HeroStride+0x200);
        const auto second=kernel::R32(Heroes+2*HeroStride+0x200);
        if(!SameUpgradedHeroes(kernel::R32(0x639224),kernel::R32(0x505970),state,mode,first,second)){
            // A real restart reset the room: announce the level again.
            banner_level=0;
        }
    }
    upgrade_pending=true;
    UpgradePlayers(ctx,level);
    kernel::W32(0x4EF42C,1);
    LOGI("Survival level %u ready",level);
    if(level==1 && rules==RulesStage::Pending){ShowRules(ctx);return;}
    if(rules==RulesStage::Shown || rules==RulesStage::Fading)return;
    LevelBanner(ctx,level);
}
bool FilterLevelMessage(uint32_t info) {
    if(!active || !info)return false;
    const auto level=BannerLevel(kernel::R32(info+0x14),kernel::R32(info+0x18));
    if(!level || emitting_banner)return false;
    if(level==1 && (rules==RulesStage::Pending || rules==RulesStage::Shown || rules==RulesStage::Fading))return true;
    // One announcement per load, from the script or our fallback.
    if(banner_level==level || BannerShowing(level)){banner_level=level;return true;}
    banner_level=level;
    return false;
}
}

X86_EXTERN_FUNC(__imp__sub_000122D4);
X86_FUNC(sub_000122D4){__imp__sub_000122D4(ctx,base);openshao::survival::Start(ctx);}
X86_EXTERN_FUNC(__imp__sub_00011DB1);
X86_FUNC(sub_00011DB1){__imp__sub_00011DB1(ctx,base);openshao::survival::Start(ctx);}
X86_EXTERN_FUNC(__imp__sub_000164BF);
X86_FUNC(sub_000164BF){openshao::survival::RoomLoading();__imp__sub_000164BF(ctx,base);openshao::survival::RoomReady(ctx);}
X86_EXTERN_FUNC(__imp__sub_00012883);
X86_FUNC(sub_00012883) {
    const auto level=openshao::survival::ActiveSession()?openshao::survival::LevelForRoom(kernel::R32(0x505970)):0;
    __imp__sub_00012883(ctx,base);
    if(!level || !kernel::G<uint8_t>(0x5ACEDC))return;
    if(level<9){kernel::W32(0x4FC160,openshao::survival::RoomForLevel(level+1));return;}
    kernel::G<uint8_t>(0x5ACEDC)=0;
    if(!openshao::survival::completion_shown)openshao::survival::ShowCompletion(ctx);
}
X86_EXTERN_FUNC(__imp__sub_00014345);
X86_FUNC(sub_00014345) {
    if(openshao::survival::HoldRulesWorld()){kernel::Return(ctx,0,0);return;}
    __imp__sub_00014345(ctx,base);
}
