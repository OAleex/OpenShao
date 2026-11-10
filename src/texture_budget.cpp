#include "texture_budget.h"

#include <algorithm>

namespace openshao::texpack {
Budget ComputeBudget(const BudgetInputs& inputs) {
    if (inputs.forced_mb) return {inputs.forced_mb << 20, BudgetSource::Forced};
    if (inputs.driver_budget) {
        const uint64_t share = inputs.driver_budget / 10 * 6;
        const uint64_t others = inputs.driver_usage > inputs.own_usage ? inputs.driver_usage - inputs.own_usage : 0;
        const uint64_t bytes = share > others ? share - others : 0;
        return {std::max(bytes, kBudgetFloorBytes), BudgetSource::Driver};
    }
    if (inputs.dedicated_bytes) return {std::max(inputs.dedicated_bytes / 10 * 4, kBudgetFloorBytes), BudgetSource::Dedicated};
    return {kBudgetFloorBytes, BudgetSource::Fallback};
}

const char* DescribeBudget(BudgetSource source) {
    switch (source) {
        case BudgetSource::Forced: return "OPENSHAO_HD_VRAM_MB";
        case BudgetSource::Driver: return "60% of the driver's budget";
        case BudgetSource::Dedicated: return "40% of the adapter's video memory";
        default: return "the floor, the adapter reported nothing";
    }
}
}
