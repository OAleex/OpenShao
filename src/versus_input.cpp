#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace {
constexpr uint32_t Input=0x67C658;
constexpr uint16_t Start=0x800,Back=0x100;
uint32_t VersusState() {
    const auto preferences=kernel::R32(0x5ACEBC),versus=kernel::R32(0x5ACEB8);
    if(!preferences || !versus || kernel::R32(preferences+0x88)!=2)return ~0u;
    return kernel::R32(versus+0x14);
}
void ConsumeStart() {
    auto consume=[](uint32_t address) {
        const auto value=kernel::R16(address);
        if(value&Start)kernel::W16(address,uint16_t(value&~Start));
    };
    consume(Input+0x12);
    consume(Input+0x16);
    for(unsigned port=0;port<4;++port) {
        consume(Input+0x26+port*0x98);
        consume(Input+0x2A+port*0x98);
    }
}
bool SuppressStartPause() {
    const auto state=VersusState();
    if(state!=1)return false;
    const auto events=kernel::R16(Input+0x12);
    if(!(events&Start) || (events&Back))return false;
    // Pause can run before the next Versus simulation update. Leave Start
    // available to the intro while preserving the original Back pause.
    return true;
}
}

X86_EXTERN_FUNC(__imp__sub_000140B6);
X86_FUNC(sub_000140B6) {
    const bool intro=VersusState()==1;
    __imp__sub_000140B6(ctx,base);
    // The Versus update precedes pause dispatch and can already be on Display
    // Round when pause reads input. Consume the skip without changing held state.
    if(intro && (ctx.eax&255))ConsumeStart();
}
X86_EXTERN_FUNC(__imp__sub_00011104);
X86_FUNC(sub_00011104) {
    const bool suppress=SuppressStartPause();
    __imp__sub_00011104(ctx,base);
    if(suppress)ctx.eax&=~0xFFu;
}
