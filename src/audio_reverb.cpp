#include "audio_reverb.h"
#include "audio_fixed_point.h"
#include <algorithm>

namespace openshao::audio {
using namespace signal;
namespace {
constexpr int64_t Fraction = int64_t(1)<<24;
constexpr unsigned WriteTap=189, ReadTap=287;
int32_t ParameterSample(std::span<uint32_t> state,unsigned index) { return DecodeSample(state[index]); }
void WriteParameterSample(std::span<uint32_t> state,unsigned index,int32_t sample) { state[index]=uint32_t(sample)&0xFFFFFF; }
int32_t ScaleSample(int32_t a,int32_t b) { return TruncateProduct(2*int64_t(a)*b); }
int32_t AddSamples(int32_t a,int32_t b) { return ClampSample(int64_t(a)+b); }
int64_t RoundAccumulator(int64_t value) {
    const int64_t whole=value>>24;
    const uint32_t fraction=uint32_t(value)&0xFFFFFF;
    const bool up=fraction>0x800000 || (fraction==0x800000 && (whole&1));
    return WrapAccumulator((whole+up)*Fraction);
}
void ApplyDampingFilter(std::span<int32_t> samples,std::span<uint32_t> state,unsigned coefficients) {
    const int64_t feedback=ParameterSample(state,coefficients+1), feed=ParameterSample(state,coefficients+2);
    int32_t history=ParameterSample(state,coefficients);
    for (auto& sample:samples) sample=history=TruncateProduct(2*(feedback*history+feed*sample));
    WriteParameterSample(state,coefficients,history);
}
void SetTapDelay(std::span<uint32_t> state,unsigned writer,unsigned reader,uint32_t delay) {
    const int64_t size=int64_t(state[writer+6])+1;
    int64_t position=(int64_t(state[writer+4])-delay)&~int64_t(15);
    position%=size;
    if (position<0) position+=size;
    state[reader+4]=uint32_t(position);
}
void SetEarlyDelays(std::span<uint32_t> state) {
    for (unsigned line=0;line<4;++line) for (unsigned tap=0;tap<2;++tap)
        SetTapDelay(state,WriteTap+7*line,ReadTap+7*(13+2*line+tap),state[76+2*line+tap]);
    for (unsigned line=0;line<4;++line)
        SetTapDelay(state,WriteTap+7*(8+line),ReadTap+7*(25+line),state[155+line]);
}
}

bool RoomReverb::Initialize(std::span<uint32_t> state,uint32_t base) {
    uint32_t start=state[0];
    for (unsigned i=0;i<14;++i) {
        const uint32_t bytes=state[43+2*i]+1;
        if (!bytes || (bytes&3) || bytes>2*1024*1024) return false;
        delays_[i].assign(bytes/4,0);
        state[42+2*i]=start; start+=bytes;
    }
    for (unsigned i=0;i<31;++i) state[11+i]=state[11]+16*i;
    constexpr uint32_t interpolation[] = {0x424778,0x4B40E6,0x424778,0x23EB20,0xDCBAB6,
                                         0x424778,0x045B75,0x424778,0x117DE1,0x8F1750};
    std::copy(std::begin(interpolation),std::end(interpolation),state.begin()+179);
    for (unsigned i=0;i<45;++i) {
        const bool write=i<14;
        const unsigned tap=write ? WriteTap+7*i : ReadTap+7*(i-14);
        state[tap]=((i==13 || i==44)?0x4000:base+tap+7);
        state[tap+1]=write?0x4BD2:0x4BD0;
        state[tap+2]=16; state[tap+3]=state[11]+16*(write?0:i-14); state[tap+4]=0;
    }
    constexpr unsigned sources[]={0,14,16,18,1,2,3,4,5,6,7,8,9,10};
    for (unsigned i=0;i<14;++i) {
        state[WriteTap+7*i+3]=state[11]+16*sources[i];
        state[WriteTap+7*i+5]=state[42+2*i]; state[WriteTap+7*i+6]=state[43+2*i];
    }
    for (unsigned i=0;i<31;++i) {
        const unsigned line=i<13?0:i<21?(i-13)/2:i-17;
        state[ReadTap+7*i+5]=state[42+2*line]; state[ReadTap+7*i+6]=state[43+2*line];
    }
    for (unsigned i=0;i<5;++i) SetTapDelay(state,WriteTap,ReadTap+7*i,state[70+i]);
    for (unsigned i=0;i<8;++i) SetTapDelay(state,WriteTap,ReadTap+7*(5+i),state[147+i]+state[75]);
    for (unsigned i=0;i<4;++i) SetTapDelay(state,WriteTap+7*(4+i),ReadTap+7*(21+i),state[84+i]);
    for (unsigned i=0;i<2;++i) SetTapDelay(state,WriteTap+7*(12+i),ReadTap+7*(29+i),state[88]);
    SetEarlyDelays(state);
    std::copy_n(state.begin()+99,6,state.begin()+504);
    state[510]=state[511]=0; state[526]=0xFFFFFF; state[4]|=1;
    return true;
}

void RoomReverb::ReadDelayTaps(std::span<uint32_t> state) {
    for (unsigned tap=0;tap<31;++tap) {
        const unsigned line=tap<13?0:tap<21?(tap-13)/2:tap-17;
        auto& delay=delays_[line];
        uint32_t& position=state[ReadTap+7*tap+4];
        for (auto& sample:blocks_[tap]) {
            sample=delay[(position/4)%delay.size()];
            position=(position+4)%uint32_t(delay.size()*4);
        }
    }
}
void RoomReverb::WriteDelayLines(std::span<uint32_t> state) {
    constexpr unsigned sources[]={0,14,16,18,1,2,3,4,5,6,7,8,9,10};
    for (unsigned line=0;line<14;++line) {
        auto& delay=delays_[line];
        uint32_t& position=state[WriteTap+7*line+4];
        for (auto sample:blocks_[sources[line]]) {
            delay[(position/4)%delay.size()]=sample;
            position=(position+4)%uint32_t(delay.size()*4);
        }
    }
}

void RoomReverb::UpdateParameters(std::span<uint32_t> state,uint32_t base) {
    if (ParameterSample(state,526)>=0) {
        if (state[526]==336) { std::copy_n(state.begin()+99,6,state.begin()+504); state[526]=0xFFFFFF; }
        else {
            ++state[526];
            for (unsigned i=0;i<6;++i) {
                const int32_t target=ParameterSample(state,99+i), difference=ClampSample(int64_t(ParameterSample(state,504+i))-target);
                WriteParameterSample(state,504+i,TruncateProduct(int64_t(target)*Fraction+2*int64_t(0x7C49BA)*difference));
            }
        }
    }
    if (!state[510]) return;
    if (state[511]==256) { state[511]=0; state[510]=(state[510]<<1)&0xFFFFFF; return; }
    if (!state[511]) {
        const uint32_t step=state[510];
        if (step&1) { state[517]=base+70; state[515]=base+WriteTap; state[514]=base+ReadTap; state[516]=0; }
        else if (step&32) {
            state[515]=base+WriteTap; state[514]=base+ReadTap+7*5; state[517]=base+518; state[516]=0;
            for (unsigned i=0;i<8;++i) state[518+i]=(state[147+i]+state[75])&0xFFFFFF;
        } else if (step&(1<<13)) { state[517]=base+84; state[515]=base+WriteTap+7*4; state[514]=base+ReadTap+7*21; state[516]=7; }
        else if (step&(1<<17)) { state[517]=base+518; state[515]=base+WriteTap+7*12; state[514]=base+ReadTap+7*29; state[516]=7; state[518]=state[519]=state[88]; }
        else if (step&(1<<19)) { state[510]=0; state[526]=0xFFFFFF; return; }
        else { state[514]+=7; state[515]+=state[516]; }
        state[512]=0xFF0000; state[513]=0x7F0000;
    }
    if (state[511]==128) {
        const auto desired=state[state[517]++-base];
        SetTapDelay(state,state[515]-base,state[514]-base,desired>64?desired-64:0);
        state[512]=0x010000;
    }
    const unsigned tap=(state[514]-base-ReadTap)/7;
    if (tap<blocks_.size()) {
        int32_t gain=ParameterSample(state,513), increment=ParameterSample(state,512);
        for (auto& sample:blocks_[tap]) { sample=ScaleSample(sample,gain); gain+=increment; }
        WriteParameterSample(state,513,gain);
    }
    state[511]+=16;
}

bool RoomReverb::Process(std::span<uint32_t> state,uint32_t base,std::span<int32_t,32> input,
                         std::array<std::array<int32_t,32>,4>& output) {
    if (state.size()<554) return false;
    if (!(state[4]&1) && !Initialize(state,base)) return false;
    if (delays_[0].empty()) return false;
    if (state[4]&4) {
        state[4]&=~4u; state[510]=1; state[511]=state[526]=0;
        SetEarlyDelays(state);
    }
    ReadDelayTaps(state);
    // The original anti-alias stage is a half-gain one-pole filter before decimation.
    for (auto& sample:input) sample=ScaleSample(sample,0x400000);
    ApplyDampingFilter(input,state,106);
    for (unsigned i=0;i<16;++i) input[i]=input[2*i];
    UpdateParameters(state,base);
    auto add=[&](unsigned source,unsigned destination) {
        for (unsigned i=0;i<16;++i) blocks_[destination][i]=AddSamples(blocks_[destination][i],blocks_[source][i]);
    };
    for (unsigned b=1;b<=4;++b) add(0,b);
    for (unsigned b=0;b<4;++b) for (unsigned i=0;i<16;++i)
        blocks_[b+1][i]=RoundProduct(int64_t(blocks_[b+1][i])*Fraction+2*int64_t(blocks_[21+b][i])*ParameterSample(state,143+b));
    for (unsigned b=0;b<8;++b) for (unsigned i=0;i<16;++i)
        blocks_[25+b/2][i]=RoundProduct(int64_t(blocks_[25+b/2][i])*Fraction+2*int64_t(blocks_[5+b][i])*ParameterSample(state,89+b));
    for (unsigned i=0;i<16;++i) {
        blocks_[9][i]=AddSamples(blocks_[14][i],blocks_[15][i]);
        blocks_[10][i]=AddSamples(blocks_[13][i],blocks_[16][i]);
    }
    add(17,9); add(18,10); add(20,9); add(19,10);
    for (unsigned i=0;i<16;++i) {
        blocks_[11][i]=ScaleSample(AddSamples(blocks_[9][i],blocks_[10][i]),ParameterSample(state,98));
        blocks_[9][i]=ScaleSample(blocks_[9][i],ParameterSample(state,97));
        blocks_[10][i]=ScaleSample(blocks_[10][i],ParameterSample(state,97));
    }
    add(29,9); add(30,10); blocks_[29]=blocks_[9]; blocks_[30]=blocks_[10];
    ApplyDampingFilter(blocks_[9],state,130); ApplyDampingFilter(blocks_[10],state,133);
    for (unsigned b=0;b<3;++b) ApplyDampingFilter(blocks_[14+2*b],state,109+3*b);
    for (unsigned b=0;b<4;++b) for (unsigned i=0;i<16;++i) {
        int64_t sum=0;
        for (unsigned c=0;c<4;++c) sum=RoundAccumulator(sum+2*int64_t(blocks_[25+c][i])*ParameterSample(state,139-b+c));
        blocks_[5+b][i]=TruncateProduct(sum);
    }
    for (unsigned b=0;b<4;++b) { add(11,5+b); ApplyDampingFilter(blocks_[5+b],state,118+3*b); }
    std::copy_n(input.begin(),16,blocks_[0].begin());
    const bool four=state[105]==4;
    for (unsigned pair=0;pair<2;++pair) for (unsigned i=0;i<16;++i) {
        const int64_t left=2*int64_t(blocks_[1+2*pair][i])*ParameterSample(state,504+2*pair);
        const int64_t right=2*int64_t(blocks_[2+2*pair][i])*ParameterSample(state,505+2*pair);
        const int64_t wetLeft=2*int64_t(blocks_[25+2*pair][i])*ParameterSample(state,508);
        const int64_t wetRight=2*int64_t(blocks_[26+2*pair][i])*ParameterSample(state,508);
        const int64_t tail=2*int64_t(blocks_[29+pair][i])*(four?(ParameterSample(state,509)>>1):ParameterSample(state,509));
        auto rounded=[](int64_t value) { return RoundAccumulator(value); };
        if (four) {
            const int64_t a=rounded(rounded(left+wetLeft)+tail), b=rounded(rounded(right+wetRight)+tail);
            blocks_[21+2*pair][i]=TruncateProduct(ShiftAccumulator(a,7));
            blocks_[22+2*pair][i]=TruncateProduct(ShiftAccumulator(b,7));
        } else {
            int64_t a=rounded(right+left); a=rounded(a+wetRight); a=rounded(a+wetLeft); a=rounded(a+tail);
            blocks_[21+pair][i]=TruncateProduct(ShiftAccumulator(a,7));
        }
    }
    output={};
    for (unsigned c=0;c<(four?4u:2u);++c) for (unsigned i=0;i<32;++i) output[c][i]=blocks_[21+c][i/2];
    WriteDelayLines(state);
    return true;
}
}
