#include "texture_pack.h"
#include "texture_pack_stats.h"
#include "texture_pack_builder.h"
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

    // The packs as the loader reads them
    {
        const auto source = texpack::Source();
        Check(source != nullptr, "A source for the packs in use");
        const auto entry = source->Find(0xa1);
        Check(entry && entry->width == 16 && entry->height == 8 && entry->levels == 3 && entry->format == texpack::BcFormat::Bc3 &&
                  entry->medium_first_level == 0,
              "The source describes a texture");
        std::vector<uint8_t> all, later;
        Check(source->ReadLevels(*entry, 0, all) && all == blocks, "The source reads what Load reads");
        Check(source->ReadLevels(*entry, 1, later) && later.size() == all.size() - texpack::LevelBytes(16, 8, 0) &&
                  later.front() == 0x11 && later.back() == 0x12,
              "Reading from a later level skips the earlier ones");
        Check(!source->ReadLevels(*entry, 3, later), "A level the texture lacks is refused");
        Check(!source->Find(0xdead), "A texture the packs lack is not found");
        const auto bad = source->Find(0xa2);
        Check(bad && !source->ReadLevels(*bad, 0, later), "A file that disagrees with its line is refused");
        const uint64_t generation = texpack::Generation();
        texpack::SetPacks({root / "a", root / "b"});
        Check(texpack::Generation() != generation, "Setting the packs changes the generation");
        Check(source->ReadLevels(*entry, 0, all) && texpack::Source() != source, "An old source keeps reading what it described");
        Check(texpack::Source()->Preload().empty(), "Folders without a list ask for nothing ahead");
        {
            std::ofstream(root / "a" / "preload.txt") << "# the menus\n00000000000000a1\n\nnot a hash\n00000000000000dd\n00000000000000a1\n";
            std::ofstream(root / "b" / "preload.txt") << "00000000000000b1 and the rest of the line is ignored\n00000000000000a1\n";
            texpack::SetPacks({root / "a", root / "b"});
            const auto ahead = texpack::Source()->Preload();
            Check(ahead == std::vector<uint64_t>({0xa1, 0xb1}),
                  "The list to load ahead has what the folders have a replacement for, once, in order");
            fs::remove(root / "a" / "preload.txt");
            fs::remove(root / "b" / "preload.txt");
        }
        std::vector<uint8_t> other;
        texpack::SetPacks({});
        Check(texpack::Source() == nullptr, "No packs, no source");
        Check(source->ReadLevels(*entry, 0, other), "A source outlives the packs it came from");
        texpack::SetPacks({root / "a", root / "b"});
    }

    const auto before = texpack::GetStats();
    {
        texpack::Resident one_texture(1000);
        const auto held = texpack::GetStats();
        Check(held.resident == before.resident + 1 && held.resident_bytes == before.resident_bytes + 1000,
              "A held replacement counts as resident");
    }
    const auto released = texpack::GetStats();
    Check(released.resident == before.resident && released.resident_bytes == before.resident_bytes,
          "A released replacement stops counting");
    {
        texpack::Interval span;
        Check(!texpack::TakeInterval(2, span), "An interval waits for its frames");
        texpack::NoteLoad(100, 400, true);
        texpack::EndFrame();
        texpack::NoteLoad(10, 20, true);
        texpack::NoteLoad(5, 5, true);
        texpack::EndFrame();
        Check(texpack::TakeInterval(2, span) && span.frames == 2 && span.loads == 3 && span.disk_us == 115 &&
                  span.upload_us == 425 && span.worst_frame_us == 500,
              "An interval has the loads of its frames and the worst frame");
        Check(!texpack::TakeInterval(2, span), "The next interval starts over");
        texpack::EndFrame();
        texpack::EndFrame();
        Check(texpack::TakeInterval(2, span) && span.loads == 0 && span.worst_frame_us == 0, "A quiet interval has no loads");
        texpack::NoteReadyAtFirstUse();
        texpack::NoteReadyAtFirstUse();
        texpack::NoteWait(2000);
        texpack::NoteWait(4000);
        texpack::NotePopIn(3);
        texpack::NotePopIn(7);
        texpack::NoteLoad(900, 100, true, false);  // read on another thread: only the upload held up the frame
        texpack::EndFrame();
        texpack::EndFrame();
        Check(texpack::TakeInterval(2, span) && span.waits == 2 && span.wait_us == 6000 && span.pop_ins == 2 &&
                  span.pop_frames == 10 && span.pop_max >= 7 && span.ready == 2 && span.disk_us == 900 && span.worst_frame_us == 100,
              "Waits and pop-ins add up, and a read made elsewhere does not count against the frame");
        uint32_t number = 0;
        Check(!texpack::TakeMarker(number), "No marker without a request");
        texpack::RequestMarker();
        texpack::RequestMarker();
        Check(texpack::TakeMarker(number) && number == 1 && texpack::TakeMarker(number) && number == 2 && !texpack::TakeMarker(number),
              "Markers are taken in order, once each");
    }
    const auto before_noted = texpack::GetStats();
    texpack::NoteLoad(300, 700, true);
    texpack::NoteLoad(50, 0, false);
    const auto noted = texpack::GetStats();
    Check(noted.loads == before_noted.loads + 1 && noted.failures == before_noted.failures + 1 &&
              noted.disk_us == before_noted.disk_us + 350 && noted.upload_us == before_noted.upload_us + 700,
          "Loads and failures add their times");

    // A pack file among the folders
    {
        const Bytes file = Build({{0xa1, 64, 64, 16, 16, 7, 2, 0, 0x77}, {0xf1, 64, 64, 16, 16, 7, 2, 0, 0x55}},
                                 InfoFor(2, ",\"preload\":[\"00000000000000f1\"]"));
        {
            std::ofstream out(root / "c.ostp", std::ios::binary);
            out.write(reinterpret_cast<const char*>(file.data()), std::streamsize(file.size()));
        }
        std::ofstream(root / "junk.ostp") << "not a texture pack";
        texpack::SetPacks({root / "a", root / "c.ostp", root / "junk.ostp", root / "missing.ostp", root / "b"});
        auto source = texpack::Source();
        Check(source != nullptr, "A pack file among the folders, with files that are not packs, gives a source");
        std::vector<uint8_t> out;
        auto e = source->Find(0xa1);
        Check(e && e->width == 16 && e->height == 8 && source->ReadLevels(*e, 0, out) && out.front() == 0x10,
              "A folder before the file gives its own texture when both have it");
        const auto f = source->Find(0xf1);
        Check(f && f->width == 64 && f->levels == 7 && source->ReadLevels(*f, 0, out) && out.front() == 0x55,
              "A texture only the file has is read from the file");
        Check(source->Find(0xb1).has_value() && !source->Find(0xdead), "A folder after the file is read too");
        Check(source->Preload() == std::vector<uint64_t>({0xf1}), "The list to load ahead comes from the file");
        texpack::SetPacks({root / "c.ostp", root / "a"});
        source = texpack::Source();
        e = source->Find(0xa1);
        Check(e && e->width == 64 && source->ReadLevels(*e, 0, out) && out.front() == 0x77,
              "The file before the folder gives its own texture when both have it");
        texpack::SetPacks({root / "junk.ostp"});
        Check(texpack::Source() == nullptr && !texpack::Enabled(), "Only a file that is not a pack turns the replacement off");
    }

    texpack::SetPacks({});
    Check(!texpack::Enabled() && !texpack::Find(0xa1), "An empty list turns the replacement off");
    fs::remove_all(root);
    std::cout << "PASS: texture packs\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
