#include "audio_effect_chain.h"
#include "audio_crosstalk.h"
#include "audio_fixed_point.h"
#include "audio_reverb.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace openshao::audio {
using namespace signal;
namespace {
uint32_t ReadImageWord(std::span<const uint8_t> bytes,size_t offset) {
    uint32_t value; std::memcpy(&value,bytes.data()+offset,4); return value;
}
struct EffectImageCipher {
    uint32_t low=0x49DE12BA,high=0x7FA49BCA;
    void Advance() {
        uint32_t next=(low>>1)|(high<<31);
        high=((((((low<<2)^low)<<1)^low)<<28)^high)&0x80000000u | (high>>1); low=next;
    }
    std::array<uint8_t,8> Seed() {
        for (unsigned i=0;i<81;++i) Advance();
        std::array<uint8_t,8> seed; std::memcpy(seed.data(),&low,4); std::memcpy(seed.data()+4,&high,4); return seed;
    }
};
void DecodeEffectBytes(std::span<uint8_t> bytes,const std::array<uint8_t,8>& key) {
    EffectImageCipher cipher; const auto seed=cipher.Seed(); cipher.Advance();
    for (size_t i=0;i<bytes.size();++i) {
        cipher.Advance(); const uint8_t factor=seed[i&7];
        bytes[i]=factor^uint8_t(cipher.low)^uint8_t(bytes[i]-key[i&7]*factor);
    }
}
enum class EffectAlgorithm { Splitter, Capture, Reverb, Crosstalk };
struct EffectNode {
    EffectAlgorithm kind;
    std::vector<uint8_t> code;
    std::vector<uint32_t> parameters;
    std::vector<uint8_t> scratch;
    RoomReverb reverb;
};
bool IdentifyEffectAlgorithm(std::span<const uint8_t> code,EffectAlgorithm& kind) {
    uint64_t hash=14695981039346656037ull;
    for (size_t i=0;i<code.size();++i) hash=(hash^((i>=4&&i<8)?0:code[i]))*1099511628211ull;
    switch (hash) {
    case 0xDDD7C789A714E88Bull: kind=EffectAlgorithm::Splitter; return true;
    case 0x205DF40F9FE0159Bull: kind=EffectAlgorithm::Capture; return true;
    case 0x13CF05D0E014E22Eull: kind=EffectAlgorithm::Reverb; return true;
    case 0xF1E0A321578CFED9ull: kind=EffectAlgorithm::Crosstalk; return true;
    default:return false;
    }
}
}
struct EffectChain::Impl {
    std::vector<EffectNode> filters;
    std::vector<EffectDescription> descriptions;
    // Image connection IDs are mix bins (32 samples) or shared buffers, never CPU addresses.
    std::array<int32_t,1024> bins{};
    std::array<int32_t,2048> intermediate{};
    struct Update { uint32_t effect,offset; std::vector<uint8_t> bytes; };
    std::vector<Update> pending;
    bool ready=false;
    std::string error;
    EffectStatistics statistics;
    std::span<int32_t> Connection(uint32_t link) {
        if (link>=0x1400 && link<=0x17E0) return {bins.data()+link-0x1400,32};
        if (link>=0xC00 && link<=0xFE0) return {bins.data()+link-0xC00,32};
        if (link>=0x400 && link<=0xBE0) return {intermediate.data()+link-0x400,32};
        return {};
    }
    bool ProcessFilters() {
        for (unsigned index=0;index<filters.size();++index) {
            auto& filter=filters[index]; auto& state=filter.parameters;
            const uint32_t base=descriptions[index].state.address/4;
            if (filter.kind==EffectAlgorithm::Splitter) {
                if (state[14]>8) return false;
                auto input=Connection(state[5]); if (input.empty()) return false;
                state[4]|=1; state[24]=base+15;
                for (unsigned channel=0;channel<state[14];++channel) {
                    auto output=Connection(state[6+channel]); if (output.empty()) return false;
                    const int64_t gain=DecodeSample(state[15+channel]);
                    for (unsigned i=0;i<32;++i) output[i]=TruncateProduct(2*gain*input[i]+((state[4]&2)?int64_t(output[i])*(int64_t(1)<<24):0));
                }
            } else if (filter.kind==EffectAlgorithm::Capture) {
                auto input=Connection(state[5]),output=Connection(state[6]);
                if (input.empty() || output.empty() || filter.scratch.empty()) return false;
                if (!(state[4]&1)) {
                    state[4]|=1; state[16]=0x010000; // short fractional immediate in the image
                    state[9]=0x4000; state[10]=0x4BD2; state[11]=32; state[12]=state[6]; state[14]=state[0]; state[15]=state[1]-1;
                }
                uint32_t phase=state[16],count=0;
                const int64_t gain=DecodeSample(state[8]);
                for (unsigned i=0;i<32;++i) {
                    if (phase>=state[7]) {
                        const int64_t product=2*gain*input[i];
                        output[count++]=TruncateProduct(product+8*int64_t(TruncateProduct(product))*(int64_t(1)<<24)); phase=0;
                    }
                    ++phase;
                }
                state[16]=phase; state[11]=count;
                for (unsigned i=0;i<count;++i) {
                    const uint32_t sample=uint32_t(output[i])&0xFFFFFF;
                    for (unsigned b=0;b<4;++b) filter.scratch[(state[13]+b)%filter.scratch.size()]=uint8_t(sample>>(8*b));
                    state[13]=(state[13]+4)%uint32_t(filter.scratch.size());
                }
            } else if (filter.kind==EffectAlgorithm::Crosstalk) {
                std::array<std::span<const int32_t>,4> inputs;
                std::array<std::span<int32_t>,4> outputs;
                for (unsigned c=0;c<4;++c) { inputs[c]=Connection(state[5+c]); outputs[c]=Connection(state[9+c]); }
                if (!CancelSpeakerCrosstalk(state,inputs,outputs)) return false;
            } else {
                auto input=Connection(state[5]); if (input.empty()) return false;
                std::array<std::array<int32_t,32>,4> wet;
                if (!filter.reverb.Process(state,base,std::span<int32_t,32>(input.data(),32),wet)) return false;
                for (unsigned c=0;c<4;++c) {
                    auto output=Connection(state[7+c]); if (output.empty()) return false;
                    for (unsigned i=0;i<32;++i) output[i]=ClampSample(int64_t(output[i])+wet[c][i]);
                }
            }
        }
        return true;
    }
    void Apply(uint32_t effect,uint32_t offset,std::span<const uint8_t> bytes) {
        auto& state=filters[effect].parameters;
        for (unsigned i=0;i<bytes.size();++i) {
            const unsigned at=offset+i,shift=(at%4)*8;
            state[at/4]=((state[at/4]&~(255u<<shift)) | (uint32_t(bytes[i])<<shift))&0xFFFFFF;
        }
    }
};
EffectChain::EffectChain():impl_(std::make_unique<Impl>()) {}
EffectChain::~EffectChain()=default;

