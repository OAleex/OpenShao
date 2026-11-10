#include "audio_mix_bins.h"
#include "audio_speaker_mix.h"
#include "audio_spatial.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

struct Packet { uint32_t data = 1, size = 0, used = 0; };
struct Voice {
    uint32_t channels = 2, freq = 0, rate = 48000, data = 1, play_start = 0;
    uint32_t loop_start = 0, loop_len = 256, play_len = 256;
    uint8_t nbins = 0, bin[8]{};
    int32_t bin_mb[32]{};
    float volume = 1, headroom = 1, p[3]{}, min_dist = 1, max_dist = 1e9f;
    bool is3d = false, playing = true, paused = false, sync = false, stream = false;
    bool looping = true, active = true, ending = false;
    uint32_t mode = 0;
    double pos = 0;
    std::vector<float> curve;
    std::deque<Packet> packets;
};
namespace audio { constexpr uint32_t kRate = 48000; }
std::recursive_mutex g_lock;
std::map<uint32_t, Voice> g_voices;
float g_listener[3]{}, g_front[3]{0,0,1}, g_top[3]{0,1,0};
unsigned completed = 0, effect_blocks = 0;
uint32_t FramesOf(const Voice&, uint32_t bytes) { return bytes / 4; }
float Sample(Voice&, uint32_t, uint32_t frame, int channel) {
    return (channel ? -.2f : .1f) + float(frame) * .001f;
}
void Complete(Voice&, Packet&, uint32_t) { ++completed; }
float source_dry = 1, source_wet = 1;
void AudioI3DL2Gains(const Voice&, float, float& dry, float& wet) { dry = source_dry; wet = source_wet; }
struct AudioSourceEnvironment {};
AudioSourceEnvironment* AudioI3DL2State(const Voice&) { return nullptr; }
void AudioI3DL2Samples(AudioSourceEnvironment*, const float* input, float* dry, float* wet, uint32_t channels) {
    std::copy_n(input, channels, dry); std::copy_n(input, channels, wet);
}
void RenderEffectBlock(const openshao::audio::mix::Frame& buses, float* out) {
    static bool checked = false;
    if (!checked) {
        bool unlocked = false;
        std::thread other([&] { unlocked = g_lock.try_lock(); if (unlocked) g_lock.unlock(); });
        other.join();
        if (!unlocked) { std::fputs("FAIL: Effect processing must release the game's voice lock\n", stderr); std::exit(1); }
        checked = true;
    }
    ++effect_blocks;
    for (unsigned i = 0; i < 32; ++i) {
        out[2*i] = buses[0][i] + buses[10][i];
        out[2*i+1] = buses[1][i];
    }
}
void RenderEffectBlock(const openshao::audio::mix::Frame& buses, openshao::audio::SpeakerBlock& speakers) {
    float stereo[64]; RenderEffectBlock(buses,stereo);
    for (unsigned c=0;c<6;++c) speakers[c]=buses[c];
    for (unsigned i=0;i<32;++i) speakers[0][i]+=buses[10][i];
}
void AudioOutputMetrics(const float*,uint32_t,uint32_t,openshao::audio::output::Mode) {}
#include "audio_mixer.inl"

