#pragma once
#include <atomic>
#include <cstdint>

namespace openshao::audio::output {
enum class Mode : uint8_t { Mono, Stereo, Surround };
struct State {
    uint32_t channels=2, device_channels=2, speaker_mask=0;
    Mode mode=Mode::Stereo;
    Mode EffectiveMode() const { return mode==Mode::Surround && channels<6 ? Mode::Stereo : mode; }
};
// One snapshot keeps mode changes and device changes coherent for a render callback.
inline std::atomic<uint64_t> state{2ull | (2ull<<8) | (1ull<<48)};
inline State Current() {
    const auto value=state.load(std::memory_order_acquire);
    return {uint32_t(value&255),uint32_t((value>>8)&255),uint32_t(value>>16),Mode((value>>48)&3)};
}
inline void SetDeviceLayout(uint32_t channels,uint32_t device_channels,uint32_t mask=0) {
    const uint64_t layout=(channels==6 ? 6ull : 2ull) | (uint64_t(device_channels&255)<<8) | (uint64_t(mask)<<16);
    auto old=state.load(std::memory_order_relaxed);
    while (!state.compare_exchange_weak(old,(old&(3ull<<48))|layout,std::memory_order_release,std::memory_order_relaxed)) {}
}
inline void SetMode(Mode mode) {
    auto old=state.load(std::memory_order_relaxed);
    while (!state.compare_exchange_weak(old,(old&~(3ull<<48))|(uint64_t(mode)<<48),std::memory_order_release,std::memory_order_relaxed)) {}
}
inline const char* Name(Mode mode) { return mode==Mode::Mono ? "Mono" : mode==Mode::Surround ? "Surround" : "Stereo"; }
}
