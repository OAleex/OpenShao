#include "audio_mix_bins.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace openshao::audio::mix;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Near(float a, float b) { return std::abs(a - b) < 0.00001f; }
int main() {
    Frame frame{};
    Headroom headroom;
    for (unsigned bin=0;bin<32;++bin)
        Check(Near(headroom.PcmGain(bin),1.f), "every mix bin must preserve PCM gain at its default reserve");
    Check(Near(headroom.Gain(0), .5f) && Near(headroom.Gain(30), .5f) &&
          Near(headroom.Gain(31), 1.f), "XDK defaults reserve one bit in bins 0 through 30");
    Check(headroom.Set(6, 3) && Near(headroom.Gain(6), .125f), "bin headroom is a bit shift");
    Check(!headroom.Set(32, 1) && !headroom.Set(6, 8) && Near(headroom.Gain(6), .125f),
          "invalid headroom must leave the previous gain intact");
    Check(Near(headroom.PcmGain(6), .25f), "additional reserve reduces PCM gain relative to the default");
    for (unsigned bin : {6,7,8,9,2,16}) headroom.Set(bin, 1);
    int32_t volumes[32]{};
    float samples[8] = {.25f, -.125f};
    const uint8_t bins[] = {6, 8, 7, 9, 2, 16};
    const auto stereo = BuildRouting(2, 6, bins, volumes, .5f);
    Accumulate(frame, 0, stereo, samples, headroom);
    Check(Near(frame[6][0], .125f) && Near(frame[7][0], .125f) && Near(frame[2][0], .125f),
          "even voice slots carry the source left channel irrespective of bin parity");
    Check(Near(frame[8][0], -.0625f) && Near(frame[9][0], -.0625f) && Near(frame[16][0], -.0625f),
          "odd slots carry right, including effect sends");
    Accumulate(frame, 0, stereo, samples, headroom);
    Check(Near(frame[6][0], .25f), "voices add into shared bins instead of taking a maximum");
    Check(Near(frame[6][1], 0.f), "mixing must not change the next sample");
    const uint8_t front[] = {0, 1};
    const auto mono = BuildRouting(1, 2, front, volumes, 1.f);
    Accumulate(frame, 31, mono, samples, headroom);
    Check(Near(frame[0][31], .25f) && Near(frame[1][31], .25f),
          "mono is sent once to each requested output");
    const auto implicit = BuildRouting(2, 0, nullptr, volumes, 1.f);
    Accumulate(frame, 30, implicit, samples, headroom);
    Check(Near(frame[0][30], .25f) && Near(frame[1][30], -.125f),
          "default stereo must preserve left and right independently");
    volumes[16] = -10000;
    const auto muted = BuildRouting(2, 6, bins, volumes, 1.f);
    Accumulate(frame, 2, muted, samples, headroom);
    Check(frame[16][2] == 0.f && Near(frame[8][2], -.125f), "muted sends do not mute audible bins");
    std::puts("PASS: hardware slot parity, bin summing, source channels, sends and submix headroom");
}
