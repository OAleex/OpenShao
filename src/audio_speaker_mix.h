#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "audio_output.h"

namespace openshao::audio {
// Speaker order follows the mix bins: front L/R, centre, LFE, rear L/R.
using SpeakerBlock=std::array<std::array<float,32>,6>;

inline void DownmixStereo(const SpeakerBlock& speakers,float stereo[64]) {
    constexpr float surround_gain=.7071067811865475f;
    for (uint32_t i=0;i<32;++i) {
        const float centre=speakers[2][i]*surround_gain;
        stereo[2*i]=speakers[0][i]+centre+speakers[4][i]*surround_gain;
        stereo[2*i+1]=speakers[1][i]+centre+speakers[5][i]*surround_gain;
    }
}

inline void FormatSpeakers(const SpeakerBlock& speakers,float* pcm,uint32_t channels,output::Mode mode) {
    if (channels==6 && mode==output::Mode::Surround) {
        for (uint32_t i=0;i<32;++i) for (uint32_t c=0;c<6;++c) pcm[6*i+c]=speakers[c][i];
        return;
    }
    float stereo[64]; DownmixStereo(speakers,stereo);
    for (uint32_t i=0;i<32;++i) {
        const float mono=(stereo[2*i]+stereo[2*i+1])*.5f;
        pcm[channels*i]=mode==output::Mode::Mono ? mono : stereo[2*i];
        pcm[channels*i+1]=mode==output::Mode::Mono ? mono : stereo[2*i+1];
        for (uint32_t c=2;c<channels;++c) pcm[channels*i+c]=0;
    }
}

class SpeakerPeakLimiter {
public:
    void Reset() { gain_=1.f; }
    void Process(float* pcm,uint32_t frames,uint32_t channels=2) {
        constexpr float ceiling=32767.f/32768.f;
        // Fifty-millisecond release at the mixer's fixed 48 kHz output rate.
        constexpr float release_step=.000416579873f;
        for (uint32_t i=0;i<frames;++i) {
            float peak=0;
            for (uint32_t c=0;c<channels;++c) {
                float& sample=pcm[channels*i+c];
                if (!std::isfinite(sample)) sample=0;
                peak=std::max(peak,std::abs(sample));
            }
            const float target=peak>ceiling ? ceiling/peak : 1.f;
            if (target<gain_) gain_=target;
            else { gain_+=(1.f-gain_)*release_step; gain_=std::min(gain_,target); }
            if (gain_>.9999f && target==1.f) gain_=1.f;
            for (uint32_t c=0;c<channels;++c) pcm[channels*i+c]*=gain_;
        }
    }
private:
    float gain_=1.f;
};
}
