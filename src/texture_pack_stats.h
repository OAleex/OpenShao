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
// A stretch of frames, for the log: what the loads cost while it lasted, and the worst frame of loading in it
struct Interval {
    uint64_t frames = 0, loads = 0, failures = 0, disk_us = 0, upload_us = 0, worst_frame_us = 0;
    uint32_t resident = 0;
    uint64_t resident_bytes = 0;
};
// The renderer calls this once a frame
void EndFrame();
// True once `frames` frames have passed since the last time, with the figures of those frames
bool TakeInterval(uint64_t frames, Interval& out);
// A mark in the log, for the player to say a scene began (the overlay's F10 asks for it); false when none is waiting
void RequestMarker();
bool TakeMarker(uint32_t& number);
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
