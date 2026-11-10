#include "dialogue_queue.h"
#include "dialogue_recovery_catalog.h"
#include "difficulty.h"
#include "survival_mode.h"
#include "survival_session.h"
#include "ed_nexus.h"
#include "launcher.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <algorithm>
#include <cstdio>
#include <bit>
#include <chrono>
#include <array>
#include <unordered_set>

// vosound (0x1A18F0) drops requests while its single voice slot is busy; defer them until it is free.
void ResetDialogueState();  // Room-load cleanup, also run after a death/retry.
namespace {
constexpr uint32_t VoiceHandle = 0x6C03DC, CharacterBank = 0x6C03D8, SoundManager = 0x653BC0;
constexpr uint32_t VoiceRequest = 0x653C68, CurrentRoom = 0x505970;
// Retail bug: fatality music (16C9E from 0x3EAF0/0x82FC0) asks for silence while a 0x4E69E0 stinger (0.5 s) is buffered, so cinematic fatalities had none.
struct FatalityMusicCall { uint32_t caller, music; };
constexpr std::array<FatalityMusicCall, 2> FatalityMusicCalls{{{0x3EC4A, 0x3637C236}, {0x83080, 0x2CB765E}}};
constexpr uint32_t FatalitySceneList = 0x4E69E0;  // NUL-terminated, read by 0x822C0
// The Xbox port changed these 12 scene sounds (category 6D72DE68/type 2 -> E8DF302F/type 1), silencing the music.
constexpr std::array<uint32_t, 12> FatalitySceneSounds{0x0589A72D, 0x07183053, 0x0797EE85, 0x3262BC14,
    0x469D6FFA, 0x4ED4183D, 0x682A48EF, 0xA47FFE3B, 0xBD1F92B2, 0xCD72F608, 0xCF48698B, 0xD0B1679D};
constexpr uint32_t XboxFatalityCategory = 0xE8DF302F, Ps2FatalityCategory = 0x6D72DE68;
constexpr uint32_t Players = 0x5D1AB8, PlayerStride = 0x3238, ActorCount = 14;
constexpr uint32_t ActorFlag = 0x1DFE, PlayerBank = 0x2429;
constexpr uint32_t CageRoom = 98, CageScene = 0x2D2875B0;
constexpr uint32_t SceneName = 0x514508, SceneLoadingOrPlaying = 0x514505;
constexpr uint32_t World = 0x639224, MessageManager = 0x4E6E20;

openshao::dialogue::Queue queue;
std::chrono::steady_clock::time_point last_update{};
// Silence required before a restored line or caption; `silent` is the last update's verdict.
double VoiceGap = 0.8, SceneGap = 1.5;
double quiet = 0, gap = VoiceGap;
bool silent = false;
bool cinematic_active = false;
// Our additions play once per attempt: revisiting a map keeps them heard, while a death/retry
// checkpoint restore or returning to the front end (no world) lets them play again.
std::unordered_set<uint32_t> attempt_heard;
uint32_t attempt_restores = 0;
bool attempt_reset = false;  // set when a checkpoint restore cleared the attempt
// Campaign starts with Raiden's intro (7922A984), which a totem save never replays; it stays
// "unsaved" until a slot is saved or loaded (used by tutorial-only additions).
constexpr uint32_t CampaignIntroScene = 0x7922A984;
struct { bool fresh = false; uint32_t start = 0; } campaign;
void NoteCampaignScene(uint32_t scene) {
    if (scene == CampaignIntroScene && kernel::R32(CurrentRoom) == 96)
        campaign = {true, openshao::difficulty::SaveSlotTransfers()};
}
bool CampaignUnsaved() {
    return campaign.fresh && openshao::difficulty::SaveSlotTransfers() == campaign.start;
}

void SyncAttempt() {
    const uint32_t restores = openshao::difficulty::CheckpointRestores();
    if (restores != attempt_restores) {
        attempt_restores = restores;
        attempt_heard.clear();
        attempt_reset = true;
    }
    if (!kernel::R32(World)) attempt_heard.clear();
}
int32_t FindGroupBank(uint32_t sound);
// Variation groups (e.g. a freeing reaction family) repeat like native speech; direct takes do not.
bool HeardThisAttempt(uint32_t sound) {
    SyncAttempt();
    return attempt_heard.count(sound) != 0 && FindGroupBank(sound) < 0;
}

bool Enqueue(const openshao::dialogue::Line& line) {
    // An addition already heard this attempt counts as delivered.
    if (!line.timed && HeardThisAttempt(line.sound)) return true;
    const bool empty = !queue.Size();
    if (!queue.Push(line)) return false;
    if (empty) { last_update = std::chrono::steady_clock::now(); quiet = 0; }
    return true;
}
struct CageCaption {
    uint32_t room = 0, world = 0;
    bool pending = false, delivered = false;
    double waited = 0;
} cage;
struct ScriptVoice {
    uint32_t sound = 0, handle = 0;
    uint64_t serial = 0;
} script_voice;
uint64_t voice_serial = 0;
uint32_t optional_voice = 0;  // Handle of a playing addition of ours.
struct AttackTutorial {
    uint32_t world = 0, room = 0, scene = 0;
    ScriptVoice voice;
    bool pending = false;
    std::array<uint32_t, 16> message{};
} attack_tutorial;
struct WuShiContext {
    uint32_t world = 0, room = 0;
    bool gate_finished = false;
} wu_shi_context;

void RefreshWuShiContext() {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (wu_shi_context.world != world || wu_shi_context.room != room)
        wu_shi_context = {world, room};
}
// Forest Map 9 acid hints repeat every 800 updates; play each monk's once per visit.
struct { uint32_t world = 0, room = 0; uint8_t played = 0; } acid_hint;
bool ObsoleteWuShiVoice(uint32_t sound) {
    RefreshWuShiContext();
    if (wu_shi_context.room != 96) return false;
    switch (sound) {
    case 0xF3D35938: case 0x5873A43A: case 0x3185C09E: case 0x64DC7FC9: case 0xBF0AC77D:
        // The intro's delayed children can fire after the next cinematic began.
        return kernel::R32(SceneName) && kernel::R32(SceneName) != 0x7922A984;
    case 0x2AD7B0C5: case 0x59BC5679: case 0x826AEECD:
        return wu_shi_context.gate_finished;
    default: return false;
    }
}
void TrackScriptVoice(uint32_t sound, uint32_t handle) {
    if (!handle) return;
    script_voice = {sound, handle, ++voice_serial};
    optional_voice = 0;  // PlayLine re-marks its own additions; a reused handle is native speech.
    if (sound == 0x526CF529 || sound == 0xDF681048) acid_hint.played |= sound == 0x526CF529 ? 1 : 2;
}

struct MissingCue { uint32_t room, target, sound; int16_t character; };
// These activatetrigger targets have ordinal -1 and no definition; retain their script timing.
// Owner removed Raiden's vo_rdn_help_gl_ps2_13 (room 96 B001C237 -> 822C1D1F) from the story.
constexpr std::array<MissingCue, 6> MissingCues{{
    {101, 0xC1CE6AC6, 0x7CE0B357, 0}, {101, 0x0EAC5D51, 0xE0967133, 1},
    {101, 0xFB84C07D, 0x3B40C987, 0}, {101, 0x2326CE4B, 0xDDF65883, 1},
    {101, 0xCB402320, 0x0620E037, 0}, {101, 0x6465CF88, 0xA7360BE3, 1},
}};
std::array<uint16_t, MissingCues.size()> recovered_cues{};
uint32_t cue_room = 0, cue_world = 0;
uint16_t kitana_trap = 0;
// Gates opened when an arena is cleared (spawner-driven, no event of their own):
// _d_door05up -> (a)_d_move02 and _d_door05_b -> (a)_d_move_exit mark the opening.
constexpr std::array<std::pair<uint32_t, uint32_t>, 2> GateOpenings{{
    {0x9406D458, 142}, {0x9A955116, 33}}};
uint32_t gate_warning_world = 0;  // World whose gate warning already played.
constexpr double GateWarningDelay = 1.5;  // Owner's choice: a beat after the gate starts opening.
struct { uint32_t world = 0, sound = 0; double age = 0; } gate_warning;  // Armed, not yet played.
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
// Ermac's phase lines: kept through cinematic cleanup, delivered on his live actor.
struct ErmacPhaseVoice {
    bool requested = false, ready = false, queued = false, delivered = false;
    bool canceled = false;
    bool scene_observed = false;
    uint32_t scene = 0;
    double waited = 0;
};
std::array<ErmacPhaseVoice, 2> ermac_phase_voices{};
uint32_t ermac_world = 0, ermac_room = 0;
constexpr std::array<uint32_t, 2> ErmacPhaseSounds{0x14DC5B86u, 0x29BC7236u};
// Ermac's first line (vo_erm_wu_shi_13_01, unused in retail) opens his presentation:
// _d_staterm3 -> _d_ermac (769191E3/3) spawns him from the statue.
constexpr uint32_t ErmacIntroTarget = 0x769191E3, ErmacIntroOrdinal = 3, ErmacIntroSound = 0x537C2156;
constexpr double ErmacIntroDelay = 0.5;
struct { uint32_t world = 0; double age = 0; bool pending = false, done = false; } ermac_intro;
uint32_t phase_world = 0, phase_room = 0;
uint32_t followup_world = 0, followup_room = 0, followup_seen = 0;
uint32_t forest_hint_world = 0, forest_hint_room = 0;
bool forest_hint_responded = false;
struct SupplementalCue {
    uint32_t world = 0, room = 0;
    bool observed = false, delivered = false, finished = false;
    uint32_t scene = 0;
    bool scene_bound = false;
    double waited = 0;
    bool canceled = false;
    bool Done() const { return delivered || canceled; }
};
SupplementalCue monastery_intro, forest_entrance, forest_second_tree,
    shrine_offering, wu_shi_fire, mileena_trio_opening, scorpion_combat,
    monastery_fist, foundry_bridge, forest_snakes, wu_shi_combo;
SupplementalCue* const SupplementalCues[]{&monastery_intro, &forest_entrance, &forest_second_tree,
    &shrine_offering, &wu_shi_fire, &mileena_trio_opening, &scorpion_combat,
    &monastery_fist, &foundry_bridge, &forest_snakes, &wu_shi_combo};
uint32_t foundry_bridge_ticks = 0;
struct { uint32_t handle = 0, sound = 0; double age = 0; } voice_watch;  // Release diagnostics.
bool forest_snakes_triggered = false;
// Keep the _d_snakes handle: the five-slot registry can clear its tag before the next tick.
uint32_t forest_snakes_stream_handle = 0;
bool forest_snakes_cutscene_seen = false;
struct MileenaMonasteryCombat {
    uint32_t world = 0, room = 0, pending = 0, mileena_id = 0;
    uint32_t scene = 0;
    double waited = 0;
    uint8_t next = 0;
    bool canceled = false;
} mileena_monastery_combat;
uint32_t native_world = 0, native_room = 0;
VariationCursor spear_choice, fire_choice, shao_choice;
bool shao_challenge = false;
// The lead monk opens the Shao Kahn fight as his intro scene (60F8EA8D) ends, before his first taunt:
// Liu found_10_05_2 / Kung found_10_04_2 ("You have no honor").
constexpr uint32_t ShaoIntroScene = 0x60F8EA8D;
struct { uint32_t world = 0; bool armed = false, seen_scene = false; } shao_opener;

struct ScopedGuestWord {
    uint32_t address, value;
    ScopedGuestWord(uint32_t at, uint32_t replacement) : address(at), value(kernel::R32(at)) {
        kernel::W32(address, replacement);
    }
    ~ScopedGuestWord() { kernel::W32(address, value); }
};

bool RestorationEnabled() { return openshao::launcher::Current().restore_dialogue; }

// Caption sound 0x6C4: 0 or FFFFFFFF (portrait prompts such as Baraka's impale) means no caption voice.
bool CaptionHasVoice() {
    const uint32_t sound = kernel::R32(MessageManager + 0x6C4);
    return sound && sound != 0xFFFFFFFFu;
}

// Time a voiced caption has been shown without creating its voice (updated every voice tick).
double caption_pending_age = 0;
constexpr double CaptionVoiceStart = 0.5;

uint32_t CaptionVoiceHandle() {
    if (!kernel::G<uint8_t>(MessageManager + 0x6C0)) return 0;
    const uint32_t handle = kernel::R32(MessageManager + 0x6C8);
    // Reserve the slot between caption submission and native voice creation, but only briefly:
    // a portrait prompt (e.g. Baraka's impale, template 8) is voiced by its script's vosound instead.
    return handle ? handle : CaptionHasVoice() && caption_pending_age < CaptionVoiceStart ? 0xFFFFFFFFu : 0;
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

// Number of takes in a loaded variation group (1 for a plain sound or an unknown group).
uint32_t GroupSize(uint32_t sound) {
    const uint32_t count = kernel::R32(SoundManager + 0xC), banks = kernel::R32(SoundManager + 0x14);
    if (!banks || count > 256) return 1;
    for (uint32_t bank = 0; bank < count; ++bank) {
        const uint32_t entry = banks + bank * 24, groups = kernel::R32(entry + 12);
        if (!kernel::R32(entry + 4) || !groups) continue;
        const uint32_t size = kernel::R32(groups);
        if (size > 4096) continue;
        uint32_t group = groups + 4;
        for (uint32_t i = 0; i < size; ++i) {
            const uint32_t choices = kernel::R32(group + 4);
            if (!choices || choices > 1024) break;
            if (kernel::R32(group) == sound) return choices;
            group += 12 + choices * 4;
        }
    }
    return 1;
}

// Address of a sound's 28-byte definition in the loaded banks, or 0.
uint32_t FindSoundDefinition(uint32_t sound, uint32_t* first = nullptr, uint32_t* last = nullptr) {
    const uint32_t count = kernel::R32(SoundManager + 0xC), banks = kernel::R32(SoundManager + 0x14);
    if (!banks || count > 256) return 0;
    for (uint32_t bank = 0; bank < count; ++bank) {
        const uint32_t entry = banks + bank * 24, header = kernel::R32(entry + 4), definitions = kernel::R32(entry + 8);
        if (!header || !definitions) continue;
        const uint32_t size = kernel::R32(header + 0x18);
        if (size > 8192) continue;
        for (uint32_t i = 0; i < size; ++i)
            if (kernel::R32(definitions + i * 28) == sound) {
                if (first) *first = definitions;
                if (last) *last = definitions + (size - 1) * 28;
                return definitions + i * 28;
            }
    }
    return 0;
}
// Category volume used by the native player (0x150CC0): table 0x653C14 {id, group, volume}.
float CategoryVolume(uint32_t category) {
    const uint32_t table = kernel::R32(0x653C54), entries = kernel::R32(0x653C14);
    if (!table || !entries) return 1.f;
    const uint32_t count = kernel::R32(table);
    for (uint32_t i = 0; i < count && i < 256; ++i)
        if (kernel::R32(entries + i * 12) == category) return std::bit_cast<float>(kernel::R32(entries + i * 12 + 8));
    return 1.f;
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

// Diagnostics: which sound banks are loaded (slot, header, definition count, first hash).
void LogSoundBanks(const char* reason) {
    const uint32_t count = kernel::R32(SoundManager + 0xC), banks = kernel::R32(SoundManager + 0x14);
    if (!banks || count > 256) return;
    char text[1024];
    int used = std::snprintf(text, sizeof(text), "Sound banks (%s) room=%u n=%u:", reason,
                             kernel::R32(CurrentRoom), count);
    for (uint32_t bank = 0; bank < count && used > 0 && used < int(sizeof(text)) - 40; ++bank) {
        const uint32_t entry = banks + bank * 24, header = kernel::R32(entry + 4), definitions = kernel::R32(entry + 8);
        if (!header || !definitions) continue;
        used += std::snprintf(text + used, sizeof(text) - used, " [%u %08X %u %08X]", bank, header,
                              kernel::R32(header + 0x18), kernel::R32(definitions));
    }
    LOGD("%s", text);
}

// Liu's standalone take in Baraka's encounter, solo Kung Lao only.
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

// Wu Shi Map 08 (room 103): remark on the 2nd catapult launch (an enemy fires the 1st),
// then on the next barricade broken.
constexpr uint32_t CatapultRoom = 103, CatapultEngage = 0xF5F029C2, BarricadeBreak = 0xF67B4949;
uint8_t catapult_remarks = 0;  // bit 0: catapult line queued, bit 1: barricade line queued
uint8_t catapult_launches = 0;

void RefreshNativeEncounter() {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (native_world == world && native_room == room) return;
    spear_choice = fire_choice = shao_choice = {};
    shao_challenge = false;
    catapult_remarks = 0;
    catapult_launches = 0;
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
        // The original group keeps one slot per native take, so every take is equally likely.
        std::array<uint32_t, 64> candidates{};
        uint32_t count = std::clamp(GroupSize(sound), 1u, 32u);
        const uint32_t native_slots = count;
        for (uint32_t j = 0; j < family.takes.size() && j < 31; ++j) {
            const auto& take = family.takes[j];
            if ((take.once && (selection.seen & (1u << j))) ||
                (take.mode != openshao::dialogue::TakeMode::Any && (!mode_state ||
                 (take.mode == openshao::dialogue::TakeMode::Solo) != solo)) ||
                (take.reaction && !IsHeroReacting()) || !HasSoundDefinition(take.sound)) continue;
            candidates[count++] = j + 1;
        }
        if (count == native_slots) return {};
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

openshao::dialogue::Line CaptureLine(uint32_t sound, uint32_t bank, uint32_t actor, uint32_t source = 0) {
    openshao::dialogue::Line line{sound, bank, actor, kernel::R32(CurrentRoom)};
    line.world = kernel::R32(World);
    line.actor_type = kernel::G<int16_t>(Players + actor * PlayerStride + ActorFlag);
    line.actor_bank = kernel::G<int8_t>(Players + actor * PlayerStride + PlayerBank);
    line.actor_bound = true;
    line.actor_id = kernel::R32(Players + actor * PlayerStride + 0x68);
    line.scene = kernel::R32(SceneName);
    line.source = source;
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
    auto line = CaptureLine(sound, 0x871111EC, actor);
    line.optional = true;
    if (!Enqueue(line)) return false;
    followup_seen |= bit;
    return true;
}

void QueueBarakaSpokenFollowup(X86Context& ctx, uint32_t actor) {
    // Solo never requests the Ko-op 08_1 cue: allow one alternate 08 take after phase 3.
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

void UpdateAttackTutorial(X86Context& ctx) {
    if (!attack_tutorial.pending) return;
    if (attack_tutorial.world != kernel::R32(World) ||
        attack_tutorial.room != kernel::R32(CurrentRoom) ||
        attack_tutorial.scene != kernel::R32(SceneName)) {
        attack_tutorial = {};
        return;
    }
    if (openshao::ed_nexus::GameplayPaused() || kernel::R32(0x6C92F8)) return;
    const auto voice = attack_tutorial.voice;
    if (RestorationEnabled() && script_voice.serial == voice.serial &&
        kernel::R32(VoiceHandle) == voice.handle && IsNativeSoundPlaying(ctx, voice.handle)) return;
    const auto message = attack_tutorial.message;
    attack_tutorial = {};
    // This panel closes the intro; pending lines must not resume inside the tutorial.
    queue.Clear();
    const auto saved = ctx;
    // Own a copy of messagetext's stack descriptor until playback ends.
    ctx.esp -= uint32_t(sizeof(message));
    const uint32_t info = ctx.esp;
    for (uint32_t i = 0; i < message.size(); ++i) kernel::W32(info + i * 4, message[i]);
    ctx.ecx = MessageManager;
    kernel::CallGuest(ctx, 0x135A3, {info, 1});
    ctx = saved;
    LOGD("Dialogue tutorial: submitted text=24 after its introductory voice");
}

bool OriginalAudioStreamBusy(X86Context& ctx, bool include_buffered = true) {
    // Stream registry: tag +0, buffered handle +4, playing handle +8 (separate from vosound).
    for (uint32_t slot = 0; slot < 5; ++slot) {
        const uint32_t entry = 0x69654C + slot * 12;
        // Buffered streams reserve optional dialogue but never block deferred script requests.
        if (include_buffered && kernel::R32(entry + 4)) return true;
        const uint32_t playing = kernel::R32(entry + 8);
        if (!playing) continue;
        if (IsNativeSoundPlaying(ctx, playing)) return true;
    }
    return false;
}

void RefreshSupplemental(SupplementalCue& cue, uint32_t world, uint32_t room) {
    if (cue.world != world || cue.room != room) {
        cue = {};
        cue.world = world;
        cue.room = room;
    } else if (cue.scene_bound && cue.scene != kernel::R32(SceneName)) {
        cue.canceled = true;
    }
}

void BindSupplemental(SupplementalCue& cue) {
    if (cue.scene_bound) return;
    cue.scene = kernel::R32(SceneName);
    cue.scene_bound = true;
}

void ObserveSupplemental(SupplementalCue& cue) {
    BindSupplemental(cue);
    cue.observed = true;
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
    if (source_playing) ObserveSupplemental(cue);
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

void UpdateSupplementalVoices(X86Context& ctx, double elapsed) {
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    RefreshSupplemental(monastery_intro, world, room);
    RefreshSupplemental(forest_entrance, world, room);
    RefreshSupplemental(forest_second_tree, world, room);
    RefreshSupplemental(shrine_offering, world, room);
    RefreshSupplemental(wu_shi_fire, world, room);
    RefreshSupplemental(mileena_trio_opening, world, room);
    RefreshSupplemental(scorpion_combat, world, room);
    RefreshSupplemental(monastery_fist, world, room);
    RefreshSupplemental(wu_shi_combo, world, room);
    if (acid_hint.world != world || acid_hint.room != room) acid_hint = {world, room, 0};
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
    // Deadline starts after the source finishes; contention cannot defer a reply forever.
    for (auto* cue : SupplementalCues) {
        if (!cue->finished || cue->Done()) continue;
        cue->waited += elapsed;
        if (cue->waited > openshao::dialogue::Queue::MaxBlocked) cue->canceled = true;
    }
    if (mileena_monastery_combat.pending) {
        mileena_monastery_combat.waited += elapsed;
        if (mileena_monastery_combat.scene != kernel::R32(SceneName) ||
            mileena_monastery_combat.waited > openshao::dialogue::Queue::MaxBlocked)
            mileena_monastery_combat.pending = 0;
    }
    // MTR2392 spawns the Foundry monks without a speech event: use a settling delay.
    if (room == 74 && !foundry_bridge.Done() &&
        !kernel::G<uint8_t>(SceneLoadingOrPlaying) && !IsCinematicPlaying(ctx) &&
        !openshao::ed_nexus::GameplayPaused() && foundry_bridge_ticks < 90)
        ++foundry_bridge_ticks;
    if (room == 74 && foundry_bridge_ticks >= 90) {
        BindSupplemental(foundry_bridge);
        foundry_bridge.finished = true;
    }
    // Forest Map 10 = room 137; _d_snakes' activation does not prove the scene finished.
    if (room == 137 && forest_snakes_triggered && !forest_snakes.finished &&
        !forest_snakes.Done()) {
        // A scene can animate even when audio playback fails; observe its start and end.
        const bool scene_active = kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx);
        if (scene_active) {
            BindSupplemental(forest_snakes);
            forest_snakes_cutscene_seen = true;
        }
        // Recover handles from native slots; keep the saved handle once they are cleared.
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
            if (playing) ObserveSupplemental(forest_snakes);
            else if (forest_snakes.observed && !buffered) {
                forest_snakes.finished = true;
            }
        } else if (forest_snakes_cutscene_seen && !scene_active &&
                   !OriginalAudioStreamBusy(ctx, false)) {
            // Cinematic completion also permits the follow-up when its soundtrack failed.
            forest_snakes.finished = true;
        }
    }
    if (room == 106 && !wu_shi_fire.finished && !wu_shi_fire.Done())
        ObserveOriginalStreamFinish(ctx, wu_shi_fire, 0x50329483);
    // Wait for the door stream (cs_wu_shi_13_2.adx) before the shrine caption.
    if (room == 108 && !shrine_offering.finished && !shrine_offering.Done()) {
        ObserveOriginalStreamFinish(ctx, shrine_offering, 0x800DFF7A);
    }
    const bool caption_active = kernel::G<uint8_t>(MessageManager + 0x6C0);
    const uint32_t text = kernel::R32(MessageManager + 0x6D8);
    const uint32_t caption_handle = kernel::R32(MessageManager + 0x6C8);
    // Observe displayed map messages; journal lookups must not trigger speech.
    if (caption_active) {
        if (room == 192 && text == 54) ObserveSupplemental(monastery_intro);
        if (room == 128 && text == 55) ObserveSupplemental(forest_entrance);
        if (room == 129 && text == 63 && caption_handle &&
            kernel::R32(MessageManager + 0x6C4) == 0x01DD37EB)
            ObserveSupplemental(forest_second_tree);
        // Text 45 with audio marks the opening three-sister fight.
        if (room == 195 && text == 45 && caption_handle)
            ObserveSupplemental(mileena_trio_opening);
        // Text 100 is the Fist of Ruin acquisition popup (Raiden 4_07).
        if (room == 195 && text == 100)
            ObserveSupplemental(monastery_fist);
    }
    const std::array caption_cues{std::pair{&monastery_intro, 54u},
        std::pair{&forest_entrance, 55u}, std::pair{&forest_second_tree, 63u},
        std::pair{&mileena_trio_opening, 45u}, std::pair{&monastery_fist, 100u}};
    for (const auto& [caption_cue, source_text] : caption_cues)
        if (caption_cue->observed && (!caption_active || text != source_text))
            caption_cue->finished = true;
    if (scorpion_combat.observed && !kernel::R32(VoiceHandle)) scorpion_combat.finished = true;
    SupplementalCue* cue = nullptr;
    uint32_t sound = 0;
    uint32_t cue_actor = 0;
    if (room == 74 && foundry_bridge.finished && !foundry_bridge.Done()) {
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        sound = hero == 0 ? 0xA09578E5u : hero == 1 ? 0x7B43C051u : 0;
        cue = &foundry_bridge;
    } else if (room == 96 && wu_shi_combo.finished && !wu_shi_combo.Done()) {
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        sound = hero == 0 ? 0xBF0AC77Du : hero == 1 ? 0x64DC7FC9u : 0;
        cue = &wu_shi_combo;
    } else if (room == 137 && forest_snakes_triggered &&
               forest_snakes.finished && !forest_snakes.Done()) {
        sound = 0x919A865Fu; // Raiden LF10_01_3 in the local Forest bank
        cue = &forest_snakes;
    } else if (room == 195 && monastery_fist.finished && !monastery_fist.Done() &&
        kernel::G<int16_t>(Players + ActorFlag) == 0) {
        sound = 0xA6A85EC8u; // VO_LIUKANG_EM_5_02 in room195's native bank
        cue = &monastery_fist;
    } else if (room == 192 && monastery_intro.observed && !monastery_intro.Done()) {
        sound = 0xD64BB4B8u;
        cue = &monastery_intro;
    } else if (room == 128 && forest_entrance.observed && !forest_entrance.Done() &&
               kernel::G<int16_t>(Players + ActorFlag) == 0) {
        sound = 0xC629944Cu;
        cue = &forest_entrance;
    } else if (room == 129 && forest_second_tree.observed &&
               !forest_second_tree.Done()) {
        // Raiden elaborates on Smoke's authentic tree hint; both play in order.
        sound = 0xC33F8FCEu;
        cue = &forest_second_tree;
    } else if (room == 108 && shrine_offering.finished &&
               !shrine_offering.Done()) {
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        sound = hero == 0 ? 0x29314E8Bu : hero == 1 ? 0xE5E6EFA6u : 0;
        cue = &shrine_offering;
    } else if (room == 195 && mileena_trio_opening.observed &&
               !mileena_trio_opening.Done()) {
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
               !scorpion_combat.Done()) {
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
    } else if (room == 106 && wu_shi_fire.finished && !wu_shi_fire.Done()) {
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
    // After the snake scene, buffered streams must not starve Raiden's follow-up.
    if (OriginalAudioStreamBusy(ctx, cue != &forest_snakes)) return;

    auto line = CaptureLine(sound, 0x871111EC, cue_actor);
    line.optional = true;
    line.blocked = cue ? cue->waited : mileena_monastery_combat.waited;
    line.wait_for_streams = true;
    if (HasSoundDefinition(sound) && Enqueue(line)) {
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
        line.scene == kernel::R32(SceneName);
}

bool IsCinematicPlaying(X86Context& ctx) {
    const auto saved = ctx;
    ctx.ecx = 0x5144E8;
    const auto result = kernel::CallGuest(ctx, 0x1A76D0, {});
    ctx = saved;
    return uint8_t(result) == 1;
}


void UpdateErmacPhaseVoices(X86Context& ctx, double elapsed, bool cinematic) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (ermac_world != world || ermac_room != room || !RestorationEnabled() || !world) {
        ermac_phase_voices = {};
        ermac_world = world;
        ermac_room = room;
    }
    if (!RestorationEnabled() || !world || room != 108) return;
    for (auto& phase : ermac_phase_voices) {
        if (!phase.requested || phase.delivered) continue;
        if (cinematic && !phase.scene_observed) {
            phase.scene_observed = true;
            phase.scene = kernel::R32(SceneName);
        }
        if (phase.ready) phase.waited += elapsed;
        if ((phase.scene_observed && phase.scene != kernel::R32(SceneName)) ||
            phase.waited > openshao::dialogue::Queue::MaxBlocked) {
            phase.requested = false;
            phase.canceled = true;
        }
    }
    // Retry lines dropped by scene or actor cleanup until native playback confirms delivery.
    for (auto& phase : ermac_phase_voices)
        if (phase.queued && !phase.delivered && !queue.Size()) phase.queued = false;
    if (cinematic ||
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
            auto line = CaptureLine(ErmacPhaseSounds[i], 0x871111ECu, slot);
            line.blocked = phase.waited;
            line.wait_for_streams = true;
            if (Enqueue(line)) {
                phase.queued = true;
            }
            return;
        }
    }
}

void UpdateErmacIntro(double elapsed, bool paused) {
    if (!ermac_intro.pending) return;
    if (!RestorationEnabled() || kernel::R32(World) != ermac_intro.world || kernel::R32(CurrentRoom) != 108) {
        ermac_intro.pending = false;
        return;
    }
    if (paused || (ermac_intro.age += elapsed) < ErmacIntroDelay) return;
    uint32_t speaker = ActorCount;
    for (uint32_t slot = 0; slot < ActorCount && speaker == ActorCount; ++slot)
        if (kernel::G<int16_t>(Players + slot * PlayerStride + ActorFlag) == 68 &&
            kernel::R32(Players + slot * PlayerStride + 0x68)) speaker = slot;
    // The spawn can take a moment to create the actor; give up after a few seconds.
    if (speaker == ActorCount) {
        if (ermac_intro.age > 5) ermac_intro.pending = false;
        return;
    }
    ermac_intro.pending = false;
    if (!HasSoundDefinition(ErmacIntroSound)) {
        LOGD("Dialogue Ermac intro: %08X not loaded", ErmacIntroSound);
        return;
    }
    if (Enqueue(CaptureLine(ErmacIntroSound, 0x871111ECu, speaker))) LOGD("Dialogue Ermac intro queued");
}

void UpdateCageCaption(X86Context& ctx) {
    if (!cage.pending) return;
    if (!RestorationEnabled() || kernel::R32(CurrentRoom) != cage.room ||
        kernel::R32(World) != cage.world || kernel::R32(SceneName) != CageScene) {
        cage.pending = false;
        return;
    }
    // A skip can clear a new message this frame; delivery needs the native voice to start.
    if (kernel::G<uint8_t>(MessageManager + 0x6C0) &&
        kernel::R32(MessageManager + 0x6D8) == 33 &&
        kernel::R32(MessageManager + 0x6C4) == 0x68D2A09B &&
        kernel::R32(MessageManager + 0x6C8)) {
        cage.pending = false;
        cage.delivered = true;
        LOGD("Restored Cage platform caption: text=33 template=5 room=%u", cage.room);
        return;
    }
    // The cinematic flag stays zero while the stream loads; the lifetime flag covers cleanup.
    if (!silent || kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx) ||
        openshao::ed_nexus::GameplayPaused() || kernel::R32(VoiceHandle) || queue.Size() ||
        kernel::G<uint8_t>(MessageManager + 0x6C0) || kernel::G<uint8_t>(0x4E67C4) ||
        kernel::G<uint8_t>(0x4E66F4) || kernel::G<uint8_t>(0x59E095) ||
        kernel::R32(0x5B22FC) || int32_t(kernel::R32(0x6C0404)) > 0 ||
        kernel::R32(0x5146E4) || int32_t(kernel::R32(MessageManager + 0x6FC)) > 0) return;

    const auto saved = ctx;
    // mtxt_platform has no trigger: use the timed template of the Johnny Cage lines.
    kernel::CallGuest(ctx, 0x1385F, {33, 5});
    ctx = saved;
    silent = false;
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
    TrackScriptVoice(line.sound, handle);
    optional_voice = line.optional ? handle : 0;
    if (handle && !line.timed && FindGroupBank(line.sound) < 0) attempt_heard.insert(line.sound);
    return handle;
}

// vo_liukang_wu_shi_1_03/1_04 or vo_kunglao_wu_shi_1_01/1_02 (Ko-op/solo) for player 1.
uint32_t EscapeWarningSound() {
    const auto character = kernel::G<int16_t>(Players + ActorFlag);
    const uint32_t state = kernel::R32(0x5ACEBC);
    if ((character != 0 && character != 1) || !state) return 0;
    // Native isoneplayer (0x740B0) tests the game state's +0x88 field.
    const bool solo = kernel::R32(state + 0x88) == 0;
    return character == 0 ? (solo ? 0xBEC5353Eu : 0x0CE5E92Eu) : (solo ? 0xEA53782Au : 0xADF302FAu);
}

// Tombs Map 13 (room 12): Orochi devours Tarkatans to heal. The hero remarks on it once per visit:
// vo_liukang_st_13_03 / vo_kunglao_st_13_03 ("he's eating the Tarkatans to get his energy back").
constexpr uint32_t OrochiRoom = 12, HealthComponent = 0x200, Health = 0x368, MaxHealth = 0x370;
struct OrochiFeast {
    uint32_t world = 0;
    bool done = false;
    std::array<uint32_t, ActorCount> id{};
    std::array<int32_t, ActorCount> health{};
} orochi_feast;

// Id of a living non-hero actor in a slot (health component +0x200, health +0x368), or 0.
uint32_t LivingEnemy(uint32_t slot) {
    const uint32_t at = Players + slot * PlayerStride, id = kernel::R32(at + 0x68);
    const auto flag = kernel::G<int16_t>(at + ActorFlag);
    const uint32_t component = id ? kernel::R32(at + HealthComponent) : 0;
    if (flag == 0 || flag == 1 || !component || (component & 3)) return 0;
    return int32_t(kernel::R32(component + Health)) > 0 ? id : 0;
}

// Wu Shi Map 06: the warlord spawns in the same tick as 94FE469F, so a new arrival is any
// enemy missing from the previous update's snapshot.
struct { uint32_t world = 0; bool armed = false; std::array<uint32_t, ActorCount> before{}; } warlord_watch;
std::array<uint32_t, ActorCount> enemies_last_update{};

void ArmWarlordWatch() {
    warlord_watch = {kernel::R32(World), true, enemies_last_update};
}

void NoteEnemies() {
    if (kernel::R32(CurrentRoom) != 101) { enemies_last_update = {}; return; }
    for (uint32_t slot = 0; slot < ActorCount; ++slot) enemies_last_update[slot] = LivingEnemy(slot);
}

bool WarlordPresent() {
    if (!warlord_watch.armed || warlord_watch.world != kernel::R32(World)) return false;
    for (uint32_t slot = 0; slot < ActorCount; ++slot) {
        const uint32_t id = LivingEnemy(slot);
        if (id && std::find(warlord_watch.before.begin(), warlord_watch.before.end(), id) ==
                  warlord_watch.before.end()) return true;
    }
    return false;
}

void UpdateOrochiFeast() {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (room != OrochiRoom || orochi_feast.world != world) {
        orochi_feast = {};
        orochi_feast.world = room == OrochiRoom ? world : 0;
    }
    if (room != OrochiRoom || !world || !RestorationEnabled()) return;
    for (uint32_t slot = 0; slot < ActorCount; ++slot) {
        const uint32_t at = Players + slot * PlayerStride, id = kernel::R32(at + 0x68);
        const auto flag = kernel::G<int16_t>(at + ActorFlag);
        const uint32_t component = id ? kernel::R32(at + HealthComponent) : 0;
        if (!component || (component & 3) || flag == 0 || flag == 1) { orochi_feast.id[slot] = 0; continue; }
        const int32_t health = int32_t(kernel::R32(component + Health));
        const int32_t maximum = int32_t(kernel::R32(component + MaxHealth));
        const int32_t previous = orochi_feast.health[slot];
        orochi_feast.health[slot] = health;
        if (orochi_feast.id[slot] != id) { orochi_feast.id[slot] = id; continue; }
        if (orochi_feast.done || previous <= 0 || health <= previous || maximum <= 0 || health > maximum) continue;
        LOGD("Dialogue Orochi heal: slot=%u flag=%d health %d -> %d of %d", slot, flag, previous, health, maximum);
        const auto hero = kernel::G<int16_t>(Players + ActorFlag);
        const uint32_t sound = hero == 0 ? 0xAE950209u : hero == 1 ? 0x94AD5722u : 0;
        if (!sound || !HasSoundDefinition(sound)) {
            LOGD("Dialogue Orochi heal: hero %d remark %08X not loaded", hero, sound);
            orochi_feast.done = true;
            continue;
        }
        auto line = CaptureLine(sound, 0x871111EC, 0);
        line.optional = true;
        if (Enqueue(line)) orochi_feast.done = true;
    }
}

// Wu Shi Map 06 first save totem: when its prompt (text 60) appears, Raiden explains shrines
// (vo_rdn_wu_shi_6_01). Tutorial only: new, unsaved campaigns.
constexpr uint32_t SaveShrineRoom = 101, SaveShrinePrompt = 60, SaveShrineHint = 0x4B368C31;
struct { uint32_t world = 0, room = 0; bool done = false; } save_shrine_hint;

void UpdateSaveShrineHint() {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (save_shrine_hint.world != world || save_shrine_hint.room != room) save_shrine_hint = {world, room, false};
    if (!RestorationEnabled() || !world || room != SaveShrineRoom || save_shrine_hint.done ||
        !kernel::G<uint8_t>(MessageManager + 0x6C0) ||
        kernel::R32(MessageManager + 0x6D8) != SaveShrinePrompt) return;
    save_shrine_hint.done = true;
    if (!CampaignUnsaved()) return;
    if (!HasSoundDefinition(SaveShrineHint)) {
        LOGD("Dialogue save shrine hint: %08X not loaded", SaveShrineHint);
        return;
    }
    if (Enqueue(CaptureLine(SaveShrineHint, 0x871111EC, 0))) LOGD("Dialogue save shrine hint queued");
}

void UpdateShaoOpener(X86Context& ctx, bool cinematic, bool paused) {
    if (!shao_opener.armed) return;
    if (!RestorationEnabled() || kernel::R32(World) != shao_opener.world || kernel::R32(CurrentRoom) != 73) {
        shao_opener = {};
        return;
    }
    // Wait for the intro to start and then end: the fight begins as it closes.
    if (cinematic || kernel::R32(0x6C92F8)) { shao_opener.seen_scene = true; return; }
    if (!shao_opener.seen_scene || paused) return;
    shao_opener.armed = false;
    const auto hero = kernel::G<int16_t>(Players + ActorFlag);
    const uint32_t opener = hero == 0 ? 0x2AFD818Fu : hero == 1 ? 0xC99901EFu : 0;
    if (!opener || !HasSoundDefinition(opener) || HeardThisAttempt(opener)) return;
    const auto line = CaptureLine(opener, 0x871111EC, 0);
    if (!kernel::R32(VoiceHandle) && !CaptionVoiceHandle() && !queue.Size()) PlayLine(ctx, line);
    else Enqueue(line);
    LOGD("Dialogue Shao Kahn opener: sound=%08X", opener);
}

// play_cincamera (thunk 0x16924 -> 0x70830) activates camera slot 1 ([0x599844]+0x1C) for the
// path. Foundry cues: Map 04 hooks/cauldron (cin1/cin2), Map 03 forge door after the press.
constexpr uint32_t CameraManager = 0x599844, CinCameraSlot = 1;
// `during` < 0: speak as the camera ends; otherwise speak that many seconds into the camera.
// A cue with both takes uses the lead monk; a single-take cue plays if that monk is in play.
struct CameraCue { uint32_t room, camera, liu, kung; double during; const char* name; };
constexpr CameraCue CameraCues[] = {
    {67, 1, 0x26AAB15F, 0xEA7D1072, -1, "foundry hooks hint"},
    {67, 2, 0x0AA55E0E, 0x67D43C24, 2.0, "foundry cauldron remark"},
    {66, 1, 0, 0xE7636035, 1.0, "foundry forge door remark"},
};
struct {
    uint32_t world = 0, room = 0; const CameraCue* cue = nullptr; bool armed = false, seen = false; double age = 0;
    bool missing_logged = false;
} hooks_hint;

// Ko-op aware: is this monk (0 Liu, 1 Kung) controlled by either player?
bool MonkInPlay(int16_t monk) {
    if (kernel::G<int16_t>(Players + ActorFlag) == monk) return true;
    const uint32_t game = kernel::R32(0x5ACEBC);
    return game && kernel::R32(game + 0x88) != 0 && kernel::G<int16_t>(Players + 0x3238 + ActorFlag) == monk;
}

bool CinCameraActive() {
    const uint32_t manager = kernel::R32(CameraManager);
    return manager && kernel::R32(manager + 0x1C) == CinCameraSlot;
}

void NoteCinCamera(uint32_t index) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    LOGD("Dialogue cin camera: index=%u room=%u active=%d", index, room, CinCameraActive());
    if (!RestorationEnabled() || !world) return;
    for (const auto& cue : CameraCues)
        if (cue.room == room && cue.camera == index) hooks_hint = {world, room, &cue, true, false, 0};
}

void UpdateHooksHint(bool cinematic, bool paused, double elapsed) {
    if (!hooks_hint.armed) return;
    if (!RestorationEnabled() || kernel::R32(World) != hooks_hint.world ||
        kernel::R32(CurrentRoom) != hooks_hint.room) {
        hooks_hint = {};
        return;
    }
    const auto& cue = *hooks_hint.cue;
    const bool camera = cinematic || CinCameraActive();
    if (camera) hooks_hint.seen = true;
    if (paused || kernel::R32(0x6C92F8)) return;
    if (cue.during < 0) {
        if (camera) return;
    } else if (camera && (hooks_hint.age += std::min(elapsed, 0.1)) < cue.during) {
        return;
    }
    const auto hero = kernel::G<int16_t>(Players + ActorFlag);
    uint32_t hint = hero == 0 ? cue.liu : hero == 1 ? cue.kung : 0;
    if (!cue.liu != !cue.kung) hint = MonkInPlay(cue.liu ? 0 : 1) ? (cue.liu | cue.kung) : 0;
    if (!hint || HeardThisAttempt(hint)) { hooks_hint.armed = false; return; }
    if (!HasSoundDefinition(hint)) {
        if (!hooks_hint.missing_logged) {
            LOGD("Dialogue %s: %08X not loaded", cue.name, hint);
            LogSoundBanks("missing cue");
            hooks_hint.missing_logged = true;
        }
        // A scene can reload its bank: keep trying while the camera runs.
        if (!camera) hooks_hint.armed = false;
        return;
    }
    hooks_hint.armed = false;
    if (Enqueue(CaptureLine(hint, 0x871111EC, 0)))
        LOGD("Dialogue %s: sound=%08X camera_seen=%d", cue.name, hint, hooks_hint.seen);
}

// Foundry Map 02 furnace (gameevent 26 once used): Liu's vo_liukang_found_2_01_2 2 s into
// every visit until then. Game events: byte [[0x5ACEBC] + 0x4874 + id].
constexpr uint32_t FurnaceRoom = 65, FurnaceEvent = 26, FurnaceRemark = 0x2BB4C118;
constexpr double FurnaceDelay = 2.0;
struct { uint32_t world = 0, room = 0; double age = 0; bool done = false; } furnace_hint;

bool GameEventSet(uint32_t id) {
    const uint32_t game = kernel::R32(0x5ACEBC);
    return game && id < 200 && kernel::G<uint8_t>(game + 0x4874 + id);
}

void UpdateFurnaceHint(bool cinematic, bool paused, double elapsed) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (furnace_hint.world != world || furnace_hint.room != room) furnace_hint = {world, room};
    if (!RestorationEnabled() || !world || room != FurnaceRoom || furnace_hint.done) return;
    if (cinematic || CinCameraActive() || paused || kernel::R32(0x6C92F8)) return;
    if (furnace_hint.age == 0)
        LOGD("Dialogue furnace room: furnace_used=%d hero=%d", GameEventSet(FurnaceEvent),
             kernel::G<int16_t>(Players + ActorFlag));
    // A long frame (room streaming) must not eat the delay.
    if ((furnace_hint.age += std::min(elapsed, 0.1)) < FurnaceDelay) return;
    furnace_hint.done = true;
    const bool solved = GameEventSet(FurnaceEvent);
    // Only Liu has a take: in Ko-op he speaks from either slot.
    const bool liu = MonkInPlay(0);
    if (solved || !liu) {
        LOGD("Dialogue furnace remark skipped: furnace_used=%d liu=%d", solved, liu);
        return;
    }
    // Unlike other additions, this hint repeats on every entry until the furnace is used.
    attempt_heard.erase(FurnaceRemark);
    if (!HasSoundDefinition(FurnaceRemark)) {
        LOGD("Dialogue furnace remark: %08X not loaded", FurnaceRemark);
        return;
    }
    if (Enqueue(CaptureLine(FurnaceRemark, 0x871111EC, 0))) LOGD("Dialogue furnace remark queued");
}

void UpdateGateWarning(X86Context& ctx, double elapsed, bool paused) {
    if (!gate_warning.sound) return;
    if (!RestorationEnabled() || kernel::R32(World) != gate_warning.world || kernel::R32(CurrentRoom) != 96) {
        gate_warning = {};
        return;
    }
    if (paused || (gate_warning.age += elapsed) < GateWarningDelay) return;
    if (HeardThisAttempt(gate_warning.sound)) { gate_warning = {}; return; }
    const auto line = CaptureLine(gate_warning.sound, 0x871111EC, 0);
    const bool busy = kernel::R32(VoiceHandle) || CaptionVoiceHandle() || queue.Size() ||
        kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx);
    if (!busy) PlayLine(ctx, line);
    else if (!Enqueue(line)) return;
    LOGD("Dialogue gate warning: sound=%08X %s", gate_warning.sound, busy ? "queued" : "played");
    gate_warning = {};
}

