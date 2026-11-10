#pragma once
#include <cstddef>
namespace openshao::scorpion::audio_preview {
void MixMenuVoice(float* game, const float* voice_stereo, size_t frames,unsigned channels=2);
}
