#include <cstdio>
#include <cstdlib>
#if __has_include("audio_output.h")
#include "audio_output.h"
#include "audio_speaker_mix.h"
#include "audio_spatial.h"
#include <array>
#include <cmath>
using namespace openshao::audio;
static void Check(bool ok,const char* message) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
int main() {
    output::SetDeviceLayout(6,8,0x63F);
    output::SetMode(output::Mode::Surround);
    auto state=output::Current();
    Check(state.channels==6 && state.device_channels==8 && state.EffectiveMode()==output::Mode::Surround,"a multichannel device must retain a six-channel carrier");
    SpeakerBlock speakers{};
    for (unsigned c=0;c<6;++c) speakers[c][c]=.125f*float(c+1);
    std::array<float,194> pcm; pcm.fill(99);
    FormatSpeakers(speakers,pcm.data(),state.channels,state.EffectiveMode());
    for (unsigned i=0;i<32;++i) for (unsigned c=0;c<6;++c)
        Check(pcm[6*i+c]==speakers[c][i],"surround impulses must retain their speaker and amplitude, including LFE");
    Check(pcm[192]==99 && pcm[193]==99,"formatting must respect the destination length");
    output::SetMode(output::Mode::Stereo); state=output::Current();
    FormatSpeakers(speakers,pcm.data(),state.channels,state.EffectiveMode());
    float stereo[64]; DownmixStereo(speakers,stereo);
    for (unsigned i=0;i<32;++i) {
        Check(pcm[6*i]==stereo[2*i] && pcm[6*i+1]==stereo[2*i+1],"stereo must keep the approved downmix even on a surround device");
        for (unsigned c=2;c<6;++c) Check(pcm[6*i+c]==0,"stereo must silence unused carrier speakers");
    }
    output::SetMode(output::Mode::Mono); state=output::Current();
    FormatSpeakers(speakers,pcm.data(),state.channels,state.EffectiveMode());
    for (unsigned i=0;i<32;++i) Check(pcm[6*i]==pcm[6*i+1] && std::abs(pcm[6*i]-(stereo[2*i]+stereo[2*i+1])*.5f)<1e-6f,"mono must combine both sides without doubling their gain");
    output::SetDeviceLayout(2,2); output::SetMode(output::Mode::Surround); state=output::Current();
    Check(state.EffectiveMode()==output::Mode::Stereo,"surround requested on a stereo device must fall back to stereo");
    FormatSpeakers(speakers,pcm.data(),state.channels,state.EffectiveMode());
    for (unsigned i=0;i<64;++i) Check(pcm[i]==stereo[i],"stereo fallback must use the existing mix rather than a device-specific downmix");
    auto front=SurroundPan(0,1), back=SurroundPan(0,-1), right=SurroundPan(1,0);
    Check(front[2]==0 && front[3]==0 && front[0]>0 && front[1]>0,"a front source must not be copied to the rear");
    Check(back[0]==0 && back[1]==0 && back[2]>0 && back[3]>0,"a rear source must reach the rear pair");
    Check(right[0]==0 && right[2]==0 && right[1]>0 && right[3]>0,"a side source must pan between the right speakers");
    const auto centre=SurroundPan(0,1,true),rear=SurroundPan(0,-1,true);
    Check(centre[4]==1 && centre[0]==0 && centre[1]==0 && rear[4]==0,"a centre-enabled positional voice must use centre only in front of the listener");
    for (unsigned i=0;i<360;++i) {
        const float a=float(i)*.01745329252f; const auto gains=SurroundPan(std::sin(a),std::cos(a)); float energy=0;
        for (float gain:gains) { Check(gain>=0 && std::isfinite(gain),"all panning gains must be finite and non-negative"); energy+=gain*gain; }
        Check(std::abs(energy-1)<1e-5f,"panning must preserve energy around the listener");
    }
    SpeakerPeakLimiter limiter; float peak[6]={2,1,.5f,-1,-.5f,.25f}; limiter.Process(peak,1,6);
    Check(peak[0]<1 && std::abs(peak[0]-2*peak[1])<1e-6f && std::abs(peak[4]+2*peak[5])<1e-6f,"overload protection must apply one linked gain to every speaker");
    std::puts("PASS: speaker routing, mono/stereo/surround, fallback, constant-power positioning and linked limiting");
}
#else
int main() { std::fputs("FAIL: multichannel output is unavailable\n",stderr); return 1; }
#endif
