#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace openshao::audio::signal {
constexpr int32_t SampleMin = -8388608, SampleMax = 8388607;
inline int32_t DecodeSample(uint32_t value) { return int32_t(value << 8) >> 8; }
inline int32_t ClampSample(int64_t value) { return int32_t(std::clamp<int64_t>(value, SampleMin, SampleMax)); }
inline int32_t TruncateProduct(int64_t value) { return ClampSample(value >> 24); }
inline int32_t RoundProduct(int64_t value) {
    int64_t whole = value >> 24;
    const uint32_t fraction = uint32_t(value) & 0xFFFFFF;
    if (fraction > 0x800000 || (fraction == 0x800000 && (whole & 1))) ++whole;
    return ClampSample(whole);
}
inline int64_t WrapAccumulator(int64_t value) { return int64_t(uint64_t(value) << 8) >> 8; }
inline int64_t ShiftAccumulator(int64_t value, unsigned bits) {
    return bits >= 56 ? 0 : WrapAccumulator(int64_t(uint64_t(value) << bits));
}
inline int32_t QuantizeSample(float value) {
    if (!std::isfinite(value)) return 0;
    return int32_t(std::clamp(value, -1.f, SampleMax/8388608.f)*8388608.f);
}
inline float NormalizeSample(int32_t value) { return value/8388608.f; }
}
