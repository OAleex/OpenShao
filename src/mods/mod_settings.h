#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "../texture_pack_source.h"
// What the user chose for a mod, kept by the mod's id in settings.toml beside the registry. It is not part of the mod's
// files, so installing another version of the mod, or removing it and installing it again, keeps the choices.
namespace openshao::mods {
// High when nothing was chosen, or the file cannot be read
texpack::Quality ReadTextureQuality(const std::filesystem::path& root,std::string_view id);
void WriteTextureQuality(const std::filesystem::path& root,std::string_view id,texpack::Quality quality);
// The texture mods from the one that wins to the one that gives way when several have the same texture. `ids` are the
// installed texture mods in the order they were installed: the user's order comes first, the mods it does not know follow in
// that order, and the ones it names that are no longer installed are left out.
std::vector<std::string> TextureOrder(const std::filesystem::path& root,const std::vector<std::string>& ids);
void WriteTextureOrder(const std::filesystem::path& root,const std::vector<std::string>& order);
}