void UpdateDialogue(X86Context& ctx) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = last_update.time_since_epoch().count()
        ? std::chrono::duration<double>(now - last_update).count() : 0;
    last_update = now;
    SyncAttempt();
    if (attempt_reset) {
        // Death/retry: the map's additions get a fresh start, as on a first visit.
        attempt_reset = false;
        ResetDialogueState();
        LOGD("Dialogue attempt reset by checkpoint restore");
    }
    if (kernel::G<uint8_t>(MessageManager + 0x6C0) && !kernel::R32(MessageManager + 0x6C8) && CaptionHasVoice())
        caption_pending_age += elapsed;
    else
        caption_pending_age = 0;
    if (const uint32_t voice = kernel::R32(VoiceHandle); voice != voice_watch.handle) {
        if (voice_watch.handle)
            LOGD("Dialogue voice released: sound=%08X handle=%08X after %.2fs", voice_watch.sound,
                 voice_watch.handle, voice_watch.age);
        voice_watch = {voice, script_voice.handle == voice ? script_voice.sound : 0, 0};
    } else if (voice) {
        voice_watch.age += elapsed;
    }
    const bool cinematic = kernel::R32(World) &&
        (kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx));
    const bool scene_ended = cinematic_active && !cinematic;
    const bool paused = openshao::ed_nexus::GameplayPaused();
    // A skip can start and finish between updates; the play hook keeps the edge.
    if (cinematic || cinematic_active) queue.EnterCinematic();
    cinematic_active = cinematic;
    if (scene_ended) { quiet = 0; gap = SceneGap; }
    queue.DropOtherRooms(kernel::R32(CurrentRoom), kernel::R32(World));
    queue.DropOtherScenes(kernel::R32(SceneName));
    const double pending_elapsed = paused || cinematic || scene_ended ? 0 : elapsed;
    if (cage.pending) {
        cage.waited += pending_elapsed;
        if (cage.waited > openshao::dialogue::Queue::MaxBlocked) cage.pending = false;
    }
    UpdateForestHintResponse();
    UpdateGateWarning(ctx, elapsed, paused);
    UpdateShaoOpener(ctx, cinematic, paused);
    UpdateHooksHint(cinematic, paused, elapsed);
    UpdateFurnaceHint(cinematic, paused, elapsed);
    UpdateSaveShrineHint();
    NoteEnemies();
    UpdateOrochiFeast();
    UpdateErmacIntro(elapsed, paused);
    UpdateSupplementalVoices(ctx, pending_elapsed);
    UpdateErmacPhaseVoices(ctx, pending_elapsed, cinematic);
    silent = false;
    if (!queue.Size() && !cage.pending) return;
    if (!RestorationEnabled() || !kernel::R32(World)) { queue.Clear(); return; }
    if (cinematic) queue.EnterCinematic();
    // Only the native loading flag suspends this update.
    const bool loading = kernel::R32(0x6C92F8) != 0;
    const bool speech = kernel::R32(VoiceHandle) || CaptionVoiceHandle();
    // vosound owns its timing; optional additions still yield to scene audio.
    const bool speaking = speech || (queue.WaitsForStreams() && OriginalAudioStreamBusy(ctx, false));
    const bool blocked = paused || cinematic || loading || speaking;
    const uint32_t voice = kernel::R32(VoiceHandle) ? kernel::R32(VoiceHandle) : CaptionVoiceHandle();
    if (const auto expired = queue.Age(scene_ended ? 0 : elapsed, paused, cinematic || loading || speaking,
                                       cinematic, speech, voice))
        LOGD("Dialogue expired: %zu line(s) waited too long", expired);
    if (blocked) {
        quiet = 0;
        if (cinematic || loading) gap = SceneGap;
        return;
    }
    if (!scene_ended) quiet += elapsed;
    if (quiet < gap) return;
    silent = true;
    if (!queue.Size()) return;
    gap = VoiceGap;
    silent = false;
    const auto line = queue.Pop();
    for (size_t i = 0; i < ermac_phase_voices.size(); ++i)
        if (line->room == 108 && line->sound == ErmacPhaseSounds[i] && !ermac_phase_voices[i].requested)
            return;
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
    const uint32_t source = kernel::R32(ctx.esp + 12);
    const bool moon = RestorationEnabled() && kernel::R32(CurrentRoom) == 98 &&
        kernel::R32(args) == 0 && kernel::R32(args + 4) == 0x871111EC && kernel::R32(args + 8) == 1;
    if (moon) {
        // _d_kl_moon2b lacks the once flag of Liu's counterpart and would requeue every tick.
        const uint32_t trigger = kernel::R32(ctx.esp + 12);
        if (trigger) kernel::W32(trigger + 0x70, kernel::R32(trigger + 0x70) | 2);
    }
    const bool kitana_laugh = RestorationEnabled() && kernel::R32(CurrentRoom) == 194 &&
        kernel::R32(args) == 0xDF95FA70 && kernel::R32(args + 4) == 0x871111EC
        && kernel::R32(args + 8) == 0xFFFFFFFF;
    if (kitana_laugh && !kitana_seeded) {
        // Only two laugh events here: vary the first so SHORT stays reachable.
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
    // The pillar cues only requested LONG_2: use the stage's loaded laugh groups.
    uint32_t sound = moon ? 0xEFD60587 : laugh_bank >= 0 ? laugh : kernel::R32(args);
    uint32_t bank_name = laugh_bank >= 0
        ? kernel::R32(kernel::R32(SoundManager + 0x14) + uint32_t(laugh_bank) * 24) : kernel::R32(args + 4);
    if (RestorationEnabled() && player < ActorCount && kernel::R32(args + 4) == 0xAECADCD8 &&
        kernel::R32(args + 8) == 0xFFFFFFFF) {
        const uint32_t room = kernel::R32(CurrentRoom), actor_at = Players + player * PlayerStride;
        const auto character = kernel::G<int16_t>(actor_at + ActorFlag);
        // Forest Map 9 hints reference absent IDs; its recordings describe this acid puzzle.
        if (room == 136) {
            if (sound == 0x93FDBC98 && character == 0) { sound = 0x526CF529; bank_name = 0x871111EC; }
            if (sound == 0x8B431000 && character == 1) { sound = 0xDF681048; bank_name = 0x871111EC; }
            if (acid_hint.world != kernel::R32(World) || acid_hint.room != room)
                acid_hint = {kernel::R32(World), room, 0};
            if ((sound == 0x526CF529 || sound == 0xDF681048) && (acid_hint.played & (1u << character)))
                sound = 0;
            // Explicit banks use the variation-only API (0x151610); these need search-all.
        }
        // Wheel cue during hit reactions (state 12, 0x15DF30): use the unused "progress lost" group.
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
    if (RestorationEnabled() && ObsoleteWuShiVoice(sound)) {
        LOGD("Dialogue discarded: sound=%08X source beat has ended", sound);
        sound = 0;
    }
    ScopedGuestWord repaired_sound(args, sound);
    ScopedGuestWord repaired_bank(args + 4, bank_name);
    // Native speech keeps its timing: interrupt an addition of ours instead of deferring it.
    if (RestorationEnabled() && sound && optional_voice && kernel::R32(VoiceHandle) == optional_voice) {
        if (IsNativeSoundPlaying(ctx, optional_voice)) {
            const auto saved = ctx;
            ctx.ecx = 0x653C64;
            kernel::CallGuest(ctx, 0x152380, {optional_voice});
            ctx = saved;
            LOGD("Dialogue addition %08X interrupted by native sound=%08X", optional_voice, sound);
        }
        kernel::W32(VoiceHandle, 0);
        optional_voice = 0;
    }
    const uint32_t original_voice = kernel::R32(VoiceHandle);
    // Caption contention takes the native busy exit; restore the script slot afterwards.
    // The script's own line for the caption being shown is that caption's voice: let it start now.
    const bool caption_line = kernel::R32(MessageManager + 0x6C4) == sound && !kernel::R32(MessageManager + 0x6C8);
    const uint32_t caption_voice = RestorationEnabled() && !original_voice
        ? (CaptionVoiceHandle() && !caption_line ? CaptionVoiceHandle() : queue.HasRequired() ? 0xFFFFFFFFu : 0) : 0;
    const uint32_t busy = caption_voice ? caption_voice : original_voice;
    if (caption_voice) {
        ScopedGuestWord marked_busy(VoiceHandle, caption_voice);
        __imp__sub_001A18F0(ctx, base);
    } else {
        __imp__sub_001A18F0(ctx, base);
    }
    if (!RestorationEnabled() || player >= ActorCount || IsCinematicPlaying(ctx) || !kernel::R32(args)) return;
    bool accepted = !caption_voice && kernel::R32(VoiceHandle) && kernel::R32(VoiceHandle) != busy;
    if (accepted) {
        TrackScriptVoice(kernel::R32(args), kernel::R32(VoiceHandle));
        LOGD("Dialogue voice: sound=%08X handle=%08X room=%u", kernel::R32(args), kernel::R32(VoiceHandle),
             kernel::R32(CurrentRoom));
    }
    const auto flag = kernel::G<uint16_t>(Players + player * PlayerStride + ActorFlag);
    if (busy && !accepted && openshao::dialogue::ActorAccepts(kernel::R32(args + 8), flag)) {
        auto line = CaptureLine(kernel::R32(args), kernel::R32(args + 4), player, source);
        line.timed = true;
        line.behind = busy;
        accepted = Enqueue(line);
        if (accepted) LOGD("Dialogue deferred: sound=%08X busy=%08X caption=%u/%08X/%08X", line.sound, busy,
                           kernel::G<uint8_t>(MessageManager + 0x6C0), kernel::R32(MessageManager + 0x6C4),
                           kernel::R32(MessageManager + 0x6C8));
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
    // Phase-3 speech allows one alternate 08 take in solo; earlier 07 taunts stay native.
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
    // Arm Scorpion's follow-up only after the original 04 request is accepted.
    if (kernel::R32(CurrentRoom) == 160 && original_request == 0xB9E947A4 &&
        player < ActorCount && flag == 52) {
        RefreshSupplemental(scorpion_combat, kernel::R32(World), 160);
        ObserveSupplemental(scorpion_combat);
    }
    if (context.selection) {
        ++context.selection->cursor.next;
        if (context.once) context.selection->seen |= 1u << (context.take - 1);
    }
    if (laugh_bank >= 0) ++kitana_variation;
    if (kernel::R32(CurrentRoom) == 73 && flag == 62 && kernel::R32(args) == 0x56E516CA &&
        kernel::R32(args + 4) == 0xAECADCD8 && kernel::R32(args + 8) == 0xFFFFFFFF) {
        // vo_shang plays Shao Kahn's group; add one unused Shang taunt as a response.
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
void ResetDialogueState() {
    if (!kernel::R32(World)) attempt_heard.clear();
    queue.Clear();
    last_update = {};
    cinematic_active = false;
    cage = {};
    attack_tutorial = {};
    script_voice = {};
    voice_serial = 0;
    wu_shi_context = {};
    recovered_cues.fill(0);
    cue_room = cue_world = 0;
    kitana_trap = 0;
    acid_hint = {};
    gate_warning_world = 0;
    gate_warning = {};
    ermac_intro = {};
    shao_opener = {};
    hooks_hint = {};
    furnace_hint = {};
    save_shrine_hint = {};
    warlord_watch = {};
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
    ermac_phase_voices = {};
    ermac_world = ermac_room = 0;
    phase_world = phase_room = 0;
    followup_world = followup_room = followup_seen = 0;
    forest_hint_world = forest_hint_room = 0;
    forest_hint_responded = false;
    monastery_intro = forest_entrance = forest_second_tree = shrine_offering = wu_shi_fire = mileena_trio_opening = scorpion_combat = monastery_fist = foundry_bridge = forest_snakes = wu_shi_combo = {};
    foundry_bridge_ticks = 0;
    forest_snakes_triggered = false;
    forest_snakes_stream_handle = 0;
    forest_snakes_cutscene_seen = false;
    mileena_monastery_combat = {};
    native_world = native_room = 0;
    spear_choice = fire_choice = shao_choice = {};
    shao_challenge = false;
}

X86_FUNC(sub_001A1850) {
    ResetDialogueState();
    __imp__sub_001A1850(ctx, base);
}

void NoteCatapultSound(uint32_t sound) {
    if (!RestorationEnabled() || !kernel::R32(World) || native_room != CatapultRoom) return;
    if (sound == CatapultEngage && catapult_launches < 2 && ++catapult_launches < 2) return;
    const uint8_t bit = sound == CatapultEngage ? 1 : sound == BarricadeBreak && (catapult_remarks & 1) ? 2 : 0;
    if (!bit || (catapult_remarks & bit)) return;
    catapult_remarks |= bit;
    const auto hero = kernel::G<int16_t>(Players + ActorFlag);
    // vo_liukang_wu_shi_8_09 / vo_kunglao_wu_shi_8_06 (catapults), 8_03 / 8_07 (barricades)
    const uint32_t line_sound = bit == 1 ? (hero == 0 ? 0xEB5B5725u : hero == 1 ? 0xB2DD7840u : 0)
                                         : (hero == 0 ? 0xA1EB4F84u : hero == 1 ? 0x8FBD51F0u : 0);
    if (!line_sound || !HasSoundDefinition(line_sound)) return;
    auto line = CaptureLine(line_sound, 0x871111EC, 0);
    line.after_cinematic = true;  // the catapult and barricade shots use camera cuts
    if (Enqueue(line)) LOGD("Dialogue catapult remark: sound=%08X", line_sound);
}

// Keep the original spatial playback, repetition gates and handle ownership.
X86_EXTERN_FUNC(__imp__sub_00151560);
X86_FUNC(sub_00151560) {
    RefreshNativeEncounter();
    const auto argument = ctx.esp + 4, original = kernel::R32(argument);
    NoteCatapultSound(original);
    const bool baraka_kung = original == 0x6935F9CD && IsBarakaSoloKungLao() &&
        HasSoundDefinition(0x530DACE6);
    const bool eligible = RestorationEnabled() && kernel::R32(World) && native_room == 160 &&
        original == 0x12F19BF6 && HasSoundDefinition(0x24E7FCEE);
    const bool alternate = eligible && ChoiceIndex(ctx, spear_choice, 2) != 0;
    ScopedGuestWord selected(argument, baraka_kung ? 0x530DACE6u :
                                   alternate ? 0x24E7FCEEu : original);
    __imp__sub_00151560(ctx, base);
    openshao::survival::VoicePlayed(original,ctx.eax);
    if (eligible && ctx.eax) ++spear_choice.next;
}

X86_EXTERN_FUNC(__imp__sub_00151610);
X86_FUNC(sub_00151610) {
    RefreshNativeEncounter();
    const auto bank = kernel::R32(ctx.esp + 4), sound = kernel::R32(ctx.esp + 8);
    NoteCatapultSound(sound);
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
    // A direct banked sound ID is not a random group: replace the argument itself.
    std::optional<ScopedGuestWord> direct;
    if (baraka_kung) direct.emplace(ctx.esp + 8, replacement);
    std::optional<ScopedGuestWord> count, current, first;
    if (group) {
        count.emplace(group + 4, 1);
        current.emplace(group + 8, 0);
        first.emplace(group + 12, replacement);
    }
    __imp__sub_00151610(ctx, base);
    openshao::survival::VoicePlayed(sound,ctx.eax);
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
            mileena_monastery_combat.scene = kernel::R32(SceneName);
            mileena_monastery_combat.waited = 0;
        }
    }
    if ((character == 66 || character == 67) && animation == 1723) {
        const uint32_t group = character == 66 ? 0xE35C1CF7 : 0xF0252683;
        // Owner removed Raiden's em_4_01 capture / em_4_05 freeing warnings from the story.
        if (FindGroupBank(group) >= 0) Enqueue(CaptureLine(group, 0xAECADCD8, slot));
    }
}

// Keep one take per room so a variation cannot restart the caption voice.
X86_EXTERN_FUNC(__imp__sub_000111EF);
X86_FUNC(sub_000111EF) {
    const uint32_t caller = kernel::R32(ctx.esp), text = kernel::R32(ctx.esp + 4);
    // The voice array (0xD2CE0) has no bounds check; host text IDs have no voice.
    if(text>=openshao::survival::LabelBase){kernel::Return(ctx,0,0);return;}
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

// Observe _d_snakes' audiostreamplay even if audio is rejected.
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

// Scene audio audit hook, set by the audio runtime (absent in tests); see audio_audit.inl.
void (*openshao_audio_audit)(bool begin);
static std::chrono::steady_clock::time_point audit_until{};
static void LogAudioCategories() {
    static bool logged = false;
    if (logged) return;
    const uint32_t groups = kernel::R32(0x653C50), group_table = kernel::R32(0x653BDC);
    const uint32_t categories = kernel::R32(0x653C54), category_table = kernel::R32(0x653C14);
    if (!groups || !group_table || !categories || !category_table) return;
    logged = true;
    for (uint32_t i = 0, n = std::min<uint32_t>(kernel::R32(groups), 32); i < n; ++i)
        LOGI("audio audit: group %08X volume %.3f", kernel::R32(group_table + i * 8),
             std::bit_cast<float>(kernel::R32(group_table + i * 8 + 4)));
    for (uint32_t i = 0, n = std::min<uint32_t>(kernel::R32(categories), 256); i < n; ++i)
        LOGI("audio audit: category %08X group %08X volume %.3f", kernel::R32(category_table + i * 12),
             kernel::R32(category_table + i * 12 + 4), std::bit_cast<float>(kernel::R32(category_table + i * 12 + 8)));
}
static void AuditSceneAudio(bool begin) {
    if (!openshao_audio_audit) return;
    const auto now = std::chrono::steady_clock::now();
    if (begin) {
        audit_until = now + std::chrono::seconds(40);
        LogAudioCategories();
    } else if (now > audit_until) return;
    openshao_audio_audit(begin);
}

// Capture actual audiostreamplay handles; trigger activation does not prove playback.
X86_EXTERN_FUNC(__imp__sub_0017C0B0);
X86_FUNC(sub_0017C0B0) {
    const uint32_t sound = kernel::R32(ctx.esp + 4);
    {
        bool found = false;
        for (uint32_t i = 0; i < 5; ++i)
            if (kernel::R32(0x69654C + i * 12) == sound && kernel::R32(0x69654C + i * 12 + 4)) found = true;
        LOGI("audio stream: play %08X caller %08X%s", sound, kernel::R32(ctx.esp), found ? "" : " (not buffered)");
    }
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
    AuditSceneAudio(false);
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

// Keep the authored animation; swap its hardcoded take for the localized bank7 ones.
X86_EXTERN_FUNC(__imp__sub_0017C040);
X86_FUNC(sub_0017C040) {
    const uint32_t world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (stream_world != world || stream_room != room) {
        stream_variations = {};
        stream_world = world;
        stream_room = room;
    }
    const uint32_t argument = ctx.esp + 4, original = kernel::R32(argument);
    const uint32_t caller = kernel::R32(ctx.esp);
    uint32_t sound = original;
    // Frame 1300 requests Liu in solo Kung Lao play: buffer Kung's take under Liu's tag.
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
                // These takes fit the same failed-fatality recovery phase (their own cue is unbound).
                constexpr uint32_t takes[]{0x8D15F4C2,0xCAB58E12,0xFB5D948F,0xC63DBD3F};
                choice = takes[ChoiceIndex(ctx, selected_cursor, 4)];
                ++selected_cursor.next;
            } else choice = ChooseGroupSound(ctx, family.group, selected_cursor);
            // This take addresses both monks: keep its neutral source outside Ko-op.
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
    // Retail fix (always on): restore the PS2 category and voice type of the fatality scene sounds.
    if (std::find(FatalitySceneSounds.begin(), FatalitySceneSounds.end(), sound) != FatalitySceneSounds.end())
        if (const uint32_t definition = FindSoundDefinition(sound);
            definition && kernel::R32(definition + 0x10) == XboxFatalityCategory) {
            kernel::W32(definition + 0x10, Ps2FatalityCategory);
            kernel::G<uint8_t>(definition + 0x16) = 2;
            LOGI("fatality scene sound %08X: PS2 category and voice type restored", sound);
        }
    ScopedGuestWord selected(argument, sound);
    __imp__sub_0017C040(ctx, base);
    uint32_t buffered = 0;
    for (size_t i = 0; i < before.size(); ++i)
        if (kernel::R32(0x69654C + uint32_t(i) * 12) == sound) buffered = kernel::R32(0x69654C + uint32_t(i) * 12 + 4);
    // Diagnostic for silent scene audio: what the stream defines and whether it was buffered.
    if (const uint32_t definition = FindSoundDefinition(sound)) {
        const uint32_t category = kernel::R32(definition + 0x10);
        LOGI("audio stream: buffer %08X caller %08X handle %08X ref %08X vol %.2f category %08X (%.2f) %s", sound,
             caller, buffered, kernel::R32(definition + 4), std::bit_cast<float>(kernel::R32(definition + 8)),
             category, CategoryVolume(category), kernel::G<uint8_t>(definition + 0x15) ? "disc" : "memory");
    } else LOGI("audio stream: buffer %08X caller %08X handle %08X without definition", sound, caller, buffered);
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

// audiostreamdelete, logged with the other stream events.
X86_EXTERN_FUNC(__imp__sub_0017C110);
X86_FUNC(sub_0017C110) {
    LOGI("audio stream: delete %08X caller %08X", kernel::R32(ctx.esp + 4), kernel::R32(ctx.esp));
    __imp__sub_0017C110(ctx, base);
}

// Music diagnostics: start 0xEAA90 (1698D), stop 0xEA8F0 (1353F), enable 0xEA7A0 (12D60), fades, stops on [0x62DC08].
constexpr uint32_t MusicHandle = 0x62DC08;
// A track that is still loading is started again every update; log each change only once.
bool NewMusicEvent(const char* text, uint32_t value) {
    static std::array<std::pair<const char*, uint32_t>, 8> last{};
    for (auto& [label, previous] : last) {
        if (label == text) {
            if (previous == value) return false;
            previous = value;
            return true;
        }
        if (!label) { label = text; previous = value; return true; }
    }
    return true;
}
#define OPENSHAO_MUSIC_LOG(name, text) \
X86_EXTERN_FUNC(__imp__##name); \
X86_FUNC(name) { \
    static const char* const label = text; \
    if (NewMusicEvent(label, kernel::R32(ctx.esp + 4))) \
        LOGI("music player: %s %08X caller %08X handle %08X", label, kernel::R32(ctx.esp + 4), kernel::R32(ctx.esp), \
             kernel::R32(MusicHandle)); \
    __imp__##name(ctx, base); \
}
OPENSHAO_MUSIC_LOG(sub_0001353F, "stop")
OPENSHAO_MUSIC_LOG(sub_00012B85, "fade-out pool")
OPENSHAO_MUSIC_LOG(sub_000125A4, "fade-in pool")
#undef OPENSHAO_MUSIC_LOG
X86_EXTERN_FUNC(__imp__sub_0001698D);
X86_FUNC(sub_0001698D) {
    const auto track=kernel::R32(ctx.esp+4),caller=kernel::R32(ctx.esp);
    if(openshao::survival::DeferMusic(track)) {
        if(kernel::R32(MusicHandle)) {
            // Stop the prior handle first, like retail music play.
            const auto saved=ctx;
            kernel::CallGuest(ctx,0x1353F,{});
            ctx=saved;
            LOGI("Survival previous music stopped for rules screen");
        }
        if(NewMusicEvent("Survival deferred music",track))
            LOGI("Survival music deferred until rules fade: %08X",track);
        kernel::Return(ctx,ctx.eax,0); // Music play is cdecl; the caller removes the argument.
        return;
    }
    if(NewMusicEvent("play",track))
        LOGI("music player: play %08X caller %08X handle %08X",track,caller,kernel::R32(MusicHandle));
    __imp__sub_0001698D(ctx,base);
}
// Retail fix: fatality scenes turned the music off via musicenable (0x182C0D); keep it on during their stinger.
constexpr uint32_t ScriptMusicEnable = 0x182C0D;
bool FatalitySceneSoundActive() {
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t sound = kernel::R32(0x69654C + i * 12);
        if (!sound) continue;
        for (uint32_t entry = FatalitySceneList; entry < FatalitySceneList + 64 * 4; entry += 4) {
            const uint32_t listed = kernel::R32(entry);
            if (!listed) break;
            if (listed == sound) return true;
        }
    }
    return false;
}
X86_EXTERN_FUNC(__imp__sub_00012D60);
X86_FUNC(sub_00012D60) {
    const uint32_t enable = kernel::R32(ctx.esp + 4), caller = kernel::R32(ctx.esp);
    LOGI("music player: enable %08X caller %08X handle %08X", enable, caller, kernel::R32(MusicHandle));
    if (!enable && caller == ScriptMusicEnable && FatalitySceneSoundActive()) {
        LOGI("fatality scene: music kept on");
        kernel::Return(ctx, ctx.eax, 0);  // cdecl: the caller removes the argument
        return;
    }
    __imp__sub_00012D60(ctx, base);
}
X86_EXTERN_FUNC(__imp__sub_00152380);
X86_FUNC(sub_00152380) {
    const uint32_t handle = kernel::R32(ctx.esp + 4);
    if (handle && handle == kernel::R32(VoiceHandle))
        LOGD("Dialogue stop: handle=%08X caller=%08X room=%u text=%u template=%u", handle,
             kernel::R32(ctx.esp), kernel::R32(CurrentRoom), kernel::R32(MessageManager + 0x6D8),
             kernel::R32(MessageManager + 0x6D4));
    if (handle && handle == kernel::R32(MusicHandle))
        LOGI("music player: voice stop %08X caller %08X", handle, kernel::R32(ctx.esp));
    __imp__sub_00152380(ctx, base);
}

// Retail fix: cinematic fatalities asked for silence; play the fatality music like the others.
X86_EXTERN_FUNC(__imp__sub_00016C9E);
X86_FUNC(sub_00016C9E) {
    const uint32_t slot = ctx.esp + 4;
    const uint32_t caller = kernel::R32(ctx.esp);
    LOGI("music request %08X caller %08X", kernel::R32(slot), caller);
    const auto call = std::find_if(FatalityMusicCalls.begin(), FatalityMusicCalls.end(),
                                   [&](const FatalityMusicCall& c) { return c.caller == caller; });
    if (call != FatalityMusicCalls.end() && kernel::R32(slot) == 0xFFFFFFFF) {
        if (FatalitySceneSoundActive()) {
            ScopedGuestWord music(slot, call->music);
            LOGI("fatality scene: music %08X restored (caller %08X)", call->music, caller);
            __imp__sub_00016C9E(ctx, base);
            return;
        }
    }
    __imp__sub_00016C9E(ctx, base);
}

// Regular voice update: releases the handle once the native sound stops playing.
X86_EXTERN_FUNC(__imp__sub_001A1860);
X86_FUNC(sub_001A1860) {
    const uint32_t caller = kernel::R32(ctx.esp);
    const uint32_t type = kernel::R32(MessageManager + 0x6D4), text = kernel::R32(MessageManager + 0x6D8);
    const uint32_t mapping = kernel::R32(0x5B04A8), room = kernel::R32(CurrentRoom);
    // Feedback has no speech: keep its presentation, skip only the script stop.
    bool voiced = !mapping || text >= 4096 || kernel::R32(mapping + text * 4) != 0;
    for (const auto& cue : CaptionFamilies)
        if (cue.room == room && cue.text == text && !cue.original && HasSoundDefinition(cue.recovered)) voiced = true;
    if ((room == 30 && text == 58) || (room == 192 && text == 237)) voiced = true;
    if (RestorationEnabled() && kernel::R32(World) && kernel::R32(VoiceHandle) &&
        kernel::G<uint8_t>(MessageManager + 0x6C0) && (type == 8 || type == 9) && !voiced &&
        (caller == 0x85EE1 || caller == 0x864B7)) {
        LOGD("Dialogue retained: unvoiced feedback text=%u template=%u", text, type);
        ctx.eax = kernel::R32(VoiceHandle);
        ctx.esp += 4;
        return;
    }
    __imp__sub_001A1860(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_001A1880);
X86_FUNC(sub_001A1880) {
    __imp__sub_001A1880(ctx, base);
    openshao::survival::UpdateVoice(ctx);
    AuditSceneAudio(false);
    // A panel already due takes precedence over starting another queued line.
    UpdateAttackTutorial(ctx);
    // The queue decides first whether the scene is quiet; the Cage caption then uses that verdict.
    UpdateDialogue(ctx);
    UpdateCageCaption(ctx);
}

// Only the first attack panel follows its recorded introduction.
X86_EXTERN_FUNC(__imp__sub_000135A3);
X86_FUNC(sub_000135A3) {
    const uint32_t info = kernel::R32(ctx.esp + 4);
    if(openshao::survival::FilterLevelMessage(info)){kernel::Return(ctx,0,2);return;}
    const uint32_t type = kernel::R32(info + 0x14), text = kernel::R32(info + 0x18);
    if (attack_tutorial.pending && (type == 6 || type == 8 || type == 9) && (type != 6 || text != 24))
        attack_tutorial = {};
    if (RestorationEnabled() && ctx.ecx == MessageManager && kernel::R32(World) &&
        kernel::R32(CurrentRoom) == 96 && kernel::R32(SceneName) == 0x18797C6C &&
        (kernel::R32(ctx.esp + 8) & 0xFF) == 1 && type == 6 && text == 24) {
        queue.Clear();
        if ((script_voice.sound == 0x65138D8A || script_voice.sound == 0x7625BA4E) &&
            script_voice.handle == kernel::R32(VoiceHandle) && IsNativeSoundPlaying(ctx, script_voice.handle)) {
            if (!attack_tutorial.pending) {
                attack_tutorial = {kernel::R32(World), 96, kernel::R32(SceneName), script_voice, true};
                for (uint32_t i = 0; i < attack_tutorial.message.size(); ++i)
                    attack_tutorial.message[i] = kernel::R32(info + i * 4);
                LOGD("Dialogue tutorial: waiting only for sound=%08X", script_voice.sound);
            }
            ctx.esp += 12;
            return;
        }
    }
    __imp__sub_000135A3(ctx, base);
}

// Native animstreamplay (0x717D0), reached through its original thunk.
X86_EXTERN_FUNC(__imp__sub_00014985);
X86_FUNC(sub_00014985) {
    const uint32_t scene = kernel::R32(SceneName);
    queue.Clear();
    attack_tutorial = {};
    script_voice = {};
    // A new scene closes armed optional replies, even when it replays the same name.
    for (auto* cue : SupplementalCues)
        if (cue->scene_bound) cue->canceled = true;
    mileena_monastery_combat.pending = 0;
    for (auto& phase : ermac_phase_voices) {
        if (!phase.requested || phase.delivered) continue;
        if (phase.scene_observed) {
            phase.requested = false;
            phase.canceled = true;
        }
        else {
            phase.scene_observed = true;
            phase.scene = scene;
        }
    }
    // Names every in-engine scene in the log, to match scene audio reports with their streams.
    LOGI("scene: play %08X room %u", scene, kernel::R32(CurrentRoom));
    __imp__sub_00014985(ctx, base);
    cinematic_active = kernel::G<uint8_t>(SceneLoadingOrPlaying) || IsCinematicPlaying(ctx);
    NoteCampaignScene(scene);
    if (RestorationEnabled() && scene == ShaoIntroScene && kernel::R32(CurrentRoom) == 73)
        shao_opener = {kernel::R32(World), true, false};
    AuditSceneAudio(true);
    cage.pending = false;
    if (RestorationEnabled() && !cage.delivered && scene == CageScene &&
        kernel::R32(CurrentRoom) == CageRoom && kernel::G<uint8_t>(SceneLoadingOrPlaying)) {
        cage.room = CageRoom;
        cage.world = kernel::R32(World);
        cage.pending = true;
        cage.waited = 0;
    }
}

// play_cincamera execute (thunk to 0x70830): args[0] is the room's camera path index.
X86_EXTERN_FUNC(__imp__sub_00016924);
X86_FUNC(sub_00016924) {
    const uint32_t event = kernel::R32(ctx.esp + 4);
    const uint32_t index = kernel::R32(kernel::R32(event + 8));
    __imp__sub_00016924(ctx, base);
    NoteCinCamera(index);
}

X86_EXTERN_FUNC(__imp__sub_00170330);
X86_FUNC(sub_00170330) {
    const uint32_t event = kernel::R32(ctx.esp + 4), actor = kernel::R32(ctx.esp + 8);
    const uint32_t args = kernel::R32(event + 8), player = kernel::R32(actor + 0x10);
    const uint32_t enable = kernel::R32(args), target = kernel::R32(args + 4), index = kernel::R32(args + 8);
    __imp__sub_00170330(ctx, base);
    openshao::survival::TriggerActivated(ctx, target, enable == 1);
    if (kernel::R32(CurrentRoom) == 96)  // Wu Shi Map 01 trigger trace for dialogue timing reports.
        LOGD("Dialogue trigger: target=%08X index=%u enable=%u", target, index, enable);
    RefreshWuShiContext();
    if (RestorationEnabled()) {
        // A canceled native trigger no longer owns a pending speech request.
        constexpr uint32_t triggers = 0x6820A0;
        if (enable == 0 && index < kernel::R32(triggers + 0xA8C0) && index < 2000)
            queue.DropSource(kernel::R32(triggers + index * 0xD8 + 0xC));
        if (kernel::R32(CurrentRoom) == 96 && enable == 1 &&
            ((target == 0xBDF916B3 && index == 157) || (target == 0x7D27A93F && index == 161))) {
            wu_shi_context.gate_finished = true;
            for (const auto sound : {0x2AD7B0C5u, 0x59BC5679u, 0x826AEECDu}) queue.DropSound(sound);
        }
    }
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
        mileena_trio_opening.canceled = true;
        mileena_monastery_combat.pending = 0;
        mileena_monastery_combat.canceled = true;
    }
    if (kernel::R32(CurrentRoom) == 160 && enable == 1 &&
        target == 2390456960u && index == 110) { // Original _d_death52
        RefreshSupplemental(scorpion_combat, kernel::R32(World), 160);
        scorpion_combat.canceled = true;
    }
    // Water levers expire the opening hint before any victory stream can begin.
    if (RestorationEnabled() && kernel::R32(CurrentRoom) == 106 && enable == 0) {
        switch (target) {
        case 1704478846u: case 1320491965u: case 1471032060u:
        case 418364475u: case 805076000u: case 3069521306u:
        case 3253869836u: case 1364000925u:
            RefreshSupplemental(wu_shi_fire, kernel::R32(World), 106);
            wu_shi_fire.canceled = true;
            break;
        default: break;
        }
    }
    if (!RestorationEnabled() || enable != 1) return;
    const uint32_t room = kernel::R32(CurrentRoom), world = kernel::R32(World);
    if (cue_room != room || cue_world != world) {
        recovered_cues.fill(0);
        gate_warning_world = 0;
        gate_warning = {};
        kitana_trap = 0;
                cue_room = room;
        cue_world = world;
    }
    // The monk's escape warning plays with the first gate opening (cleared-arena jingle).
    if (room == 96 && gate_warning_world != world &&
        std::find(GateOpenings.begin(), GateOpenings.end(), std::pair{target, index}) != GateOpenings.end()) {
        const uint32_t sound = EscapeWarningSound();
        if (sound && HasSoundDefinition(sound)) {
            gate_warning = {world, sound, 0};
            gate_warning_world = world;
        }
    }
    // _d_rai_combo (B001C237) once carried Raiden's combo tip; the monk's answer now takes its place.
    if (room == 96 && target == 0xB001C237) {
        RefreshSupplemental(wu_shi_combo, world, room);
        if (!wu_shi_combo.Done()) {
            BindSupplemental(wu_shi_combo);
            wu_shi_combo.observed = wu_shi_combo.finished = true;
        }
    }
    if (room == 101 && target == 0x94FE469F && index == 0x3A) ArmWarlordWatch();
    if (room == 108 && target == ErmacIntroTarget && index == ErmacIntroOrdinal &&
        !(ermac_intro.done && ermac_intro.world == world)) {
        ermac_intro = {world, 0, true, true};
    }
    if (room == 108) {
        if (ermac_room != room || ermac_world != world) {
            ermac_phase_voices = {};
            ermac_world = world;
            ermac_room = room;
        }
        // bossentersphase uses the script controller, so handle it before actor validation.
        if (target == 0x5A3297F2u && index == 13) {
            if (!ermac_phase_voices[0].delivered && !ermac_phase_voices[0].canceled) {
                ermac_phase_voices[0].requested = true;
                ermac_phase_voices[0].ready = true;
            }
        } else if (target == 0x7873AA87u && index == 0xFFFFFFFFu) {
            ermac_phase_voices[0].requested = false;
            if (!ermac_phase_voices[1].delivered && !ermac_phase_voices[1].canceled) {
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
    // _d_snd_trap (200 ticks after _d_trapwall): Kitana's threat belongs to the captive phase.
    if (room == 195 && target == 0xD11A17FD && index == 69 && character == 65 && !(kitana_trap & actor_bit)) {
        auto line = CaptureLine(0x9B1AFD52, 0x871111EC, player);
        line.after_cinematic = true;
        if (Enqueue(line)) kitana_trap |= actor_bit;
    }
    if (index != 0xFFFFFFFF) return;
    for (size_t i = 0; i < MissingCues.size(); ++i) {
        const auto& cue = MissingCues[i];
        if (cue.room != room || cue.target != target || (recovered_cues[i] & actor_bit) ||
            (cue.character >= 0 && cue.character != character)) continue;
        // _d_war_evnts can rerun after the warlord is gone: "what is that?" needs him there.
        if ((cue.target == 0xCB402320 || cue.target == 0x6465CF88) && !WarlordPresent()) {
            LOGD("Dialogue warlord remark skipped: the warlord did not appear");
            continue;
        }
        auto line = CaptureLine(cue.sound, 0x871111EC, player);
        line.after_cinematic = true;
        if (Enqueue(line)) {
            recovered_cues[i] |= actor_bit;
        }
    }
}

void openshao::dialogue::SetRestoreGaps(double voice, double scene) {
    VoiceGap = voice;
    SceneGap = scene;
    gap = voice;
    quiet = 0;
}

void openshao::dialogue::AuditTick() { AuditSceneAudio(false); }
