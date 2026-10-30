#include "dialogue_queue.h"
#include "dialogue_recovery_catalog.h"
#include "ed_nexus.h"
#include "launcher.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <chrono>
#include <array>

// vosound (0x1A18F0) drops requests while its single voice slot is busy.
// Defer those requests until the native voice update releases the slot.
namespace {
constexpr uint32_t VoiceHandle = 0x6C03DC, CharacterBank = 0x6C03D8, SoundManager = 0x653BC0;
constexpr uint32_t VoiceRequest = 0x653C68, CurrentRoom = 0x505970;
constexpr uint32_t Players = 0x5D1AB8, PlayerStride = 0x3238, ActorCount = 14;
constexpr uint32_t ActorFlag = 0x1DFE, PlayerBank = 0x2429;
constexpr uint32_t CageRoom = 98, CageScene = 0x2D2875B0;
constexpr uint32_t SceneName = 0x514508, SceneLoadingOrPlaying = 0x514505;
constexpr uint32_t World = 0x639224, MessageManager = 0x4E6E20;

openshao::dialogue::Queue queue;
std::chrono::steady_clock::time_point last_update{};
bool Enqueue(const openshao::dialogue::Line& line) {
    const bool empty = !queue.Size();
    if (!queue.Push(line)) return false;
    // A new queue must not inherit elapsed loading time from the previous update.
    if (empty) last_update = std::chrono::steady_clock::now();
    return true;
}
struct CageCaption {
    uint32_t room = 0, world = 0;
    bool pending = false, delivered = false;
} cage;

struct MissingCue { uint32_t room, target, sound; int16_t character; };
// These activatetrigger targets have ordinal -1 and no definition; retain their script timing.
constexpr std::array<MissingCue, 7> MissingCues{{
    {101, 0xC1CE6AC6, 0x7CE0B357, 0}, {101, 0x0EAC5D51, 0xE0967133, 1},
    {101, 0xFB84C07D, 0x3B40C987, 0}, {101, 0x2326CE4B, 0xDDF65883, 1},
    {101, 0xCB402320, 0x0620E037, 0}, {101, 0x6465CF88, 0xA7360BE3, 1},
    {96, 0xB001C237, 0x822C1D1F, -1},
}};
std::array<uint16_t, MissingCues.size()> recovered_cues{};
uint32_t cue_room = 0, cue_world = 0;
uint16_t kitana_trap = 0;
uint16_t escape_warning = 0;
uint32_t kitana_variation = 0, shang_variation = 0;
bool kitana_seeded = false;
constexpr std::array<uint32_t, 3> KitanaLaughs{0xCE417166, 0x3B6D9293, 0x8109A438};
constexpr std::array<uint32_t, 2> ShangTaunts{0x415F052D, 0x13B9E084};
struct StreamFamily { uint32_t original, group; };
constexpr std::array<StreamFamily, 2> StreamFamilies{{
    {0x8D15F4C2, 0xB16C28BE}, {0xD77E5AAC, 0xEED9A6D2},
}};
struct VariationCursor { uint32_t next = 0; bool seeded = false; };
std::array<VariationCursor, StreamFamilies.size()> stream_variations{};
uint32_t stream_world = 0, stream_room = 0;
struct CaptionFamily {
    uint32_t room, text, original, recovered;
    uint32_t alternate = 0;
    bool original_usable = true;
};
constexpr std::array<CaptionFamily, 12> CaptionFamilies{{
    {101, 102, 0, 0x83D60341}, {108, 15, 0, 0x36E6C19B},
    {101, 14, 0x39D84601, 0x0C96F6E1}, {99, 11, 0x77ACBBDF, 0x73D3D82C},
    {99, 13, 0x43181561, 0xFC932D8C}, {142, 91, 0x24FB3EAC, 0x199B171C},
    {99, 98, 0xF5FC2C0E, 0x09138B4C}, {101, 103, 0xC58C67CF, 0xBEB62AF1},
    {137, 64, 0, 0xE555529C}, {46, 81, 0xAD4AC829, 0x902AE199},
    {100, 99, 0xC5B98BD2, 0xA5974565},
    {128, 62, 0x901D2B23, 0xFF90543D, 0xC2F07D8D, false},
}};
std::array<VariationCursor, CaptionFamilies.size()> caption_choices{};
uint32_t caption_world = 0, caption_room = 0;
struct ContextSelection {
    VariationCursor cursor;
    uint32_t seen = 0, actor_id = 0;
    int16_t actor_type = -2;
    int8_t actor_bank = -128;
};
std::array<std::array<ContextSelection, ActorCount>, std::size(openshao::dialogue::ContextFamilies)> context_choices{};
uint32_t context_world = 0, context_room = 0;
bool baraka_coop_seen = false;
uint32_t baraka_coop_actor = 0;
struct ContextChoice { ContextSelection* selection = nullptr; uint32_t sound = 0, take = 0; bool once = false; };
struct ActivationCue { uint32_t room, target, ordinal, sound; int16_t actor; };
constexpr ActivationCue ActivationCues[]{
    {96,0x30A4AFE0,0xFFFFFFFF,0x2AD7B0C5,-1},
};
std::array<uint16_t, std::size(ActivationCues)> activation_seen{};
// Level scripts emit Ermac's phase events. Retain them through cinematic queue
// cleanup, then bind to his live actor and confirm playback before marking delivery.
struct ErmacPhaseVoice {
    bool requested = false, ready = false, queued = false, delivered = false;
};
std::array<ErmacPhaseVoice, 2> ermac_phase_voices{};
uint32_t ermac_world = 0, ermac_room = 0;
constexpr std::array<uint32_t, 2> ErmacPhaseSounds{0x14DC5B86u, 0x29BC7236u};
uint32_t phase_world = 0, phase_room = 0;
bool capture_warning = false, freeing_warning = false;
uint32_t followup_world = 0, followup_room = 0, followup_seen = 0;
uint32_t forest_hint_world = 0, forest_hint_room = 0;
bool forest_hint_responded = false;
struct SupplementalCue {
    uint32_t world = 0, room = 0;
    bool observed = false, delivered = false, finished = false;
};
SupplementalCue monastery_intro, forest_entrance, forest_second_tree,
    shrine_offering, wu_shi_fire, mileena_trio_opening, scorpion_combat,
    monastery_fist, foundry_bridge, forest_snakes;
uint32_t foundry_bridge_ticks = 0;
bool forest_snakes_triggered = false;
// Keep the _d_snakes handle: the five-slot registry can clear its tag before the next tick.
uint32_t forest_snakes_stream_handle = 0;
bool forest_snakes_cutscene_seen = false;
struct MileenaMonasteryCombat {
    uint32_t world = 0, room = 0, pending = 0, mileena_id = 0;
    uint8_t next = 0;
    bool canceled = false;
} mileena_monastery_combat;
uint32_t native_world = 0, native_room = 0;
VariationCursor spear_choice, fire_choice, shao_choice;
bool shao_challenge = false;

struct ScopedGuestWord {
    uint32_t address, value;
    ScopedGuestWord(uint32_t at, uint32_t replacement) : address(at), value(kernel::R32(at)) {
        kernel::W32(address, replacement);
    }
    ~ScopedGuestWord() { kernel::W32(address, value); }
};

bool RestorationEnabled() { return openshao::launcher::Current().restore_dialogue; }

uint32_t CaptionVoiceHandle() {
    if (!kernel::G<uint8_t>(MessageManager + 0x6C0)) return 0;
    const uint32_t handle = kernel::R32(MessageManager + 0x6C8);
    // Reserve the slot between caption submission and native voice creation.
    return handle ? handle : kernel::R32(MessageManager + 0x6C4) ? 0xFFFFFFFFu : 0;
}

int32_t FindGroupBank(uint32_t sound) {
    const uint32_t count = kernel::R32(SoundManager + 0xC), banks = kernel::R32(SoundManager + 0x14);
    if (!banks || count > 256) return -1;
    for (uint32_t bank = 0; bank < count; ++bank) {
        const uint32_t entry = banks + bank * 24, groups = kernel::R32(entry + 12);
        if (!kernel::R32(entry + 4) || !groups) continue;
        const uint32_t size = kernel::R32(groups);
        if (size > 4096) continue;
        uint32_t group = groups + 4;
        for (uint32_t i = 0; i < size; ++i) {
            const uint32_t choices = kernel::R32(group + 4);
            if (!choices || choices > 1024) break;
            if (kernel::R32(group) == sound) return int32_t(bank);
            group += 12 + choices * 4;
        }
    }
    return -1;
}

bool HasSoundDefinition(uint32_t sound) {
    const uint32_t count = kernel::R32(SoundManager + 0xC), banks = kernel::R32(SoundManager + 0x14);
    if (!banks || count > 256) return false;
    for (uint32_t bank = 0; bank < count; ++bank) {
        const uint32_t entry = banks + bank * 24, header = kernel::R32(entry + 4), definitions = kernel::R32(entry + 8);
        if (!header || !definitions) continue;
        const uint32_t size = kernel::R32(header + 0x18);
        if (size > 8192) continue;
        for (uint32_t i = 0; i < size; ++i)
            if (kernel::R32(definitions + i * 28) == sound) return true;
    }
    return false;
}

// Some character sounds bypass vosound. Limit the speaker correction to
// Liu's standalone take in Baraka's encounter, excluding cinematic audio.
bool IsBarakaSoloKungLao() {
    if (!RestorationEnabled() || !kernel::R32(World) || kernel::R32(CurrentRoom) != 15 ||
        kernel::G<int16_t>(Players + ActorFlag) != 1) return false;
    const uint32_t mode = kernel::R32(0x5ACEBC);
    return mode && kernel::R32(mode + 0x88) == 0;
}

bool HasBankSoundDefinition(uint32_t bank, uint32_t sound) {
    if (bank >= kernel::R32(SoundManager + 0xC) || bank >= 256) return false;
    const auto entry = kernel::R32(SoundManager + 0x14) + bank * 24;
    const auto header = kernel::R32(entry + 4), definitions = kernel::R32(entry + 8);
    if (!header || !definitions || kernel::R32(header + 0x18) > 8192) return false;
    for (uint32_t i = 0; i < kernel::R32(header + 0x18); ++i)
        if (kernel::R32(definitions + i * 28) == sound) return true;
    return false;
}

uint32_t FindBankGroup(uint32_t bank, uint32_t sound) {
    if (bank >= kernel::R32(SoundManager + 0xC) || bank >= 256) return 0;
    const auto entry = kernel::R32(SoundManager + 0x14) + bank * 24;
    const auto groups = kernel::R32(entry + 12);
    if (!kernel::R32(entry + 4) || !groups || kernel::R32(groups) > 4096) return 0;
    uint32_t group = groups + 4;
    for (uint32_t i = 0; i < kernel::R32(groups); ++i) {
        const auto count = kernel::R32(group + 4);
        if (!count || count > 1024) return 0;
        if (kernel::R32(group) == sound) return group;
        group += 12 + count * 4;
    }
    return 0;
}

uint32_t ChoiceIndex(X86Context& ctx, VariationCursor& cursor, uint32_t count) {
    if (!cursor.seeded) {
        const auto saved = ctx;
        cursor.next = kernel::CallGuest(ctx, 0x15A430, {0});
        ctx = saved;
        cursor.seeded = true;
    }
    return cursor.next % count;
}

void RefreshNativeEncounter() {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (native_world == world && native_room == room) return;
    spear_choice = fire_choice = shao_choice = {};
    shao_challenge = false;
    native_world = world;
    native_room = room;
}

uint32_t ChooseGroupSound(X86Context& ctx, uint32_t sound, VariationCursor& cursor) {
    const int32_t bank = FindGroupBank(sound);
    if (bank < 0) return 0;
    const uint32_t groups = kernel::R32(kernel::R32(SoundManager + 0x14) + uint32_t(bank) * 24 + 12);
    uint32_t group = groups + 4;
    for (uint32_t i = 0; i < kernel::R32(groups); ++i) {
        const uint32_t count = kernel::R32(group + 4);
        if (kernel::R32(group) == sound) {
            if (!cursor.seeded) {
                const auto saved = ctx;
                cursor.next = kernel::CallGuest(ctx, 0x15A430, {0});
                ctx = saved;
                cursor.seeded = true;
            }
            const uint32_t choice = kernel::R32(group + 12 + (cursor.next % count) * 4);
            if (choice) ++cursor.next;
            return choice;
        }
        group += 12 + count * 4;
    }
    return 0;
}

bool IsHeroReacting() {
    for (uint32_t slot = 0; slot < 2; ++slot) {
        const auto at = Players + slot * PlayerStride;
        const auto type = kernel::G<int16_t>(at + ActorFlag);
        if ((type == 0 || type == 1) && kernel::R32(at + 0x1E04) == 12) return true;
    }
    return false;
}

ContextChoice SelectContextSound(X86Context& ctx, uint32_t sound, uint32_t actor) {
    if (!RestorationEnabled() || actor >= ActorCount) return {};
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (!world) return {};
    if (context_world != world || context_room != room) {
        context_choices = {};
        baraka_coop_seen = false;
        baraka_coop_actor = 0;
        context_world = world;
        context_room = room;
    }
    const auto at = Players + actor * PlayerStride;
    const auto type = kernel::G<int16_t>(at + ActorFlag);
    const auto bank = kernel::G<int8_t>(at + PlayerBank);
    for (size_t i = 0; i < std::size(openshao::dialogue::ContextFamilies); ++i) {
        const auto& family = openshao::dialogue::ContextFamilies[i];
        if (family.room != room || family.original != sound ||
            (family.actor >= 0 && type != family.actor && type != family.alternate_actor)) continue;
        auto& selection = context_choices[i][actor];
        const auto id = kernel::R32(at + 0x68);
        if (selection.actor_id != id || selection.actor_type != type || selection.actor_bank != bank) {
            selection = {};
            selection.actor_id = id;
            selection.actor_type = type;
            selection.actor_bank = bank;
        }
        const auto mode_state = kernel::R32(0x5ACEBC);
        const bool solo = mode_state && kernel::R32(mode_state + 0x88) == 0;
        std::array<uint32_t, 32> candidates{};
        uint32_t count = 1;
        for (uint32_t j = 0; j < family.takes.size() && j < 31; ++j) {
            const auto& take = family.takes[j];
            if ((take.once && (selection.seen & (1u << j))) ||
                (take.mode != openshao::dialogue::TakeMode::Any && (!mode_state ||
                 (take.mode == openshao::dialogue::TakeMode::Solo) != solo)) ||
                (take.reaction && !IsHeroReacting()) || !HasSoundDefinition(take.sound)) continue;
            candidates[count++] = j + 1;
        }
        if (count == 1) return {};
        if (!selection.cursor.seeded) {
            const auto saved = ctx;
            selection.cursor.next = kernel::CallGuest(ctx, 0x15A430, {0});
            ctx = saved;
            selection.cursor.seeded = true;
        }
        const auto choice = candidates[selection.cursor.next % count];
        if (!choice) return {&selection, sound, 0, false};
        const auto& take = family.takes[choice - 1];
        return {&selection, take.sound, choice, take.once};
    }
    return {};
}

openshao::dialogue::Line CaptureLine(uint32_t sound, uint32_t bank, uint32_t actor) {
    openshao::dialogue::Line line{sound, bank, actor, kernel::R32(CurrentRoom)};
    line.world = kernel::R32(World);
    line.actor_type = kernel::G<int16_t>(Players + actor * PlayerStride + ActorFlag);
    line.actor_bank = kernel::G<int8_t>(Players + actor * PlayerStride + PlayerBank);
    line.actor_bound = true;
    line.actor_id = kernel::R32(Players + actor * PlayerStride + 0x68);
    line.scene = kernel::R32(SceneName);
    return line;
}

bool QueueFollowup(uint32_t bit, uint32_t sound, uint32_t actor) {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (!world || actor >= ActorCount || !HasSoundDefinition(sound)) return false;
    if (followup_world != world || followup_room != room) {
        followup_seen = 0;
        followup_world = world;
        followup_room = room;
    }
    if (followup_seen & bit) return false;
    if (!Enqueue(CaptureLine(sound, 0x871111EC, actor))) return false;
    followup_seen |= bit;
    return true;
}

void QueueBarakaSpokenFollowup(X86Context& ctx, uint32_t actor) {
    // Solo never requests the co-op 08_1 cue. Allow one alternate 08 take
    // after the phase-3 speech, ahead of any optional reaction.
    const auto saved = ctx;
    const uint32_t first = kernel::CallGuest(ctx, 0x15A430, {0}) %
        std::size(openshao::dialogue::BarakaSpokenAfter);
    ctx = saved;
    for (uint32_t n = 0; n < std::size(openshao::dialogue::BarakaSpokenAfter); ++n) {
        const auto sound = openshao::dialogue::BarakaSpokenAfter[
            (first + n) % std::size(openshao::dialogue::BarakaSpokenAfter)];
        if (!HasSoundDefinition(sound)) continue;
        QueueFollowup(64, sound, actor);
        break;
    }
}

bool IsCinematicPlaying(X86Context& ctx);

bool IsNativeSoundPlaying(X86Context& ctx, uint32_t handle) {
    const auto saved = ctx;
    ctx.ecx = 0x653C64;
    const bool active = kernel::CallGuest(ctx, 0x152360, {handle}) != 0;
    ctx = saved;
    return active;
}

bool OriginalAudioStreamBusy(X86Context& ctx, bool include_buffered = true) {
    // Streams use a separate registry: tag at +0, buffered handle at +4,
    // playing handle at +8. They do not occupy the vosound slot.
    for (uint32_t slot = 0; slot < 5; ++slot) {
        const uint32_t entry = 0x69654C + slot * 12;
        // Handles remain valid during tag cleanup. Buffered streams reserve
        // optional dialogue, but must not block deferred script requests.
        if (include_buffered && kernel::R32(entry + 4)) return true;
        const uint32_t playing = kernel::R32(entry + 8);
        if (!playing) continue;
        // Retail's own global-voice update uses this exact handle predicate.
        if (IsNativeSoundPlaying(ctx, playing)) return true;
    }
    return false;
}

void RefreshSupplemental(SupplementalCue& cue, uint32_t world, uint32_t room) {
    if (cue.world == world && cue.room == room) return;
    cue = {};
    cue.world = world;
    cue.room = room;
}

void ObserveOriginalStreamFinish(X86Context& ctx, SupplementalCue& cue, uint32_t sound) {
    // Require observed playback before completion; buffering or room entry is insufficient.
    bool source_playing = false, source_buffered = false;
    for (uint32_t slot = 0; slot < 5; ++slot) {
        const uint32_t entry = 0x69654C + slot * 12;
        if (kernel::R32(entry) != sound) continue;
        if (kernel::R32(entry + 4)) source_buffered = true;
        const uint32_t handle = kernel::R32(entry + 8);
        if (!handle) continue;
        if (IsNativeSoundPlaying(ctx, handle)) source_playing = true;
    }
    if (source_playing) cue.observed = true;
    // A finished stream may lose its tag and handles before this update.
    if (cue.observed && !source_playing && !source_buffered) cue.finished = true;
}

void UpdateForestHintResponse() {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (forest_hint_world != world || forest_hint_room != room) {
        forest_hint_world = world;
        forest_hint_room = room;
        forest_hint_responded = false;
    }
    if (!RestorationEnabled() || !world || room != 128 || forest_hint_responded ||
        !kernel::G<uint8_t>(MessageManager + 0x6C0) ||
        kernel::R32(MessageManager + 0x6D8) != 62 ||
        !kernel::R32(MessageManager + 0x6C8)) return;

    // Text 62 owns Raiden's hint. Queue a monk's response only after its voice starts.
    const auto caption_sound = kernel::R32(MessageManager + 0x6C4);
    if (caption_sound != 0x901D2B23 && caption_sound != 0xFF90543D &&
        caption_sound != 0xC2F07D8D) return;
    const auto monk = kernel::G<int16_t>(Players + ActorFlag);
    const uint32_t response = monk == 0 ? 0x33A9328Cu
                            : monk == 1 ? 0xC46D848Du : 0;
    if (!response || !HasSoundDefinition(response)) return;
    if (Enqueue(CaptureLine(response, 0x871111EC, 0))) {
        forest_hint_responded = true;
    }
}

void UpdateSupplementalVoices(X86Context& ctx) {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    RefreshSupplemental(monastery_intro, world, room);
    RefreshSupplemental(forest_entrance, world, room);
    RefreshSupplemental(forest_second_tree, world, room);
    RefreshSupplemental(shrine_offering, world, room);
    RefreshSupplemental(wu_shi_fire, world, room);
    RefreshSupplemental(mileena_trio_opening, world, room);
    RefreshSupplemental(scorpion_combat, world, room);
    RefreshSupplemental(monastery_fist, world, room);
    if (foundry_bridge.world != world || foundry_bridge.room != room)
        foundry_bridge_ticks = 0;
    RefreshSupplemental(foundry_bridge, world, room);
    if (forest_snakes.world != world || forest_snakes.room != room)
    {
        forest_snakes_triggered = false;
        forest_snakes_stream_handle = 0;
        forest_snakes_cutscene_seen = false;
    }
    RefreshSupplemental(forest_snakes, world, room);
    if (mileena_monastery_combat.world != world || mileena_monastery_combat.room != room) {
        mileena_monastery_combat = {};
        mileena_monastery_combat.world = world;
        mileena_monastery_combat.room = room;
    }
    if (!RestorationEnabled() || !world) return;
    // MTR2392 spawns the Foundry monks without a speech event. This supplemental
    // remark needs a settling delay rather than an existing dialogue trigger.
    if (room == 74 && !foundry_bridge.delivered &&
        !kernel::G<uint8_t>(SceneLoadingOrPlaying) && !IsCinematicPlaying(ctx) &&
        !openshao::ed_nexus::GameplayPaused() && foundry_bridge_ticks < 90)
        ++foundry_bridge_ticks;
    if (room == 74 && foundry_bridge_ticks >= 90)
        foundry_bridge.finished = true;
    // Forest Map 10 is native room 137 (debug_map 138). _d_snakes plays the
    // prebuffered E8F451C7; its activation alone does not confirm scene completion.
    if (room == 137 && forest_snakes_triggered && !forest_snakes.finished &&
        !forest_snakes.delivered) {
        // A scene can animate even when audio playback fails; observe its start and end.
        const bool scene_active = kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx);
        if (scene_active) forest_snakes_cutscene_seen = true;
        // Recover handles from native slots when playback bypasses the script wrapper.
        // Keep the saved handle if those slots have already been cleared.
        bool buffered = false;
        for (uint32_t i = 0; i < 5; ++i) {
            const uint32_t entry = 0x69654C + i * 12;
            if (kernel::R32(entry) != 0xE8F451C7) continue;
            buffered |= kernel::R32(entry + 4) != 0;
            if (const uint32_t handle = kernel::R32(entry + 8))
                forest_snakes_stream_handle = handle;
        }
        if (forest_snakes_stream_handle) {
            const bool playing = IsNativeSoundPlaying(ctx, forest_snakes_stream_handle);
            if (playing) forest_snakes.observed = true;
            else if (forest_snakes.observed && !buffered) {
                forest_snakes.finished = true;
            }
        } else if (forest_snakes_cutscene_seen && !scene_active &&
                   !OriginalAudioStreamBusy(ctx, false)) {
            // Cinematic completion also permits the follow-up when its soundtrack failed.
            forest_snakes.finished = true;
        }
    }
    if (room == 106 && !wu_shi_fire.finished && !wu_shi_fire.delivered)
        ObserveOriginalStreamFinish(ctx, wu_shi_fire, 0x50329483);
    // The shrine approach caption precedes the offering. Wait for the
    // door-opening stream (cs_wu_shi_13_2.adx) to finish before acknowledging it.
    if (room == 108 && !shrine_offering.finished && !shrine_offering.delivered) {
        ObserveOriginalStreamFinish(ctx, shrine_offering, 0x800DFF7A);
    }
    const bool caption_active = kernel::G<uint8_t>(MessageManager + 0x6C0);
    const uint32_t text = kernel::R32(MessageManager + 0x6D8);
    const uint32_t caption_handle = kernel::R32(MessageManager + 0x6C8);
    // Observe displayed map messages; journal lookups must not trigger speech.
    if (caption_active) {
        if (room == 192 && text == 54) monastery_intro.observed = true;
        if (room == 128 && text == 55) forest_entrance.observed = true;
        if (room == 129 && text == 63 && caption_handle &&
            kernel::R32(MessageManager + 0x6C4) == 0x01DD37EB)
            forest_second_tree.observed = true;
        // Text 45 with audio marks the opening three-sister fight.
        if (room == 195 && text == 45 && caption_handle)
            mileena_trio_opening.observed = true;
        // Text 100 is the Fist of Ruin acquisition popup (Raiden 4_07).
        if (room == 195 && text == 100)
            monastery_fist.observed = true;
    }
    // Liu's remark follows popup dismissal so it cannot overlap Raiden's tutorial.
    if (room == 195 && monastery_fist.observed && !caption_active)
        monastery_fist.finished = true;
    SupplementalCue* cue = nullptr;
    uint32_t sound = 0;
    uint32_t cue_actor = 0;
    if (room == 74 && foundry_bridge.finished && !foundry_bridge.delivered) {
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        sound = hero == 0 ? 0xA09578E5u : hero == 1 ? 0x7B43C051u : 0;
        cue = &foundry_bridge;
    } else if (room == 137 && forest_snakes_triggered &&
               forest_snakes.finished && !forest_snakes.delivered) {
        sound = 0x919A865Fu; // Raiden LF10_01_3 in the local Forest bank
        cue = &forest_snakes;
    } else if (room == 195 && monastery_fist.finished && !monastery_fist.delivered &&
        kernel::G<int16_t>(Players + ActorFlag) == 0) {
        sound = 0xA6A85EC8u; // VO_LIUKANG_EM_5_02 in room195's native bank
        cue = &monastery_fist;
    } else if (room == 192 && monastery_intro.observed && !monastery_intro.delivered) {
        sound = 0xD64BB4B8u;
        cue = &monastery_intro;
    } else if (room == 128 && forest_entrance.observed && !forest_entrance.delivered &&
               kernel::G<int16_t>(Players + ActorFlag) == 0) {
        sound = 0xC629944Cu;
        cue = &forest_entrance;
    } else if (room == 129 && forest_second_tree.observed &&
               !forest_second_tree.delivered) {
        // Raiden elaborates on Smoke's authentic tree hint; both play in order.
        sound = 0xC33F8FCEu;
        cue = &forest_second_tree;
    } else if (room == 108 && shrine_offering.finished &&
               !shrine_offering.delivered) {
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        sound = hero == 0 ? 0x29314E8Bu : hero == 1 ? 0xE5E6EFA6u : 0;
        cue = &shrine_offering;
    } else if (room == 195 && mileena_trio_opening.observed &&
               !mileena_trio_opening.delivered) {
        // The three-woman taunt requires all three sisters before Kitana is freed.
        bool kitana = false, jade = false;
        uint32_t mileena = ActorCount;
        for (uint32_t slot = 0; slot < ActorCount; ++slot) {
            const auto at = Players + slot * PlayerStride;
            if (!kernel::R32(at + 0x68)) continue;
            switch (kernel::G<int16_t>(at + ActorFlag)) {
            case 65: kitana = true; break;
            case 66: mileena = slot; break;
            case 67: jade = true; break;
            default: break;
            }
        }
        if (kitana && mileena < ActorCount && jade) {
            sound = 0x804E23CEu;
            cue = &mileena_trio_opening;
            cue_actor = mileena;
        }
    } else if (room == 160 && scorpion_combat.observed &&
               !scorpion_combat.delivered) {
        // Follow the accepted Scorpion 04 taunt with 02_1 while the same boss is present.
        for (uint32_t slot = 0; slot < ActorCount; ++slot) {
            const auto at = Players + slot * PlayerStride;
            if (kernel::G<int16_t>(at + ActorFlag) != 52 ||
                !kernel::R32(at + 0x68)) continue;
            sound = 0x168B9C83u;
            cue = &scorpion_combat;
            cue_actor = slot;
            break;
        }
    } else if (room == 195 && mileena_monastery_combat.pending &&
               !mileena_monastery_combat.canceled) {
        for (uint32_t slot = 0; slot < ActorCount; ++slot) {
            const auto at = Players + slot * PlayerStride;
            if (kernel::G<int16_t>(at + ActorFlag) != 66 ||
                kernel::R32(at + 0x68) != mileena_monastery_combat.mileena_id) continue;
            sound = mileena_monastery_combat.pending;
            cue_actor = slot;
            break;
        }
    } else if (room == 106 && wu_shi_fire.finished && !wu_shi_fire.delivered) {
        // The fire-puzzle hint expires when a water lever is operated.
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        const auto saved = ctx;
        const uint32_t variation = hero <= 1 && hero >= 0
            ? kernel::CallGuest(ctx, 0x15A430, {0}) % 2 : 0;
        ctx = saved;
        sound = hero == 0 ? (variation ? 0x96DE55A2u : 0xD17E2F72u)
              : hero == 1 ? (variation ? 0x172550E4u : 0x50852A34u) : 0;
        cue = &wu_shi_fire;
    }
    if ((!cue && !mileena_monastery_combat.pending) || !sound) return;
    if (caption_active || kernel::G<uint8_t>(SceneLoadingOrPlaying) ||
        IsCinematicPlaying(ctx) || openshao::ed_nexus::GameplayPaused() ||
        kernel::R32(VoiceHandle) || queue.Size() ||
        kernel::G<uint8_t>(0x4E67C4) || kernel::G<uint8_t>(0x4E66F4) ||
        kernel::G<uint8_t>(0x59E095) || kernel::R32(0x5B22FC) ||
        int32_t(kernel::R32(0x6C0404)) > 0 || kernel::R32(0x5146E4) ||
        int32_t(kernel::R32(MessageManager + 0x6FC)) > 0) return;
    // After the snake scene ends, future buffered streams must not starve Raiden's
    // follow-up. Active streams still block it; other optional cues also wait for buffers.
    if (OriginalAudioStreamBusy(ctx, cue != &forest_snakes)) return;

