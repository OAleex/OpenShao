#include "survival_unlock_state.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <cstdio>
#include <string>

namespace fs = std::filesystem;
using openshao::survival::unlock::State;
using openshao::survival::unlock::Encounter;
using openshao::survival::unlock::FoundrySurvivalPortal;
int main() {
    unsigned failures=0;
    auto check=[&](bool okay,const char* explanation){ if(!okay){++failures;std::fprintf(stderr,"FAIL: %s\n",explanation);} };
    const auto root=fs::temp_directory_path()/
        ("openshao-survival-unlock-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(fs::create_directory(root),"the unit fixture owns its unique temporary save folder");
    State state;state.SetRoot(root);
    check(!state.Available() && !state.Persisted(),"new game defaults to locked");
    check(state.Discover()==Encounter::New && state.Available() && state.Pending(),"first green portal discovers pending unlock");
    check(state.Discover()==Encounter::PendingSave,"repeat visit before saving reminds player to save");
    State abandoned;abandoned.SetRoot(root);abandoned.Discover();abandoned.DiscardPending();
    check(!abandoned.Available(),"loading another campaign without saving discards temporary unlock");
    const auto writer=[](const fs::path& file,std::string_view data){std::ofstream f(file,std::ios::binary);f.write(data.data(),data.size());if(!f)throw std::runtime_error("no write");};
    check(!state.ConfirmSave([](const auto&,std::string_view){throw std::runtime_error("disk full");}) && state.Pending(),"failed persistence retains unsaved state");
    check(!openshao::survival::unlock::StorySaveConfirmed(false,true,true) &&
          !openshao::survival::unlock::StorySaveConfirmed(true,false,true) &&
          !openshao::survival::unlock::StorySaveConfirmed(true,true,false) &&
          openshao::survival::unlock::StorySaveConfirmed(true,true,true),
          "only a successful native slot-copy-out Save Game can commit the global unlock; Options and failed writes cannot");
    State before_save;before_save.SetRoot(root);
    check(!before_save.Available(),"closing without saving forgets unlock");
    check(state.ConfirmSave(writer) && state.Persisted() && !state.Pending(),"successful normal save confirms global unlock");
    State another_slot;another_slot.SetRoot(root);
    check(another_slot.Available() && another_slot.Persisted() && another_slot.Discover()==Encounter::AlreadySaved,"loading another campaign in same save folder inherits the global unlock");
    const auto other=root/"different-profile";fs::create_directories(other);
    another_slot.SetRoot(other);
    check(!another_slot.Available(),"changing save directory cannot leak another profile's unlock");
    another_slot.SetRoot(root);
    check(another_slot.Persisted(),"switching back to saved profile restores global unlock");
    {std::ofstream invalid(root/State::FileName,std::ios::binary|std::ios::trunc);invalid<<"wrong";}
    State damaged;damaged.SetRoot(root);
    check(!damaged.Available(),"corrupt unlock file fails closed");
    check(FoundrySurvivalPortal(64,161,false) && FoundrySurvivalPortal(74,161,false),"Foundry survival portal is recognized");
    check(!FoundrySurvivalPortal(64,162,false) && !FoundrySurvivalPortal(161,161,false) &&
          !FoundrySurvivalPortal(63,161,false) && !FoundrySurvivalPortal(96,161,false) &&
          !FoundrySurvivalPortal(64,161,true),
          "other destinations, other worlds and active Survival do not trigger story unlock");
    fs::remove_all(root);
    std::printf("Survival unlock: %u failures\n",failures);
    return failures?1:0;
}
