#pragma once
#include "catalog.h"
#include "progress.h"
#include <filesystem>
#include <functional>
#include <memory>
namespace openshao::mods {
struct Installed { Manifest mod; bool enabled=true; };
std::vector<Installed> ReadInstalled(const std::filesystem::path& root);
void ValidateEnabled(const std::vector<Installed>& installed);
void ValidateRoutines(const std::filesystem::path& root,const std::vector<Installed>& installed,
                      std::string_view staged_id={},const std::filesystem::path& staged_folder={});
std::vector<Installed> ActiveMods(const std::filesystem::path& root,std::string_view language);
std::filesystem::path EnabledLanguagePack(const std::filesystem::path& root,std::string_view language);
struct Package {
    Manifest mod;
    std::filesystem::path folder;
    std::filesystem::path ostp_file;  // a texture pack file is checked where it is and copied once, not unpacked
    ~Package();
};
using Original=std::function<std::vector<uint8_t>(const Patch&)>;
std::shared_ptr<Package> InspectPackage(const std::filesystem::path& archive,const std::filesystem::path& root,const Progress& progress={});
void InstallPackage(const std::filesystem::path& root,const Package& package,const Original& original,const Progress& progress={});
void SetEnabled(const std::filesystem::path& root,std::string_view id,bool enabled);
void Uninstall(const std::filesystem::path& root,std::string_view id);
void Recover(const std::filesystem::path& root);
}
