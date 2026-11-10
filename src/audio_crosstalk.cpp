#include "audio_crosstalk.h"
#include "audio_fixed_point.h"
#include <algorithm>

namespace openshao::audio::signal {
namespace {
constexpr int64_t Fraction = int64_t(1) << 24;
int64_t ClampFilterAccumulator(int64_t value) { return std::clamp<int64_t>(value, -(int64_t(1)<<47), (int64_t(1)<<47)-1); }
void WriteHistorySample(uint32_t& word, int64_t accumulator) { word = uint32_t(TruncateProduct(accumulator)) & 0xFFFFFF; }

void CancelChannelPair(std::span<uint32_t> state, uint32_t left_ring, uint32_t right_ring,
          uint32_t left_history, uint32_t right_history, uint32_t position_word,
          std::span<const int32_t> input_left, std::span<const int32_t> input_right,
          std::span<int32_t> output_left, std::span<int32_t> output_right) {
    const int64_t feedback = DecodeSample(state[63]), cross = DecodeSample(state[64]);
    const bool enabled = (state[4] & 2) != 0;
    uint32_t position = state[position_word];
    for (uint32_t i = 0; i < 32; ++i) {
        // Full-precision sums; each history write truncates and saturates like the original filter.
        int64_t right = ClampFilterAccumulator(2*feedback*DecodeSample(state[right_history]));
        right = ClampFilterAccumulator(right + 2*cross*DecodeSample(state[left_ring+position]));
        int64_t left = ClampFilterAccumulator(2*feedback*DecodeSample(state[left_history]));
        left = ClampFilterAccumulator(left + 2*cross*DecodeSample(state[right_ring+position]));
        WriteHistorySample(state[right_history], right);
        WriteHistorySample(state[left_history], left);
        left = ClampFilterAccumulator(left + int64_t(input_left[i])*Fraction);
        right = ClampFilterAccumulator(right + int64_t(input_right[i])*Fraction);
        WriteHistorySample(state[left_ring+position], left);
        WriteHistorySample(state[right_ring+position], right);
        if (!enabled) { left = int64_t(input_left[i])*Fraction; right = int64_t(input_right[i])*Fraction; }
        const int32_t existing_left = output_left[i], existing_right = output_right[i];
        output_left[i] = TruncateProduct(ClampFilterAccumulator(left + int64_t(existing_left)*Fraction));
        output_right[i] = TruncateProduct(ClampFilterAccumulator(right + int64_t(existing_right)*Fraction));
        if (++position == 11) position = 0;
    }
    state[position_word] = position;
}
}

bool CancelSpeakerCrosstalk(std::span<uint32_t> state,
                  const std::array<std::span<const int32_t>, 4>& inputs,
                  const std::array<std::span<int32_t>, 4>& outputs) {
    if (state.size() < 65 || state[37] >= 11 || state[62] >= 11) return false;
    for (uint32_t i = 0; i < 4; ++i) if (inputs[i].size() < 32 || outputs[i].size() < 32) return false;
    if (!(state[4] & 1)) {
        state[4] |= 1;
        state[63] = 0x7645A1;
        state[64] = 0xF88313;
    }
    CancelChannelPair(state, 13, 25, 36, 24, 37, inputs[0], inputs[1], outputs[0], outputs[1]);
    CancelChannelPair(state, 38, 50, 61, 49, 62, inputs[2], inputs[3], outputs[2], outputs[3]);
    return true;
}
}
