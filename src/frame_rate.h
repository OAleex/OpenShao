#pragma once
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace openshao::frames {
inline std::atomic<int> target_rate{60};
inline std::atomic<bool> synchronize{false};
inline int EffectiveRate(int requested,bool vsync,double refresh) {
    if(!vsync)return requested==120?120:60;
    return std::isfinite(refresh) && refresh>=30 && refresh<=1000?int(std::lround(refresh)):60;
}
inline void Configure(int rate,bool vsync=false){target_rate.store(rate==120?120:60,std::memory_order_relaxed);synchronize.store(vsync,std::memory_order_relaxed);}
int Target();
inline int OutputCap(){return synchronize.load(std::memory_order_relaxed)?0:target_rate.load(std::memory_order_relaxed);}
// Menu logic runs once at 60 Hz. Distribute display frames without changing
// that rate on displays whose refresh is not an exact multiple of 60.
class OutputCadence {
    int remainder_=0,rate_=60;
public:
    unsigned Take(int rate) {
        if(rate_!=rate){rate_=rate;remainder_=0;}
        remainder_+=std::max(60,rate);
        const auto count=unsigned(remainder_/60);remainder_%=60;return count;
    }
};
inline void (*before_polygons)(uint32_t)=nullptr;
// Output-only passes draw a queued polygon list again. Before: the captured pass follows
// and needs the per-frame polygon flags. After: the captured pass already cleared them.
enum class Replay:uint8_t{None,Before,After};
inline thread_local Replay replay=Replay::None;
inline void BeforePolygons(uint32_t address){if(before_polygons)before_polygons(address);}
}
