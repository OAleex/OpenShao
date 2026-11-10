#include "audio_speaker_mix.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void Check(bool ok,const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static bool Near(float a,float b) { return std::abs(a-b)<1e-6f; }
int main() {
    openshao::audio::SpeakerBlock speakers{};
    float stereo[64]{};
    speakers[0][0]=.25f; speakers[1][0]=-.125f;
    openshao::audio::DownmixStereo(speakers,stereo);
    Check(Near(stereo[0],.25f) && Near(stereo[1],-.125f),"front stereo must keep its original gain and channel separation");
    speakers={}; speakers[2][4]=.25f;
    openshao::audio::DownmixStereo(speakers,stereo);
    Check(Near(stereo[8],.176776695f) && Near(stereo[9],.176776695f),"centre must reach both stereo speakers");
    speakers={}; speakers[4][7]=.25f; speakers[5][7]=-.125f;
    openshao::audio::DownmixStereo(speakers,stereo);
    Check(Near(stereo[14],.176776695f) && Near(stereo[15],-.088388348f),"rear channels must reach the corresponding stereo side");
    speakers={}; speakers[3][0]=1;
    openshao::audio::DownmixStereo(speakers,stereo);
    Check(stereo[0]==0 && stereo[1]==0,"the dedicated LFE feed must not be doubled into full-range speakers");
    speakers={}; speakers[0][31]=.1f; speakers[2][31]=.2f; speakers[4][31]=.3f;
    openshao::audio::DownmixStereo(speakers,stereo);
    Check(Near(stereo[62],.45355339f) && Near(stereo[63],.14142136f),"speaker contributions must sum at the last block sample");
    Check(stereo[0]==0 && stereo[1]==0,"an empty block must not retain previous output");
    openshao::audio::SpeakerPeakLimiter limiter;
    stereo[0]=2; stereo[1]=-1;
    limiter.Process(stereo,1);
    Check(std::abs(stereo[0])<1 && Near(stereo[0],-2*stereo[1]),"peak limiting must preserve the stereo ratio");
    stereo[0]=.125f; stereo[1]=.25f;
    limiter.Process(stereo,1);
    Check(stereo[0]<.125f && Near(stereo[1],2*stereo[0]),"gain must recover smoothly after a peak");
    for (unsigned block=0;block<1000;++block) {
        for (float& sample:stereo) sample=.125f;
        limiter.Process(stereo,32);
    }
    Check(Near(stereo[0],.125f),"a short peak must not leave the following audio permanently attenuated");
    limiter.Reset(); stereo[0]=.25f; stereo[1]=-.125f;
    limiter.Process(stereo,1);
    Check(Near(stereo[0],.25f) && Near(stereo[1],-.125f),"ordinary stereo levels must remain unchanged");
    std::puts("PASS: stereo gain, centre/rear coverage, channel separation and block boundaries");
}
