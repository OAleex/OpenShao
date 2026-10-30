#pragma once
#include <algorithm>
#include <cstdint>

namespace openshao::frames {
// Guest simulation uses 60 steps per second, independently of the output cadence.
class Clock {
public:
    unsigned Take(int64_t now) {
        if(!started_ || now<last_ || now-last_>250000000) {
            started_=true;origin_=last_=now;steps_=1;alpha_=0;++epoch_;return 1;
        }
        last_=now;
        const auto elapsed=uint64_t(std::max<int64_t>(0,now-origin_));
        alpha_=double(elapsed*60%1000000000ull)/1000000000.;
        const auto expected=elapsed*60/1000000000ull+1;
        if(expected<=steps_)return 0;
        const auto count=unsigned(std::min<uint64_t>(8,expected-steps_));
        steps_+=count;return count;
    }
    double Alpha()const{return alpha_;}
    uint64_t Epoch()const{return epoch_;}
    void Reset(){started_=false;steps_=0;alpha_=0;}
private:
    bool started_=false;
    int64_t origin_=0,last_=0;
    uint64_t steps_=0,epoch_=0;
    double alpha_=0;
};
}
