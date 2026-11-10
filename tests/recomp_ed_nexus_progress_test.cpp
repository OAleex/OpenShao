#include "ed_nexus_progress.h"
#include <iostream>
int main() {
    using namespace openshao::ed_nexus;
    unsigned failures = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) { ++failures; std::cerr << text << '\n'; } };
    Progress saved{}, all{};
    saved[0] = 0x05; saved[1] = 0x80;
    all[0] = 0xFF; all[2] = 0x0F; all[0x33] = 0x01;
    const auto view = Unlock(saved, all);
    check(view[0] == 0xFF && view[1] == 0x80 && view[2] == 0x0F && view[0x33] == 0x01,
          "The session view must add every native unlock to the saved progress");
    check(Restore(saved, all, view) == saved, "Leaving the view must restore the exact saved progress");
    auto changed = view;
    changed[1] |= 0x01; changed[3] = 0x10;
    const auto kept = Restore(saved, all, changed);
    check(kept[0] == 0x05 && kept[1] == 0x81 && kept[2] == 0 && kept[3] == 0x10 && kept[0x33] == 0,
          "Progress earned while the view is active must survive, without the forced unlocks");
    check(Unlock(Restore(saved, all, changed), all) == Unlock(changed, all),
          "Saving inside a view and returning to it must not lose new progress");
    check(ProgressBytes == 13 * 4, "The block matches the 13 DWORDs the save routines copy");
    constexpr uint32_t Characters = 0x598DF0, Popup = 0x596810, Main = 0x598430;
    check(FreezeSurvivalCharacterCallbacks(true, false, Characters, Main),
          "Pending Survival difficulty must freeze the parent before the popup reaches the stack");
    check(FreezeSurvivalCharacterCallbacks(true, false, Popup, Characters) &&
          FreezeSurvivalCharacterCallbacks(false, true, Popup, Characters),
          "The underlying Versus callbacks remain frozen through the modal and Back transition");
    check(!FreezeSurvivalCharacterCallbacks(false, true, Characters, Main) &&
          !FreezeSurvivalCharacterCallbacks(true, false, Popup, Main) &&
          !FreezeSurvivalCharacterCallbacks(false, false, Popup, Characters) &&
          !FreezeSurvivalCharacterCallbacks(true, false, Main, 0),
          "Normal selection, unrelated popups and the root menu keep their native callbacks");
    return failures ? 1 : 0;
}
