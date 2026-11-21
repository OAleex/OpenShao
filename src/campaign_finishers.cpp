#include "campaign_finishers.h"

#include "scorpion_preview_audio.h"
#include "survival_session.h"
#include "kernel/kernel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

X86_EXTERN_FUNC(__imp__sub_0016F560);
X86_EXTERN_FUNC(__imp__sub_0016F020);
X86_EXTERN_FUNC(__imp__sub_001F2E90);
X86_EXTERN_FUNC(__imp__sub_00013F85);
X86_EXTERN_FUNC(__imp__sub_00013382);
X86_EXTERN_FUNC(__imp__sub_000166B3);
X86_EXTERN_FUNC(__imp__sub_001DED00);
X86_EXTERN_FUNC(__imp__sub_00014903);
X86_EXTERN_FUNC(__imp__sub_0016E0E0);

namespace openshao::campaign_finishers {
namespace {
constexpr uint32_t CurrentRoom = 0x505970, CampaignState = 0x5ACEBC;
constexpr uint32_t World = 0x639224, PlayerObjects = 0x64D230;
constexpr uint32_t Clock = 0x5CE878, FinisherManager = 0x58A624, FinisherOwner = 0x58E4BC;
constexpr uint32_t CurrentMessage = 0x4E6E20 + 0x6C0;
constexpr uint32_t SquadHistory = 0x6C0168, Capacity = 0x4928, PlayerStride = 0x50;
constexpr uint32_t ActorTable = 0x5CE880, ActorStride = 0x3238;
constexpr unsigned RoomActorSlots = 16;
constexpr uint16_t PriestType = 27;
constexpr uint32_t MapAnimation = 0x1995B0, NativeVoiceHandle = 0x6C03DC;
constexpr uint32_t PickupDragon = 0xD17C902E; // MKFIRELOGO01
// Gate sequence pause lock, counted in Update calls (one per gameplay update).
constexpr uint32_t GateWaitFrames = 1200;   // ready -> gate animation; lifted if the gate never moves
constexpr uint32_t GateSettleFrames = 180;  // the gate animation and collision change
constexpr uint32_t GateVoiceFrames = 900;   // longest wait for the extra's gate line
constexpr int64_t PauseLockLease = 500;     // ms; renewed by every Update while the lock holds

struct Encounter {
    uint32_t room, tier, pickup_notice;
    std::string_view pickup, tutorial, full, gate, aftermath, respawn, second_respawn;
};
constexpr Encounter Monastery{194, 2, 88, "_d_rdn_multal", "_d_multal2", "_d_multal03",
                              "_d_gate01_off", "_d_multal04", "_d_respawn16", ""};
constexpr Encounter Forest{142, 3, 91, "_d_fatlvl3_b", "_d_fatlvl3_c", "_d_fat_full",
                           "_d_complete", "_d_win05", "_d_spawn17", "_d_spawn18"};

struct TutorialMessage {
    uint32_t action = 0;
    std::array<uint32_t, 3> arguments{};
};

struct Attempt {
    const Encounter* encounter = nullptr;
    uint32_t state = 0, world = 0, collected_at = 0;
    unsigned players = 0, extras = 0;
    std::array<uint32_t, 2> identities{};
    unsigned spawned_squads = 0;
    bool awarded = false, ready = false;
    bool gate_voice_pending = false, gate_voice_played = false;
    uint32_t lock_frame = 0, gate_frame = 0;
    bool lock_done = false;
    uint32_t notice = 0;
    std::array<TutorialMessage, 2> pending_messages{};
    unsigned pending_message_count = 0;
    bool AllExtras() const { return extras == (1u << players) - 1; }
    bool IsExtra(unsigned player) const { return (extras & (1u << player)) != 0; }
};
Attempt attempt;
uint32_t update_frames = 0;

bool ExtraFighter(uint32_t id) {
    return id == 83 || id == 84 || id == 89 || id == 90;
}

uint32_t Actor(unsigned player) { return kernel::R32(PlayerObjects + 12 * player); }

void PlayMonasteryGateVoice() {
    const auto actor = Actor(0);
    if (!actor || kernel::R32(actor + 0x68) != 1) return;
    using scorpion::audio_preview::PlayCampaignVoice;
    switch (kernel::R16(actor + 0x1DFE)) {
    case 83: PlayCampaignVoice(7081, 54); break; // vo_brka_st16_07_3
    case 84: PlayCampaignVoice(7214, 74); break; // vo_rpt_lf_17_05_12
    case 89: PlayCampaignVoice(7272, 21); break; // vo_kit_em3_laugh_long_2
    case 90: PlayCampaignVoice(7176, 11); break; // vo_jc_wu_map9_05
    }
}

const Encounter* CurrentEncounter() {
    const auto room = kernel::R32(CurrentRoom);
    if (room == Monastery.room) return &Monastery;
    if (room == Forest.room) return &Forest;
    return nullptr;
}

Attempt* CurrentAttempt() {
    if (!attempt.encounter || CurrentEncounter() != attempt.encounter ||
        attempt.state != kernel::R32(CampaignState) || attempt.world != kernel::R32(World) ||
        survival::ActiveSession()) return nullptr;
    if (kernel::R32(attempt.state + 0x88) != attempt.players - 1) {
        Reset();
        return nullptr;
    }
    for (unsigned player = 0; player < attempt.players; ++player) {
        if (kernel::R32(attempt.state + 0xCC + player * 4) != attempt.identities[player]) {
            // Loading another party can reuse the same state and world storage.
            Reset();
            return nullptr;
        }
        // Mixed parties can use temporary actors during the native cinematic.
        if (attempt.AllExtras()) {
            const auto actor = Actor(player);
            if (!actor || kernel::R16(actor + 0x1DFE) != attempt.identities[player]) {
                Reset();
                return nullptr;
            }
        }
    }
    return &attempt;
}

std::string_view TriggerName(uint32_t descriptor) {
    if (!descriptor || !mem::IsCommitted(descriptor + 0xAC) || !mem::IsCommitted(descriptor + 0xBB))
        return {};
    const auto* name = static_cast<const char*>(kernel::HostPtr(descriptor + 0xAC));
    const auto* end = static_cast<const char*>(std::memchr(name, '\0', 16));
    return {name, end ? size_t(end - name) : 16};
}

std::string_view ConditionTrigger(X86Context& ctx) {
    const auto instance = kernel::R32(ctx.ecx + 8);
    return instance ? TriggerName(kernel::R32(instance + 0xC)) : std::string_view{};
}

bool MeterBelowTier(const Attempt& current) {
    for (unsigned player = 0; player < current.players; ++player)
        if (current.IsExtra(player) &&
            kernel::R32(current.state + Capacity + (player + 1) * PlayerStride) < current.encounter->tier)
            return true;
    return false;
}

void Collected(const Encounter* encounter) {
    if (survival::ActiveSession()) return;
    if (const auto* existing = CurrentAttempt()) {
        // Exit Game, a level reload or a new campaign with the same fighter reuse the state and
        // world storage. An awarded attempt whose meter is back below the tier belongs to that
        // earlier run, so the pickup starts a new one.
        if (!existing->awarded || !MeterBelowTier(*existing)) return;
        Reset();
    }
    const auto state = kernel::R32(CampaignState), world = kernel::R32(World);
    if (!state || !world) return;
    const auto mode = kernel::R32(state + 0x88);
    if (mode > 1) return;
    unsigned extras = 0;
    std::array<uint32_t, 2> identities{};
    for (unsigned player = 0; player <= mode; ++player) {
        const auto actor = Actor(player);
        if (!actor || kernel::R32(actor + 0x68) != player + 1) return;
        // Apply the replacement only after selection and the loaded actor agree.
        const auto id = identities[player] = kernel::R32(state + 0xCC + player * 4);
        if (kernel::R16(actor + 0x1DFE) != id) return;
        if (ExtraFighter(id)) extras |= 1u << player;
        else if (id != 0 && id != 1 && id != 82 && id != 88) return;
    }
    if (extras) attempt = {encounter, state, world, kernel::R32(Clock), mode + 1, extras, identities};
}

bool Award(X86Context& ctx, const Attempt& current, unsigned player) {
    const auto actor = Actor(player);
    if (!actor || kernel::R32(actor + 0x68) != player + 1) return false;
    const auto at = current.state + Capacity + (player + 1) * PlayerStride;
    if (kernel::R32(at) >= current.encounter->tier) return false;
    const auto saved = ctx;
    kernel::CallGuest(ctx, 0x14903, {actor, current.encounter->tier});
    ctx = saved;
    return true;
}

bool AwardExtras(X86Context& ctx, Attempt& current) {
    if (current.awarded) return false;
    current.awarded = true;
    bool changed = false;
    for (unsigned player = 0; player < current.players; ++player)
        if (current.IsExtra(player)) changed |= Award(ctx, current, player);
    return changed;
}

bool SquadDefeated(unsigned group) {
    for (unsigned i = 0; i < 20; ++i) {
        const auto entry = SquadHistory + i * 12;
        if (kernel::R16(entry) == group) return kernel::R32(entry + 4) == 0;
    }
    return false;
}

bool Clear(const Attempt& current) {
    const auto& encounter = *current.encounter;
    const unsigned required = encounter.room == Forest.room ? 3 : 2;
    if ((current.spawned_squads & required) != required) return false;
    if (!SquadDefeated(16)) return false;
    if (encounter.room == Forest.room) return SquadDefeated(15);
    // The Monastery priest is activated separately from squad 16.
    for (unsigned slot = 3; slot < RoomActorSlots; ++slot) {
        const auto actor = ActorTable + slot * ActorStride;
        if (kernel::G<uint8_t>(actor + 0x70) || kernel::R16(actor + 0x1DFE) != PriestType) continue;
        const auto health = kernel::R32(actor + 0x200);
        if (health && mem::IsCommitted(health + 0x36B) && kernel::R32(health + 0x368)) return false;
    }
    return true;
}

// Pausing mid-sequence left the gate without collision but drawn closed.
bool GateSequence(Attempt& current) {
    if (current.lock_done) return false;
    if (!current.lock_frame) {
        const bool ready = current.ready || (current.AllExtras() && current.awarded && Clear(current));
        if (!ready) return false;
        current.lock_frame = update_frames;
    }
    bool hold;
    if (!current.gate_frame) {
        hold = update_frames - current.lock_frame < GateWaitFrames;
    } else {
        const auto since = update_frames - current.gate_frame;
        const bool voice = current.gate_voice_pending || scorpion::audio_preview::CampaignVoicePlaying();
        hold = since < GateSettleFrames || (voice && since < GateVoiceFrames);
    }
    if (!hold) current.lock_done = true;
    return hold;
}

uint32_t ScriptRoutine(uint32_t action) {
    const auto data = kernel::R32(action + 0x10);
    const auto callback = data ? kernel::R32(data + 0x10) : 0;
    const auto table = callback ? kernel::R32(callback) : 0;
    return table ? kernel::R32(table) : 0;
}
}

void Reset() {
    scorpion::audio_preview::StopCampaignVoice();
    attempt = {};
    pause_lock_until.store(0, std::memory_order_relaxed);
}

bool BlocksTutorialWhite(unsigned player) {
    const auto* current = CurrentAttempt();
    // A mixed encounter requires a finisher the extra's move registry cannot run.
    return current && player < current->players && !current->AllExtras() &&
        !current->ready && current->IsExtra(player);
}

void Update(X86Context& ctx) {
    ++update_frames;
    auto* current = CurrentAttempt();
    const bool locked = current && GateSequence(*current);
    pause_lock_until.store(locked ? PauseLockNow() + PauseLockLease : 0, std::memory_order_relaxed);
    if (!current || kernel::G<uint8_t>(CurrentMessage) ||
        kernel::G<uint8_t>(0x5AE5B0) || kernel::G<uint8_t>(0x6C4198) || kernel::G<uint8_t>(0x514505) ||
        kernel::R32(0x6C92F8) || kernel::R32(0x6C9308) || kernel::R32(0x5ACEDC)) return;
    if (current->notice) {
        const auto notice = current->notice;
        current->notice = 0;
        survival::ShowStoryNotice(ctx, notice);
    } else if (const auto count = current->pending_message_count) {
        const auto messages = current->pending_messages;
        current->pending_message_count = 0;
        const auto saved = ctx;
        for (unsigned i = 0; i < count; ++i) {
            const auto& message = messages[i];
            ctx.ecx = message.action;
            kernel::CallGuest(ctx, 0x1F2E90, message.arguments.data(), message.arguments.size());
            ctx = saved;
        }
    } else if (current->gate_voice_pending && !kernel::R32(NativeVoiceHandle)) {
        current->gate_voice_pending = false;
        current->gate_voice_played = true;
        PlayMonasteryGateVoice();
    }
}
}

