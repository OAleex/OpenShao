#pragma once
#include "manager.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include "mod_settings.h"
#include <mutex>
#include <optional>
#include <thread>
namespace openshao::mods {
// An installed mod that a package chosen by the user would replace: what to ask before it does
// Textures that two enabled texture pack files both have; the winner is the one that comes first in the order
struct Overlap {std::string winner,loser;size_t count=0;};
struct Replacement {
    std::string name,installed,incoming;
};
struct BrowserState {
    std::vector<Mod> catalog;
    std::vector<Installed> installed;
    std::string status,error;
    uint64_t done=0,total=0,revision=0;
    bool busy=false,updates=false,checking=false;
    std::map<std::string,texpack::Quality> quality;  // of the installed texture pack files, by mod id
    std::vector<std::string> order;    // the installed texture mods, the one that wins first
    std::vector<Overlap> overlaps;
    std::optional<Replacement> asking;  // set while an installation waits for the answer
};
class Browser {
    std::filesystem::path root_;
    mutable std::mutex mutex_;
    BrowserState state_;
    std::atomic<bool> cancel_{false};
    std::condition_variable answered_;
    std::optional<bool> answer_;
    std::jthread worker_;
    using Job=std::function<void()>;
    void Start(const Job& job,bool checking=false);
    Progress ProgressCallback();
    void Reload();
    bool Confirm(const Replacement& what);
public:
    explicit Browser(const std::filesystem::path& root);
    ~Browser();
    BrowserState View() const;
    void Refresh();
    // With `ask`, a package whose mod is already installed waits for Answer before it replaces it, and Cancel ends the wait
    void InstallFile(const std::filesystem::path& file,const std::filesystem::path& game,bool ask=false);
    void Answer(bool install);
    void Install(std::string id,const std::filesystem::path& game);
    void Enable(std::string id,bool enabled);
    void SetQuality(std::string id,texpack::Quality quality);
    // Moves a texture mod one place up (toward winning) or down in the order
    void MoveTexture(std::string id,bool up);
    void Remove(std::string id);
    void Cancel();
};
std::vector<Mod> DisplayMods(const BrowserState& state);
const Release* LatestCompatible(const Mod& mod);
}
