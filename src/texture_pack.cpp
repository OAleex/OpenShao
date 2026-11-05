#include "texture_pack.h"
#include "core/log.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace openshao::texpack {
namespace {
struct Item {
    Entry entry;
    std::filesystem::path file;
};
using Index = std::unordered_map<uint64_t, Item>;
// What the folders in use hold: the replacements, and the ones to load ahead
struct Set {
    Index index;
    std::vector<uint64_t> preload;
};

std::mutex lock;
std::shared_ptr<const Set> current;
std::atomic<uint64_t> generation{0};

bool SafeName(const std::string& name) {
    return !name.empty() && name.find_first_of("\\:") == std::string::npos && name.front() != '/' &&
           name.find("..") == std::string::npos;
}

bool Hex64(const std::string& text, uint64_t& value) {
    if (text.size() != 16 || text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
    value = std::strtoull(text.c_str(), nullptr, 16);
    return true;
}

void ReadIndex(const std::filesystem::path& folder, Index& index) {
    std::ifstream in(folder / "index.txt");
    if (!in) {
        LOGE("Texture pack: %s has no index.txt", folder.string().c_str());
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string hash, name;
        Item item;
        uint64_t key;
        if (!(fields >> hash >> item.entry.w >> item.entry.h >> item.entry.levels >> name) || !Hex64(hash, key)) continue;
        const Entry& e = item.entry;
        if (!SafeName(name) || !e.w || !e.h || (e.w & 3) || (e.h & 3) || e.w > 8192 || e.h > 8192 || !e.levels ||
            e.levels > 14) continue;
        item.file = folder / name;
        index.emplace(key, std::move(item));  // the first pack to name a texture wins
    }
}
}

namespace {
// preload.txt: one hash on each line, those of the textures to have ready before the menus draw them. Lines that start with
// # are comments. Only what the folders have a replacement for is kept, once, in the order given.
void ReadPreload(const std::filesystem::path& folder, Set& set) {
    std::ifstream in(folder / "preload.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string hash;
        uint64_t key;
        if (!(fields >> hash) || !Hex64(hash, key) || !set.index.count(key)) continue;
        if (std::find(set.preload.begin(), set.preload.end(), key) == set.preload.end()) set.preload.push_back(key);
    }
}
}

void SetPacks(const std::vector<std::filesystem::path>& folders) {
    auto set = std::make_shared<Set>();
    for (const auto& folder : folders) ReadIndex(folder, set->index);
    for (const auto& folder : folders) ReadPreload(folder, *set);
    if (!folders.empty())
        LOGI("Texture packs: %zu replacements from %zu folders, %zu to load ahead", set->index.size(), folders.size(),
             set->preload.size());
    std::lock_guard<std::mutex> guard(lock);
    current = set->index.empty() ? nullptr : std::move(set);
    ++generation;
}

namespace {
std::shared_ptr<const Set> Current() {
    std::lock_guard<std::mutex> guard(lock);
    return current;
}
}

bool Enabled() { return Current() != nullptr; }

std::optional<Entry> Find(uint64_t hash) {
    const auto set = Current();
    if (!set) return std::nullopt;
    const auto it = set->index.find(hash);
    if (it == set->index.end()) return std::nullopt;
    return it->second.entry;
}

uint32_t LevelBytes(uint32_t w, uint32_t h, uint32_t level) {
    const uint32_t lw = std::max(1u, w >> level), lh = std::max(1u, h >> level);
    return ((lw + 3) / 4) * ((lh + 3) / 4) * 16;
}

namespace {
// The blocks of a texture from `first_level` on, after checking the file against its line of the index
bool ReadItem(const Item& item, uint32_t first_level, std::vector<uint8_t>& blocks) {
    const Entry& e = item.entry;
    const std::string name = item.file.string();
    if (first_level >= e.levels) return false;
    std::ifstream in(item.file, std::ios::binary);
    uint8_t head[128];
    if (!in.read(reinterpret_cast<char*>(head), sizeof(head)) || std::memcmp(head, "DDS ", 4) ||
        std::memcmp(head + 84, "DXT5", 4)) {
        LOGE("Texture pack: %s is not a DXT5 DDS", name.c_str());
        return false;
    }
    uint32_t height, width;
    std::memcpy(&height, head + 12, 4);
    std::memcpy(&width, head + 16, 4);
    if (width != e.w || height != e.h) {
        LOGE("Texture pack: %s is %ux%u, the index says %ux%u", name.c_str(), width, height, e.w, e.h);
        return false;
    }
    size_t skipped = 0, total = 0;
    for (uint32_t l = 0; l < e.levels; l++) (l < first_level ? skipped : total) += LevelBytes(e.w, e.h, l);
    blocks.resize(total);
    in.seekg(std::streamoff(sizeof(head) + skipped));
    if (!in.read(reinterpret_cast<char*>(blocks.data()), std::streamsize(total))) {
        LOGE("Texture pack: %s is shorter than its index says", name.c_str());
        return false;
    }
    return true;
}

// A snapshot of the packs, for the loader's threads
class FolderSource final : public PackSource {
public:
    explicit FolderSource(std::shared_ptr<const Set> set) : set_(std::move(set)) {}
    std::optional<SourceEntry> Find(uint64_t hash) const override {
        const auto it = set_->index.find(hash);
        if (it == set_->index.end()) return std::nullopt;
        const Entry& e = it->second.entry;
        // the folders do not say how large the game's texture is, so the Medium quality is the High one
        return SourceEntry{hash, uint16_t(e.w), uint16_t(e.h), uint8_t(e.levels), BcFormat::Bc3, 0};
    }
    bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const override {
        const auto it = set_->index.find(entry.hash);
        return it != set_->index.end() && ReadItem(it->second, first_level, out);
    }
    std::vector<uint64_t> Preload() const override { return set_->preload; }

private:
    std::shared_ptr<const Set> set_;
};
}

std::shared_ptr<const PackSource> Source() {
    auto set = Current();
    return set ? std::make_shared<FolderSource>(std::move(set)) : nullptr;
}

uint64_t Generation() { return generation.load(); }

bool Load(uint64_t hash, std::vector<uint8_t>& blocks) {
    const auto set = Current();
    if (!set) return false;
    const auto it = set->index.find(hash);
    return it != set->index.end() && ReadItem(it->second, 0, blocks);
}
}
