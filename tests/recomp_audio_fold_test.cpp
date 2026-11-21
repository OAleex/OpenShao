#include "audio_mix_bins.h"
#include "audio_spatial.h"
#include "audio_speaker_mix.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace openshao::audio;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Near(float a, float b) { return std::abs(a - b) < 1e-5f; }

// Bins to speakers as the dry effect path does: XTLK pairs add onto front and rear.
static SpeakerBlock Speakers(const mix::Frame& frame) {
    SpeakerBlock speakers{};
    for (unsigned i = 0; i < 32; ++i) {
        for (unsigned c = 0; c < 6; ++c) speakers[c][i] = frame[c][i];
        speakers[0][i] += frame[6][i]; speakers[1][i] += frame[7][i];
        speakers[4][i] += frame[8][i]; speakers[5][i] += frame[9][i];
    }
    return speakers;
}
static void Render(const mix::Routing& routing, const float* input, output::Mode mode, float* pcm, uint32_t channels) {
    mix::Frame frame{};
    mix::Headroom headroom;
    mix::Accumulate(frame, 0, routing, input, headroom, true);
    FormatSpeakers(Speakers(frame), pcm, channels, mode);
}

int main() {
    // The game sends 3D effects (footsteps, hits) at 0 mB to XTLK FL/BL/FR/BR, centre, I3DL2 and LFE.
    const uint8_t effect_bins[] = {6, 8, 7, 9, 2, 10, 3};
    int32_t volumes[32]{};
    const float level = .197f, sample[8] = {1.f};
    auto effect = mix::BuildRouting(1, 7, effect_bins, volumes, level);
    float pcm[192]{};
    Render(effect, sample, output::Mode::Stereo, pcm, 2);
    Check(Near(pcm[0], level * 2.41421356f), "without normalization the stereo fold adds the copies (+7.7 dB)");

    mix::NormalizeStereoFold(effect, 1);
    Render(effect, sample, output::Mode::Stereo, pcm, 2);
    Check(Near(pcm[0], level) && Near(pcm[1], level), "a mono effect sent equally to every speaker plays at its requested level in stereo");
    Render(effect, sample, output::Mode::Mono, pcm, 2);
    Check(Near(pcm[0], level) && Near(pcm[1], level), "the same effect plays at its requested level in mono");
    Check(Near(effect.slots[5].gain, level) && Near(effect.slots[6].gain, level), "reverb and LFE sends keep their level");

    // A panned 3D source: the quiet side drops, the loud side still equals the request.
    auto panned = mix::BuildRouting(1, 7, effect_bins, volumes, 1.f);
    for (uint32_t i = 0; i < panned.count; ++i)
        if (panned.slots[i].bin == 7 || panned.slots[i].bin == 9) panned.slots[i].gain = .25f;
    mix::NormalizeStereoFold(panned, 1);
    Render(panned, sample, output::Mode::Stereo, pcm, 2);
    Check(Near(pcm[0], 1.f) && pcm[1] < pcm[0], "a panned effect keeps its direction and peaks at its requested level");

    // Music and speech: front bins only, never changed.
    const uint8_t front[] = {0, 1};
    auto music = mix::BuildRouting(2, 2, front, volumes, .8f);
    const auto music_before = music;
    mix::NormalizeStereoFold(music, 2);
    const float stereo_sample[8] = {.5f, -.25f};
    Render(music, stereo_sample, output::Mode::Stereo, pcm, 2);
    Check(Near(music.slots[0].gain, music_before.slots[0].gain) && Near(pcm[0], .4f) && Near(pcm[1], -.2f),
          "stereo music keeps its exact level and separation");
    auto speech = mix::BuildRouting(1, 2, front, volumes, .9f);
    mix::NormalizeStereoFold(speech, 1);
    Render(speech, sample, output::Mode::Stereo, pcm, 2);
    Check(Near(pcm[0], .9f) && Near(pcm[1], .9f), "mono speech on the front pair keeps its level");
    auto implicit = mix::BuildRouting(1, 0, nullptr, volumes, .9f);
    mix::NormalizeStereoFold(implicit, 1);
    Check(Near(implicit.slots[0].gain, .9f) && Near(implicit.slots[1].gain, .9f), "voices without mix bins are untouched");

    // A centre-only send is already below the request in stereo and must not be boosted.
    const uint8_t centre_bin[] = {2};
    auto centre = mix::BuildRouting(1, 1, centre_bin, volumes, 1.f);
    mix::NormalizeStereoFold(centre, 1);
    Check(Near(centre.slots[0].gain, 1.f), "normalization never boosts a send");

    // 5.1: the mixer pans a positionless 3D voice to the centre speaker alone, at full level.
    const auto pan = SurroundPan(0, 0, true);
    Check(Near(pan[4], 1.f) && pan[0] == 0 && pan[1] == 0 && pan[2] == 0 && pan[3] == 0,
          "a centred 3D effect plays once, on the centre speaker, in 5.1");
    float surround[192]{};
    mix::Frame frame{};
    frame[2][0] = level;
    FormatSpeakers(Speakers(frame), surround, 6, output::Mode::Surround);
    Check(Near(surround[2], level) && surround[0] == 0 && surround[1] == 0 && surround[4] == 0 && surround[5] == 0,
          "5.1 output carries the centre feed unchanged");
    std::puts("PASS: effect fold at requested level in stereo, mono and 5.1; music and speech unchanged");
}
