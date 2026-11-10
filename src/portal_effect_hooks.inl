#include "portal_effect.h"
#include <bit>
#include <mutex>
#include <unordered_set>

namespace {
thread_local uint32_t portal_lookup_destination=0;
std::mutex portal_generators_mutex;
std::unordered_set<uint32_t> portal_generators;

bool PortalGuestRange(uint32_t address,uint32_t bytes) {
    return address>=0x10000 && address<=0xFFFFFFFFu-bytes;
}
bool IsPortalGenerator(uint32_t generator) {
    std::lock_guard guard(portal_generators_mutex);
    return portal_generators.contains(generator);
}
}

X86_EXTERN_FUNC(__imp__sub_00105E10);
X86_FUNC(sub_00105E10) {
    __imp__sub_00105E10(ctx,base);
    // An allocation for any native class can reuse a former generator's address.
    if(PortalGuestRange(ctx.eax,0x18)) {
        std::lock_guard guard(portal_generators_mutex);
        portal_generators.erase(ctx.eax+0x18);
    }
}

X86_EXTERN_FUNC(__imp__sub_001ADD80);
X86_FUNC(sub_001ADD80) {
    const uint32_t bank=kernel::R32(ctx.esp+4),effect=kernel::R32(ctx.esp+8);
    const uint32_t destination=kernel::R32(ctx.esp+12);
    portal_lookup_destination=0;
    __imp__sub_001ADD80(ctx,base);
    // The original exit event's mode 3 resolves to native effect bank 2.
    if(bank!=2 || effect!=105 || !ctx.eax) return;
    const uint32_t source=kernel::R32(0x7068B8);
    if(!PortalGuestRange(source,106*0xEC) || ctx.eax!=source+105*0xEC) return;
    portal_lookup_destination=destination?destination:ctx.eax;
}

X86_EXTERN_FUNC(__imp__sub_001ED400);
X86_FUNC(sub_001ED400) {
    const uint32_t descriptor=kernel::R32(ctx.esp+4);
    const bool known=descriptor && descriptor==portal_lookup_destination;
    const uint32_t existing=PortalGuestRange(descriptor,0xEC)?kernel::R32(descriptor+0xE8):0;
    portal_lookup_destination=0;
    __imp__sub_001ED400(ctx,base);
    const uint32_t generator=PortalGuestRange(ctx.eax,0x18)?ctx.eax+0x18:existing;
    if(!PortalGuestRange(generator,0x174)) return;
    std::lock_guard guard(portal_generators_mutex);
    portal_generators.erase(generator);
    if(known) portal_generators.insert(generator);
}

X86_EXTERN_FUNC(__imp__sub_001282F0);
X86_FUNC(sub_001282F0) {
    // Every arena exit (rooms 0xA1..0xBF) uses (s)fx_s_ray105i (bank 2, effect 105) with the same rotation bug.
    const uint32_t room=kernel::R32(0x505970);
    bool repair=room>=0xA1 && room<=0xBF;
    const uint32_t renderer=ctx.ecx;
    uint32_t cursor=0;
    if(repair && PortalGuestRange(renderer,0xC0)) {
        const uint32_t slot=kernel::R32(ctx.esp+4),pointers=kernel::R32(renderer);
        repair=slot<100000 && PortalGuestRange(pointers,(slot+1)*4);
        if(repair) {
            const uint32_t particle=kernel::R32(pointers+slot*4);
            repair=PortalGuestRange(particle,0x10C);
            if(repair) {
                const uint32_t descriptor=particle+0x1C,flags=kernel::R32(descriptor+0x84);
                cursor=kernel::R32(renderer+0x34);
                repair=(flags&0x80) && !(flags&0x20000) &&
                       IsPortalGenerator(kernel::R32(descriptor+0xE4)) && PortalGuestRange(cursor,80);
            }
        }
    } else repair=false;
    __imp__sub_001282F0(ctx,base);
    // Material flushes advance the batch start, but retain this vertex cursor.
    if(!repair || kernel::R32(renderer+0x34)!=cursor+80) return;
    const uint32_t owner=kernel::R32(0x639224);
    const uint32_t camera=PortalGuestRange(owner,0x308)?kernel::R32(owner+0x304):0;
    if(!PortalGuestRange(camera,0x34C)) return;
    const std::array<float,3> eye{
        std::bit_cast<float>(kernel::R32(camera+0x340)),
        std::bit_cast<float>(kernel::R32(camera+0x344)),
        std::bit_cast<float>(kernel::R32(camera+0x348))};
    for(float value:eye) if(!std::isfinite(value)) return;
    std::array<float,16> matrix;
    for(unsigned i=0;i<16;++i) matrix[i]=std::bit_cast<float>(kernel::R32(renderer+0x40+i*4));
    auto vertices=std::span<uint8_t>(static_cast<uint8_t*>(kernel::HostPtr(cursor)),80);
    if(!openshao::RestorePortalWorldAxes(vertices,matrix)) return;
    openshao::FacePortalQuad(vertices,eye);
    LOG_FIRST(4,"openshao graphics: restored arena %02X exit portal billboard orientation",room);
}
