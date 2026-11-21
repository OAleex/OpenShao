#include "texture_dev_folder.h"
#include "mods/json.h"
#include "mods/sha256.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "../third_party/stb/stb_image.h"

namespace openshao::texpack {
namespace {
constexpr uint32_t kMinSide = 4, kMaxSide = 8192;
constexpr size_t kMaxProblems = 40;

struct Name {
    uint64_t hash = 0;
    uint32_t width = 0, height = 0;  // of the game's own texture
};

bool Hex(char c, unsigned& v) {
    if (c >= '0' && c <= '9') v = unsigned(c - '0');
    else if (c >= 'a' && c <= 'f') v = unsigned(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v = unsigned(c - 'A' + 10);
    else return false;
    return true;
}

// <16 hexadecimal digits>_<format>_<width>x<height>.png
bool ParseName(const std::string& file, Name& out) {
    if (file.size() < 6 || file.size() > 120) return false;
    const std::string ext = file.substr(file.size() - 4);
    if (ext != ".png" && ext != ".PNG") return false;
    const std::string stem = file.substr(0, file.size() - 4);
    if (stem.size() <= 17 || stem[16] != '_') return false;
    uint64_t hash = 0;
    for (size_t i = 0; i < 16; ++i) {
        unsigned v;
        if (!Hex(stem[i], v)) return false;
        hash = (hash << 4) | v;
    }
    const size_t second = stem.find('_', 17);
    if (second == std::string::npos || second == 17) return false;
    for (size_t i = 17; i < second; ++i)
        if (!((stem[i] >= '0' && stem[i] <= '9') || (stem[i] >= 'A' && stem[i] <= 'Z') || (stem[i] >= 'a' && stem[i] <= 'z'))) return false;
    const size_t x = stem.find('x', second + 1);
    if (x == std::string::npos || x == second + 1 || x + 1 >= stem.size()) return false;
    uint64_t w = 0, h = 0;
    for (size_t i = second + 1; i < x; ++i) {
        if (stem[i] < '0' || stem[i] > '9') return false;
        w = w * 10 + uint64_t(stem[i] - '0');
        if (w > 100000) return false;
    }
    for (size_t i = x + 1; i < stem.size(); ++i) {
        if (stem[i] < '0' || stem[i] > '9') return false;
        h = h * 10 + uint64_t(stem[i] - '0');
        if (h > 100000) return false;
    }
    if (!w || !h) return false;
    out = {hash, uint32_t(w), uint32_t(h)};
    return true;
}

bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = in.tellg();
    if (size <= 0 || size > (512ll << 20)) return false;
    bytes.resize(size_t(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size));
    return bool(in);
}

struct Picture {
    int width = 0, height = 0;
    std::unique_ptr<uint8_t, void (*)(void*)> pixels{nullptr, [](void* p) { stbi_image_free(p); }};
};

bool Decode(const std::vector<uint8_t>& bytes, Picture& out, std::string& why) {
    int w = 0, h = 0, channels = 0;
    uint8_t* px = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
    if (!px) {
        why = stbi_failure_reason() ? stbi_failure_reason() : "cannot be decoded";
        return false;
    }
    out.width = w;
    out.height = h;
    out.pixels.reset(px);
    return true;
}

uint32_t LevelCount(uint32_t w, uint32_t h) {
    uint32_t n = 1, side = std::max(w, h);
    while (side > 1) {
        side >>= 1;
        ++n;
    }
    return n;
}

struct Entry {
    std::filesystem::path path;
    uint32_t width = 0, height = 0;  // of the edited picture
    uint32_t original_width = 0, original_height = 0;
    std::string file;
};

class DevFolderSource final : public PackSource {
public:
    explicit DevFolderSource(std::unordered_map<uint64_t, Entry> entries) : entries_(std::move(entries)) {}
    std::optional<SourceEntry> Find(uint64_t hash) const override {
        const auto it = entries_.find(hash);
        if (it == entries_.end()) return std::nullopt;
        const Entry& e = it->second;
        return SourceEntry{hash, uint16_t(e.width), uint16_t(e.height), uint8_t(LevelCount(e.width, e.height)), BcFormat::Rgba8, 0, 0};
    }
    // Decodes the picture and makes the levels: each is the 2 x 2 average of the one before, in 8 bits a channel
    bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const override {
        const auto it = entries_.find(entry.hash);
        if (it == entries_.end() || first_level >= entry.levels) return false;
        std::vector<uint8_t> bytes;
        Picture pic;
        std::string why;
        if (!ReadFile(it->second.path, bytes) || !Decode(bytes, pic, why)) return false;
        if (uint32_t(pic.width) != it->second.width || uint32_t(pic.height) != it->second.height) return false;  // the file changed since it was looked at
        uint32_t w = it->second.width, h = it->second.height;
        std::vector<uint8_t> level(pic.pixels.get(), pic.pixels.get() + size_t(w) * h * 4), next;
        out.clear();
        for (uint32_t l = 0; l < entry.levels; ++l) {
            if (l >= first_level) out.insert(out.end(), level.begin(), level.end());
            if (l + 1 == entry.levels) break;
            const uint32_t nw = std::max(1u, w >> 1), nh = std::max(1u, h >> 1);
            next.assign(size_t(nw) * nh * 4, 0);
            for (uint32_t y = 0; y < nh; ++y)
                for (uint32_t x = 0; x < nw; ++x) {
                    // the 2 x 2 of the level above; where a side has only one row or column, that one counts twice
                    const uint32_t x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
                    const uint32_t y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
                    for (int c = 0; c < 4; ++c) {
                        const unsigned sum = level[(size_t(y0) * w + x0) * 4 + c] + level[(size_t(y0) * w + x1) * 4 + c] +
                                             level[(size_t(y1) * w + x0) * 4 + c] + level[(size_t(y1) * w + x1) * 4 + c];
                        next[(size_t(y) * nw + x) * 4 + c] = uint8_t((sum + 2) / 4);
                    }
                }
            level.swap(next);
            w = nw;
            h = nh;
        }
        return true;
    }
    size_t Count() const override { return entries_.size(); }

private:
    std::unordered_map<uint64_t, Entry> entries_;
};
}

std::string PixelSignature(uint32_t width, uint32_t height, const uint8_t* rgba) {
    std::vector<uint8_t> data;
    const std::string head = std::to_string(width) + "x" + std::to_string(height) + ":";
    data.reserve(head.size() + size_t(width) * height * 4);
    data.insert(data.end(), head.begin(), head.end());
    data.insert(data.end(), rgba, rgba + size_t(width) * height * 4);
    return mods::Sha256(data);
}

std::shared_ptr<const PackSource> OpenDevFolder(const std::filesystem::path& folder, DevFolderReport& report, unsigned threads) {
    const auto start = std::chrono::steady_clock::now();
    report = {};
    const auto problem = [&](const std::string& text) {
        if (report.problems.size() < kMaxProblems) report.problems.push_back(text);
    };
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        problem("the folder " + folder.string() + " cannot be read");
        return nullptr;
    }

