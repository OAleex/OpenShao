#include "texture_loader.h"
#include "texture_pack_builder.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace openshao;
using Clock = std::chrono::steady_clock;

static void Check(bool ok, const std::string& text) { if (!ok) throw std::runtime_error(text); }

static fs::path root;

static fs::path Write(const std::string& name, const Bytes& b) {
    const fs::path path = root / name;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    return path;
}

// Textures 0x100, 0x101, ...: 64 x 64 at 4x of a 16 x 16 original, seven levels, filled with a byte of their own
static std::vector<Tex> Many(int count) {
    std::vector<Tex> textures;
    for (int i = 0; i < count; ++i) textures.push_back({uint64_t(0x100 + i), 64, 64, 16, 16, 7, 2, 0, uint8_t(i * 3)});
    return textures;
}

struct Opened {
    std::shared_ptr<const texpack::PackFile> file;
    std::shared_ptr<const texpack::PackSource> source;
    const texpack::FileEntry* Find(uint64_t hash) const { return file->Find(hash); }
    bool ReadLevels(const texpack::FileEntry& e, uint32_t first, std::vector<uint8_t>& out) const { return file->ReadLevels(e, first, out); }
    operator std::shared_ptr<const texpack::PackSource>() const { return source; }
    const Opened* operator->() const { return this; }
};

static Opened Open(const std::string& name, int count = 8) {
    std::string error;
    std::shared_ptr<const texpack::PackFile> pack = texpack::PackFile::Open(Write(name, Build(Many(count), InfoFor(size_t(count)))), error);
    Check(pack != nullptr, "The test pack opens: " + error);
    return {pack, texpack::AsSource(pack)};
}

static std::vector<texpack::Loaded> TakeAll(texpack::Loader& loader) {
    std::vector<texpack::Loaded> out;
    loader.TakeReady(out);
    return out;
}

