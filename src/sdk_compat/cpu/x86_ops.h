#pragma once

// Generated code is linked in: the SDK's game-DLL ABI must not replace the weak overrides.
#include_next "cpu/x86_ops.h"

#undef X86_HOST_API
#undef X86_HOST_EXPORT
#undef X86_WEAK_FUNC
#define X86_HOST_API
#define X86_HOST_EXPORT
#define X86_WEAK_FUNC(name, impl) \
    extern "C" __attribute__((weak, noinline)) void name(X86Context& __restrict ctx, uint8_t* base) { impl(ctx, base); }
