#include "texture_dev_folder.h"
#include "png_writer.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace openshao::texpack;

static void Check(bool ok, const std::string& text) {
    if (!ok) throw std::runtime_error(text);
}

using Bytes = std::vector<uint8_t>;
static fs::path root;

static void Write(const std::string& name, const Bytes& b) {
    std::ofstream out(root / name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

// A picture whose pixels follow `seed`, so two pictures differ
static Bytes Pixels(uint32_t w, uint32_t h, int seed) {
    Bytes p(size_t(w) * h * 4);
    for (size_t i = 0; i < p.size(); ++i) p[i] = uint8_t((i * 7 + size_t(seed) * 31 + (i >> 5)) & 0xFF);
    return p;
}

static std::string Hex16(uint64_t h) {
    char b[17];
    std::snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(h));
    return b;
}

static std::string FileName(uint64_t hash, uint32_t ow, uint32_t oh) { return Hex16(hash) + "_DXT5_" + std::to_string(ow) + "x" + std::to_string(oh) + ".png"; }

int main() try {
    root = fs::temp_directory_path() / "openshao-devfolder-test";
    fs::remove_all(root);
    fs::create_directories(root);

    // The signature matches the one the Python script writes (hashlib.sha256 of "2x2:" and bytes 1 to 16)
    {
        Bytes p;
        for (uint8_t i = 1; i <= 16; ++i) p.push_back(i);
        Check(PixelSignature(2, 2, p.data()) == "c63575beb3bb0ea56abf587f4d589d2d85c6af225097b055bfdc4e26b85138a4", "The signature is the same as Python's");
        Bytes red;
        for (int i = 0; i < 4; ++i) red.insert(red.end(), {255, 0, 0, 255});
        Check(PixelSignature(4, 1, red.data()) == "e90464c2d1fbe143790d1e66bb891f5658208a6861c2934ebb88b5f006b5d587", "The size is part of the signature");
        Check(PixelSignature(1, 4, red.data()) != PixelSignature(4, 1, red.data()), "The same bytes in another shape give another signature");
    }

    // A folder as an extraction leaves it, and then edits
    std::string manifest = "{";
    const auto sign = [&](uint64_t hash, uint32_t w, uint32_t h, int seed, bool in_manifest = true) {
        const Bytes p = Pixels(w, h, seed);
        if (in_manifest) {
            if (manifest.size() > 1) manifest += ",";
            manifest += "\"" + Hex16(hash) + "\":{\"file\":\"x\",\"pixels\":\"" + PixelSignature(w, h, p.data()) + "\"}";
        }
        return p;
    };
    // 1 unchanged, 2 one pixel changed, 3 four times larger, 4 saved again with a text chunk, 5 corrupted, 6 not in the proportion,
    // 7 not in the manifest, 8 too small, 9 a name that does not fit, 10 and 11 the same hash
    Write(FileName(0x1, 16, 16), pngtest::Png(16, 16, sign(0x1, 16, 16, 1)));
    {
        Bytes p = sign(0x2, 16, 16, 2);
        p[40] ^= 1;  // one bit of one pixel
        Write(FileName(0x2, 16, 16), pngtest::Png(16, 16, p));
    }
    {
        sign(0x3, 16, 16, 3);
        Write(FileName(0x3, 16, 16), pngtest::Png(64, 64, Pixels(64, 64, 9)));
    }
    Write(FileName(0x4, 16, 16), pngtest::Png(16, 16, sign(0x4, 16, 16, 4), "Comment\0saved by another program"));
    {
        sign(0x5, 16, 16, 5);
        Bytes png = pngtest::Png(16, 16, Pixels(16, 16, 5));
        png.resize(png.size() / 2);
        Write(FileName(0x5, 16, 16), png);
    }
    {
        sign(0x6, 16, 16, 6);
        Write(FileName(0x6, 16, 16), pngtest::Png(32, 16, Pixels(32, 16, 6)));
    }
    Write(FileName(0x7, 16, 16), pngtest::Png(16, 16, sign(0x7, 16, 16, 7, false)));
    {
        sign(0x8, 4, 4, 8);
        Write(FileName(0x8, 4, 4), pngtest::Png(2, 2, Pixels(2, 2, 8)));
    }
    Write("not_a_texture.png", pngtest::Png(4, 4, Pixels(4, 4, 1)));
    Write(Hex16(0xa) + "_DXT5_8x8.txt", Bytes{1, 2, 3});
    {
        sign(0xa, 8, 8, 1);
        Write(FileName(0xa, 8, 8), pngtest::Png(8, 8, Pixels(8, 8, 90)));
        Write(Hex16(0xa) + "_A8R8G8B8_8x8.png", pngtest::Png(8, 8, Pixels(8, 8, 91)));  // sorts before the DXT5 one
    }
    manifest += "}";
    {
        std::ofstream m(root / "manifest.json");
        m << manifest;
    }

    DevFolderReport report;
    const auto source = OpenDevFolder(root, report, 1);
    Check(source != nullptr, "A folder with edits gives a source");
    Check(report.manifest && report.files == 10, "Ten files are named as textures, and the manifest was read: " + std::to_string(report.files));
    Check(report.unchanged == 2, "The unchanged one, and the one saved again with other metadata, are left out");
    Check(report.edited == 4 && report.unsigned_files == 1, "Edited: one pixel, four times larger, no signature, and the duplicate hash");
    Check(report.refused == 3, "Refused: corrupted, wrong proportion, too small");
    Check(report.duplicates == 1 && report.ignored == 2, "One duplicate, and two names that do not fit");
    Check(source->Count() == 4, "The source holds the four");
    Check(!source->Find(0x1) && !source->Find(0x4), "An unchanged texture is not a replacement");
    Check(!source->Find(0x5) && !source->Find(0x6) && !source->Find(0x8), "A refused texture is not a replacement");

    // One pixel changed: the edited picture, at its own size
    {
        const auto e = source->Find(0x2);
        Check(e && e->width == 16 && e->height == 16 && e->levels == 5 && e->format == BcFormat::Rgba8, "A one-pixel edit is a replacement with five levels");
        std::vector<uint8_t> out;
        Check(source->ReadLevels(*e, 0, out), "It reads");
        Bytes expected = Pixels(16, 16, 2);
        expected[40] ^= 1;
        size_t total = 0;
        for (uint32_t l = 0; l < 5; ++l) total += size_t(std::max(1u, 16u >> l)) * std::max(1u, 16u >> l) * 4;
        Check(out.size() == total && std::equal(expected.begin(), expected.end(), out.begin()), "Level 0 is the picture as it was saved, and the total size is that of every level");
        std::vector<uint8_t> later;
        Check(source->ReadLevels(*e, 2, later) && later.size() == total - (256 + 64) * 4 && std::equal(later.begin(), later.end(), out.begin() + (256 + 64) * 4),
              "Starting at level 2 gives the same bytes of those levels");
        Check(!source->ReadLevels(*e, 5, later), "A first level past the last is refused");
    }

    // Four times larger: seven levels, and each is the 2 x 2 average of the one above
    {
        const auto e = source->Find(0x3);
        Check(e && e->width == 64 && e->height == 64 && e->levels == 7, "A picture four times larger keeps its size and has seven levels");
        std::vector<uint8_t> out;
        Check(source->ReadLevels(*e, 0, out), "It reads");
        const Bytes p = Pixels(64, 64, 9);
        const auto at = [&](uint32_t x, uint32_t y, int c) { return p[(size_t(y) * 64 + x) * 4 + size_t(c)]; };
        bool ok = true;
        const size_t level1 = size_t(64) * 64 * 4;
        for (uint32_t y = 0; y < 32 && ok; ++y)
            for (uint32_t x = 0; x < 32 && ok; ++x)
                for (int c = 0; c < 4; ++c) {
                    const unsigned sum = at(2 * x, 2 * y, c) + at(2 * x + 1, 2 * y, c) + at(2 * x, 2 * y + 1, c) + at(2 * x + 1, 2 * y + 1, c);
                    ok = ok && out[level1 + (size_t(y) * 32 + x) * 4 + size_t(c)] == uint8_t((sum + 2) / 4);
                }
        Check(ok, "Level 1 is the 2 x 2 average of level 0");
        Check(out.size() == (4096 + 1024 + 256 + 64 + 16 + 4 + 1) * 4, "The levels go down to one pixel");
    }

    // The same hash twice: the file with the smaller name is the one that counts
    {
        const auto e = source->Find(0xa);
        std::vector<uint8_t> out;
        const Bytes first = Pixels(8, 8, 91);
        Check(e && source->ReadLevels(*e, 0, out) && std::equal(first.begin(), first.end(), out.begin()), "Of two files with one hash, the name that sorts first is used");
    }
    Check(!report.problems.empty() && report.seconds >= 0, "The report says what it found");

    // A file that changes after it was looked at is refused when read, not trusted
    {
        Write(FileName(0x2, 16, 16), pngtest::Png(8, 8, Pixels(8, 8, 2)));
        std::vector<uint8_t> out;
        Check(!source->ReadLevels(*source->Find(0x2), 0, out), "A file that changed size since it was looked at is not read");
    }

    // Nothing edited: no source. No manifest: everything counts as edited.
    {
        fs::remove_all(root);
        fs::create_directories(root);
        std::string m = "{\"" + Hex16(0x20) + "\":{\"pixels\":\"" + PixelSignature(16, 16, Pixels(16, 16, 1).data()) + "\"}}";
        Write(FileName(0x20, 16, 16), pngtest::Png(16, 16, Pixels(16, 16, 1)));
        std::ofstream(root / "manifest.json") << m;
        DevFolderReport r;
        Check(OpenDevFolder(root, r, 1) == nullptr && r.unchanged == 1 && r.edited == 0, "A folder where nothing was edited gives no source");
        fs::remove(root / "manifest.json");
        const auto s = OpenDevFolder(root, r, 1);
        Check(s && !r.manifest && r.unsigned_files == 1 && s->Count() == 1, "Without a manifest, every picture counts as edited");
        DevFolderReport missing;
        Check(OpenDevFolder(root / "nowhere", missing) == nullptr && !missing.problems.empty(), "A folder that does not exist gives no source and a reason");
    }

    // Many files, several threads: the same answer as one thread
    {
        fs::remove_all(root);
        fs::create_directories(root);
        std::string m = "{";
        for (uint64_t i = 1; i <= 150; ++i) {
            const Bytes p = Pixels(16, 8, int(i));
            if (i > 1) m += ",";
            m += "\"" + Hex16(i) + "\":{\"pixels\":\"" + PixelSignature(16, 8, p.data()) + "\"}";
            Bytes shown = p;
            if (i % 30 == 0) shown[3] ^= 0x80;  // five of them edited
            Write(FileName(i, 16, 8), pngtest::Png(16, 8, shown));
        }
        m += "}";
        std::ofstream(root / "manifest.json") << m;
        DevFolderReport one, many;
        const auto a = OpenDevFolder(root, one, 1);
        const auto b = OpenDevFolder(root, many, 8);
        Check(a && b && one.edited == 5 && many.edited == 5 && one.unchanged == 145 && many.unchanged == 145 && a->Count() == b->Count(),
              "Five edits among 150 pictures, found the same by one thread and by eight");
        for (uint64_t i = 30; i <= 150; i += 30) Check(a->Find(i) && b->Find(i), "Each edited one is found");
    }

    fs::remove_all(root);
    std::cout << "PASS: texture developer folder\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
