#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace openshao::audio::i3dl2 {
template<size_t N> using Words = std::array<uint32_t, N>;
inline uint32_t Bits(float value) { return std::bit_cast<uint32_t>(value); }
inline float Float(uint32_t value) { return std::bit_cast<float>(value); }

template<size_t N> class Deferred {
public:
    void Set(const Words<N>& value, bool deferred) {
        if (deferred) { pending_ = value; queued_ = true; }
        else { active_ = value; configured_ = true; queued_ = false; }
    }
    bool Commit() {
        if (!queued_) return false;
        active_ = pending_; configured_ = true; queued_ = false; return true;
    }
    bool Configured() const { return configured_; }
    const Words<N>& Active() const { return active_; }
private:
    Words<N> active_{}, pending_{};
    bool configured_ = false, queued_ = false;
};

struct Source {
    int32_t direct = 0, direct_hf = 0, room = 0, room_hf = 0;
    float room_rolloff = 0;
    int32_t obstruction = 0;
    float obstruction_lf = 0;
    int32_t occlusion = 0;
    float occlusion_lf = 0;
};
struct Levels { int32_t direct_mb, room_mb, direct_hf_mb, room_hf_mb; };
inline Source Decode(const Words<9>& p) {
    return {int32_t(p[0]), int32_t(p[1]), int32_t(p[2]), int32_t(p[3]), Float(p[4]),
            int32_t(p[5]), Float(p[6]), int32_t(p[7]), Float(p[8])};
}
inline bool Range(float value, float low, float high) {
    return std::isfinite(value) && value >= low && value <= high;
}
inline bool ValidSource(const Words<9>& words) {
    const auto p = Decode(words);
    return p.direct >= -10000 && p.direct <= 1000 && p.direct_hf >= -10000 && p.direct_hf <= 0 &&
           p.room >= -10000 && p.room <= 1000 && p.room_hf >= -10000 && p.room_hf <= 0 &&
           Range(p.room_rolloff, 0, 10) && p.obstruction >= -10000 && p.obstruction <= 0 &&
           Range(p.obstruction_lf, 0, 1) && p.occlusion >= -10000 && p.occlusion <= 0 && Range(p.occlusion_lf, 0, 1);
}
inline bool ValidListener(const Words<12>& p) {
    return int32_t(p[0]) >= -10000 && int32_t(p[0]) <= 0 && int32_t(p[1]) >= -10000 && int32_t(p[1]) <= 0 &&
           Range(Float(p[2]), 0, 10) && Range(Float(p[3]), .1f, 20) && Range(Float(p[4]), .1f, 2) &&
           int32_t(p[5]) >= -10000 && int32_t(p[5]) <= 1000 && Range(Float(p[6]), 0, .3f) &&
           int32_t(p[7]) >= -10000 && int32_t(p[7]) <= 2000 && Range(Float(p[8]), 0, .1f) &&
           Range(Float(p[9]), 0, 100) && Range(Float(p[10]), 0, 100) && Range(Float(p[11]), 20, 20000);
}
// Source gain equations and truncation follow the supported title's calculator at 0x408CB0.
inline Levels Calculate(const Source& p, float distance, float room_rolloff, float) {
    const float occluded_lf = float(p.occlusion)*p.occlusion_lf;
    const float occluded_hf = float(p.occlusion)*(1-p.occlusion_lf);
    const int32_t direct = p.direct + int32_t(occluded_lf + float(p.obstruction)*p.obstruction_lf);
    const int32_t direct_hf = p.direct_hf + int32_t(occluded_hf + float(p.obstruction)*(1-p.obstruction_lf));
    const float rolloff = distance * p.room_rolloff * room_rolloff;
    const int32_t compensation = rolloff > 1 ? int32_t(2000.f*std::log10(rolloff)) : 0;
    // The native converter uses the room level for this HF coefficient as well.
    return {direct, p.room + int32_t(occluded_lf + compensation),
            std::clamp(direct_hf, -10000, 0), std::clamp(p.room + int32_t(occluded_hf), -10000, 0)};
}
inline float Gain(int32_t mb) { return mb <= -10000 ? 0.f : std::pow(10.f, mb/2000.f); }
// Native one-pole response (0x408B18), quantized to a 16-bit feedback coefficient.
inline float Pole(int32_t hf_mb, float reference) {
    if (hf_mb >= 0) return 0;
    const double g = std::pow(10.0, hf_mb/1000.0);
    const double c = std::cos(6.283185307179586 * reference/48000.0);
    const double root = std::sqrt(std::max(0.0, 2*g*(1-c)-g*g*(1-c*c)));
    const double a = (1-g*c-root)/(1-g);
    return float(std::clamp(std::floor(a*65536), 0.0, 65535.0)/65536);
}
inline float Filter(float sample, float pole, float& previous) {
    previous = (1-pole)*sample + pole*previous;
    return previous;
}
}
