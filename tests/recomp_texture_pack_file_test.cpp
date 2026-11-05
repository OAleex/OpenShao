#include "texture_pack_file.h"
#include "mods/sha256.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace openshao;
using Bytes = std::vector<uint8_t>;

static void Check(bool ok, const std::string& text) { if (!ok) throw std::runtime_error(text); }

static void Put(Bytes& b, size_t at, uint64_t value, int size) {
    for (int i = 0; i < size; ++i) b[at + size_t(i)] = uint8_t(value >> (8 * i));
}
static uint64_t Get(const Bytes& b, size_t at, int size) {
    uint64_t v = 0;
    for (int i = 0; i < size; ++i) v |= uint64_t(b[at + size_t(i)]) << (8 * i);
    return v;
}
static void Pad(Bytes& b) { while (b.size() % 16) b.push_back(0); }

struct Tex {
    uint64_t hash;
    uint16_t w, h, ow, oh;
    uint8_t levels, format, flags, fill;
};

static const char* kInfo =
    R"({"id":"Test_Pack-1.0","name":"Test pack","version":"1.0.0","authors":["Someone","Another"],)"
    R"("summary":"A pack for the tests.","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture",)"
    R"("texture_count":3,"description":"Longer text.","website":"https://example.org"})";

static uint64_t Level(uint16_t w, uint16_t h, uint32_t l, uint8_t format) {
    return texpack::LevelSize(w, h, l, texpack::BcFormat(format));
}

// A valid pack of three textures and a 2 x 2 logo, laid out as the specification says
static Bytes Build(const std::vector<Tex>& textures, const std::string& info = kInfo, bool logo = true) {
    Bytes file(128, 0), meta(info.begin(), info.end());
    std::memcpy(file.data(), "OSTPACK\0", 8);
    Put(file, 8, 1, 2);
    Put(file, 10, 0, 2);
    Put(file, 12, 128, 4);
    Bytes logo_bytes;
    if (logo) {
        logo_bytes = {2, 0, 2, 0};
        for (int i = 0; i < 16; ++i) logo_bytes.push_back(uint8_t(i * 10));
    }
    Bytes index, data;
    for (const Tex& t : textures) {
        Bytes e(40, 0);
        Put(e, 0, t.hash, 8);
        Put(e, 8, t.w, 2);
        Put(e, 10, t.h, 2);
        Put(e, 12, t.ow, 2);
        Put(e, 14, t.oh, 2);
        e[16] = t.levels;
        e[17] = t.format;
        e[18] = t.flags;
        uint64_t size = 0;
        for (uint32_t l = 0; l < t.levels; ++l) {
            const uint64_t n = Level(t.w, t.h, l, t.format);
            data.insert(data.end(), size_t(n), uint8_t(t.fill + l));
            size += n;
        }
        Put(e, 24, data.size() - size, 8);
        Put(e, 32, size, 8);
        Pad(data);
        index.insert(index.end(), e.begin(), e.end());
    }
    auto place = [&](const Bytes& part, size_t field) {
        Pad(file);
        Put(file, field, file.size(), 8);
        Put(file, field + 8, part.size(), 8);
        file.insert(file.end(), part.begin(), part.end());
    };
    place(meta, 24);
    if (logo) place(logo_bytes, 40);
    place(index, 56);
    place(data, 72);
    Put(file, 88, textures.size(), 4);
    const std::string hash = mods::Sha256(index);
    for (size_t i = 0; i < 32; ++i) file[96 + i] = uint8_t(std::stoi(hash.substr(i * 2, 2), nullptr, 16));
    Put(file, 16, file.size(), 8);
    return file;
}

static void FixIndexHash(Bytes& file) {
    const size_t at = size_t(Get(file, 56, 8)), size = size_t(Get(file, 64, 8));
    const std::string hash = mods::Sha256(std::span<const uint8_t>(file.data() + at, size));
    for (size_t i = 0; i < 32; ++i) file[96 + i] = uint8_t(std::stoi(hash.substr(i * 2, 2), nullptr, 16));
}

static std::vector<Tex> Three() {
    return {{0x10, 64, 64, 16, 16, 7, 2, 1, 0x20},   // 4x of a 16 x 16 texture, with alpha
            {0x20, 32, 16, 32, 16, 6, 1, 0, 0x40},   // as large as the game's, BC1
            {0x30, 128, 128, 32, 32, 8, 2, 0, 0x60}};
}

