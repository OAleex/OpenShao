#pragma once
#include <atomic>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>

namespace openshao::frames {
inline constexpr std::array<int,5> RateOptions{60,120,144,165,240};
inline constexpr bool SupportedRate(int rate) {
    return std::find(RateOptions.begin(),RateOptions.end(),rate)!=RateOptions.end();
}
inline constexpr int SafeRate(int rate){return SupportedRate(rate)?rate:60;}
inline std::atomic<int> target_rate{60};
inline std::atomic<bool> synchronize{false};
inline int EffectiveRate(int requested,bool vsync,double refresh) {
    if(!vsync)return SafeRate(requested);
    return std::isfinite(refresh) && refresh>=30 && refresh<=1000?int(std::lround(refresh)):60;
}
inline void Configure(int rate,bool vsync=false){target_rate.store(SafeRate(rate),std::memory_order_relaxed);synchronize.store(vsync,std::memory_order_relaxed);}
int Target();
inline int OutputCap(){return synchronize.load(std::memory_order_relaxed)?0:target_rate.load(std::memory_order_relaxed);}
// Menus run at 60 Hz; spread display frames on refresh rates that are not multiples of 60.
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
// Replay of a queued polygon list: Before needs the per-frame flags, After already cleared them.
enum class Replay:uint8_t{None,Before,After};
inline thread_local Replay replay=Replay::None;
inline void BeforePolygons(uint32_t address){if(before_polygons)before_polygons(address);}
}