    // what the extraction recorded: the signature of each picture's pixels, by hash
    std::unordered_map<uint64_t, std::string> signatures;
    {
        std::vector<uint8_t> bytes;
        if (ReadFile(folder / "manifest.json", bytes)) {
            try {
                const mods::Json root = mods::Json::Parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
                for (const auto& [key, value] : std::get<mods::Json::Object>(root.value)) {
                    uint64_t hash = 0;
                    bool ok = key.size() == 16;
                    for (char c : key) {
                        unsigned v;
                        ok = ok && Hex(c, v);
                        if (ok) hash = (hash << 4) | v;
                    }
                    if (ok && value.Has("pixels")) signatures[hash] = value.At("pixels").Text();
                }
                report.manifest = true;
            } catch (const std::exception& e) {
                problem(std::string("manifest.json cannot be read: ") + e.what());
            }
        } else {
            problem("there is no manifest.json: every picture counts as edited");
        }
    }

    struct Work {
        std::filesystem::path path;
        std::string file;
        Name name;
    };
    std::vector<Work> work;
    std::vector<std::string> files;
    for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
        if (!item.is_regular_file(ec)) continue;
        files.push_back(item.path().filename().string());
    }
    std::sort(files.begin(), files.end());  // the same result whatever the order the system lists them in
    for (const auto& file : files) {
        Name name;
        if (!ParseName(file, name)) {
            if (file != "manifest.json" && file.find(".before-signatures") == std::string::npos) {
                ++report.ignored;
                problem("ignored: " + file + " is not named <hash>_<format>_<width>x<height>.png");
            }
            continue;
        }
        work.push_back({folder / file, file, name});
    }
    report.files = work.size();

    enum class Result { Edited, Unchanged, Refused };
    struct Outcome {
        Result result = Result::Refused;
        Entry entry;
        bool has_signature = true;
        std::string why;
    };
    std::vector<Outcome> outcomes(work.size());
    std::atomic<size_t> next{0};
    const auto worker = [&] {
        for (size_t i = next.fetch_add(1); i < work.size(); i = next.fetch_add(1)) {
            const Work& w = work[i];
            Outcome& o = outcomes[i];
            std::vector<uint8_t> bytes;
            Picture pic;
            if (!ReadFile(w.path, bytes)) {
                o.why = "cannot be read";
                continue;
            }
            if (!Decode(bytes, pic, o.why)) continue;
            const uint32_t pw = uint32_t(pic.width), ph = uint32_t(pic.height);
            if (pw < kMinSide || ph < kMinSide || pw > kMaxSide || ph > kMaxSide) {
                o.why = "is " + std::to_string(pw) + "x" + std::to_string(ph) + ", a side must be from " + std::to_string(kMinSide) + " to " + std::to_string(kMaxSide);
                continue;
            }
            if (uint64_t(pw) * w.name.height != uint64_t(ph) * w.name.width) {
                o.why = "is " + std::to_string(pw) + "x" + std::to_string(ph) + ", not in the proportion of the original " + std::to_string(w.name.width) + "x" + std::to_string(w.name.height);
                continue;
            }
            const auto sig = signatures.find(w.name.hash);
            o.has_signature = sig != signatures.end();
            o.entry = {w.path, pw, ph, w.name.width, w.name.height, w.file};
            o.result = o.has_signature && PixelSignature(pw, ph, pic.pixels.get()) == sig->second ? Result::Unchanged : Result::Edited;
        }
    };
    unsigned count = threads ? threads : std::max(1u, std::thread::hardware_concurrency());
    count = std::min<unsigned>(count, unsigned(std::max<size_t>(work.size(), 1)));
    std::vector<std::thread> pool;
    for (unsigned t = 1; t < count; ++t) pool.emplace_back(worker);
    worker();
    for (auto& t : pool) t.join();

    std::unordered_map<uint64_t, Entry> edited;
    for (size_t i = 0; i < work.size(); ++i) {
        const Outcome& o = outcomes[i];
        if (o.result == Result::Refused) {
            ++report.refused;
            problem("refused: " + work[i].file + " " + o.why);
        } else if (o.result == Result::Unchanged) {
            ++report.unchanged;
        } else {
            auto [it, added] = edited.emplace(work[i].name.hash, o.entry);  // the files are in name order: the first one stays
            if (!added) {
                ++report.duplicates;
                problem("duplicate: " + work[i].file + " has the hash of " + it->second.file + ", which is used");
            } else {
                ++report.edited;
                if (!o.has_signature) ++report.unsigned_files;
            }
        }
    }
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (edited.empty()) return nullptr;
    return std::make_shared<DevFolderSource>(std::move(edited));
}
}
