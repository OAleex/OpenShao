#pragma once
#include <chrono>

namespace openshao::launcher::footer_layout {

struct CachedCheckPulse {
    using Clock = std::chrono::steady_clock;
    static constexpr auto duration = std::chrono::milliseconds(650);
    Clock::time_point until{};
    void Start(Clock::time_point now) { until = now + duration; }
    bool Active(Clock::time_point now) const { return now < until; }
};

struct Rows {
    float divider;
    float primary;
    float secondary;
    float button_top;
    float button_bottom;

    constexpr explicit Rows(float height)
        : divider(height - 76.f),
          primary(divider + 13.f),
          secondary(divider + 44.f),
          button_top(divider + 6.f),
          button_bottom(divider + 38.f) {}
};

}
