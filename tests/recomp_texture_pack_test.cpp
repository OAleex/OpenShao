#include "texture_pack.h"
#include "core/log.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace logx { void Write(Level, const char*, ...) {} }

namespace fs = std::filesystem;
using namespace openshao;

static void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }

// A DXT5 DDS of the given size, with every level filled with a recognizable byte
static void WriteDds(const fs::path& path, uint32_t w, uint32_t h, uint32_t levels, uint8_t fill, const char* fourcc = "DXT5") {
    std::vector<uint8_t> file(128, 0);
    std::memcpy(file.data(), "DDS ", 4);
    std::memcpy(&file[12], &h, 4);
    std::memcpy(&file[16], &w, 4);
    std::memcpy(&file[28], &levels, 4);
    std::memcpy(&file[84], fourcc, 4);
    for (uint32_t l = 0; l < levels; ++l) file.insert(file.end(), texpack::LevelBytes(w, h, l), uint8_t(fill + l));
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), std::streamsize(file.size()));
}

int main() try {
    const fs::path root = fs::temp_directory_path() / "openshao-texpack-test";
    fs::remove_all(root);
    fs::create_directories(root / "a/tex");
    fs::create_directories(root / "b");
    Check(texpack::LevelBytes(512, 256, 0) == 128 * 64 * 16 && texpack::LevelBytes(512, 256, 9) == 16,
          "A level is made of 4x4 blocks of 16 bytes, at least one");

    WriteDds(root / "a/tex/one.dds", 16, 8, 3, 0x10);
    WriteDds(root / "a/tex/wrong_size.dds", 16, 16, 1, 0x20);
    WriteDds(root / "a/tex/wrong_format.dds", 8, 8, 1, 0x30, "DXT1");
    WriteDds(root / "b/two.dds", 8, 8, 1, 0x40);
    {
        std::ofstream index(root / "a/index.txt");
        index << "# hash width height levels file\n"
              << "00000000000000a1 16 8 3 tex/one.dds\n"
              << "00000000000000a2 8 8 1 tex/wrong_size.dds\n"          // the file is 16x16
              << "00000000000000a3 8 8 1 tex/wrong_format.dds\n"
              << "00000000000000a4 8 8 1 tex/missing.dds\n"
              << "00000000000000a5 8 8 1 ../outside.dds\n"             // leaves the pack
              << "00000000000000a6 8 8 1 /etc/passwd\n"
              << "00000000000000a7 6 8 1 tex/one.dds\n"                // not made of whole blocks
              << "not-a-hash 8 8 1 tex/one.dds\n"
              << "00000000000000a8 8 8 1 tex/one.dds\n";
        std::ofstream other(root / "b/index.txt");
        other << "00000000000000a1 8 8 1 two.dds\n00000000000000b1 8 8 1 two.dds\n";
    }

    Check(!texpack::Enabled(), "No pack is in use at first");
    texpack::SetPacks({root / "a", root / "b"});
    Check(texpack::Enabled(), "A pack with replacements turns the feature on");

    const auto one = texpack::Find(0xa1);
    Check(one && one->w == 16 && one->h == 8 && one->levels == 3, "The first pack names the texture");
    std::vector<uint8_t> blocks;
    Check(texpack::Load(0xa1, blocks), "A valid file loads");
    Check(blocks.size() == texpack::LevelBytes(16, 8, 0) + texpack::LevelBytes(16, 8, 1) + texpack::LevelBytes(16, 8, 2),
          "Every level is read");
    Check(blocks.front() == 0x10 && blocks.back() == 0x12, "The levels come largest first");
    Check(texpack::Find(0xb1).has_value(), "Another pack adds its own textures");

    for (const uint64_t unsafe : {0xa5ull, 0xa6ull, 0xa7ull, 0xa9ull}) Check(!texpack::Find(unsafe), "Unsafe or malformed lines are skipped");
    Check(!texpack::Load(0xa2, blocks), "A file that disagrees with the index is refused");
    Check(!texpack::Load(0xa3, blocks), "A file that is not DXT5 is refused");
    Check(!texpack::Load(0xa4, blocks), "A missing file is refused");
    Check(!texpack::Load(0xdead, blocks), "An unknown hash is refused");

    texpack::SetPacks({});
    Check(!texpack::Enabled() && !texpack::Find(0xa1), "An empty list turns the replacement off");
    fs::remove_all(root);
    std::cout << "PASS: texture packs\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
