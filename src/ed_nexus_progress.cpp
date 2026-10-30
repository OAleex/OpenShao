#include "ed_nexus.h"
#include "ed_nexus_progress.h"
#include "kernel/kernel.h"
#include "core/log.h"

// Expose native unlock-all progress to readers while saves retain the player's progress.
namespace {
using namespace openshao::ed_nexus;

Progress unlocked_progress{};
bool prepared = false;
unsigned view_depth = 0;
Progress outer_progress{};

Progress ReadProgress() {
    Progress result{};
    for (uint32_t i = 0; i < ProgressBytes; ++i) result[i] = kernel::G<uint8_t>(ProgressBase + i);
    return result;
}

void WriteProgress(const Progress& value) {
    for (uint32_t i = 0; i < ProgressBytes; ++i) kernel::G<uint8_t>(ProgressBase + i) = value[i];
}

bool PrepareUnlockedProgress(X86Context& ctx) {
    if (prepared) return true;
    const auto saved = ReadProgress();
    const auto registers = ctx;
    // The unused native unlock-all routine (0xB67A0) needs the live progress block.
    // Capture its result once, then restore the player's progress.
    kernel::CallGuest(ctx, 0x12E5A, {});
    ctx = registers;
    unlocked_progress = ReadProgress();
    WriteProgress(saved);
    prepared = true;
    LOGI("Session unlock prepared from the native unlock-all routine");
    return true;
}

struct ScopedUnlockedProgress {
    bool active = false;
    Progress saved{};
    explicit ScopedUnlockedProgress(X86Context& ctx) {
        if (!KontentUnlocked() || !PrepareUnlockedProgress(ctx)) return;
        saved = ReadProgress();
        if (view_depth++ == 0) outer_progress = saved;
        WriteProgress(Unlock(saved, unlocked_progress));
        active = true;
    }
    ~ScopedUnlockedProgress() {
        if (!active) return;
        WriteProgress(Restore(saved, unlocked_progress, ReadProgress()));
        --view_depth;
    }
};

// Save the real progress even when serialization runs inside an unlocked reader scope.
struct ScopedSavedProgress {
    bool active = false;
    ScopedSavedProgress() {
        if (!view_depth) return;
        WriteProgress(Restore(outer_progress, unlocked_progress, ReadProgress()));
        active = true;
    }
    ~ScopedSavedProgress() {
        if (active) WriteProgress(Unlock(ReadProgress(), unlocked_progress));
    }
};
}

#define OPENSHAO_PROGRESS_READER(name) \
X86_EXTERN_FUNC(__imp__##name); \
X86_FUNC(name) { ScopedUnlockedProgress view(ctx); __imp__##name(ctx, base); }
OPENSHAO_PROGRESS_READER(sub_000132A6) // Single unlock bit.
OPENSHAO_PROGRESS_READER(sub_0001498F) // Unlock bit from 0x168.
OPENSHAO_PROGRESS_READER(sub_000117AD) // Unlocked item count.
OPENSHAO_PROGRESS_READER(sub_000127BB) // Character selection setup.
OPENSHAO_PROGRESS_READER(sub_00012829) // Frontend unlock list.
OPENSHAO_PROGRESS_READER(sub_000167DF) // Frontend unlock list.
OPENSHAO_PROGRESS_READER(sub_00015010) // Frontend menu, all-unlocked flag.
OPENSHAO_PROGRESS_READER(sub_000159E3) // Frontend menu, all-unlocked flag.
#undef OPENSHAO_PROGRESS_READER

#define OPENSHAO_PROGRESS_SAVE(name) \
X86_EXTERN_FUNC(__imp__##name); \
X86_FUNC(name) { ScopedSavedProgress persist; __imp__##name(ctx, base); }
OPENSHAO_PROGRESS_SAVE(sub_00013B57) // Save serializers (copy progress into a save buffer).
OPENSHAO_PROGRESS_SAVE(sub_0001566E)
OPENSHAO_PROGRESS_SAVE(sub_00015EF2)
OPENSHAO_PROGRESS_SAVE(sub_00012720)
OPENSHAO_PROGRESS_SAVE(sub_000166DB)
OPENSHAO_PROGRESS_SAVE(sub_00014291) // Saved-progress snapshot (0x59E640).
#undef OPENSHAO_PROGRESS_SAVE
