#include "scorpion_preview_mix.h"
#include <algorithm>
namespace openshao::scorpion::audio_preview {
void MixMenuVoice(float* game, const float* voice_stereo, size_t frames,unsigned channels) {
    // Attenuate the menu bed under the preview so the sum never clips.
    constexpr float menu_gain = 0.30f;
    constexpr float preview_gain = 0.65f;
    for (size_t i = 0; i < frames; ++i) {
        for (unsigned c=0;c<channels;++c) game[i*channels+c]*=menu_gain;
        game[i*channels]+=voice_stereo[2*i]*preview_gain;
        game[i*channels+1]+=voice_stereo[2*i+1]*preview_gain;
    }
}
}
