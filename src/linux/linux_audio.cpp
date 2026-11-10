#include <SDL3/SDL.h>

#include <functional>
#include <vector>

#include "audio/audio.h"
#include "audio_sdl_stream.h"
#include "audio_silent_output.h"
#include "core/log.h"
#include "host.h"
#include "launcher.h"
#include "scorpion_preview_audio.h"

namespace audio {
namespace {
struct Output {
    std::function<void(float*, uint32_t)> render;
    std::vector<float> block = std::vector<float>(kRate / 100 * 6);
};

void SDLCALL Feed(void* data, SDL_AudioStream* stream, int additional, int) {
    auto* out = static_cast<Output*>(data);
    namespace output=openshao::audio::output;
    const auto before=output::Current();
    // Convert SDL's byte request to frames before a device migration changes the format.
    uint32_t pending=output::RequestedFrames(additional,before.channels);
    const uint32_t channels=output::RefreshStreamLayout(stream);
    if (!channels) { LOG_FIRST(1,"audio: cannot update SDL device layout: %s",SDL_GetError()); return; }
    const auto current=output::Current();
    if (before.channels!=current.channels || before.device_channels!=current.device_channels)
        LOGI("audio: SDL device=%u channels, PCM carrier=%u",current.device_channels,channels);
    SDL_SetAudioStreamGain(stream, openshao::launcher::AudioGain());
    constexpr uint32_t kFrames = kRate / 100;
    while (pending) {
        const uint32_t frames=std::min(pending,kFrames);
        out->render(out->block.data(), frames);
        openshao::scorpion::audio_preview::Mix(out->block.data(), frames,channels);
        SDL_PutAudioStreamData(stream, out->block.data(), int(frames*channels*sizeof(float)));
        pending-=frames;
    }
}
}

void StartOutput(std::function<void(float*, uint32_t)> render) {
    namespace output=openshao::audio::output;
    if (openshao::options.background) {
        const unsigned channels=output::SilentChannels();
        output::StartSilent(std::move(render),channels);
        LOGI("audio: silent diagnostic sink, %u channels (no physical device)",channels);
        return;
    }
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        LOGE("audio: SDL audio unavailable (%s), silent stereo clock", SDL_GetError());
        output::StartSilent(std::move(render),2);
        return;
    }
    static Output device;  // lives for the process, as the XAudio2 thread does
    device.render = std::move(render);
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 6, int(kRate)};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, &device);
    if (!stream) {
        const SDL_AudioSpec stereo{SDL_AUDIO_F32,2,int(kRate)};
        stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&stereo,Feed,&device);
    }
    if (!stream) {
        LOGE("audio: no playback device (%s), playing silently", SDL_GetError());
        openshao::audio::output::StartSilent(device.render,2);
        return;
    }
    const uint32_t channels=openshao::audio::output::RefreshStreamLayout(stream);
    if (!channels) {
        LOGE("audio: cannot query playback layout (%s), playing silently",SDL_GetError());
        SDL_DestroyAudioStream(stream);
        openshao::audio::output::StartSilent(device.render,2);
        return;
    }
    LOGI("audio: SDL device=%u channels, PCM carrier=%u",openshao::audio::output::Current().device_channels,channels);
    SDL_SetAudioStreamGain(stream, openshao::launcher::AudioGain());
    SDL_ResumeAudioStreamDevice(stream);
}

}