    if (HasSoundDefinition(sound) && Enqueue(CaptureLine(sound, 0x871111EC, cue_actor))) {
        if (cue) cue->delivered = true;
        else {
            mileena_monastery_combat.pending = 0;
            ++mileena_monastery_combat.next;
        }
    }
}

bool MatchesActor(const openshao::dialogue::Line& line) {
    return line.player < ActorCount && (!line.actor_bound ||
        (line.actor_type == kernel::G<int16_t>(Players + line.player * PlayerStride + ActorFlag) &&
         line.actor_bank == kernel::G<int8_t>(Players + line.player * PlayerStride + PlayerBank) &&
         line.actor_id == kernel::R32(Players + line.player * PlayerStride + 0x68))) &&
        (!line.after_cinematic || line.scene == kernel::R32(SceneName));
}

bool IsCinematicPlaying(X86Context& ctx) {
    const auto saved = ctx;
    ctx.ecx = 0x5144E8;
    const auto result = kernel::CallGuest(ctx, 0x1A76D0, {});
    ctx = saved;
    return uint8_t(result) == 1;
}

void UpdateErmacPhaseVoices(X86Context& ctx) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (ermac_world != world || ermac_room != room || !RestorationEnabled() || !world) {
        ermac_phase_voices = {};
        ermac_world = world;
        ermac_room = room;
    }
    if (!RestorationEnabled() || !world || room != 108) return;
    // Retry lines dropped by scene or actor cleanup until native playback confirms delivery.
    for (auto& phase : ermac_phase_voices)
        if (phase.queued && !phase.delivered && !queue.Size()) phase.queued = false;
    if (kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx) ||
        openshao::ed_nexus::GameplayPaused() || CaptionVoiceHandle() ||
        kernel::R32(VoiceHandle) || queue.Size() || OriginalAudioStreamBusy(ctx, false)) return;
    // Latest phase supersedes a line whose associated boss phase is over.
    for (size_t i = ermac_phase_voices.size(); i-- > 0;) {
        auto& phase = ermac_phase_voices[i];
        if (!phase.requested || !phase.ready || phase.delivered || phase.queued ||
            !HasSoundDefinition(ErmacPhaseSounds[i])) continue;
        for (uint32_t slot = 0; slot < ActorCount; ++slot) {
            const uint32_t at = Players + slot * PlayerStride;
            if (kernel::G<int16_t>(at + ActorFlag) != 68 || !kernel::R32(at + 0x68)) continue;
            if (Enqueue(CaptureLine(ErmacPhaseSounds[i], 0x871111ECu, slot))) {
                phase.queued = true;
            }
            return;
        }
    }
}

