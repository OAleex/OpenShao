#include <chrono>
#include <cstdio>
#include <thread>
#include "vblank_compat.h"

int main() {
    using namespace std::chrono;
    openshao::VBlankClock clock;
    // A notification after 3 ms releases the wait at once (the old 16 ms sleep added latency).
    std::thread producer([&] {
        std::this_thread::sleep_for(milliseconds(3));
        clock.Tick();
    });
    auto start = steady_clock::now();
    const bool signalled = clock.WaitNext(milliseconds(100));
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now()-start).count();
    producer.join();
    if (!signalled || elapsed >= 60) {
        std::fprintf(stderr,"FAIL: vertical blank wait took %lld ms signalled=%d\n",(long long)elapsed,int(signalled));
        return 1;
    }
    // A past pulse is not the next blank; must wait for a new one or timeout.
    start=steady_clock::now();
    if (clock.WaitNext(milliseconds(8)) || steady_clock::now()-start < milliseconds(5)) {
        std::puts("FAIL: stale vertical blank was mistaken for a new one");
        return 2;
    }
    std::thread second([&] {
        std::this_thread::sleep_for(milliseconds(4));
        clock.Tick();
    });
    bool ok=clock.WaitNext(milliseconds(100));
    second.join();
    if (!ok) { std::puts("FAIL: subsequent retrace not observed"); return 3; }
    std::puts("PASS: original wait-until-next-vblank semantics; no fixed 16ms delay");
}
