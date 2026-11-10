#include "audio_crosstalk.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>

using namespace openshao::audio::signal;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Synthetic() {
    std::array<uint32_t, 93> state{};
    std::array<std::array<int32_t, 32>, 4> inputs{}, outputs{};
    std::array<std::span<const int32_t>,4> in;
    std::array<std::span<int32_t>,4> out;
    for (unsigned c=0;c<4;++c) { in[c]=inputs[c]; out[c]=outputs[c]; }
    state[4]=2;
    inputs[0][0]=838860;
    Check(CancelSpeakerCrosstalk(state,in,out), "a valid crosstalk block must process");
    Check(state[4]==3 && state[63]==0x7645A1 && state[64]==0xF88313,
          "the native effect must initialize the original feedback coefficients once");
    Check(outputs[0][0]==838860 && outputs[1][0]==0,
          "an impulse must retain the direct channel before the cross delay arrives");
    Check(outputs[1][10]==0 && outputs[1][11]<0,
          "cross cancellation must begin after the original eleven-sample delay");
    Check(state[37]==10 && state[62]==10, "each channel pair must retain its circular delay position");
    Check(outputs[2]==std::array<int32_t,32>{} && outputs[3]==std::array<int32_t,32>{},
          "front input must not alter the rear channel pair");
    state={}; state[4]=0;
    outputs={}; inputs[1][0]=-12345;
    outputs[0][0]=100;
    Check(CancelSpeakerCrosstalk(state,in,out) && outputs[0][0]==838960 && outputs[1][0]==-12345,
          "disabled crosstalk must add the dry input to the existing destination");
    Check(state[13]!=0, "bypass must keep the delay history current for later activation");
    state={}; outputs={}; inputs={};
    inputs[0][0]=500; inputs[1][0]=-300; outputs[0][0]=100;
    out[1]=out[0];
    Check(CancelSpeakerCrosstalk(state,in,out) && outputs[0][0]==-200,
          "aliased channel destinations must use the same original destination sample");
    out[1]=outputs[1];
    const auto before=state;
    state[37]=11;
    Check(!CancelSpeakerCrosstalk(state,in,out), "invalid ring positions must be rejected before processing");
    state=before;
    Check(!CancelSpeakerCrosstalk(std::span<uint32_t>(state).first(64),in,out), "truncated state must be rejected");
}

int main() {
    Synthetic();
    std::puts("PASS: native crosstalk delay, channel independence, bypass, coefficient initialization and state bounds");
}
