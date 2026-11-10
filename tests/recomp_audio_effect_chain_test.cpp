#include "audio_effect_chain.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <cmath>
static void Check(bool ok,const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static std::vector<uint8_t> Read(const char* path) {
    std::ifstream stream(path,std::ios::binary); return {std::istreambuf_iterator<char>(stream),{}};
}
static void Fingerprint(uint64_t& value,std::span<const uint8_t> bytes) {
    for (auto byte:bytes) value=(value^byte)*1099511628211ull;
}
static void CheckSpeakerCoverage(const std::vector<uint8_t>& image) {
    for (unsigned channel:{2u,4u,5u}) {
        openshao::audio::EffectChain chain;
        Check(chain.LoadImage(image),"load the effect chain for speaker coverage");
        float buses[32][32]{},output[64]{};
        buses[channel][0]=.125f;
        Check(chain.ProcessBlock(buses,output),"process an isolated speaker channel");
        Check(std::abs(output[0])+std::abs(output[1])>.05f,
              "centre and rear speaker channels must remain audible on stereo output");
    }
    openshao::audio::EffectChain chain;
    Check(chain.LoadImage(image),"load the scene-bed route");
    float buses[32][32]{},output[64]{};
    // Stereo beds request slots [6,8,7,9,2,16]. Their right source feeds 8/9.
    buses[8][0]=buses[9][0]=.125f;
    Check(chain.ProcessBlock(buses,output),"process the scene-bed right channel");
    Check(std::abs(output[0])+std::abs(output[1])>.05f,
          "the scene-bed right source must not disappear after crosstalk processing");
}
int main(int argc,char** argv) {
    openshao::audio::EffectChain chain;
    Check(!chain.LoadImage({}) && !chain.IsReady() && !chain.LastError().empty(),"empty effect images must fail explicitly");
    std::array<uint8_t,0x818> corrupt{};
    Check(!chain.LoadImage(corrupt),"invalid effect tables must be rejected");
    if (argc>=2) {
        const auto image=Read(argv[1]);
        CheckSpeakerCoverage(image);
        const bool loaded=chain.LoadImage(image); Check(loaded,chain.LastError().c_str());
        uint32_t flags=0,observed=0;
        Check(chain.ReadParameters(0,16,{reinterpret_cast<uint8_t*>(&flags),4}),"read initial effect flags");
        const uint32_t changed=flags^2;
        Check(chain.SetParameters(0,16,{reinterpret_cast<const uint8_t*>(&changed),4},true),"queue a deferred effect update");
        Check(chain.ReadParameters(0,16,{reinterpret_cast<uint8_t*>(&observed),4}) && observed==flags,
              "deferred parameters must remain inactive until commit");
        chain.CommitParameters();
        Check(chain.ReadParameters(0,16,{reinterpret_cast<uint8_t*>(&observed),4}) && observed==changed,
              "commit must activate deferred parameters");
        Check(chain.SetParameters(0,16,{reinterpret_cast<const uint8_t*>(&flags),4},false),"restore effect flags");
        Check(!chain.SetParameters(0,chain.Descriptions()[0].state.size,{reinterpret_cast<const uint8_t*>(&flags),4},false),
              "parameter updates cannot cross the effect state boundary");
        Check(!chain.LoadImage(corrupt) && chain.IsReady(),"a rejected replacement must preserve the playing effect chain");
        float buses[32][32]{},output[64]{};
        uint64_t fingerprint=14695981039346656037ull;
        std::vector<std::vector<uint8_t>> histories;
        for (const auto& effect:chain.Descriptions()) histories.emplace_back(effect.state.size);
        uint32_t random=0x12345678;
        for (unsigned block=0;block<3000;++block) {
            if (chain.Descriptions().size()==9 && (block==400 || block==900 || block==1100)) {
                std::array<uint32_t,554> parameters;
                Check(chain.ReadParameters(3,0,{reinterpret_cast<uint8_t*>(parameters.data()),sizeof(parameters)}),"read room parameters before update");
                if (block==400) {
                    for (unsigned i=70;i<89;++i) parameters[i]+=16;
                    for (unsigned i=99;i<105;++i) parameters[i]/=2;
                    parameters[4]|=4;
                } else parameters[105]=block==900?2:4;
                const auto bytes=std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(parameters.data()),sizeof(parameters));
                Check(chain.SetParameters(3,16,bytes.subspan(16,4),true) &&
                      chain.SetParameters(3,280,bytes.subspan(280,264),true),"queue room parameters");
                chain.CommitParameters();
            }
            for (auto& bus:buses) for (auto& sample:bus) {
                random=1664525u*random+1013904223u;
                const float divisor=block>=1400 && block<1600 ? 1.f : 32.f;
                sample=block<2000 ? (int32_t(random>>8)-0x800000)/8388608.f/divisor : 0;
            }
            openshao::audio::SpeakerBlock speakers;
            Check(chain.ProcessSpeakers(buses,speakers),chain.LastError().c_str());
            // The frozen reference covers effect processing before the host stereo downmix.
            for (unsigned i=0;i<32;++i) for (unsigned c=0;c<2;++c) output[i*2+c]=speakers[c][i];
            Fingerprint(fingerprint,{reinterpret_cast<const uint8_t*>(output),sizeof(output)});
            for (unsigned effect=0;effect<histories.size();++effect) {
                Check(chain.ReadParameters(effect,0,histories[effect]),"read processed effect histories");
                Fingerprint(fingerprint,histories[effect]);
            }
        }
        uint64_t image_fingerprint=14695981039346656037ull;
        Fingerprint(image_fingerprint,image);
        // These sequences matched the original programs sample for sample and state
        // word for state word before the reference execution tools were removed.
        if (image_fingerprint==0x9BBE14297F9AF82Bull)
            Check(fingerprint==0x1569F22F3E074DF0ull,"standard effects must match the validated output and history sequence");
        if (image_fingerprint==0x9863549866BC8B56ull)
            Check(fingerprint==0xA6BB097863538E80ull,"speaker crosstalk must match the validated output and history sequence");
        Check(chain.Statistics().blocks==3000 && !chain.Statistics().failures,"processing must finish without dropping blocks");
    }
    std::puts("PASS: effect image validation and block processing");
}
