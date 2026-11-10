#pragma once
#include <cstdint>

// How much video memory the texture pack replacements may take. The renderer reads the numbers from its driver and this works
// out the limit, so the rule is tested without a GPU. docs/MODS.md describes it.
namespace openshao::texpack {
struct BudgetInputs {
    uint64_t forced_mb = 0;         // a limit the user set, in MiB; 0 when none
    uint64_t driver_budget = 0;     // what the driver says this process may use, already net of other programs; 0 when unknown
    uint64_t driver_usage = 0;      // what this process uses in all
    uint64_t own_usage = 0;         // the part of `driver_usage` that is the replacements
    uint64_t dedicated_bytes = 0;   // the adapter's own video memory; 0 when unknown
};

enum class BudgetSource { Forced, Driver, Dedicated, Fallback };

struct Budget {
    uint64_t bytes = 0;
    BudgetSource source = BudgetSource::Fallback;
};

// A forced limit is taken as it is. With the driver's budget the limit is 60% of it less what the process uses apart from the
// replacements; without, 40% of the adapter's own memory; with neither, the floor. Everything but a forced limit has a floor.
Budget ComputeBudget(const BudgetInputs& inputs);

constexpr uint64_t kBudgetFloorBytes = 256ull << 20;

const char* DescribeBudget(BudgetSource source);
}
