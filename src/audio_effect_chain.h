#pragma once
#include "audio_speaker_mix.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace openshao::audio {
struct EffectSegment { uint32_t address=0,size=0; };
struct EffectDescription { EffectSegment code,state,y,scratch; };
struct EffectStatistics { uint64_t blocks=0,failures=0; };

// Decodes the game's effect assets into C++ filters, delay lines and channel routes.
class EffectChain {
public:
    EffectChain();
    ~EffectChain();
    bool LoadImage(std::span<const uint8_t> image);
    bool ProcessBlock(const float buses[32][32],float stereo[64]);
    bool ProcessSpeakers(const float buses[32][32],SpeakerBlock& speakers);
    bool SetParameters(uint32_t effect,uint32_t offset,std::span<const uint8_t> bytes,bool deferred);
    bool ReadParameters(uint32_t effect,uint32_t offset,std::span<uint8_t> bytes) const;
    bool ReadImageSegment(uint32_t effect,uint32_t segment,uint32_t offset,std::span<uint8_t> bytes) const;
    void CommitParameters();
    bool IsReady() const;
    const std::string& LastError() const;
    const std::vector<EffectDescription>& Descriptions() const;
    EffectStatistics Statistics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
