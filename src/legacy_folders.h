#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace openshao::paths {
struct LegacyFolder {
    std::filesystem::path previous, current;
};
// Earlier releases used other default folder names. They are recognized by their contents, never by name.
// A data folder holds this project's game selection, folder preferences, installed mods or launcher settings.
bool IsDataFolder(const std::filesystem::path& folder);
// A saves folder holds console saves of the supported game, identified by its Xbox title ID.
bool IsSavesFolder(const std::filesystem::path& folder);
// The one folder directly inside `parent` that matches, other than `current`.
// Returns nothing when `current` exists, when nothing matches, or when several folders match.
std::filesystem::path FindPreviousFolder(const std::filesystem::path& parent, const std::filesystem::path& current,
                                         const std::function<bool(const std::filesystem::path&)>& matches);
// Renames each previous default folder when the current one does not exist yet.
// Returns the folders to use, in order: the current name, or the previous one when it cannot move.
// With move disabled, nothing changes on disk and an unmoved previous folder stays in use.
std::vector<std::filesystem::path> AdoptLegacyFolders(const std::vector<LegacyFolder>& folders, bool move,
                                                      std::string& warning);
// Points stored paths inside a renamed folder at its new location. Other values stay unchanged.
bool RewriteLegacyPaths(const std::filesystem::path& file, const std::vector<LegacyFolder>& folders,
                        std::string& error);
}
