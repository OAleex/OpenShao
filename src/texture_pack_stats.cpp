#include "texture_pack_stats.h"
#include <algorithm>
#include <atomic>

namespace openshao::texpack {
namespace {
std::atomic<uint32_t> resident_count{0};
std::atomic<uint64_t> resident_bytes{0}, load_count{0}, load_failures{0}, disk_us_total{0}, upload_us_total{0};
std::atomic<uint64_t> frame_us{0};  // the loads of the frame in progress
std::atomic<uint32_t> markers_asked{0}, markers_taken{0};
// Kept by the renderer thread, which is the one that ends the frames
struct Since {
    uint64_t frames = 0, worst_frame_us = 0, loads = 0, failures = 0, disk_us = 0, upload_us = 0;
} since;
}

Stats GetStats() {
    return {resident_count.load(std::memory_order_relaxed), resident_bytes.load(std::memory_order_relaxed),
            load_count.load(std::memory_order_relaxed), load_failures.load(std::memory_order_relaxed),
            disk_us_total.load(std::memory_order_relaxed), upload_us_total.load(std::memory_order_relaxed)};
}

void NoteLoad(uint64_t disk_us, uint64_t upload_us, bool ok) {
    (ok ? load_count : load_failures).fetch_add(1, std::memory_order_relaxed);
    disk_us_total.fetch_add(disk_us, std::memory_order_relaxed);
    upload_us_total.fetch_add(upload_us, std::memory_order_relaxed);
    frame_us.fetch_add(disk_us + upload_us, std::memory_order_relaxed);
}

void EndFrame() {
    since.worst_frame_us = std::max(since.worst_frame_us, frame_us.exchange(0, std::memory_order_relaxed));
    ++since.frames;
}

bool TakeInterval(uint64_t frames, Interval& out) {
    if (since.frames < frames) return false;
    const Stats now = GetStats();
    out.frames = since.frames;
    out.loads = now.loads - since.loads;
    out.failures = now.failures - since.failures;
    out.disk_us = now.disk_us - since.disk_us;
    out.upload_us = now.upload_us - since.upload_us;
    out.worst_frame_us = since.worst_frame_us;
    out.resident = now.resident;
    out.resident_bytes = now.resident_bytes;
    since = {0, 0, now.loads, now.failures, now.disk_us, now.upload_us};
    return true;
}

void RequestMarker() { markers_asked.fetch_add(1, std::memory_order_relaxed); }

bool TakeMarker(uint32_t& number) {
    if (markers_taken.load(std::memory_order_relaxed) >= markers_asked.load(std::memory_order_relaxed)) return false;
    number = markers_taken.fetch_add(1, std::memory_order_relaxed) + 1;
    return true;
}

Resident::Resident(uint64_t bytes) : bytes_(bytes) {
    resident_count.fetch_add(1, std::memory_order_relaxed);
    resident_bytes.fetch_add(bytes_, std::memory_order_relaxed);
}

Resident::~Resident() {
    resident_count.fetch_sub(1, std::memory_order_relaxed);
    resident_bytes.fetch_sub(bytes_, std::memory_order_relaxed);
}
}
