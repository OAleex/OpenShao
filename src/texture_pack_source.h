#pragma once
#include <cstdint>
#include <memory>
#include <cstddef>
#include <optional>
#include <vector>

// What the loader needs of a texture pack, whatever the way it is stored on disk.
namespace openshao::texpack {
enum class BcFormat : uint8_t { Bc1 = 1, Bc3 = 2 };
enum class Quality { High, Medium };

struct SourceEntry {
    uint64_t hash = 0;  // of the level 0 bytes of the game's own texture
    uint16_t width = 0, height = 0;  // level 0 of the stored texture
    uint8_t levels = 0;
    BcFormat format = BcFormat::Bc3;
    uint32_t medium_first_level = 0;  // the first level to use for the Medium quality
    uint32_t origin = 0;              // which of several packs gave it, for the one that combines them
};

class PackSource {
public:
    virtual ~PackSource() = default;
    virtual std::optional<SourceEntry> Find(uint64_t hash) const = 0;
    // The BC blocks of the levels from `first_level` on, every level tightly packed, the largest first. Safe to call
    // from several threads.
    virtual bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const = 0;
    // The textures the pack asks to have ready before anything draws them: those of the menus, in the order to load them
    virtual std::vector<uint64_t> Preload() const { return {}; }
    virtual size_t Count() const { return 0; }  // how many replacements it holds
};
}
