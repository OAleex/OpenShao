#include "kontent_menu.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"

namespace openshao::kontent {
static_assert(KontentRegion(NorthAmerica) == RestOfWorld && KontentRegion(RestOfWorld) == RestOfWorld && KontentRegion(2) == 2);
void HideDemo() {
    const uint32_t region = kernel::R32(GameRegion);
    if (KontentRegion(region) != region) kernel::W32(GameRegion, KontentRegion(region));
}
}

// 0x41190 selects the Kontent movie (0x4E2C3C) and registers its clips once, from the cached region.
X86_EXTERN_FUNC(__imp__sub_00013175);
X86_FUNC(sub_00013175) {
    openshao::kontent::HideDemo();
    __imp__sub_00013175(ctx, base);
}
