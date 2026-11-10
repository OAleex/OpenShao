#pragma once
#include <array>
#include <cmath>

namespace openshao::audio {
// Equal-power pan between adjacent speakers; phantom centre without a centre send; LFE separate.
inline std::array<float,5> SurroundPan(float right,float forward,bool centre=false) {
    constexpr float pi=3.14159265358979323846f;
    float angle=std::atan2(right,forward);
    if (std::abs(right)+std::abs(forward)<1e-6f) angle=0;
    constexpr float positions[]={-110*pi/180,-30*pi/180,0,30*pi/180,110*pi/180,250*pi/180};
    constexpr unsigned speakers[]={2,0,4,1,3,2};
    if (angle<positions[0]) angle+=2*pi;
    std::array<float,5> gains{};
    for (unsigned i=0;i<5;) {
        const unsigned next=!centre && i==1 ? 3 : i+1;
        if (angle>=positions[i] && angle<=positions[next]) {
            const float t=(angle-positions[i])/(positions[next]-positions[i]);
            gains[speakers[i]]=std::cos(t*pi*.5f);
            gains[speakers[next]]=std::sin(t*pi*.5f);
            break;
        }
        i=next;
    }
    for (float& gain:gains) if (gain<1e-6f) gain=0;
    return gains;
}
}
