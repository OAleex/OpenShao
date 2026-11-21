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
// Stereo fold weights: centre and rear at -3 dB; LFE, I3DL2 and effect sends are not speaker feeds.
inline bool StereoFoldWeights(uint8_t bin, float& left, float& right) {
    constexpr float side = .70710678f;
    left = right = 0;
    switch (bin) {
    case 0: case 6: left = 1; return true;
    case 1: case 7: right = 1; return true;
    case 4: case 8: left = side; return true;
    case 5: case 9: right = side; return true;
    case 2: left = right = side; return true;
    default: return false;
    }
}
// Mono copies on several speaker bins must not add up in the fold: the louder side equals the loudest send.
inline void NormalizeStereoFold(Routing& routing, uint32_t channels) {
    if (channels != 1) return;
    float left = 0, right = 0, peak = 0;
    for (uint32_t i = 0; i < routing.count; ++i) {
        float l, r;
        if (!StereoFoldWeights(routing.slots[i].bin, l, r)) continue;
        left += routing.slots[i].gain * l;
        right += routing.slots[i].gain * r;
        peak = std::max(peak, routing.slots[i].gain);
    }
    const float fold = std::max(left, right);
    if (fold <= peak || fold <= 0) return;
    const float scale = peak / fold;
    for (uint32_t i = 0; i < routing.count; ++i) {
        float l, r;
        if (StereoFoldWeights(routing.slots[i].bin, l, r)) routing.slots[i].gain *= scale;
    }
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
