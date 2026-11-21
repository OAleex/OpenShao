#include "mod_settings.h"
#include "../file_ops.h"
#include <sstream>
#include <string>
#include <toml++/toml.hpp>
namespace fs=std::filesystem;
namespace openshao::mods {
namespace {
toml::table Load(const fs::path& root) {
    try{if(fs::is_regular_file(root/"settings.toml"))return toml::parse_file(files::Utf8(root/"settings.toml"));}catch(...){}
    return {};
}
}
texpack::Quality ReadTextureQuality(const fs::path& root,std::string_view id) {
    const auto table=Load(root);
    const auto* mods=table["mods"].as_table();if(!mods)return texpack::Quality::High;
    const auto* mod=(*mods)[id].as_table();if(!mod)return texpack::Quality::High;
    return (*mod)["texture_quality"].value_or(std::string_view{})=="medium"?texpack::Quality::Medium:texpack::Quality::High;
}
void WriteTextureQuality(const fs::path& root,std::string_view id,texpack::Quality quality) {
    auto table=Load(root);
    if(!table["mods"].is_table())table.insert_or_assign("mods",toml::table{});
    auto& mods=*table["mods"].as_table();
    if(!mods[id].is_table())mods.insert_or_assign(id,toml::table{});
    mods[id].as_table()->insert_or_assign("texture_quality",quality==texpack::Quality::Medium?"medium":"high");
    std::ostringstream out;out<<toml::toml_formatter{table,toml::format_flags::none};
    fs::create_directories(root);
    files::AtomicWrite(root/"settings.toml",out.str());
}
}