bool EffectChain::LoadImage(std::span<const uint8_t> image) {
    auto reject=[&](const char* error) { impl_->error=error; return false; };
    if (image.size()<0x818 || (image.size()&3)) return reject("Invalid effect image length");
    const uint32_t codeWords=ReadImageWord(image,0x804),stateWords=ReadImageWord(image,0x80C);
    if (ReadImageWord(image,0x800)!=0 || codeWords>0x1000 || stateWords>0xF80) return reject("Invalid effect image layout");
    const size_t stateStart=0x818+size_t(codeWords)*4, descriptor=stateStart+size_t(stateWords)*4;
    if (ReadImageWord(image,0x808)!=stateStart || descriptor>image.size() || image.size()-descriptor<8) return reject("Invalid effect table offset");
    const uint32_t count=ReadImageWord(image,descriptor),scratchBytes=ReadImageWord(image,descriptor+4);
    const size_t keyStart=descriptor+8+size_t(count)*32;
    if (!count || count>64 || scratchBytes>32*1024*1024 || keyStart>image.size() || image.size()-keyStart<size_t(count)*8) return reject("Invalid effect table size");
    auto next=std::make_unique<Impl>();
    std::vector<uint8_t> decoded(image.begin(),image.end()); std::span<uint8_t> bytes(decoded);
    EffectImageCipher seed; DecodeEffectBytes(bytes.subspan(keyStart,count*8),seed.Seed());
    uint32_t cursor=0x818;
    for (unsigned i=0;i<count;++i) {
        const size_t at=descriptor+8+i*32;
        const uint32_t code=ReadImageWord(image,at),codeSize=ReadImageWord(image,at+4),state=ReadImageWord(image,at+8),stateSize=ReadImageWord(image,at+12),
                       y=ReadImageWord(image,at+16),ySize=ReadImageWord(image,at+20),scratch=ReadImageWord(image,at+24),scratchSize=ReadImageWord(image,at+28);
        if ((code|codeSize|state|stateSize|y|ySize)&3 || code!=cursor || code>stateStart || codeSize>stateStart-code ||
            state<stateStart || state>descriptor || stateSize>descriptor-state || y>0x2000 || ySize>0x2000-y ||
            scratch<0xC000 || scratch>0xC000+scratchBytes || scratchSize>0xC000+scratchBytes-scratch) return reject("Effect segment is outside its allocation");
        std::array<uint8_t,8> key; std::copy_n(bytes.begin()+keyStart+i*8,8,key.begin());
        DecodeEffectBytes(bytes.subspan(code,codeSize),key);
        EffectNode filter;
        if (codeSize<8 || !IdentifyEffectAlgorithm(bytes.subspan(code,codeSize),filter.kind)) return reject("Unsupported effect algorithm in game image");
        const size_t required=filter.kind==EffectAlgorithm::Splitter?46:filter.kind==EffectAlgorithm::Capture?39:filter.kind==EffectAlgorithm::Crosstalk?93:554;
        if (stateSize/4!=required || ReadImageWord(bytes,code+4)!=0x80+(state-stateStart)/4) return reject("Effect parameter layout does not match its algorithm");
        filter.code.assign(bytes.begin()+code,bytes.begin()+code+codeSize);
        filter.parameters.resize(stateSize/4); filter.scratch.resize(scratchSize);
        for (unsigned k=0;k<stateSize/4;++k) filter.parameters[k]=ReadImageWord(image,state+4*k)&0xFFFFFF;
        next->filters.push_back(std::move(filter));
        next->descriptions.push_back({{0x5CC+code-0x818,codeSize},{0x200+state-uint32_t(stateStart),stateSize},{y,ySize},{scratch,scratchSize}});
        cursor+=codeSize;
    }
    if (cursor!=stateStart && (cursor+4!=stateStart || ReadImageWord(bytes,cursor)!=12)) return reject("Effect segments do not cover their image");
    if (!next->ProcessFilters()) return reject("Invalid channel connection or delay configuration");
    next->ready=true; impl_=std::move(next); return true;
}
bool EffectChain::ProcessBlock(const float buses[32][32],float stereo[64]) {
    SpeakerBlock speakers{};
    const bool processed=ProcessSpeakers(buses,speakers);
    DownmixStereo(speakers,stereo);
    return processed;
}
bool EffectChain::ProcessSpeakers(const float buses[32][32],SpeakerBlock& speakers) {
    speakers={}; if (!impl_->ready) return false;
    for (unsigned b=0;b<32;++b) for (unsigned i=0;i<32;++i) impl_->bins[b*32+i]=QuantizeSample(buses[b][i]);
    if (!impl_->ProcessFilters()) { impl_->ready=false; impl_->error="Effect block has invalid channel or delay parameters"; ++impl_->statistics.failures; return false; }
    for (unsigned c=0;c<6;++c) for (unsigned i=0;i<32;++i) speakers[c][i]=NormalizeSample(impl_->bins[c*32+i]);
    ++impl_->statistics.blocks; return true;
}
bool EffectChain::SetParameters(uint32_t effect,uint32_t offset,std::span<const uint8_t> bytes,bool deferred) {
    if (!impl_->ready || effect>=impl_->filters.size()) return false;
    const size_t size=impl_->filters[effect].parameters.size()*4;
    if (offset>size || bytes.size()>size-offset) return false;
    if (deferred) impl_->pending.push_back({effect,offset,{bytes.begin(),bytes.end()}}); else impl_->Apply(effect,offset,bytes);
    return true;
}
bool EffectChain::ReadParameters(uint32_t effect,uint32_t offset,std::span<uint8_t> bytes) const { return ReadImageSegment(effect,1,offset,bytes); }
bool EffectChain::ReadImageSegment(uint32_t effect,uint32_t segment,uint32_t offset,std::span<uint8_t> bytes) const {
    if (!impl_->ready || effect>=impl_->filters.size() || segment>3) return false;
    const auto& filter=impl_->filters[effect]; const auto& description=impl_->descriptions[effect];
    const size_t size=segment==0?description.code.size:segment==1?description.state.size:segment==2?description.y.size:description.scratch.size;
    if (offset>size || bytes.size()>size-offset) return false;
    if (segment==0) std::copy_n(filter.code.begin()+offset,bytes.size(),bytes.begin());
    else if (segment==1) for (unsigned i=0;i<bytes.size();++i) bytes[i]=uint8_t(filter.parameters[(offset+i)/4]>>((offset+i)%4*8));
    else if (segment==3) std::copy_n(filter.scratch.begin()+offset,bytes.size(),bytes.begin());
    else std::fill(bytes.begin(),bytes.end(),0);
    return true;
}
void EffectChain::CommitParameters() { for (const auto& update:impl_->pending) impl_->Apply(update.effect,update.offset,update.bytes); impl_->pending.clear(); }
bool EffectChain::IsReady() const { return impl_->ready; }
const std::string& EffectChain::LastError() const { return impl_->error; }
const std::vector<EffectDescription>& EffectChain::Descriptions() const { return impl_->descriptions; }
EffectStatistics EffectChain::Statistics() const { return impl_->statistics; }
}