void UpdateCageCaption(X86Context& ctx) {
    if (!cage.pending) return;
    if (!RestorationEnabled() || kernel::R32(CurrentRoom) != cage.room ||
        kernel::R32(World) != cage.world || kernel::R32(SceneName) != CageScene) {
        cage.pending = false;
        return;
    }
    // A skip can clear a newly queued message during the rest of that frame.
    // Delivery is complete only once the native renderer starts its voice.
    if (kernel::G<uint8_t>(MessageManager + 0x6C0) &&
        kernel::R32(MessageManager + 0x6D8) == 33 &&
        kernel::R32(MessageManager + 0x6C4) == 0x68D2A09B &&
        kernel::R32(MessageManager + 0x6C8)) {
        cage.pending = false;
        cage.delivered = true;
        LOGD("Restored Cage platform caption: text=33 template=5 room=%u", cage.room);
        return;
    }
    // The cinematic flag is still zero while the stream loads. Its separate
    // lifetime flag stays set until native completion/skip cleanup finishes.
    if (kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx) ||
        openshao::ed_nexus::GameplayPaused() || kernel::R32(VoiceHandle) || queue.Size() ||
        kernel::G<uint8_t>(MessageManager + 0x6C0) || kernel::G<uint8_t>(0x4E67C4) ||
        kernel::G<uint8_t>(0x4E66F4) || kernel::G<uint8_t>(0x59E095) ||
        kernel::R32(0x5B22FC) || int32_t(kernel::R32(0x6C0404)) > 0 ||
        kernel::R32(0x5146E4) || int32_t(kernel::R32(MessageManager + 0x6FC)) > 0) return;

    const auto saved = ctx;
    // mtxt_platform has no incoming trigger. Keep its localized text/audio and
    // use the timed, non-modal template of the connected Johnny Cage lines.
    kernel::CallGuest(ctx, 0x1385F, {33, 5});
    ctx = saved;
}

