#include "texture_pack_match.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace openshao::texpack;

static void Check(bool ok, const std::string& text) {
    if (!ok) throw std::runtime_error(text);
}

namespace {
class FakePack final : public PackSource {
public:
    std::map<uint64_t, SourceEntry> entries;
    std::optional<SourceEntry> Find(uint64_t hash) const override {
        const auto it = entries.find(hash);
        if (it == entries.end()) return std::nullopt;
        return it->second;
    }
    bool ReadLevels(const SourceEntry&, uint32_t, std::vector<uint8_t>&) const override { return false; }
};

constexpr gpu::FormatInfo kBgra{"A8R8G8B8", 32, true, false, false, gpu::Host::BGRA8};
constexpr gpu::FormatInfo kBc1{"DXT1", 8, false, true, false, gpu::Host::BC1};
constexpr gpu::FormatInfo kBc2{"DXT3", 16, false, true, false, gpu::Host::BC2};
constexpr gpu::FormatInfo kBc3{"DXT5", 16, false, true, false, gpu::Host::BC3};
constexpr gpu::FormatInfo kDepth{"D24S8", 32, true, false, true, gpu::Host::R32F};
constexpr gpu::FormatInfo kFloat{"R32F", 32, false, false, false, gpu::Host::R32F};

gpu::TextureDesc Texture(uint32_t w, uint32_t h, uint32_t format = 0x06) {
    gpu::TextureDesc d;
    d.format = format;
    d.width = w;
    d.height = h;
    return d;
}

SourceEntry Entry(uint64_t hash, uint16_t w, uint16_t h) {
    SourceEntry e;
    e.hash = hash;
    e.width = w;
    e.height = h;
    e.levels = 1;
    return e;
}
}

int main() try {
    // The formats the renderers hold as colour are replaceable
    Check(CanReplace(Texture(256, 256), kBgra), "BGRA8 texture");
    Check(CanReplace(Texture(256, 256, 0x0C), kBc1), "BC1 texture");
    Check(CanReplace(Texture(256, 256, 0x0E), kBc2), "BC2 texture");
    Check(CanReplace(Texture(256, 256, 0x0F), kBc3), "BC3 texture");

    // Cube maps, volumes, linear textures, depth, palettes and float formats are not
    {
        gpu::TextureDesc cube = Texture(256, 256);
        cube.cube = true;
        Check(!CanReplace(cube, kBgra), "a cube map is not replaced");
        gpu::TextureDesc volume = Texture(64, 64);
        volume.depth = 8;
        Check(!CanReplace(volume, kBgra), "a volume is not replaced");
        gpu::TextureDesc linear = Texture(256, 256, 0x12);
        linear.linear = true;
        Check(!CanReplace(linear, kBgra), "a linear texture is not replaced");
        Check(!CanReplace(Texture(256, 256, 0x2E), kDepth), "a depth texture is not replaced");
        Check(!CanReplace(Texture(256, 256, 0x0B), kBgra), "a palette texture is not replaced");
        Check(!CanReplace(Texture(256, 256, 0x30), kFloat), "a float texture is not replaced");
    }

    // A replacement is found by hash and has to keep the aspect ratio
    {
        FakePack pack;
        pack.entries[1] = Entry(1, 1024, 1024);
        pack.entries[2] = Entry(2, 1024, 512);
        pack.entries[3] = Entry(3, 1024, 500);
        pack.entries[4] = Entry(4, 8192, 4096);
        Check(Match(pack, Texture(256, 256), 1).has_value(), "a larger copy of a square picture");
        Check(Match(pack, Texture(256, 128), 2).has_value(), "a larger copy of a wide picture");
        Check(!Match(pack, Texture(256, 128), 3).has_value(), "a picture with another aspect ratio is refused");
        Check(!Match(pack, Texture(256, 256), 2).has_value(), "a wide copy of a square picture is refused");
        Check(!Match(pack, Texture(256, 256), 99).has_value(), "a hash the pack does not hold");
        Check(Match(pack, Texture(2048, 1024), 4).has_value(), "the largest size of a pack");
        Check(Match(pack, Texture(256, 256), 1)->width == 1024, "the entry is returned as stored");
    }

    // The aspect ratio test does not overflow with the largest sizes
    {
        FakePack pack;
        pack.entries[5] = Entry(5, 65535, 65535);
        gpu::TextureDesc big = Texture(4096, 4096);
        Check(Match(pack, big, 5).has_value(), "a square picture at the limits of the entry fields");
    }

    // An empty pack replaces nothing
    {
        FakePack pack;
        Check(!Match(pack, Texture(256, 256), 1).has_value(), "an empty pack");
    }
    std::cout << "texture pack match: ok\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
