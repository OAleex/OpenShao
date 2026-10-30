#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include "app/app.h"
#include "frame_clock.h"
#include "frame_rate.h"
#include "frame_status.h"
#include "menu_frames.h"
#include "render_frame_state.h"
#include "xdk/d3d8.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
// These entry points and layouts belong to the SHA-256-verified retail image.
// Only visual queues are reused. Input, scripts, animation and physics remain on
// the complete original gameplay callback at 60 Hz.
using Clock=std::chrono::steady_clock;
thread_local bool active=false,extra=false,catchup=false,faulted=false;
thread_local uint64_t tick=0,draws=0,extras=0;
thread_local uint32_t room=0,world=0,queue=0;
thread_local std::array<uint8_t,0x25F0> native_queue{};
thread_local std::array<uint8_t,0x588> native_sprites{};
thread_local std::array<uint8_t,0xD804> native_polygons{};
constexpr auto font_addresses=openshao::frames::FontAddresses;
using Fonts=openshao::frames::Fonts;
thread_local Fonts native_fonts{};
thread_local bool have_queue=false;
thread_local gpu::State native_gpu{};
thread_local uint32_t render_cache=0;
thread_local std::array<uint8_t,0x950> native_render_cache{};
thread_local float alpha=1;
// ParticleSystemImpl at 0x50ED90: +0 visible particle list, +4 count. FUN_001289B0
// clears both after drawing, so every output-only pass needs the captured list again.
constexpr uint32_t particle_list=0x50ED90;
thread_local std::array<uint32_t,2> native_particles{};
using openshao::frames::Block;
// The first reason the current capture cannot feed an interpolated frame.
thread_local Block capture_block=Block::None;
// A failed state check disables interpolation only for the room where it happened.
thread_local uint32_t fault_world=0,fault_room=0;
thread_local openshao::frames::Window report{};
thread_local Block pending_block=Block::None;
thread_local bool extra_since_logic=true;
// Menu output frames run outside the gameplay callback and only blend 2D queues.
thread_local bool in_gameplay=false,menu_extra=false;
// Gameplay logic frame counter; it stops while a pause menu holds the world.
thread_local uint32_t held_logic=0;
struct Pose {uint32_t bones=0,count=0;uint64_t tick=0;std::vector<float> previous,current;};
struct PoseFlags {uint32_t model,bones;uint8_t root;std::vector<uint8_t> bones_dirty;};
thread_local std::unordered_map<uint32_t,Pose> poses;
thread_local std::array<float,6> camera{},previous_camera{};
thread_local bool have_camera=false;
thread_local std::array<uint32_t,2> fade_before{};
thread_local std::array<uint32_t,6> screen_before{};
using Materials=std::vector<std::pair<uint32_t,uint32_t>>;
thread_local std::vector<Materials> material_passes;
thread_local size_t material_pass=0;
bool Valid(uint32_t p,size_t n){return p>=0x10000 && n && uint64_t(p)+n<0x100000000ull && mem::IsCommitted(p) && mem::IsCommitted(p+uint32_t(n)-1);}
void CopyOut(uint32_t p,void* to,size_t n){std::memcpy(to,kernel::HostPtr(p),n);}
void CopyIn(uint32_t p,const void* from,size_t n){std::memcpy(kernel::HostPtr(p),from,n);}
void RestoreGpu(gpu::State state) {
    // Swap rotates the surface memory while keeping its guest header stable.
    for(auto* surface:{&state.rt,&state.zs})if(Valid(surface->header,20)) {
        surface->data=kernel::R32(surface->header+4);
        surface->format=kernel::R32(surface->header+12);
        surface->size=kernel::R32(surface->header+16);
    }
    std::lock_guard lock(d3d::Lock());d3d::State()=std::move(state);
}
bool CaptureFonts(Fonts& fonts) {
    for(size_t i=0;i<font_addresses.size();++i) {
        const auto address=font_addresses[i];
        CopyOut(address,fonts[i].data(),fonts[i].size());
        if(!openshao::frames::ValidFontQueue(fonts[i],address))return false;
    }
    return true;
}
void RestoreFonts(const Fonts& fonts){for(size_t i=0;i<fonts.size();++i)CopyIn(font_addresses[i],fonts[i].data(),fonts[i].size());}
std::vector<float> ReadPose(uint32_t model,uint32_t bones,uint32_t count) {
    std::vector<float> out((count+1)*20);
    CopyOut(model+0x60,out.data(),80);
    for(uint32_t i=0;i<count;++i)CopyOut(bones+i*0x390+0x10,out.data()+(i+1)*20,80);
    return out;
}
void WritePose(uint32_t model,const Pose& p,bool blend) {
    if(p.tick!=tick || !Valid(model,0xB0) || !Valid(p.bones,p.count*0x390) ||
       kernel::R32(model+0x28)!=p.bones || kernel::R32(model+0x30)!=p.count)return;
    auto values=p.current;
    if(blend && p.previous.size()==values.size()) {
        // Reject discontinuous moves rather than dragging a model across a room.
        const auto dx=values[16]-p.previous[16],dy=values[17]-p.previous[17],dz=values[18]-p.previous[18];
        const bool finite=std::all_of(values.begin(),values.end(),[](float f){return std::isfinite(f);}) &&
                          std::all_of(p.previous.begin(),p.previous.end(),[](float f){return std::isfinite(f);});
        if(finite && dx*dx+dy*dy+dz*dz<10000)
            for(size_t i=0;i<values.size();++i)values[i]=p.previous[i]+(values[i]-p.previous[i])*alpha;
    }
    CopyIn(model+0x60,values.data(),80);
    for(uint32_t i=0;i<p.count;++i)CopyIn(p.bones+i*0x390+0x10,values.data()+(i+1)*20,80);
}
void RestorePoses(){for(auto& [model,p]:poses)WritePose(model,p,false);}
void Flush(X86Context ctx){kernel::CallGuest(ctx,0x3C8DE0,{});}
uint32_t Queue() {
    const auto wrapper=kernel::R32(0x510B74);
    return Valid(wrapper,4)?kernel::R32(wrapper):0;
}
void Call(X86Context ctx,uint32_t fn){kernel::CallGuest(ctx,fn,{});}
std::array<uint32_t,2> Particles() {
    std::array<uint32_t,2> list{kernel::R32(particle_list),kernel::R32(particle_list+4)};
    if(list[1]>=0x10000 || (list[1] && !Valid(list[0],list[1]*4)))list={};
    return list;
}
void SetParticles(const std::array<uint32_t,2>& list){kernel::W32(particle_list,list[0]);kernel::W32(particle_list+4,list[1]);}
void Drop(Block reason){have_queue=false;if(capture_block==Block::None)capture_block=reason;}
void Fault(const char* message) {
    faulted=true;pending_block=Block::Fault;
    fault_world=kernel::R32(0x639224);fault_room=kernel::R32(0x505970);
    LOGW("%s; using 60 FPS until the room changes",message);
}
int64_t NowNs(){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();}
// A reason counts only when no output frame came between two intervals.
void Account(unsigned steps) {
    if(!steps)return;
    report.ticks+=steps;
    if(!extra_since_logic) {
        const auto reason=pending_block==Block::None?Block::Late:pending_block;
        report.Add(reason);
    }
    extra_since_logic=false;pending_block=Block::None;
    if(report.ticks>=600) {
        // Release logs omit info messages, so degraded output is reported here.
        if(openshao::frames::Target()>60 && report.extras*600<report.ticks*unsigned(openshao::frames::Target()-60)*9)
            LOGR("Display output target: %u of %u gameplay intervals interpolated; main limit: %s",
                 report.extras,report.ticks,openshao::frames::BlockName(report.Dominant()));
        report={};
    }
}
namespace menu = openshao::frames::menu;
struct MenuQueues {
    menu::Polygons polygons{};
    menu::Sprites sprites{};
    Fonts fonts{};
    bool Capture() {
        CopyOut(menu::PolygonQueue,polygons.data(),polygons.size());
        CopyOut(menu::SpriteQueue,sprites.data(),sprites.size());
        uint32_t count=0;
        if(!menu::PolygonCount(polygons,count) || menu::U32(sprites.data())>menu::SpriteLimit)return false;
        return CaptureFonts(fonts);
    }
    void Write()const {
        CopyIn(menu::PolygonQueue,polygons.data(),polygons.size());
        CopyIn(menu::SpriteQueue,sprites.data(),sprites.size());
        RestoreFonts(fonts);
    }
};
struct MenuState {
    MenuQueues previous,current,blend;
    openshao::frames::OutputCadence cadence;
    bool have_previous=false,last_produced=false;
    int64_t last_ns=0;
    unsigned slow=0,backoff=0,backoff_length=600,fault_cooldown=0;
    openshao::frames::Window report{};
};
MenuState& Menu(){static auto state=std::make_unique<MenuState>();return *state;}
// Everything a render pass consumes or advances, other than the 2D queues.
struct RenderSideEffects {
    uint32_t queue=0,cache=0,flag=0,countdown=0;
    std::array<uint32_t,2> particles{};
    std::array<uint8_t,0x25F0> tracked{};
    std::array<uint8_t,0x950> render_cache{};
    std::array<uint32_t,2> fade{};
    std::array<uint8_t,24> screen{};
    std::array<uint32_t,3> counters{};
    std::vector<uint8_t> actors;
    gpu::State gpu{};
    static constexpr std::array<uint32_t,3> counter_addresses{0x5A0CC0,0x5AFC60,0x5CE878};
    bool Capture() {
        queue=Queue();cache=kernel::R32(0x639750);
        if(!Valid(queue,tracked.size()) || !Valid(cache,render_cache.size()))return false;
        CopyOut(queue,tracked.data(),tracked.size());CopyOut(cache,render_cache.data(),render_cache.size());
        flag=kernel::R32(0x639438);countdown=kernel::R32(0x63304C);
        particles={kernel::R32(particle_list),kernel::R32(particle_list+4)};
        fade={kernel::R32(0x50E598),kernel::R32(0x637AD4)};
        CopyOut(0x639CE8,screen.data(),screen.size());
        for(size_t i=0;i<counters.size();++i)counters[i]=kernel::R32(counter_addresses[i]);
        actors.resize(14*0x3238);CopyOut(0x5D1AB8,actors.data(),actors.size());
        {std::lock_guard lock(d3d::Lock());gpu=d3d::State();}
        return true;
    }
    // Returns false when the pass changed game state that must stay at 60 Hz.
    bool Restore() {
        CopyIn(queue,tracked.data(),tracked.size());CopyIn(cache,render_cache.data(),render_cache.size());
        kernel::W32(0x639438,flag);kernel::W32(0x63304C,countdown);
        SetParticles(particles);
        kernel::W32(0x50E598,fade[0]);kernel::W32(0x637AD4,fade[1]);
        CopyIn(0x639CE8,screen.data(),screen.size());
        RestoreGpu(std::move(gpu));
        bool same=std::memcmp(actors.data(),kernel::HostPtr(0x5D1AB8),actors.size())==0;
        for(size_t i=0;i<counters.size();++i)if(kernel::R32(counter_addresses[i])!=counters[i]){same=false;kernel::W32(counter_addresses[i],counters[i]);}
        if(!same)CopyIn(0x5D1AB8,actors.data(),actors.size());
        return same;
    }
};
bool MenuWanted(MenuState& m,int64_t now) {
    const int64_t period=m.last_ns?now-m.last_ns:0;
    m.last_ns=now;
    if(period>250000000)m.have_previous=false;
    // Two output frames must not slow the 60 Hz menu logic. Long stalls are loading, not output cost.
    const auto rate=openshao::frames::Target();
    const int64_t budget=app::Get().vsync?(int64_t((rate+59)/60)*1000000000/rate+1500000):17500000;
    const bool late=m.last_produced && period>budget && period<100000000;
    if(m.fault_cooldown){--m.fault_cooldown;return false;}
    if(!app::Get().vsync) {
        // Without VSync, skipping one output frame lets Pace catch up immediately.
        m.backoff=0;m.slow=0;
        return !late;
    }
    // With VSync each present waits for the display, so a 60 Hz display halves the menu logic.
    if(m.backoff){--m.backoff;return false;}
    if(late)++m.slow;else m.slow=0;
    if(m.slow>=3) {
        m.slow=0;m.have_previous=false;m.backoff=m.backoff_length;
        m.backoff_length=std::min(m.backoff_length*2,36000u);
        LOGR("Display output target: menu output frames paused for %u updates; VSync presents exceed the update time",m.backoff);
        return false;
    }
    return true;
}
void MenuAccount(MenuState& m,bool produced,Block reason) {
    ++m.report.ticks;
    if(produced)++m.report.extras;else m.report.Add(reason);
    if(m.report.ticks>=600) {
        if(m.report.extras*10<m.report.ticks*9)
            LOGR("Display output target: %u of %u menu updates had an output frame; main limit: %s",
                 m.report.extras,m.report.ticks,openshao::frames::BlockName(m.report.Dominant()));
        m.report={};
    }
}
}

