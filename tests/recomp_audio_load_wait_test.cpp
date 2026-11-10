#include "kernel/kernel.h"
#include <array>
#include <cstdio>
#include <cstdlib>

namespace {
alignas(64) std::array<uint8_t, 0x10000> guest{};
unsigned polls = 0, blanks = 0, pending_polls = 0;
int completed_status = 0;
void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "audio load wait: %s\n", reason);
        std::exit(1);
    }
}
}

namespace kernel {
uint8_t* g_base = guest.data();
uint32_t CallGuest(X86Context& ctx, uint32_t function, const uint32_t* args, size_t count) {
    Check(function == 0x1EBDC0 && count == 1 && args[0] == 12,
          "polling must query the original sound partition");
    ++polls;
    // A guest callback may change all caller-saved state.
    ctx.eax = ctx.ecx = ctx.edx = 0;
    ctx.zf ^= 1;
    return polls <= pending_polls ? 1 : uint32_t(completed_status);
}
}

X86_FUNC(__imp__sub_00123920) {
    ++blanks;
    ctx.esp += 4;
}
X86_WEAK_FUNC(sub_00123920, __imp__sub_00123920)

void Wait(uint32_t caller, unsigned pending, int status) {
    polls = blanks = 0;
    pending_polls = pending;
    completed_status = status;
    X86Context ctx{};
    ctx.esp = 0x8000;
    ctx.eax = 0x12345678;
    ctx.ecx = 0x23456789;
    ctx.edx = 0x34567890;
    ctx.esi = 12;
    ctx.edi = 0x456789AB;
    ctx.zf = 1;
    kernel::W32(ctx.esp, caller);
    auto expected = ctx;
    expected.esp += 4;
    sub_00123920(ctx, guest.data());
    Check(std::memcmp(&ctx, &expected, sizeof(ctx)) == 0,
          "waiting must preserve the native stack and register state");
}

int main() {
    Wait(0xABCDEF, 0, 0);
    Check(blanks == 1 && polls == 0, "other VBlank callers must use the original routine");
    Wait(0x152026, 0, 0);
    Check(blanks == 0 && polls == 1, "completed sound loads must return without waiting for VBlank");
    Wait(0x152026, 1, 0);
    Check(blanks == 0 && polls == 2, "pending sound loads must be checked until completion");
    Wait(0x152026, 0, -1);
    Check(blanks == 1 && polls == 1, "failed sound loads must retain the native retry pacing");
    Wait(0x152026, 0xFFFFFFFFu, 0);
    Check(blanks == 0 && polls > 0, "long loads must return control to the native servicing loop");
    std::puts("audio load wait: ok");
}
