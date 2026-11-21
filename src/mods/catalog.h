#pragma once
#include "json.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
namespace openshao::mods {
inline constexpr const char* CatalogUrl="https://raw.githubusercontent.com/OAleex/OpenShao-Mods/master/mods.json";
inline constexpr const char* GameSha256="48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a";
struct Patch {
    std::string path, source_hash, target_hash;
    std::string file, wad;
    uint64_t offset=0, size=0, record=0;
    bool range=false;
};
struct Payload { std::string path, sha256; uint64_t size=0; };
struct Release { std::string version,recomp,url,sha256; uint64_t size=0; };
struct Dependency { std::string id,version; };
struct Mod {
    std::string id,name,summary,type,language,install_dir;
    std::string language_name,unsupported_reason;
    std::vector<std::string> authors;
    std::vector<Release> versions;
    std::vector<Dependency> dependencies;
    std::vector<std::string> conflicts,features;
};
struct Manifest : Mod {
    std::string version,recomp,raw;
    unsigned schema=1;
    std::vector<Patch> patches;
    std::vector<Payload> files;
    std::vector<std::string> routines;
    // A texture pack in one file, in place of files and patches (schema 3)
    std::optional<Payload> ostp;
};
int CompareVersions(std::string_view a, std::string_view b);
std::vector<Mod> ParseCatalog(std::string_view json);
Manifest ParseManifest(std::string_view json);
}
