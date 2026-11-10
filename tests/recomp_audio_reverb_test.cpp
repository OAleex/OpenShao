#include "audio_reverb.h"
#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    openshao::audio::RoomReverb reverb;
    std::array<uint32_t, 554> state{};
    std::array<int32_t,32> input{};
    std::array<std::array<int32_t,32>,4> output{};
    Check(!reverb.Process(state, 0x103, input, output), "unallocated delay lines must be rejected");
    state[0]=0xC000; state[11]=0x466; state[105]=4;
    for (unsigned i=0;i<14;++i) state[43+i*2]=2047;
    for (unsigned i=70;i<89;++i) state[i]=128;
    Check(reverb.Process(state,0x103,input,output), "a silent valid room must process");
    for (const auto& channel:output) for (int32_t sample:channel)
        Check(sample==0, "silence must remain silent");
    Check(state[4]&1, "the room must retain its initialized state");
    state[4]|=4;
    Check(reverb.Process(state,0x103,input,output) && !(state[4]&4), "parameter changes must start a smooth transition");
    Check(state[510]==1 && state[511]==16 && state[526]==1, "tap and damping transitions must advance independently");
    openshao::audio::RoomReverb impulse_room;
    state={}; state[0]=0xC000; state[11]=0x466; state[105]=4;
    for (unsigned i=0;i<14;++i) state[43+i*2]=2047;
    for (unsigned i=70;i<89;++i) state[i]=128;
    for (unsigned i=99;i<103;++i) state[i]=0x10000;
    state[108]=0x7FFFFF;
    input[0]=1<<20;
    Check(impulse_room.Process(state,0x103,input,output),"a room impulse must process");
    for (const auto& channel:output) for (auto sample:channel) Check(sample==0,"reflections must respect the room delay");
    input={};
    Check(impulse_room.Process(state,0x103,input,output),"the delay must advance through silence");
    Check(output[0][0]==0,"a 128-byte delay at 24 kHz must not arrive after only one block");
    Check(impulse_room.Process(state,0x103,input,output),"the reflected impulse must process");
    Check(output[0][0]>1000000 && output[0][0]<1100000,"early reflection gain must retain its original scale");
    Check(output[0][0]==output[1][0] && output[0][0]==output[2][0] && output[0][0]==output[3][0],
          "equal room parameters must produce equal reflections in all four channels");
    std::puts("PASS: room delay allocation, silence, initialization and parameter transitions");
}
