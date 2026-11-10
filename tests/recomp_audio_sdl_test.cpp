#include <cstdio>
#include <initializer_list>
#if __has_include("audio_sdl_stream.h")
#include "audio_sdl_stream.h"
#include <cstdlib>
static void Check(bool ok,const char* message) { if (!ok) { std::fprintf(stderr,"FAIL: %s (%s)\n",message,SDL_GetError()); std::exit(1); } }
int main() {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    for (int channels:{2,6}) {
        Check(SDL_InitSubSystem(SDL_INIT_AUDIO),"silent driver must initialize");
        SDL_AudioSpec spec{SDL_AUDIO_F32,channels,48000};
        auto* stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        Check(stream,"silent playback stream must open");
        Check(openshao::audio::output::RefreshStreamLayout(stream)==unsigned(channels),"carrier must follow the actual device format");
        const auto state=openshao::audio::output::Current();
        Check(state.channels==unsigned(channels) && state.device_channels==unsigned(channels),"published device format must match the actual silent device");
        Check(openshao::audio::output::RequestedFrames(480*channels*int(sizeof(float)),unsigned(channels))==480,"callback accounting must use input frames, independently of a later layout change");
        if (channels==6) {
            SDL_AudioSpec stereo{SDL_AUDIO_F32,2,48000};
            Check(SDL_SetAudioStreamFormat(stream,&stereo,nullptr),"a live stream input format must be changeable");
            Check(openshao::audio::output::RequestedFrames(3840,2)==480 && openshao::audio::output::RequestedFrames(3840,6)==160,"an old byte request must keep its frame count across a stereo-to-surround change");
            Check(openshao::audio::output::RefreshStreamLayout(stream)==6,"device negotiation must restore a six-channel input after migration");
            SDL_AudioSpec restored{}; Check(SDL_GetAudioStreamFormat(stream,&restored,nullptr) && restored.channels==6,"the live stream must receive PCM in the published carrier format");
        }
        SDL_DestroyAudioStream(stream); SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    std::puts("PASS: actual SDL dummy devices negotiate stereo and surround, with frame-based callbacks");
}
#else
int main() { std::fputs("FAIL: device format negotiation is unavailable\n",stderr); return 1; }
#endif
