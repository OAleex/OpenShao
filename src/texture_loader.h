#pragma once
#include "texture_pack_source.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Reads the blocks of the textures a renderer asks for from a texture pack, on threads of its own, so that reading never
// waits on the thread that draws. The renderer asks, later takes what is ready and gives the buffers back. Knows nothing
// of the graphics API.
namespace openshao::texpack {
struct Loaded {
    uint64_t hash = 0;
    std::shared_ptr<const PackSource> pack;
    SourceEntry entry;
    uint32_t first_level = 0;  // the level the blocks start at: the quality may skip the largest ones
    bool ok = false;           // false: the file could not be read, and `blocks` is empty
    std::vector<uint8_t> blocks;
    uint64_t read_us = 0;  // time spent reading
    uint64_t wait_us = 0;  // from the request to the blocks being ready
};

struct LoaderStats {
    uint32_t pending = 0, in_flight = 0, ready = 0;
    uint64_t ready_bytes = 0;
    uint64_t completed = 0, failed = 0;
    uint64_t dropped = 0;  // asked for and never delivered: cancelled, too old, from an earlier pack or over the limit
    uint64_t read_us = 0, wait_us = 0;
    uint64_t pool_hits = 0;  // reads that reused a buffer given back
};

class Loader {
public:
    struct Options {
        unsigned threads = 2;                      // 0: nothing is read until ProcessOne is called
        uint64_t max_ready_bytes = 256ull << 20;   // ready blocks nobody took yet; the oldest go first
        uint32_t max_age_frames = 240;             // a request not made again for this many frames is not read
        std::function<void()> on_read;             // called by the reading thread before each read (for tests)
    };

    Loader();
    explicit Loader(Options options);
    ~Loader();
    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;

    // The pack to read from and the quality. Requests and results of an earlier pack are dropped; a read in progress
    // finishes and its result is dropped.
    void SetPack(std::shared_ptr<const PackSource> pack, Quality quality);
    // Asks for a texture. False when the pack has no such texture. Asking again raises the priority, keeps the request
    // from growing old and makes it the most recent of its priority. Higher priorities are read first, and among equal
    // ones the most recent request.
    bool Request(uint64_t hash, float priority = 0);
    // Drops a request that is not being read yet
    bool Cancel(uint64_t hash);
    // The renderer calls this once a frame; it is how requests grow old
    void AdvanceFrame();
    // Moves up to `max` ready results, the oldest first, to the end of `out`
    size_t TakeReady(std::vector<Loaded>& out, size_t max = size_t(-1));
    // Gives a buffer back to be reused
    void Recycle(std::vector<uint8_t>&& buffer);
    // Reads the best pending request on the calling thread. False when there is none.
    bool ProcessOne();
    LoaderStats GetStats() const;

private:
    struct Pending {
        float priority = 0;
        uint64_t sequence = 0;
        uint64_t frame = 0;
        std::chrono::steady_clock::time_point requested;
    };
    struct Key {
        float priority;
        uint64_t sequence;
        uint64_t hash;
        bool operator<(const Key& o) const {
            return priority != o.priority ? priority > o.priority : sequence != o.sequence ? sequence > o.sequence : hash < o.hash;
        }
    };
    void Work();

    const Options options_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
    std::shared_ptr<const PackSource> pack_;
    Quality quality_ = Quality::High;
    uint64_t generation_ = 0, sequence_ = 0, frame_ = 0;
    std::unordered_map<uint64_t, Pending> pending_;
    std::set<Key> order_;
    std::unordered_set<uint64_t> in_flight_;
    std::deque<Loaded> ready_;
    std::unordered_set<uint64_t> ready_hashes_;
    std::vector<std::vector<uint8_t>> pool_;
    LoaderStats stats_;
    std::vector<std::thread> threads_;
};
}
