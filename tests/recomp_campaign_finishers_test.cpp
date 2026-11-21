#include "campaign_finishers.h"
#include "scorpion_preview_audio.h"
#include "kernel/kernel.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t Room = 0x505970, StatePointer = 0x5ACEBC;
constexpr uint32_t State = 0x720000, World = 0x639224;
constexpr uint32_t Players = 0x64D230, P1 = 0x5D1AB8, P2 = 0x5D4CF0;
constexpr uint32_t Level = State + 0x4928, Charge = State + 0x4904;
constexpr uint32_t Stack = 0x8000, Trigger = 0x740000, Definition = 0x741000;
constexpr uint32_t Action = 0x742000, ActionData = 0x743000, Callback = 0x744000;
constexpr uint32_t Vtable = 0x745000, Condition = 0x746000, Manager = 0x747000;
constexpr uint32_t ActionArgs = 0x749000;
constexpr uint32_t Clock = 0x5CE878, Squads = 0x6C0168;
constexpr uint32_t CurrentMessage = 0x4E74E0, Cinematic = 0x6C4198;
std::array<uint8_t, 8 << 20> memory{};
unsigned failed = 0, notices = 0, label = 0, native_actions = 0;
unsigned voice_calls = 0, voice_bank = 0, voice_index = 0;
std::array<uint32_t, 4> posted_messages{};
size_t posted_count = 0;
std::array<uint32_t, 3> observed_frames{};
bool survival_active = false;
bool voice_playing = false;

void Check(bool result, const char* message) {
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", message); ++failed; }
}
void Name(const char* name) {
    std::memset(kernel::HostPtr(Definition + 0xAC), 0, 16);
    std::memcpy(kernel::HostPtr(Definition + 0xAC), name, std::strlen(name));
}
void Setup(unsigned room, unsigned first, unsigned second = 0xFFFF) {
    openshao::campaign_finishers::Reset();
    memory.fill(0);
    notices = label = native_actions = 0;
    posted_count = 0;
    voice_calls = voice_bank = voice_index = 0;
    survival_active = false;
    voice_playing = false;
    kernel::W32(World, 0x700000);
    kernel::W32(Room, room);
    kernel::W32(StatePointer, State);
    kernel::W32(State + 0x88, second != 0xFFFF);
    kernel::W32(State + 0xCC, first);
    kernel::W32(State + 0xD0, second);
    kernel::W32(Players, P1);
    kernel::W32(Players + 12, second == 0xFFFF ? 0 : P2);
    kernel::W32(P1 + 0x68, 1);
    kernel::W32(P2 + 0x68, 2);
    kernel::W16(P1 + 0x1DFE, uint16_t(first));
    kernel::W16(P2 + 0x1DFE, uint16_t(second));
    kernel::W32(Level + 0x50, 1);
    kernel::W32(Level + 0xA0, 1);
    kernel::W32(Charge + 0x50, 0x41200000);
    kernel::W32(Trigger + 0xC, Definition);
    kernel::W32(Condition + 8, Trigger);
    kernel::W32(Action + 0x10, ActionData);
    kernel::W32(Action + 8, ActionArgs);
    kernel::W32(ActionData + 0x10, Callback);
    kernel::W32(Callback, Vtable);
    kernel::W32(Clock, 100);
    kernel::W32(0x58A624, Manager);
}
}

X86_EXTERN_FUNC(sub_0016F560);
X86_EXTERN_FUNC(sub_001F2E90);
X86_EXTERN_FUNC(sub_00013F85);
X86_EXTERN_FUNC(sub_000166B3);
X86_EXTERN_FUNC(sub_001DED00);
X86_EXTERN_FUNC(sub_00014903);
X86_EXTERN_FUNC(sub_0016E0E0);

