#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

struct X86Context;

namespace openshao::campaign_finishers {
inline constexpr uint32_t AcquiredLabel = 0xFFFFFD04;
inline constexpr uint32_t AllyAcquiredLabel = 0xFFFFFD05;
bool BlocksTutorialWhite(unsigned player);
void Reset();
void Update(X86Context& ctx);

// Renewed by Update during an extra's gate sequence; lapses on its own if Update stops.
inline std::atomic<int64_t> pause_lock_until{0};
inline int64_t PauseLockNow() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool PauseLocked() { return PauseLockNow() < pause_lock_until.load(std::memory_order_relaxed); }
// Start and Select (Back) are removed from every port while the gate sequence runs.
inline uint16_t PauseButtons(uint16_t buttons, bool locked) {
    constexpr uint16_t Start = 0x10, Select = 0x20;
    return locked ? uint16_t(buttons & ~(Start | Select)) : buttons;
}
}
