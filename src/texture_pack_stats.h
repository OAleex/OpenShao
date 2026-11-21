#pragma once
#include <cstdint>

// What the renderer spends on texture pack replacements, for the performance overlay.
namespace openshao::texpack {
struct Stats {
    uint32_t resident = 0;        // replacements held on the GPU now
    uint64_t resident_bytes = 0;  // their BC blocks, every level
    uint64_t loads = 0, failures = 0;
    uint64_t disk_us = 0, upload_us = 0;  // reading the pack, and recording the uploads on the render thread
    uint64_t waits = 0, wait_us = 0;      // textures that came after being asked for, and how long they took to come
    uint64_t pop_ins = 0, pop_frames = 0, pop_max = 0;  // frames a texture was seen as the game's own before its replacement
    uint64_t render_us = 0;  // what the loads cost the render thread: the uploads, and the reading when it was done there
    uint64_t ready = 0;      // textures whose replacement was already here when they were first drawn
    uint64_t evictions = 0, evicted_bytes = 0;  // replacements taken out of video memory to stay within the budget
    uint64_t reloads = 0;          // replacements loaded again after being taken out
    uint64_t no_room = 0;          // replacements that did not fit and stayed the game's own picture for a while
    uint64_t memory_failures = 0;  // images the device had no memory for
    uint64_t budget_bytes = 0;     // what the replacements may take now
    uint64_t hashes = 0, hashed_bytes = 0;  // textures hashed to look for a replacement: the cost of having a pack on
};
Stats GetStats();
// `disk_on_render_thread` is whether the reading held up the frame; the loader's threads read apart from it
void NoteLoad(uint64_t disk_us, uint64_t upload_us, bool ok, bool disk_on_render_thread = true);
void NoteWait(uint64_t wait_us);
void NotePopIn(uint64_t frames);
void NoteReadyAtFirstUse();
void NoteEviction(uint64_t count, uint64_t bytes);
void NoteReload();
void NoteNoRoom();
void NoteMemoryFailure();
void SetBudget(uint64_t bytes);
void NoteHash(uint64_t bytes);
// A stretch of frames, for the log: what the loads cost while it lasted, and the worst frame of loading in it
struct Interval {
    uint64_t frames = 0, loads = 0, failures = 0, disk_us = 0, upload_us = 0, worst_frame_us = 0;
    uint64_t waits = 0, wait_us = 0, pop_ins = 0, pop_frames = 0, pop_max = 0, ready = 0;
    uint32_t resident = 0;
    uint64_t resident_bytes = 0;
    uint64_t evictions = 0, evicted_bytes = 0, reloads = 0, no_room = 0, memory_failures = 0, budget_bytes = 0;
    uint64_t hashes = 0, hashed_bytes = 0;
};
// The renderer calls this once a frame
void EndFrame();
// True once `frames` frames have passed since the last time, with the figures of those frames
bool TakeInterval(uint64_t frames, Interval& out);
// A mark in the log, for the player to say a scene began (Ctrl+F9 on Linux asks for it); false when none is waiting
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