static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Reset() {
    g_voices.clear(); g_voices.emplace(1, Voice{});
    g_mix_output_at = 32; effect_blocks = completed = 0;
    g_mix_peak_limiter.Reset();
}
int main() {
    Reset();
    float estimated[2][2];
    Levels(g_voices[1],estimated);
    Check(std::abs(estimated[0][0]-1.f)<1e-6f && std::abs(estimated[1][1]-1.f)<1e-6f,
          "diagnostic stereo gains must use the same PCM headroom convention as rendering");
    std::array<float,128> whole{}, split{};
    Render(whole.data(), 64);
    Check(effect_blocks == 2 && g_voices[1].pos == 64, "one effect block consumes exactly 32 source frames");
    Check(std::abs(whole[0] - .1f) < 1e-6f && std::abs(whole[1] + .2f) < 1e-6f,
          "default stereo streams must retain their PCM gain through the buses");
    Reset();
    Render(split.data(), 7); Render(split.data()+14, 10); Render(split.data()+34, 47);
    Check(whole == split && effect_blocks == 2, "host callback boundaries do not reset effect blocks or lose samples");
    Reset();
    g_voices[1].is3d = true;
    Render(split.data(), 1);
    Check(std::abs(split[0] - .1f) < 1e-6f,
          "the first four slots of a 3D voice use the native zero-bit HRTF headroom");
    Reset();
    g_voices.emplace(2, Voice{});
    Render(split.data(), 1);
    Check(std::abs(split[0] - .2f) < 1e-6f, "render sums simultaneous voices in the same bus");
    Reset();
    auto& voice = g_voices[1]; voice.nbins = 1; voice.bin[0] = 10; voice.channels = 1;
    Render(split.data(), 1);
    Check(std::abs(split[0] - .1f) < 1e-6f, "effect-only voice retains its send gain");
    Reset();
    auto& room = g_voices[1]; room.is3d = true; room.channels = 1;
    room.nbins = 1; room.bin[0] = 10; room.bin_mb[10] = -600;
    source_wet = 2.5f;
    Render(split.data(), 1);
    Check(std::abs(split[0] - .05011872f) < 1e-6f,
          "room-distance compensation must not undo an explicit mix-bin volume");
    source_wet = 1;
    Reset();
    auto& stream = g_voices[1]; stream.stream = true; stream.looping = false;
    stream.packets.push_back({1, 16, 0}); stream.packets.push_back({2, 16, 0});
    Render(split.data(), 32);
    Check(completed == 2 && stream.packets.empty(), "stream packets complete across effect block boundaries");
    Reset();
    g_voices[1].sync = true;
    Render(split.data(), 32);
    Check(g_voices[1].pos == 0 && split[0] == 0.f, "synchronization hold does not consume samples");
    Reset();
    g_voices[1].volume = 10;
    Render(split.data(), 1);
    Check(std::abs(split[0])<=1.f && std::abs(split[1])<=1.f && std::abs(split[1]+2*split[0])<1e-5f,
          "an overloaded stereo mix must preserve the channel ratio instead of clipping each side independently");
    Reset();
    openshao::audio::output::SetDeviceLayout(6,6);
    openshao::audio::output::SetMode(openshao::audio::output::Mode::Surround);
    auto& multichannel=g_voices[1]; multichannel.channels=6; multichannel.nbins=6;
    for (unsigned c=0;c<6;++c) multichannel.bin[c]=uint8_t(c);
    std::array<float,194> surround; surround.fill(99);
    Render(surround.data(),32);
    Check(surround[0]==.1f && surround[2]==-.2f && surround[3]==-.2f && surround[5]==-.2f,
          "the actual render callback must deliver centre, LFE and rear channels separately");
    Check(surround[192]==99,"the render callback must not exceed its six-channel buffer");
    Reset();
    auto& spatial=g_voices[1]; spatial.channels=1; spatial.is3d=true; spatial.nbins=4;
    for (unsigned c=0;c<4;++c) spatial.bin[c]=uint8_t(c+6);
    spatial.p[2]=-1;
    const auto back=VoiceRouting(spatial);
    Check(back.slots[0].gain==0 && back.slots[1].gain==0 && back.slots[2].bin==4 && back.slots[2].gain>0,
          "surround positioning must bypass stereo crosstalk and put a rear source behind the listener");
    spatial.nbins=5; spatial.bin[4]=2;
    const auto rear_centre=VoiceRouting(spatial);
    Check(rear_centre.slots[4].gain==0,"a positioned rear source must not leak through its authored centre send");
    Render(surround.data(),7);
    openshao::audio::output::SetDeviceLayout(2,2);
    std::array<float,4> switched{99,99,99,99}; Render(switched.data(),1);
    Check(switched[2]==99 && switched[3]==99,"a live carrier change must invalidate the cached block and respect the new stride");
    openshao::audio::output::SetMode(openshao::audio::output::Mode::Mono);
    Render(switched.data(),1);
    Check(switched[0]==switched[1],"a live mono selection must immediately combine both sides");
    std::puts("PASS: real mixer rendering, callback continuity, bin sends, voice sums and packet lifecycle");
}
