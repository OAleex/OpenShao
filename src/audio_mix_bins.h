#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace openshao::audio::mix {
constexpr uint32_t BinCount = 32, FrameSamples = 32;
using Frame = std::array<std::array<float, FrameSamples>, BinCount>;

class Headroom {
public:
    Headroom() { gains_.fill(.5f); gains_[31] = 1.f; }
    bool Set(uint32_t bin, uint32_t bits) {
        if (bin >= BinCount || bits > 7) return false;
        gains_[bin] = std::ldexp(1.f, -int(bits));
        return true;
    }
    float Gain(uint32_t bin) const { return gains_[bin]; }
    // Bus reserve is fixed-point headroom, not volume: unity at driver defaults.
    float PcmGain(uint32_t bin) const { return gains_[bin] * (bin == 31 ? 1.f : 2.f); }
private:
    std::array<float, BinCount> gains_;
};

struct Slot { uint8_t bin = 0, channel = 0; float gain = 0; };
struct Routing { std::array<Slot, 8> slots{}; uint32_t count = 0; };
inline Routing BuildRouting(uint32_t channels, uint32_t count, const uint8_t* bins,
                            const int32_t* volumes, float voice_gain) {
    static constexpr uint8_t front[] = {0, 1};
    if (!count) { count = 2; bins = front; }
    Routing routing;
    routing.count = std::min(count, 8u);
    channels = std::clamp(channels, 1u, 8u);
    for (uint32_t i = 0; i < routing.count; ++i) {
        const uint8_t bin = bins[i] & 31;
        const int32_t mb = volumes[bin];
        const float gain = mb <= -10000 ? 0.f : std::pow(10.f, mb / 2000.f);
        routing.slots[i] = {bin, uint8_t(i % channels), gain * voice_gain};
    }
    return routing;
}
inline void Accumulate(Frame& frame, uint32_t sample, const Routing& routing,
                       const float* input, const Headroom& headroom, bool spatial_voice = false,
                       const float* wet_input = nullptr) {
    for (uint32_t i = 0; i < routing.count; ++i) {
        const auto& slot = routing.slots[i];
        // The native driver sets HRTF headroom to zero; hardware 3D slots 0..3 use it.
        const float reserve = spatial_voice && i < 4 ? 1.f : headroom.PcmGain(slot.bin);
        const float* source = slot.bin == 10 && wet_input ? wet_input : input;
        frame[slot.bin][sample] += source[slot.channel] * slot.gain * reserve;
    }
}
}
