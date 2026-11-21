#include "texture_loader.h"
#include <algorithm>

namespace openshao::texpack {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kPoolBuffers = 16, kPoolMaxCapacity = 64u << 20;

uint64_t Micros(Clock::time_point from, Clock::time_point to) {
    return uint64_t(std::max<int64_t>(0, std::chrono::duration_cast<std::chrono::microseconds>(to - from).count()));
}
}

Loader::Loader() : Loader(Options{}) {}

Loader::Loader(Options options) : options_(std::move(options)) {
    for (unsigned i = 0; i < options_.threads; ++i) threads_.emplace_back([this] { Work(); });
}

Loader::~Loader() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    for (auto& thread : threads_) thread.join();
}

// The buffers are kept for the next read; the mutex is held
static void Keep(std::vector<std::vector<uint8_t>>& pool, std::vector<uint8_t>&& buffer) {
    if (pool.size() < kPoolBuffers && buffer.capacity() && buffer.capacity() <= kPoolMaxCapacity) pool.push_back(std::move(buffer));
}

void Loader::SetPack(std::shared_ptr<const PackFile> pack, Quality quality) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++generation_;
    pack_ = std::move(pack);
    quality_ = quality;
    stats_.dropped += pending_.size() + ready_.size();
    pending_.clear();
    order_.clear();
    for (auto& result : ready_) Keep(pool_, std::move(result.blocks));
    ready_.clear();
    ready_hashes_.clear();
    stats_.ready_bytes = 0;
}

bool Loader::Request(uint64_t hash, float priority) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pack_ || !pack_->Find(hash)) return false;
        if (ready_hashes_.count(hash) || in_flight_.count(hash)) return true;
        Pending& pending = pending_[hash];
        if (pending.sequence) {
            order_.erase({pending.priority, pending.sequence, hash});
            pending.priority = std::max(pending.priority, priority);
        } else {
            pending.priority = priority;
            pending.requested = Clock::now();
        }
        pending.sequence = ++sequence_;
        pending.frame = frame_;
        order_.insert({pending.priority, pending.sequence, hash});
    }
    wake_.notify_one();
    return true;
}

bool Loader::Cancel(uint64_t hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = pending_.find(hash);
    if (it == pending_.end()) return false;
    order_.erase({it->second.priority, it->second.sequence, hash});
    pending_.erase(it);
    ++stats_.dropped;
    return true;
}

void Loader::AdvanceFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++frame_;
}

size_t Loader::TakeReady(std::vector<Loaded>& out, size_t max) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t taken = 0;
    while (taken < max && !ready_.empty()) {
        Loaded& front = ready_.front();
        ready_hashes_.erase(front.hash);
        stats_.ready_bytes -= front.blocks.size();
        out.push_back(std::move(front));
        ready_.pop_front();
        ++taken;
    }
    return taken;
}

void Loader::Recycle(std::vector<uint8_t>&& buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    Keep(pool_, std::move(buffer));
}

bool Loader::ProcessOne() {
    std::unique_lock<std::mutex> lock(mutex_);
    uint64_t hash = 0;
    Pending pending;
    for (;;) {
        if (order_.empty()) return false;
        const Key key = *order_.begin();
        order_.erase(order_.begin());
        const auto it = pending_.find(key.hash);
        pending = it->second;
        pending_.erase(it);
        hash = key.hash;
        if (frame_ - pending.frame <= options_.max_age_frames) break;
        ++stats_.dropped;  // not asked for again in a long while: the picture has moved on
    }
    in_flight_.insert(hash);
    const uint64_t generation = generation_;
    const auto pack = pack_;
    const Quality quality = quality_;
    std::vector<uint8_t> buffer;
    if (!pool_.empty()) {
        buffer = std::move(pool_.back());
        pool_.pop_back();
        ++stats_.pool_hits;
    }
    lock.unlock();

    if (options_.on_read) options_.on_read();
    Loaded result;
    result.hash = hash;
    result.pack = pack;
    result.entry = pack->Find(hash);
    const auto start = Clock::now();
    if (result.entry) {
        result.first_level = FirstLevel(*result.entry, quality);
        result.ok = pack->ReadLevels(*result.entry, result.first_level, buffer);
    }
    const auto done = Clock::now();

    lock.lock();
    in_flight_.erase(hash);
    if (generation != generation_) {  // the pack changed while this was being read
        ++stats_.dropped;
        Keep(pool_, std::move(buffer));
        return true;
    }
    result.read_us = Micros(start, done);
    result.wait_us = Micros(pending.requested, done);
    stats_.read_us += result.read_us;
    stats_.wait_us += result.wait_us;
    if (result.ok) {
        ++stats_.completed;
        stats_.ready_bytes += buffer.size();
        result.blocks = std::move(buffer);
    } else {
        ++stats_.failed;
        Keep(pool_, std::move(buffer));
    }
    ready_.push_back(std::move(result));
    ready_hashes_.insert(hash);
    while (stats_.ready_bytes > options_.max_ready_bytes && ready_.size() > 1) {
        Loaded& oldest = ready_.front();
        stats_.ready_bytes -= oldest.blocks.size();
        ready_hashes_.erase(oldest.hash);
        Keep(pool_, std::move(oldest.blocks));
        ready_.pop_front();
        ++stats_.dropped;
    }
    return true;
}

void Loader::Work() {
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !order_.empty(); });
            if (stop_) return;
        }
        ProcessOne();
    }
}

LoaderStats Loader::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    LoaderStats stats = stats_;
    stats.pending = uint32_t(pending_.size());
    stats.in_flight = uint32_t(in_flight_.size());
    stats.ready = uint32_t(ready_.size());
    return stats;
}
}