uint32_t PlayLine(X86Context& ctx, const openshao::dialogue::Line& line) {
    const auto saved = ctx;
    int32_t bank;
    if (line.bank == kernel::R32(CharacterBank)) {
        bank = int8_t(kernel::G<uint8_t>(Players + line.player * PlayerStride + PlayerBank));
    } else {
        ctx.ecx = SoundManager;
        bank = int32_t(kernel::CallGuest(ctx, 0x150DF0, {line.bank}));
        ctx = saved;
    }
    ScopedGuestWord request(VoiceRequest, 1);
    ctx.ecx = SoundManager;
    const uint32_t handle = bank < 0
        ? kernel::CallGuest(ctx, 0x151560, {line.sound, 0x3F800000, 0, 0, 0, 0xBF800000, 0, 0})
        : kernel::CallGuest(ctx, 0x151610, {uint32_t(bank), line.sound, 0x3F800000, 0, 0, 0, 0xBF800000, 0, 0});
    ctx = saved;
    kernel::W32(VoiceHandle, handle);
    return handle;
}

void UpdateDialogue(X86Context& ctx) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = last_update.time_since_epoch().count()
        ? std::chrono::duration<double>(now - last_update).count() : 0;
    last_update = now;
    UpdateForestHintResponse();
    UpdateSupplementalVoices(ctx);
    UpdateErmacPhaseVoices(ctx);
    if (!queue.Size()) return;
    if (!RestorationEnabled() || !kernel::R32(World)) { queue.Clear(); return; }
    const bool paused = openshao::ed_nexus::GameplayPaused();
    queue.DropOtherRooms(kernel::R32(CurrentRoom), kernel::R32(World));
    const bool cinematic = kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx);
    if (cinematic) queue.EnterCinematic();
    const bool blocked = paused || cinematic || kernel::R32(VoiceHandle) ||
        CaptionVoiceHandle() || OriginalAudioStreamBusy(ctx, false);
    queue.Age(elapsed, blocked);
    if (blocked || !queue.Size()) return;
    const auto line = queue.Pop();
    // A phase3 line is no longer timely once the original boss entered phase4.
    if (line->room == 108 && line->sound == ErmacPhaseSounds[0] &&
        ermac_phase_voices[1].requested) {
        ermac_phase_voices[0].queued = false;
        return;
    }
    if (!MatchesActor(*line)) {
        for (size_t i = 0; i < ermac_phase_voices.size(); ++i)
            if (line->room == 108 && line->sound == ErmacPhaseSounds[i])
                ermac_phase_voices[i].queued = false;
        return;
    }
    const auto handle = PlayLine(ctx, *line);
    for (size_t i = 0; i < ermac_phase_voices.size(); ++i) {
        if (line->room != 108 || line->sound != ErmacPhaseSounds[i]) continue;
        ermac_phase_voices[i].queued = false;
        ermac_phase_voices[i].delivered = handle != 0;
    }
    LOGD("Restored dialogue: sound=%08X handle=%08X waited=%.2fs", line->sound, handle, line->waited);
}
}

