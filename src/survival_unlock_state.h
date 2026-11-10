#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>

namespace openshao::survival::unlock {
enum class Encounter { New, PendingSave, AlreadySaved };

// Story saves stay intact; a native Save Game confirms this sidecar for all slots and pages.
class State {
public:
    static constexpr std::string_view Signature = "OpenShao Survival Unlocked v1\n";
    static constexpr std::string_view FileName = "openshao-survival-unlock.dat";

    void SetRoot(const std::filesystem::path& root) {
        const auto next = root.empty() ? std::filesystem::path{} : root / FileName;
        if (path_ == next) return;
        path_ = next;
        persisted_ = pending_ = false;
        if (path_.empty()) return;
        std::ifstream file(path_, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() != static_cast<std::streamoff>(Signature.size())) return;
        file.seekg(0);
        std::string actual(Signature.size(), '\0');
        if (file.read(actual.data(), static_cast<std::streamsize>(actual.size())) && actual == Signature)
            persisted_ = true;
    }
    Encounter Discover() {
        if (persisted_) return Encounter::AlreadySaved;
        if (pending_) return Encounter::PendingSave;
        pending_ = true;
        return Encounter::New;
    }
    bool Available() const { return persisted_ || pending_; }
    bool Persisted() const { return persisted_; }
    bool Pending() const { return pending_; }
    void DiscardPending() { pending_ = false; }

    // A failed sidecar write keeps the unlock pending.
    bool ConfirmSave(const std::function<void(const std::filesystem::path&, std::string_view)>& write) {
        if (persisted_) return true;
        if (!pending_ || path_.empty()) return false;
        try { write(path_, Signature); }
        catch (...) { return false; }
        persisted_ = true;
        pending_ = false;
        return true;
    }
    const std::filesystem::path& Path() const { return path_; }
private:
    std::filesystem::path path_;
    bool persisted_ = false, pending_ = false;
};

// Foundry (rooms 64..95) portal to Survival 1 (room 161); never the Survival chain itself.
inline constexpr bool FoundrySurvivalPortal(uint32_t room, uint32_t destination, bool active_survival) {
    return !active_survival && room / 32 == 2 && destination == 161;
}
// Options can also write save.dat: a real Save Game copies a campaign slot out first.
inline constexpr bool StorySaveConfirmed(bool slot_prepared, bool native_write,
                                         bool succeeded) {
    return slot_prepared && native_write && succeeded;
}
}
