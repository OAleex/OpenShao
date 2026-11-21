#pragma once
#include <cstdint>

// Retail lists The Suffering 2 demo only for English on NA consoles; elsewhere Kontent drops and skips that row.
namespace openshao::kontent {
inline constexpr uint32_t GameRegion = 0x5CE874;
inline constexpr uint32_t NorthAmerica = 1, RestOfWorld = 4;
constexpr uint32_t KontentRegion(uint32_t region) { return region == NorthAmerica ? RestOfWorld : region; }
// Replaces a North American region in guest memory. Call after the retail code caches XGetGameRegion.
void HideDemo();
}
