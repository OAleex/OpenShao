#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace openshao::paths {
struct LegacyFolder {
    std::filesystem::path previous, current;
};
// Earlier default folders are recognized by content (game selection, preferences, mods, settings).
bool IsDataFolder(const std::filesystem::path& folder);
// A saves folder holds console saves of the supported game, identified by its Xbox title ID.
bool IsSavesFolder(const std::filesystem::path& folder);
// The single matching folder in `parent` other than `current`, if `current` does not exist.
std::filesystem::path FindPreviousFolder(const std::filesystem::path& parent, const std::filesystem::path& current,
                                         const std::function<bool(const std::filesystem::path&)>& matches);
// Renames previous default folders when the current one is missing; returns the folders to use.
std::vector<std::filesystem::path> AdoptLegacyFolders(const std::vector<LegacyFolder>& folders, bool move,
                                                      std::string& warning);
// Points stored paths inside a renamed folder at its new location. Other values stay unchanged.
bool RewriteLegacyPaths(const std::filesystem::path& file, const std::vector<LegacyFolder>& folders,
                        std::string& error);
}
