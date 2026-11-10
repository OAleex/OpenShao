#include "shadow_depth_format.h"

#ifdef _WIN32
namespace openshao {
// Offscreen D16/F16 use D32 (avoids bad D24S8 sampling); screen-sized and stencil formats keep D24S8.
bool FloatShadowDepth(bool supported, uint32_t guest_format, bool offscreen) {
    return supported && offscreen && (guest_format == 0x2C || guest_format == 0x2D || guest_format == 0x30 || guest_format == 0x31);
}

ShadowDepthFormats ShadowFormats(DXGI_FORMAT resource) {
    if (resource == DXGI_FORMAT_R32_TYPELESS)
        return {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, false};
    return {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, true};
}

}
#endif
