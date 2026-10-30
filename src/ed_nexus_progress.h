#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace openshao::ed_nexus {
// Native progress block: unlock bitfield plus the original cheat flags (0x5A375C-0x5A378F).
// The save routines copy exactly these 13 DWORDs.
inline constexpr uint32_t ProgressBase = 0x5A375C, ProgressBytes = 0x34;
using Progress = std::array<uint8_t, ProgressBytes>;

inline Progress Unlock(const Progress& saved, const Progress& all) {
    Progress result{};
    for (size_t i = 0; i < result.size(); ++i) result[i] = uint8_t(saved[i] | all[i]);
    return result;
}

// Removes only the bits the session view added, keeping any change the guest made meanwhile.
inline Progress Restore(const Progress& saved, const Progress& all, const Progress& current) {
    Progress result{};
    for (size_t i = 0; i < result.size(); ++i) {
        const uint8_t forced = uint8_t(all[i] & ~saved[i]);
        result[i] = uint8_t((current[i] & ~forced) | (saved[i] & forced));
    }
    return result;
}
}
