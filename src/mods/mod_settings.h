#pragma once
#include <filesystem>
#include <string_view>
#include "../texture_pack_source.h"
// What the user chose for a mod, kept by the mod's id in settings.toml beside the registry. It is not part of the mod's
// files, so installing another version of the mod, or removing it and installing it again, keeps the choices.
namespace openshao::mods {
// High when nothing was chosen, or the file cannot be read
texpack::Quality ReadTextureQuality(const std::filesystem::path& root,std::string_view id);
void WriteTextureQuality(const std::filesystem::path& root,std::string_view id,texpack::Quality quality);
}
