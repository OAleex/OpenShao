#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace openshao::audio {
// Four-channel room network. Delay storage runs at 24 kHz; the host supplies 48 kHz blocks.
class RoomReverb {
public:
    bool Process(std::span<uint32_t> parameters, uint32_t parameter_address,
                 std::span<int32_t,32> input, std::array<std::array<int32_t,32>,4>& output);
private:
    std::array<std::vector<int32_t>,14> delays_;
    std::array<std::array<int32_t,16>,31> blocks_{};
    bool Initialize(std::span<uint32_t>, uint32_t);
    void ReadDelayTaps(std::span<uint32_t>);
    void WriteDelayLines(std::span<uint32_t>);
    void UpdateParameters(std::span<uint32_t>, uint32_t);
};
}