X86_EXTERN_FUNC(__imp__sub_00107880);
X86_FUNC(sub_00107880) {
    const auto caller=kernel::R32(ctx.esp),model=kernel::R32(ctx.esp+4);
    __imp__sub_00107880(ctx,base);
    if(!active || caller!=0x12B8E4 || !Valid(model,0xB0))return;
    const auto bones=kernel::R32(model+0x28),count=kernel::R32(model+0x30);
    if(count>512 || !Valid(bones,count*0x390))return;
    auto& p=poses[model];
    if(p.bones!=bones || p.count!=count)p={};
    if(p.tick!=tick) {
        if(p.tick+1==tick)p.previous=std::move(p.current);else p.previous.clear();
        p.current=ReadPose(model,bones,count);
        p.bones=bones;p.count=count;p.tick=tick;
    }
    WritePose(model,p,true);
}

X86_EXTERN_FUNC(__imp__sub_00114BC0);
X86_FUNC(sub_00114BC0) {
    std::array<uint32_t,2> saved{kernel::R32(0x50E598),kernel::R32(0x637AD4)};
    if(active && extra){kernel::W32(0x50E598,fade_before[0]);kernel::W32(0x637AD4,fade_before[1]);}
    else if(active)fade_before=saved;
    __imp__sub_00114BC0(ctx,base);
    if((active && extra) || menu_extra){kernel::W32(0x50E598,saved[0]);kernel::W32(0x637AD4,saved[1]);}
}
X86_EXTERN_FUNC(__imp__sub_001308E0);
X86_FUNC(sub_001308E0) {
    std::array<uint32_t,6> saved{};CopyOut(0x639CE8,saved.data(),24);
    if(active && extra)CopyIn(0x639CE8,screen_before.data(),24);
    else if(active)screen_before=saved;
    __imp__sub_001308E0(ctx,base);
    if((active && extra) || menu_extra)CopyIn(0x639CE8,saved.data(),24);
}
X86_EXTERN_FUNC(__imp__sub_001221C0);
X86_FUNC(sub_001221C0) {
    // Preserve the animation input for each native material pass. Keeping the
    // original velocity also keeps its texture-selection branch active.
    Materials frames;
    if((active || menu_extra) && Valid(ctx.ecx,4)) {
        auto lookup=ctx;const auto vtable=kernel::R32(ctx.ecx);
        if(Valid(vtable,12)) {
            const auto wrapper=kernel::CallGuest(lookup,kernel::R32(vtable+8),{});
            if(Valid(wrapper,8)) {
                const auto list=kernel::R32(wrapper+4);
                if(Valid(list,16)) {
                    const auto count=kernel::R32(list),indexes=kernel::R32(list+8),bank=kernel::R32(list+12);
                    if(count<4096 && Valid(indexes,count*4) && Valid(bank,4))for(uint32_t i=0;i<count;++i) {
                        const auto index=kernel::R32(indexes+i*4);
                        if(!Valid(bank+index*4,4))continue;
                        const auto material=kernel::R32(bank+index*4);
                        if(Valid(material,0x54)) {
                            frames.emplace_back(material+0x40,kernel::R32(material+0x40));
                        }
                    }
                }
            }
        }
    }
    if(active && !extra)material_passes.push_back(frames);
    if(active && extra) {
        const auto* previous=material_pass<material_passes.size()?&material_passes[material_pass]:nullptr;
        bool matches=previous && previous->size()==frames.size();
        if(matches)for(size_t i=0;i<frames.size();++i)if((*previous)[i].first!=frames[i].first)matches=false;
        if(matches)for(const auto& [p,v]:*previous)kernel::W32(p,v);
        else Fault("Native material state changed");
    }
    if(active)++material_pass;
    __imp__sub_001221C0(ctx,base);
    if((active && extra) || menu_extra)for(const auto& [p,v]:frames)kernel::W32(p,v);
}

