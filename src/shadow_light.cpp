#include "kernel/kernel.h"
#include <cmath>

X86_EXTERN_FUNC(__imp__sub_0012D3B0);
X86_FUNC(sub_0012D3B0) {
    const uint32_t model = ctx.ecx;
    // The constructor seeds the shadow at the world origin with a 10000-unit radius.
    const bool first_light = !kernel::R32(0x6C92EC) && kernel::G<float>(model + 0x1a8) == 0 &&
        kernel::G<float>(model + 0x168) == 0 && kernel::G<float>(model + 0x16c) == 0 &&
        kernel::G<float>(model + 0x170) == 0 && kernel::G<float>(model + 0x1ac) == 10000;

    __imp__sub_0012D3B0(ctx, base);
    if (!first_light) return;

    const float smoothing = kernel::G<float>(0x632B70);
    if (!std::isfinite(smoothing) || smoothing < 1) return;
    float x = kernel::G<float>(model + 0x168);
    float y = kernel::G<float>(model + 0x16c);
    float z = kernel::G<float>(model + 0x170);
    const float radius = 10000 + (kernel::G<float>(model + 0x1ac) - 10000) * smoothing;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(radius) || radius <= 0) return;

    // Recover the first target from the constructor seed's native interpolation; distant targets reset natively.
    const float x_squared = x * x;
    const float y_squared = y * y;
    const float z_squared = z * z;
    if (std::sqrt((x_squared + z_squared) + y_squared) <= 10000) {
        x *= smoothing;
        y *= smoothing;
        z *= smoothing;
    }
    kernel::G<float>(model + 0x168) = x;
    kernel::G<float>(model + 0x16c) = y;
    kernel::G<float>(model + 0x170) = z;
    kernel::G<float>(model + 0x1ac) = radius;
}
