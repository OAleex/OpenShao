#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "core/input.h"

namespace openshao::netplay {
inline constexpr unsigned kPorts = 4;
// One port's controller report as it is recorded and sent: 20 bytes, little endian.
struct PadSample {
    uint8_t connected = 0, reserved = 0;
    uint16_t buttons = 0;
    std::array<uint8_t, 8> analog{};
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    friend bool operator==(const PadSample&, const PadSample&) = default;
};
inline constexpr size_t kPadBytes = 20;
using FrameSample = std::array<PadSample, kPorts>;
inline constexpr size_t kFrameBytes = kPadBytes * kPorts;

inline PadSample FromPad(const input::Pad& p) {
    PadSample s;
    s.connected = p.connected ? 1 : 0;
    s.buttons = p.buttons;
    std::memcpy(s.analog.data(), p.analog, 8);
    s.lx = p.lx; s.ly = p.ly; s.rx = p.rx; s.ry = p.ry;
    return s;
}
inline input::Pad ToPad(const PadSample& s) {
    input::Pad p;
    p.connected = s.connected != 0;
    p.buttons = s.buttons;
    std::memcpy(p.analog, s.analog.data(), 8);
    p.lx = s.lx; p.ly = s.ly; p.rx = s.rx; p.ry = s.ry;
    return p;
}
inline void Encode(const FrameSample& frame, uint8_t* out) {
    for (const auto& s : frame) {
        out[0] = s.connected; out[1] = 0;
        out[2] = uint8_t(s.buttons); out[3] = uint8_t(s.buttons >> 8);
        std::memcpy(out + 4, s.analog.data(), 8);
        const int16_t axes[4]{s.lx, s.ly, s.rx, s.ry};
        for (unsigned i = 0; i < 4; ++i) {
            const auto v = uint16_t(axes[i]);
            out[12 + i * 2] = uint8_t(v); out[13 + i * 2] = uint8_t(v >> 8);
        }
        out += kPadBytes;
    }
}
// Rejects reserved bits so a corrupt sample cannot reach the game.
inline bool Decode(const uint8_t* in, FrameSample& frame) {
    for (auto& s : frame) {
        if (in[0] > 1 || in[1]) return false;
        s.connected = in[0];
        s.buttons = uint16_t(in[2] | in[3] << 8);
        std::memcpy(s.analog.data(), in + 4, 8);
        int16_t* axes[4]{&s.lx, &s.ly, &s.rx, &s.ry};
        for (unsigned i = 0; i < 4; ++i) *axes[i] = int16_t(uint16_t(in[12 + i * 2] | in[13 + i * 2] << 8));
        in += kPadBytes;
    }
    return true;
}

// The sample every game thread reads between two frame boundaries. Publication replaces all
// ports at once, so no reader can combine ports from different frames.
class FrameInputs {
public:
    void Publish(uint32_t frame, const FrameSample& sample) {
        std::lock_guard guard(lock_);
        frame_ = frame; sample_ = sample; published_ = true;
    }
    bool Published() const { std::lock_guard guard(lock_); return published_; }
    uint32_t Frame() const { std::lock_guard guard(lock_); return frame_; }
    input::Pad Pad(int port) const {
        std::lock_guard guard(lock_);
        return port >= 0 && unsigned(port) < kPorts ? ToPad(sample_[unsigned(port)]) : input::Pad{};
    }
private:
    mutable std::mutex lock_;
    FrameSample sample_{};
    uint32_t frame_ = 0;
    bool published_ = false;
};
}  // namespace openshao::netplay
