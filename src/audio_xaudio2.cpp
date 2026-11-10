#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmreg.h>
#include <xaudio2.h>
#include <mmdeviceapi.h>
#include <atomic>
#include <future>
#include <thread>
#include <vector>
#include "audio/audio.h"
#include "audio_output.h"
#include "audio_silent_output.h"
#include "core/log.h"
#include "host.h"
#include "launcher.h"
#include "scorpion_preview_audio.h"

namespace audio {
namespace {
struct EngineEvents final : IXAudio2EngineCallback {
    std::atomic<bool> failed{false};
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT) override { failed.store(true,std::memory_order_release); }
};
struct DeviceEvents final : IMMNotificationClient {
    std::atomic<bool> changed{false};
    std::atomic<ULONG> references{1};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** result) override {
        if (!result) return E_POINTER;
        *result=nullptr;
        if (id==__uuidof(IUnknown) || id==__uuidof(IMMNotificationClient)) { *result=this; AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,ERole,LPCWSTR) override {
        if (flow==eRender) changed.store(true,std::memory_order_release);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) override { changed.store(true,std::memory_order_release); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { changed.store(true,std::memory_order_release); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) override { return S_OK; }
};
}
void StartOutput(std::function<void(float*,uint32_t)> render) {
    namespace output=openshao::audio::output;
    if (openshao::options.background) {
        const unsigned channels=output::SilentChannels();
        output::StartSilent(std::move(render),channels);
        LOGI("audio: silent diagnostic sink, %u channels (no physical device)",channels);
        return;
    }
    std::promise<void> ready;
    auto initialized=ready.get_future();
    std::thread([render=std::move(render),ready=std::move(ready)]() mutable {
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        DeviceEvents devices;
        IMMDeviceEnumerator* enumerator=nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,
                                       __uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator))))
            enumerator->RegisterEndpointNotificationCallback(&devices);
        constexpr uint32_t frames=kRate/100,slots=4;
        bool first=true;
        for (;;) {
            devices.changed.store(false,std::memory_order_release);
            IXAudio2* engine=nullptr;
            IXAudio2MasteringVoice* master=nullptr;
            IXAudio2SourceVoice* source=nullptr;
            EngineEvents events;
            bool registered=false;
            auto release=[&] {
                if (source) source->DestroyVoice();
                if (master) master->DestroyVoice();
                if (registered) engine->UnregisterForCallbacks(&events);
                if (engine) engine->Release();
            };
            uint32_t channels=2,device_channels=2;
            DWORD mask=0;
            HRESULT status=XAudio2Create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR);
            if (SUCCEEDED(status)) { status=engine->RegisterForCallbacks(&events); registered=SUCCEEDED(status); }
            if (SUCCEEDED(status)) status=engine->CreateMasteringVoice(&master);
            if (SUCCEEDED(status)) {
                XAUDIO2_VOICE_DETAILS details{}; master->GetVoiceDetails(&details);
                device_channels=details.InputChannels; master->GetChannelMask(&mask);
                const bool fronts=(mask&0xF)==0xF;
                const bool surrounds=(mask&0x30)==0x30 || (mask&0x600)==0x600;
                channels=device_channels>=6 && fronts && surrounds ? 6 : 2;
                WAVEFORMATEXTENSIBLE format{};
                format.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
                format.Format.nChannels=WORD(channels); format.Format.nSamplesPerSec=kRate;
                format.Format.wBitsPerSample=32; format.Format.nBlockAlign=WORD(channels*sizeof(float));
                format.Format.nAvgBytesPerSec=kRate*format.Format.nBlockAlign; format.Format.cbSize=22;
                format.Samples.wValidBitsPerSample=32;
                format.dwChannelMask=channels==2 ? 3 : ((mask&0x30)==0x30 ? 0x3F : 0x60F);
                format.SubFormat={WAVE_FORMAT_IEEE_FLOAT,0,0x10,{0x80,0,0,0xAA,0,0x38,0x9B,0x71}};
                status=engine->CreateSourceVoice(&source,&format.Format);
            }
            if (SUCCEEDED(status)) status=source->Start();
            if (FAILED(status)) {
                release();
                output::SetDeviceLayout(2,0);
                if (first) { ready.set_value(); first=false; }
                LOGW("audio: playback device unavailable (0x%08X), silent stereo clock; retrying",unsigned(status));
                std::vector<float> silent(frames*2);
                for (unsigned i=0;i<200;++i) { render(silent.data(),frames); Sleep(10); }
                continue;
            }
            output::SetDeviceLayout(channels,device_channels,mask);
            if (first) { ready.set_value(); first=false; }
            LOGI("audio: XAudio2 device=%u channels mask=%08X, PCM carrier=%u",device_channels,unsigned(mask),channels);
            std::vector<float> buffers(slots*frames*channels);
            for (uint32_t n=0;!events.failed.load(std::memory_order_acquire) && !devices.changed.load(std::memory_order_acquire);) {
                XAUDIO2_VOICE_STATE queued{}; source->GetState(&queued,XAUDIO2_VOICE_NOSAMPLESPLAYED);
                if (queued.BuffersQueued>=3) { Sleep(2); continue; }
                float* block=buffers.data()+(n%slots)*frames*channels;
                master->SetVolume(openshao::launcher::AudioGain());
                render(block,frames);
                openshao::scorpion::audio_preview::Mix(block,frames,channels);
                XAUDIO2_BUFFER packet{}; packet.AudioBytes=frames*channels*sizeof(float);
                packet.pAudioData=reinterpret_cast<const BYTE*>(block);
                if (FAILED(source->SubmitSourceBuffer(&packet))) break;
                ++n;
            }
            release();
            LOGW("audio: playback device changed; reopening output");
        }
    }).detach();
    initialized.wait();
}
}
