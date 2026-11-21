#include "texture_pack.h"
#include "core/log.h"
#include <algorithm>
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

std::mutex lock;
std::shared_ptr<const Index> current;

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

void SetPacks(const std::vector<std::filesystem::path>& folders) {
    auto index = std::make_shared<Index>();
    for (const auto& folder : folders) ReadIndex(folder, *index);
    if (!folders.empty()) LOGI("Texture packs: %zu replacements from %zu folders", index->size(), folders.size());
    std::lock_guard<std::mutex> guard(lock);
    current = index->empty() ? nullptr : std::move(index);
}

namespace {
std::shared_ptr<const Index> Current() {
    std::lock_guard<std::mutex> guard(lock);
    return current;
}
}

bool Enabled() { return Current() != nullptr; }

std::optional<Entry> Find(uint64_t hash) {
    const auto index = Current();
    if (!index) return std::nullopt;
    const auto it = index->find(hash);
    if (it == index->end()) return std::nullopt;
    return it->second.entry;
}

uint32_t LevelBytes(uint32_t w, uint32_t h, uint32_t level) {
    const uint32_t lw = std::max(1u, w >> level), lh = std::max(1u, h >> level);
    return ((lw + 3) / 4) * ((lh + 3) / 4) * 16;
}

bool Load(uint64_t hash, std::vector<uint8_t>& blocks) {
    const auto index = Current();
    if (!index) return false;
    const auto it = index->find(hash);
    if (it == index->end()) return false;
    const Entry& e = it->second.entry;
    const std::string name = it->second.file.string();
    std::ifstream in(it->second.file, std::ios::binary);
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
    size_t total = 0;
    for (uint32_t l = 0; l < e.levels; l++) total += LevelBytes(e.w, e.h, l);
    blocks.resize(total);
    if (!in.read(reinterpret_cast<char*>(blocks.data()), std::streamsize(total))) {
        LOGE("Texture pack: %s is shorter than its index says", name.c_str());
        return false;
    }
    return true;
}
}
