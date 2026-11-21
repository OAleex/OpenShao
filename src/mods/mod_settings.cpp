#include "mod_settings.h"
#include "../file_ops.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>
namespace fs=std::filesystem;
namespace openshao::mods {
namespace {
// Reading is forgiving: a file that cannot be parsed means nothing was chosen.
toml::table Load(const fs::path& root) {
    try{if(fs::is_regular_file(root/"settings.toml"))return toml::parse_file(files::Utf8(root/"settings.toml"));}catch(...){}
    return {};
}
// Writing is not: starting from an empty table would replace a file that was only damaged with one key, and every other choice would be lost.
toml::table LoadForWrite(const fs::path& root) {
    if(!fs::is_regular_file(root/"settings.toml"))return {};
    try{return toml::parse_file(files::Utf8(root/"settings.toml"));}
    catch(const std::exception& e){throw std::runtime_error(std::string("settings.toml cannot be read, so it is left as it is. Fix or remove it. ")+e.what());}
}
}
texpack::Quality ReadTextureQuality(const fs::path& root,std::string_view id) {
    const auto table=Load(root);
    const auto* mods=table["mods"].as_table();if(!mods)return texpack::Quality::High;
    const auto* mod=(*mods)[id].as_table();if(!mod)return texpack::Quality::High;
    return (*mod)["texture_quality"].value_or(std::string_view{})=="medium"?texpack::Quality::Medium:texpack::Quality::High;
}
void WriteTextureQuality(const fs::path& root,std::string_view id,texpack::Quality quality) {
    auto table=LoadForWrite(root);
    if(!table["mods"].is_table())table.insert_or_assign("mods",toml::table{});
    auto& mods=*table["mods"].as_table();
    if(!mods[id].is_table())mods.insert_or_assign(id,toml::table{});
    mods[id].as_table()->insert_or_assign("texture_quality",quality==texpack::Quality::Medium?"medium":"high");
    std::ostringstream out;out<<toml::toml_formatter{table,toml::format_flags::none};
    fs::create_directories(root);
    files::AtomicWrite(root/"settings.toml",out.str());
}
std::vector<std::string> TextureOrder(const fs::path& root,const std::vector<std::string>& ids) {
    std::vector<std::string> order;
    const auto add=[&](const std::string& id){
        if(std::find(ids.begin(),ids.end(),id)!=ids.end()&&std::find(order.begin(),order.end(),id)==order.end())order.push_back(id);
    };
    const auto table=Load(root);
    if(const auto* stored=table["textures"]["priority"].as_array())
        for(const auto& node:*stored)if(const auto id=node.value<std::string>())add(*id);
    for(const auto& id:ids)add(id);
    return order;
}
void WriteTextureOrder(const fs::path& root,const std::vector<std::string>& order) {
    auto table=LoadForWrite(root);
    toml::array list;for(const auto& id:order)list.push_back(id);
    if(!table["textures"].is_table())table.insert_or_assign("textures",toml::table{});
    table["textures"].as_table()->insert_or_assign("priority",std::move(list));
    std::ostringstream out;out<<toml::toml_formatter{table,toml::format_flags::none};
    fs::create_directories(root);
    files::AtomicWrite(root/"settings.toml",out.str());
}
}
