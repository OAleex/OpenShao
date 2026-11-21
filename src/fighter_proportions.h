#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::fighter_proportions {
// Runs before native 0x107880 rebuilds world bones from local transforms.
void BeforePoseRebuild(const X86Context& ctx, uint32_t caller, uint32_t model);
}
