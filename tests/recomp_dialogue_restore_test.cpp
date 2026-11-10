#include "dialogue_queue.h"
#include "kernel/kernel.h"
#include "launcher.h"
#include <array>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

namespace {
std::array<uint8_t, 8 << 20> memory{};
openshao::launcher::Settings settings;
bool paused = false, accept_message = true;
bool native_stream_playing = false;
bool defer_survival_music = false;
unsigned native_music_stops = 0;
bool accept_animation = true;
bool accept_audio = true;
bool accept_stream = true;
unsigned scene_calls = 0;
unsigned activation_calls = 0;
unsigned stream_play_calls = 0;
uint32_t last_native_sound = 0, last_native_bank = 0;
uint32_t variation_seed = 0;
std::vector<uint32_t> voice_stops;
uint32_t caption_lookup_sound = 0;
std::vector<std::array<uint32_t, 2>> messages;
std::vector<std::array<uint32_t, 3>> sounds;
std::vector<uint32_t> streams;
std::array<uint32_t, 16> submitted_message{};
void SubmitMessage(uint32_t info) {
    for (uint32_t i = 0; i < submitted_message.size(); ++i)
        submitted_message[i] = kernel::R32(info + i * 4);
    messages.push_back({submitted_message[6], submitted_message[5]});
    kernel::G<uint8_t>(0x4E74E0) = 1;
    kernel::W32(0x4E74F4, submitted_message[5]);
    kernel::W32(0x4E74F8, submitted_message[6]);
}
void Original(X86Context& ctx, unsigned arguments = 0) {
    ctx.eax = 0x1234;
    ctx.ecx = 0x5678;
    ctx.esp += 4 + arguments * 4;
}
}
namespace logx { void Write(Level, const char*, ...) {} }
uint32_t checkpoint_restores = 0;
uint32_t save_slot_transfers = 0;
namespace openshao::difficulty { uint32_t CheckpointRestores() { return checkpoint_restores; }
uint32_t SaveSlotTransfers() { return save_slot_transfers; } }
namespace openshao::launcher { const Settings& Current() { return settings; } }
namespace openshao::ed_nexus { bool GameplayPaused() { return paused; } }
namespace openshao::survival {void VoicePlayed(uint32_t,uint32_t){} void UpdateVoice(X86Context&){} bool FilterLevelMessage(uint32_t){return false;} bool DeferMusic(uint32_t){return defer_survival_music;} void TriggerActivated(X86Context&,uint32_t,bool){} }
namespace kernel {
uint8_t* g_base = memory.data();
uint32_t CallGuest(X86Context& ctx, uint32_t fn, const uint32_t* args, size_t count) {
    // Guest execution is the external boundary. The host hooks and their state
    // below are production code; the native message renderer is tested in-game.
    if (fn == 0x1A76D0) return G<uint8_t>(0x6C4198);
    if (fn == 0x1353F && !count) {
        ++native_music_stops;
        W32(0x62DC08,0);
        W32(0x62DC0C,0);
        return 0;
    }
    if (fn == 0x152360) {
        if (ctx.ecx != 0x653C64 || !count || !args[0]) std::abort();
        return native_stream_playing ? 1u : 0u;
    }
    if (fn == 0x15A430) return variation_seed;
    if (fn == 0x152380) {
        if (ctx.ecx != 0x653C64 || count != 1) std::abort();
        voice_stops.push_back(args[0]);
        return 0;
    }
    if (fn == 0x150DF0) {
        for (uint32_t i = 0; i < R32(0x653BCC); ++i)
            if (R32(R32(0x653BD4) + i * 24) == args[0]) return i;
        return uint32_t(-1);
    }
    if ((fn == 0x151560 && count == 8) || (fn == 0x151610 && count == 9)) {
        const auto bank = fn == 0x151560 ? uint32_t(-1) : args[0];
        const auto sound = args[fn == 0x151560 ? 0 : 1];
        sounds.push_back({fn, bank, sound});
        ctx.eax = 0xDEAD;
        ctx.ecx = 0xBEEF;
        ctx.esp += 100;
        return accept_audio ? 0x5000u : 0u;
    }
    if (fn == 0x1385F && count == 2) {
        messages.push_back({args[0], args[1]});
        if (accept_message) {
            G<uint8_t>(0x4E74E0) = 1;
            W32(0x4E74F4, args[1]);
            W32(0x4E74F8, args[0]);
        }
        ctx.eax = 0xDEAD;
        ctx.ecx = 0xBEEF;
        ctx.esp += 100;
        return 0;
    }
    if (fn == 0x135A3 && count == 2) {
        if (ctx.ecx != 0x4E6E20 || args[1] != 1) std::abort();
        SubmitMessage(args[0]);
        ctx.eax = 0xDEAD;
        ctx.ecx = 0xBEEF;
        ctx.esp += 100;
        return 0;
    }
    std::cerr << "Unexpected guest call " << std::hex << fn << '\n';
    std::abort();
}
}
X86_FUNC(__imp__sub_001A1880) { Original(ctx); }
X86_FUNC(__imp__sub_001A1860) { kernel::W32(0x6C03DC, 0); Original(ctx); }
X86_WEAK_FUNC(sub_001A1860, __imp__sub_001A1860)
X86_FUNC(__imp__sub_000135A3) {
    SubmitMessage(kernel::R32(ctx.esp + 4));
    Original(ctx, 2);
}
X86_WEAK_FUNC(sub_000135A3, __imp__sub_000135A3)
X86_FUNC(__imp__sub_001A1850) { kernel::W32(0x6C03DC, 0); Original(ctx); }
X86_FUNC(__imp__sub_001A18F0) {
    const auto args = kernel::R32(kernel::R32(ctx.esp + 4) + 8);
    const auto actor = kernel::R32(kernel::R32(ctx.esp + 8) + 0x10);
    last_native_sound = kernel::R32(args);
    last_native_bank = kernel::R32(args+4);
    if (last_native_sound && !kernel::G<uint8_t>(0x6C4198) && !kernel::R32(0x6C03DC) &&
        (kernel::R32(args + 8) == 0xFFFFFFFFu || kernel::R16(0x5D38B6 + actor * 0x3238) || !kernel::R32(args + 8))) {
        kernel::W32(0x6C03DC, 0x5000);
    }
    Original(ctx, 3);
}
X86_FUNC(__imp__sub_00170330) { ++activation_calls; Original(ctx, 3); }
X86_FUNC(__imp__sub_00016924) {
    if (const auto manager = kernel::R32(0x599844)) kernel::W32(manager + 0x1C, 1);
    Original(ctx, 3);
}
X86_WEAK_FUNC(sub_00016924, __imp__sub_00016924)
X86_WEAK_FUNC(sub_00170330, __imp__sub_00170330)
X86_FUNC(__imp__sub_0017C040) {
    const auto sound=kernel::R32(ctx.esp + 4);
    streams.push_back(sound);
    if (accept_stream) { kernel::W32(0x69654C,sound);kernel::W32(0x696550,0x5000); }
    Original(ctx);
}
X86_WEAK_FUNC(sub_0017C040, __imp__sub_0017C040)
X86_FUNC(__imp__sub_0017C0B0) {
    ++stream_play_calls;
    const auto sound = kernel::R32(ctx.esp + 4);
    if (accept_stream) for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t entry = 0x69654C + i * 12;
        if (kernel::R32(entry) != sound || !kernel::R32(entry + 4)) continue;
        kernel::W32(entry + 8, kernel::R32(entry + 4));
        kernel::W32(entry + 4, 0);
        break;
    }
    Original(ctx);
}
X86_WEAK_FUNC(sub_0017C0B0, __imp__sub_0017C0B0)
X86_FUNC(__imp__sub_0017C110) { Original(ctx); }
X86_WEAK_FUNC(sub_0017C110, __imp__sub_0017C110)
X86_FUNC(__imp__sub_00016C9E) { Original(ctx); }
X86_WEAK_FUNC(sub_00016C9E, __imp__sub_00016C9E)
X86_FUNC(__imp__sub_0001698D) { Original(ctx); }
X86_WEAK_FUNC(sub_0001698D, __imp__sub_0001698D)
X86_FUNC(__imp__sub_0001353F) { Original(ctx); }
X86_WEAK_FUNC(sub_0001353F, __imp__sub_0001353F)
X86_FUNC(__imp__sub_00012D60) { Original(ctx); }
X86_WEAK_FUNC(sub_00012D60, __imp__sub_00012D60)
X86_FUNC(__imp__sub_00012B85) { Original(ctx); }
X86_WEAK_FUNC(sub_00012B85, __imp__sub_00012B85)
X86_FUNC(__imp__sub_000125A4) { Original(ctx); }
X86_WEAK_FUNC(sub_000125A4, __imp__sub_000125A4)
X86_FUNC(__imp__sub_00152380) { Original(ctx); }
X86_WEAK_FUNC(sub_00152380, __imp__sub_00152380)
X86_FUNC(__imp__sub_000147F0) { Original(ctx, 3); }
X86_WEAK_FUNC(sub_000147F0, __imp__sub_000147F0)
X86_FUNC(__imp__sub_000111EF) { Original(ctx); ctx.eax = caption_lookup_sound; }
X86_WEAK_FUNC(sub_000111EF, __imp__sub_000111EF)
X86_FUNC(__imp__sub_00176270) {
    const auto slot = kernel::R32(ctx.esp + 4), animation = kernel::R32(ctx.esp + 8);
    if (accept_animation && slot < 14) kernel::W32(0x5D1AB8 + slot * 0x3238 + 0x24, animation);
    Original(ctx);
}
X86_WEAK_FUNC(sub_00176270, __imp__sub_00176270)
X86_FUNC(__imp__sub_00151560) {
    sounds.push_back({0x151560, uint32_t(-1), kernel::R32(ctx.esp+4)});
    Original(ctx,8); ctx.eax=accept_audio?0x5000:0;
}
X86_WEAK_FUNC(sub_00151560, __imp__sub_00151560)
X86_FUNC(__imp__sub_00151610) {
    const auto bank=kernel::R32(ctx.esp+4), sound=kernel::R32(ctx.esp+8);
    uint32_t selected=sound;
    if (kernel::R32(0x3204)==sound && kernel::R32(0x3208)) {
        const auto index=kernel::R32(0x320C)%kernel::R32(0x3208);
        selected=kernel::R32(0x3210+index*4);
        kernel::W32(0x320C,(index+1)%kernel::R32(0x3208));
    }
    sounds.push_back({0x151610,bank,selected});
    Original(ctx,9); ctx.eax=accept_audio?0x5000:0;
}
X86_WEAK_FUNC(sub_00151610, __imp__sub_00151610)
X86_FUNC(__imp__sub_00014985) {
    ++scene_calls;
    kernel::G<uint8_t>(0x514505) = 1;
    kernel::G<uint8_t>(0x5144F8) = 1;
    Original(ctx, 3);
}
// Retail forwarding before restoration also links, so the missing behavior
// produces a failing assertion rather than an unresolved symbol.
X86_WEAK_FUNC(sub_00014985, __imp__sub_00014985)
X86_EXTERN_FUNC(sub_001A1880);
X86_EXTERN_FUNC(sub_001A1850);
X86_EXTERN_FUNC(sub_00014985);
X86_EXTERN_FUNC(sub_001A18F0);
X86_EXTERN_FUNC(sub_00170330);
X86_EXTERN_FUNC(sub_0017C040);
X86_EXTERN_FUNC(sub_0017C0B0);
X86_EXTERN_FUNC(sub_000147F0);
X86_EXTERN_FUNC(sub_000111EF);
X86_EXTERN_FUNC(sub_00176270);
X86_EXTERN_FUNC(sub_00151560);
X86_EXTERN_FUNC(sub_00151610);
X86_EXTERN_FUNC(sub_000135A3);
X86_EXTERN_FUNC(sub_001A1860);

