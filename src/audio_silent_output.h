#pragma once
#include "audio_output.h"
#include <chrono>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>

namespace openshao::audio::output {
inline unsigned SilentChannels() {
    const char* value=std::getenv("OPENSHAO_AUDIO_TEST_CHANNELS");
    return value && value[0]=='6' && !value[1] ? 6 : 2;
}
// Background diagnostics pull samples without a device to keep the audio clock.
inline void StartSilent(std::function<void(float*,uint32_t)> render,unsigned channels) {
    SetDeviceLayout(channels,channels);
    std::thread([render=std::move(render),channels] {
        std::vector<float> block(480*channels);
        auto next=std::chrono::steady_clock::now();
        for (;;) {
            render(block.data(),480);
            next+=std::chrono::milliseconds(10);
            const auto now=std::chrono::steady_clock::now();
            if (next<now-std::chrono::milliseconds(100)) next=now;
            std::this_thread::sleep_until(next);
        }
    }).detach();
}
}