using namespace openshao::campaign_finishers;

X86_FUNC(sub_0016F020) {
    Reset();
    __imp__sub_0016F020(ctx, base);
}

X86_FUNC(sub_0016F560) {
    const auto* encounter = CurrentEncounter();
    const bool pickup = encounter && TriggerName(kernel::R32(ctx.ecx + 0xC)) == encounter->pickup;
    __imp__sub_0016F560(ctx, base);
    if (pickup && (ctx.eax & 0xFF)) Collected(encounter);
}

X86_FUNC(sub_0016E0E0) {
    const auto* current = CurrentAttempt();
    const auto trigger = ctx.ecx;
    if (!current || !current->AllExtras()) {
        __imp__sub_0016E0E0(ctx, base);
        return;
    }
    const auto& room = *current->encounter;
    const auto name = TriggerName(kernel::R32(trigger + 0xC));
    const bool tutorial = name == room.tutorial;
    const bool forest_gate = room.room == Forest.room && name == room.gate && current->ready;
    if (!tutorial && !forest_gate) {
        __imp__sub_0016E0E0(ctx, base);
        return;
    }
    const auto count = kernel::R32(trigger + 0x14), times = kernel::R32(trigger + 0xC4);
    std::array<uint32_t, 16> authored{};
    if (!count || count > authored.size() || !times || !mem::IsCommitted(times) ||
        !mem::IsCommitted(times + count * 4 - 1)) {
        __imp__sub_0016E0E0(ctx, base);
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        authored[i] = kernel::R32(times + i * 4);
        if (tutorial) {
            // The extra-only path skips the pickup cinematic.
            kernel::W32(times + i * 4, std::min(authored[i], 30u));
        } else if (authored[i] == 900 || authored[i] == 902) {
            // The Forest gate waits for the Brutality cinematic: keep its actions in their original order.
            kernel::W32(times + i * 4, authored[i] - 870);
        }
    }
    __imp__sub_0016E0E0(ctx, base);
    for (uint32_t i = 0; i < count; ++i) kernel::W32(times + i * 4, authored[i]);
}

