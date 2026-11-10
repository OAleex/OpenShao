#include "survival_unlock.h"
#include "file_ops.h"
#include "restart.h"
#include "runtime_paths.h"
#include "core/log.h"

namespace openshao::survival::unlock {
namespace {
State state;
bool native_save_slot_prepared=false;
State& Current() {
    const auto& active = restart::ActiveSaveDirectory();
    state.SetRoot(active.empty() ? paths::SaveRoot() : active);
    return state;
}
}
bool Available() { return Current().Available(); }
bool Persisted() { return Current().Persisted(); }
bool Pending() { return Current().Pending(); }
Encounter Discover() { return Current().Discover(); }
void CampaignLoaded() { Current().DiscardPending(); native_save_slot_prepared=false; }
void NativeSaveSlotPrepared() { native_save_slot_prepared=true; }
void NativeSaveCompleted(bool successful, bool from_story_save) {
    const bool confirmed=StorySaveConfirmed(native_save_slot_prepared,from_story_save,successful);
    native_save_slot_prepared=false;
    if (!confirmed || !Current().Pending()) return;
    if (Current().ConfirmSave([](const auto& path, std::string_view value) { files::AtomicWrite(path, value); }))
        LOGI("Survival unlocked globally after successful native story save");
    else
        LOGW("Survival unlock remains pending: could not persist global unlock after saving");
}
}
