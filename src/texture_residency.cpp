#include "texture_residency.h"
#include <algorithm>

namespace openshao::texpack {
void Residency::Add(uint64_t hash, uint64_t bytes, uint64_t frame) {
    auto it = items_.find(hash);
    if (it != items_.end()) {
        bytes_ -= it->second.bytes;
        it->second.bytes = bytes;
        it->second.last_used = frame;
    } else {
        items_.emplace(hash, Item{bytes, frame, next_order_++});
    }
    bytes_ += bytes;
}

void Residency::Touch(uint64_t hash, uint64_t frame) {
    const auto it = items_.find(hash);
    if (it != items_.end() && frame > it->second.last_used) it->second.last_used = frame;
}

bool Residency::Remove(uint64_t hash) {
    const auto it = items_.find(hash);
    if (it == items_.end()) return false;
    bytes_ -= it->second.bytes;
    items_.erase(it);
    return true;
}

void Residency::Clear() {
    items_.clear();
    bytes_ = 0;
}

bool Residency::IsProtected(const Item& item, uint64_t frame) const {
    // drawn or added this frame, or in the last `protect_frames`; a frame from the future (the counter went back) also protects
    return item.last_used >= frame || frame - item.last_used < options_.protect_frames;
}

size_t Residency::Protected(uint64_t frame) const {
    size_t n = 0;
    for (const auto& [hash, item] : items_) n += IsProtected(item, frame);
    return n;
}

ResidencyPlan Residency::Plan(uint64_t frame, const ResidencyLimits& limits, uint64_t incoming_bytes, uint32_t incoming_count) const {
    ResidencyPlan plan;
    const bool over = bytes_ + incoming_bytes > limits.max_bytes || items_.size() + incoming_count > limits.max_count;
    if (!over) return plan;

    // down to the low-water mark of each limit, counting what comes in
    const auto target = [&](uint64_t limit, uint64_t incoming) -> uint64_t {
        const uint64_t mark = uint64_t(double(limit) * options_.low_water);
        return mark > incoming ? mark - incoming : 0;
    };
    const uint64_t target_bytes = target(limits.max_bytes, incoming_bytes);
    const uint64_t target_count = target(limits.max_count, incoming_count);

    struct Candidate {
        uint64_t hash, bytes, last_used, order;
    };
    std::vector<Candidate> candidates;
    for (const auto& [hash, item] : items_)
        if (!IsProtected(item, frame)) candidates.push_back({hash, item.bytes, item.last_used, item.order});
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.last_used != b.last_used ? a.last_used < b.last_used : a.order < b.order;
    });

    uint64_t bytes = bytes_, count = items_.size();
    for (const Candidate& c : candidates) {
        if (bytes <= target_bytes && count <= target_count) break;
        if (plan.drop.size() >= options_.max_per_plan) break;
        plan.drop.push_back(c.hash);
        bytes -= c.bytes;
        --count;
    }
    plan.fits = bytes + incoming_bytes <= limits.max_bytes && count + incoming_count <= limits.max_count;
    return plan;
}
}