X86_FUNC(sub_001F2E90) {
    auto* current = CurrentAttempt();
    if (current) {
        const auto name = TriggerName(kernel::R32(ctx.esp + 8));
        const auto& room = *current->encounter;
        if (name == room.pickup || name == room.tutorial || name == room.gate || name == room.aftermath) {
            const auto routine = ScriptRoutine(ctx.ecx);
            if (name == room.gate && routine == MapAnimation && !current->gate_frame)
                current->gate_frame = std::max(update_frames, 1u);
            if (&room == &Monastery && name == room.gate && routine == MapAnimation &&
                current->ready && current->players == 1 && !current->gate_voice_played) {
                __imp__sub_001F2E90(ctx, base);
                if (CurrentAttempt() == current) current->gate_voice_pending = true;
                return;
            }
            if (name == room.tutorial && current->AllExtras()) {
                const auto args = kernel::R32(ctx.ecx + 8);
                if (routine == 0x1D2710) {
                    const auto squad = args ? kernel::R32(args) : 0;
                    if (squad == 15 || squad == 16) current->spawned_squads |= 1u << (squad - 15);
                }
                // The pickup script starts the dragon separately from the cinematic.
                if (routine == MapAnimation && args && kernel::R32(args) == 1 &&
                    kernel::R32(args + 4) == PickupDragon) {
                    kernel::Return(ctx, 0, 3);
                    return;
                }
            }
            if (name == room.tutorial && routine == 0x1116D) {
                if (current->AllExtras()) {
                    if (AwardExtras(ctx, *current)) current->notice = AcquiredLabel;
                } else {
                    for (unsigned player = 0; player < current->players; ++player)
                        if (!current->IsExtra(player)) Award(ctx, *current, player);
                }
                kernel::Return(ctx, 0, 3);
                return;
            }
            if (name == room.tutorial && routine == 0x12847 && !current->AllExtras()) {
                const auto args = kernel::R32(ctx.ecx + 8);
                if (args && kernel::R32(args + 4) == 6) {
                    if (kernel::R32(args) == room.pickup_notice && AwardExtras(ctx, *current))
                        current->notice = AllyAcquiredLabel;
                    // The native prompt changes the active message's state when queued.
                    // Hold the tutorial prompts until the ally's frame has been closed.
                    if ((current->notice == AllyAcquiredLabel || current->pending_message_count) &&
                        current->pending_message_count < current->pending_messages.size()) {
                        current->pending_messages[current->pending_message_count++] = {ctx.ecx, {
                            kernel::R32(ctx.esp + 4), kernel::R32(ctx.esp + 8), kernel::R32(ctx.esp + 12)}};
                        kernel::Return(ctx, 0, 3);
                        return;
                    }
                }
            }
            if (current->AllExtras() &&
                ((name == room.pickup && (routine == 0x114D3 || routine == 0x14985 || routine == 0x1564B)) ||
                 (name == room.tutorial && (routine == 0x12847 || routine == 0x1296E)) ||
                 ((name == room.gate || name == room.aftermath) && routine == 0x12847))) {
                kernel::Return(ctx, 0, 3);
                return;
            }
        }
    }
    __imp__sub_001F2E90(ctx, base);
}

