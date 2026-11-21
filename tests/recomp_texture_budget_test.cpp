#include "texture_budget.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace openshao::texpack;

static void Check(bool ok, const std::string& text) {
    if (!ok) throw std::runtime_error(text);
}

constexpr uint64_t kMiB = 1ull << 20;

int main() try {
    // A limit the user sets is taken as it is, with no floor and whatever else is known
    {
        BudgetInputs in;
        in.forced_mb = 64;
        in.driver_budget = 4096 * kMiB;
        Budget b = ComputeBudget(in);
        Check(b.bytes == 64 * kMiB && b.source == BudgetSource::Forced, "a forced limit stays below the floor");
        in.forced_mb = 1;
        Check(ComputeBudget(in).bytes == kMiB, "even one MiB");
    }

    // The driver's budget: 60% of it, less what the process uses apart from the replacements
    {
        BudgetInputs in;
        in.driver_budget = 3000 * kMiB;
        in.driver_usage = 500 * kMiB;
        in.own_usage = 200 * kMiB;
        const Budget b = ComputeBudget(in);
        Check(b.source == BudgetSource::Driver, "the source is the driver");
        Check(b.bytes == 3000 * kMiB / 10 * 6 - 300 * kMiB, "60% less the other usage");
    }

    // Only the part the replacements do not use counts against them, so their own memory is not charged twice
    {
        BudgetInputs in;
        in.driver_budget = 3000 * kMiB;
        in.driver_usage = 200 * kMiB;
        in.own_usage = 200 * kMiB;
        Check(ComputeBudget(in).bytes == 3000 * kMiB / 10 * 6, "all the usage is the replacements");
        in.own_usage = 900 * kMiB;  // more than the total: the other usage is nothing, not a negative number
        Check(ComputeBudget(in).bytes == 3000 * kMiB / 10 * 6, "an own usage past the total");
    }

    // Another program takes memory: the limit falls, to the floor and not below it
    {
        BudgetInputs in;
        in.driver_budget = 2000 * kMiB;
        in.own_usage = 0;
        in.driver_usage = 800 * kMiB;
        Check(ComputeBudget(in).bytes == 2000 * kMiB / 10 * 6 - 800 * kMiB, "the other program is subtracted");
        in.driver_usage = 1000 * kMiB;  // 200 MiB would be left, under the floor
        Check(ComputeBudget(in).bytes == kBudgetFloorBytes, "what is left under the floor is the floor");
        in.driver_usage = 1800 * kMiB;
        Check(ComputeBudget(in).bytes == kBudgetFloorBytes, "the floor holds when the others use nearly everything");
    }

    // Without the driver's figure, 40% of the adapter's memory
    {
        BudgetInputs in;
        in.dedicated_bytes = 4096 * kMiB;
        const Budget b = ComputeBudget(in);
        Check(b.source == BudgetSource::Dedicated && b.bytes == 4096 * kMiB / 10 * 4, "40% of the adapter's memory");
        in.dedicated_bytes = 512 * kMiB;
        Check(ComputeBudget(in).bytes == kBudgetFloorBytes, "a small adapter gets the floor");
    }

    // With nothing known
    {
        const Budget b = ComputeBudget({});
        Check(b.source == BudgetSource::Fallback && b.bytes == kBudgetFloorBytes, "the floor when nothing is known");
    }

    // The driver's figure wins over the adapter's size
    {
        BudgetInputs in;
        in.driver_budget = 1000 * kMiB;
        in.dedicated_bytes = 8192 * kMiB;
        Check(ComputeBudget(in).source == BudgetSource::Driver, "the driver's figure comes first");
    }

    // Every source has a description
    for (auto s : {BudgetSource::Forced, BudgetSource::Driver, BudgetSource::Dedicated, BudgetSource::Fallback})
        Check(*DescribeBudget(s) != '\0', "a description");
    std::cout << "texture budget: ok\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
