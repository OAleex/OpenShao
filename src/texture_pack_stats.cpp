#include "texture_pack_stats.h"
#include <atomic>

namespace openshao::texpack {
namespace {
std::atomic<uint32_t> resident_count{0};
std::atomic<uint64_t> resident_bytes{0}, load_count{0}, load_failures{0}, disk_us_total{0}, upload_us_total{0};
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
