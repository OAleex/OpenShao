#include "survival_session.h"
#include "survival_mode.h"
#include "survival_unlock.h"
#include "difficulty.h"
#include "kernel/kernel.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
constexpr uint32_t Manager=0x4E6E20,Template=0x4E6A98,MessageSlot=Manager+0x6C0;
std::array<uint8_t,8<<20> memory{};
bool starting=false,playing=false,transition=false,native_room_reset=false;
unsigned failures=0,native_starts=0,native_maps=0;
std::vector<unsigned> skill_grants,health_tiers,fatality_upgrades;
std::vector<uint32_t> health_progressions;
bool debug_refill=false;
unsigned portal_encounters=0;
std::vector<uint32_t> sounds,stops,events,music_tracks;
std::vector<uint8_t> backdrop_alphas;
std::vector<std::array<uint32_t,2>> messages;
void Check(bool ok,const char* why){if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",why);}}
X86Context Context(){X86Context ctx{};ctx.esp=0x8000;ctx.esi=0x1234;ctx.edi=0x4321;return ctx;}
std::vector<std::array<uint32_t,2>> frame_positions;
void NativeInfo(uint32_t type,uint32_t text){kernel::W32(0x730000+0x14,type);kernel::W32(0x730000+0x18,text);}
}
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count){
    uint32_t result=0;
    if(fn==0x151560 && count==8){sounds.push_back(args[0]);events.push_back(args[0]);result=0x100+uint32_t(sounds.size());}
    else if(fn==0x152360 && count==1){Check(ctx.ecx==0x653C64,"voice completion uses the native sound-query object");result=playing;}
    else if(fn==0x152380 && count==1){Check(ctx.ecx==0x653C64,"owned announcements stop through the native sound object");stops.push_back(args[0]);events.push_back(0xF0000000|args[0]);}
    else if(fn==0x16BD6 && count==1)W32(0x514738,args[0]);
    else if(fn==0x1698D && count==1)music_tracks.push_back(args[0]);
    else if(fn==0x12FCC0 && count==0){}
    else if(fn==0x12FDE0 && count==0){
        const auto panel=ctx.ecx;
        Check(G<uint8_t>(panel)==1 && G<uint8_t>(panel+1)==1 &&
              R32(panel+4)==0 && R32(panel+8)==0x44200000 &&
              R32(panel+12)==0 && R32(panel+16)==0x43F00000 &&
              R32(panel+36)==0x3F000000 && R32(panel+40)==0,
              "Survival backdrop uses the original full-viewport native rectangle layout");
        const auto color=R32(panel+20);
        for(unsigned offset=24;offset<=32;offset+=4)
            Check(R32(panel+offset)==color,"all four corners share the same black fade opacity");
        Check(!(color&0xFFFFFF),"Survival backdrop preserves pure black RGB");
        backdrop_alphas.push_back(uint8_t(color>>24));
    }
    else if(fn==0xB06F0 && count==3){
        const auto state=R32(0x5ACEBC);
        Check(R32(state+0x88)<2,"native campaign setter must not change the game mode");
        Check(args[0]<2 && (ctx.eax<=10 || (ctx.eax>=13 && ctx.eax<=22)) &&
              args[1]==memory[0x4F3390+ctx.eax] && args[2]==0,
              "only campaign skill ranks use the native setter ABI and tuned maximum");
        memory[state+0x1C4+args[0]*0x1C+ctx.eax]=uint8_t(args[1]);
        skill_grants.push_back(args[0]*32+ctx.eax);
    }
    else if(fn==0x14903 && count==2){
        const auto state=R32(0x5ACEBC),actor=args[0];
        const auto player=R32(actor+0x68);
        Check(player>=1 && player<=2 && actor==0x5CE880+player*0x3238 && args[1]==3,
              "native Fatality level setter targets an active hero with three unlocked levels");
        const auto amount_at=state+0x4904+player*0x50;
        const auto previous_amount=R32(amount_at);
        W32(state+0x4928+player*0x50,args[1]);
        Check(R32(amount_at)==previous_amount,"unlocking Fatality capacity does not charge its meter");
        fatality_upgrades.push_back(player);
    }
    else if(fn==0x11636 && count==0){
        const auto state=R32(0x5ACEBC);
        const auto upgrades=R32(state+0xE0);
        Check(upgrades==4,"the native campaign health calculator sees all four story pickups");
        health_progressions.push_back(upgrades);
        W32(state+0x94,(200+100*upgrades)*(R32(state+0x88)==1?2u:1u));
    }
    else if(fn==0x12D65 && count==1){
        Check(debug_refill,"story-upgraded Survival health bypasses Monk healing reduction");
        health_tiers.push_back(args[0]);
        const auto state=R32(0x5ACEBC);
        const bool coop=R32(state+0x88)==1;
        Check(R32(state+0x88)<2,"native health upgrade restores the actual Solo/Ko-op mode");
        W32(state+0x90,args[0]);
        for(unsigned player=1;player<=(coop?2u:1u);++player){
            const auto component=R32(0x5CE880+player*0x3238+0x200);
            W32(component+0x370,(R32(state+0x94)*args[0])/(coop?2u:1u));
            W32(component+0x368,R32(component+0x370));
        }
    }
    else if(fn==0x1376A && count==0)memory[MessageSlot]=0;
    else if(fn==0x135A3 && count==2) {
        Check(ctx.ecx==Manager,"rules and level banners use the original message manager");
        if(!openshao::survival::FilterLevelMessage(args[0])){
            const auto type=R32(args[0]+0x14),text=R32(args[0]+0x18);
            messages.push_back({type,text});events.push_back(0xE0000000|type);

            if(!type){
                frame_positions.push_back({R32(args[0]+0xC),R32(args[0]+0x10)});
                Check(args[1]==0,"the A-only frame copies an owned native descriptor instead of indexing template text tables");
                std::memcpy(HostPtr(MessageSlot),HostPtr(args[0]),64);
                W32(Manager+0x6A8,1);W32(Manager+0x6AC,30);W32(Manager+0x6B0,0);
            }
        }
    }
    else {std::fprintf(stderr,"Unexpected session call %X\n",fn);std::abort();}
    ctx.esp=ctx.ecx=ctx.eax=0xBAD;return result;
}
}
namespace logx {void Write(Level,const char*,...) {}}
namespace mem {uint32_t AllocSystem(uint32_t size,uint32_t){Check(size==44,"Survival allocates one native rect");return 0x77F000;}}
namespace openshao::difficulty {void SetDebugRefill(bool enabled){debug_refill=enabled;}}
namespace openshao::survival {
bool Starting(){return starting;}
void Started(){starting=false;}
}
namespace openshao::survival::unlock {
bool portal_available=false;
bool Available(){return portal_available;}
Encounter Discover(){return portal_encounters++==0?Encounter::New:portal_encounters==2?Encounter::PendingSave:Encounter::AlreadySaved;}
}
X86_FUNC(__imp__sub_000122D4){++native_starts;kernel::W32(0x505970,96);kernel::W32(0x5CABAC,96);kernel::Return(ctx,77,0);}
X86_FUNC(__imp__sub_00011DB1){__imp__sub_000122D4(ctx,base);}
X86_FUNC(__imp__sub_00012883){++native_maps;if(transition){memory[0x5ACEDC]=1;kernel::W32(0x4FC160,162);}kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_000164BF){
    if(native_room_reset){
        native_room_reset=false;
        const auto state=kernel::R32(0x5ACEBC);
        kernel::W32(state+0x90,1);
        for(unsigned player=0;player<2;++player)
            for(unsigned skill=0;skill<27;++skill)
                memory[state+0x1C4+player*0x1C+skill]=0;
        for(unsigned player=1;player<=2;++player){
            const auto component=kernel::R32(0x5CE880+player*0x3238+0x200);
            if(component)kernel::W32(component+0x370,100);
            kernel::W32(state+0x4928+player*0x50,0);
        }
    }
    kernel::Return(ctx,1,0);
}
X86_FUNC(__imp__sub_00014345){memory[0x5AE5B0]=0;kernel::Return(ctx,0,0);}
X86_EXTERN_FUNC(sub_000122D4);
X86_EXTERN_FUNC(sub_00011DB1);
X86_EXTERN_FUNC(sub_00012883);
X86_EXTERN_FUNC(sub_000164BF);
X86_EXTERN_FUNC(sub_00014345);
int main(){
    using namespace openshao::survival;
    memory[Template]=1;kernel::W32(Template+0xC,320);kernel::W32(Template+0x10,370);
    kernel::W32(Template+0x1C,2);kernel::W32(Template+0x20,0x3F800000);
    memory[Template+0x28]=memory[Template+0x29]=memory[Template+0x2A]=1;
    kernel::W32(Template+0x34,10);kernel::W32(Manager+0x68C,0x40);
    auto ctx=Context();sub_000122D4(ctx,memory.data());
    Check(!ActiveSession() && kernel::R32(0x505970)==96,"story startup retains its original room");
    AnnounceMode(ctx);AnnounceMode(ctx);
    Check(stops==std::vector<uint32_t>{0x101},"reselecting Survival stops its previous recording before replaying");
    StopAnnouncement(ctx);StopAnnouncement(ctx);
    Check(stops==std::vector<uint32_t>{0x101,0x102},"Back cancels the owned announcement once without stopping unrelated audio");
    for(bool koop:{false,true}){
        ResetSession();starting=true;ctx=Context();
        if(koop)sub_00011DB1(ctx,memory.data());else sub_000122D4(ctx,memory.data());
        Check(ActiveSession() && !starting && kernel::R32(0x505970)==161 && kernel::R32(0x5CABAC)==161,
              "either native startup callback starts Survival after the ordinary character/difficulty flow");
        Check(ctx.esp==0x8004 && ctx.eax==77 && ctx.esi==0x1234 && ctx.edi==0x4321,"the native startup return ABI is preserved");
    }
    sounds.clear();stops.clear();events.clear();messages.clear();music_tracks.clear();
    NativeInfo(2,156);
    Check(FilterLevelMessage(0x730000),"the native Level One banner is deferred before the rules are shown");
    VoicePlayed(0xD5FEA8B2,0x200);playing=false;UpdateVoice(ctx);
    Check(sounds.empty(),"an early scripted level cue cannot start the challenge before rules confirmation");
    kernel::W32(0x639224,0x720000);RoomReady(ctx);RoomReady(ctx);
    Check(messages==std::vector<std::array<uint32_t,2>>{{0,RulesLabel}},"initial gameplay displays one native framed rules message before Level One");
    Check(!frame_positions.empty() && frame_positions.back()==std::array<uint32_t,2>{320,240},
          "the Survival rules frame stays centered over its black backdrop");
    Check(memory[MessageSlot+0x29] && memory[MessageSlot+0x2A] && kernel::R32(Manager+0x68C)==0x40,
          "rules use native A confirmation and modal gameplay pause");
    Check(memory[0x5AE5B0]==1,"the world is frozen immediately, before its first combat update");
    Check(HoldRulesWorld() && RulesBackdropOpacity()==255,"the original rules frame keeps an opaque black backdrop while waiting for A");
    DrawRulesBackdrop(ctx);
    Check(backdrop_alphas==std::vector<uint8_t>{255},"the native rules frame is composited over fully opaque black");
    Check(DeferMusic(0x709D5875) && music_tracks.empty(),"Level One music is held until the black fade has ended");
    Check(sounds==std::vector<uint32_t>{0x5AFBC61B},"How Long Will You Survive accompanies the in-game rules frame");
    Check(FilterLevelMessage(0x730000),"Level One remains deferred while the frame waits for A");
    playing=true;UpdateVoice(ctx);
    Check(messages.size()==1 && stops.empty(),"waiting at the rules frame neither starts the fight nor cancels its voice");
    memory[MessageSlot]=0;UpdateVoice(ctx);
    Check(messages==std::vector<std::array<uint32_t,2>>{{0,RulesLabel}} && RulesBackdropOpacity()==255 &&
          memory[0x5AE5B0]==1 && music_tracks.empty(),
          "A dismissal begins the fade with black still opaque and prevents early gameplay, music and Level One banner");
    RoomReady(ctx);
    Check(messages.size()==1,"duplicate room-ready calls during the fade cannot release Level One early");
    auto unpause=Context();sub_00014345(unpause,memory.data());
    Check(memory[0x5AE5B0]==1,"the native unpause thunk also preserves the fading rules pause");
    Check(stops==std::vector<uint32_t>{0x101},"dismissing rules stops their owned voice before the level announcement");
    auto previous_alpha=RulesBackdropOpacity();
    for(unsigned tick=1;tick<=36;++tick){
        UpdateVoice(ctx);
        const auto alpha=RulesBackdropOpacity();
        Check(alpha<=previous_alpha,"the black backdrop fades monotonically toward the arena");
        previous_alpha=alpha;
        if(tick<36){
            DrawRulesBackdrop(ctx);
            Check(backdrop_alphas.back()==alpha,"the visible native rectangle uses the current fade alpha");
            Check(messages.size()==1 && memory[0x5AE5B0]==1 && music_tracks.empty(),
                  "during the fade, the arena, music and original Level One banner stay on hold");
        }
    }
    Check(RulesBackdropOpacity()==0 && !HoldRulesWorld() && memory[0x5AE5B0]==0 &&
          music_tracks==std::vector<uint32_t>{0x709D5875},
          "the fully revealed arena releases gameplay and its music exactly once");
    Check(messages==std::vector<std::array<uint32_t,2>>{{0,RulesLabel},{2,156}},
          "Level One begins only after the black transition completes");
    Check(events==std::vector<uint32_t>{0xE0000000,0x5AFBC61B,0xF0000101,0xE0000002},
          "the native rules frame, rules voice, cancellation and Level One follow the requested order");
    unpause=Context();sub_00014345(unpause,memory.data());
    Check(memory[0x5AE5B0]==0,"native world pause releases normally after the transition");
    Check(FilterLevelMessage(0x730000),"a later authored Level One copy cannot duplicate its announcement");
    VoicePlayed(0xD5FEA8B2,0x201);UpdateVoice(ctx);
    Check(sounds.size()==1,"Survive This waits for the actual Level One recording to finish");
    playing=false;UpdateVoice(ctx);UpdateVoice(ctx);
    Check(sounds==std::vector<uint32_t>{0x5AFBC61B,0x396797D8},"Survive This plays exactly once after Level One");
    RoomLoading();RoomReady(ctx);VoicePlayed(0xD5FEA8B2,0x202);UpdateVoice(ctx);
    Check(messages.size()==3 && messages.back()[0]==2 && sounds.size()==2,
          "retrying Level One repeats its banner without repeating rules or the opening challenge");
    // Level Two: the banner may post while the room variable still names the previous
    // arena; the room-ready fallback must not repeat it (and its announcer voice).
    {
        const auto before=messages.size();
        RoomLoading();NativeInfo(2,157);
        Check(!FilterLevelMessage(0x730000),"the native Level Two banner posted during loading is preserved");
        kernel::W32(0x505970,162);RoomReady(ctx);RoomReady(ctx);
        Check(messages.size()==before,"room-ready does not repeat a Level Two banner the script already posted");
        Check(FilterLevelMessage(0x730000),"a second native Level Two copy in the same load is suppressed");
        RoomLoading();RoomReady(ctx);
        Check(messages.size()==before+1 && messages.back()==std::array<uint32_t,2>{2,157},
              "a genuine Level Two reload announces once through the fallback");
        Check(FilterLevelMessage(0x730000),"the native copy after the fallback banner is suppressed");
        NativeInfo(2,156);kernel::W32(0x505970,161);
    }
    const unsigned rooms[]={161,162,163,164,165,166,187,191};
    const unsigned next[]={162,163,164,165,166,187,191,190};
    transition=true;
    for(unsigned i=0;i<8;++i){
        kernel::W32(0x505970,rooms[i]);memory[0x5ACEDC]=0;ctx=Context();sub_00012883(ctx,memory.data());
        Check(kernel::R32(0x4FC160)==next[i] && memory[0x5ACEDC],"accepted native portals follow the authored level order");
    }
    transition=false;kernel::W32(0x505970,161);memory[0x5ACEDC]=0;kernel::W32(0x4FC160,0xFEED);
    ctx=Context();sub_00012883(ctx,memory.data());
    Check(kernel::R32(0x4FC160)==0xFEED && !memory[0x5ACEDC],"inactive portals cannot advance an arena");
    transition=true;kernel::W32(0x505970,190);memory[0x5ACEDC]=0;kernel::W32(0x514738,0);memory[MessageSlot]=0;memory[0x5AE5B0]=0;
    // A queued frame behind the exit's one-tick prompt still waits for A.
    memory[MessageSlot]=1;kernel::W32(MessageSlot+0x14,4);kernel::W32(MessageSlot+0x18,68);
    auto before=messages.size();ctx=Context();
    TriggerActivated(ctx,0xBF0B9329,true);
    Check(messages.size()==before+1 && messages.back()==std::array<uint32_t,2>{0,CompleteLabel},
          "the ninth arena congratulates as soon as its exit portal opens");
    std::memcpy(kernel::HostPtr(MessageSlot+0x40),kernel::HostPtr(MessageSlot),0x40);
    kernel::W32(MessageSlot+0x14,4);kernel::W32(MessageSlot+0x18,68);
    UpdateVoice(ctx);
    Check(ActiveSession() && kernel::R32(0x514738)==0,"the frame queued behind the Interact prompt keeps the arena waiting");
    memory[MessageSlot+0x40]=0;
    std::memcpy(kernel::HostPtr(MessageSlot),kernel::HostPtr(MessageSlot+0x40),0x40);memory[MessageSlot]=1;kernel::W32(MessageSlot+0x18,CompleteLabel);
    kernel::W32(MessageSlot+0x14,0);
    ctx=Context();sub_00012883(ctx,memory.data());
    Check(!memory[0x5ACEDC] && ActiveSession() && kernel::R32(0x514738)==0 &&
          messages.size()==before+1 && messages.back()==std::array<uint32_t,2>{0,CompleteLabel} &&
          frame_positions.back()==std::array<uint32_t,2>{320,370} && memory[0x5AE5B0]==1,
          "entering the ninth exit stays in the arena under the one framed congratulations, world paused");
    memory[0x5ACEDC]=0;ctx=Context();sub_00012883(ctx,memory.data());
    Check(messages.size()==before+1 && !memory[0x5ACEDC],"repeated ninth exits cannot stack the congratulations or load level ten");
    UpdateVoice(ctx);UpdateVoice(ctx);
    Check(ActiveSession() && kernel::R32(0x514738)==0 && memory[0x5AE5B0]==1,"the arena waits for A on the congratulations");
    memory[MessageSlot]=0;UpdateVoice(ctx);
    Check(!ActiveSession() && kernel::R32(0x514738)==1,"A on the congratulations returns to the main menu");
    memory[0x5AE5B0]=0;
    ResetSession();kernel::W32(0x505970,161);ctx=Context();sub_00012883(ctx,memory.data());
    Check(kernel::R32(0x4FC160)==162,"ordinary map transitions remain native outside Survival");
    // The green portal is in Foundry Map 1 (room 64) and loads Survival room 161; the
    // story stays in the Foundry with first/unsaved/saved repeat messages.
    transition=false;
    for(unsigned visit=0;visit<3;++visit){
        kernel::W32(0x505970,64);memory[MessageSlot]=0;
        const auto before=messages.size();ctx=Context();
        frame_positions.clear();
        Check(InterceptFoundryPortal(ctx,161) && kernel::R32(0x505970)==64 && messages.size()==before+1 &&
              messages.back()==std::array<uint32_t,2>{0,visit==0?PortalNewLabel:
                  visit==1?PortalPendingLabel:PortalSavedLabel},
              "Foundry green portal stays in story and shows correct new/pending/saved native message");
        Check(frame_positions==std::vector<std::array<uint32_t,2>>{{320,370}},
              "the Foundry portal frame keeps the native story-message anchor, lower on screen");
        const auto shown=messages.size();
        Check(InterceptFoundryPortal(ctx,161) && messages.size()==shown,
              "a repeated portal request while the message is open is absorbed without a second message");
    }
    // Breaking the statues enables _d_exit02: the unlock frame follows by itself.
    {
        ResetSession();kernel::W32(0x505970,64);kernel::W32(0x639224,0x720000);
        memory[MessageSlot]=0;memory[0x5AE5B0]=0;kernel::W32(0x6C92F8,0);kernel::W32(0x6C9308,0);
        const auto frames=[&]{unsigned n=0;for(const auto& m:messages)n+=m[0]==0 && m[1]>=PortalNewLabel && m[1]<=PortalSavedLabel;return n;};
        auto before=frames();ctx=Context();
        TriggerActivated(ctx,0x818F7EBC,true);UpdateVoice(ctx);
        Check(frames()==before,"the portal rays alone do not present the unlock");
        memory[MessageSlot]=1;
        TriggerActivated(ctx,0xBF0B9329,true);UpdateVoice(ctx);
        Check(frames()==before,"the release frame waits for the current message to close");
        memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(frames()==before+1,"releasing the Foundry portal shows the unlock frame without any button");
        memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(frames()==before+1,"the release frame is shown once");
        before=frames();TriggerActivated(ctx,0xBF0B9329,false);memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(frames()==before,"deactivating the trigger does nothing");
        unlock::portal_available=true;TriggerActivated(ctx,0xBF0B9329,true);memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(frames()==before,"an already unlocked mode is not announced again on release");
        unlock::portal_available=false;TriggerActivated(ctx,0xBF0B9329,true);kernel::W32(0x505970,65);UpdateVoice(ctx);
        kernel::W32(0x505970,64);memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(frames()==before,"leaving the room cancels a pending release frame");
        memory[MessageSlot]=0;
    }
    memory[MessageSlot]=0;
    Check(!InterceptFoundryPortal(ctx,65) && !InterceptFoundryPortal(ctx,162),
          "ordinary Foundry room changes and other Survival rooms load normally");
    kernel::W32(0x505970,40);
    Check(!InterceptFoundryPortal(ctx,161),"only the Foundry portal can unlock Survival");
    constexpr uint32_t Live=0x720000,First=0x760000,Second=0x761000;
    for(unsigned skill=0;skill<27;++skill)memory[0x4F3390+skill]=uint8_t(1+skill%5);
    for(unsigned coop=0;coop<2;++coop){
        ResetSession();starting=true;ctx=Context();
        kernel::W32(0x5ACEBC,Live);kernel::W32(Live+0x88,coop);
        kernel::W32(Live+0x94,100);kernel::W32(Live+0x90,1);
        for(unsigned player=1;player<=2;++player){
            const auto component=player==1?First:Second;
            kernel::W32(0x5CE880+player*0x3238+0x200,component);
            kernel::W32(0x5CE880+player*0x3238+0x68,player);
            kernel::W32(component+0x368,45);kernel::W32(component+0x370,100);
            kernel::W32(Live+0x4928+player*0x50,1);
            kernel::W32(Live+0x4904+player*0x50,0x42C80000+player);
        }
        skill_grants.clear();health_tiers.clear();fatality_upgrades.clear();health_progressions.clear();
        for(unsigned player=0;player<2;++player){
            for(unsigned skill=0;skill<27;++skill)
                memory[Live+0x1C4+player*0x1C+skill]=0;
            for(unsigned skill=23;skill<27;++skill)
                memory[Live+0x1C4+player*0x1C+skill]=uint8_t(coop?2:0);
        }
        sub_000122D4(ctx,memory.data());kernel::W32(0x505970,162);
        kernel::W32(0x639224,0x790000+coop);
        RoomReady(ctx);
        Check(skill_grants.size()==42 && health_tiers==std::vector<unsigned>{1} &&
              health_progressions==std::vector<uint32_t>{4},
              "both players receive 21 campaign skills and all four native health pickups once per arena");
        Check(fatality_upgrades.size()==(coop?2u:1u) &&
              kernel::R32(Live+0x4978)==3 &&
              kernel::R32(Live+0x49C8)==(coop?3u:1u) &&
              kernel::R32(Live+0x4954)==0x42C80001 &&
              kernel::R32(Live+0x49A4)==0x42C80002,
              "three Fatality levels unlock only for active players without filling existing charge");
        Check(kernel::R32(Live+0xE0)==4 && kernel::R32(Live+0x90)==1 &&
              kernel::R32(Live+0x94)==(coop?1200u:600u) && kernel::R32(First+0x370)==600 &&
              kernel::R32(First+0x368)==kernel::R32(First+0x370),
              "native story pickups refill Solo and respect the shared Ko-op pool");
        if(coop)Check(kernel::R32(Second+0x370)==600 && kernel::R32(Second+0x368)==600,
                      "Ko-op player two receives the native campaign-upgraded health");
        Check(kernel::R32(Live+0x88)==coop,"campaign upgrades preserve the actual player mode");
        for(unsigned player=0;player<2;++player)
            for(unsigned skill=0;skill<27;++skill){
                const auto value=memory[Live+0x1C4+player*0x1C+skill];
                if(skill<=10 || (skill>=13 && skill<=22))
                    Check(value==memory[0x4F3390+skill],"campaign skill receives its tuned rank");
                else
                    Check(value==uint8_t(skill>=23 && coop?2:0),
                          "Versus and Ko-op-only moves retain native unlock state");
            }
        RoomReady(ctx);
        Check(skill_grants.size()==42 && health_tiers.size()==1 && fatality_upgrades.size()==(coop?2u:1u),
              "repeated room-ready callbacks cannot reapply abilities or refill health during combat");
        const auto damage=kernel::R32(First+0x370)-5;
        kernel::W32(First+0x368,damage);
        // Native Ko-op-only ranks may change independently of the campaign
        // loadout. That must not trigger an upgrade or heal a damaged hero.
        memory[Live+0x1C4+23]=uint8_t(coop?1:0);
        const auto previous_messages=messages.size();
        RoomLoading();RoomReady(ctx);
        Check(skill_grants.size()==42 && health_tiers.size()==1 && fatality_upgrades.size()==(coop?2u:1u) &&
              kernel::R32(First+0x368)==damage && messages.size()==previous_messages,
              "same-room loading callbacks preserve combat damage, upgrades and the one-time level banner");
        const auto messages_before_restart=messages.size();
        native_room_reset=true;
        ctx=Context();sub_000164BF(ctx,memory.data());
        Check(skill_grants.size()==84 && health_tiers.size()==2 && fatality_upgrades.size()==2*(coop?2u:1u) &&
              kernel::R32(Live+0x4978)==3 && kernel::R32(Live+0x49C8)==(coop?3u:0u) &&
              kernel::R32(Live+0xE0)==4 && kernel::R32(Live+0x90)==1 &&
              kernel::R32(First+0x368)==kernel::R32(First+0x370) &&
              messages.size()==messages_before_restart+1,
              "a real native same-room reinitialization restores upgrades and shows its level banner again");
        RoomLoading();kernel::W32(0x505970,163);
        if(coop)kernel::W32(Second+0x370,0);
        RoomReady(ctx);
        if(coop){
            Check(skill_grants.size()==84 && health_tiers.size()==2 && fatality_upgrades.size()==2*(coop?2u:1u),
                  "Ko-op upgrades wait until P2's native health component is initialized");
            kernel::W32(Second+0x370,100);
            UpdateVoice(ctx);
        }
        Check(skill_grants.size()==126 && health_tiers.size()==3 && fatality_upgrades.size()==2*(coop?2u:1u),
              "a new Survival arena preserves already-unlocked Fatality levels without redundant grants");
        ResetSession();RoomLoading();kernel::W32(0x505970,164);RoomReady(ctx);
        Check(skill_grants.size()==126 && health_tiers.size()==3 && fatality_upgrades.size()==2*(coop?2u:1u) && !debug_refill,
              "leaving Survival never applies upgrades to normal gameplay");
    }
    const auto intro_for_abort=[&](bool already_paused){
        memory[MessageSlot]=0;
        memory[0x5AE5B0]=uint8_t(already_paused);
        kernel::W32(0x6C92F8,0);kernel::W32(0x6C9308,0);
        kernel::W32(0x5ACEBC,0);
        starting=true;ctx=Context();sub_000122D4(ctx,memory.data());
        kernel::W32(0x639224,0x720000);RoomReady(ctx);
        Check(ActiveSession() && HoldRulesWorld() && memory[0x5AE5B0]==1,
              "the intro pauses gameplay before rules confirmation even after a fresh session starts");
        memory[MessageSlot]=0;UpdateVoice(ctx);
        Check(HoldRulesWorld() && memory[0x5AE5B0]==1,
              "native A confirmation enters the held fade before testing an early exit");
        DeferMusic(0x709D5875);
    };
    intro_for_abort(false);
    const auto before_abort_music=music_tracks.size();
    ResetSession();UpdateVoice(ctx);
    Check(!ActiveSession() && !HoldRulesWorld() && memory[0x5AE5B0]==0 &&
          music_tracks.size()==before_abort_music,
          "aborting mid-fade releases only the owned pause and drops deferred level music");
    intro_for_abort(false);
    kernel::W32(0x6C92F8,1);ResetSession();
    Check(memory[0x5AE5B0]==1,"aborting during native loading cannot clear its separate pause");
    kernel::W32(0x6C92F8,0);UpdateVoice(ctx);
    Check(memory[0x5AE5B0]==0,"the abandoned intro pause drains after the native loading state ends");
    intro_for_abort(false);
    kernel::W32(0x6C9308,1);ResetSession();
    Check(memory[0x5AE5B0]==1,"aborting with another native menu open must preserve that menu's pause");
    kernel::W32(0x6C9308,0);UpdateVoice(ctx);
    Check(memory[0x5AE5B0]==0,"the abandoned intro pause drains when the other native menu closes");
    intro_for_abort(true);ResetSession();UpdateVoice(ctx);
    Check(memory[0x5AE5B0]==1,"Survival never claims or clears an external pause that predates its rules");
    auto native_release=Context();sub_00014345(native_release,memory.data());
    Check(memory[0x5AE5B0]==0,"native unpause remains available after exiting the Survival session");
    std::printf("Survival session: %u failures\n",failures);return failures?1:0;
}
