#include "ed_nexus.h"
#include "survival_mode.h"
#include "survival_session.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "ed_nexus_hooks.inl"
#include "controller.h"
#include "difficulty.h"
#include "core/log.h"

// Debug Player 2: game mode at state+0x88 (0 solo, 1 Ko-op, 2 Versus); 0xCB690 shows the second hero from it.
// Switching changes the mode, player count (0x633038), ports (0xD46D0) and health split (0xD4790) at the next map load.
namespace openshao::ed_nexus {
namespace {
constexpr uint32_t LiveState = 0x5ACEBC, PlayerCount = 0x633038;
uint32_t campaign_mode = ~0u, saved_live_mode = ~0u;
bool rebuild_hud = false;
// HUD layout by mode (0x921D0): clear the menu widgets (0x152A9 on 0x4EF3C8) and rebuild (0x15060).
void RebuildHud(X86Context& ctx) {
    const auto saved = ctx;
    ctx.ecx = 0x4EF3C8;
    kernel::CallGuest(ctx, 0x152A9, {0});
    ctx = saved;
    kernel::CallGuest(ctx, 0x15060, {});
    ctx = saved;
}
uint32_t State() { return kernel::R32(LiveState); }
void ApplyPlayer2(X86Context& ctx) {
    if (!player2_pending) return;
    const uint32_t state = State();
    if (!state) return;
    player2_pending = false;
    const uint32_t current = kernel::R32(state + 0x88), wanted = player2_target ? 1 : 0;
    if (current > 1 || current == wanted) { if (kontent_menu) WritePlayer2Text(); return; }
    if (campaign_mode == ~0u) campaign_mode = current;
    kernel::W32(state + 0x88, wanted);
    kernel::W32(PlayerCount, wanted ? 2 : 1);
    const auto saved = ctx;
    kernel::CallGuest(ctx, 0x1271B, {});  // controller ports for one or two players
    ctx = saved;
    // Ko-op splits the health pool between the heroes; refill at the new maximum.
    openshao::difficulty::SetDebugRefill(true);
    kernel::CallGuest(ctx, 0x12D65, {kernel::R32(state + 0x90)});
    openshao::difficulty::SetDebugRefill(false);
    ctx = saved;
    openshao::controller::SetMultiplayer(wanted == 1);
    rebuild_hud = true;
    if (wanted == campaign_mode) campaign_mode = ~0u;
    if (kontent_menu) WritePlayer2Text();
    LOGI("Debug player 2 %s for this map", wanted ? "joined" : "left");
}
}
bool Player2Active() { const uint32_t state = State(); return state && kernel::R32(state + 0x88) == 1; }
void Player2BeforeSave() {
    const uint32_t state = State();
    if (campaign_mode == ~0u || !state) return;
    saved_live_mode = kernel::R32(state + 0x88);
    kernel::W32(state + 0x88, campaign_mode);
}
void Player2AfterSave() {
    const uint32_t state = State();
    if (saved_live_mode != ~0u && state) kernel::W32(state + 0x88, saved_live_mode);
    saved_live_mode = ~0u;
}
void ResetPlayer2() {
    player2_pending = false;
    if (campaign_mode == ~0u) return;
    // The restored or new state carries the campaign's own mode; follow it again.
    kernel::W32(PlayerCount, campaign_mode == 1 ? 2 : 1);
    openshao::controller::SetMultiplayer(campaign_mode == 1);
    campaign_mode = ~0u;
    LOGI("Debug player 2 override cleared");
}
}

X86_EXTERN_FUNC(__imp__sub_00016ACD);
X86_FUNC(sub_00016ACD) {
    // Every room change passes through this loader (thiscall, one argument).
    if (openshao::survival::InterceptFoundryPortal(ctx, kernel::R32(ctx.esp + 4))) {
        kernel::Return(ctx, 0, 1);
        return;
    }
    openshao::survival::RoomLoading();
    openshao::ed_nexus::ApplyPlayer2(ctx);
    __imp__sub_00016ACD(ctx, base);
    openshao::survival::RoomReady(ctx);
    if (openshao::ed_nexus::rebuild_hud) {
        openshao::ed_nexus::rebuild_hud = false;
        openshao::ed_nexus::RebuildHud(ctx);
    }
}