X86_EXTERN_FUNC(__imp__sub_001A18F0);
X86_FUNC(sub_001A18F0) {
    const uint32_t event = kernel::R32(ctx.esp + 4), actor = kernel::R32(ctx.esp + 8);
    const uint32_t args = kernel::R32(event + 8), player = kernel::R32(actor + 0x10);
    const uint32_t original_request = kernel::R32(args);
    const bool moon = RestorationEnabled() && kernel::R32(CurrentRoom) == 98 &&
        kernel::R32(args) == 0 && kernel::R32(args + 4) == 0x871111EC && kernel::R32(args + 8) == 1;
    const bool kitana_laugh = RestorationEnabled() && kernel::R32(CurrentRoom) == 194 &&
        kernel::R32(args) == 0xDF95FA70 && kernel::R32(args + 4) == 0x871111EC
        && kernel::R32(args + 8) == 0xFFFFFFFF;
    if (kitana_laugh && !kitana_seeded) {
        // This room has only two authored laugh events. Vary the first family
        // so SHORT remains reachable without requiring a third event.
        const auto saved = ctx;
        kitana_variation = kernel::CallGuest(ctx, 0x15A430, {0}) % KitanaLaughs.size();
        ctx = saved;
        kitana_seeded = true;
    }
    uint32_t laugh = KitanaLaughs[kitana_variation % KitanaLaughs.size()];
    int32_t laugh_bank = kitana_laugh ? FindGroupBank(laugh) : -1;
    if (kitana_laugh && laugh_bank < 0) {
        laugh = KitanaLaughs[0];
        laugh_bank = FindGroupBank(laugh);
    }
    // The pillar cues requested only LONG_2. Recover the stage's native laugh
    // groups at those same cues, using only banks already loaded for this room.
    uint32_t sound = moon ? 0xEFD60587 : laugh_bank >= 0 ? laugh : kernel::R32(args);
    uint32_t bank_name = laugh_bank >= 0
        ? kernel::R32(kernel::R32(SoundManager + 0x14) + uint32_t(laugh_bank) * 24) : kernel::R32(args + 4);
    if (RestorationEnabled() && player < ActorCount && kernel::R32(args + 4) == 0xAECADCD8 &&
        kernel::R32(args + 8) == 0xFFFFFFFF) {
        const uint32_t room = kernel::R32(CurrentRoom), actor_at = Players + player * PlayerStride;
        const auto character = kernel::G<int16_t>(actor_at + ActorFlag);
        // The Forest Map 9 hints reference absent IDs. Its existing recordings
        // describe this acid puzzle; retain their original delayed triggers.
        if (room == 136) {
            if (sound == 0x93FDBC98 && character == 0) { sound = 0x526CF529; bank_name = 0x871111EC; }
            if (sound == 0x8B431000 && character == 1) { sound = 0xDF681048; bank_name = 0x871111EC; }
            // Explicit banks use the variation-only API (0x151610). These
            // two direct definitions require the native search-all path.
        }
        // The wheel's ordinary cue also runs during hit reactions. Native
        // InReaction (0x15DF30) recognizes actor state 12; the unused group
        // specifically describes losing wheel progress after an interruption.
        if (room == 104 && sound == 0xC3FF4851 && character == 71 &&
            kernel::R32(actor_at + 0x1E04) == 12 && FindGroupBank(0x3F5456F6) >= 0) sound = 0x3F5456F6;
    }
    // Correct the standalone "Are you okay?" speaker only during solo Kung Lao play.
    if (original_request == 0x6935F9CD && player < ActorCount) {
        if (IsBarakaSoloKungLao() && HasSoundDefinition(0x530DACE6)) {
            sound = 0x530DACE6;
            bank_name = 0x871111EC;
        }
    }
    const auto context = SelectContextSound(ctx, sound, player);
    if (context.selection) {
        sound = context.sound;
        if (context.take) bank_name = 0x871111EC;
    }
    ScopedGuestWord repaired_sound(args, sound);
    ScopedGuestWord repaired_bank(args + 4, bank_name);
    const uint32_t original_voice = kernel::R32(VoiceHandle);
    // Route caption contention through the native handler's normal busy exit.
    // The caption owns its handle; restore the separate script slot afterward.
    const uint32_t caption_voice = RestorationEnabled() && !original_voice
        ? (CaptionVoiceHandle() ? CaptionVoiceHandle() : queue.Size() ? 0xFFFFFFFFu : 0) : 0;
    const uint32_t busy = caption_voice ? caption_voice : original_voice;
    if (caption_voice) {
        ScopedGuestWord marked_busy(VoiceHandle, caption_voice);
        __imp__sub_001A18F0(ctx, base);
    } else {
        __imp__sub_001A18F0(ctx, base);
    }
    if (!RestorationEnabled() || player >= ActorCount || IsCinematicPlaying(ctx) || !kernel::R32(args)) return;
    bool accepted = !caption_voice && kernel::R32(VoiceHandle) && kernel::R32(VoiceHandle) != busy;
    const auto flag = kernel::G<uint16_t>(Players + player * PlayerStride + ActorFlag);
    if (busy && !accepted && openshao::dialogue::ActorAccepts(kernel::R32(args + 8), flag)) {
        const auto line = CaptureLine(kernel::R32(args), kernel::R32(args + 4), player);
        accepted = Enqueue(line);
        if (accepted) LOGD("Dialogue deferred: sound=%08X busy=%08X", line.sound, busy);
    }
    if (!accepted) return;
    if (kernel::R32(CurrentRoom) == 44 && kernel::R32(args) == 0x6B25C1AE &&
        kernel::G<int16_t>(Players + player * PlayerStride + ActorFlag) == 69)
        QueueFollowup(2, 0x2C85BB7E, player);
    if (kernel::R32(CurrentRoom) == 15 && flag == 56 &&
        original_request == 0xD8855C8Eu &&
        kernel::R32(Players + player * PlayerStride + 0x68)) {
        const auto mode = kernel::R32(0x5ACEBC);
        if (mode && kernel::R32(mode + 0x88)) {
            // Keep _d_vo_coop's 08_1, then queue one alternate take.
            baraka_coop_seen = true;
            baraka_coop_actor = kernel::R32(Players + player * PlayerStride + 0x68);
            QueueBarakaSpokenFollowup(ctx, player);
        }
    }
    // Phase-3 speech permits one alternate 08 take in solo and one later reaction.
    // Earlier 07 taunts must keep their original timing and recordings.
    if (kernel::R32(CurrentRoom) == 15 && flag == 56 &&
        original_request == 0xA0553378 && kernel::R32(Players + player * PlayerStride + 0x68)) {
        const auto mode = kernel::R32(0x5ACEBC);
        // Ko-op also requires the same actor's 08_1 cue; solo has no _d_vo_coop event.
        if (mode && (!kernel::R32(mode + 0x88) ||
                     (baraka_coop_seen && baraka_coop_actor ==
                      kernel::R32(Players + player * PlayerStride + 0x68)))) {
            if (!kernel::R32(mode + 0x88))
                QueueBarakaSpokenFollowup(ctx, player);
            const auto saved = ctx;
            const uint32_t offset = kernel::CallGuest(ctx, 0x15A430, {0}) %
                std::size(openshao::dialogue::BarakaLateReactions);
            ctx = saved;
            for (uint32_t j = 0; j < std::size(openshao::dialogue::BarakaLateReactions); ++j) {
                const uint32_t sound = openshao::dialogue::BarakaLateReactions[
                    (offset + j) % std::size(openshao::dialogue::BarakaLateReactions)].sound;
                if (HasSoundDefinition(sound)) {
                    QueueFollowup(32, sound, player);
                    break;
                }
            }
        }
    }
    // Attach the monks' responses to their character-specific Raiden reminders.
    if (kernel::R32(CurrentRoom) == 96) {
        const auto hero = kernel::G<int16_t>(Players + player * PlayerStride + ActorFlag);
        if (hero == 0 && kernel::R32(args) == 0x3185C09E)
            QueueFollowup(8, 0xBF0AC77D, player);
        if (hero == 1 && kernel::R32(args) == 0x5873A43A)
            QueueFollowup(16, 0x64DC7FC9, player);
    }
    // Arm Scorpion's follow-up only after the original 04 request is accepted.
    if (kernel::R32(CurrentRoom) == 160 && original_request == 0xB9E947A4 &&
        player < ActorCount && flag == 52) {
        RefreshSupplemental(scorpion_combat, kernel::R32(World), 160);
        scorpion_combat.observed = true;
    }
    if (context.selection) {
        ++context.selection->cursor.next;
        if (context.once) context.selection->seen |= 1u << (context.take - 1);
    }
    if (laugh_bank >= 0) ++kitana_variation;
    if (kernel::R32(CurrentRoom) == 73 && flag == 62 && kernel::R32(args) == 0x56E516CA &&
        kernel::R32(args + 4) == 0xAECADCD8 && kernel::R32(args + 8) == 0xFFFFFFFF) {
        // vo_shang actually plays Shao Kahn's announcer group. Keep it, then
        // recover one of Shang's unused combat taunts as a contextual response.
        const uint32_t sound = ShangTaunts[shang_variation % ShangTaunts.size()];
        const int32_t bank = FindGroupBank(sound);
        if (bank >= 0) {
            const auto name = kernel::R32(kernel::R32(SoundManager + 0x14) + uint32_t(bank) * 24);
            if (Enqueue(CaptureLine(sound, name, player))) ++shang_variation;
        }
    }
}

