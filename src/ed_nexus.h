#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::ed_nexus {
void FrontendInput(X86Context& ctx, uint8_t* base);
bool Unlocked();
bool GameplayPaused();
bool KontentUnlocked();
}
