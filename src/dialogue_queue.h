#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace openshao::dialogue {
// Script voice lines rejected only because the single native voice slot was busy.
struct Line {
    uint32_t sound = 0, bank = 0, player = 0, room = 0;
    double waited = 0;
    uint32_t world = 0;
    int16_t actor_type = -1;
    int8_t actor_bank = -1;
    bool actor_bound = false;
    uint32_t actor_id = 0, scene = 0;
    bool after_cinematic = false;
};

class Queue {
public:
    static constexpr size_t Capacity = 4;
    // Requests that wait longer than this no longer match the scene that asked for them.
    static constexpr double MaxWait = 4.0;

    bool Push(const Line& line) {
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].sound == line.sound && lines_[i].bank == line.bank &&
                lines_[i].player == line.player && lines_[i].world == line.world &&
                lines_[i].room == line.room && lines_[i].actor_bound == line.actor_bound &&
                lines_[i].actor_id == line.actor_id && lines_[i].actor_type == line.actor_type &&
                lines_[i].actor_bank == line.actor_bank) return false;
        if (count_ == Capacity) return false;
        lines_[count_++] = line;
        return true;
    }
    // Time blocked by pause or another voice does not expire a valid request.
    void Age(double seconds, bool paused) {
        if (!paused) for (size_t i = 0; i < count_; ++i) lines_[i].waited += seconds;
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i) if (lines_[i].waited <= MaxWait) lines_[kept++] = lines_[i];
        count_ = kept;
    }
    void DropOtherRooms(uint32_t room, uint32_t world = 0) {
        size_t kept = 0;
        for (size_t i = 0; i < count_; ++i)
            if (lines_[i].room == room && (!world || lines_[i].world == world)) lines_[kept++] = lines_[i];
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
    size_t Size() const { return count_; }

private:
    Line lines_[Capacity]{};
    size_t count_ = 0;
};

// Mirrors the native vosound actor filter at 0x1A1953-0x1A196B.
inline bool ActorAccepts(uint32_t condition, uint16_t actor_flag) {
    return condition == 0xFFFFFFFFu || actor_flag != 0 || condition == 0;
}
}