// Waits until `done` holds, up to ten seconds
template <class F>
static void WaitUntil(F done, const char* what) {
    const auto limit = Clock::now() + std::chrono::seconds(10);
    while (!done()) {
        Check(Clock::now() < limit, std::string("Timed out waiting: ") + what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static texpack::Loader::Options NoThreads() {
    texpack::Loader::Options o;
    o.threads = 0;
    return o;
}

int main() try {
    root = fs::temp_directory_path() / "openshao-texloader-test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto pack = Open("a.ostp");

    // A request is read, and what is delivered is what the pack holds
    {
        texpack::Loader loader(NoThreads());
        Check(!loader.Request(0x100), "Without a pack nothing can be asked for");
        loader.SetPack(pack, texpack::Quality::High);
        Check(loader.Request(0x100), "A texture of the pack can be asked for");
        Check(loader.GetStats().pending == 1, "The request waits");
        Check(loader.ProcessOne() && !loader.ProcessOne(), "One request is read, and then there is nothing to read");
        auto out = TakeAll(loader);
        Check(out.size() == 1 && out[0].ok && out[0].hash == 0x100 && out[0].first_level == 0, "It is delivered");
        std::vector<uint8_t> expected;
        Check(pack->ReadLevels(*pack->Find(0x100), 0, expected) && out[0].blocks == expected, "The blocks are those of the pack");
        Check(out[0].entry.hash == 0x100 && out[0].entry.levels == 7 && out[0].entry.width == 64 && out[0].pack == pack.source,
              "The result names its entry and keeps the pack");
        Check(out[0].wait_us >= out[0].read_us, "The wait includes the read");
        Check(TakeAll(loader).empty(), "A result is delivered once");
        const auto stats = loader.GetStats();
        Check(stats.completed == 1 && stats.failed == 0 && stats.dropped == 0 && stats.ready == 0 && stats.ready_bytes == 0,
              "The counts agree");

        Check(!loader.Request(0x999) && !loader.Request(0) && loader.GetStats().pending == 0, "A texture the pack lacks is refused");
    }

    // The quality sets where the blocks start
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::Medium);
        loader.Request(0x101);
        loader.ProcessOne();
        const auto out = TakeAll(loader);
        const auto* entry = pack->Find(0x101);
        Check(out.size() == 1 && out[0].first_level == 1 && out[0].blocks.size() == entry->data_size - texpack::LevelSize(64, 64, 0, entry->format) &&
                  out[0].blocks.front() == uint8_t(3 + 1),
              "Medium starts at the second level");
    }

    // Asking again does not read again
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        for (int i = 0; i < 100; ++i) Check(loader.Request(0x102), "Asked many times");
        Check(loader.GetStats().pending == 1, "Many requests make one");
        loader.ProcessOne();
        Check(loader.Request(0x102) && loader.GetStats().pending == 0 && loader.GetStats().ready == 1,
              "A texture that is ready is not asked for again");
        Check(TakeAll(loader).size() == 1, "Delivered once");
        Check(loader.Request(0x102) && loader.GetStats().pending == 1, "After it was taken it can be asked for again");
    }

    // The highest priority first, and the most recent among equals
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100, 1);
        loader.Request(0x101, 5);
        loader.Request(0x102, 3);
        loader.Request(0x103, 3);
        while (loader.ProcessOne()) {}
        const auto out = TakeAll(loader);
        const std::vector<uint64_t> want = {0x101, 0x103, 0x102, 0x100};
        Check(out.size() == 4, "All four are read");
        for (size_t i = 0; i < 4; ++i) Check(out[i].hash == want[i], "The order of reading follows priority and recency");
    }
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100, 1);
        loader.Request(0x101, 2);
        loader.Request(0x102, 2);
        loader.Request(0x100, 9);  // asked again with a higher priority
        loader.Request(0x101, 0);  // a lower priority does not lower it
        loader.Request(0x101, 2);
        loader.ProcessOne();
        Check(TakeAll(loader)[0].hash == 0x100, "Asking again raises the priority");
        loader.ProcessOne();
        Check(TakeAll(loader)[0].hash == 0x101, "Asking again makes a request the most recent");
    }

    // Cancelling
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100);
        loader.Request(0x101);
        Check(loader.Cancel(0x100) && !loader.Cancel(0x100) && !loader.Cancel(0x105), "Only a pending request is cancelled, once");
        while (loader.ProcessOne()) {}
        const auto out = TakeAll(loader);
        Check(out.size() == 1 && out[0].hash == 0x101 && loader.GetStats().dropped == 1, "A cancelled request is not read");
    }

    // Requests grow old
    {
        auto options = NoThreads();
        options.max_age_frames = 10;
        texpack::Loader loader(options);
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100);
        for (int i = 0; i < 11; ++i) loader.AdvanceFrame();
        loader.Request(0x101);
        Check(loader.ProcessOne(), "A request that is not old is read");
        const auto out = TakeAll(loader);
        Check(out.size() == 1 && out[0].hash == 0x101, "The old one is passed over");
        Check(!loader.ProcessOne() && loader.GetStats().dropped == 1, "The old one is dropped, not read");
        loader.Request(0x100);
        for (int i = 0; i < 10; ++i) loader.AdvanceFrame();
        Check(loader.ProcessOne(), "A request exactly as old as the limit is still read");
        loader.Request(0x102);
        for (int i = 0; i < 6; ++i) loader.AdvanceFrame();
        loader.Request(0x102);  // asked again: young again
        for (int i = 0; i < 6; ++i) loader.AdvanceFrame();
        Check(loader.ProcessOne(), "Asking again keeps a request from growing old");
    }

    // Changing the pack drops what belongs to the old one
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100);
        loader.Request(0x101);
        loader.ProcessOne();  // 0x101 is ready, 0x100 is pending
        const auto other = Open("b.ostp", 4);
        loader.SetPack(other, texpack::Quality::Medium);
        Check(loader.GetStats().pending == 0 && loader.GetStats().ready == 0 && loader.GetStats().dropped == 2 &&
                  TakeAll(loader).empty() && !loader.ProcessOne(),
              "Pending and ready results of the old pack are dropped");
        Check(!loader.Request(0x104) && loader.Request(0x103), "The new pack decides what can be asked for");
        loader.ProcessOne();
        const auto out = TakeAll(loader);
        Check(out.size() == 1 && out[0].pack == other.source && out[0].first_level == 1, "Results come from the new pack and quality");
    }

    // A read in progress when the pack changes is dropped when it ends
    {
        std::atomic<bool> inside{false};
        std::promise<void> release;
        auto released = release.get_future().share();
        texpack::Loader::Options options;
        options.threads = 1;
        options.on_read = [&] {
            inside = true;
            released.wait();
        };
        texpack::Loader loader(options);
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100);
        WaitUntil([&] { return inside.load(); }, "the read to start");
        Check(loader.GetStats().in_flight == 1 && !loader.Cancel(0x100), "A read in progress cannot be cancelled");
        Check(loader.Request(0x100), "Asking for what is being read is accepted and changes nothing");
        loader.SetPack(Open("c.ostp", 2), texpack::Quality::High);
        release.set_value();
        WaitUntil([&] { return loader.GetStats().in_flight == 0; }, "the read to end");
        Check(TakeAll(loader).empty() && loader.GetStats().dropped == 1 && loader.GetStats().ready == 0,
              "The result of a read that began under the old pack is dropped");
    }

    // A file that cannot be read is reported as a failure
    {
        const auto path = Write("short.ostp", Build(Many(3), InfoFor(3)));
        std::string error;
        std::shared_ptr<const texpack::PackFile> broken = texpack::PackFile::Open(path, error);
        Check(broken != nullptr, "Opens before the damage");
        fs::resize_file(path, 300);
        texpack::Loader loader(NoThreads());
        loader.SetPack(texpack::AsSource(broken), texpack::Quality::High);
        loader.Request(0x101);
        loader.ProcessOne();
        const auto out = TakeAll(loader);
        Check(out.size() == 1 && !out[0].ok && out[0].blocks.empty() && out[0].hash == 0x101 && loader.GetStats().failed == 1 &&
                  loader.GetStats().ready_bytes == 0,
              "A file that was cut short gives a failed result");
    }

    // Ready blocks that nobody takes are limited, the oldest going first
    {
        auto options = NoThreads();
        options.max_ready_bytes = 12000;  // a little over two textures
        texpack::Loader loader(options);
        loader.SetPack(pack, texpack::Quality::High);
        for (int i = 0; i < 5; ++i) {
            loader.Request(uint64_t(0x100 + i));
            loader.ProcessOne();
        }
        const auto stats = loader.GetStats();
        Check(stats.ready_bytes <= 12000 && stats.ready >= 1 && stats.ready < 5 && stats.dropped == 5 - stats.ready,
              "Over the limit the oldest ready blocks are dropped");
        const auto out = TakeAll(loader);
        Check(out.back().hash == 0x104 && out.front().hash != 0x100, "The newest are the ones kept");
        auto tiny = NoThreads();
        tiny.max_ready_bytes = 1;
        texpack::Loader small(tiny);
        small.SetPack(pack, texpack::Quality::High);
        small.Request(0x100);
        small.ProcessOne();
        Check(small.GetStats().ready == 1, "The newest result is kept even when it alone passes the limit");
    }

    // Buffers given back are reused
    {
        texpack::Loader loader(NoThreads());
        loader.SetPack(pack, texpack::Quality::High);
        loader.Request(0x100);
        loader.ProcessOne();
        auto out = TakeAll(loader);
        Check(loader.GetStats().pool_hits == 0, "The first read has no buffer to reuse");
        loader.Recycle(std::move(out[0].blocks));
        loader.Request(0x101);
        loader.ProcessOne();
        Check(loader.GetStats().pool_hits == 1, "A buffer that was given back is reused");
    }

    // Threads reading while others ask and take
    {
        texpack::Loader::Options options;
        options.threads = 4;
        texpack::Loader loader(options);
        const int count = 32;
        loader.SetPack(Open("many.ostp", count), texpack::Quality::High);
        std::atomic<bool> stop{false};
        std::vector<std::thread> askers;
        for (int t = 0; t < 6; ++t)
            askers.emplace_back([&, t] {
                std::mt19937 random(unsigned(t) + 1);
                for (int i = 0; i < 800 && !stop; ++i) {
                    loader.Request(uint64_t(0x100 + int(random() % count)), float(random() % 5));
                    if (i % 50 == 0) loader.AdvanceFrame();
                    if (i % 97 == 0) loader.Cancel(uint64_t(0x100 + int(random() % count)));
                }
            });
        std::map<uint64_t, int> seen;
        bool wrong = false;
        const auto limit = Clock::now() + std::chrono::seconds(20);
        // keep asking from here until every texture came out at least once
        while (int(seen.size()) < count && Clock::now() < limit) {
            for (int i = 0; i < count; ++i) loader.Request(uint64_t(0x100 + i), 1);
            std::vector<texpack::Loaded> out;
            loader.TakeReady(out);
            for (auto& r : out) {
                const int index = int(r.hash - 0x100);
                wrong = wrong || !r.ok || r.blocks.empty() || r.blocks.front() != uint8_t(index * 3) || r.first_level != 0;
                ++seen[r.hash];
                loader.Recycle(std::move(r.blocks));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stop = true;
        for (auto& t : askers) t.join();
        Check(int(seen.size()) == count, "Every texture was delivered while several threads asked");
        Check(!wrong, "Every delivery had the right blocks");
        const auto stats = loader.GetStats();
        Check(stats.failed == 0 && stats.completed >= uint64_t(count), "Nothing failed");
    }

    // A loader that is destroyed with work in hand does not hang
    for (int i = 0; i < 40; ++i) {
        texpack::Loader::Options options;
        options.threads = 3;
        texpack::Loader loader(options);
        loader.SetPack(pack, texpack::Quality::High);
        for (int k = 0; k < 8; ++k) loader.Request(uint64_t(0x100 + k));
        if (i % 2) std::this_thread::sleep_for(std::chrono::microseconds(300));
    }

    fs::remove_all(root);
    std::cout << "PASS: texture loader\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
