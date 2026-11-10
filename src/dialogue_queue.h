#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace openshao::dialogue {
// Script voice lines rejected only because the single native voice slot was busy.
struct Line {
    uint32_t sound = 0, bank = 0, player = 0, room = 0;
    double waited = 0;
    double blocked = 0;  // time spent behind another voice, a scene or loading
    uint32_t world = 0;
    int16_t actor_type = -1;
    int8_t actor_bank = -1;
    bool actor_bound = false;
    uint32_t actor_id = 0, scene = 0;
    bool after_cinematic = false;
    bool wait_for_streams = false;
    uint32_t source = 0;  // Native trigger definition; deactivation expires its pending speech.
    bool timed = false;     // A deferred native request: it belongs to the moment it was made.
    bool optional = false;  // Our own addition: native speech may interrupt it.
    uint32_t behind = 0;    // Voice that blocked a timed request; waiting for it is not late.
    double late = 0;        // Unpaused time spent waiting on anything else.
};

class Queue {
public:
    static constexpr size_t Capacity = 4;
    // Requests that wait longer than this no longer match the scene that asked for them.
    static constexpr double MaxWait = 4.0;
    // Loading and other non-speech blocking cannot keep an obsolete request indefinitely.
    static constexpr double MaxBlocked = 8.0;
    // A deferred native line may wait out the voice it was blocked by, then this much more.
    static constexpr double MaxLate = 2.0;

    bool Push(const Line& line) {
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].sound == line.sound && lines_[i].bank == line.bank &&
                lines_[i].player == line.player && lines_[i].world == line.world &&
                lines_[i].room == line.room && lines_[i].actor_bound == line.actor_bound &&
                lines_[i].actor_id == line.actor_id && lines_[i].actor_type == line.actor_type &&
                lines_[i].actor_bank == line.actor_bank) return false;
        // Native requests go ahead of our optional additions and may displace the last one.
        if (count_ == Capacity) {
            size_t victim = count_;
            for (size_t i = count_; !line.optional && i-- > 0;)
                if (lines_[i].optional) { victim = i; break; }
            if (victim == count_) return false;
            for (size_t i = victim + 1; i < count_; ++i) lines_[i - 1] = lines_[i];
            --count_;
        }
        size_t at = count_;
        for (size_t i = 0; !line.optional && i < count_; ++i)
            if (lines_[i].optional) { at = i; break; }
        for (size_t i = count_; i > at; --i) lines_[i] = lines_[i - 1];
        lines_[at] = line;
        ++count_;
        return true;
    }
    // Speech holds the channel until it ends; other blocking expires.
    size_t Age(double seconds, bool paused, bool blocked = false, bool cinematic = false, bool speech = false,
             uint32_t voice = 0) {
        if (!paused) for (size_t i = 0; i < count_; ++i) {
            if (!cinematic && !(voice && voice == lines_[i].behind)) lines_[i].late += seconds;
            if (!blocked) lines_[i].waited += seconds;
            else if (!speech && !(cinematic && lines_[i].after_cinematic)) lines_[i].blocked += seconds;
        }
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].waited <= MaxWait && lines_[i].blocked <= MaxBlocked &&
                (!lines_[i].timed || lines_[i].late <= MaxLate)) lines_[kept++] = lines_[i];
        const size_t expired = count_ - kept;
        count_ = kept;
        return expired;
    }
    void DropOtherRooms(uint32_t room, uint32_t world = 0) {
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].room == room && (!world || lines_[i].world == world)) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    void DropOtherScenes(uint32_t scene) {
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].scene == scene) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    void EnterCinematic() {
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].after_cinematic) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    std::optional<Line> Pop() {
        if (!count_) return std::nullopt;
        const Line first = lines_[0];
        for (size_t i = 1; i < count_; ++i) lines_[i - 1] = lines_[i];
        --count_;
        return first;
    }
    void Clear() { count_ = 0; }
    void DropSource(uint32_t source) {
        if (!source) return;
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].source != source) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    void DropSound(uint32_t sound) {
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].sound != sound) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    size_t Size() const { return count_; }
    // Whether a native or restored line waits; optional additions alone never hold back native speech.
    bool HasRequired() const {
        for (size_t i = 0; i < count_; ++i) if (!lines_[i].optional) return true;
        return false;
    }
    bool WaitsForStreams() const { return !count_ || lines_[0].wait_for_streams; }

private:
    Line lines_[Capacity]{};
    size_t count_ = 0;
};

// Silence before a restored line after speech or a cinematic/loading (zero in tests).
void SetRestoreGaps(double voice, double scene);

// Scene audio audit: called every gameplay frame; logs the voices heard while a scene's window is open.
void AuditTick();

// Mirrors the native vosound actor filter at 0x1A1953-0x1A196B.
inline bool ActorAccepts(uint32_t condition, uint16_t actor_flag) {
    return condition == 0xFFFFFFFFu || actor_flag != 0 || condition == 0;
}
}
