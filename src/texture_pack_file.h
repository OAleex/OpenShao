#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "texture_pack_source.h"

// A texture pack in the single file format: a header, the pack's information, an optional logo, an index sorted by
// texture hash and the BC blocks of every texture, largest level first. Open checks the whole structure before anything
// from the file is used.
namespace openshao::texpack {
struct FileEntry {
    uint64_t hash = 0;  // of the level 0 bytes of the game's own texture
    uint16_t width = 0, height = 0;            // level 0 of the stored texture
    uint16_t orig_width = 0, orig_height = 0;  // the game's texture
    uint8_t levels = 0;
    BcFormat format = BcFormat::Bc3;
    bool alpha = false;
    uint64_t data_offset = 0, data_size = 0;  // inside the data section
};

struct PackInfo {
    std::string id, name, version, summary, min_recomp;
    std::string description, website, license, credits;
    std::vector<std::string> authors, features;
    std::vector<uint64_t> preload;  // hashes of the replacements to have ready before the menus draw them (version 1.1)
};

struct Logo {
    uint16_t width = 0, height = 0;
    std::vector<uint8_t> rgba;  // width * height * 4, top row first
};

// Bytes of one level of a stored texture
uint64_t LevelSize(uint32_t width, uint32_t height, uint32_t level, BcFormat format);
// The first level to use: all of them for High, and for Low the ones that leave the texture at twice the game's size
uint32_t FirstLevel(const FileEntry& entry, Quality quality);

class PackFile;
// The pack as the loader reads it. With Low it starts every texture at the level that leaves it at twice the game's size.
std::shared_ptr<const PackSource> AsSource(std::shared_ptr<const PackFile> pack, Quality quality = Quality::High);

class PackFile {
public:
    // Null with the reason in `error` when the file is not a valid pack
    static std::unique_ptr<PackFile> Open(const std::filesystem::path& path, std::string& error);

    const PackInfo& info() const { return info_; }
    // Absent when the pack has none or the one it has is not valid
    const std::optional<Logo>& logo() const { return logo_; }
    const std::vector<FileEntry>& entries() const { return entries_; }
    const FileEntry* Find(uint64_t hash) const;
    // Reads the BC blocks of the levels from `first_level` on into `out`. Safe to call from several threads.
    bool ReadLevels(const FileEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const;

private:
    PackFile() = default;
    std::filesystem::path path_;
    uint64_t file_size_ = 0, data_offset_ = 0, data_size_ = 0;
    PackInfo info_;
    std::optional<Logo> logo_;
    std::vector<FileEntry> entries_;
};
}