namespace {
X86Context Context(uint32_t object) {
    X86Context ctx{};
    ctx.esp = Stack;
    ctx.ecx = object;
    return ctx;
}
void Pickup(const char* name) {
    Name(name);
    auto ctx = Context(Trigger);
    sub_0016F560(ctx, memory.data());
}
void Dispatch(const char* name, uint32_t routine) {
    Name(name);
    kernel::W32(Vtable, routine);
    kernel::W32(Stack + 8, Definition);
    kernel::W32(Stack + 12, Trigger);
    auto ctx = Context(Action);
    sub_001F2E90(ctx, memory.data());
    Check(ctx.esp == Stack + 16, "dispatcher preserves callee cleanup");
}
void NativeNotice(const char* name, uint32_t text) {
    kernel::W32(ActionArgs, text);
    kernel::W32(ActionArgs + 4, 6);
    Dispatch(name, 0x12847);
}
bool ConditionResult(const char* name, unsigned arg, X86Func* function) {
    Name(name);
    kernel::W32(Condition + 4, arg);
    auto ctx = Context(Condition);
    function(ctx, memory.data());
    Check(ctx.esp == Stack + 12, "condition preserves callee cleanup");
    return (ctx.eax & 255) == 1;
}
void Frame() {
    auto ctx = Context(0);
    openshao::campaign_finishers::Update(ctx);
}
void Complete(unsigned tier, unsigned owner) {
    kernel::W32(0x58E4BC, owner);
    kernel::W32(Manager + 0x1A4, kernel::R32(Clock));
    kernel::W32(Manager + 0x1A8, tier);
}
void DeadSquad(unsigned entry, unsigned group) {
    kernel::W16(Squads + entry * 12, uint16_t(group));
    kernel::W32(Squads + entry * 12 + 4, 0);
}
void Spawn(const char* name, unsigned group) {
    kernel::W32(ActionArgs, group);
    Dispatch(name, 0x1D2710);
}
}

namespace mem {
bool IsCommitted(uint32_t address) { return address >= 0x10000 && address < memory.size(); }
}
namespace kernel {
uint8_t* g_base = memory.data();
uint32_t CallGuest(X86Context& ctx, uint32_t function, const uint32_t* args, size_t count) {
    if (function == 0x14903 && count == 2) W32(Level + R32(args[0] + 0x68) * 0x50, args[1]);
    if (function == 0x1F2E90 && count == 3) {
        const auto esp = ctx.esp;
        for (size_t i = 0; i < count; ++i) W32(esp + uint32_t((i + 1) * 4), args[i]);
        sub_001F2E90(ctx, memory.data());
        ctx.esp = esp;
    }
    return 0;
}
}
namespace openshao::survival {
bool ActiveSession() { return survival_active; }
void ShowStoryNotice(X86Context&, uint32_t text) {
    ++notices;
    label = text;
    if (posted_count < posted_messages.size()) posted_messages[posted_count++] = text;
}
}
namespace openshao::scorpion::audio_preview {
bool PlayCampaignVoice(unsigned bank, unsigned index) {
    ++voice_calls;
    voice_bank = bank;
    voice_index = index;
    return true;
}
void StopCampaignVoice() {}
bool CampaignVoicePlaying() { return voice_playing; }
}
X86_FUNC(__imp__sub_0016F560) { kernel::Return(ctx, 1, 1); }
X86_FUNC(__imp__sub_0016E0E0) {
    const auto frames = kernel::R32(ctx.ecx + 0xC4);
    for (uint32_t i = 0; i < kernel::R32(ctx.ecx + 0x14) && i < observed_frames.size(); ++i)
        observed_frames[i] = kernel::R32(frames + i * 4);
    kernel::Return(ctx, 1, 1);
}
X86_WEAK_FUNC(sub_0016E0E0, __imp__sub_0016E0E0)
X86_FUNC(__imp__sub_0016F020) { kernel::Return(ctx, 0, 0); }
X86_FUNC(__imp__sub_001F2E90) {
    ++native_actions;
    if (kernel::R32(Vtable) == 0x12847 && posted_count < posted_messages.size())
        posted_messages[posted_count++] = kernel::R32(ActionArgs);
    if (kernel::R32(Vtable) == 0x1116D) {
        kernel::W32(Level + 0x50, kernel::R32(Level + 0x50) + 1);
        if (kernel::R32(State + 0x88) == 1)
            kernel::W32(Level + 0xA0, kernel::R32(Level + 0xA0) + 1);
    }
    kernel::Return(ctx, 0, 3);
}
X86_FUNC(__imp__sub_00013F85) { kernel::Return(ctx, 0, 2); }
X86_FUNC(__imp__sub_00013382) { kernel::Return(ctx, 1, 2); }
X86_FUNC(__imp__sub_000166B3) {
    kernel::Return(ctx, kernel::R32(Manager + 0x1A4) >= 10 &&
        kernel::R32(Clock) - kernel::R32(Manager + 0x1A4) < 10 &&
        kernel::R32(Manager + 0x1A8) == kernel::R32(Condition + 4), 2);
}
X86_FUNC(__imp__sub_001DED00) { kernel::Return(ctx, 1, 2); }
X86_FUNC(__imp__sub_00014903) {
    kernel::W32(Level + kernel::R32(kernel::R32(Stack + 4) + 0x68) * 0x50, kernel::R32(Stack + 8));
    kernel::Return(ctx, 0, 0);
}

