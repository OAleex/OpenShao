#pragma once
#include "audio_output.h"
#include <SDL3/SDL.h>

namespace openshao::audio::output {
inline uint32_t RequestedFrames(int bytes,uint32_t channels) {
    const uint32_t stride=channels*sizeof(float);
    return bytes>0 ? (uint32_t(bytes)+stride-1)/stride : 0;
}
inline uint32_t RefreshStreamLayout(SDL_AudioStream* stream) {
    SDL_AudioSpec actual{},input{};
    if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream),&actual,nullptr) ||
        !SDL_GetAudioStreamFormat(stream,&input,nullptr)) return 0;
    const int channels=actual.channels>=6 ? 6 : 2;
    if (input.channels!=channels) {
        input.channels=channels;
        if (!SDL_SetAudioStreamFormat(stream,&input,nullptr)) return 0;
    }
    SetDeviceLayout(uint32_t(channels),uint32_t(actual.channels));
    return uint32_t(channels);
}
}
