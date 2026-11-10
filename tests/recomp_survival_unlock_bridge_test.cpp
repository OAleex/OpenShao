#include "survival_unlock.h"
#include "file_ops.h"
#include "restart.h"
#include "runtime_paths.h"
#include "core/log.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
fs::path profile, fallback;
bool fail_write=false;
unsigned failures=0;
void Check(bool okay,const char* reason) {
    if (!okay) { ++failures; std::fprintf(stderr,"FAIL: %s\n",reason); }
}
}
namespace openshao::restart {
const fs::path& ActiveSaveDirectory() { return profile; }
}
namespace openshao::paths {
const fs::path& SaveRoot() { return fallback; }
}
namespace openshao::files {
void AtomicWrite(const fs::path& target,std::string_view text) {
    if (fail_write) throw std::runtime_error("simulated full disk");
    fs::create_directories(target.parent_path());
    std::ofstream file(target,std::ios::binary|std::ios::trunc);
    file.write(text.data(),text.size());
    if (!file.flush()) throw std::runtime_error("simulated file write failure");
}
}
namespace logx { void Write(Level,const char*,...) {} }

int main() {
    using namespace openshao::survival::unlock;
    const auto root=fs::temp_directory_path()/
        ("openshao-survival-unlock-bridge-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(fs::create_directory(root),"test owns its unique temp folder");
    fallback=root;
    profile=root/"ProfileA";fs::create_directories(profile);
    Check(!Available() && !Persisted(),"fresh profile is locked");
    Check(Discover()==Encounter::New && Available() && Pending(),"portal discovery is immediately usable but pending save");
    NativeSaveCompleted(true,true);
    Check(!Persisted(),"successful Options write without a native slot copy-out does not unlock globally");
    NativeSaveSlotPrepared();NativeSaveCompleted(false,true);
    Check(!Persisted(),"failed native slot save does not persist the unlock");
    NativeSaveSlotPrepared();NativeSaveCompleted(true,false);
    Check(!Persisted(),"unexpected write callsite cannot commit a pending unlock");
    fail_write=true;NativeSaveSlotPrepared();NativeSaveCompleted(true,true);
    Check(Pending() && !Persisted(),"filesystem failure leaves pending unlock available and not permanent");
    fail_write=false;NativeSaveSlotPrepared();NativeSaveCompleted(true,true);
    Check(Persisted() && !Pending() && fs::exists(profile/State::FileName),
          "confirmed native Save Game writes the shared marker outside console save slots");
    Check(Discover()==Encounter::AlreadySaved,"repeat portal shows the permanent already-unlocked state");
    // Switching save slots and save pages does not change the global profile root.
    Check(Available(),"changing saves within one profile cannot relock the mode");
    profile=root/"ProfileB";fs::create_directories(profile);
    Check(!Available() && Discover()==Encounter::New,"a different saves root starts locked");
    CampaignLoaded();
    Check(!Available(),"loading another campaign without saving cancels the session-only unlock");
    profile=root/"ProfileA";
    Check(Persisted() && Discover()==Encounter::AlreadySaved,
          "returning to previously saved profile restores the permanent unlock");
    fs::remove_all(root);
    std::printf("Survival unlock bridge: %u failures\n",failures);
    return failures?1:0;
}
