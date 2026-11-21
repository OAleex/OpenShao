#pragma once
#include <cstdint>

// What the renderer spends on texture pack replacements, for the performance overlay.
namespace openshao::texpack {
struct Stats {
    uint32_t resident = 0;        // replacements held on the GPU now
    uint64_t resident_bytes = 0;  // their BC blocks, every level
    uint64_t loads = 0, failures = 0;
    uint64_t disk_us = 0, upload_us = 0;  // render thread time reading the pack and recording the uploads
};
Stats GetStats();
void NoteLoad(uint64_t disk_us, uint64_t upload_us, bool ok);
// Held by a texture while it holds a replacement, so the replacement counts as resident for as long as it lives.
class Resident {
public:
    explicit Resident(uint64_t bytes);
    ~Resident();
    Resident(const Resident&) = delete;
    Resident& operator=(const Resident&) = delete;
private:
    uint64_t bytes_;
};
}
