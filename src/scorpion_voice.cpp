#include "scorpion_voice.h"
#include "scorpion_preview_audio.h"
#include "launcher.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <random>

namespace {
bool preview_active = false;
std::array<unsigned, 2> preview_index{};

bool ScorpionSpearQueue(uint32_t actor) {
    if (!actor || actor > UINT32_MAX - 0x234F ||
        !mem::IsCommitted(actor + 0x234F) || !mem::IsCommitted(actor + 0x23E8) ||
        !mem::IsCommitted(actor + 0x23EB)) return false;
    // Native recognition uses only the first queued sound (0x17E81E-0x17E82A).
    const auto queued = kernel::G<uint8_t>(actor + 0x234F);
    return queued > 0 && queued <= 24 && kernel::R32(actor + 0x23E8) == openshao::scorpion::Lines[0];
}

bool ScorpionCombatSoundCaller(uint32_t caller) {
    switch (caller) {
    // These are the six native 3D sound call sites in the combat dispatcher.
    case 0x17E3BD: case 0x17E438: case 0x17E681:
    case 0x17E71A: case 0x17E797: case 0x17E956: return true;
    default: return false;
    }
}
}

namespace openshao::scorpion {
void StopPreview(X86Context&) {
    if (preview_active) audio_preview::Stop();
    preview_active = false;
}

bool Preview(X86Context& ctx, bool enabled) {
    StopPreview(ctx);
    const unsigned pair = enabled ? 1u : 0u;
    const unsigned next = preview_index[pair] % 2;
    // AFS stores the common pair before the rare pair. Read installed game data
    // because the combat bank may not be loaded in the main menu.
    if (!audio_preview::Play(pair * 2 + next)) return false;
    preview_active = true;
    preview_index[pair] = 1 - next;
    return true;
}
}

X86_EXTERN_FUNC(__imp__sub_00151B90);
X86_FUNC(sub_00151B90) {
    const auto caller = kernel::R32(ctx.esp);
    const auto slot = ctx.esp + 8;
    const auto original = kernel::R32(slot);
    const bool enabled = openshao::launcher::Current().scorpion_unfiltered;
    const bool recognized = std::find(openshao::scorpion::Lines.begin(), openshao::scorpion::Lines.end(), original) != openshao::scorpion::Lines.end();
    const bool combat = ScorpionCombatSoundCaller(caller);
    const bool valid_queue = !enabled || !combat || !recognized || caller != 0x17E956 || ScorpionSpearQueue(ctx.esi);
    // Combat dispatch can run before the full voice queue has been built.
    if (enabled && combat && recognized && valid_queue) {
        // Keep this draw independent from the game's random stream and combat simulation.
        static thread_local std::mt19937 random(std::random_device{}());
        // Common lines: 5% each. Rare lines: 50% and 40%.
        std::uniform_int_distribution<unsigned> choose(0, 99);
        const auto draw = choose(random);
        const auto index = draw < 5 ? 0u : draw < 10 ? 1u : draw < 60 ? 2u : 3u;
        kernel::W32(slot, openshao::scorpion::Lines[index]);
    }
    __imp__sub_00151B90(ctx, base);
    kernel::W32(slot, original);
}
