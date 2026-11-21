#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// Which texture pack replacements to take out of video memory when they take too much. It only decides: the renderer tells
// it what it uploads and draws, and does what it answers. It knows nothing of the graphics API, so it is tested without a GPU.
// The design is in docs/RESIDENCIA_VRAM.md.
namespace openshao::texpack {
struct ResidencyLimits {
    uint64_t max_bytes = 0;
    uint32_t max_count = 0;
};

struct ResidencyOptions {
    uint32_t protect_frames = 120;  // what was drawn in the last frames stays (two seconds at 60 frames per second)
    double low_water = 0.9;         // once a limit is passed, drop to this part of it, so it is not passed again at once
    uint32_t max_per_plan = 16;     // the most one plan takes out, so no frame does all the work; the rest goes in the next ones
};

struct ResidencyPlan {
    std::vector<uint64_t> drop;  // hashes to take out, the least recently used first
    bool fits = true;            // with them gone, what is there and what comes in are within both limits
};

class Residency {
public:
    Residency() = default;
    explicit Residency(ResidencyOptions options) : options_(options) {}

    // A replacement now in video memory, last used in `frame`. Adding a hash it already has replaces its size.
    void Add(uint64_t hash, uint64_t bytes, uint64_t frame);
    // The game drew it in `frame`. A hash it does not know is ignored.
    void Touch(uint64_t hash, uint64_t frame);
    // It left video memory for another reason (a new set of packs). False when it was not there.
    bool Remove(uint64_t hash);
    void Clear();

    bool Contains(uint64_t hash) const { return items_.count(hash) != 0; }
    uint64_t bytes() const { return bytes_; }
    size_t count() const { return items_.size(); }

    // What to take out so that the replacements here and `incoming_bytes` in `incoming_count` more fit the limits. Nothing when
    // they fit already. Never a replacement drawn in the last `protect_frames` frames or added in this one; when that leaves too
    // little to take out, `fits` is false and the renderer leaves the incoming one as the game's own texture.
    ResidencyPlan Plan(uint64_t frame, const ResidencyLimits& limits, uint64_t incoming_bytes = 0, uint32_t incoming_count = 0) const;

    // How many are protected in `frame`
    size_t Protected(uint64_t frame) const;

private:
    struct Item {
        uint64_t bytes = 0, last_used = 0, order = 0;  // `order` breaks ties: the one added first goes first
    };
    bool IsProtected(const Item& item, uint64_t frame) const;
    ResidencyOptions options_;
    std::unordered_map<uint64_t, Item> items_;
    uint64_t bytes_ = 0, next_order_ = 0;
};
}
