#include "brightness.h"
#include "kernel/kernel.h"
#include <bit>

namespace openshao::brightness {
void Initialize(int percent) {
    // Seed the mapped image before guest startup. Native profile loading can replace this value.
    kernel::W32(0x4DF5C0, std::bit_cast<uint32_t>(percent / 100.f));
    LOGR("Brightness: initial preference=%d percent", percent);
}
}

X86_EXTERN_FUNC(__imp__sub_00123A50);
X86_FUNC(sub_00123A50) {
    const auto caller = kernel::R32(ctx.esp);
    if (caller == 0x4C18A) {
        const auto scene = kernel::R32(0x5982D4);
        // Press START shares the calibration handler, whose draft is not initialized until Start.
        if (scene && kernel::R32(scene + 0x2E4) == 0x12)
            kernel::W32(ctx.esp + 4, kernel::R32(0x4DF5C0));
    } else if (caller == 0x4BDD3) {
        // Default resets the draft before the preview; confirm/cancel stay in the retail handler.
        const auto value = std::bit_cast<uint32_t>(openshao::brightness::DefaultPercent / 100.f);
        kernel::W32(0x598308, value);
        kernel::W32(ctx.esp + 4, value);
        LOGR("Brightness: native Default preview=%d percent", openshao::brightness::DefaultPercent);
    }
    __imp__sub_00123A50(ctx, base);
}
