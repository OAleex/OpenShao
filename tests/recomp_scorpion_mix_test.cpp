#include "scorpion_preview_mix.h"
#include <array>
#include <cmath>
#include <iostream>
int main() {
    using openshao::scorpion::audio_preview::MixMenuVoice;
    std::array<float,8> game{.9f,.9f,-.9f,-.9f,1.f,-1.f,.75f,.75f};
    const std::array<float,8> voice{1.f,1.f,-1.f,-1.f,1.f,-1.f,0.f,0.f};
    MixMenuVoice(game.data(), voice.data(), game.size()/2);
    bool safe = true;
    for (unsigned i=0;i<6;++i) safe &= std::abs(game[i]) <= .951f;
    safe &= game[6] < .35f && game[7] < .35f;
    if (!safe) {
        std::cerr << "Scorpion preview must reserve headroom instead of clipping or boosting menu ambience\n";
        return 1;
    }
    std::array<float,14> surround; surround.fill(1);
    MixMenuVoice(surround.data(),voice.data(),2,6);
    for (unsigned i=0;i<2;++i) {
        if (std::abs(surround[6*i]-(.3f+voice[2*i]*.65f))>1e-6f || surround[6*i+2]!=.3f || surround[6*i+5]!=.3f) return 2;
    }
    if (surround[12]!=1 || surround[13]!=1) return 3;
    std::cout << "PASS: preview voice mixes without menu amplification or saturation\n";
}
