#include "gpu/shaders.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {
void Check(bool passed, const char* description) {
    if (!passed) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}
bool SameTexture(const gpu::TexInfo& a, const gpu::TexInfo& b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}
gpu::State Programmable() {
    gpu::State state{};
    state.ps = std::make_shared<gpu::PixelShader>();
    state.ps->def[53] = 1;
    return state;
}
}

int main() {
    using namespace gpu;
    const TexInfo empty{}, ordinary{kTex2D, 0, 0, 0}, shadow{kTex2D, 1, 1, 0};
    {
        auto state = Programmable();
        state.rs[kRsPsTextureModes] = 1;
        TexInfo baseline[4]{ordinary}, inherited[4]{ordinary, empty, empty, shadow};
        const auto without_shadow = MakePixelKey(state, baseline);
        state.tss[3][kTssAlphaKill] = 4;
        const auto with_shadow = MakePixelKey(state, inherited);
        Check(with_shadow == without_shadow,
              "disabled inherited stage-three shadow and alpha-kill must not create another pipeline key");
        Check(PixelHlsl(with_shadow) == PixelHlsl(without_shadow),
              "disabled inherited bindings must generate the same pixel shader source");
        Check(SameTexture(with_shadow.tex[0], ordinary), "active stage-zero texture classification is retained");
        Check(SameTexture(with_shadow.tex[3], empty) && with_shadow.alphakill == 0,
              "disabled stage metadata is cleared in the finalized programmable key");
    }
    {
        auto state = Programmable();
        state.rs[kRsPsTextureModes] = 1u << 15;
        state.tss[3][kTssAlphaKill] = 4;
        TexInfo textures[4]{empty, empty, empty, shadow};
        const auto key = MakePixelKey(state, textures);
        Check(((key.tex_modes >> 15) & 31) == 2 && SameTexture(key.tex[3], shadow),
              "active stage-three projected shadow preserves its lowered mode and texture classification");
        Check(key.alphakill == 8, "active projected-shadow alpha-kill remains enabled");
    }
    {
        auto state = Programmable();
        state.rs[kRsPsTextureModes] = 1u << 15;
        state.tss[3][kTssAlphaKill] = 4;
        TexInfo bound[4]{empty, empty, empty, ordinary}, unbound[4]{};
        const auto active = MakePixelKey(state, bound), disabled = MakePixelKey(state, unbound);
        Check(((active.tex_modes >> 15) & 31) == 1 && SameTexture(active.tex[3], ordinary),
              "bound ordinary mode-one texture must survive lowering before canonicalization");
        Check(((disabled.tex_modes >> 15) & 31) == 0 && SameTexture(disabled.tex[3], empty) && disabled.alphakill == 0,
              "unbound mode-one stage lowers to disabled before its metadata is cleared");
        Check(!(active == disabled), "active and disabled stage-three shaders remain distinct");
    }
    for (uint32_t mode : {4u, 5u, 10u, 17u}) {
        auto state = Programmable();
        state.rs[kRsPsTextureModes] = mode << 15;
        state.tss[3][kTssAlphaKill] = 4;
        TexInfo textures[4]{empty, empty, empty, shadow};
        const auto key = MakePixelKey(state, textures);
        Check(((key.tex_modes >> 15) & 31) == mode && SameTexture(key.tex[3], shadow) && key.alphakill == 8,
              "passthrough, clip and DOT texture modes retain their original key metadata");
    }
    {
        State state{};
        state.tss[0][kTssColorOp] = 4;
        state.tss[0][kTssAlphaOp] = 4;
        state.tss[3][kTssAlphaKill] = 4;
        TexInfo inherited[4]{ordinary, empty, empty, shadow}, baseline[4]{ordinary};
        const auto key = MakePixelKey(state, inherited);
        Check(!key.programmable && SameTexture(key.tex[0], ordinary) && SameTexture(key.tex[3], shadow),
              "fixed-function texture classification is unchanged");
        Check(key.alphakill == 8 && !(key == MakePixelKey(state, baseline)),
              "fixed-function inherited bindings and alpha-kill retain existing behavior");
    }
    std::puts("PASS: programmable disabled-stage pipeline key canonicalization");
}
