#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

// High resolution texture packs. A pack is the hd folder of an enabled mod: an index.txt and the DDS files it names.
// Each line of the index reads: hash, width, height, levels, file. The hash covers the level 0 bytes of the game's
// own texture, and the file holds BC3 blocks of every level, largest first.
namespace openshao::texpack {
struct Entry {
    uint32_t w = 0, h = 0, levels = 0;
};
// Replaces the packs in use. Each folder holds an index.txt. An empty list turns the replacement off.
void SetPacks(const std::vector<std::filesystem::path>& folders);
bool Enabled();
std::optional<Entry> Find(uint64_t hash);
// The BC3 blocks of every level of a replacement, read from the pack
bool Load(uint64_t hash, std::vector<uint8_t>& blocks);
// Bytes of one BC3 level
uint32_t LevelBytes(uint32_t w, uint32_t h, uint32_t level);
}
