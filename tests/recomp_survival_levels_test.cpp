#include "survival_levels.h"
#include <cstdio>

int main() {
    using namespace openshao::survival;
    unsigned failures = 0;
    const auto check = [&](bool value, const char* reason) {
        if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", reason); }
    };
    constexpr unsigned rooms[]{161, 162, 163, 164, 165, 166, 187, 191, 190};
    for (unsigned i = 0; i < 9; ++i)
        check(RoomForLevel(i + 1) == rooms[i] && LevelForRoom(rooms[i]) == i + 1,
              "level lookup follows the authored messages, with native archive IDs adjusted by 20");
    check(!LevelForRoom(96) && !LevelForRoom(160) && !LevelForRoom(167) && !LevelForRoom(189),
          "campaign, Versus and missing rooms are excluded");
    check(RoomForLevel(0) == 0xFFFFFFFF && RoomForLevel(10) == 0xFFFFFFFF,
          "the update contains exactly nine levels");
    std::printf("Survival levels: %u failures\n", failures);
    return failures ? 1 : 0;
}
