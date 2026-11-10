#include "cpu/x86_ops.h"

X86_EXTERN_FUNC(__imp__sub_00144C20);
X86_FUNC(sub_00144C20) {
    // The native ctor copies two uninitialized locals into each record: zero them first.
    X86_MEM32(ctx.esp - 8) = 0;
    X86_MEM32(ctx.esp - 4) = 0;
    __imp__sub_00144C20(ctx, base);
}
