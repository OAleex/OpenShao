#include "texture_pack.h"
#include "texture_pack_file.h"
#include "texture_dev_folder.h"
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
std::shared_ptr<const PackSource> current;  // the packs in use, in the order they were given
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
        return SourceEntry{hash, uint16_t(e.w), uint16_t(e.h), uint8_t(e.levels), BcFormat::Bc3, 0, 0};
    }
    bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const override {
        const auto it = set_->index.find(entry.hash);
        return it != set_->index.end() && ReadItem(it->second, first_level, out);
    }
    std::vector<uint64_t> Preload() const override { return set_->preload; }
    size_t Count() const override { return set_->index.size(); }

private:
    std::shared_ptr<const Set> set_;
};
}

namespace {
// Several packs as one: the first to have a texture gives it
class MergedSource final : public PackSource {
public:
    explicit MergedSource(std::vector<std::shared_ptr<const PackSource>> members) : members_(std::move(members)) {}
    std::optional<SourceEntry> Find(uint64_t hash) const override {
        for (size_t i = 0; i < members_.size(); ++i)
            if (auto entry = members_[i]->Find(hash)) {
                entry->origin = uint32_t(i);
                return entry;
            }
        return std::nullopt;
    }
    bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const override {
        return entry.origin < members_.size() && members_[entry.origin]->ReadLevels(entry, first_level, out);
    }
    std::vector<uint64_t> Preload() const override {
        std::vector<uint64_t> all;
        for (const auto& member : members_)
            for (uint64_t hash : member->Preload())
                if (std::find(all.begin(), all.end(), hash) == all.end()) all.push_back(hash);
        return all;
    }
    size_t Count() const override {
        size_t n = 0;
        for (const auto& member : members_) n += member->Count();
        return n;
    }

private:
    std::vector<std::shared_ptr<const PackSource>> members_;
};

std::shared_ptr<const PackSource> Current() {
    std::lock_guard<std::mutex> guard(lock);
    return current;
}
}

void SetPacks(const std::vector<PackSpec>& packs) {
    std::vector<std::shared_ptr<const PackSource>> members;
    for (const auto& spec : packs) {
        const auto& path = spec.path;
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec) && !std::filesystem::exists(path / "index.txt", ec)) {
            // no index: a folder of pictures, as the developer mode reads it; only what was edited counts
            DevFolderReport report;
            auto source = OpenDevFolder(path, report);
            // the path goes last: the log replaces a path, and what follows it, with a mark
            LOGI("Texture pack: developer folder: %zu pictures, %zu edited (%zu without a signature), %zu as extracted, %zu refused, "
                 "%zu ignored, %zu duplicates, looked at in %.2f s, folder %s",
                 report.files, report.edited, report.unsigned_files, report.unchanged, report.refused, report.ignored,
                 report.duplicates, report.seconds, path.string().c_str());
            for (const auto& line : report.problems) LOGI("Texture pack: developer folder: %s", line.c_str());
            if (source) members.push_back(std::move(source));
        } else if (std::filesystem::is_directory(path, ec)) {
            auto set = std::make_shared<Set>();
            ReadIndex(path, set->index);
            ReadPreload(path, *set);
            if (set->index.empty()) continue;
            LOGI("Texture pack: folder %s: %zu replacements, %zu to load ahead", path.string().c_str(), set->index.size(),
                 set->preload.size());
            members.push_back(std::make_shared<FolderSource>(std::move(set)));
        } else if (std::filesystem::is_regular_file(path, ec)) {
            std::string error;
            std::shared_ptr<const PackFile> file = PackFile::Open(path, error);
            if (!file) {
                LOGE("Texture pack: %s is not a valid pack file: %s", path.string().c_str(), error.c_str());
                continue;
            }
            LOGI("Texture pack: file: %zu replacements, %zu to load ahead, %s quality, file %s", file->entries().size(),
                 file->info().preload.size(), spec.quality == Quality::Medium ? "medium" : "high", path.string().c_str());
            members.push_back(AsSource(std::move(file), spec.quality));
        } else {
            LOGE("Texture pack: %s is neither a folder nor a file", path.string().c_str());
        }
    }
    std::shared_ptr<const PackSource> source;
    if (members.size() == 1) source = members.front();
    else if (members.size() > 1) source = std::make_shared<MergedSource>(std::move(members));
    if (source) LOGI("Texture packs: %zu replacements in %zu packs, %zu to load ahead", source->Count(), packs.size(), source->Preload().size());
    std::lock_guard<std::mutex> guard(lock);
    current = std::move(source);
    ++generation;
}

bool Enabled() { return Current() != nullptr; }

std::optional<Entry> Find(uint64_t hash) {
    const auto source = Current();
    const auto entry = source ? source->Find(hash) : std::nullopt;
    if (!entry) return std::nullopt;
    return Entry{entry->width, entry->height, entry->levels};
}

std::shared_ptr<const PackSource> Source() { return Current(); }

uint64_t Generation() { return generation.load(); }

bool Load(uint64_t hash, std::vector<uint8_t>& blocks) {
    const auto source = Current();
    const auto entry = source ? source->Find(hash) : std::nullopt;
    return entry && source->ReadLevels(*entry, 0, blocks);
}
}
