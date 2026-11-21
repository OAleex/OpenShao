#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace openshao::camera {
using Angles = std::array<float, 2>;

// The gameplay camera consumes one base yaw/pitch pair per native update.
class Framing {
    Angles previous{}, filtered{};
    uint32_t world = 0, room = 0, camera = 0;
public:
    void Reset() { world = 0; }

    Angles Update(Angles target, uint32_t next_world, uint32_t next_room, uint32_t next_camera) {
        constexpr float turn = 2 * std::numbers::pi_v<float>;
        bool continuous = world == next_world && room == next_room && camera == next_camera;
        for (size_t i = 0; i < target.size(); ++i)
            continuous = continuous && std::isfinite(target[i]) &&
                std::abs(std::remainder(target[i] - previous[i], turn)) < .35f;
        world = next_world;
        room = next_room;
        camera = next_camera;
        previous = target;
        if (!continuous) { filtered = target; return target; }

        for (size_t i = 0; i < target.size(); ++i) {
            const float delta = std::remainder(target[i] - filtered[i], turn);
            filtered[i] = target[i] - delta * .74f;
        }
        return filtered;
    }
};
}
