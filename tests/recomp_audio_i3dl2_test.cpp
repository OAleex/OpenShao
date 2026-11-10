#include <cmath>
#include <cstdio>
#include <cstdlib>

#if __has_include("audio_i3dl2.h")
#include "audio_i3dl2.h"
using namespace openshao::audio::i3dl2;
static void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Near(float a, float b) { return std::abs(a-b) < 0.00002f; }

int main() {
    Source source;
    source.direct = -600;
    source.room = -1200;
    source.room_rolloff = 1;
    auto levels = Calculate(source, 1, 1, 5000);
    Check(levels.direct_mb == -600 && levels.room_mb == -1200,
          "independent source direct and room attenuation must survive the mixer");
    Check(Near(Gain(levels.direct_mb), .5011872f) && Near(Gain(levels.room_mb), .2511886f),
          "Xbox hundredths of dB must convert to amplitude gains");
    source.obstruction = -2000; source.obstruction_lf = .25f;
    source.occlusion = -4000; source.occlusion_lf = .5f;
    levels = Calculate(source, 1, 1, 5000);
    Check(levels.direct_mb == -3100 && levels.room_mb == -3200,
          "obstruction must affect only direct sound while occlusion also affects the room send");
    Check(levels.direct_hf_mb == -3500 && levels.room_hf_mb == -3200,
          "HF attenuation must use the remaining LF-ratio share, matching the title's converter");
    source = {}; source.room = -1200; source.room_rolloff = 1;
    levels = Calculate(source, 10, 1, 5000);
    Check(levels.room_mb == 800 && Near(Gain(levels.room_mb), 2.5118864f),
          "room rolloff must compensate the common distance gain rather than attenuate the send twice");
    Check(Calculate(source, 10, 0, 5000).room_mb == -1200,
          "zero listener room rolloff must disable the relative room-distance compensation");
    Check(Gain(-10000) == 0, "the Xbox silence sentinel must remain silent");
    Check(Pole(0, 24000) == 0 && Near(Pole(-2000, 24000), .81817627f),
          "the one-pole HF response must use the native 16-bit coefficient quantization");
    float previous = 0;
    const float pole = Pole(-2000, 24000);
    const float first = Filter(1, pole, previous);
    const float tail = Filter(0, pole, previous);
    Check(Near(first, .18182373f) && Near(tail, .14876385f),
          "source HF filtering must retain a decaying state across samples");
    double total = first + tail;
    for (unsigned i = 0; i < 1000; ++i) total += Filter(0, pole, previous);
    Check(std::abs(total-1) < .00001, "HF filtering must preserve DC energy instead of reducing the whole voice volume");

    Deferred<9> parameters;
    Words<9> first_values{}; first_values[0] = 1;
    Words<9> second_values{}; second_values[0] = 2;
    parameters.Set(first_values, true);
    Check(!parameters.Configured(), "deferred source parameters must not change currently playing audio");
    Check(parameters.Commit() && parameters.Active()[0] == 1,
          "CommitDeferredSettings must atomically expose queued source parameters");
    parameters.Set(second_values, true);
    Check(parameters.Active()[0] == 1, "a later deferred change must leave the old parameters active");
    parameters.Set(first_values, false);
    Check(!parameters.Commit() && parameters.Active()[0] == 1,
          "an immediate update must replace an older queued update");
    Words<9> source_words{};
    source_words[4] = Bits(1);
    source_words[6] = Bits(.25f); source_words[8] = Bits(.5f);
    Check(ValidSource(source_words), "finite source settings in the Xbox ranges are valid");
    source_words[4] = 0x7FC00000;
    Check(!ValidSource(source_words), "NaN room rolloff must be rejected before native math or audio mixing");
    std::puts("PASS: I3DL2 direct/room levels, HF filtering, deferred updates and parameter validation");
}
#else
int main() { std::fputs("FAIL: I3DL2 source and listener handling has not been implemented\n", stderr); return 1; }
#endif