X86_FUNC(sub_00013F85) {
    const auto* current = CurrentAttempt();
    if (current && current->AllExtras() && ConditionTrigger(ctx) == current->encounter->tutorial) {
        kernel::Return(ctx, 1, 2);
        return;
    }
    __imp__sub_00013F85(ctx, base);
}

X86_FUNC(sub_00013382) {
    const auto* current = CurrentAttempt();
    if (current && ConditionTrigger(ctx) == current->encounter->full && current->AllExtras()) {
        kernel::Return(ctx, 0, 2);
        return;
    }
    __imp__sub_00013382(ctx, base);
}

X86_FUNC(sub_001DED00) {
    const auto* current = CurrentAttempt();
    if (current && current->AllExtras()) {
        const auto name = ConditionTrigger(ctx);
        if (name == current->encounter->respawn || name == current->encounter->second_respawn) {
            kernel::Return(ctx, 0, 2);
            return;
        }
    }
    __imp__sub_001DED00(ctx, base);
}

X86_FUNC(sub_000166B3) {
    auto* current = CurrentAttempt();
    if (!current || kernel::R32(ctx.ecx + 4) != current->encounter->tier) {
        __imp__sub_000166B3(ctx, base);
        return;
    }
    const auto name = ConditionTrigger(ctx);
    const auto& room = *current->encounter;
    if (name != room.gate && name != room.aftermath) {
        __imp__sub_000166B3(ctx, base);
        return;
    }
    if (current->AllExtras()) {
        if (!current->ready) current->ready = current->awarded && Clear(*current);
        // The Monastery aftermath is a separate prompt for the original finisher.
        kernel::Return(ctx, current->ready && (room.room == Forest.room || name == room.gate), 2);
        return;
    }
    if (current->ready) {
        kernel::Return(ctx, 1, 2);
        return;
    }
    const auto manager = kernel::R32(FinisherManager);
    const auto owner = kernel::R32(FinisherOwner);
    const bool story_completion = manager && owner < current->players && !current->IsExtra(owner) &&
        kernel::R32(manager + 0x1A4) >= current->collected_at;
    __imp__sub_000166B3(ctx, base);
    if (!(ctx.eax & 0xFF)) return;
    if (!story_completion) {
        ctx.eax &= ~0xFFu;
        return;
    }
    current->ready = true;
}

X86_FUNC(sub_00014903) {
    const auto* current = CurrentAttempt();
    if (current && !current->AllExtras() && !current->ready && kernel::R32(ctx.esp) == 0xA5A21) {
        const auto actor = kernel::R32(ctx.esp + 4);
        for (unsigned player = 0; player < current->players; ++player) {
            if (current->IsExtra(player) && actor == Actor(player)) {
                kernel::Return(ctx, 0, 0);
                return;
            }
        }
    }
    __imp__sub_00014903(ctx, base);
}