static fs::path root;
static fs::path Write(const std::string& name, const Bytes& b) {
    const fs::path path = root / name;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    return path;
}

static std::string OpenError(const Bytes& b, const char* name = "bad.ostp") {
    std::string error;
    const auto pack = texpack::PackFile::Open(Write(name, b), error);
    Check(!pack, std::string("A pack that should be refused was opened: ") + name);
    Check(!error.empty(), "A refused pack says why");
    return error;
}

// The error of the pack after a change, which must name `part`
static void Refused(const char* what, const std::string& part, const std::function<void(Bytes&)>& change, bool fix_hash = false) {
    Bytes b = Build(Three());
    change(b);
    if (fix_hash) FixIndexHash(b);
    const std::string error = OpenError(b);
    Check(error.find(part) != std::string::npos, std::string(what) + ": the reason names \"" + part + "\" but says: " + error);
}

static size_t Entry(const Bytes& b, int i) { return size_t(Get(b, 56, 8)) + size_t(i) * 40; }

int main() try {
    root = fs::temp_directory_path() / "openshao-texpack-file-test";
    fs::remove_all(root);
    fs::create_directories(root);

    // A valid pack
    const Bytes good = Build(Three());
    std::string error;
    auto pack = texpack::PackFile::Open(Write("good.ostp", good), error);
    Check(pack != nullptr, "A valid pack opens: " + error);
    Check(pack->info().id == "Test_Pack-1.0" && pack->info().name == "Test pack" && pack->info().version == "1.0.0" &&
              pack->info().authors.size() == 2 && pack->info().min_recomp == "0.4.1" &&
              pack->info().description == "Longer text." && pack->info().website == "https://example.org",
          "The information is read");
    Check(pack->logo() && pack->logo()->width == 2 && pack->logo()->height == 2 && pack->logo()->rgba.size() == 16 &&
              pack->logo()->rgba[5] == 50,
          "The logo is read");
    Check(pack->entries().size() == 3 && pack->Find(0x20) && pack->Find(0x20)->format == texpack::BcFormat::Bc1 &&
              !pack->Find(0x20)->alpha && pack->Find(0x10)->alpha && pack->Find(0x10)->orig_width == 16,
          "The index is read");
    Check(!pack->Find(0x15) && !pack->Find(0) && !pack->Find(0xFFFF), "An unknown hash is not found");

    // Reading levels
    const auto* a = pack->Find(0x10);
    Bytes blocks;
    Check(pack->ReadLevels(*a, 0, blocks), "The levels are read");
    uint64_t all = 0;
    for (uint32_t l = 0; l < a->levels; ++l) all += Level(a->width, a->height, l, uint8_t(a->format));
    Check(blocks.size() == all && blocks.front() == 0x20 && blocks.back() == 0x26, "Every level, the largest first");
    Check(pack->ReadLevels(*a, 2, blocks) && blocks.size() == all - Level(64, 64, 0, 2) - Level(64, 64, 1, 2) &&
              blocks.front() == 0x22,
          "Reading from a later level skips the earlier ones");
    Check(!pack->ReadLevels(*a, 7, blocks) && !pack->ReadLevels(*a, 200, blocks), "A level the texture lacks is refused");

    // The quality picks the level
    Check(texpack::FirstLevel(*a, texpack::Quality::High) == 0, "High reads every level");
    Check(texpack::FirstLevel(*a, texpack::Quality::Medium) == 1, "Medium leaves 4x at 2x");
    Check(texpack::FirstLevel(*pack->Find(0x20), texpack::Quality::Medium) == 0, "A texture as large as the game's is not reduced");
    Check(texpack::FirstLevel(*pack->Find(0x30), texpack::Quality::Medium) == 1 &&
              texpack::FirstLevel(*pack->Find(0x30), texpack::Quality::High) == 0,
          "Another 4x texture");

    // Several threads read at once
    {
        std::vector<std::thread> threads;
        std::vector<int> ok(8, 0);
        for (int t = 0; t < 8; ++t)
            threads.emplace_back([&, t] {
                Bytes mine;
                bool all_ok = true;
                for (int i = 0; i < 200; ++i) {
                    const auto* e = pack->Find(t % 2 ? 0x30 : 0x10);
                    all_ok = all_ok && pack->ReadLevels(*e, 0, mine) && mine.front() == (t % 2 ? 0x60 : 0x20);
                }
                ok[size_t(t)] = all_ok;
            });
        for (auto& th : threads) th.join();
        for (int v : ok) Check(v, "Threads read the same pack at once");
    }

    // A pack without a logo, and with no textures
    Check(texpack::PackFile::Open(Write("nologo.ostp", Build(Three(), kInfo, false)), error) &&
              !texpack::PackFile::Open(Write("nologo.ostp", Build(Three(), kInfo, false)), error)->logo(),
          "A pack without a logo opens");
    {
        const std::string empty_info = R"({"id":"E","name":"Empty","version":"1.0.0","authors":["A"],"summary":"s",)"
                                       R"("min_recomp":"0.4.1","features":["hdtextures"],"type":"texture","texture_count":0})";
        const auto empty = texpack::PackFile::Open(Write("empty.ostp", Build({}, empty_info)), error);
        Check(empty && empty->entries().empty() && !empty->Find(1), "A pack with no textures opens: " + error);
    }

    // A logo that is not valid is dropped and the pack stays valid
    {
        Bytes b = Build(Three());
        Put(b, size_t(Get(b, 40, 8)), 300, 2);  // a width of 300
        const auto dropped = texpack::PackFile::Open(Write("badlogo.ostp", b), error);
        Check(dropped && !dropped->logo() && dropped->entries().size() == 3, "A logo with a size out of its limit is dropped");
        Bytes c = Build(Three());
        c[size_t(Get(c, 40, 8))] = 3;  // says 3 wide, but the pixels are for 2
        const auto mismatched = texpack::PackFile::Open(Write("badlogo2.ostp", c), error);
        Check(mismatched && !mismatched->logo(), "A logo whose size disagrees with its pixels is dropped");
    }

    // Header
    Refused("magic", "not a texture pack", [](Bytes& b) { b[0] = 'X'; });
    Refused("major version", "version", [](Bytes& b) { Put(b, 8, 2, 2); });
    Refused("header size", "size or flags", [](Bytes& b) { Put(b, 12, 64, 4); });
    Refused("flags", "size or flags", [](Bytes& b) { Put(b, 92, 1, 4); });
    Refused("file size", "file size", [](Bytes& b) { Put(b, 16, Get(b, 16, 8) + 16, 8); });
    Refused("truncated", "file size", [](Bytes& b) { b.resize(b.size() - 16); });
    {
        Bytes tiny(40, 0);
        Check(OpenError(tiny).find("shorter") != std::string::npos, "A file shorter than a header is refused");
        Check(OpenError(Bytes{}).find("shorter") != std::string::npos, "An empty file is refused");
        std::string e;
        Check(!texpack::PackFile::Open(root / "missing.ostp", e) && !e.empty(), "A missing file is refused");
    }

    // Sections
    Refused("section outside", "section", [](Bytes& b) { Put(b, 72, Get(b, 16, 8) + 16, 8); });
    Refused("section size overflow", "section", [](Bytes& b) { Put(b, 80, ~0ull, 8); });
    Refused("misaligned", "section", [](Bytes& b) { Put(b, 24, Get(b, 24, 8) + 1, 8); });
    Refused("overlap", "section", [](Bytes& b) { Put(b, 56, Get(b, 24, 8), 8); });
    Refused("information too large", "limit", [](Bytes& b) { Put(b, 32, (1u << 20) + 16, 8); });
    Refused("no information", "limit", [](Bytes& b) { Put(b, 32, 0, 8); });
    Refused("too many textures", "limit", [](Bytes& b) { Put(b, 88, 100001, 4); });
    Refused("index size", "limit", [](Bytes& b) { Put(b, 64, 80, 8); });
    Refused("logo too large", "limit", [](Bytes& b) { Put(b, 48, 262149, 8); });
    Refused("logo offset without a logo", "limit", [](Bytes& b) { Put(b, 48, 0, 8); });

    // Index
    Refused("checksum", "checksum", [](Bytes& b) { b[size_t(Get(b, 56, 8)) + 1] ^= 1; });
    Refused("order", "ascending", [](Bytes& b) { Put(b, Entry(b, 1), 0x05, 8); }, true);
    Refused("repeated hash", "ascending", [](Bytes& b) { Put(b, Entry(b, 1), 0x10, 8); }, true);
    Refused("width not a multiple of 4", "multiple of 4", [](Bytes& b) { Put(b, Entry(b, 0) + 8, 62, 2); }, true);
    Refused("width too small", "multiple of 4", [](Bytes& b) { Put(b, Entry(b, 0) + 8, 0, 2); }, true);
    Refused("width too large", "multiple of 4", [](Bytes& b) { Put(b, Entry(b, 0) + 8, 8196, 2); }, true);
    Refused("no original size", "original", [](Bytes& b) { Put(b, Entry(b, 0) + 12, 0, 2); }, true);
    Refused("proportion", "proportion", [](Bytes& b) { Put(b, Entry(b, 0) + 14, 8, 2); }, true);
    Refused("no levels", "levels", [](Bytes& b) { b[Entry(b, 0) + 16] = 0; }, true);
    Refused("too many levels", "levels", [](Bytes& b) { b[Entry(b, 0) + 16] = 8; }, true);
    Refused("unknown format", "format", [](Bytes& b) { b[Entry(b, 0) + 17] = 3; }, true);
    Refused("unknown flags", "flags", [](Bytes& b) { b[Entry(b, 0) + 18] = 2; }, true);
    Refused("compression", "compressed", [](Bytes& b) { b[Entry(b, 0) + 19] = 1; }, true);
    Refused("reserved", "reserved", [](Bytes& b) { b[Entry(b, 0) + 20] = 1; }, true);
    Refused("data size", "does not match its levels", [](Bytes& b) { Put(b, Entry(b, 0) + 32, 100, 8); }, true);
    Refused("data offset misaligned", "aligned", [](Bytes& b) { Put(b, Entry(b, 1) + 24, Get(b, Entry(b, 1) + 24, 8) + 1, 8); }, true);
    Refused("data outside", "outside the data", [](Bytes& b) { Put(b, Entry(b, 2) + 24, Get(b, 80, 8), 8); }, true);
    Refused("data offset overflow", "outside the data", [](Bytes& b) { Put(b, Entry(b, 2) + 24, ~0ull - 15, 8); }, true);
    Refused("data overlap", "overlap", [](Bytes& b) { Put(b, Entry(b, 1) + 24, Get(b, Entry(b, 0) + 24, 8), 8); }, true);

    // Information
    auto info_refused = [](const char* what, const std::string& info) {
        const std::string error = OpenError(Build(Three(), info));
        Check(error.find("information") != std::string::npos, std::string(what) + ": " + error);
    };
    info_refused("not JSON", "{not json");
    info_refused("not an object", "[1,2]");
    info_refused("no id", R"({"name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("bad id", R"({"id":"has space","name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("bad version", R"({"id":"i","name":"n","version":"1.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("bad min version", R"({"id":"i","name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"x","features":["hdtextures"],"type":"texture"})");
    info_refused("wrong type", R"({"id":"i","name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"language"})");
    info_refused("no feature", R"({"id":"i","name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["fonts"],"type":"texture"})");
    info_refused("no authors", R"({"id":"i","name":"n","version":"1.0.0","authors":[],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("authors not text", R"({"id":"i","name":"n","version":"1.0.0","authors":[1],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("name too long", R"({"id":"i","name":")" + std::string(65, 'n') +
                                      R"(","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("control character", R"({"id":"i","name":"a\u0001b","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})");
    info_refused("count differs", R"({"id":"i","name":"n","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture","texture_count":9})");
    {
        // text of a different alphabet counts by character, not by byte
        std::string accents = R"({"id":"i","name":")";
        for (int i = 0; i < 64; ++i) accents += "\xC3\xA9";
        accents += R"(","version":"1.0.0","authors":["a"],"summary":"s","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture"})";
        Check(texpack::PackFile::Open(Write("accents.ostp", Build(Three(), accents)), error) != nullptr,
              "Sixty four accented characters fit the limit of the name: " + error);
    }

    fs::remove_all(root);
    std::cout << "PASS: texture pack files\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