X86_EXTERN_FUNC(__imp__sub_00114CD0);
X86_EXTERN_FUNC(__imp__sub_001300D0);
X86_FUNC(sub_001300D0) {
    if(active && ctx.ecx==0x639760) {
        if(extra)CopyIn(ctx.ecx,native_sprites.data(),native_sprites.size());
        else {CopyOut(ctx.ecx,native_sprites.data(),native_sprites.size());if(kernel::R32(ctx.ecx)>32)Drop(Block::Sprites);}
    }
    __imp__sub_001300D0(ctx,base);
}
namespace {
const bool polygon_observer=[] {
    openshao::frames::before_polygons=[](uint32_t address) {
        if(active && address==0x63F5E0) {
            if(extra)CopyIn(address,native_polygons.data(),native_polygons.size());
            else {CopyOut(address,native_polygons.data(),native_polygons.size());if(kernel::R32(address)>512)Drop(Block::Polygons);}
        }
    };
    return true;
}();
}
X86_EXTERN_FUNC(__imp__sub_001EE6B0);
X86_FUNC(sub_001EE6B0) {
    if(active)for(size_t i=0;i<font_addresses.size();++i)if(ctx.ecx==font_addresses[i]) {
        if(extra)CopyIn(ctx.ecx,native_fonts[i].data(),native_fonts[i].size());
        else {
            CopyOut(ctx.ecx,native_fonts[i].data(),native_fonts[i].size());
            if(!openshao::frames::ValidFontQueue(native_fonts[i],ctx.ecx))Drop(Block::Fonts);
        }
        break;
    }
    __imp__sub_001EE6B0(ctx,base);
}
X86_FUNC(sub_00114CD0) {
    // Pause menus and loading screens nested in the gameplay callback.
    const bool held=in_gameplay && !extra && (!kernel::R32(0x639224) || kernel::R32(0x6C92F8) || !kernel::R32(0x4EF42C));
    const bool was_active=active;
    if(held) {
        if(active){have_queue=false;capture_block=Block::State;have_camera=false;poses.clear();}
        active=false;
    }
    // A held frame with a running world would only repeat the 3D scene and blend live particles.
    const auto logic=kernel::R32(0x5AFC60);
    const bool frozen=held && logic==held_logic;
    if(held)held_logic=logic;
    if((frozen || (!active && !held && !in_gameplay)) && openshao::frames::Target()>60) {
        auto& m=Menu();
        const auto entry=ctx;
        const auto output_count=m.cadence.Take(openshao::frames::Target());
        const bool healthy=MenuWanted(m,NowNs());
        const bool wanted=healthy && output_count>1;
        RenderSideEffects effects;
        const bool captured=m.current.Capture();
        bool produced=false;
        Block reason=wanted?(captured?Block::Buffer:Block::Fonts):Block::Late;
        for(unsigned output=1;wanted && captured && output<output_count && effects.Capture();++output) {
            m.blend=m.current;
            if(m.have_previous) {
                menu::BlendPolygons(m.previous.polygons,m.current.polygons,m.blend.polygons,float(output)/output_count);
                menu::BlendSprites(m.previous.sprites,m.current.sprites,m.blend.sprites,float(output)/output_count);
                for(size_t i=0;i<m.blend.fonts.size();++i)
                    menu::BlendText(m.previous.fonts[i],m.current.fonts[i],m.blend.fonts[i],font_addresses[i],float(output)/output_count);
            }
            m.blend.Write();
            app::Get().fps_cap=openshao::frames::OutputCap();
            menu_extra=true;openshao::frames::replay=openshao::frames::Replay::Before;
            __imp__sub_00114CD0(ctx,base);
            menu_extra=false;openshao::frames::replay=openshao::frames::Replay::None;
            // Immediate vertex data is copied while recording, so no Flush is needed.
            m.current.Write();
            if(effects.Restore())produced=true;
            else {
                reason=Block::Fault;m.fault_cooldown=600;
                LOGW("Menu output frame changed game state; using 60 FPS for 600 updates");
                produced=false;ctx=entry;
                break;
            }
            ctx=entry;
        }
        app::Get().fps_cap=produced || (healthy && output_count==1)?openshao::frames::OutputCap():60;
        m.last_produced=produced;
        MenuAccount(m,produced,reason);
        __imp__sub_00114CD0(ctx,base);
        std::swap(m.previous,m.current);m.have_previous=captured;
        active=was_active;
        return;
    }
    if(held) {
        // Loops nested in the gameplay callback present once per update; pace them at 60 Hz.
        Menu().have_previous=false;app::Get().fps_cap=60;
        __imp__sub_00114CD0(ctx,base);active=was_active;return;
    }
    if(active){material_pass=0;if(!extra)material_passes.clear();}
    if(active && !extra && !catchup && kernel::R32(0x639224) && !kernel::R32(0x6C92F8)) {
        capture_block=Block::None;
        queue=Queue();have_queue=Valid(queue,native_queue.size());
        if(!have_queue)capture_block=Block::Buffer;
        {std::lock_guard lock(d3d::Lock());native_gpu=d3d::State();}
        render_cache=kernel::R32(0x639750);
        if(Valid(render_cache,native_render_cache.size()))CopyOut(render_cache,native_render_cache.data(),native_render_cache.size());
        else Drop(Block::Buffer);
        if(have_queue)CopyOut(queue,native_queue.data(),native_queue.size());
        CopyOut(0x639760,native_sprites.data(),native_sprites.size());
        CopyOut(0x63F5E0,native_polygons.data(),native_polygons.size());
        if(have_queue && !CaptureFonts(native_fonts))Drop(Block::Fonts);
        native_particles=Particles();
        previous_camera=camera;
        const std::array<uint32_t,6> addresses{0x699C58,0x699C5C,0x699C60,0x699C68,0x699C6C,0x699C70};
        for(size_t i=0;i<6;++i)std::memcpy(&camera[i],kernel::HostPtr(addresses[i]),4);
        if(!have_camera)previous_camera=camera;
        have_camera=true;
    }
    std::array<float,6> saved{};
    const std::array<uint32_t,6> addresses{0x699C58,0x699C5C,0x699C60,0x699C68,0x699C6C,0x699C70};
    if(active && have_camera)for(size_t i=0;i<6;++i) {
        CopyOut(addresses[i],&saved[i],4);
        const float delta=camera[i]-previous_camera[i];
        const bool continuous=std::isfinite(delta) && std::abs(delta)<(i<3?100.f:3.14f);
        float value=continuous?previous_camera[i]+delta*alpha:camera[i];
        CopyIn(addresses[i],&value,4);
    }
    if(active && extra)for(auto& [model,p]:poses)WritePose(model,p,true);
    __imp__sub_00114CD0(ctx,base);
    // The render worker reads guest vertices asynchronously. Drain it before
    // restoring visual poses or allowing the next guest update to reuse buffers.
    if(active){Flush(ctx);RestorePoses();for(size_t i=0;i<6;++i)CopyIn(addresses[i],&saved[i],4);++draws;}
}

