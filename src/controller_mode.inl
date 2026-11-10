#include "game_preferences.h"
#include "recomp_settings.h"
#include "ed_nexus.h"
#include "survival_mode.h"
// Native mode: 0 solo, 1 Ko-op, 2 Versus; read from the guest stack before Poll.
void SyncRetailFrontendMode() {
    // Effects are gated per player at this mask: use the saved host preference.
    const auto flags=kernel::R32(0x639674);
    const auto desired=(flags&~0xFu)|(openshao::controller::RumbleEnabled()?0xFu:0u);
    if(flags!=desired)kernel::W32(0x639674,desired);
    const uint32_t depth=kernel::R32(0x6C9308);
    if (!depth || depth>16) return;
    const uint32_t top=kernel::R32(0x6C9308+depth*4);
    if (top==0x598430 || top==0x5967C0) openshao::controller::SetMultiplayer(false);
    else if (top==0x598DF0) {
        // Survival keeps Main on Versus (2) for the roster, also while its popup opens (else ports remap and Back closes the roster).
        const bool survival=openshao::survival::CharacterSelection() || openshao::survival::SetupPopup();
        const uint32_t selected=survival?openshao::survival::PlayerMode():kernel::R32(0x598430+0x38);
        openshao::controller::SetMultiplayer(selected==1 || selected==2);
    }
    // Ko-op saves need a second Start source until the native mode setter runs.
    else if (top==0x596810 && kernel::R32(top+0x3C)==6)
        openshao::controller::SetMultiplayer(true);
}
X86_EXTERN_FUNC(__imp__sub_00015C86);
X86_FUNC(sub_00015C86) {
    const uint32_t menu=ctx.ecx;
    openshao::ed_nexus::FrontendInput(ctx,base);
    if(openshao::recomp::MainInput(ctx,base)){
        openshao::recomp::RestoreMainSelector();
        return;
    }
    __imp__sub_00015C86(ctx,base);
    openshao::recomp::RestoreMainSelector();
    LOG_FIRST(4,"input: retail menu callback this=%08X",menu);
    if (menu!=0x598430) return;
    openshao::preferences::Apply(ctx, base);
    const uint32_t depth=kernel::R32(0x6C9308);
    if (!depth || depth>16) return;
    const uint32_t top=kernel::R32(0x6C9308+depth*4);
    static uint64_t last_menu=~uint64_t{};
    const uint64_t state=(uint64_t(top)<<32)|kernel::R32(menu+0x38);
    if (state!=last_menu) {LOGI("input: retail menu top=%08X selected=%u depth=%u",top,uint32_t(state),depth);last_menu=state;}
    SyncRetailFrontendMode();
}
X86_EXTERN_FUNC(__imp__sub_0001670D);
X86_EXTERN_FUNC(__imp__sub_001B5070);
X86_FUNC(sub_001B5070) {
    // The Versus roster entry needs two devices; Survival solo relaxes only this query.
    const auto depth=kernel::R32(0x6C9308);
    const auto top=depth && depth<=16?kernel::R32(0x6C9308+depth*4):0;
    const bool solo_entry=kernel::R32(ctx.esp)==0x48EBD &&
        openshao::survival::CharacterSelection() && openshao::survival::PlayerMode()==0 &&
        top==0x598430;
    __imp__sub_001B5070(ctx,base);
    if(solo_entry && ctx.eax<2)ctx.eax=2;
}
X86_FUNC(sub_0001670D) {
    const uint32_t mode=kernel::R32(ctx.esp+4);
    openshao::ed_nexus::ResetPlayer2();
    __imp__sub_0001670D(ctx,base);
    if (mode<=2) openshao::controller::SetMultiplayer(mode!=0);
}