// Clears the stored script voice handle when a level initializes.
X86_EXTERN_FUNC(__imp__sub_001A1850);
X86_FUNC(sub_001A1850) {
    queue.Clear();
    last_update = {};
    cage = {};
    recovered_cues.fill(0);
    cue_room = cue_world = 0;
    kitana_trap = 0;
    escape_warning = 0;
    kitana_variation = shang_variation = 0;
    kitana_seeded = false;
    stream_variations = {};
    stream_world = stream_room = 0;
    caption_choices = {};
    caption_world = caption_room = 0;
    context_choices = {};
    context_world = context_room = 0;
    baraka_coop_seen = false;
    baraka_coop_actor = 0;
    activation_seen = {};
    ermac_phase_voices = {};
    ermac_world = ermac_room = 0;
    phase_world = phase_room = 0;
    capture_warning = freeing_warning = false;
    followup_world = followup_room = followup_seen = 0;
    forest_hint_world = forest_hint_room = 0;
    forest_hint_responded = false;
    monastery_intro = forest_entrance = forest_second_tree = shrine_offering = wu_shi_fire = mileena_trio_opening = scorpion_combat = monastery_fist = foundry_bridge = forest_snakes = {};
    foundry_bridge_ticks = 0;
    forest_snakes_triggered = false;
    forest_snakes_stream_handle = 0;
    forest_snakes_cutscene_seen = false;
    mileena_monastery_combat = {};
    native_world = native_room = 0;
    spear_choice = fire_choice = shao_choice = {};
    shao_challenge = false;
    __imp__sub_001A1850(ctx, base);
}

// Keep the original spatial playback, repetition gates and handle ownership.
X86_EXTERN_FUNC(__imp__sub_00151560);
X86_FUNC(sub_00151560) {
    RefreshNativeEncounter();
    const auto argument = ctx.esp + 4, original = kernel::R32(argument);
    const bool baraka_kung = original == 0x6935F9CD && IsBarakaSoloKungLao() &&
        HasSoundDefinition(0x530DACE6);
    const bool eligible = RestorationEnabled() && kernel::R32(World) && native_room == 160 &&
        original == 0x12F19BF6 && HasSoundDefinition(0x24E7FCEE);
    const bool alternate = eligible && ChoiceIndex(ctx, spear_choice, 2) != 0;
    ScopedGuestWord selected(argument, baraka_kung ? 0x530DACE6u :
                                   alternate ? 0x24E7FCEEu : original);
    __imp__sub_00151560(ctx, base);
    if (eligible && ctx.eax) ++spear_choice.next;
}

X86_EXTERN_FUNC(__imp__sub_00151610);
X86_FUNC(sub_00151610) {
    RefreshNativeEncounter();
    const auto bank = kernel::R32(ctx.esp + 4), sound = kernel::R32(ctx.esp + 8);
    const bool baraka_kung = sound == 0x6935F9CD && IsBarakaSoloKungLao() &&
        HasBankSoundDefinition(bank, 0x530DACE6);
    uint32_t replacement = baraka_kung ? 0x530DACE6u : 0;
    const bool fire = RestorationEnabled() && kernel::R32(World) && native_room == 160 &&
        sound == 0xDB4300AA && HasBankSoundDefinition(bank, 0x7D06D23D);
    if (fire && ChoiceIndex(ctx, fire_choice, 6) == 5) replacement = 0x7D06D23D;
    bool challenge = false;
    if (RestorationEnabled() && kernel::R32(World) && native_room == 73 && sound == 0xFEDCADEA) {
        if (!shao_challenge) {
            const auto take = ChoiceIndex(ctx, shao_choice, 2) ? 0x9311AED8u : 0xAE718768u;
            if (HasBankSoundDefinition(bank, take)) { replacement = take; challenge = true; }
        } else if (IsHeroReacting() && HasBankSoundDefinition(bank, 0x213172C8)) replacement = 0x213172C8;
    }
    const auto group = replacement ? FindBankGroup(bank, sound) : 0;
    // A direct banked sound ID is not a random group; replace the actual
    // argument. Group mutations are only for authored grouped variants.
    std::optional<ScopedGuestWord> direct;
    if (baraka_kung) direct.emplace(ctx.esp + 8, replacement);
    std::optional<ScopedGuestWord> count, current, first;
    if (group) {
        count.emplace(group + 4, 1);
        current.emplace(group + 8, 0);
        first.emplace(group + 12, replacement);
    }
    __imp__sub_00151610(ctx, base);
    if (ctx.eax) {
        if (fire) ++fire_choice.next;
        if (challenge && group) shao_challenge = true;
    }
}

