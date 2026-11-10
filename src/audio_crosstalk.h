#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace openshao::audio::signal {
// Connections: front L/R then rear L/R (additive); state is the effect's 24-bit parameter/history block.
bool CancelSpeakerCrosstalk(std::span<uint32_t> state,
                  const std::array<std::span<const int32_t>, 4>& inputs,
                  const std::array<std::span<int32_t>, 4>& outputs);
}
