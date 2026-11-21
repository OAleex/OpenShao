#include "ed_nexus.h"
#include "ed_nexus_progress.h"
#include "fighter_selection.h"
#include "kernel/kernel.h"
#include "core/log.h"

// Expose native unlock-all progress to readers while saves retain the player's progress.
namespace {
using namespace openshao::ed_nexus;

Progress unlocked_progress{};
bool prepared = false;
unsigned view_depth = 0;
Progress outer_progress{};

bool FighterPopup(uint32_t object) {
    if(object!=0x598DF0)return false;
    if(openshao::fighter_selection::HintActive())return true;
    // Also frozen after Accept, while the roster stays on screen until the game starts.
    const bool difficulty_popup=openshao::fighter_selection::RosterBackdrop();
    const bool character_selection=openshao::fighter_selection::CharacterSelection();
    if(!difficulty_popup && !character_selection)return false;
    const auto depth=kernel::R32(0x6C9308);
    if(!depth || depth>=16)return false;
    const auto top=kernel::R32(0x6C9308+depth*4);
    const auto parent=depth>1?kernel::R32(0x6C9308+(depth-1)*4):0;
    return openshao::ed_nexus::FreezeSurvivalCharacterCallbacks(
        difficulty_popup,character_selection,top,parent);
}

// Keep the Versus portraits (mode 2) while the fighter screen or its popup is open.
void RestoreVersusPresentation(uint32_t mode) {
    const auto depth=kernel::R32(0x6C9308);
    const bool still_selecting=openshao::fighter_selection::CharacterSelection() && depth<16 &&
        kernel::R32(0x6C9308+depth*4)==0x598DF0;
    kernel::W32(0x598468,(still_selecting || openshao::fighter_selection::SetupPopup())?2:mode);
}

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
    // Run the unused unlock-all (0xB67A0) once on live progress, then restore it.
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
        // Survival uses saved Versus unlocks from its first frame (no temporary unlock-all).
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
OPENSHAO_PROGRESS_READER(sub_00012829) // Frontend unlock list.
OPENSHAO_PROGRESS_READER(sub_000167DF) // Frontend unlock list.
#undef OPENSHAO_PROGRESS_READER

X86_EXTERN_FUNC(__imp__sub_000127BB);
X86_FUNC(sub_000127BB) {
    ScopedUnlockedProgress view(ctx);
    __imp__sub_000127BB(ctx,base);
    // Shared Solo selection: keep the roster but neutralize the P2 highlight.
    if(!openshao::fighter_selection::CharacterSelection() || openshao::fighter_selection::PlayerMode()!=0)return;
    constexpr uint32_t Character=0x598DF0;
    const auto p1=kernel::R32(Character+0x3C),p2=kernel::R32(Character+0x40);
    if(p1>=8 || p2>=8)return;
    const auto state=Character+0x48+p2*12;
    const auto original=kernel::R32(state);
    if(p1==p2 && original==4)kernel::W32(state,2);
    else if(p1!=p2 && original==3)kernel::W32(state,1);
    else return;
    const auto saved=ctx;
    ctx.ecx=Character;
    kernel::CallGuest(ctx,0x11703,{});
    ctx=saved;
}

// Both selector callbacks must process the real campaign mode.
X86_EXTERN_FUNC(__imp__sub_000159E3);
X86_FUNC(sub_000159E3) {
    // Versus could advance to Level Select under the shared difficulty popup.
    if(FighterPopup(ctx.ecx)){kernel::Return(ctx,0,0);return;}
    ScopedUnlockedProgress view(ctx);
    const bool character=openshao::fighter_selection::CharacterSelection();
    const auto mode=character?openshao::fighter_selection::PlayerMode():0;
    if(character)kernel::W32(0x598468,mode);
    __imp__sub_000159E3(ctx,base);
    if(character)RestoreVersusPresentation(mode);
}

X86_EXTERN_FUNC(__imp__sub_00015010);
X86_EXTERN_FUNC(__imp__sub_00011703);
X86_FUNC(sub_00011703) {
    if(ctx.ecx!=0x598DF0){__imp__sub_00011703(ctx,base);return;}
    // Locked fighters use the native cursor clips while focused and the silhouette clip at rest.
    std::array<uint32_t,8> states{};
    unsigned locked=0;
    for(unsigned slot=0;slot<states.size();++slot){
        if(!openshao::fighter_selection::LockedPortrait(slot))continue;
        const auto address=0x598E38+slot*12;
        states[slot]=kernel::R32(address);
        if(states[slot]<2 || states[slot]>4)kernel::W32(address,0);
        locked|=1u<<slot;
    }
    __imp__sub_00011703(ctx,base);
    for(unsigned slot=0;slot<states.size();++slot)
        if(locked&(1u<<slot))kernel::W32(0x598E38+slot*12,states[slot]);
}
X86_FUNC(sub_00015010) {
    if(FighterPopup(ctx.ecx)){kernel::Return(ctx,0,2);return;}
    if(openshao::fighter_selection::LockedInput(ctx))return;
    ScopedUnlockedProgress view(ctx);
    const bool character=openshao::fighter_selection::CharacterSelection();
    const auto mode=character?openshao::fighter_selection::PlayerMode():0;
    unsigned locked=0;
    if(ctx.ecx==0x598DF0)for(unsigned slot=0;slot<8;++slot){
        if(!openshao::fighter_selection::LockedPortrait(slot))continue;
        const auto address=0x598E38+slot*12;
        if(!kernel::R32(address))kernel::W32(address,1);
        locked|=1u<<slot;
    }
    const bool expand=locked || (character && mode==0);
    const auto previous=expand?kernel::R32(0x58E4D0):0;
    if(character)kernel::W32(0x598468,mode);
    if(expand)kernel::W32(0x58E4D0,3);
    __imp__sub_00015010(ctx,base);
    for(unsigned slot=0;slot<8;++slot){
        const auto address=0x598E38+slot*12;
        if((locked&(1u<<slot)) && kernel::R32(address)==1)kernel::W32(address,0);
    }
    if(expand)kernel::W32(0x58E4D0,previous);
    if(character)RestoreVersusPresentation(mode);
}

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
