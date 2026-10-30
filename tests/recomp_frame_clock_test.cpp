#include "frame_clock.h"
#include "frame_rate.h"
#include <cmath>
#include <iostream>

int main() {
    unsigned failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    using openshao::frames::Clock;
    for(const int rate:{40,60,75,90,100,120,144,165,240}) {
        Clock clock;
        check(clock.Take(0)==1,"A new gameplay interval must begin with one native update");
        unsigned updates=0;
        for(int output=1;output<=rate*10;++output) {
            updates+=clock.Take(int64_t(output)*1000000000/rate);
            check(clock.Alpha()>=0 && clock.Alpha()<1,"Visual interpolation must stay inside the current simulation interval");
        }
        check(updates==600,"Ten seconds must contain 600 gameplay updates at every output rate");
    }
    Clock clock;
    clock.Take(0);
    check(clock.Take(8333333)==0 && std::abs(clock.Alpha()-.5)<.000001,
          "The midpoint output must interpolate without advancing gameplay");
    check(clock.Take(16666667)==1 && clock.Alpha()<.000001,"The next simulation interval must advance once");
    check(clock.Take(16666667)==0,"Rendering twice at the same timestamp must not advance gameplay");
    check(clock.Take(116666667)==6,"A short missed interval must retain the six elapsed simulation steps");
    check(clock.Take(1116666667)==1,"A loading stall must start a new interval without a burst of catch-up gameplay");
    check(clock.Take(100)==1 && clock.Alpha()==0,"A clock discontinuity must clear timing history");
    clock.Reset();
    check(clock.Take(5000000000)==1 && clock.Alpha()==0,"Changing frame-rate mode must start without accumulated gameplay steps");
    for(int rate:{60,75,100,120,144,165,240}) {
        check(openshao::frames::EffectiveRate(60,true,rate)==rate && openshao::frames::EffectiveRate(120,true,rate)==rate,
              "VSync must follow the current display refresh instead of the stored software cap");
        check(openshao::frames::EffectiveRate(60,false,rate)==60 && openshao::frames::EffectiveRate(120,false,rate)==120,
              "Disabling VSync must restore the selected software cap");
    }
    check(openshao::frames::EffectiveRate(120,true,0)==60 && openshao::frames::EffectiveRate(60,true,NAN)==60,
          "Unavailable display modes must use the safe 60 Hz fallback");
    check(openshao::frames::EffectiveRate(60,true,59.94)==60,"Fractional refresh must round to its nominal output target");
    for(int rate:{60,75,100,120,144,165,240}) {
        openshao::frames::OutputCadence cadence;unsigned outputs=0;
        for(unsigned update=0;update<600;++update)outputs+=cadence.Take(rate);
        check(outputs==unsigned(rate)*10,"Menu output must follow display refresh over 600 native updates");
    }
    openshao::frames::OutputCadence cadence;
    cadence.Take(100);
    check(cadence.Take(60)==1 && cadence.Take(120)==2,"Changing monitors must clear the previous output remainder");
    openshao::frames::Configure(60,true);
    check(openshao::frames::OutputCap()==0,"The old 60 FPS software cap must not throttle synchronized display frames");
    openshao::frames::Configure(120,false);
    check(openshao::frames::OutputCap()==120,"Disabling VSync must restore the requested software pacing");
    return failures?1:0;
}