int main() {
    // Ordering checks below advance one voice update at a time; pacing has its own timed checks.
    openshao::dialogue::SetRestoreGaps(0, 0);
    unsigned failures = 0;
    auto check = [&](bool ok, const char* why) {
        if (!ok) { ++failures; std::cerr << why << '\n'; }
    };
    auto reset = [&] {
        memory.fill(0);
        variation_seed = 0;
        voice_stops.clear();
        caption_lookup_sound = 0;
        accept_animation = true;
        accept_audio = true;
        accept_stream = true;
        native_stream_playing = false;
        settings.restore_dialogue = true;
        paused = false;
        accept_message = true;
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_001A1850(ctx, memory.data());
        kernel::W32(0x505970, 98);
        kernel::W32(0x639224, 0x80010000);
        kernel::W32(0x4EF42C, 1);  // gameplay frames, not a held loading frame
        messages.clear();
        sounds.clear();
        streams.clear();
    };
    reset();
    defer_survival_music=true;
    kernel::W32(0x62DC08,0x5000);
    kernel::W32(0x62DC0C,0x12345678);
    kernel::W32(0x1004,0x709D5875);
    X86Context music_ctx{};music_ctx.esp=0x1000;music_ctx.eax=0xABCD;
    sub_0001698D(music_ctx,memory.data());
    check(native_music_stops==1 && !kernel::R32(0x62DC08) && !kernel::R32(0x62DC0C) &&
          music_ctx.esp==0x1004 && music_ctx.eax==0xABCD,
          "Deferring Level One music stops a preexisting native track exactly once and preserves the calling ABI");
    music_ctx={};music_ctx.esp=0x1000;music_ctx.eax=0xBCDE;
    sub_0001698D(music_ctx,memory.data());
    check(native_music_stops==1 && music_ctx.esp==0x1004 && music_ctx.eax==0xBCDE,
          "Repeated deferred requests cannot replay or repeatedly stop music");
    defer_survival_music=false;
    music_ctx={};music_ctx.esp=0x1000;
    sub_0001698D(music_ctx,memory.data());
    check(music_ctx.esp==0x1004 && music_ctx.eax==0x1234,
          "Ordinary music requests still reach the native music player outside Survival");
    auto tick = [&] {
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_001A1880(ctx, memory.data());
        check(ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1004,
              "Caption submission must preserve the native update result and stack");
    };
    auto play = [&](uint32_t scene = 0x2D2875B0) {
        kernel::W32(0x514508, scene);
        // Play argument zero is a position/camera selector, not the scene hash.
        kernel::W32(0x1004, 0x2000);
        kernel::W32(0x2008, 0x2100);
        kernel::W32(0x2100, 0xBD0DDDaf);
        X86Context ctx{}; ctx.esp = 0x1000;
        const auto before = scene_calls;
        sub_00014985(ctx, memory.data());
        check(scene_calls == before + 1 && ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1010,
              "The restoration must forward the original scene without changing its result");
    };
    auto finish = [&] {
        kernel::G<uint8_t>(0x514505) = 0;
        kernel::G<uint8_t>(0x5144F8) = 0;
        kernel::G<uint8_t>(0x6C4198) = 0;
    };

    auto voice = [&](uint32_t actor_slot, uint32_t sound = 0x12345678, uint32_t bank = 0x871111EC,
                     uint32_t condition = 0xFFFFFFFF, uint32_t trigger = 0) {
        kernel::W32(0x1004, 0x2000); kernel::W32(0x1008, 0x2200);
        kernel::W32(0x2008, 0x2100); kernel::W32(0x2100, sound);
        kernel::W32(0x2104, bank); kernel::W32(0x2108, condition);
        kernel::W32(0x2210, actor_slot);
        kernel::W32(0x100C, trigger);
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_001A18F0(ctx, memory.data());
        check(ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1010,
              "Voice recovery must preserve native registers and its three-argument stack");
    };

    auto tutorial = [&](uint32_t text = 24, uint32_t type = 6) {
        for (uint32_t i = 0; i < 16; ++i) kernel::W32(0x2400 + i * 4, i + 0x100);
        kernel::W32(0x2414, type); kernel::W32(0x2418, text);
        kernel::W32(0x1004, 0x2400); kernel::W32(0x1008, 1);
        X86Context ctx{}; ctx.esp = 0x1000; ctx.ecx = 0x4E6E20;
        sub_000135A3(ctx, memory.data());
        check(ctx.esp == 0x100C, "Tutorial submission must retain the native callee-cleaned ABI");
    };
    auto tutorial_voice = [&] {
        reset(); kernel::W32(0x505970, 96); kernel::W32(0x514508, 0x18797C6C);
        voice(0, 0x65138D8A); native_stream_playing = true;
    };
    tutorial_voice(); tutorial();
    check(messages.empty() && kernel::R32(0x6C03DC) == 0x5000,
          "The first attack tutorial must wait for the preceding character voice instead of cutting it");
    // The native event uses a temporary stack descriptor. Deferral must own a copy.
    kernel::W32(0x2418, 999); kernel::W32(0x2420, 999);
    tick(); tick();
    check(messages.empty(), "The first attack tutorial must wait for actual playback completion");
    native_stream_playing = false; paused = true; tick();
    check(messages.empty(), "A completed tutorial voice must not open its panel while paused");
    paused = false; tick(); tick();
    check(messages.size() == 1 && messages[0] == std::array<uint32_t, 2>{24, 6} &&
          submitted_message[8] == 0x108,
          "The tutorial must submit its original descriptor exactly once after the voice ends");

    tutorial_voice(); native_stream_playing = false; tutorial();
    check(messages.size() == 1, "An already-finished voice must not add a tutorial delay");
    tutorial_voice(); tutorial(25, 0);
    check(messages.size() == 1, "Non-modal messages must retain their native timing");
    for (uint32_t room : {96u, 98u, 136u}) for (uint32_t type : {6u, 8u, 9u}) {
        tutorial_voice(); kernel::W32(0x505970, room); tutorial(73, type);
        check(messages.size() == 1,
              "Other tutorial panels and feedback must open on their native schedule, without a message backlog");
    }
    tutorial_voice(); kernel::W32(0x6C03DC, 0x7000); tutorial(24, 6);
    check(messages.size() == 1, "The first attack panel must not wait for an unrelated voice");
    tutorial_voice(); tutorial();
    kernel::W32(0x6C03DC, 0); voice(0, 0x7183CEE9); tick();
    check(messages.size() == 1, "A replacement voice must not extend the pending attack panel's original wait");
    tutorial_voice(); tutorial(24, 6); tutorial(73, 8);
    check(messages.size() == 1 && messages[0] == std::array<uint32_t, 2>{73, 8},
          "An unrelated feedback panel must not wait behind the pending attack panel");
    native_stream_playing = false; tick(); tick();
    check(messages.size() == 1, "A newer tutorial must invalidate a pending older panel instead of replaying it later");
    tutorial_voice(); tutorial(); settings.restore_dialogue = false; tick();
    check(messages.size() == 1, "Disabling restoration must release the original pending tutorial");
    auto presentation_stop = [&](uint32_t caller, uint32_t type, uint32_t text, uint32_t mapped_sound = 0) {
        kernel::W32(0x1000, caller); kernel::G<uint8_t>(0x4E74E0) = 1;
        kernel::W32(0x4E74F4, type); kernel::W32(0x4E74F8, text);
        kernel::W32(0x5B04A8, 0x6000); kernel::W32(0x6000 + text * 4, mapped_sound);
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_001A1860(ctx, memory.data());
        check(ctx.esp == 0x1004, "Voice-stop filtering must retain the native ABI");
    };
    for (uint32_t caller : {0x85EE1u, 0x864B7u}) {
        tutorial_voice(); presentation_stop(caller, 8, 73);
        check(kernel::R32(0x6C03DC) == 0x5000,
              "Unvoiced combat feedback must preserve the playing instruction without delaying the feedback");
        tutorial_voice(); presentation_stop(caller, 9, 110);
        check(kernel::R32(0x6C03DC) == 0x5000, "An unvoiced lock-on hint must preserve current speech");
        tutorial_voice(); presentation_stop(caller, 8, 73, 0x12345678);
        check(!kernel::R32(0x6C03DC), "A voiced replacement must retain native voice ownership");
    }
    tutorial_voice(); presentation_stop(0x716BF, 8, 73);
    check(!kernel::R32(0x6C03DC), "A real scene change must still stop its old voice");
    tutorial_voice(); settings.restore_dialogue = false; presentation_stop(0x85EE1, 8, 73);
    check(!kernel::R32(0x6C03DC), "Disabled restoration must keep the native presentation stop");
    tutorial_voice(); settings.restore_dialogue = false; tutorial();
    check(messages.size() == 1, "Disabling dialogue restoration must retain retail tutorial timing");
    for (uint32_t address : {0x505970u, 0x639224u, 0x514508u}) {
        tutorial_voice(); tutorial(); kernel::W32(address, kernel::R32(address) + 1);
        native_stream_playing = false; tick();
        check(messages.empty(), "A deferred tutorial must not survive a change of room, world or scene");
    }
    tutorial_voice(); tutorial(); play(0x18797C6C); finish();
    native_stream_playing = false; tick();
    check(messages.empty(), "Replaying the same scene must cancel its old deferred tutorial");
    tutorial_voice(); tutorial();
    X86Context cleared{}; cleared.esp = 0x1000; sub_001A1850(cleared, memory.data());
    native_stream_playing = false; tick();
    check(messages.empty(), "Level initialization must clear a deferred tutorial");

    reset(); tick();
    check(messages.empty(), "Unrelated gameplay must not display the Cage message");
    play(); tick();
    check(messages.empty(), "The message must wait while the scene is still loading");
    kernel::G<uint8_t>(0x6C4198) = 1; tick();
    check(messages.empty(), "The message must wait while the cinematic is playing");
    finish(); kernel::W32(0x6C03DC, 1); tick();
    check(messages.empty(), "The message must not interrupt an existing script voice");
    kernel::W32(0x6C03DC, 0); kernel::G<uint8_t>(0x4E74E0) = 1; tick();
    check(messages.empty(), "The message must not replace an existing caption");
    kernel::G<uint8_t>(0x4E74E0) = 0; paused = true; tick();
    check(messages.empty(), "The message must wait while gameplay is paused");
    paused = false;
    for (uint32_t gate : {0x5146E4u, 0x5B22FCu, 0x6C0404u, 0x4E751Cu}) {
        kernel::W32(gate, 1); tick();
        check(messages.empty(), "The message must wait while native caption presentation is blocked");
        kernel::W32(gate, 0);
    }
    tick();
    check(messages.size() == 1 && messages[0] == std::array<uint32_t, 2>{33, 5},
          "The completed Cage scene must submit localized caption 33 with automatic template 5");
    kernel::W32(0x4E74E4, 0x68D2A09B);
    kernel::W32(0x4E74E8, 0x100);
    tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick(); play(); finish(); tick();
    check(messages.size() == 1, "Replaying the cue must not repeat the restored caption in one room load");

    reset(); play(); finish(); tick();
    check(messages.size() == 1, "Scene cleanup after a skip must also allow the message");
    reset(); play(); finish();
    kernel::W32(0x69654C, 0xABCD1234); kernel::W32(0x696550, 0); kernel::W32(0x696554, 0x5000);
    native_stream_playing = true; tick();
    check(messages.empty(), "The restored Cage caption must not talk over an original audiostream");
    native_stream_playing = false; tick();
    check(messages.size() == 1, "The restored Cage caption must follow once the audiostream ends");
    openshao::dialogue::SetRestoreGaps(.3, .3);
    reset(); play(); finish(); tick();
    check(messages.empty(), "The restored Cage caption must not start in the frame its scene ends");
    std::this_thread::sleep_for(std::chrono::milliseconds(350)); tick();
    check(messages.size() == 1, "The restored Cage caption must start after the short silence");
    openshao::dialogue::SetRestoreGaps(0, 0);
    reset(); play(); settings.restore_dialogue = false; finish(); tick();
    settings.restore_dialogue = true; tick();
    check(messages.empty(), "Disabling restoration must cancel the pending caption");
    reset(); play(); finish(); kernel::W32(0x505970, 99); tick();
    kernel::W32(0x505970, 98); tick();
    check(messages.empty(), "Leaving the room must cancel the pending caption permanently");
    reset(); play(); finish(); kernel::W32(0x639224, 0x80020000); tick();
    check(messages.empty(), "A replacement world must not inherit a pending caption");
    reset(); play(); play(0x11111111); finish(); tick();
    check(messages.empty(), "Another scene must cancel the pending Cage caption");
    reset(); kernel::W32(0x505970, 96); play(); finish(); tick();
    check(messages.empty(), "The Cage restoration must stay in the authored room");
    reset(); play(); finish(); accept_message = false; tick();
    accept_message = true; tick();
    check(messages.size() == 2 && kernel::R32(0x4E74F8) == 33,
          "A native rejection must leave the request pending until it is accepted");
    reset(); play(); finish(); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0;
    tick();
    check(messages.size() == 2,
          "Skip cleanup that clears an unplayed caption must not lose the restored line");

    reset(); kernel::W32(0x6C03DC, 1); voice(13);
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x12345678,
          "A busy voice from the fourteenth native actor slot must be recovered");
    reset(); kernel::W32(0x6C03DC, 1); voice(14);
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "An out-of-range actor must not create a deferred voice");
    reset(); kernel::W32(0x6C03DC, 1); voice(0);
    kernel::W32(0x6C03DC, 0); kernel::G<uint8_t>(0x4E74E0) = 1;
    kernel::W32(0x4E74E8, 0x200); tick();
    check(sounds.empty(), "Deferred story voices must wait for a voiced caption to finish");
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1, "The deferred story voice must resume after the caption");
    reset(); kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74E8, 0x200);
    voice(0);
    check(!kernel::R32(0x6C03DC), "A new script voice must not overlap an active voiced caption");
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1, "A script voice delayed behind a caption must remain available");
    reset(); kernel::G<uint8_t>(0x4E74E0)=1;kernel::W32(0x4E74E4,0x68D2A09B);
    voice(0);tick();
    check(!kernel::R32(0x6C03DC) && sounds.empty(),
          "A queued voiced caption must reserve its slot before the renderer creates its voice handle");
    kernel::G<uint8_t>(0x4E74E0)=0;kernel::W32(0x6C03DC,0);tick();
    check(sounds.size()==1, "A script voice behind a newly queued caption must remain recoverable");
    reset(); kernel::W32(0x6C03DC, 1); voice(0);
    kernel::W32(0x639224, 0x80020000); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "Replacing a world with the same room ID must drop old voices");
    reset(); kernel::W32(0x6C03D8, 0x2E24BA3C);
    kernel::G<uint8_t>(0x5D1AB8 + 13 * 0x3238 + 0x2429) = 7;
    kernel::W32(0x6C03DC, 1); voice(13, 0x33445566, 0x2E24BA3C);
    kernel::G<uint8_t>(0x5D1AB8 + 13 * 0x3238 + 0x2429) = 8;
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "A changed actor bank must not replay a voice on a replacement actor");
    reset(); kernel::W32(0x6C03DC, 1); voice(0);
    kernel::W32(0x5D1AB8 + 0x68, 99); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "Reusing an actor slot for another object must cancel its old dialogue");
    reset(); kernel::W32(0x6C03DC, 1); voice(0);
    kernel::W32(0x639224, 0); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "World teardown must cancel dialogue before native audio resources unload");

    for (const bool observed_frame : {false, true}) {
        reset(); kernel::W32(0x505970, 96); play(0x11112222);
        if (observed_frame) tick();
        kernel::W32(0x6C03DC, 1); voice(0, 0x12345678);
        finish(); kernel::W32(0x6C03DC, 0); tick(); tick();
        check(sounds.empty(), observed_frame
            ? "Normal scene completion must discard speech deferred during its cleanup"
            : "Skipping a scene before its first voice update must discard its deferred speech");
    }
    reset(); kernel::W32(0x514508, 0x11112222);
    kernel::W32(0x6C03DC, 1); voice(0);
    kernel::W32(0x514508, 0x33334444); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "A changed scene must cancel old speech even if its active frames were missed");

    reset(); kernel::W32(0x6C03DC, 1); voice(0, 0x11110000);
    kernel::W32(0x6C03DC, 0); voice(0, 0x22220000); tick();
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 2 && sounds[0][2] == 0x11110000 && sounds[1][2] == 0x22220000,
          "New native requests must wait behind older recovered lines");
    // Retail audiostreambuffer holds +4 until audiostreamplay transfers the
    // handle to +8; neither belongs to the original vosound/caption slot.
    reset(); kernel::W32(0x6C03DC, 1); voice(0, 0x12345678);
    kernel::W32(0x6C03DC, 0);
    kernel::W32(0x69654C, 0xABCD1234); kernel::W32(0x696550, 0x5000);
    tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0x12345678,
          "An unplayed future stream must never starve an already-authored deferred vosound");
    reset(); kernel::W32(0x6C03DC, 1); voice(0, 0x12345678);
    kernel::W32(0x6C03DC, 0);
    kernel::W32(0x69654C, 0xABCD1234);
    kernel::W32(0x696550, 0); kernel::W32(0x696554, 0x5000);
    native_stream_playing = true; tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0x12345678,
          "An authored vosound must follow its native speech slot independently of other active streams");
    native_stream_playing = false; tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0x12345678,
          "Ending a separate stream must not repeat an already restored script line");
    reset(); kernel::W32(0x6C03DC, 1); voice(0); tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(4100)); tick();
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1, "A localized voice longer than four seconds must not expire its waiting response");
    openshao::dialogue::SetRestoreGaps(.3, .3);
    reset(); kernel::W32(0x6C03DC, 1); voice(0); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "A restored line must not start glued to the speech that delayed it");
    std::this_thread::sleep_for(std::chrono::milliseconds(350)); tick();
    check(sounds.size() == 1, "A restored line must play once the short silence has passed");
    openshao::dialogue::SetRestoreGaps(0, 0);
    reset(); kernel::W32(0x6C03DC, 1); voice(0); kernel::W32(0x6C03DC, 0);
    kernel::W32(0x6C92F8, 1); tick();
    check(sounds.empty(), "A restored line must not play during a held loading frame");
    kernel::W32(0x6C92F8, 0); kernel::W32(0x4EF42C, 0); tick();
    check(sounds.size() == 1,
          "A zero render-callback flag must not suppress dialogue during a running Wu Shi world");
    kernel::W32(0x4EF42C, 1); tick();
    check(sounds.size() == 1, "A later render-callback flag change must not duplicate restored dialogue");
    reset(); kernel::W16(0x5D38B6, 1); voice(0, 0, 0x871111EC, 1);
    check(last_native_sound == 0xEFD60587 && !kernel::R32(0x2100),
          "Kung Lao's connected moon event must use its existing original recording and restore the script bytes");
    // Native once gates check trigger+0x70 bit1: _d_kl_moon2b (0x401) and the 0x403
    // moon triggers must keep separate gates.
    for (uint32_t trigger : {0x2500u, 0x2600u}) {
        reset(); kernel::W16(0x5D38B6, 1);
        kernel::W32(trigger + 0x70, 0x401);
        voice(0, 0, 0x871111EC, 1, trigger);
        check((kernel::R32(trigger + 0x70) & 2) != 0,
              "Restoring the moon cue must retain native once-only scheduling instead of looping every frame");
        check((kernel::R32(trigger + 0x70) & ~2u) == 0x401,
              "Repairing moon repetition must preserve the trigger's other conditions and enable flags");
    }
    reset(); kernel::W16(0x5D38B6, 1); kernel::W32(0x2570, 0x401);
    voice(0, 0xA8767F57, 0x871111EC, 1, 0x2500);
    check(kernel::R32(0x2570) == 0x401, "Other native speech must retain its repeat behavior");
    reset(); kernel::W16(0x5D38B6, 1); kernel::W32(0x505970, 96); kernel::W32(0x2570, 0x401);
    voice(0, 0, 0x871111EC, 1, 0x2500);
    check(kernel::R32(0x2570) == 0x401, "The moon scheduling repair must stay in its original room");
    reset(); kernel::W16(0x5D38B6, 1); settings.restore_dialogue = false; voice(0, 0, 0x871111EC, 1);
    check(last_native_sound == 0, "Disabling restoration must preserve the original zero sound request");

    auto activate = [&](uint32_t room, uint32_t hash, int16_t character, uint32_t enable = 1,
                        uint32_t index = 0xFFFFFFFF) {
        kernel::W32(0x505970, room); kernel::W16(0x5D38B6, character);
        kernel::W32(0x1004, 0x2000); kernel::W32(0x1008, 0x2200);
        kernel::W32(0x2008, 0x2100); kernel::W32(0x2100, enable);
        kernel::W32(0x2104, hash); kernel::W32(0x2108, index); kernel::W32(0x2210, 0);
        X86Context ctx{}; ctx.esp = 0x1000;
        const auto before = activation_calls;
        sub_00170330(ctx, memory.data());
        check(activation_calls == before + 1 && ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1010,
              "Missing-trigger recovery must forward the native activation and preserve its result");
    };
    auto original_stream_play = [&](uint32_t sound) {
        kernel::W32(0x1004, sound);
        X86Context ctx{}; ctx.esp = 0x1000;
        const auto before = stream_play_calls;
        sub_0017C0B0(ctx, memory.data());
        check(stream_play_calls == before + 1 && ctx.eax == 0x1234 &&
              ctx.ecx == 0x5678 && ctx.esp == 0x1004,
              "Original audiostreamplay must forward the native function unchanged");
    };
    auto original_stream_event = [&](uint32_t sound) {
        kernel::W32(0x1004,0x2000);kernel::W32(0x2008,0x2100);
        kernel::W32(0x2100,sound);
        X86Context ctx{};ctx.esp=0x1000;
        sub_000147F0(ctx,memory.data());
        check(ctx.eax==0x1234 && ctx.ecx==0x5678 && ctx.esp==0x1010,
              "Original snake script event observer must forward the retail audiostreamplay handler unchanged");
    };
    struct CueFixture { uint32_t target, sound; int16_t character; };
    const CueFixture cues[] = {
        {0xC1CE6AC6, 0x7CE0B357, 0}, {0x0EAC5D51, 0xE0967133, 1},
        {0xFB84C07D, 0x3B40C987, 0}, {0x2326CE4B, 0xDDF65883, 1},
        {0xCB402320, 0x0620E037, 0}, {0x6465CF88, 0xA7360BE3, 1},
    };
    // The warlord remarks need the warlord: an enemy that appears with _d_war_evnts' spawn.
    auto enemy = [&](uint32_t slot, uint32_t id, int32_t health) {
        const uint32_t at = 0x5D1AB8 + slot * 0x3238, component = 0x7400 + slot * 0x400;
        kernel::W32(at + 0x68, id); kernel::W16(at + 0x1DFE, 20); kernel::W32(at + 0x200, component);
        kernel::W32(component + 0x368, uint32_t(health)); kernel::W32(component + 0x370, 500);
    };
    for (auto cue : cues) {
        const bool warlord = cue.target == 0xCB402320 || cue.target == 0x6465CF88;
        if (warlord) {
            reset(); kernel::W32(0x505970, 101); enemy(4, 0xEE01, 300); tick();
            activate(101, cue.target, cue.character); tick();
            check(sounds.empty(), "Other enemies alone do not stand in for the warlord");
            reset(); kernel::W32(0x505970, 101); enemy(4, 0xEE01, 300); tick();
            activate(101, 0x94FE469F, cue.character, 1, 0x3A); tick();
            activate(101, cue.target, cue.character); tick();
            check(sounds.empty(), "When the warlord does not appear (already beaten), the monk stays quiet");
        }
        reset();
        if (warlord) {
            kernel::W32(0x505970, 101); enemy(4, 0xEE01, 300); tick();
            enemy(5, 0xB055, 500); activate(101, 0x94FE469F, cue.character, 1, 0x3A); tick();
        }
        activate(101, cue.target, cue.character); tick();
        check(sounds.size() == 1 && sounds[0][2] == cue.sound,
              "A surviving Wu Shi activation must recover its exact authored character line");
        kernel::W32(0x6C03DC, 0); activate(101, cue.target, cue.character); tick();
        check(sounds.size() == 1, "An authored recovery cue must play only once per actor and room load");
        reset(); activate(101, cue.target, 2); tick();
        check(sounds.empty(), "Character-specific recovered lines must not use a substitute character's voice");
        reset(); activate(101, cue.target, cue.character, 0); tick();
        check(sounds.empty(), "A trigger deactivation must not start recovered dialogue");
        reset(); activate(101, cue.target, cue.character, 1, 12); tick();
        check(sounds.empty(), "A valid original trigger must not gain a duplicate recovery request");
    }
    reset(); activate(96, 0xB001C237, 0); tick();
    check(sounds.empty(), "Raiden's combo tip (vo_rdn_help_gl_ps2_13) was removed from the story");
    reset(); tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(4100));
    activate(101, 0xC1CE6AC6, 0); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x7CE0B357,
          "A new recovered line must not inherit elapsed time from before it entered an empty queue");
    reset(); settings.restore_dialogue = false; activate(101, 0xC1CE6AC6, 0); tick();
    check(sounds.empty(), "The existing enhancement switch must disable missing-trigger recovery");

    reset(); kernel::G<uint8_t>(0x6C4198) = 1; activate(101, 0xC1CE6AC6, 0); tick();
    check(sounds.empty(), "An authored post-scene line must wait for its current cinematic");
    kernel::G<uint8_t>(0x6C4198) = 0; tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x7CE0B357,
          "The authored post-scene line must remain available when the scene ends");
    reset(); kernel::G<uint8_t>(0x514505) = 1;
    activate(101, 0xC1CE6AC6, 0); tick();
    check(sounds.empty(), "A recovered post-scene line must wait while the scene loads before its cinematic flag is set");
    kernel::G<uint8_t>(0x514505) = 0; tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x7CE0B357,
          "The loading-scene guard must retain the authored post-scene request");

    reset(); kernel::W32(0x6C03DC, 1); kernel::W32(0x5D1AB8 + 0x68, 10);
    voice(0, 0x11112222); kernel::W32(0x5D1AB8 + 0x68, 11); voice(0, 0x11112222);
    kernel::W32(0x6C03DC, 0); tick(); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x11112222,
          "A replaced actor's stale request must not suppress its replacement's same-sound request");
    auto kitana_bank = [&] {
        kernel::W32(0x653BCC, 1); kernel::W32(0x653BD4, 0x3000);
        kernel::W32(0x3000, 0xAECADCD8); kernel::W32(0x3004, 0x3100);
        kernel::W32(0x300C, 0x3200); kernel::W32(0x3200, 1);
        kernel::W32(0x3204, 0xCE417166); kernel::W32(0x3208, 2);
        kernel::W32(0x320C, 0); kernel::W32(0x3210, 0x983580A0); kernel::W32(0x3214, 0xDF95FA70);
    };
    reset(); kitana_bank(); kernel::W32(0x505970, 194); voice(0, 0xDF95FA70);
    check(last_native_sound == 0xCE417166 && kernel::R32(0x2100) == 0xDF95FA70 && kernel::R32(0x2104) == 0x871111EC,
          "Kitana's authored laugh must select her original localized variation group and preserve script bytes");
    reset(); kitana_bank(); kernel::W32(0x505970, 194); kernel::W32(0x6C03DC, 1);
    voice(0, 0xDF95FA70); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1 && sounds[0] == std::array<uint32_t, 3>{0x151610, 0, 0xCE417166},
          "A deferred Kitana variation must use the banked variation resolver");
    reset(); kernel::W32(0x505970, 194); voice(0, 0xDF95FA70);
    check(last_native_sound == 0xDF95FA70, "An unavailable variation bank must preserve Kitana's original take");
    reset(); kitana_bank(); kernel::W32(0x505970, 194); settings.restore_dialogue = false;
    voice(0, 0xDF95FA70);
    check(last_native_sound == 0xDF95FA70, "Disabling restoration must preserve Kitana's original selection");

    auto shang_bank = [&] {
        kitana_bank();
        kernel::W32(0x3200, 2);
        kernel::W32(0x3204, 0x415F052D);
        kernel::W32(0x3218, 0x13B9E084); kernel::W32(0x321C, 1);
        kernel::W32(0x3220, 0); kernel::W32(0x3224, 0xA0C975BF);
    };
    reset(); shang_bank(); kernel::W32(0x505970, 73); kernel::W16(0x5D38B6, 62);
    voice(0, 0x56E516CA, 0xAECADCD8);
    check(last_native_sound == 0x56E516CA && kernel::R32(0x6C03DC),
          "Shang recovery must preserve the original Shao Kahn announcer request");
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1 && sounds[0] == std::array<uint32_t, 3>{0x151610, 0, 0x415F052D},
          "Shang's original combat cue must recover a native taunt group after the announcer");
    kernel::W32(0x6C03DC, 0); voice(0, 0x56E516CA, 0xAECADCD8);
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 2 && sounds[1][2] == 0x13B9E084,
          "Both original localized Shang combat groups must be reachable");
    for (bool disabled : {false, true}) {
        reset(); shang_bank(); kernel::W32(0x505970, 73);
        kernel::W16(0x5D38B6, disabled ? 62 : 58); settings.restore_dialogue = !disabled;
        voice(0, 0x56E516CA, 0xAECADCD8); kernel::W32(0x6C03DC, 0); tick();
        check(sounds.empty(), "Shang taunts require the enhancement and Shang's own boss form");
    }
    reset(); shang_bank(); kernel::W32(0x505970, 73); kernel::W16(0x5D38B6, 62);
    voice(0, 0x56E516CA, 0xAECADCD8); kernel::W16(0x5D38B6, 58);
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "A pending Shang taunt must be cancelled when the boss changes form");
    reset(); shang_bank(); kernel::W32(0x505970, 73); kernel::W16(0x5D38B6, 62);
    kernel::W32(0x6C03DC, 1); voice(0, 0x56E516CA, 0xAECADCD8);
    voice(0, 0x56E516CA, 0xAECADCD8);
    kernel::W32(0x6C03DC, 0); tick(); kernel::W32(0x6C03DC, 0); tick();
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 2 && sounds[0][2] == 0x56E516CA && sounds[1][2] == 0x415F052D,
          "An announcer deferred by another voice must keep one ordered Shang response");

    reset(); activate(195, 0xD11A17FD, 65, 1, 69); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x9B1AFD52,
          "The authored Kitana trap activation must recover her captive threat");
    kernel::W32(0x6C03DC, 0); activate(195, 0xD11A17FD, 65, 1, 69); tick();
    check(sounds.size() == 1, "Kitana's captive threat must play only once per actor and room load");
    reset(); activate(195, 0xD11A17FD, 0, 1, 69); tick();
    check(sounds.empty(), "A player triggering the trap must not be mistaken for captive Kitana");
    reset(); settings.restore_dialogue = false; activate(195, 0xD11A17FD, 65, 1, 69); tick();
    check(sounds.empty(), "Disabling recovery must preserve the original trap without added speech");
    reset(); activate(195, 0xD11A17FD, 65, 1, 68); tick();
    check(sounds.empty(), "An unrelated trap ordinal must not recover Kitana's captive threat");
    reset(); kernel::W32(0x6C03DC, 1); activate(195, 0xD11A17FD, 65, 1, 69);
    play(0xAABBCCDD); finish(); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "Kitana's next cinematic must cancel an unplayed captive threat");

    reset(); kitana_bank(); kernel::W32(0x3200, 3);
    kernel::W32(0x3218, 0x3B6D9293); kernel::W32(0x321C, 1);
    kernel::W32(0x3220, 0); kernel::W32(0x3224, 0xD21BF725);
    kernel::W32(0x3228, 0x8109A438); kernel::W32(0x322C, 1);
    kernel::W32(0x3230, 0); kernel::W32(0x3234, 0x098D0330);
    kernel::W32(0x505970, 194); voice(0, 0xDF95FA70);
    kernel::W32(0x6C03DC, 0); voice(0, 0xDF95FA70);
    check(last_native_sound == 0x3B6D9293, "Kitana's laugh cues must also recover her medium variations");
    kernel::W32(0x6C03DC, 0); voice(0, 0xDF95FA70);
    check(last_native_sound == 0x8109A438, "Kitana's laugh cues must also recover her short variations");
    reset(); kitana_bank();kernel::W32(0x3204,0x8109A438);variation_seed=2;
    kernel::W32(0x505970,194);voice(0,0xDF95FA70);
    check(last_native_sound==0x8109A438,
          "Kitana's short group must be reachable on the first of her two authored laugh cues");

    reset(); kernel::W32(0x505970,136); voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0x526CF529 && last_native_bank==0x871111EC &&
          kernel::R32(0x2100)==0x93FDBC98 && kernel::R32(0x2104)==0xAECADCD8,
          "The acid puzzle's broken Liu hint must use his existing localized Forest Map 9 line");
    kernel::W32(0x6C03DC, 0); voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0, "The acid hint's authored 800-update recurrence must play only once per visit");
    kernel::W32(0x6C03DC, 0); kernel::W16(0x5D38B6,1); voice(0,0x8B431000,0xAECADCD8);
    check(last_native_sound==0xDF681048, "Each monk keeps his own acid hint in Ko-op");
    kernel::W32(0x6C03DC, 0); kernel::W16(0x5D38B6,0);
    kernel::W32(0x505970,137); tick(); kernel::W32(0x505970,136); tick();
    voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0x526CF529, "Re-entering Forest Map 9 must allow the acid hint again");
    reset(); kernel::W32(0x505970,136); kernel::W32(0x6C03DC,0x7000);
    voice(0,0x93FDBC98,0xAECADCD8); kernel::W32(0x6C03DC,0); tick(); tick();
    check(sounds.size()==1 && sounds[0][2]==0x526CF529, "A deferred acid hint must still play");
    kernel::W32(0x6C03DC,0); voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0, "A deferred acid hint that played must not repeat");
    reset(); kernel::W32(0x505970,136); kernel::W16(0x5D38B6,1);
    voice(0,0x8B431000,0xAECADCD8);
    check(last_native_sound==0xDF681048,
          "The acid puzzle's broken Kung hint must use his existing localized Forest Map 9 line");
    reset(); kernel::W32(0x505970,136); settings.restore_dialogue=false;
    voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0x93FDBC98, "Disabling recovery must preserve the original broken hint ID");
    reset(); kernel::W32(0x505970,136); kernel::W16(0x5D38B6,2);
    voice(0,0x93FDBC98,0xAECADCD8);
    check(last_native_sound==0x93FDBC98, "An acid hint must retain its exact original protagonist context");

    auto cage_wheel_bank=[&] { kitana_bank();kernel::W32(0x3204,0x3F5456F6); };
    reset(); cage_wheel_bank(); kernel::W32(0x505970,104);kernel::W16(0x5D38B6,71);
    kernel::W32(0x5D1AB8+0x1E04,12);voice(0,0xC3FF4851,0xAECADCD8);
    check(last_native_sound==0x3F5456F6,
          "Cage's wheel cue must recover his interrupted-wheel group while he is in a hit reaction");
    reset(); cage_wheel_bank();kernel::W32(0x505970,104);kernel::W16(0x5D38B6,71);
    voice(0,0xC3FF4851,0xAECADCD8);
    check(last_native_sound==0xC3FF4851, "An uninterrupted Cage wheel action must keep its original dialogue");
    reset(); cage_wheel_bank();kernel::W32(0x505970,104);kernel::W16(0x5D38B6,71);
    kernel::W32(0x5D1AB8+0x1E04,12);settings.restore_dialogue=false;
    voice(0,0xC3FF4851,0xAECADCD8);
    check(last_native_sound==0xC3FF4851, "Disabling recovery must preserve Cage's original wheel group");
    auto stream_bank = [&](uint32_t group, const std::vector<uint32_t>& choices) {
        kernel::W32(0x653BCC, 1); kernel::W32(0x653BD4, 0x3000);
        kernel::W32(0x3000, 0xAECADCD8); kernel::W32(0x3004, 0x3100);
        kernel::W32(0x300C, 0x3200); kernel::W32(0x3200, 1);
        kernel::W32(0x3204, group); kernel::W32(0x3208, uint32_t(choices.size()));
        kernel::W32(0x320C, 0);
        kernel::W32(0x3008, 0x4000); kernel::W32(0x3118, uint32_t(choices.size()));
        for (size_t i = 0; i < choices.size(); ++i) {
            kernel::W32(0x3210 + uint32_t(i) * 4, choices[i]);
            kernel::W32(0x4000 + uint32_t(i) * 28, choices[i]);
        }
    };
    auto stream = [&](uint32_t sound) {
        for (unsigned i=0;i<5;++i) kernel::W32(0x69654C+i*12,0);
        kernel::W32(0x1004, sound);
        X86Context ctx{}; ctx.esp = 0x1000;
        const auto before = streams.size();
        sub_0017C040(ctx, memory.data());
        check(streams.size() == before + 1 && ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1004,
              "A restored stream must preserve the original one-argument cdecl request and return registers");
        check(kernel::R32(0x1004) == sound, "A stream variation must restore the caller's original sound argument");
    };
    const std::vector<uint32_t> jade{0x8D15F4C2, 0xCAB58E12};
    const std::vector<uint32_t> mileena{0xDA6E38DD, 0x9DCE420D, 0xA0AE6BBD, 0x128EB7AD,
                                       0x2FEE9E1D, 0x684EE4CD, 0x552ECD7D, 0xD77E5AAC};
    reset(); stream_bank(0xB16C28BE, jade); kernel::W32(0x505970, 194);
    stream(0x8D15F4C2); stream(0x8D15F4C2); stream(0x8D15F4C2);
    check(streams == std::vector<uint32_t>{0x8D15F4C2, 0xCAB58E12, 0x8D15F4C2},
          "Jade's existing stream cue must reach both original localized variations without adding another voice");
    reset(); stream_bank(0xEED9A6D2, mileena); kernel::W32(0x505970, 195);
    kernel::W32(0x5ACEBC,0x3500);kernel::W32(0x3588,1);
    for (size_t i = 0; i < mileena.size(); ++i) stream(0xD77E5AAC);
    check(streams == mileena, "Mileena's existing stream cue must reach all eight original group members");
    for (bool known_mode : {false,true}) {
        reset();stream_bank(0xEED9A6D2,mileena);kernel::W32(0x505970,195);
        if (known_mode) kernel::W32(0x5ACEBC,0x3500);
        for (size_t i=0;i<mileena.size();++i) stream(0xD77E5AAC);
        check(std::find(streams.begin(),streams.end(),0x9DCE420D)==streams.end(),
              "Mileena's plural monks warning requires a known Ko-op mode, including when selected from her original stream group");
    }
    reset(); stream_bank(0xB16C28BE, jade); kernel::W32(0x505970, 194); variation_seed = 1;
    stream(0x8D15F4C2);
    check(streams.back() == 0xCAB58E12, "A single Jade cue must be able to select the otherwise unheard alternative");
    reset(); stream_bank(0xB16C28BE, jade); kernel::W32(0x505970, 194);
    settings.restore_dialogue = false; stream(0x8D15F4C2);
    check(streams.back() == 0x8D15F4C2, "Disabling restoration must retain the original direct stream");
    settings.restore_dialogue = true; stream(0x12345678);
    check(streams.back() == 0x12345678, "Unrelated streams must pass through without selection changes");
    kernel::W32(0x505970, 160); stream(0x8D15F4C2);
    check(streams.back() == 0x8D15F4C2, "A monastery stream variation must stay in its original battle rooms");
    reset(); kernel::W32(0x505970, 194); stream(0x8D15F4C2);
    check(streams.back() == 0x8D15F4C2, "An unloaded original variation group must preserve the original stream");
    reset(); stream_bank(0xB16C28BE, jade); kernel::W32(0x505970, 194);
    stream(0x8D15F4C2); kernel::W32(0x639224, 0x80020000); stream(0x8D15F4C2);
    check(streams == std::vector<uint32_t>{0x8D15F4C2, 0x8D15F4C2},
          "A replacement world must not inherit a previous encounter's stream selection cursor");
    reset(); stream_bank(0xB16C28BE,jade); kernel::W32(0x505970,194); variation_seed=1;
    accept_stream=false;stream(0x8D15F4C2);accept_stream=true;stream(0x8D15F4C2);
    check(streams==std::vector<uint32_t>{0xCAB58E12,0xCAB58E12},
          "A rejected native stream must leave the selected unused take available for the next real cue");
    reset(); stream_bank(0xB16C28BE,{0x8D15F4C2,0xCAB58E12,0xFB5D948F,0xC63DBD3F});
    kernel::W32(0x3208,2);kernel::W32(0x505970,195);
    for (unsigned i=0;i<4;++i)stream(0x8D15F4C2);
    check(streams==std::vector<uint32_t>{0x8D15F4C2,0xCAB58E12,0xFB5D948F,0xC63DBD3F},
          "Jade's original recovery cue must also reach her two matching unused defiant recovery takes");
    reset(); stream_bank(0xB16C28BE, jade); kernel::W32(0x505970, 194); variation_seed = 1;
    kernel::W32(0x3118, 1); stream(0x8D15F4C2);
    check(streams.back() == 0x8D15F4C2,
          "A variation whose direct definition is absent from the loaded locale must retain the original stream");

    struct CaptionFixture { uint32_t room, text, original, recovered; };
    for(const uint32_t host_text:{0xFFFFFD20u,0xFFFFFD00u,0xFFFFFD01u,0xFFFFFD02u,0xFFFFFD03u}){
        X86Context custom{};custom.esp=0x1000;custom.ecx=0x12345678;
        kernel::W32(custom.esp,0x86EE7);kernel::W32(custom.esp+4,host_text);
        caption_lookup_sound=0xDEADBEEF;
        sub_000111EF(custom,memory.data());
        check(custom.eax==0 && custom.ecx==0x12345678 && custom.esp==0x1004,
              "Host Survival rules and portal messages bypass the native voice array before its unbounded text lookup");
    }
    const CaptionFixture caption_fixtures[]{
        {101, 102, 0, 0x83D60341}, {108, 15, 0, 0x36E6C19B},
        {101, 14, 0x39D84601, 0x0C96F6E1}, {99, 11, 0x77ACBBDF, 0x73D3D82C},
        {99, 13, 0x43181561, 0xFC932D8C}, {142, 91, 0x24FB3EAC, 0x199B171C},
        {99, 98, 0xF5FC2C0E, 0x09138B4C}, {101, 103, 0xC58C67CF, 0xBEB62AF1},
        {137, 64, 0, 0xE555529C}, {46, 81, 0xAD4AC829, 0x902AE199},
        {100, 99, 0xC5B98BD2, 0xA5974565}, {128, 62, 0x901D2B23, 0xFF90543D},
    };
    auto caption_lookup = [&](uint32_t text, uint32_t caller = 0x86EE7) {
        kernel::W32(0x1000, caller); kernel::W32(0x1004, text);
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_000111EF(ctx, memory.data());
        check(ctx.ecx == 0x5678 && ctx.esp == 0x1004 && kernel::R32(0x1004) == text,
              "A recovered caption voice must preserve native lookup state, stack and text identity");
        return ctx.eax;
    };
    for (const auto& cue : caption_fixtures) {
        reset(); kernel::W32(0x505970, cue.room); variation_seed = 1;
        stream_bank(0x12345678, {cue.recovered}); caption_lookup_sound = cue.original;
        for (unsigned frame = 0; frame < 8; ++frame)
            check(caption_lookup(cue.text) == cue.recovered,
                  "An authored caption must reach its matching original recording and keep that choice across render frames");
        settings.restore_dialogue = false;
        check(caption_lookup(cue.text) == cue.original, "Disabling recovery must preserve the original caption voice lookup");
        settings.restore_dialogue = true;
        check(caption_lookup(cue.text, 0x12345678) == cue.original,
              "Journal and other audio queries must not inherit the native caption renderer's recovered voice");
        check(caption_lookup(cue.text + 1) == cue.original, "An unrelated caption must retain its own voice mapping");
        kernel::W32(0x505970, 1);
        check(caption_lookup(cue.text) == cue.original, "Recovered captions must stay in their exact original room");
        kernel::W32(0x505970, cue.room); kernel::W32(0x3118, 0);
        check(caption_lookup(cue.text) == cue.original,
              "A recording unavailable in the loaded locale must preserve the original caption mapping");
        kernel::W32(0x3118, 1); caption_lookup_sound = 0x12345678;
        check(caption_lookup(cue.text) == 0x12345678,
              "An unexpected existing caption mapping must not be replaced by a recovery inferred for another source");
    }
    auto definitions = [&](const std::vector<uint32_t>& ids) {
        kernel::W32(0x653BCC, 1); kernel::W32(0x653BD4, 0x3000);
        kernel::W32(0x3000, 0xAECADCD8); kernel::W32(0x3004, 0x3100);
        kernel::W32(0x3008, 0x4000); kernel::W32(0x3118, uint32_t(ids.size()));
        for (size_t i = 0; i < ids.size(); ++i) kernel::W32(0x4000 + uint32_t(i) * 28, ids[i]);
    };
    // The monk's escape warning plays as the first gate opens (_d_door05up/_d_door05_b), not on quake hits.
    struct EscapeFixture { uint32_t first, second, solo, koop; int16_t actor; };
    const EscapeFixture escape[] = {
        {0x983DC662, 0x9F8C2AF9, 0xBEC5353E, 0x0CE5E92E, 0},
        {0x29AEB768, 0xB7CA22CB, 0xEA53782A, 0xADF302FA, 1},
    };
    for (const auto& cue : escape) for (uint32_t mode : {0u, 1u}) {
        const uint32_t expected = mode ? cue.koop : cue.solo;
        reset(); definitions({expected}); kernel::W32(0x5ACEBC, 0x3400); kernel::W32(0x3488, mode);
        activate(96, cue.first, cue.actor); kernel::W32(0x6C03DC, 0); tick();
        check(sounds.empty(), "A quake hit must no longer carry the escape warning");
        activate(96, 0x9406D458, cue.actor, 1, 142); tick();
        check(sounds.empty(), "The gate warning waits a beat after the gate starts opening");
        std::this_thread::sleep_for(std::chrono::milliseconds(1600)); tick();
        check(sounds.size() == 1 && sounds[0][2] == expected,
              "The first gate must carry the protagonist's solo or Ko-op escape warning");
        kernel::W32(0x6C03DC, 0); activate(96, 0x9406D458, cue.actor, 1, 142); tick(); tick();
        check(sounds.size() == 1, "The gate warning plays once per room load");
        reset(); definitions({expected}); kernel::W32(0x5ACEBC, 0x3400); kernel::W32(0x3488, mode);
        activate(96, 0x9406D458, cue.actor, 0, 142); activate(98, 0x9406D458, cue.actor, 1, 142); tick();
        check(sounds.empty(), "Only the first gate's activation in Wu Shi Map 01 carries the warning");
        reset(); definitions({expected}); kernel::W32(0x5ACEBC, 0x3400); settings.restore_dialogue = false;
        activate(96, 0x9406D458, cue.actor, 1, 142); tick();
        check(sounds.empty(), "Disabling recovery leaves the gate unchanged");
    }
    reset(); definitions({0xBEC5353E}); kernel::W32(0x5ACEBC, 0x3400);
    activate(96, 0x9A955116, 0, 1, 33); tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1600)); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0xBEC5353E, "Either gate opening carries the warning");
    // Tombs Map 31 has a connected text-58 event and two unused original
    // entrance takes. The chosen voice follows the actual lead character.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t expected = hero == 0 ? 0xB8867E92u : 0x82BE2BB9u;
        reset(); definitions({0xB8867E92,0x82BE2BB9});
        kernel::W32(0x505970,30); kernel::W16(0x5D38B6,hero);
        for (unsigned frame=0; frame<6; ++frame)
            check(caption_lookup(58)==expected,
                  "The original Soul Tombs entrance caption must select the first player's existing entrance line");
        check(caption_lookup(59)==0,"An unrelated caption must not inherit the Soul Tombs voice");
        check(caption_lookup(58,0x12345678)==0,"A non-renderer caller must not acquire the entrance voice");
        settings.restore_dialogue=false;
        check(caption_lookup(58)==0,"Disabled dialogue recovery must leave the original Tombs caption silent");
        settings.restore_dialogue=true;
        kernel::W32(0x505970,15);
        check(caption_lookup(58)==0,"The Tombs entrance voice must not appear in another room");
        kernel::W32(0x505970,30); kernel::W32(0x3118,0);
        check(caption_lookup(58)==0,"An absent bank definition must preserve the silent caption");
        definitions({0xB8867E92,0x82BE2BB9}); caption_lookup_sound=0x12345678;
        check(caption_lookup(58)==0x12345678,
              "An already voiced original caption must retain its original playback mapping");
    }
    reset(); definitions({0xB8867E92,0x82BE2BB9}); kernel::W32(0x505970,30);
    kernel::W16(0x5D38B6,69);
    check(caption_lookup(58)==0,"An unrecognized lead actor must not voice the Soul Tombs entrance");
    // Evil Monastery Map 1 has an original unvoiced Wall Climb hint (text237)
    // reached by _d_txt_help; the matching hero's 1_04 recording can voice it.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t expected = hero == 0 ? 0xADA2A592u : 0x20A640F3u;
        reset(); definitions({0xADA2A592,0x20A640F3});
        kernel::W32(0x505970,192); kernel::W16(0x5D38B6,hero);
        for (unsigned frame=0; frame<6; ++frame)
            check(caption_lookup(237)==expected,
                  "The original Wall Climb hint must use the selected monk's matching unused recording");
        check(caption_lookup(238)==0,"Adjacent texts must not inherit the Wall Climb voice");
        check(caption_lookup(237,0x12345678)==0,"Only the caption renderer may recover Wall Climb speech");
        settings.restore_dialogue=false;
        check(caption_lookup(237)==0,"Disabling restoration must leave the original Wall Climb hint silent");
        settings.restore_dialogue=true; kernel::W32(0x505970,194);
        check(caption_lookup(237)==0,"The Wall Climb voice must remain in the authored Map 1 room");
        kernel::W32(0x505970,192); kernel::W32(0x3118,0);
        check(caption_lookup(237)==0,"Unavailable original voice definitions must leave the hint silent");
        definitions({0xADA2A592,0x20A640F3}); caption_lookup_sound=0x12345678;
        check(caption_lookup(237)==0x12345678,"Existing original caption audio must never be overwritten");
    }
    reset(); definitions({0xADA2A592,0x20A640F3}); kernel::W32(0x505970,192);
    kernel::W16(0x5D38B6,69);
    check(caption_lookup(237)==0,"An unrelated lead actor must not acquire the Wall Climb recording");
    struct VoiceFamilyFixture { uint32_t room, source; int16_t actor; bool coop; std::vector<uint32_t> takes; };
    const VoiceFamilyFixture families[]{
        {104, 0xC3FF4851, 71, false, {0x34179BEB,0xBB576E4B}},
        {72, 0x898DD582, 54, true, {0x7D0C8867,0x3AACF2B7,0x1EDCBD5D,0x597CC78D,0x641CEE3D}},
        {72, 0x898DD582, 54, false, {0x7D0C8867,0x3AACF2B7,0xDB7B83D3,0x9CDBF903,0x1EDCBD5D,0x597CC78D,0x641CEE3D}},
        {144, 0x1569801B, 59, false, {0xE8D4E56F,0xAF749FBF,0xD4A5AA7A,0x9305D0AA,0x1C45250A,
            0x8B04D055,0xCCA4AA85,0xF1C48335,0xBB749B94,0x4FE2748B,0x72825D3B}},
        {132, 0x2DB6EB65, 66, true, {0xBEAE2A44,0xF90E5094}},
        {160, 0xB9E947A4, 52, true, {0xBDBB26C9,0x784BCBC8,0xD32CA20D,0x948CD8DD,0xA9ECF16D,0x2763861E,0x60C3FCCE,
            0x0F9BFAD9,0x452BE278,0x9C60D6C1}},
        {160, 0xB9E947A4, 55, false, {0xBDBB26C9,0x784BCBC8,0xA9ECF16D,0x2763861E,0x60C3FCCE,
            0x0F9BFAD9,0x452BE278,0x9C60D6C1}},
        {15, 0x457D44DD, 0, false, {0x02DD3E0D}},
        {15, 0x280C26F7, 1, false, {0x6FAC5C27}},
        {73, 0x56E516CA, 62, true, {0x584D7D7D,0xC4F60FC3,0xEA6DA16D,0xD9F33F7B}},
    };
    for (const auto& family : families) {
        reset(); definitions(family.takes); kernel::W32(0x505970, family.room);
        kernel::W32(0x5ACEBC, 0x3500); kernel::W32(0x3588, family.coop ? 1 : 0);
        kernel::W16(0x5D38B6, family.actor);
        // Kano's attack complaints require the protagonist's hit-reaction state.
        kernel::W16(0x5D38B6 + 0x3238, 0); kernel::W32(0x5D1AB8 + 0x3238 + 0x1E04, 12);
        std::vector<uint32_t> seen;
        for (size_t n = 0; n < (family.takes.size() + 1) * 3; ++n) {
            kernel::W32(0x6C03DC, 0); voice(0, family.source, family.actor < 2 ? 0x871111EC : 0xAECADCD8);
            seen.push_back(last_native_sound);
            check(kernel::R32(0x2100) == family.source, "A contextual voice must restore its original script payload");
            if (last_native_sound != family.source)
                check(last_native_bank == 0x871111EC, "A selected direct leaf must use direct lookup rather than the variation-only bank API");
        }
        for (const auto wanted : family.takes)
            check(std::find(seen.begin(), seen.end(), wanted) != seen.end(),
                  "Every semantically matched contextual take must be reachable from the existing native request");
        check(std::find(seen.begin(), seen.end(), family.source) != seen.end(),
              "Contextual variation must keep the original native request reachable");
        settings.restore_dialogue = false; kernel::W32(0x6C03DC, 0); voice(0, family.source);
        check(last_native_sound == family.source, "Disabling restoration must retain every original scripted family");
    }
    // Baraka's phase1/2 07 taunt stays retail; only native phase3 06_1 may queue a
    // single supplemental reaction.
    const std::vector<uint32_t> baraka_reactions{0xF26D1CDF,0xB5CD660F,0x88AD4FBF,
        0x37CA2251,0x706A5881,0x4D0A7131,0x91BD29E5,0xD61D5335,0xEB7D7A85};
    for (const auto seed : {0u, 1u, 1234u}) {
        reset();definitions(baraka_reactions);kernel::W32(0x505970,15);
        kernel::W16(0x5D38B6,56);kernel::W32(0x5D1AB8+0x68,0xABCD);
        kernel::W32(0x5ACEBC,0x3500);kernel::W32(0x3588,0);variation_seed=seed;
        for (int k=0;k<9;++k) {kernel::W32(0x6C03DC,0);voice(0,0x66FE4E8E,0xAECADCD8);tick();}
        check(last_native_sound==0x66FE4E8E && sounds.empty(),
              "Baraka's early 07 taunt must never randomly become a restored laugh");
        kernel::W32(0x6C03DC,0);voice(0,0xA0553378,0xAECADCD8);
        check(last_native_sound==0xA0553378 && sounds.empty(),
              "Original phase3 Baraka speech must finish before its optional late reaction");
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==1 &&
              std::find(baraka_reactions.begin(),baraka_reactions.end(),sounds[0][2])!=baraka_reactions.end(),
              "An actual phase3 Baraka speech may yield exactly one late reaction");
        kernel::W32(0x6C03DC,0);voice(0,0xA0553378,0xAECADCD8);
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==1,"Baraka's recovered phase3 reaction may not form a laughter loop");
    }
    for (bool coop : {false,true}) {
        reset();definitions({0x9F25265E,0xA2450FEE,0xF26D1CDF});
        kernel::W32(0x505970,15);kernel::W16(0x5D38B6,56);
        kernel::W32(0x5D1AB8+0x68,0xBABA);kernel::W32(0x5ACEBC,0x3500);
        kernel::W32(0x3588,coop?1:0);variation_seed=1;
        voice(0,0xD8855C8E,0x871111EC);
        check(last_native_sound==0xD8855C8E,
              "Baraka's authored cooperative speech 08_1 must be preserved instead of replaced");
        kernel::W32(0x6C03DC,0);tick();
        check(coop?(sounds.size()==1 && sounds[0][2]==0xA2450FEE):sounds.empty(),
              "An alternative Baraka 08 speech may follow original 08_1 only in co-op");
        kernel::W32(0x6C03DC,0);voice(0,0xD8855C8E,0x871111EC);
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==(coop?1u:0u),
              "Co-op original 08 speech may not repeatedly add the same optional line");
        kernel::W32(0x6C03DC,0);voice(0,0xA0553378,0xAECADCD8);
        kernel::W32(0x6C03DC,0);tick();
        if (!coop) {
            check(sounds.size()==1 && sounds[0][2]==0xA2450FEE,
                  "Solo Baraka must speak an 08 alternative before the late reaction");
            kernel::W32(0x6C03DC,0);tick();
        }
        check(sounds.size()==2 && sounds.back()[2]==0xF26D1CDF,
              "After authentic phase3 speech, co-op and solo Baraka each allow one late reaction");
    }
    // Solo never gets retail 08_1: a real phase3 06 request reaches both approved 08
    // alternatives, then its late reaction plays last.
    for (const uint32_t seed : {0u, 1u}) {
        reset();definitions({0x9F25265E,0xA2450FEE,0xF26D1CDF});
        kernel::W32(0x505970,15);kernel::W16(0x5D38B6,56);
        kernel::W32(0x5D1AB8+0x68,0xBABA);
        kernel::W32(0x5ACEBC,0x3500);kernel::W32(0x3588,0);variation_seed=seed;
        voice(0,0x66FE4E8E,0xAECADCD8);
        check(last_native_sound==0x66FE4E8E && sounds.empty(),
              "Early solo Baraka taunts must never trigger his late spoken 08 recordings");
        kernel::W32(0x6C03DC,0);voice(0,0xA0553378,0xAECADCD8);
        check(last_native_sound==0xA0553378 && sounds.empty(),
              "Solo Baraka must speak original phase3 06 before the new 08 alternatives");
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==1 && sounds[0][2]==(seed?0xA2450FEEu:0x9F25265Eu),
              "Solo Baraka must be able to select BOTH 08_2 and 08_3 across encounters");
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==2 && sounds[1][2]==0xF26D1CDF,
              "Solo Baraka's optional laugh must only come after the added spoken 08");
        kernel::W32(0x6C03DC,0);voice(0,0xA0553378,0xAECADCD8);
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==2,
              "Repeating phase3 cannot loop either solo Baraka speech or reaction");
    }
    // A missing take falls back to the other loaded one; with neither, keep the
    // original phase3 voice and the later reaction.
    reset();definitions({0xA2450FEE,0xF26D1CDF});
    kernel::W32(0x505970,15);kernel::W16(0x5D38B6,56);
    kernel::W32(0x5D1AB8+0x68,0xBABA);kernel::W32(0x5ACEBC,0x3500);
    kernel::W32(0x3588,0);variation_seed=0;
    voice(0,0xA0553378,0xAECADCD8);
    kernel::W32(0x6C03DC,0);tick();
    check(sounds.size()==1 && sounds[0][2]==0xA2450FEE,
          "Missing Baraka 08_2 must allow the loaded 08_3 in solo without forcing absent audio");
    reset();definitions({0xF26D1CDF});kernel::W32(0x505970,15);
    kernel::W16(0x5D38B6,56);kernel::W32(0x5D1AB8+0x68,0xBABA);
    kernel::W32(0x5ACEBC,0x3500);kernel::W32(0x3588,1);
    voice(0,0xA0553378,0xAECADCD8);kernel::W32(0x6C03DC,0);tick();
    check(sounds.empty(),"A cooperative fight must not laugh before a real 08 speech request");
    reset();definitions({0xF26D1CDF});kernel::W32(0x505970,15);
    kernel::W16(0x5D38B6,56);kernel::W32(0x5D1AB8+0x68,0xBABA);
    kernel::W32(0x5ACEBC,0x3500);settings.restore_dialogue=false;
    voice(0,0xA0553378,0xAECADCD8);kernel::W32(0x6C03DC,0);tick();
    check(sounds.empty(),"Disabling dialogue recovery preserves original Baraka only");
    // LF Map 5's two Mileena 5_03 takes address both monks and never replace 5_02, her
    // entrance or other actors' and rooms' taunts.
    for (int scenario = 0; scenario < 5; ++scenario) {
        reset(); definitions({0xBEAE2A44,0xF90E5094});
        kernel::W32(0x505970, scenario == 2 ? 133 : 132);
        kernel::W32(0x5ACEBC,0x3500); kernel::W32(0x3588,scenario == 0 ? 1 : 0);
        kernel::W16(0x5D38B6,scenario == 3 ? 67 : 66);
        if (scenario == 4) settings.restore_dialogue = false;
        voice(0,scenario == 1 ? 0xD38D1909 : 0x2DB6EB65,0xAECADCD8);
        check(last_native_sound == (scenario == 1 ? 0xD38D1909u : 0x2DB6EB65u),
              "Mileena duo-only takes must not replace solo, entrance, wrong-room or disabled voices");
    }
    reset(); definitions({0xFF90543D,0xC2F07D8D}); kernel::W32(0x505970,128);
    caption_lookup_sound=0x901D2B23;
    check(caption_lookup(62)==0xFF90543D,
          "A known unresolved original caption ID must use a valid forest hint even with random seed zero");
    reset(); definitions({0xFF90543D,0xC2F07D8D}); kernel::W32(0x505970,128);
    caption_lookup_sound=0x901D2B23; variation_seed=1;
    check(caption_lookup(62)==0xC2F07D8D,
          "Both original forest-craving hints must be reachable at the repaired original caption");
    // Respond to the authentic Forest Map 1 tree hint only AFTER its Raiden
    // caption starts a native voice; neither lookup nor an unvoiced hint is a cue.
    auto forest_caption = [&](uint32_t text, uint32_t sound, uint32_t handle) {
        kernel::G<uint8_t>(0x4E74E0) = 1;
        kernel::W32(0x4E74F8, text);
        kernel::W32(0x4E74E4, sound);
        kernel::W32(0x4E74E8, handle);
    };
    // Evil Monastery Map 4 Fist of Ruin tutorial (text100): Liu's Kitana remark may follow,
    // never interrupt it or play in another room.
    for (int scenario=0;scenario<6;++scenario) {
        reset();definitions(scenario==4 ? std::vector<uint32_t>{} :
                            std::vector<uint32_t>{0xA6A85EC8});
        kernel::W32(0x505970,scenario==1 ? 194 : 195);
        kernel::W16(0x5D38B6,scenario==2 ? 1 : 0);
        if(scenario==5) settings.restore_dialogue=false;
        forest_caption(scenario==3 ? 99 : 100,0x12345678,0x5000);
        tick();check(sounds.empty(),"The Fist of Ruin popup must not be interrupted by Liu's Kitana comment");
        kernel::G<uint8_t>(0x4E74E0)=0;tick();
        const bool expected=scenario==0;
        check((sounds.size()==1)==expected &&
              (!expected || sounds[0][2]==0xA6A85EC8),
              "Liu's EM5_02 may play only after real Fist popup closure, with Liu and same native bank");
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.size()==(expected ? 1u:0u),"Fist of Ruin follow-up must never repeat");
    }
    reset();definitions({0xA6A85EC8});kernel::W32(0x505970,195);
    kernel::W16(0x5D38B6,0);forest_caption(100,0x11EB4F7C,0);
    tick();kernel::G<uint8_t>(0x4E74E0)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0xA6A85EC8,
          "Fist of Ruin type6 popup can be a visible message without a normal caption audio handle");
    // Foundry Map 11 background warriors (_d_bgchar01) have no native sound request: one
    // guarded editorial line after the room settles.
    for (const auto hero : {int16_t(0),int16_t(1)}) {
        reset();definitions({0xA09578E5,0x7B43C051});
        kernel::W32(0x505970,74);kernel::W16(0x5D38B6,hero);
        for (unsigned i=0;i<89;++i) tick();
        check(sounds.empty(),"Foundry spectator comment must wait for the area to finish settling");
        tick();
        check(sounds.size()==1 && sounds.back()[2]==(hero==0 ? 0xA09578E5u : 0x7B43C051u),
              "Foundry Map11 spectator dialogue must match the active hero's native bank line");
        kernel::W32(0x6C03DC,0);for (unsigned i=0;i<8;++i) tick();
        check(sounds.size()==1,"Foundry Map11 commentary must play once only");
    }
    for (int scenario=0;scenario<4;++scenario) {
        reset();definitions(scenario==2?std::vector<uint32_t>{}:
                            std::vector<uint32_t>{0xA09578E5});
        kernel::W32(0x505970,scenario==1?73:74);
        kernel::W16(0x5D38B6,scenario==3?57:0);
        if(scenario==0)settings.restore_dialogue=false;
        for(unsigned i=0;i<95;++i)tick();
        check(sounds.empty(),"Foundry spectator remarks must not play in wrong room, without bank/hero or disabled recovery");
    }
    // Forest Map 10 = room 137 (debug 138 is a display index). _d_snakes (91FD0438/19) starts
    // the emergence; entering or prebuffering E8F451C7 proves nothing.
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W16(0x5D38B6,1);tick();
    check(sounds.empty(),"Forest snake guidance must not play on map entry");
    activate(137,0x91FD0438u,1,1,19);tick();
    check(sounds.empty(),"Snake emergence trigger alone does not prove the source scene finished");
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);tick();
    check(sounds.empty(),"Buffered snake scene must not permit an early Raiden voice");
    kernel::W32(0x696550,0);kernel::W32(0x696554,0x5000);
    native_stream_playing=true;tick();
    check(sounds.empty(),"Raiden cannot interrupt the original snake emergence scene");
    native_stream_playing=false;kernel::G<uint8_t>(0x514505)=1;tick();
    check(sounds.empty(),"Raiden waits even after stream ends while cinematic flags remain active");
    kernel::G<uint8_t>(0x514505)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "Raiden LF10_01_3 may speak only after the actual snake scene audio finishes");
    kernel::W32(0x6C03DC,0);tick();
    check(sounds.size()==1,"Raiden snake hint must never repeat after delivery");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696554,0x5000);
    native_stream_playing=true;tick();native_stream_playing=false;tick();
    check(sounds.empty(),"Snake audio without the real _d_snakes trigger is insufficient");
    reset();definitions({0x919A865F});activate(137,0x91FD0438u,1,1,19);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696554,0x5000);
    native_stream_playing=true;tick();kernel::W32(0x505970,136);
    native_stream_playing=false;tick();
    check(sounds.empty(),"Leaving Forest Map10 before the scene finishes cancels its bonus Raiden voice");
    kernel::W32(0x505970,137);tick();
    check(sounds.empty(),"Returning to Forest Map10 must not inherit a prior snake trigger");
    for (const auto room : {136u, 138u}) {
        reset();definitions({0x919A865F});activate(room,0x91FD0438u,1,1,19);
        kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696554,0x5000);
        native_stream_playing=true;tick();native_stream_playing=false;tick();
        check(sounds.empty(),"Debug map 138 and adjacent native rooms cannot trigger Forest Map10 dialogue");
    }
    for (unsigned scenario=0;scenario<5;++scenario) {
        reset();definitions(scenario==3?std::vector<uint32_t>{}:
                            std::vector<uint32_t>{0x919A865F});
        if (scenario==4)settings.restore_dialogue=false;
        activate(137,scenario==0?0x91FD0439u:0x91FD0438u,1,
                 scenario==2?0:1,scenario==1?18:19);
        kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696554,0x5000);
        native_stream_playing=true;tick();native_stream_playing=false;tick();
        check(sounds.empty(),"Wrong original activation, absent bank or disabled restoration must not add Raiden");
    }
    reset();definitions({0x919A865F});activate(137,0x91FD0438u,1,1,19);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696554,0x5000);
    native_stream_playing=true;tick();native_stream_playing=false;
    kernel::G<uint8_t>(0x4E74E0)=1;tick();
    check(sounds.empty(),"Raiden must not overlap a remaining original caption");
    kernel::G<uint8_t>(0x4E74E0)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "Raiden can play once when the native snake stream and caption are finished");
    // Retail may show the snake camera yet reject its stream: a real _d_snakes activation plus
    // the cinematic edge delivers Raiden after the scene, never during it.
    reset();definitions({0x919A865F});activate(137,0x91FD0438u,1,1,19);
    kernel::G<uint8_t>(0x6C4198)=1;tick();
    check(sounds.empty(),"A visual snake cinematic in progress cannot be interrupted by Raiden");
    kernel::G<uint8_t>(0x6C4198)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "After genuine snake cinematic completion, Raiden may speak even if original retail stream never played");
    kernel::W32(0x6C03DC,0);tick();
    check(sounds.size()==1,"Visual fallback must never play Raiden twice");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::G<uint8_t>(0x6C4198)=1;tick();
    kernel::G<uint8_t>(0x6C4198)=0;tick();
    check(sounds.empty(),"An unrelated cinematic without original snake activation must not request Raiden");
    // Independent source: the 17C0B0 PLAY callback accepting E8F451C7, even when
    // activatetrigger 170330 bypassed our wrapper.
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    original_stream_event(0xE8F451C7);tick();
    check(sounds.empty(),"A native script audiostreamplay request alone cannot prove Raiden's scene has played");
    kernel::G<uint8_t>(0x6C4198)=1;tick();
    check(sounds.empty(),"The original snake script sound event cannot interrupt its ongoing visual cinematic");
    kernel::G<uint8_t>(0x6C4198)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "The real _d_snakes snd_cut event plus completed cinematic must work even if the indirect activation and soundtrack PLAY are absent");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    original_stream_event(0x50329483);
    kernel::G<uint8_t>(0x6C4198)=1;tick();kernel::G<uint8_t>(0x6C4198)=0;tick();
    check(sounds.empty(),"An unrelated native stream script event cannot arm the Forest snake cue");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
    original_stream_play(0xE8F451C7);tick();
    check(sounds.empty(),"Accepted native snake PLAY must wait for proof that sound truly started");
    native_stream_playing=true;tick();
    check(sounds.empty(),"Native snake audio must not be interrupted even without an activation hook");
    // Retail may erase the original tag immediately while the native sound
    // continues. The PLAY hook's saved handle must survive this cleanup.
    kernel::W32(0x69654C,0);kernel::W32(0x696554,0);
    tick();
    check(sounds.empty(),"Clearing a retail stream tag must not allow Raiden over active snake audio");
    native_stream_playing=false;kernel::G<uint8_t>(0x6C4198)=1;tick();
    check(sounds.empty(),"Native camera/cinematic still blocks Raiden after original stream finishes");
    kernel::G<uint8_t>(0x6C4198)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "Authentic original snake stream PLAY followed by end must release Raiden even without activatetrigger");
    kernel::W32(0x6C03DC,0);tick();
    check(sounds.size()==1,"The authentic original PLAY can only yield one Raiden line");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
    original_stream_play(0xE8F451C7);
    native_stream_playing=true;tick();native_stream_playing=false;
    // Another future cinematic is prebuffered in a DIFFERENT native slot;
    // it is not playing and must not suppress Raiden after snake audio ends.
    kernel::W32(0x696558,0x12345678);kernel::W32(0x69655C,0x6000);
    tick();
    check(sounds.size()==1 && sounds.back()[2]==0x919A865F,
          "A separate future prebuffer must not permanently suppress Raiden after the snake stream ends");
    // Neither merely buffering a room-load sample nor a rejected PLAY request
    // authorizes Raiden, even when the visual room is correct.
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
    tick();native_stream_playing=true;tick();native_stream_playing=false;tick();
    check(sounds.empty(),"Room-load prebuffer alone cannot arm the Raiden cue");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    original_stream_play(0xE8F451C7);
    native_stream_playing=true;tick();native_stream_playing=false;tick();
    check(sounds.empty(),"An original PLAY without a buffered native handle is not proof of a real stream");
    for (const auto room : {136u,138u}) {
        reset();definitions({0x919A865F});kernel::W32(0x505970,room);
        kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
        original_stream_play(0xE8F451C7);
        native_stream_playing=true;tick();native_stream_playing=false;tick();
        check(sounds.empty(),"Original stream PLAY from an unrelated room cannot queue Forest Map10 Raiden");
    }
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
    original_stream_play(0xE8F451C7);native_stream_playing=true;tick();
    kernel::W32(0x505970,136);native_stream_playing=false;tick();
    kernel::W32(0x505970,137);tick();
    check(sounds.empty(),"Leaving and reentering must clear accepted snake PLAY instance and native handle");
    reset();definitions({0x919A865F});kernel::W32(0x505970,137);
    settings.restore_dialogue=false;
    kernel::W32(0x69654C,0xE8F451C7);kernel::W32(0x696550,0x5000);
    original_stream_play(0xE8F451C7);
    native_stream_playing=true;tick();native_stream_playing=false;tick();
    check(sounds.empty(),"Disabling restoration must not arm optional Raiden via original stream PLAY");
    for (const auto hero : {int16_t(0), int16_t(1)}) for (uint32_t take = 0; take < 2; ++take) {
        const uint32_t reply = hero == 0 ? 0x33A9328Cu : 0xC46D848Du;
        reset(); definitions({0x33A9328C, 0xC46D848D, 0xFF90543D, 0xC2F07D8D}); kernel::W32(0x505970, 128);
        kernel::W16(0x5D38B6, hero); variation_seed = take;
        caption_lookup_sound = 0x901D2B23;
        const auto raiden = caption_lookup(62);
        check(raiden == (take ? 0xC2F07D8Du : 0xFF90543Du),
              "The Forest follow-up must not change the native Raiden voice mapping");
        tick();
        check(sounds.empty(), "Rendering a forest hint alone must not trigger a new response");
        forest_caption(62, raiden, 0); tick();
        check(sounds.empty(), "A forest hint with no confirmed native handle cannot trigger a response");
        forest_caption(62, raiden, 0x5000); tick();
        check(sounds.empty(), "The monk must not interrupt Raiden's original forest hint");
        kernel::W32(0x4E74E8, 0); tick();
        check(sounds.empty(), "The monk must still wait while Raiden's caption is pending");
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.size() == 1 && sounds[0][2] == reply,
              "The correct monk must answer after Raiden's voice ends, without replacing it");
        kernel::W32(0x6C03DC, 0); forest_caption(62, raiden, 0x5000); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.size() == 1, "Reopened forest hints must not repeat a monk's reply in the same room load");
        kernel::W32(0x505970, 129); tick();
        kernel::W32(0x505970, 128); forest_caption(62, raiden, 0x5000); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.size() == 1, "Re-entering the Forest Map 1 room must not repeat an addition already heard");
        ++checkpoint_restores; tick();
        kernel::W32(0x6C03DC, 0); forest_caption(62, raiden, 0x5000); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.size() == 2 && sounds.back()[2] == reply,
              "After a death/retry checkpoint restore the hint response plays again");
    }
    for (uint32_t scenario = 0; scenario < 7; ++scenario) {
        reset(); definitions({0x33A9328C, 0xC46D848D}); kernel::W32(0x505970, 128);
        kernel::W16(0x5D38B6, 0);
        forest_caption(62, 0xFF90543D, 0x5000);
        if (scenario == 0) settings.restore_dialogue = false;
        if (scenario == 1) kernel::W32(0x505970, 129);
        if (scenario == 2) kernel::W32(0x4E74F8, 63);
        if (scenario == 3) kernel::W32(0x4E74E4, 0x12345678);
        if (scenario == 4) kernel::W16(0x5D38B6, 69);
        if (scenario == 5) definitions({});
        if (scenario == 6) kernel::W32(0x639224, 0);
        tick(); kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "Forest recovery must reject disabled, wrong-context or unavailable replies");
    }
    reset(); definitions({0x33A9328C}); kernel::W32(0x505970, 128);
    kernel::W16(0x5D38B6, 0); forest_caption(62, 0x901D2B23, 0x5000);
    tick(); kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0x33A9328C,
          "An already playable original Forest hint must also retain its voice before the reply");

    // Map 1's original post-stream title text54 is silent. Raiden's stage
    // exposition follows only a caption that the native UI actually displayed.
    reset(); definitions({0xD64BB4B8}); kernel::W32(0x505970, 192);
    forest_caption(54, 0, 0); tick();
    check(sounds.empty(), "Raiden's Monastery introduction must not overlap its native title");
    kernel::G<uint8_t>(0x4E74E0) = 0; kernel::G<uint8_t>(0x514505) = 1; tick();
    check(sounds.empty(), "Raiden must wait for all native introductory streams");
    kernel::G<uint8_t>(0x514505) = 0; kernel::W32(0x6C03DC, 0x5000); tick();
    check(sounds.empty(), "Raiden must not interrupt another active native voice");
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0xD64BB4B8u,
          "Raiden must explain the Shadow Priest portals only after the Monastery introduction");
    kernel::W32(0x6C03DC, 0); forest_caption(54, 0, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1, "Reopened Monastery titles must not repeat Raiden's scene explanation");
    for (uint32_t gate : {0x696550u, 0x69655Cu, 0x6C0404u, 0x5146E4u, 0x4E751Cu}) {
        reset(); definitions({0xD64BB4B8}); kernel::W32(0x505970, 192);
        forest_caption(54, 0, 0); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; kernel::W32(gate, 1); tick();
        check(sounds.empty(), "The added Raiden speech must never cover an active native stream or transition");
        kernel::W32(gate, 0); tick();
        check(sounds.size() == 1 && sounds.back()[2] == 0xD64BB4B8,
              "The contextual line must resume when the separate native audio gate clears");
    }
    reset(); definitions({0xD64BB4B8}); kernel::W32(0x505970, 192);
    forest_caption(54, 0, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; paused = true; tick();
    check(sounds.empty(), "Raiden's new story line must not start during a paused game");
    paused = false; tick();
    check(sounds.size() == 1, "The queued Raiden explanation must remain available after pause");
    for (uint32_t scenario = 0; scenario < 6; ++scenario) {
        reset(); definitions({0xD64BB4B8}); kernel::W32(0x505970, 192);
        forest_caption(54, 0, 0);
        if (scenario == 0) settings.restore_dialogue = false;
        if (scenario == 1) kernel::W32(0x505970, 193);
        if (scenario == 2) kernel::W32(0x4E74F8, 115);
        if (scenario == 3) kernel::W32(0x639224, 0);
        if (scenario == 4) definitions({});
        if (scenario == 5) kernel::W32(0x4E74F8, 237);
        tick(); kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "Raiden's Monastery exposition must require the true title and loaded source");
    }
    // Living Forest's exact authored title runs after its original intro.
    // The available Liu Kang opening thought cannot be assigned to Kung Lao.
    reset(); definitions({0xC629944C}); kernel::W32(0x505970, 128);
    kernel::W16(0x5D38B6, 0); forest_caption(55, 0, 0); tick();
    check(sounds.empty(), "Liu's forest entrance line cannot replace the original area title");
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0xC629944C,
          "Liu Kang should speak after the authentic first forest area title");
    kernel::W32(0x6C03DC, 0); forest_caption(55, 0, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1, "The forest entrance thought must play once per room visit");
    reset(); definitions({0xC629944C}); kernel::W32(0x505970, 128);
    kernel::W16(0x5D38B6, 1); forest_caption(55, 0, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.empty(), "Kung Lao must not receive Liu Kang's forest entrance voice");
    reset(); definitions({0xC629944C}); kernel::W32(0x505970, 128);
    kernel::W16(0x5D38B6, 0); forest_caption(55, 0, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; kernel::W32(0x696550, 0x5000); tick();
    check(sounds.empty(), "The opening forest thought must not collide with the original intro stream");
    kernel::W32(0x696550, 0); tick();
    check(sounds.size() == 1, "Opening thought may start when the native scene stops");

    for (const bool observed_frame : {false, true}) {
        reset(); definitions({0xC629944C}); kernel::W32(0x505970, 128);
        forest_caption(55, 0, 0); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0;
        play(0x12344321);
        if (observed_frame) tick();
        finish(); tick(); tick();
        check(sounds.empty(), "An old area-title reply must not survive a later finished or skipped scene");
    }
    reset(); definitions({0xC629944C}); kernel::W32(0x505970, 128);
    forest_caption(55, 0, 0); tick(); kernel::G<uint8_t>(0x4E74E0) = 0;
    kernel::W32(0x514508, 0x12344321); tick();
    check(sounds.empty(), "A scene replacement must invalidate an extra still waiting outside the voice queue");

    // The second tree's original text63 is voiced by Smoke. Raiden's unused
    // 2_03 guidance should complement that voice, not replace or duplicate it.
    reset(); definitions({0xC33F8FCE}); kernel::W32(0x505970, 129);
    forest_caption(63, 0x01DD37EB, 0); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.empty(), "An unplayed Smoke tree hint must not cause a Raiden reply");
    forest_caption(63, 0x01DD37EB, 0x5000); tick();
    check(sounds.empty(), "Raiden must wait for Smoke's original voice to end");
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1 && sounds.back()[2] == 0xC33F8FCE,
          "Raiden's forest-craving guidance must follow Smoke's original hint");
    kernel::W32(0x6C03DC, 0); forest_caption(63, 0x01DD37EB, 0x5000); tick();
    kernel::G<uint8_t>(0x4E74E0) = 0; tick();
    check(sounds.size() == 1, "Repeating the second tree hint cannot repeat the Raiden line");
    for (uint32_t scenario = 0; scenario < 5; ++scenario) {
        reset(); definitions({0xC33F8FCE}); kernel::W32(0x505970, 129);
        forest_caption(63, 0x01DD37EB, 0x5000);
        if (scenario == 0) settings.restore_dialogue = false;
        if (scenario == 1) kernel::W32(0x505970, 128);
        if (scenario == 2) kernel::W32(0x4E74F8, 62);
        if (scenario == 3) kernel::W32(0x4E74E4, 0x12345678);
        if (scenario == 4) definitions({});
        tick(); kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "The second tree reply requires Smoke's real played hint in the correct room");
    }
    // The original Monastery marker is informational only: it must not queue
    // the rejected Liu Kang tower comment for either playable monk.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        reset(); definitions({0x1F827982}); kernel::W32(0x505970, 192);
        kernel::W16(0x5D38B6, hero); forest_caption(115, 0, 0); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "Evil Monastery marker must not restore rejected tower speech");
    }

    // Warrior Shrine: the door stream follows the offering; title is mtxt_shrine. A visit only
    // counts once cs_wu_shi_13_2 (800DFF7A) has started and ended.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const auto expected = hero == 0 ? 0x29314E8Bu : 0xE5E6EFA6u;
        reset(); definitions({0x29314E8B, 0xE5E6EFA6}); kernel::W32(0x505970, 108);
        kernel::W16(0x5D38B6, hero); forest_caption(15, 0, 0); tick();
        check(sounds.empty(), "Warrior Shrine comments cannot preempt the original offering instruction");
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "The earlier offering hint alone cannot start the shrine speech");
        forest_caption(211, 0, 0); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "The offering alone cannot start speech ahead of the door opening");
        forest_caption(0, 0, 0); tick();
        kernel::G<uint8_t>(0x4E74E0) = 0; tick();
        check(sounds.empty(), "A displayed shrine area title is not proof of a finished scene");
        kernel::W32(0x69654C, 0x800DFF7A);
        kernel::W32(0x696550, 0x5000); tick();
        check(sounds.empty(), "The shrine door audio must not be treated as played when only buffered");
        kernel::W32(0x696550, 0);
        kernel::W32(0x696554, 0x5000);
        native_stream_playing = true; tick();
        check(sounds.empty(), "The Warrior Shrine door scene must finish without overlapping dialogue");
        native_stream_playing = false;
        kernel::G<uint8_t>(0x514505) = 1; tick();
        check(sounds.empty(), "Shrine dialogue must wait while the original cutscene is still flagged active");
        kernel::G<uint8_t>(0x514505) = 0;
        kernel::W32(0x696558, 0x12345678);
        kernel::W32(0x69655C, 0x5000); tick();
        check(sounds.empty(), "An unrelated original buffered scene must still reserve its audio slot");
        kernel::W32(0x69655C, 0); tick();
        check(sounds.size() == 1 && sounds.back()[2] == expected,
              "The correct monk should speak only after the original shrine door audio ends");
        kernel::W32(0x6C03DC, 0); tick();
        check(sounds.size() == 1, "The shrine audio cannot trigger the same optional voice again");
        kernel::W32(0x69654C, 0); kernel::W32(0x696554, 0); tick();
        check(sounds.size() == 1, "Native stream deletion must not repeat the shrine line");
    }
    reset(); definitions({0x29314E8B}); kernel::W32(0x505970, 108);
    kernel::W16(0x5D38B6, 0); kernel::W32(0x69654C, 0x800DFF7A);
    kernel::W32(0x696554, 0x5000); tick();
    check(sounds.empty(), "A shrine audio handle never observed playing cannot trigger the extra dialogue");
    reset(); definitions({0x29314E8B}); kernel::W32(0x505970, 108);
    kernel::W16(0x5D38B6, 0); kernel::W32(0x69654C, 0x800DFF7A);
    kernel::W32(0x696554, 0x5000); native_stream_playing = true; tick();
    kernel::W32(0x505970, 107); native_stream_playing = false; tick();
    check(sounds.empty(), "Leaving the shrine room before audio completion cancels its optional voice");
    reset(); definitions({0x29314E8B}); kernel::W32(0x505970, 108);
    kernel::W16(0x5D38B6, 0); kernel::W32(0x69654C, 0x800DFF7A);
    kernel::W32(0x696554, 0x5000); native_stream_playing = true; tick();
    settings.restore_dialogue = false; native_stream_playing = false; tick();
    check(sounds.empty(), "Disabled restoration cannot add shrine commentary after the native scene");
    // Wu Shi Map 11: four alternative takes; pick one hero line only after the first
    // voiced scene really ends.
    for (const auto hero : {int16_t(0), int16_t(1)}) for (uint32_t take : {0u, 1u}) {
        const uint32_t expected = hero == 0 ? (take ? 0x96DE55A2u : 0xD17E2F72u)
                                            : (take ? 0x172550E4u : 0x50852A34u);
        reset(); definitions({0xD17E2F72, 0x96DE55A2, 0x50852A34, 0x172550E4});
        kernel::W32(0x505970, 106); kernel::W16(0x5D38B6, hero);
        variation_seed = take;
        tick(); check(sounds.empty(), "Wu Shi's initial fire commentary cannot start on mere room entry");
        kernel::W32(0x69654C, 0x50329483); kernel::W32(0x696550, 0x5000); tick();
        check(sounds.empty(), "An original Wu Shi scene buffer is not evidence of completed voice playback");
        kernel::W32(0x696550, 0); kernel::W32(0x696554, 0x5000);
        native_stream_playing = true; tick();
        check(sounds.empty(), "Never interrupt the original seven-second fire-puzzle opening audio");
        native_stream_playing = false; tick();
        check(sounds.size() == 1 && sounds.back()[2] == expected,
              "Wu Shi may play one of the matching original hero takes only after the first CS ends");
        kernel::W32(0x6C03DC, 0); tick();
        check(sounds.size() == 1, "Both fire-puzzle variants must not play in the same visit");
    }
    reset(); definitions({0xD17E2F72}); kernel::W32(0x505970,106);
    kernel::W16(0x5D38B6, 0);
    kernel::W32(0x69654C,0x50329483); kernel::W32(0x696554,0x5000);
    native_stream_playing=true; tick();
    activate(106, 1704478846u, 0, 0, 7); // original first water lever disables its fire
    native_stream_playing=false; tick();
    check(sounds.empty(), "Already operated water levers must cancel the stale fire-puzzle hint");
    reset(); definitions({0xD17E2F72}); kernel::W32(0x505970,106);
    kernel::W16(0x5D38B6,0);
    kernel::W32(0x69654C, 0x50329483); kernel::W32(0x696554, 0x5000);
    native_stream_playing=false; tick();
    check(sounds.empty(), "Wu Shi's voice must never play without observing its original CS start");
    // The user explicitly rejected both optional Foundry Map 5 lines.
    // The original stamping stream must never enqueue those removed voices.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        reset(); definitions({0x0AA55E0E, 0x67D43C24});
        kernel::W32(0x505970, 68); kernel::W16(0x5D38B6, hero);
        tick(); check(sounds.empty(), "Removed Foundry voice must not start on map entry");
        kernel::W32(0x69654C, 0x7824C60A);
        kernel::W32(0x696550, 0x5000); tick();
        check(sounds.empty(), "Removed Foundry voice must not play while scene is buffered");
        kernel::W32(0x696550, 0); kernel::W32(0x696554, 0x5000);
        native_stream_playing = true; tick();
        check(sounds.empty(), "Removed Foundry voice must not interrupt the original scene");
        native_stream_playing = false; tick();
        check(sounds.empty(), "Rejected Foundry voices must remain disabled after the original scene finishes");
        tick(); check(sounds.empty(), "Rejected Foundry voices must not recur on later ticks");
    }
    reset(); definitions({0x00A33555}); kernel::W32(0x505970,104);
    kernel::W16(0x5D38B6+0x3238,71); kernel::W32(0x5D1AB8+0x3238+0x68,2);
    caption_lookup_sound=0xB283E945;
    check(caption_lookup(7)==0xB283E945,"Cage's cauldron follow-up must preserve the original armory caption voice");
    tick();
    check(sounds.size()==1 && sounds[0][2]==0x00A33555,
          "The authored armory instruction must queue Cage's explanation of the cauldron mechanism");
    kernel::W32(0x6C03DC,0); caption_lookup(7); tick();
    check(sounds.size()==1,"Rendering Cage's caption again must not repeat its cauldron explanation");
    reset(); definitions({0x2C85BB7E}); kernel::W32(0x505970,44); kernel::W16(0x5D38B6,69);
    voice(0,0x6B25C1AE); kernel::W32(0x6C03DC,0); tick();
    check(sounds.size()==1 && sounds[0][2]==0x2C85BB7E,
          "Sub-Zero's original off-boat cue must permit his contextual departure follow-up once");
    // Keep Sub-Zero's original help voice, but never add the rejected
    // catapult/barrier advice just because the help event fired.
    for (uint32_t iteration = 0; iteration < 2; ++iteration) {
        if (!iteration) {
            reset(); definitions({0x5645E81E}); kernel::W32(0x505970,44);
            kernel::W16(0x5D38B6,69);
        }
        voice(0,0xE5761AE6); kernel::W32(0x6C03DC,0); tick();
        check(last_native_sound==0xE5761AE6 && sounds.empty(),
              "Original Sub-Zero help must remain and never trigger rejected bonus dialogue");
    }
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const auto original = hero == 0 ? 0x3185C09Eu : 0x5873A43Au;
        const auto reply = hero == 0 ? 0xBF0AC77Du : 0x64DC7FC9u;
        reset(); definitions({0xBF0AC77D, 0x64DC7FC9}); kernel::W32(0x505970, 96);
        kernel::W16(0x5D38B6, hero); voice(0, original);
        kernel::W32(0x6C03DC, 0); tick(); tick();
        check(last_native_sound == original && sounds.empty(),
              "The intro response must not pull the Wu Shi 2_02 reply forward");
        reset(); definitions({0xBF0AC77D, 0x64DC7FC9});
        activate(96, 0xB001C237, hero); tick(); tick();
        check(sounds.size()==1 && sounds[0][2]==reply,
              "The monk's Wu Shi 2_02 reply takes the removed combo tip's place, in the hero's voice");
        kernel::W32(0x6C03DC, 0); activate(96, 0xB001C237, hero); tick(); tick();
        check(sounds.size()==1, "The Wu Shi 2_02 reply plays once");
        if (hero == 0) {
            reset(); definitions({0xBF0AC77D, 0x64DC7FC9});
            activate(96, 0xB001C237, hero); tick(); tick();
            voice_stops.clear(); native_stream_playing = true; voice(0, 0x12345678);
            native_stream_playing = false;
            check(voice_stops.size()==1 && voice_stops[0]==0x5000 &&
                  last_native_sound==0x12345678 && kernel::R32(0x6C03DC)==0x5000,
                  "Native speech must interrupt our own addition instead of being deferred behind it");
            reset(); definitions({0x12345678}); kernel::W32(0x505970, 96); kernel::W32(0x6C03DC, 0x7000);
            native_stream_playing = true; voice(0, 0x12345678); native_stream_playing = false;
            check(voice_stops.empty() || voice_stops.back()!=0x7000,
                  "Native speech must never stop another native voice");
        }
        reset(); definitions({0xBF0AC77D, 0x64DC7FC9}); kernel::W16(0x5D38B6, 5);
        activate(96, 0xB001C237, 5); tick(); tick();
        check(sounds.empty(), "Only Liu Kang or Kung Lao answer");
    }
    // Soul Tombs/Baraka: Liu 16_05 is replaced only for solo Kung Lao; never in Ko-op,
    // other rooms, without definitions or with recovery off.
    for (int scenario=0; scenario<6; ++scenario) {
        reset(); definitions(scenario == 4 ? std::vector<uint32_t>{} :
                             std::vector<uint32_t>{0x530DACE6});
        kernel::W32(0x505970,scenario == 2 ? 16 : 15);
        kernel::W32(0x5ACEBC,0x3500); kernel::W32(0x3588, scenario == 1 ? 1 : 0);
        kernel::W16(0x5D38B6, scenario == 3 ? 0 : 1);
        if (scenario == 5) settings.restore_dialogue = false;
        voice(0,0x6935F9CD);
        const uint32_t expected = scenario == 0 ? 0x530DACE6u : 0x6935F9CDu;
        check(last_native_sound == expected && kernel::R32(0x2100)==0x6935F9CD,
              "Baraka's standalone original line may select Kung only in the valid solo-Kung context");
    }
    // Scorpion's 04 taunt plays first; 02_1 is an optional once follow-up, never a
    // replacement, loop or another actor's trigger.
    reset(); definitions({0x168B9C83}); kernel::W32(0x505970,160);
    kernel::W16(0x5D38B6,52); kernel::W32(0x5D1AB8+0x68,0x9000); tick();
    check(sounds.empty(),"Scorpion cannot speak an added line solely on boss room entry");
    voice(0,0xB9E947A4,0xAECADCD8);
    check(last_native_sound==0xB9E947A4 && sounds.empty(),
          "Scorpion's authored 04 combat taunt must play before the distinct 02 take");
    kernel::W32(0x6C03DC,0);kernel::W32(0x696550,0x5000);tick();
    check(sounds.empty(),"Scorpion's added line must wait for a prepared original cinematic stream");
    kernel::W32(0x696550,0);tick();
    check(sounds.size()==1 && sounds.back()[2]==0x168B9C83,
          "Scorpion's contextual 02 line may follow one genuinely accepted boss taunt");
    kernel::W32(0x6C03DC,0); voice(0,0xB9E947A4,0xAECADCD8);
    kernel::W32(0x6C03DC,0); tick();
    check(sounds.size()==1,"Scorpion's additional 02 line must not repeat in the same encounter");
    reset();definitions({0x168B9C83});kernel::W32(0x505970,160);
    kernel::W16(0x5D38B6,52);kernel::W32(0x5D1AB8+0x68,0x9000);
    voice(0,0xB9E947A4,0xAECADCD8);
    activate(160,2390456960u,52,1,110); // original _d_death52
    kernel::W32(0x6C03DC,0);tick();
    check(sounds.empty(),"Scorpion's death must cancel his optional combat response");
    for (int scenario=0;scenario<4;++scenario) {
        reset(); definitions(scenario == 3 ? std::vector<uint32_t>{} :
                             std::vector<uint32_t>{0x168B9C83});
        kernel::W32(0x505970,scenario == 0 ? 159 : 160);
        kernel::W16(0x5D38B6,scenario == 1 ? 55 : 52);
        kernel::W32(0x5D1AB8+0x68,0x9000);
        if (scenario == 2) settings.restore_dialogue=false;
        voice(0,0xB9E947A4,0xAECADCD8);
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.empty(),"Scorpion's extra speech must reject incorrect room, actor, option or bank");
    }
    // Evil Monastery Map 4: text45 opens the three-sister fight; the Mileena joke waits
    // for it to end with all three still present.
    auto three_sisters = [&] {
        for (uint32_t i=0;i<3;++i) {
            const uint32_t at=0x5D1AB8+(i+2)*0x3238;
            kernel::W16(at+0x1DFE,65+i);
            kernel::W32(at+0x68,0x8000+i);
        }
    };
    reset(); definitions({0x804E23CE}); kernel::W32(0x505970,195);
    three_sisters(); tick();
    check(sounds.empty(),"Mileena's 3-women joke must never play on room entry alone");
    forest_caption(45,0x12345678,0x5000); tick();
    check(sounds.empty(),"Mileena's joke must never overlap its original combat-start caption");
    kernel::G<uint8_t>(0x4E74E0)=0;kernel::G<uint8_t>(0x514505)=1;tick();
    check(sounds.empty(),"Mileena's joke must wait while the original intro cinematic is active");
    kernel::G<uint8_t>(0x514505)=0;kernel::W32(0x696550,0x6000);tick();
    check(sounds.empty(),"Mileena's joke must not interrupt a reserved original stream");
    kernel::W32(0x696550,0);tick();
    check(sounds.size()==1 && sounds.back()[2]==0x804E23CE,
          "Mileena may deliver one 3-women joke after the true introductory audio ends");
    kernel::W32(0x6C03DC,0); forest_caption(45,0x12345678,0x5000);tick();
    kernel::G<uint8_t>(0x4E74E0)=0;tick();
    check(sounds.size()==1,"Mileena's 3-women joke must not repeat after re-opening text45");
    for (int scenario=0;scenario<7;++scenario) {
        reset();definitions({0x804E23CE});kernel::W32(0x505970,195);three_sisters();
        forest_caption(45,0x12345678,0x5000);
        if (scenario==0) settings.restore_dialogue=false;
        if (scenario==1) kernel::W32(0x505970,194);
        if (scenario==2) kernel::W32(0x4E74F8,46);
        if (scenario==3) kernel::W32(0x4E74E8,0);
        if (scenario==4) kernel::W32(0x5D1AB8+3*0x3238+0x68,0);
        if (scenario==5) definitions({});
        if (scenario==6) activate(195,2447738644u,0,1,4); // authentic _d_freekit
        tick();kernel::G<uint8_t>(0x4E74E0)=0;tick();
        check(sounds.empty(),"The three-woman joke requires true start and opponents; freeing Kitana cancels it");
    }
    // Spare EM4 lines are one-shot fight comments: only the end of Mileena's original
    // attack animations may schedule them.
    auto mileena_anim = [&](uint32_t anim) {
        kernel::W32(0x1004,3);kernel::W32(0x1008,anim);
        X86Context ctx{};ctx.esp=0x1000;
        sub_00176270(ctx,memory.data());
    };
    for (const bool coop : {false,true}) {
        reset();definitions({0x804E23CE,0x85897096,0x7C193369,0x3BB949B9,
                             0x0ACC1FAF,0x4D6C657F});
        kernel::W32(0x505970,195);three_sisters();
        kernel::W32(0x5ACEBC,0x3500);kernel::W32(0x3588,coop ? 1 : 0);
        kernel::W16(0x5D38B6,0);kernel::W32(0x5D1AB8+0x68,0x9990);
        kernel::W16(0x5D38B6+0x3238,1);
        kernel::W32(0x5D1AB8+0x3238+0x68,coop ? 0x9991 : 0);
        forest_caption(45,0x12345678,0x5000);tick();
        kernel::G<uint8_t>(0x4E74E0)=0;tick();
        check(sounds.size()==1 && sounds.back()[2]==0x804E23CE,
              "EM4 must deliver the existing three-women joke before supplementary combat taunts");
        const uint32_t coop_takes[]={0x85897096,0x7C193369,0x3BB949B9,
                                     0x0ACC1FAF,0x4D6C657F};
        const uint32_t solo_takes[]={0x85897096,0x0ACC1FAF,0x4D6C657F};
        const uint32_t* takes=coop ? coop_takes : solo_takes;
        for (uint32_t j=0;j<(coop ? 5u : 3u);++j) {
            kernel::W32(0x6C03DC,0);tick();
            check(sounds.size()==j+1,"EM4 cannot enqueue unused taunts without a Mileena attack");
            const uint32_t attacks[]={1764,1766,1770,1771};
            mileena_anim(attacks[j%4]);tick();
            check(sounds.size()==j+1,"An active Mileena attack must keep its original attack yell");
            mileena_anim(1700);tick();
            check(sounds.size()==j+2 && sounds.back()[2]==takes[j],
                  "All requested EM4 voices must play via the native bank after genuine Mileena attacks");
            kernel::W32(0x6C03DC,0);tick();
            check(sounds.size()==j+2,"Holding an idle animation must not duplicate a Mileena taunt");
        }
        kernel::W32(0x6C03DC,0);mileena_anim(1771);mileena_anim(1700);tick();
        check(sounds.size()==(coop ? 6u : 4u),
              "EM4 must not invent co-op taunts for solo or repeat spent native takes");
    }
    reset();definitions({0x804E23CE,0x85897096});
    kernel::W32(0x505970,195);three_sisters();
    forest_caption(45,0x12345678,0x5000);tick();
    kernel::G<uint8_t>(0x4E74E0)=0;tick();
    kernel::W32(0x6C03DC,0);mileena_anim(1764);
    activate(195,2447738644u,0,1,4);mileena_anim(1700);tick();
    check(sounds.size()==1 && sounds.back()[2]==0x804E23CE,
          "Freeing Kitana must cancel any later Mileena combat taunt, not erase the original scene");
    // Forest Map 5 never plays Evil Monastery voices: only its loaded bank, no host PCM
    // or cross-bank substitution.
    reset();definitions({0x0ACC1FAF,0x4D6C657F});
    kernel::W32(0x505970,132);kernel::W16(0x5D38B6,66);
    kernel::W32(0x5D1AB8+0x68,0xB000);
    for (unsigned i=0;i<4;++i) {
        voice(0,0x2DB6EB65,0xAECADCD8);
        check(last_native_sound==0x2DB6EB65 && kernel::R32(0x2100)==0x2DB6EB65,
              "Forest Mileena's retail taunt remains unchanged without cross-bank return voices");
        kernel::W32(0x6C03DC,0);tick();
        check(sounds.empty(),"Forest must never enqueue an EM4-only return voice");
    }
    reset(); definitions({0xDB7B83D3,0x9CDBF903});
    kernel::W32(0x505970,72); kernel::W16(0x5D38B6,54);
    for (unsigned i=0;i<8;++i) {
        kernel::W32(0x6C03DC,0); voice(0,0x898DD582,0xAECADCD8);
        check(last_native_sound==0x898DD582, "An unavailable game-mode state must not choose solo-only Kano threats");
    }
    reset(); definitions({0xF26D1CDF}); kernel::W32(0x505970,15); kernel::W16(0x5D38B6,59);
    voice(0,0x66FE4E8E,0xAECADCD8);
    check(last_native_sound==0x66FE4E8E,"A Baraka family must not be selected for another actor sharing a script request");
    // Orochi (Tombs Map 13) heals by devouring Tarkatans: the hero remarks once per visit.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t remark = hero == 0 ? 0xAE950209u : 0x94AD5722u;
        const uint32_t boss = 0x5D1AB8 + 3 * 0x3238;
        reset(); definitions({0xAE950209, 0x94AD5722}); kernel::W32(0x505970, 12); kernel::W16(0x5D38B6, hero);
        kernel::W32(boss + 0x68, 0xB055); kernel::W16(boss + 0x1DFE, 80); kernel::W32(boss + 0x200, 0x7000);
        kernel::W32(0x7000 + 0x370, 1000); kernel::W32(0x7000 + 0x368, 300); tick();
        kernel::W32(0x7000 + 0x368, 250); tick(); tick();
        check(sounds.empty(), "Damage to Orochi must not trigger the feeding remark");
        kernel::W32(0x7000 + 0x368, 400); tick(); tick();
        check(sounds.size() == 1 && sounds[0][2] == remark, "Orochi's healing must trigger the hero's feeding remark");
        kernel::W32(0x6C03DC, 0); kernel::W32(0x7000 + 0x368, 500); tick(); tick();
        check(sounds.size() == 1, "The feeding remark plays only once per visit");
        reset(); definitions({remark}); kernel::W32(0x505970, 13); kernel::W16(0x5D38B6, hero);
        kernel::W32(boss + 0x68, 0xB055); kernel::W16(boss + 0x1DFE, 80); kernel::W32(boss + 0x200, 0x7000);
        kernel::W32(0x7000 + 0x370, 1000); kernel::W32(0x7000 + 0x368, 300); tick();
        kernel::W32(0x7000 + 0x368, 400); tick(); tick();
        check(sounds.empty(), "The feeding remark belongs to Orochi's arena");
    }
    // An optional addition waiting in the queue must not defer native speech when the slot is free.
    {
        const uint32_t boss = 0x5D1AB8 + 3 * 0x3238;
        reset(); definitions({0xAE950209}); kernel::W32(0x505970, 12);
        kernel::W32(boss + 0x68, 0xB055); kernel::W16(boss + 0x1DFE, 80); kernel::W32(boss + 0x200, 0x7000);
        kernel::W32(0x7000 + 0x370, 1000); kernel::W32(0x7000 + 0x368, 300); tick();
        kernel::W32(0x6C03DC, 0x7100); kernel::W32(0x7000 + 0x368, 400); tick();
        check(sounds.empty(), "The remark waits while another voice speaks");
        kernel::W32(0x6C03DC, 0); voice(0, 0x12345678);
        check(last_native_sound == 0x12345678 && kernel::R32(0x6C03DC) == 0x5000,
              "A queued optional addition must not defer native speech when the voice slot is free");
        kernel::W32(0x6C03DC, 0); tick(); tick();
        check(sounds.size() == 1 && sounds[0][2] == 0xAE950209, "The addition plays after the native line");
    }
    // Baraka's impale prompt (template 8, text 106) is voiced by vo_rai_impale 20 ticks later.
    reset(); definitions({0x323E17E6}); kernel::W32(0x505970, 15);
    kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74E4, 0x323E17E6); kernel::W32(0x4E74E8, 0); tick();
    voice(0, 0x323E17E6);
    check(last_native_sound == 0x323E17E6 && kernel::R32(0x6C03DC) == 0x5000 && sounds.empty(),
          "Raiden's impale line must start with its prompt, not after the prompt closes");
    reset(); definitions({0x323E17E6}); kernel::W32(0x505970, 15);
    kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74E4, 0xFFFFFFFF); kernel::W32(0x4E74E8, 0); tick();
    voice(0, 0x323E17E6);
    check(last_native_sound == 0x323E17E6 && kernel::R32(0x6C03DC) == 0x5000 && sounds.empty(),
          "A portrait prompt without a caption voice (sound FFFFFFFF) must not delay the script's line");
    reset(); definitions({0x12345678}); kernel::W32(0x505970, 15);
    kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74E4, 0x0BADF00D); kernel::W32(0x4E74E8, 0); tick();
    voice(0, 0x12345678);
    check(kernel::R32(0x6C03DC) == 0, "Another line still yields while a caption is creating its own voice");
    std::this_thread::sleep_for(std::chrono::milliseconds(600)); tick(); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x12345678,
          "A caption that never creates its voice must not hold other speech for its whole display");
    kernel::G<uint8_t>(0x4E74E0) = 0;
    // Ermac's first line plays as _d_ermac raises him from the statue, once per room load.
    {
        const uint32_t ermac = 0x5D1AB8 + 2 * 0x3238;
        reset(); definitions({0x537C2156}); kernel::W32(0x505970, 108);
        activate(108, 0x769191E3, 0, 1, 3); tick();
        check(sounds.empty(), "Ermac's intro waits for the spawn to settle");
        kernel::W16(ermac + 0x1DFE, 68); kernel::W32(ermac + 0x68, 0xE123);
        std::this_thread::sleep_for(std::chrono::milliseconds(600)); tick(); tick();
        check(sounds.size() == 1 && sounds[0][2] == 0x537C2156, "Ermac must open his presentation with his first line");
        kernel::W32(0x6C03DC, 0); activate(108, 0x769191E3, 0, 1, 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(600)); tick(); tick();
        check(sounds.size() == 1, "Ermac's intro line plays once per room load");
    }
    // Wu Shi Map 06: Raiden explains the save shrines at the first totem, in a new unsaved campaign.
    {
        auto new_game = [&] { kernel::W32(0x505970, 96); play(0x7922A984); finish(); tick(); };
        auto shrine_prompt = [&] {
            kernel::W32(0x505970, 101); kernel::W32(0x6C03DC, 0); tick();
            kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74F8, 60); tick();
            kernel::G<uint8_t>(0x4E74E0) = 0; tick(); tick();
        };
        reset(); definitions({0x4B368C31}); new_game(); shrine_prompt();
        check(sounds.size() == 1 && sounds[0][2] == 0x4B368C31, "A new game's first totem prompt brings Raiden's shrine hint");
        shrine_prompt();
        check(sounds.size() == 1, "The shrine hint plays once");
        kernel::W32(0x505970, 102); tick(); shrine_prompt();
        check(sounds.size() == 1, "Coming back to Map 06 does not repeat the shrine hint");
        ++checkpoint_restores; tick(); shrine_prompt();
        check(sounds.size() == 2, "After death or retry before saving, the shrine hint plays again");
        ++save_slot_transfers; reset(); definitions({0x4B368C31}); shrine_prompt();
        check(sounds.empty(), "Saving ends the shrine tutorial for that campaign");
        ++save_slot_transfers; reset(); definitions({0x4B368C31}); shrine_prompt();
        check(sounds.empty(), "A loaded campaign never gets the shrine tutorial");
        reset(); definitions({0x4B368C31}); new_game(); shrine_prompt();
        check(sounds.size() == 1, "A new game after loading another one still gets the tutorial");
        reset(); definitions({0x4B368C31}); new_game(); kernel::W32(0x505970, 107); tick();
        kernel::G<uint8_t>(0x4E74E0) = 1; kernel::W32(0x4E74F8, 60); tick(); kernel::G<uint8_t>(0x4E74E0) = 0; tick(); tick();
        check(sounds.empty(), "Only the first totem, in Wu Shi Map 06, brings the hint");
        save_slot_transfers = 0;
    }
    // Owner removed Raiden's vo_rdn_help_gl_ps2_1 ("defeat these enemies") from _d_tutor01.
    reset(); definitions({0x2AD7B0C5}); activate(96,0x30A4AFE0,0); tick();
    check(sounds.empty(), "The deleted _d_tutor01 target must stay silent");
    // Ermac phase activations come from the room script (not the live Ermac); a phase
    // cinematic can clear the queue, so hold the request until it ends.
    reset();definitions({0x14DC5B86,0x29BC7236});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    kernel::G<uint8_t>(0x514505)=1;
    activate(108,0x5A3297F2u,0,1,13);tick();
    check(sounds.empty(),"Ermac phase3 line must wait until the original phase cinematic ends");
    kernel::G<uint8_t>(0x514505)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x14DC5B86,
          "Ermac phase3 script event must use the live Ermac actor after cinematic, not the event's actor");
    kernel::W32(0x6C03DC,0);activate(108,0x5A3297F2u,0,1,13);tick();
    check(sounds.size()==1,"Ermac phase3 must not replay after native handle was created");
    reset();definitions({0x14DC5B86,0x29BC7236});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    activate(108,0x7873AA87u,0,1,0xFFFFFFFFu);tick();
    check(sounds.empty(),"Ermac stone-Cage sentence must NOT play at phase4 tick0");
    kernel::G<uint8_t>(0x514505)=1;
    activate(108,0x9113964Eu,0,1,18);tick();
    check(sounds.empty(),"Ermac must not speak while the genuine phase4 cage effect plays");
    kernel::G<uint8_t>(0x514505)=0;tick();
    check(sounds.size()==1 && sounds.back()[2]==0x29BC7236,
          "Ermac 13_03 must wait for the authored delayed cage effect AND completed cinematic");
    reset();definitions({0x14DC5B86});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    activate(108,0x5A3297F2u,0,1,13);
    kernel::W32(0x505970,107);tick();kernel::W32(0x505970,108);tick();
    check(sounds.empty(),"Leaving Ermac's room must cancel phase speech awaiting playback");
    reset();definitions({0x14DC5B86});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    activate(108,0x5A3297F2u,0,1,13);
    kernel::W32(0x639224,0x80020000);tick();
    check(sounds.empty(),"Replacing the game world must cancel Ermac's previous phase");
    // The genuine phase activation's callback actor may be an out-of-range
    // script/controller sentinel. The boss lives independently in actor slot1.
    reset();definitions({0x14DC5B86});
    kernel::W32(0x505970,108);
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    kernel::W32(0x1004,0x2000);kernel::W32(0x1008,0x2200);
    kernel::W32(0x2008,0x2100);kernel::W32(0x2100,1);
    kernel::W32(0x2104,0x5A3297F2);kernel::W32(0x2108,13);
    kernel::W32(0x2210,99);
    {X86Context ctx{};ctx.esp=0x1000;sub_00170330(ctx,memory.data());}
    tick();
    check(sounds.size()==1 && sounds.back()[2]==0x14DC5B86,
          "Ermac phase dispatch may originate from a script actor outside native actor slots");
    // A nonzero sound definition is not proof the native sound factory
    // created a handle. A failed Play must not irreversibly consume the cue.
    reset();definitions({0x14DC5B86});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    activate(108,0x5A3297F2u,0,1,13);
    accept_audio=false;tick();
    check(sounds.size()==1 && !kernel::R32(0x6C03DC),
          "An Ermac native audio factory refusal has no audible voice handle");
    accept_audio=true;tick();
    check(sounds.size()==2 && sounds.back()[2]==0x14DC5B86 &&
          kernel::R32(0x6C03DC)==0x5000,
          "Ermac phase voice must retry when its original native factory previously returned zero");
    reset();definitions({0x14DC5B86});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    settings.restore_dialogue=false;
    activate(108,0x5A3297F2u,0,1,13);tick();
    check(sounds.empty(),"Disabled restoration must preserve Ermac's original phase without optional speech");
    reset();definitions({});
    kernel::W16(0x5D38B6+0x3238,68);
    kernel::W32(0x5D1AB8+0x3238+0x68,0xE123);
    activate(108,0x5A3297F2u,0,1,13);tick();
    check(sounds.empty(),"Missing Ermac bank definitions must never manufacture a voice handle");
    auto animation = [&](uint32_t slot, uint32_t requested) {
        kernel::W32(0x1004,slot); kernel::W32(0x1008,requested);
        X86Context ctx{}; ctx.esp=0x1000;
        sub_00176270(ctx,memory.data());
        check(ctx.eax==0x1234 && ctx.ecx==0x5678 && ctx.esp==0x1004,
              "A phase recovery must preserve the original animation setter's cdecl state");
    };
    reset(); definitions({0x9EABBADC}); kernel::W32(0x505970,195); kernel::W16(0x5D38B6,65);
    animation(0,1721); tick();
    check(sounds.empty(), "Raiden's capture warning (vo_rdn_em_4_01) was removed from the story");
    for (const auto actor : {int16_t(66),int16_t(67)}) {
        const uint32_t group=actor==66?0xE35C1CF7:0xF0252683;
        reset(); stream_bank(group,{0x6B2B1C1C}); kernel::W32(0x505970,195); kernel::W16(0x5D38B6,actor);
        animation(0,1723); tick(); kernel::W32(0x6C03DC,0); tick();
        check(sounds.size()==1 && sounds[0][2]==group,
              "A real freeing attempt must queue the correct sister's original voice family, without Raiden's removed warning");
        kernel::W32(0x6C03DC,0); animation(0,1723); tick();
        check(sounds.size()==1,"A repeated same-animation request must not flood freeing voices");
        kernel::W32(0x5D1AB8+0x24,0); animation(0,1723); tick();
        check(sounds.size()==2 && sounds.back()[2]==group,
              "A later distinct freeing attempt must retain native family variation");
    }
    auto native_audio = [&](uint32_t sound, bool banked=false) {
        kernel::W32(0x1004,banked?0:sound); kernel::W32(0x1008,sound);
        X86Context ctx{}; ctx.esp=0x1000;
        if (banked) sub_00151610(ctx,memory.data()); else sub_00151560(ctx,memory.data());
        check(ctx.ecx==0x5678 && ctx.esp==(banked?0x1028u:0x1024u) &&
              kernel::R32(0x1004)==(banked?0:sound) && kernel::R32(0x1008)==sound,
              "Native take recovery must preserve original argument words, result registers and callee stack cleanup");
        return ctx.eax;
    };
    // Shao Kahn (room 73): the lead monk opens the fight as Shao Kahn's intro scene ends.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t opener = hero == 0 ? 0x2AFD818Fu : 0xC99901EFu;
        auto count = [&](uint32_t sound) {
            return std::count_if(sounds.begin(), sounds.end(), [&](const auto& x) { return x[2] == sound; });
        };
        reset(); definitions({opener}); kernel::W32(0x505970, 73); kernel::W16(0x5D38B6, hero);
        play(0x60F8EA8D); tick();
        check(count(opener) == 0, "The opener waits for Shao Kahn's intro to end");
        finish(); tick();
        check(count(opener) == 1, "The lead monk opens the Shao Kahn fight as the intro ends");
        kernel::W32(0x6C03DC, 0); native_audio(0xFEDCADEA, true); tick();
        check(count(0xFEDCADEA) == 1, "Shao Kahn's first taunt still plays with its gesture");
        reset(); definitions({opener}); kernel::W32(0x505970, 72); kernel::W16(0x5D38B6, hero);
        play(0x60F8EA8D); tick(); finish(); tick();
        check(count(opener) == 0, "The opener belongs to Shao Kahn's arena");
    }
    // Foundry Map 04 (room 67): the lead monk points out the hooks as the entry camera (cin1) ends.
    auto cin_camera = [&](uint32_t index) {
        kernel::W32(0x599844, 0x3000);
        kernel::W32(0x1004, 0x2000); kernel::W32(0x2008, 0x2100); kernel::W32(0x2100, index);
        X86Context ctx{}; ctx.esp = 0x1000;
        sub_00016924(ctx, memory.data());
        check(ctx.eax == 0x1234 && ctx.ecx == 0x5678 && ctx.esp == 0x1010,
              "The camera hook must preserve the native play_cincamera result and stack");
    };
    auto camera_ends = [&] { kernel::W32(0x301C, 0); };
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t hint = hero == 0 ? 0x26AAB15Fu : 0xEA7D1072u;
        auto heard = [&](uint32_t sound) {
            return std::count_if(sounds.begin(), sounds.end(), [&](const auto& x) { return x[2] == sound; });
        };
        reset(); definitions({hint}); kernel::W32(0x505970, 67); kernel::W16(0x5D38B6, hero); tick();
        cin_camera(1); tick(); tick();
        check(heard(hint) == 0, "The hooks hint never talks over the entry camera");
        camera_ends(); tick(); tick();
        check(heard(hint) == 1, "The lead monk points out the hooks as the entry camera ends");
        kernel::W32(0x6C03DC, 0);
        cin_camera(3); tick(); camera_ends(); tick(); tick();
        check(heard(hint) == 1, "The lift camera does not carry the hooks hint");
        cin_camera(1); tick(); camera_ends(); tick(); tick();
        check(heard(hint) == 1, "The hooks hint plays once per attempt");
        reset(); definitions({hint}); kernel::W32(0x505970, 68); kernel::W16(0x5D38B6, hero); tick();
        cin_camera(1); tick(); camera_ends(); tick(); tick();
        check(heard(hint) == 0, "The hooks hint belongs to Foundry Map 04");
        // After Test Your Might, the cauldron camera (cin2) ends with the monk's cauldron remark.
        const uint32_t cauldron = hero == 0 ? 0x0AA55E0Eu : 0x67D43C24u;
        reset(); definitions({hint, cauldron}); kernel::W32(0x505970, 67); kernel::W16(0x5D38B6, hero); tick();
        cin_camera(2); tick(); tick();
        check(heard(cauldron) == 0, "The cauldron remark waits a beat into the cauldron camera");
        for (int i = 0; i < 46; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); tick(); }
        check(heard(cauldron) == 1 && heard(hint) == 0,
              "The lead monk remarks on the cauldron during its camera, without the hooks hint");
        camera_ends(); tick(); tick();
        kernel::W32(0x6C03DC, 0);
        cin_camera(2); tick(); camera_ends(); tick(); tick();
        check(heard(cauldron) == 1, "The cauldron remark plays once per attempt");
    }
    // Foundry Map 03 (room 66): Kung Lao speaks during the forge door camera (cin1) after the press.
    {
        const uint32_t remark = 0xE7636035;
        auto heard = [&](uint32_t sound) {
            return std::count_if(sounds.begin(), sounds.end(), [&](const auto& x) { return x[2] == sound; });
        };
        auto run = [&](int ms) {
            for (int i = 0; i < ms / 50; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); tick(); }
        };
        reset(); definitions({remark}); kernel::W32(0x505970, 66); kernel::W16(0x5D38B6, 1); tick();
        cin_camera(1); tick();
        check(heard(remark) == 0, "The forge door remark waits a beat into the camera");
        run(1300);
        check(heard(remark) == 1, "Kung Lao speaks during the forge door camera");
        camera_ends(); kernel::W32(0x6C03DC, 0); tick();
        cin_camera(1); run(1300); camera_ends(); tick();
        check(heard(remark) == 1, "The forge door remark plays once per attempt");
        reset(); definitions({remark}); kernel::W32(0x505970, 66); kernel::W16(0x5D38B6, 0); tick();
        cin_camera(1); run(1300); camera_ends(); tick();
        check(heard(remark) == 0, "Liu has no forge door take");
        reset(); definitions({remark}); kernel::W32(0x505970, 66); kernel::W16(0x5D38B6, 0);
        kernel::W32(0x5ACEBC, 0x4000); kernel::W32(0x4000 + 0x88, 1); kernel::W16(0x5D38B6 + 0x3238, 1); tick();
        cin_camera(1); run(1300); camera_ends(); tick();
        check(heard(remark) == 1, "In Ko-op Kung Lao speaks the forge door remark as player 2 too");
        reset(); definitions({remark}); kernel::W32(0x505970, 67); kernel::W16(0x5D38B6, 1); tick();
        cin_camera(1); run(1300); camera_ends(); tick();
        check(heard(remark) == 0, "The forge door remark belongs to Foundry Map 03");
    }
    // Foundry Map 02 (room 65): Liu remarks on the furnace 2 s after entering, until it is used.
    {
        const uint32_t remark = 0x2BB4C118;
        auto heard = [&](uint32_t sound) {
            return std::count_if(sounds.begin(), sounds.end(), [&](const auto& x) { return x[2] == sound; });
        };
        auto enter = [&](int16_t hero, bool solved) {
            reset(); definitions({remark}); kernel::W32(0x505970, 65); kernel::W16(0x5D38B6, hero);
            kernel::W32(0x5ACEBC, 0x4000); kernel::G<uint8_t>(0x4000 + 0x4874 + 26) = solved;
            tick();
        };
        auto play_for = [&](int ms) {
            for (int i = 0; i < ms / 50; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); tick(); }
        };
        enter(0, false); tick();
        check(heard(remark) == 0, "The furnace remark waits two seconds after entering");
        play_for(2300); tick();
        check(heard(remark) == 1, "Liu remarks on the furnace two seconds after entering");
        kernel::W32(0x6C03DC, 0);
        kernel::W32(0x505970, 66); tick(); kernel::W32(0x505970, 65); tick();
        play_for(2300); tick();
        check(heard(remark) == 2, "Until the furnace is used, every entry to Foundry Map 02 repeats the remark");
        enter(0, true);
        play_for(2300); tick();
        check(heard(remark) == 0, "Once the furnace has been used (gameevent 26), the remark is gone");
        enter(1, false);
        play_for(2300); tick();
        check(heard(remark) == 0, "Kung Lao has no furnace take");
        enter(1, false); kernel::W32(0x4000 + 0x88, 1); kernel::W16(0x5D38B6 + 0x3238, 0);
        play_for(2300); tick();
        check(heard(remark) == 1, "In Ko-op Liu speaks the furnace remark as player 2 too");
    }
    // Wu Shi Map 08: one catapult remark, then one barricade remark, in the lead monk's voice.
    for (const auto hero : {int16_t(0), int16_t(1)}) {
        const uint32_t catapult = hero == 0 ? 0xEB5B5725u : 0xB2DD7840u;
        const uint32_t barricade = hero == 0 ? 0xA1EB4F84u : 0x8FBD51F0u;
        auto heard = [&](uint32_t sound) {
            return std::count_if(sounds.begin(), sounds.end(), [&](const auto& x) { return x[2] == sound; });
        };
        reset(); definitions({catapult, barricade}); kernel::W32(0x505970, 103); kernel::W16(0x5D38B6, hero);
        native_audio(0xF67B4949, true); tick();
        check(heard(barricade) == 0, "A breakable before any catapult must not trigger the barricade remark");
        native_audio(0xF5F029C2, true); tick();
        check(heard(catapult) == 0, "The first catapult is fired by an enemy: no remark yet");
        native_audio(0xF67B4949, true); tick();
        check(heard(barricade) == 0, "No barricade remark before the catapult remark");
        native_audio(0xF5F029C2, true); tick();
        check(heard(catapult) == 1, "The second catapult launch must trigger the monk's catapult remark");
        kernel::W32(0x6C03DC, 0); native_audio(0xF5F029C2, true); tick();
        check(heard(catapult) == 1, "The catapult remark plays once per visit");
        native_audio(0xF67B4949, true); tick();
        check(heard(barricade) == 1, "The first broken barricade must trigger the barricade remark");
        kernel::W32(0x6C03DC, 0); native_audio(0xF67B4949, true); tick();
        check(heard(barricade) == 1, "The barricade remark plays once per visit");
        kernel::W32(0x505970, 104); native_audio(0xB91B5BAA, true); tick();
        kernel::W32(0x505970, 103); kernel::W32(0x6C03DC, 0);
        native_audio(0xF5F029C2, true); native_audio(0xF5F029C2, true); tick(); tick();
        check(heard(catapult) == 1, "Coming back to Map 08 must not repeat the catapult remark");
        ++checkpoint_restores; tick();
        native_audio(0xF5F029C2, true); native_audio(0xF5F029C2, true); tick(); tick();
        check(heard(catapult) == 2, "After death or retry the catapult remark plays again");
        reset(); definitions({catapult}); kernel::W32(0x505970, 104); kernel::W16(0x5D38B6, hero);
        native_audio(0xF5F029C2, true); tick();
        check(heard(catapult) == 0, "Catapult remarks belong to Wu Shi Map 08");
    }
    // Retail voices can bypass vosound: Liu 16_05 at either native entry becomes the Soul Tombs
    // alternative for solo Kung (voices baked into films are out of scope).
    for (const bool banked : {false, true}) {
        for (int scenario=0;scenario<6;++scenario) {
            reset(); definitions(scenario == 4 ? std::vector<uint32_t>{} :
                                  std::vector<uint32_t>{0x530DACE6});
            kernel::W32(0x505970, scenario == 2 ? 16 : 15);
            kernel::W32(0x5ACEBC, 0x3500); kernel::W32(0x3588, scenario == 1 ? 1 : 0);
            kernel::W16(0x5D38B6, scenario == 3 ? 0 : 1);
            if (scenario == 5) settings.restore_dialogue = false;
            native_audio(0x6935F9CD,banked);
            const uint32_t expected=scenario==0 ? 0x530DACE6u : 0x6935F9CDu;
            check(sounds.back()[2]==expected,
                  "Native Baraka Liu voice must select Kung only if solo Kung and native bank is resident");
        }
    }
    reset();definitions({0x530DACE6});kernel::W32(0x505970,15);
    kernel::W32(0x5ACEBC,0x3500);kernel::W16(0x5D38B6,1);
    native_audio(0x457D44DD); // Different, real Liu projectile comment
    check(sounds.back()[2]==0x457D44DD,
          "Baraka Kung voice fix must never rewrite unrelated Liu dialogue requests");
    // Typed animation: buffer 6935F9CD at frame 1300, PLAY the same tag at 1450. The audio
    // becomes Kung, but the slot tag stays Liu so PLAY/DELETE find the handle.
    auto stream_sound = [&](uint32_t sound) {
        kernel::W32(0x1004,sound);
        X86Context ctx{};ctx.esp=0x1000;
        sub_0017C040(ctx,memory.data());
        check(ctx.esp==0x1004 && kernel::R32(0x1004)==sound,
              "Baraka animation stream fix must preserve retail call stack and sound argument");
    };
    for (int scenario=0; scenario<6; ++scenario) {
        reset();definitions(scenario==4 ? std::vector<uint32_t>{} :
                            std::vector<uint32_t>{0x530DACE6});
        kernel::W32(0x505970,scenario==2 ? 16 : 15);
        kernel::W32(0x5ACEBC,0x3500);
        kernel::W32(0x3588,scenario==1 ? 1 : 0);
        kernel::W16(0x5D38B6,scenario==3 ? 0 : 1);
        if (scenario==5) settings.restore_dialogue=false;
        stream_sound(0x6935F9CD);
        check(streams.size()==1 && streams.back()==(scenario==0 ? 0x530DACE6u : 0x6935F9CDu),
              "Original Baraka animation should buffer Kung only in valid solo Kung context");
        check(kernel::R32(0x69654C)==0x6935F9CD && kernel::R32(0x696550)==0x5000,
              "The original play/delete tag must always address the successfully buffered native audio");
    }
    reset();definitions({0x530DACE6});
    kernel::W32(0x505970,15);kernel::W32(0x5ACEBC,0x3500);
    kernel::W16(0x5D38B6,1);accept_stream=false;
    stream_sound(0x6935F9CD);
    check(!kernel::R32(0x69654C) && !kernel::R32(0x696550),
          "Failed Baraka animation buffer must not create a fake ready stream or corrupt another slot");
    accept_stream=true;
    stream_sound(0x12345678);
    check(streams.back()==0x12345678 && kernel::R32(0x69654C)==0x12345678,
          "An unrelated animation stream must remain untouched by the Baraka correction");
    reset(); definitions({0x24E7FCEE}); kernel::W32(0x505970,160); variation_seed=1;
    native_audio(0x12F19BF6);
    check(sounds.back()[2]==0x24E7FCEE,"Scorpion's authored spear voice must be able to use his existing come-here take");
    settings.restore_dialogue=false; native_audio(0x12F19BF6);
    check(sounds.back()[2]==0x12F19BF6,"Disabled recovery must preserve Scorpion's original spear recording");
    reset(); definitions({0x24E7FCEE}); kernel::W32(0x505970,160); variation_seed=1; accept_audio=false;
    check(native_audio(0x12F19BF6)==0,"A failed native audio request must preserve its failure result");
    accept_audio=true; native_audio(0x12F19BF6);
    check(sounds.back()[2]==0x24E7FCEE,"An unsuccessful spear request must not consume its recovery selection");
    reset(); stream_bank(0xDB4300AA,{0xCF260E2D,0x888674FD,0xB5E65D4D,0x07C6815D,0x3AA6A8ED});
    kernel::W32(0x3118,6); kernel::W32(0x4000+5*28,0x7D06D23D); kernel::W32(0x505970,160);
    for (unsigned i=0;i<6;++i) native_audio(0xDB4300AA,true);
    check(sounds.back()[2]==0x7D06D23D,"The sixth original Scorpion fire take must be reachable through the actual banked voice request");
    check(kernel::R32(0x3208)==5 && kernel::R32(0x3210)==0xCF260E2D,
          "A temporary native variation override must restore the original group size and first choice");
    reset(); stream_bank(0xFEDCADEA,{0x12345678}); definitions({0xAE718768,0x9311AED8,0x213172C8});
    kernel::W32(0x300C,0x3200);kernel::W32(0x505970,73);
    native_audio(0xFEDCADEA,true);
    check(sounds.back()[2]==0xAE718768,"Shao Kahn's first actual battle-taunt request must permit an original challenge take");
    native_audio(0xFEDCADEA,true);
    check(sounds.back()[2]==0x12345678,"An introductory challenge must not replace every later Shao Kahn taunt");
    kernel::W16(0x5D38B6,0);kernel::W32(0x5D1AB8+0x1E04,12); variation_seed=1;
    native_audio(0xFEDCADEA,true);
    check(sounds.back()[2]==0x213172C8,"Shao Kahn's get-up instruction must require a protagonist hit reaction at an actual taunt cue");
    for (const bool observed_frame : {false, true}) {
        reset(); kernel::W32(0x505970, 101); play(0x12344321);
        activate(101, 0xC1CE6AC6, 0);
        if (observed_frame) tick();
        finish(); tick();
        check(sounds.size() == 1 && sounds[0][2] == 0x7CE0B357,
              "The current scene's authored continuation must survive completion or a skip before its first update");
    }
    reset(); definitions({0x14DC5B86});
    kernel::W16(0x5D38B6 + 0x3238, 68); kernel::W32(0x5D1AB8 + 0x3238 + 0x68, 0xE123);
    activate(108, 0x5A3297F2u, 0, 1, 13); play(0x12344321); tick();
    finish(); kernel::W32(0x6C03DC, 1); tick();
    play(0x12344321); finish(); kernel::W32(0x6C03DC, 0); tick(); tick();
    check(sounds.empty(), "A later cinematic must cancel an old Ermac phase request even with the same scene name");
    activate(108, 0x5A3297F2u, 0, 1, 13); tick();
    check(sounds.empty(), "Repeating a canceled Ermac activation must not resurrect the stale phase voice");

    reset(); definitions({0x804E23CE, 0x85897096}); kernel::W32(0x505970, 195); three_sisters();
    forest_caption(45, 0x12345678, 0x5000); tick();
    play(0x12344321); kernel::G<uint8_t>(0x4E74E0) = 0; finish(); tick();
    mileena_anim(1764); mileena_anim(1700); tick();
    check(sounds.empty(), "Canceling Mileena's opening must not count as delivery and enable its follow-up taunts");

    reset(); definitions({0x29314E8B, 0x14DC5B86}); kernel::W32(0x505970, 108);
    kernel::W16(0x5D38B6 + 0x3238, 68); kernel::W32(0x5D1AB8 + 0x3238 + 0x68, 0xE123);
    play(0x12344321); activate(108, 0x5A3297F2u, 0, 1, 13);
    kernel::W32(0x69654C, 0x800DFF7A); kernel::W32(0x696554, 0x5000);
    native_stream_playing = true; tick();
    native_stream_playing = false; finish(); kernel::W32(0x6C03DC, 1); tick(); tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(8100)); tick();
    kernel::W32(0x6C03DC, 0); tick(); kernel::W32(0x6C03DC, 0); tick();
    check(std::none_of(sounds.begin(), sounds.end(), [](const auto& sound) { return sound[2] == 0x29314E8B; }),
          "A shrine continuation held outside the queue too long after its scene must expire");
    check(std::none_of(sounds.begin(), sounds.end(), [](const auto& sound) { return sound[2] == 0x14DC5B86; }),
          "An Ermac phase request held outside the queue too long after its scene must expire");

    reset(); definitions({0x64DC7FC9}); kernel::W32(0x505970, 96); kernel::W16(0x5D38B6, 1);
    kernel::W32(0x514508, 0x7922A984); kernel::W32(0x4EF42C, 0);
    voice(0, 0xF3D35938); voice(0, 0x5873A43A); tick();
    native_stream_playing = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(8100)); tick();
    check(sounds.empty(), "A long introduction must let Raiden finish before Kung Lao's response");
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1 && sounds[0][2] == 0x5873A43A,
          "The valid introduction response must survive a long localized recording");
    kernel::W32(0x6C03DC, 0); tick();
    check(sounds.size() == 1, "The intro response no longer carries the Wu Shi 2_02 extra");

    reset(); definitions({0x64DC7FC9}); kernel::W32(0x505970, 96); kernel::W16(0x5D38B6, 1);
    kernel::W32(0x514508, 0x7922A984); voice(0, 0xF3D35938);
    play(0x18797C6C); finish(); kernel::W32(0x6C03DC, 0); tick();
    voice(0, 0x65138D8A); native_stream_playing = true;
    voice(0, 0x5873A43A); tutorial();
    kernel::W32(0x6C03DC, 0); native_stream_playing = false; tick(); tick();
    check(messages.size() == 1 && sounds.empty(),
          "The attack panel must follow only its own line; a late intro response and its extra must be discarded");
    kernel::W32(0x6C03DC, 0); voice(0, 0x5873A43A); tick();
    check(last_native_sound == 0 && sounds.empty(),
          "An intro response emitted after a scene change must not play or arm a new extra");

    reset(); kernel::W32(0x505970, 96); kernel::W16(0x5D38B6, 1); kernel::W32(0x6C03DC, 1);
    kernel::W32(0x6820A0 + 0xA8C0, 194); kernel::W32(0x6820A0 + 180 * 0xD8 + 0xC, 0x4000);
    voice(0, 0xD733519A, 0x871111EC, 0xFFFFFFFF, 0x4000);
    activate(96, 0xACC1E66A, 1, 0, 180); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "The native cancellation of a quake warning must expire its queued voice");

    reset(); definitions({0x2AD7B0C5}); kernel::W32(0x6C03DC, 1);
    activate(96, 0x30A4AFE0, 1); voice(0, 0x59BC5679);
    activate(96, 0x7D27A93F, 1, 1, 161); kernel::W32(0x6C03DC, 0); tick();
    check(sounds.empty(), "Starting the next arena phase must discard pending instructions from before the gate");
    activate(96, 0x30A4AFE0, 1); voice(0, 0x59BC5679); tick();
    check(sounds.empty() && last_native_sound == 0,
          "Late gate-phase requests must not reappear after progression to the next arena");

    reset(); play(); tick(); finish(); kernel::W32(0x6C03DC, 1); tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(8100)); tick();
    kernel::W32(0x6C03DC, 0); tick();
    check(messages.empty(), "An unplayed Cage caption must expire instead of starting long after the scene");
    return failures ? 1 : 0;
}
