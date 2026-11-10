#include "kernel/kernel.h"
#include <chrono>
#include <thread>

X86_EXTERN_FUNC(__imp__sub_00123920);
X86_FUNC(sub_00123920) {
    // Sound-partition header loader: VBlank only polls CRI completion (slot in ESI).
    if (kernel::R32(ctx.esp) != 0x152026) {
        __imp__sub_00123920(ctx, base);
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(16667);
    for (;;) {
        auto query = ctx;
        const auto status = kernel::CallGuest(query, 0x1EBDC0, {ctx.esi});
        // CRI returns 0 when ready, 1 while reading, and -1 on error.
        if (status > 1) {
            __imp__sub_00123920(ctx, base);
            return;
        }
        // A long read yields to the audio loop once per simulation interval.
        if (status == 0 || std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
    ctx.esp += 4;
}