X86_EXTERN_FUNC(__imp__sub_001B0AF0);
X86_FUNC(sub_001B0AF0) {
    const auto skip=kernel::R32(0x5CAB98);
    if(catchup)kernel::W32(0x5CAB98,1);
    __imp__sub_001B0AF0(ctx,base);
    if(catchup)kernel::W32(0x5CAB98,skip);
}
X86_EXTERN_FUNC(__imp__sub_00011069);
X86_FUNC(sub_00011069) {
    static thread_local openshao::frames::Clock clock;
    static thread_local uint64_t epoch=0;
    struct Scope{Scope(){in_gameplay=true;}~Scope(){in_gameplay=false;}} scope;
    Menu().have_previous=false;
    if(faulted && (kernel::R32(0x639224)!=fault_world || kernel::R32(0x505970)!=fault_room)) {
        faulted=false;LOGR("Display output target: interpolation resumed after a room change");
    }
    if(openshao::frames::Target()<=60 || faulted) {
        clock.Reset();have_queue=false;poses.clear();have_camera=false;
        if(faulted){pending_block=Block::Fault;Account(1);}
        else extra_since_logic=true;
        app::Get().fps_cap=60;__imp__sub_00011069(ctx,base);return;
    }
    auto advance=[&]{return clock.Take(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());};
    unsigned due=advance();
    if(epoch!=clock.Epoch()){epoch=clock.Epoch();have_queue=false;poses.clear();have_camera=false;}
    auto eligible=[&]{return have_queue && Queue()==queue && kernel::R32(0x639224)==world && kernel::R32(0x505970)==room && kernel::R32(0x4EF42C) && !kernel::R32(0x6C92F8);};
    auto blocker=[&] {
        if(!have_queue)return capture_block==Block::None?Block::Buffer:capture_block;
        if(Queue()!=queue)return Block::Queue;
        if(kernel::R32(0x639224)!=world || kernel::R32(0x505970)!=room)return Block::Scene;
        return Block::State;
    };
    if(!due && !eligible())pending_block=blocker();
    while(!due && !eligible()) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));due=advance();
    }
    Account(due);
    // The fixed simulation clock already prioritizes due gameplay updates.
    // Reuse the next spare output slot immediately when render cost recovers.
    if(epoch!=clock.Epoch()){epoch=clock.Epoch();have_queue=false;poses.clear();have_camera=false;}
    alpha=float(clock.Alpha());
    const auto entry=ctx;
    app::Get().fps_cap=openshao::frames::OutputCap();
    const auto nextworld=kernel::R32(0x639224),nextroom=kernel::R32(0x505970);
    if(world!=nextworld || room!=nextroom){poses.clear();have_camera=false;world=nextworld;room=nextroom;}
    for(unsigned step=0;step<due;++step) {
        ctx=entry;active=true;catchup=step+1<due;have_queue=false;++tick;
        __imp__sub_00011069(ctx,base);
        active=false;catchup=false;
        if(ctx.eax)break;
    }
    if(!due && eligible()) {
        auto pending_gpu=d3d::State();
        std::array<uint8_t,0x950> pending_cache{};CopyOut(render_cache,pending_cache.data(),pending_cache.size());
        std::array<uint8_t,0x25F0> pending{};CopyOut(queue,pending.data(),pending.size());
        std::array<uint8_t,0x588> pending_sprites{};CopyOut(0x639760,pending_sprites.data(),pending_sprites.size());
        std::array<uint8_t,0xD804> pending_polygons{};CopyOut(0x63F5E0,pending_polygons.data(),pending_polygons.size());
        Fonts pending_fonts{};const bool safe_fonts=CaptureFonts(pending_fonts);
        if(!safe_fonts){Drop(Block::Fonts);pending_block=Block::Fonts;app::Get().fps_cap=60;ctx.eax=0;ctx.esp+=4;return;}
        std::array<uint8_t,14*0x3238> actors{};CopyOut(0x5D1AB8,actors.data(),actors.size());
        std::vector<PoseFlags> flags;
        for(const auto& [model,p]:poses)if(p.tick==tick && Valid(model,0x261) && Valid(p.bones,p.count*0x390)) {
            PoseFlags saved{model,p.bones,kernel::G<uint8_t>(model+0x260),{}};
            for(uint32_t i=0;i<p.count;++i)saved.bones_dirty.push_back(kernel::G<uint8_t>(p.bones+i*0x390+0x210));
            flags.push_back(std::move(saved));
        }
        constexpr std::array<uint32_t,3> counters{0x5A0CC0,0x5AFC60,0x5CE878};
        std::array<uint32_t,3> before{};
        for(size_t i=0;i<before.size();++i)before[i]=kernel::R32(counters[i]);
        CopyIn(queue,native_queue.data(),native_queue.size());
        CopyIn(0x639760,native_sprites.data(),native_sprites.size());
        CopyIn(0x63F5E0,native_polygons.data(),native_polygons.size());
        RestoreFonts(native_fonts);
        RestoreGpu(native_gpu);CopyIn(render_cache,native_render_cache.data(),native_render_cache.size());
        const auto flag=kernel::R32(0x639438),countdown=kernel::R32(0x63304C);
        const std::array<uint32_t,2> pending_particles{kernel::R32(particle_list),kernel::R32(particle_list+4)};
        SetParticles(native_particles);
        kernel::W32(0x639438,1);active=true;extra=true;
        openshao::frames::replay=openshao::frames::Replay::After;
        Call(ctx,0x114CD0);
        openshao::frames::replay=openshao::frames::Replay::None;
        active=false;extra=false;kernel::W32(0x639438,flag);kernel::W32(0x63304C,countdown);
        SetParticles(pending_particles);
        // Native actor cleanup normally clears these once after a complete
        // gameplay callback. An output-only pass must preserve that cache state.
        for(const auto& saved:flags)if(Valid(saved.model,0x261) && kernel::R32(saved.model+0x28)==saved.bones) {
            kernel::G<uint8_t>(saved.model+0x260)=saved.root;
            for(size_t i=0;i<saved.bones_dirty.size();++i)kernel::G<uint8_t>(saved.bones+uint32_t(i)*0x390+0x210)=saved.bones_dirty[i];
        }
        CopyIn(queue,pending.data(),pending.size());++extras;
        CopyIn(0x639760,pending_sprites.data(),pending_sprites.size());
        CopyIn(0x63F5E0,pending_polygons.data(),pending_polygons.size());
        RestoreFonts(pending_fonts);
        RestoreGpu(std::move(pending_gpu));CopyIn(render_cache,pending_cache.data(),pending_cache.size());
        bool changed=std::memcmp(actors.data(),kernel::HostPtr(0x5D1AB8),actors.size())!=0;
        for(size_t i=0;i<before.size();++i)if(kernel::R32(counters[i])!=before[i]){changed=true;kernel::W32(counters[i],before[i]);}
        if(changed) {
            CopyIn(0x5D1AB8,actors.data(),actors.size());
            Fault("Native frame state validation failed");
        } else {++report.extras;extra_since_logic=true;}
        ctx.eax=0;ctx.esp+=4;
    }
    app::Get().fps_cap=60;
    if(tick%600==0)LOGI("Native rendering: logic=%llu draws=%llu extra=%llu models=%zu",tick,draws,extras,poses.size());
}