// Require an animation change: repeated setters must not duplicate speech.
X86_EXTERN_FUNC(__imp__sub_00176270);
X86_FUNC(sub_00176270) {
    const auto slot = kernel::R32(ctx.esp + 4), animation = kernel::R32(ctx.esp + 8);
    const auto at = Players + (slot < ActorCount ? slot : 0) * PlayerStride;
    const auto previous = kernel::R32(at + 0x24);
    __imp__sub_00176270(ctx, base);
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (!RestorationEnabled() || !world || room != 195 || slot >= ActorCount || previous == animation ||
        kernel::R32(at + 0x24) != animation || kernel::G<uint8_t>(SceneLoadingOrPlaying)) return;
    if (phase_world != world || phase_room != room) {
        capture_warning = freeing_warning = false;
        phase_world = world;
        phase_room = room;
    }
    const auto character = kernel::G<int16_t>(at + ActorFlag);
    const auto mileena_attack = [](uint32_t anim) {
        // ERG.DT2 graph 6694: Mileena's voiced attacks, with yells at frames 4/5/13/45.
        return anim == 1764 || anim == 1766 || anim == 1770 || anim == 1771;
    };
    if (character == 66 && mileena_attack(previous) && !mileena_attack(animation) &&
        mileena_trio_opening.observed && mileena_trio_opening.delivered &&
        !mileena_monastery_combat.canceled && !mileena_monastery_combat.pending &&
        kernel::R32(at + 0x68)) {
        const auto mode = kernel::R32(0x5ACEBC);
        const bool coop = mode && kernel::R32(mode + 0x88) != 0;
        const auto next = mileena_monastery_combat.next;
        // These combat takes need room 195's bank, which is absent in Forest room 132.
        const uint32_t sound = next == 0 ? 0x85897096u
            : coop && next == 1 ? 0x7C193369u
            : coop && next == 2 ? 0x3BB949B9u
            : next == (coop ? 3u : 1u) ? 0x0ACC1FAFu
            : next == (coop ? 4u : 2u) ? 0x4D6C657Fu : 0;
        if (sound && HasSoundDefinition(sound)) {
            mileena_monastery_combat.pending = sound;
            mileena_monastery_combat.mileena_id = kernel::R32(at + 0x68);
        }
    }
    if (character == 65 && animation == 1721 && !capture_warning && HasSoundDefinition(0x9EABBADC)) {
        capture_warning = Enqueue(CaptureLine(0x9EABBADC, 0x871111EC, slot));
    }
    if ((character == 66 || character == 67) && animation == 1723) {
        const uint32_t group = character == 66 ? 0xE35C1CF7 : 0xF0252683;
        if (FindGroupBank(group) >= 0) Enqueue(CaptureLine(group, 0xAECADCD8, slot));
        if (!freeing_warning && HasSoundDefinition(0x6B2B1C1C))
            freeing_warning = Enqueue(CaptureLine(0x6B2B1C1C, 0x871111EC, slot));
    }
}

// The caption renderer queries this mapping every frame. Keep one take for its
// entire room lifetime so a variation cannot restart the native caption voice.
X86_EXTERN_FUNC(__imp__sub_000111EF);
X86_FUNC(sub_000111EF) {
    const uint32_t caller = kernel::R32(ctx.esp), text = kernel::R32(ctx.esp + 4);
    __imp__sub_000111EF(ctx, base);
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (!RestorationEnabled() || !world || caller != 0x86EE7) return;
    if (caption_world != world || caption_room != room) {
        caption_choices = {};
        caption_world = world;
        caption_room = room;
    }
    // The unvoiced Soul Tombs entrance caption has a matching take for each lead monk.
    if (room == 30 && text == 58 && !ctx.eax) {
        const auto player = kernel::G<int16_t>(Players + ActorFlag);
        const uint32_t entrance = player == 0 ? 0xB8867E92u
                               : player == 1 ? 0x82BE2BB9u : 0;
        if (entrance && HasSoundDefinition(entrance)) {
            ctx.eax = entrance;
            return;
        }
    }
    // Map 1's unvoiced Wall Climb hint (text 237) has matching stage-one recordings.
    if (room == 192 && text == 237 && !ctx.eax) {
        const auto player = kernel::G<int16_t>(Players + ActorFlag);
        const uint32_t climb = player == 0 ? 0xADA2A592u
                             : player == 1 ? 0x20A640F3u : 0;
        if (climb && HasSoundDefinition(climb)) {
            ctx.eax = climb;
            return;
        }
    }
    if (room == 104 && text == 7 && ctx.eax == 0xB283E945) {
        for (uint32_t slot = 0; slot < ActorCount; ++slot) {
            const auto at = Players + slot * PlayerStride;
            if (kernel::G<int16_t>(at + ActorFlag) == 71 && kernel::R32(at + 0x68)) {
                QueueFollowup(1, 0x00A33555, slot);
                break;
            }
        }
    }
    for (size_t i = 0; i < CaptionFamilies.size(); ++i) {
        const auto& cue = CaptionFamilies[i];
        if (cue.room != room || cue.text != text || cue.original != ctx.eax || !HasSoundDefinition(cue.recovered)) continue;
        if (!cue.original && !cue.alternate) { ctx.eax = cue.recovered; return; }
        auto& choice = caption_choices[i];
        if (!choice.seeded) {
            const auto saved = ctx;
            choice.next = kernel::CallGuest(ctx, 0x15A430, {0}) % 2;
            ctx = saved;
            choice.seeded = true;
        }
        if (!cue.original_usable) {
            ctx.eax = choice.next && cue.alternate && HasSoundDefinition(cue.alternate) ? cue.alternate : cue.recovered;
        } else if (choice.next) ctx.eax = cue.recovered;
        return;
    }
}

// Observe _d_snakes' audiostreamplay even if audio is rejected. Raiden's follow-up
// still requires a later stream or cinematic completion.
X86_EXTERN_FUNC(__imp__sub_000147F0);
X86_FUNC(sub_000147F0) {
    if (RestorationEnabled() && kernel::R32(World) && kernel::R32(CurrentRoom) == 137) {
        const uint32_t event = kernel::R32(ctx.esp + 4);
        if (event) {
            const uint32_t args = kernel::R32(event + 8);
            if (args && kernel::R32(args) == 0xE8F451C7) {
                RefreshSupplemental(forest_snakes, kernel::R32(World), 137);
                forest_snakes_triggered = true;
            }
        }
    }
    __imp__sub_000147F0(ctx, base);
}

// Capture actual audiostreamplay handles; trigger activation does not prove playback.
X86_EXTERN_FUNC(__imp__sub_0017C0B0);
X86_FUNC(sub_0017C0B0) {
    const uint32_t sound = kernel::R32(ctx.esp + 4);
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    std::array<uint32_t, 5> buffered{}, playing{};
    const bool snake = RestorationEnabled() && world && room == 137 && sound == 0xE8F451C7;
    if (snake) for (size_t i = 0; i < buffered.size(); ++i) {
        const uint32_t entry = 0x69654C + uint32_t(i) * 12;
        if (kernel::R32(entry) == sound) {
            buffered[i] = kernel::R32(entry + 4);
            playing[i] = kernel::R32(entry + 8);
        }
    }
    __imp__sub_0017C0B0(ctx, base);
    if (!snake || room != kernel::R32(CurrentRoom) || world != kernel::R32(World)) return;
    for (size_t i = 0; i < buffered.size(); ++i) {
        const uint32_t entry = 0x69654C + uint32_t(i) * 12;
        const uint32_t handle = kernel::R32(entry + 8);
        if (!buffered[i] || !handle || handle != buffered[i] ||
            kernel::R32(entry) != sound || (playing[i] && playing[i] == handle)) continue;
        RefreshSupplemental(forest_snakes, world, room);
        forest_snakes_triggered = true;
        forest_snakes_stream_handle = handle;
        break;
    }
}

