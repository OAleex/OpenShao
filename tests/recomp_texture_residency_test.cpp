#include "texture_residency.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace openshao::texpack;

static void Check(bool ok, const std::string& text) {
    if (!ok) throw std::runtime_error(text);
}

using Hashes = std::vector<uint64_t>;

// A small set: protection off unless a test turns it on, so the order of use is all that counts
static ResidencyOptions Plain(uint32_t protect = 0, uint32_t per_plan = 1000) {
    ResidencyOptions o;
    o.protect_frames = protect;
    o.low_water = 0.9;
    o.max_per_plan = per_plan;
    return o;
}

int main() try {
    // Nothing is taken out while the limits hold
    {
        Residency r(Plain());
        r.Add(1, 100, 10);
        r.Add(2, 100, 11);
        Check(r.bytes() == 200 && r.count() == 2 && r.Contains(1) && !r.Contains(3), "The totals follow what is added");
        const auto plan = r.Plan(1000, {200, 2});
        Check(plan.drop.empty() && plan.fits, "Exactly at the limits nothing goes");
        Check(r.Plan(1000, {1000, 10}).drop.empty(), "Well inside the limits nothing goes");
    }

    // The one least recently used goes first, and ties go to the one added first
    {
        Residency r(Plain());
        r.Add(1, 100, 10);
        r.Add(2, 100, 20);
        r.Add(3, 100, 30);
        r.Add(4, 100, 20);  // same use as 2, added after it
        r.Touch(1, 40);     // the oldest is now the newest
        auto plan = r.Plan(1000, {350, 100});
        // over by 50 of 350: down to 315 means one goes, the oldest use is 2
        Check(plan.drop == Hashes({2}) && plan.fits, "The least recently used goes first, and Touch moves one to the end");
        plan = r.Plan(1000, {300, 100});
        // 400 bytes against 300: down to 270, so two go: 2, then 4 (same use, added later)
        Check(plan.drop == Hashes({2, 4}), "Equal use goes in the order added");
    }

    // Once a limit is passed it goes down to the low-water mark, not just under the limit
    {
        Residency r(Plain());
        for (uint64_t i = 1; i <= 10; ++i) r.Add(i, 100, i);
        const auto plan = r.Plan(1000, {999, 100});  // 1000 bytes, limit 999, mark 899: 2 go (800 bytes is under 899)
        Check(plan.drop == Hashes({1, 2}) && plan.fits, "It goes down to 90% of the limit");
    }

    // The count is a limit of its own
    {
        Residency r(Plain());
        for (uint64_t i = 1; i <= 10; ++i) r.Add(i, 1, i);
        const auto plan = r.Plan(1000, {1000000, 10}, 0, 1);  // one more would make 11 of 10: mark 9, minus the incoming one: 8
        Check(plan.drop.size() == 2 && plan.drop[0] == 1 && plan.drop[1] == 2 && plan.fits, "The count limit works alone and counts what comes in");
    }

    // What comes in counts for the bytes
    {
        Residency r(Plain());
        r.Add(1, 400, 1);
        r.Add(2, 400, 2);
        Check(r.Plan(1000, {1000, 100}).drop.empty(), "800 of 1000 fits");
        const auto plan = r.Plan(1000, {1000, 100}, 300);  // 1100 of 1000: mark 900, minus 300: down to 600
        Check(plan.drop == Hashes({1}) && plan.fits, "The incoming bytes make room");
    }

    // What was drawn lately, or added in this frame, is never taken out
    {
        Residency r(Plain(120));
        r.Add(1, 100, 0);    // 120 frames before the plan below: just out of the window
        r.Add(2, 100, 100);  // 20 frames before it
        r.Add(3, 100, 119);
        r.Add(4, 100, 120);  // this frame
        Check(r.Protected(120) == 3, "Used in the last 120 frames, or added now, is protected: 2, 3 and 4");
        auto plan = r.Plan(120, {100, 100});
        Check(plan.drop == Hashes({1}) && !plan.fits, "Only the one not protected goes, and what is left does not fit");
        Check(r.Plan(131, {100, 100}).drop == Hashes({1}), "A little later the others are still in the window");
        plan = r.Plan(221, {100, 100});  // 100 is 121 frames back: out; 119 and 120 are 102 and 101 back: still in
        Check(plan.drop == Hashes({1, 2}) && !plan.fits, "The window moves with the frame");
        Check(r.Plan(300, {100, 100}).drop == Hashes({1, 2, 3, 4}), "Everything old enough can go, down to the mark of 90 bytes");
    }

    // A frame in the future protects (the counter went back)
    {
        Residency r(Plain(120));
        r.Add(1, 100, 500);
        Check(r.Protected(10) == 1 && r.Plan(10, {10, 1}).drop.empty(), "An entry from a later frame is not taken out");
    }

    // With nothing to take out, nothing goes and it does not fit
    {
        Residency r(Plain(120));
        r.Add(1, 100, 50);
        r.Add(2, 100, 60);
        const auto plan = r.Plan(70, {150, 10});
        Check(plan.drop.empty() && !plan.fits, "Everything protected: nothing goes and the renderer is told it does not fit");
        const auto incoming = r.Plan(70, {1000, 10}, 2000);
        Check(incoming.drop.empty() && !incoming.fits, "An incoming one larger than the limit never fits");
    }

    // At most `max_per_plan` go at a time
    {
        Residency r(Plain(0, 3));
        for (uint64_t i = 1; i <= 10; ++i) r.Add(i, 100, i);
        const auto plan = r.Plan(1000, {300, 100});
        Check(plan.drop == Hashes({1, 2, 3}) && !plan.fits, "One plan takes out three, and the rest is for the next frames");
        for (uint64_t h : plan.drop) r.Remove(h);
        Check(r.Plan(1000, {300, 100}).drop == Hashes({4, 5, 6}), "The next plan continues where this one stopped");
    }

    // Remove, Add over an existing hash, Clear and Touch of an unknown one
    {
        Residency r(Plain());
        r.Add(1, 100, 1);
        r.Add(2, 50, 2);
        Check(r.Remove(1) && !r.Remove(1) && r.bytes() == 50 && r.count() == 1, "Remove says whether it was there and updates the totals");
        r.Add(2, 80, 5);
        Check(r.bytes() == 80 && r.count() == 1, "Adding a hash again replaces its size, not adds to it");
        r.Touch(99, 10);
        Check(!r.Contains(99) && r.count() == 1, "Touching an unknown hash adds nothing");
        r.Add(7, 80, 4);
        r.Touch(2, 3);  // an older frame does not move it back
        r.Touch(7, 2);
        Check(r.Plan(6, {100, 10}).drop == Hashes({7}), "Touch with an older frame does not make it older");
        r.Clear();
        Check(r.bytes() == 0 && r.count() == 0 && r.Plan(10, {0, 0}).drop.empty(), "Clear empties it, and an empty set has nothing to take out");
    }

    // Limits at zero or very small
    {
        Residency r(Plain());
        r.Add(1, 100, 1);
        const auto plan = r.Plan(1000, {0, 0});
        Check(plan.drop == Hashes({1}) && plan.fits, "With a zero budget everything old goes");
        Residency empty(Plain());
        const auto none = empty.Plan(1000, {0, 0}, 10, 1);
        Check(none.drop.empty() && !none.fits, "Nothing here and a zero budget: the incoming one does not fit");
    }

    // Against a model, over thousands of random operations: the totals never drift and a plan is what the rules say
    {
        std::mt19937_64 rng(12345);
        Residency r(Plain(30, 8));
        struct M {
            uint64_t bytes, last, order;
        };
        std::map<uint64_t, M> model;
        uint64_t order = 0, frame = 1000;
        for (int step = 0; step < 20000; ++step) {
            frame += rng() % 4;
            const uint64_t hash = rng() % 300;
            switch (rng() % 5) {
            case 0:
            case 1: {
                const uint64_t bytes = 1 + rng() % 500;
                auto it = model.find(hash);
                if (it != model.end()) {
                    it->second.bytes = bytes;
                    it->second.last = frame;
                } else {
                    model[hash] = {bytes, frame, order++};
                }
                r.Add(hash, bytes, frame);
                break;
            }
            case 2:
                if (model.count(hash) && frame > model[hash].last) model[hash].last = frame;
                r.Touch(hash, frame);
                break;
            case 3:
                Check(r.Remove(hash) == (model.erase(hash) == 1), "Remove agrees with the model");
                break;
            default: {
                const ResidencyLimits limits{uint64_t(rng() % 30000), uint32_t(rng() % 120)};
                const uint64_t in_bytes = rng() % 800;
                const uint32_t in_count = uint32_t(rng() % 2);
                const auto plan = r.Plan(frame, limits, in_bytes, in_count);
                // the model's own plan
                uint64_t bytes = 0;
                for (auto& [h, m] : model) bytes += m.bytes;
                size_t count = model.size();
                std::map<std::pair<uint64_t, uint64_t>, uint64_t> by;
                for (auto& [h, m] : model)
                    if (!(m.last >= frame || frame - m.last < 30)) by[{m.last, m.order}] = h;
                Hashes expected;
                if (bytes + in_bytes > limits.max_bytes || count + in_count > limits.max_count) {
                    const uint64_t mb = uint64_t(double(limits.max_bytes) * 0.9), mc = uint64_t(double(limits.max_count) * 0.9);
                    const uint64_t tb = mb > in_bytes ? mb - in_bytes : 0, tc = mc > in_count ? mc - in_count : 0;
                    for (auto& [key, h] : by) {
                        if (bytes <= tb && count <= tc) break;
                        if (expected.size() >= 8) break;
                        expected.push_back(h);
                        bytes -= model[h].bytes;
                        --count;
                    }
                }
                Check(plan.drop == expected, "The plan is what the rules say, step " + std::to_string(step));
                Check(plan.fits == (bytes + in_bytes <= limits.max_bytes && count + in_count <= limits.max_count), "Fits agrees with the model");
                for (uint64_t h : plan.drop) {
                    Check(r.Remove(h), "A hash in a plan is there");
                    model.erase(h);
                }
            }
            }
            uint64_t total = 0;
            for (auto& [h, m] : model) total += m.bytes;
            Check(r.bytes() == total && r.count() == model.size(), "The totals match the model, step " + std::to_string(step));
        }
    }

    std::cout << "PASS: texture residency\n";
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
