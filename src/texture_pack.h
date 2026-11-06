#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <memory>
#include <vector>
#include "texture_pack_source.h"

// High resolution texture packs. A pack is the hd folder of an enabled mod: an index.txt and the DDS files it names, or a
// texture pack file.
// Each line of the index reads: hash, width, height, levels, file. The hash covers the level 0 bytes of the game's
// own texture, and the file holds BC3 blocks of every level, largest first.
namespace openshao::texpack {
struct Entry {
    uint32_t w = 0, h = 0, levels = 0;
};
// Replaces the packs in use, the first being the one that wins when several have a texture. Each is a folder that holds an
// index.txt, or a texture pack file. One that cannot be read is skipped. An empty list turns the replacement off.
void SetPacks(const std::vector<std::filesystem::path>& packs);
bool Enabled();
std::optional<Entry> Find(uint64_t hash);
// The BC3 blocks of every level of a replacement, read from the pack
bool Load(uint64_t hash, std::vector<uint8_t>& blocks);
// Bytes of one BC3 level
uint32_t LevelBytes(uint32_t w, uint32_t h, uint32_t level);
// The packs in use as the loader reads them, null when there are none. The source does not change when SetPacks is called
// again; the generation does, so a reader knows to ask for the new one.
std::shared_ptr<const PackSource> Source();
uint64_t Generation();
}