// The authored animation still owns the speaker, frame and stream lifetime.
// Its hardcoded take bypasses the original localized alternatives in bank7.
X86_EXTERN_FUNC(__imp__sub_0017C040);
X86_FUNC(sub_0017C040) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (stream_world != world || stream_room != room) {
        stream_variations = {};
        stream_world = world;
        stream_room = room;
    }
    const uint32_t argument = ctx.esp + 4, original = kernel::R32(argument);
    uint32_t sound = original;
    // Frame 1300 requests Liu even during solo Kung Lao play. Buffer Kung's take
    // under Liu's tag so the later PLAY and DELETE events still find it.
    const bool baraka_kung = original == 0x6935F9CD && IsBarakaSoloKungLao() &&
        HasSoundDefinition(0x530DACE6);
    if (baraka_kung) sound = 0x530DACE6;
    VariationCursor selected_cursor;
    size_t selected_family = StreamFamilies.size();
    if (RestorationEnabled() && world && (room == 194 || room == 195)) {
        for (size_t i = 0; i < StreamFamilies.size(); ++i) {
            const auto& family = StreamFamilies[i];
            if (family.original != original) continue;
            selected_cursor = stream_variations[i];
            uint32_t choice = 0;
            if (original == 0x8D15F4C2 && FindGroupBank(family.group) >= 0 &&
                HasSoundDefinition(0xFB5D948F) && HasSoundDefinition(0xC63DBD3F)) {
                // These defiant takes also match the same failed-fatality
                // recovery phase, although their old separate cue is unbound.
                constexpr uint32_t takes[]{0x8D15F4C2,0xCAB58E12,0xFB5D948F,0xC63DBD3F};
                choice = takes[ChoiceIndex(ctx, selected_cursor, 4)];
                ++selected_cursor.next;
            } else choice = ChooseGroupSound(ctx, family.group, selected_cursor);
            // This take addresses both monks. The shared original group has
            // no mode filter; keep its neutral source outside known Ko-op.
            if (choice == 0x9DCE420D) {
                const auto state = kernel::R32(0x5ACEBC);
                if (!state || !kernel::R32(state + 0x88)) choice = original;
            }
            if (choice && HasSoundDefinition(choice)) { sound = choice; selected_family = i; }
            break;
        }
    }
    std::array<uint32_t, 5> before{}, before_buffered{};
    for (size_t i = 0; i < before.size(); ++i) {
        const auto slot=0x69654C + uint32_t(i) * 12;
        before[i] = kernel::R32(slot);
        before_buffered[i] = kernel::R32(slot + 4);
    }
    ScopedGuestWord selected(argument, sound);
    __imp__sub_0017C040(ctx, base);
    if (baraka_kung) {
        // Retag only newly buffered handles; leave existing or failed buffers intact.
        for (size_t i = 0; i < before.size(); ++i) {
            const auto at = 0x69654C + uint32_t(i) * 12;
            if (kernel::R32(at) == sound && kernel::R32(at + 4) &&
                (before[i] != sound || before_buffered[i] != kernel::R32(at + 4))) {
                kernel::W32(at, original);
                break;
            }
        }
    }
    if (selected_family < stream_variations.size()) {
        for (size_t i = 0; i < before.size(); ++i) {
            const auto at = 0x69654C + uint32_t(i) * 12;
            if (!before[i] && kernel::R32(at) == sound && kernel::R32(at + 4)) {
                stream_variations[selected_family] = selected_cursor;
                break;
            }
        }
    }
}

// Regular voice update: releases the handle once the native sound stops playing.
X86_EXTERN_FUNC(__imp__sub_001A1880);
X86_FUNC(sub_001A1880) {
    __imp__sub_001A1880(ctx, base);
    UpdateCageCaption(ctx);
    UpdateDialogue(ctx);
}

// Native animstreamplay (0x717D0), reached through its original thunk.
X86_EXTERN_FUNC(__imp__sub_00014985);
X86_FUNC(sub_00014985) {
    const uint32_t scene = kernel::R32(SceneName);
    queue.Clear();
    __imp__sub_00014985(ctx, base);
    cage.pending = false;
    if (RestorationEnabled() && !cage.delivered && scene == CageScene &&
        kernel::R32(CurrentRoom) == CageRoom && kernel::G<uint8_t>(SceneLoadingOrPlaying)) {
        cage.room = CageRoom;
        cage.world = kernel::R32(World);
        cage.pending = true;
    }
}

X86_EXTERN_FUNC(__imp__sub_00170330);
X86_FUNC(sub_00170330) {
    const uint32_t event = kernel::R32(ctx.esp + 4), actor = kernel::R32(ctx.esp + 8);
    const uint32_t args = kernel::R32(event + 8), player = kernel::R32(actor + 0x10);
    const uint32_t enable = kernel::R32(args), target = kernel::R32(args + 4), index = kernel::R32(args + 8);
    __imp__sub_00170330(ctx, base);
    if (RestorationEnabled() && kernel::R32(CurrentRoom) == 137 && enable == 1 &&
        target == 0x91FD0438u && index == 19) {
        RefreshSupplemental(forest_snakes, kernel::R32(World), 137);
        forest_snakes_triggered = true;
    }
    // Freeing Kitana expires the three-woman taunt, including pending combat takes.
    if (kernel::R32(CurrentRoom) == 195 && enable == 1 &&
        ((target == 2447738644u && index == 4) ||
         (target == 3172261475u && index == 44))) {
        RefreshSupplemental(mileena_trio_opening, kernel::R32(World), 195);
        mileena_trio_opening.delivered = true;
        mileena_monastery_combat.pending = 0;
        mileena_monastery_combat.canceled = true;
    }
    if (kernel::R32(CurrentRoom) == 160 && enable == 1 &&
        target == 2390456960u && index == 110) { // Original _d_death52
        RefreshSupplemental(scorpion_combat, kernel::R32(World), 160);
        scorpion_combat.delivered = true;
    }
    // Water levers expire the opening hint before any victory stream can begin.
    if (RestorationEnabled() && kernel::R32(CurrentRoom) == 106 && enable == 0) {
        switch (target) {
        case 1704478846u: case 1320491965u: case 1471032060u:
        case 418364475u: case 805076000u: case 3069521306u:
        case 3253869836u: case 1364000925u:
            RefreshSupplemental(wu_shi_fire, kernel::R32(World), 106);
            wu_shi_fire.delivered = true;
            break;
        default: break;
        }
    }
    if (!RestorationEnabled() || enable != 1) return;
    const uint32_t room = kernel::R32(CurrentRoom), world = kernel::R32(World);
    if (cue_room != room || cue_world != world) {
        recovered_cues.fill(0);
        kitana_trap = 0;
        escape_warning = 0;
        activation_seen = {};
        cue_room = room;
        cue_world = world;
    }
    if (room == 108) {
        if (ermac_room != room || ermac_world != world) {
            ermac_phase_voices = {};
            ermac_world = world;
            ermac_room = room;
        }
        // bossentersphase uses the script controller, so handle it before actor validation.
        if (target == 0x5A3297F2u && index == 13) {
            if (!ermac_phase_voices[0].delivered) {
                ermac_phase_voices[0].requested = true;
                ermac_phase_voices[0].ready = true;
            }
        } else if (target == 0x7873AA87u && index == 0xFFFFFFFFu) {
            ermac_phase_voices[0].requested = false;
            if (!ermac_phase_voices[1].delivered) {
                ermac_phase_voices[1].requested = true;
                // The stone-cage line waits for the later cage effect activation.
                ermac_phase_voices[1].ready = false;
            }
        } else if (target == 0x9113964Eu && index == 18 &&
                   ermac_phase_voices[1].requested) {
            ermac_phase_voices[1].ready = true;
        }
    }
    if (player >= ActorCount) return;
    const auto character = kernel::G<int16_t>(Players + player * PlayerStride + ActorFlag);
    const auto actor_bit = uint16_t(1u << player);
    for (size_t i = 0; i < std::size(ActivationCues); ++i) {
        const auto& cue = ActivationCues[i];
        if (cue.room != room || cue.target != target || cue.ordinal != index ||
            (cue.actor >= 0 && cue.actor != character) || (activation_seen[i] & actor_bit) ||
            !HasSoundDefinition(cue.sound)) continue;
        if (Enqueue(CaptureLine(cue.sound, 0x871111EC, player))) activation_seen[i] |= actor_bit;
    }
    // _d_trapwall enables _d_snd_trap after 200 ticks. Kitana's threat belongs to
    // this captive phase and must not interrupt the later freeing cinematic.
    if (room == 195 && target == 0xD11A17FD && index == 69 && character == 65 && !(kitana_trap & actor_bit)) {
        auto line = CaptureLine(0x9B1AFD52, 0x871111EC, player);
        line.after_cinematic = true;
        if (Enqueue(line)) kitana_trap |= actor_bit;
    }
    if (index != 0xFFFFFFFF) return;
    const bool liu_escape = character == 0 && (target == 0x983DC662 || target == 0x9F8C2AF9);
    const bool kung_escape = character == 1 && (target == 0x29AEB768 || target == 0xB7CA22CB);
    if (room == 96 && !(escape_warning & actor_bit) && (liu_escape || kung_escape)) {
        // Quake-hit events target deleted definitions. Reuse their timing for
        // escape warnings, once per actor across both hit families.
        const uint32_t state = kernel::R32(0x5ACEBC);
        if (state) {
            // Native isoneplayer (0x740B0) tests the game state's +0x88 field.
            const bool solo = kernel::R32(state + 0x88) == 0;
            const uint32_t sound = liu_escape ? (solo ? 0xBEC5353E : 0x0CE5E92E)
                                              : (solo ? 0xEA53782A : 0xADF302FA);
            if (Enqueue(CaptureLine(sound, 0x871111EC, player))) {
                escape_warning |= actor_bit;
            }
        }
    }
    for (size_t i = 0; i < MissingCues.size(); ++i) {
        const auto& cue = MissingCues[i];
        if (cue.room != room || cue.target != target || (recovered_cues[i] & actor_bit) ||
            (cue.character >= 0 && cue.character != character)) continue;
        auto line = CaptureLine(cue.sound, 0x871111EC, player);
        line.after_cinematic = true;
        if (Enqueue(line)) {
            recovered_cues[i] |= actor_bit;
        }
    }
}