int main() {
    using namespace openshao::campaign_finishers;
    for (unsigned room : {194u, 142u}) {
        const char* pickup = room == 194 ? "_d_rdn_multal" : "_d_fatlvl3_b";
        const char* tutorial = room == 194 ? "_d_multal2" : "_d_fatlvl3_c";
        for (unsigned fighter : {0u, 1u, 82u, 88u}) {
            Setup(room, fighter);
            // A loaded actor can still reflect the previous selection.
            kernel::W16(P1 + 0x1DFE, 83);
            Pickup(pickup);
            Dispatch(pickup, 0x14985);
            Check(native_actions == 1,
                  "a saved story fighter retains its pickup scene even with a stale extra actor");
            Check(!ConditionResult(tutorial, 0, sub_00013F85),
                  "a saved story fighter still waits for its native pickup scene");

            Setup(room, 83);
            kernel::W16(P1 + 0x1DFE, uint16_t(fighter));
            Pickup(pickup);
            Dispatch(pickup, 0x14985);
            Check(native_actions == 1,
                  "a live story fighter retains its pickup scene even with a stale extra selection");

            Setup(room, 83);
            Pickup(pickup);
            kernel::W16(P1 + 0x1DFE, uint16_t(fighter));
            Dispatch(pickup, 0x14985);
            Check(native_actions == 1,
                  "an extra attempt cannot suppress a replacement story actor's scene");

            Setup(room, 83);
            Pickup(pickup);
            kernel::W32(State + 0xCC, fighter);
            kernel::W16(P1 + 0x1DFE, uint16_t(fighter));
            Dispatch(pickup, 0x14985);
            Check(native_actions == 1,
                  "reusing state and world allocations for a story fighter drops the extra scene override");
            Name(tutorial);
            constexpr uint32_t Frames = 0x74A000;
            kernel::W32(Trigger + 0x14, 2);
            kernel::W32(Trigger + 0xC4, Frames);
            kernel::W32(Frames, 0);
            kernel::W32(Frames + 4, 600);
            auto ctx = Context(Trigger);
            sub_0016E0E0(ctx, memory.data());
            Check(observed_frames[1] == 600,
                  "switching from an extra to a story fighter retains the full native scene timeline");
            Dispatch(tutorial, 0x1116D);
            Frame();
            Check(kernel::R32(Level + 0x50) == 2 && !notices,
                  "story pickups use the native award without the extra compartment notice");
        }
    }
    struct GateTake { unsigned fighter, bank, index; };
    for (const auto [fighter, bank, index] : {
             GateTake{83, 7081, 54}, GateTake{90, 7176, 11},
             GateTake{89, 7272, 21}, GateTake{84, 7214, 74}}) {
        Setup(194, fighter);
        Pickup("_d_rdn_multal");
        Dispatch("_d_multal2", 0x1116D);
        Spawn("_d_multal2", 16);
        DeadSquad(0, 16);
        Check(ConditionResult("_d_gate01_off", 2, sub_000166B3), "the Monastery gate is ready");
        Dispatch("_d_gate01_off", 0x1E12B0);
        Dispatch("_d_other_gate", 0x1995B0);
        Frame();
        Check(!voice_calls, "eligibility, collision removal and other gates do not play the line");
        Dispatch("_d_gate01_off", 0x1995B0);
        kernel::W32(0x6C03DC, 1);
        Frame();
        Check(!voice_calls, "the gate line waits for native speech to finish");
        kernel::W32(0x6C03DC, 0);
        Frame();
        Check(voice_calls == 1 && voice_bank == bank && voice_index == index,
              "the native gate animation plays the selected solo fighter's exact take");
        Dispatch("_d_gate01_off", 0x1995B0);
        Frame();
        Check(voice_calls == 1, "the gate line plays once per encounter");
    }
    for (unsigned partner : {84u, 0u}) {
        Setup(194, 83, partner);
        Pickup("_d_rdn_multal");
        Dispatch("_d_multal2", 0x1116D);
        Spawn("_d_multal2", 16);
        DeadSquad(0, 16);
        if (partner == 0) Complete(2, 1);
        Check(ConditionResult("_d_gate01_off", 2, sub_000166B3), "the Ko-op gate is ready");
        Dispatch("_d_gate01_off", 0x1995B0);
        Frame();
        Check(!voice_calls, "the gate line stays silent for both kinds of Ko-op parties");
    }
    Setup(194, 83);
    Dispatch("_d_gate01_off", 0x1995B0);
    Frame();
    Check(!voice_calls, "an already open gate without a pickup attempt does not play a line");

    Setup(142, 83);
    Pickup("_d_fatlvl3_b");
    Dispatch("_d_fatlvl3_c", 0x1116D);
    Spawn("_d_fatlvl3_c", 15);
    Spawn("_d_fatlvl3_c", 16);
    DeadSquad(0, 15);
    DeadSquad(1, 16);
    Check(ConditionResult("_d_complete", 3, sub_000166B3), "the Forest gate is ready");
    Dispatch("_d_complete", 0x1995B0);
    Frame();
    Check(!voice_calls, "the Forest Brutality gate does not play a line");
    for (unsigned room : {194u, 142u}) {
        for (unsigned party : {0u, 1u, 2u}) {
            Setup(room, 83, party == 0 ? 0xFFFF : party == 1 ? 89 : 0);
            Pickup(room == 194 ? "_d_rdn_multal" : "_d_fatlvl3_b");
            Name(room == 194 ? "_d_multal2" : "_d_fatlvl3_c");
            constexpr uint32_t Frames = 0x74A000;
            kernel::W32(Trigger + 0x14, 2);
            kernel::W32(Trigger + 0xC4, Frames);
            kernel::W32(Frames, 0);
            kernel::W32(Frames + 4, 600);
            auto ctx = Context(Trigger);
            sub_0016E0E0(ctx, memory.data());
            Check(observed_frames[1] == (party == 2 ? 600 : 30),
                  "extra-only tutorials schedule their wave after half a second while mixed parties use the original scene timeline");
            Check(kernel::R32(Frames) == 0 && kernel::R32(Frames + 4) == 600 && ctx.esp == Stack + 8,
                  "the scheduler restores the authored times and retains native cleanup");
            Name("_d_unrelated");
            ctx = Context(Trigger); sub_0016E0E0(ctx, memory.data());
            Check(observed_frames[1] == 600, "unrelated room triggers keep their own timing");
        }
    }
    for (unsigned room : {194u, 142u}) {
        const char* pickup = room == 194 ? "_d_rdn_multal" : "_d_fatlvl3_b";
        const char* tutorial = room == 194 ? "_d_multal2" : "_d_fatlvl3_c";
        for (unsigned partner : {0xFFFFu, 89u}) {
            Setup(room, 83, partner);
            Pickup(pickup);
            kernel::W32(ActionArgs, 1);
            kernel::W32(ActionArgs + 4, 0xD17C902E);
            Dispatch(tutorial, 0x1995B0);
            Check(!native_actions, "extra-only tutorials skip the dragon animation");
            Dispatch("_d_unrelated", 0x1995B0);
            kernel::W32(ActionArgs + 4, 0x957A69D6);
            Dispatch(tutorial, 0x1995B0);
            Check(native_actions == 2, "the dragon filter preserves other trigger and animation actions");
        }
        for (unsigned partner : {0xFFFFu, 83u}) {
            Setup(room, 0, partner);
            Pickup(pickup);
            kernel::W32(ActionArgs, 1);
            kernel::W32(ActionArgs + 4, 0xD17C902E);
            Dispatch(tutorial, 0x1995B0);
            Check(native_actions == 1, "campaign fighters retain the original dragon animation");
        }
    }
    Setup(194, 83);
    Check(!ConditionResult("_d_multal2", 0, sub_00013F85), "follow-up still waits for an actual pickup");
    Pickup("_d_rdn_multal");
    Dispatch("_d_rdn_multal", 0x14985);
    Check(!native_actions, "extra-only coin skips its pickup animation");
    Check(ConditionResult("_d_multal2", 0, sub_00013F85), "extra-only follow-up can run without an animation");
    Dispatch("_d_multal2", 0x1116D);
    Frame();
    Check(kernel::R32(Level + 0x50) == 2 && kernel::R32(Charge + 0x50) == 0x41200000,
          "the Monastery award adds capacity without adding charge");
    Check(notices == 1 && label == AcquiredLabel, "the Solo award uses the shared story notice");
    Dispatch("_d_multal2", 0x1116D);
    Frame();
    Check(notices == 1 && kernel::R32(Level + 0x50) == 2, "repeated dispatch cannot grant the compartment twice");
    Check(!ConditionResult("_d_respawn16", 16, sub_001DED00), "the tutorial respawn is disabled");
    Check(!ConditionResult("_d_gate01_off", 2, sub_000166B3), "the gate waits for the encounter's wave");
    DeadSquad(0, 16);
    Check(!ConditionResult("_d_gate01_off", 2, sub_000166B3), "an earlier squad record cannot release the gate");
    Spawn("_d_multal2", 16);
    constexpr uint32_t Priest = P2 + 0x3238, PriestHealth = 0x748000;
    kernel::W16(Priest + 0x1DFE, 27);
    kernel::W32(Priest + 0x200, PriestHealth);
    kernel::W32(PriestHealth + 0x368, 250);
    Check(!ConditionResult("_d_gate01_off", 2, sub_000166B3), "a living priest still blocks the Monastery gate");
    kernel::W32(PriestHealth + 0x368, 0);
    Check(ConditionResult("_d_gate01_off", 2, sub_000166B3), "the cleared wave releases the original Monastery gate");
    Reset();
    Check(!ConditionResult("_d_gate01_off", 2, sub_000166B3), "a checkpoint reload clears the encounter latch");

    Setup(142, 89, 90);
    Pickup("_d_fatlvl3_b");
    Dispatch("_d_fatlvl3_c", 0x1116D);
    Frame();
    Check(kernel::R32(Level + 0x50) == 3 && kernel::R32(Level + 0xA0) == 3 && notices == 1,
          "both extras receive three compartments through the Forest coin");
    Spawn("_d_fatlvl3_c", 15);
    Spawn("_d_fatlvl3_c", 16);
    DeadSquad(0, 15);
    Check(!ConditionResult("_d_complete", 3, sub_000166B3), "the Forest requires both encounter squads");
    DeadSquad(1, 16);
    Check(ConditionResult("_d_complete", 3, sub_000166B3) &&
          ConditionResult("_d_win05", 3, sub_000166B3), "the Forest releases the door and the Ko-op exit");

    for (unsigned partner : {0xFFFFu, 84u, 0u}) {
        Setup(142, 89, partner);
        constexpr uint32_t Frames = 0x74A000;
        constexpr std::array<uint32_t, 3> authored{0, 900, 902};
        kernel::W32(Trigger + 0x14, static_cast<uint32_t>(authored.size()));
        kernel::W32(Trigger + 0xC4, Frames);
        const auto observe = [&] {
            Name("_d_complete");
            for (uint32_t i = 0; i < authored.size(); ++i) kernel::W32(Frames + i * 4, authored[i]);
            auto ctx = Context(Trigger);
            sub_0016E0E0(ctx, memory.data());
            for (uint32_t i = 0; i < authored.size(); ++i)
                Check(kernel::R32(Frames + i * 4) == authored[i], "the door script retains its original schedule outside the tick");
        };
        observe();
        Check(observed_frames == authored, "the Forest door keeps its original timing before the coin");
        Pickup("_d_fatlvl3_b");
        Dispatch("_d_fatlvl3_c", 0x1116D);
        Spawn("_d_fatlvl3_c", 15);
        Spawn("_d_fatlvl3_c", 16);
        DeadSquad(0, 15);
        Check(!ConditionResult("_d_complete", 3, sub_000166B3), "the gate cannot open before the final squad is defeated");
        observe();
        Check(observed_frames == authored, "the gate keeps its original timing until both waves are defeated");
        DeadSquad(1, 16);
        const bool all_extras = partner != 0;
        Check(ConditionResult("_d_complete", 3, sub_000166B3) == all_extras,
              "clearing the final squad only replaces the Brutality prerequisite for extra-only parties");
        observe();
        Check(observed_frames == (all_extras ? std::array<uint32_t, 3>{0, 30, 32} : authored),
              "the Forest plays the door animation sooner and keeps the collision change two frames later");
    }

    for (unsigned room : {194u, 142u}) {
        const char* pickup = room == 194 ? "_d_rdn_multal" : "_d_fatlvl3_b";
        const char* tutorial = room == 194 ? "_d_multal2" : "_d_fatlvl3_c";
        const char* gate = room == 194 ? "_d_gate01_off" : "_d_complete";
        const unsigned tier = room == 194 ? 2 : 3, text = room == 194 ? 88 : 91;
        for (unsigned story : {0u, 1u}) {
            Setup(room, story ? 84 : 0, story ? 0 : 84);
            const auto extra_level = Level + (2 - story) * 0x50;
            kernel::W32(extra_level, tier - 1);
            Check(!BlocksTutorialWhite(1 - story), "the extra can use White before collecting the coin");
            Pickup(pickup);
            Check(BlocksTutorialWhite(1 - story) && !BlocksTutorialWhite(story) && !BlocksTutorialWhite(2),
                  "only the extra's White input is blocked in the mixed tutorial");
            Dispatch(pickup, 0x14985);
            Check(native_actions == 1, "mixed Ko-op keeps the original coin animation");
            Dispatch(tutorial, 0x1116D);
            NativeNotice("_d_unrelated", text);
            Check(kernel::R32(extra_level) == tier - 1 && notices == 0,
                  "an unrelated notice cannot grant capacity");
            posted_count = 0;
            kernel::G<uint8_t>(Cinematic) = 1;
            NativeNotice(tutorial, text);
            Check(kernel::R32(extra_level) == tier, "the extra gains capacity with the pickup notice");
            Frame();
            Check(!posted_count, "the pickup notices wait for the cinematic to finish");
            kernel::G<uint8_t>(Cinematic) = 0;
            Frame();
            Check(posted_count == 1 && posted_messages[0] == AllyAcquiredLabel,
                  "the ally frame is the first visible notice");
            kernel::G<uint8_t>(CurrentMessage) = 1;
            Frame();
            Check(posted_count == 1, "the native pickup notice waits for the ally frame to close");
            kernel::G<uint8_t>(CurrentMessage) = 0;
            Frame();
            Check(posted_count == 2 && posted_messages[0] == AllyAcquiredLabel && posted_messages[1] == text,
                  "closing the ally frame posts the native pickup notice");
            Check(!ConditionResult(gate, tier, sub_000166B3), "receiving capacity does not release the gate");
            Complete(tier, 1 - story);
            Check(!ConditionResult(gate, tier, sub_000166B3), "an extra cannot satisfy the story finisher");
            Check(BlocksTutorialWhite(1 - story), "the extra's White stays blocked while the gate requires the finisher");
            Complete(tier, story);
            Check(ConditionResult(gate, tier, sub_000166B3), "the story player's finisher satisfies the native gate");
            Check(!BlocksTutorialWhite(1 - story), "the extra can use White once the story finisher succeeds");
            Frame();
            Check(notices == 1, "the ally frame is not repeated after the finisher");
        }
        Setup(room, 0, 84);
        kernel::W32(Level + 0xA0, 3);
        Pickup(pickup);
        NativeNotice(tutorial, text);
        Check(kernel::R32(Level + 0xA0) == 3 && notices == 0 && posted_count == 1,
              "an ally who already owns that compartment receives only the native pickup notice");
        Check(BlocksTutorialWhite(1), "an already upgraded extra still cannot use White during the mixed encounter");
        kernel::W32(Room, 96);
        Check(!BlocksTutorialWhite(1), "leaving the tutorial room restores White");
        for (unsigned partner : {0xFFFFu, 89u}) {
            Setup(room, 83, partner);
            Pickup(pickup);
            Check(!BlocksTutorialWhite(0) && !BlocksTutorialWhite(1),
                  "Solo and extra-only parties keep ordinary Fatality input");
        }
    }

    // Start/Select lock during an extra's gate sequence.
    const auto frames = [](unsigned count) { for (unsigned i = 0; i < count; ++i) Frame(); };
    const auto clear_monastery = [] {
        Pickup("_d_rdn_multal");
        Dispatch("_d_multal2", 0x1116D);
        Spawn("_d_multal2", 16);
        DeadSquad(0, 16);
    };
    Check(PauseButtons(0x31, true) == 0x01 && PauseButtons(0x31, false) == 0x31,
          "the lock removes only Start and Select");
    for (unsigned partner : {0xFFFFu, 89u}) {
        Setup(194, 83, partner);
        Pickup("_d_rdn_multal");
        Dispatch("_d_multal2", 0x1116D);
        Spawn("_d_multal2", 16);
        Frame();
        Check(!PauseLocked(), "pause stays available while enemies remain");
        DeadSquad(0, 16);
        Frame();
        Check(PauseLocked(), "clearing the room locks pause before the gate can open");
        Check(ConditionResult("_d_gate01_off", 2, sub_000166B3), "the gate is ready");
        frames(100);
        Dispatch("_d_gate01_off", 0x1995B0);
        frames(179);
        Check(PauseLocked(), "pause stays locked during the gate animation");
        voice_playing = partner == 0xFFFF;
        frames(10);
        Check(PauseLocked() == (partner == 0xFFFF), "the Solo lock waits for the gate line");
        voice_playing = false;
        Frame();
        Check(!PauseLocked(), "pause returns once the gate and its line are finished");
        frames(5);
        Check(!PauseLocked(), "the lock does not return for the same encounter");
    }
    Setup(194, 83);
    clear_monastery();
    Frame();
    Check(ConditionResult("_d_gate01_off", 2, sub_000166B3), "the gate is ready");
    voice_playing = true;
    Dispatch("_d_gate01_off", 0x1995B0);
    frames(900);
    Check(!PauseLocked(), "a voice that never ends cannot keep pause locked");
    Setup(194, 83);
    clear_monastery();
    frames(1200);
    Check(PauseLocked(), "the lock waits for the gate to move");
    Frame();
    Check(!PauseLocked(), "a gate that never moves releases pause");
    Setup(194, 83);
    clear_monastery();
    Frame();
    Reset();
    Check(!PauseLocked(), "a reload clears the lock");
    for (unsigned story : {0u, 1u}) {
        Setup(142, story ? 84 : 0, story ? 0 : 84);
        Pickup("_d_fatlvl3_b");
        Dispatch("_d_fatlvl3_c", 0x1116D);
        Spawn("_d_fatlvl3_c", 15);
        Spawn("_d_fatlvl3_c", 16);
        DeadSquad(0, 15);
        DeadSquad(1, 16);
        Frame();
        Check(!PauseLocked(), "a mixed party keeps pause until the story finisher succeeds");
        Complete(3, story);
        Check(ConditionResult("_d_complete", 3, sub_000166B3), "the story Brutality opens the gate");
        Frame();
        Check(PauseLocked(), "a mixed party locks pause once the Brutality opens the gate");
        Dispatch("_d_complete", 0x1995B0);
        frames(181);
        Check(!PauseLocked(), "the Brutality gate releases pause after its animation");
    }
    Setup(194, 0);
    Pickup("_d_rdn_multal");
    DeadSquad(0, 16);
    frames(3);
    Check(!PauseLocked(), "story fighters keep the original pause behavior");

    Setup(194, 83);
    survival_active = true;
    Pickup("_d_rdn_multal");
    Dispatch("_d_rdn_multal", 0x14985);
    Check(native_actions == 1, "the campaign hooks do not change Survival");
    Frame();
    Check(!PauseLocked(), "Survival never locks pause");
    return failed != 0;
}
