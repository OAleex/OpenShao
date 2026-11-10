#include "survival_levels.h"
#include <array>
namespace openshao::survival {
namespace {
// Native resource IDs include 20 legacy files; the authored Level 1..9 messages give this order.
constexpr std::array<uint32_t,9> Rooms{161,162,163,164,165,166,187,191,190};
}
uint32_t RoomForLevel(unsigned level){return level && level<=Rooms.size()?Rooms[level-1]:0xFFFFFFFF;}
unsigned LevelForRoom(uint32_t room){for(unsigned i=0;i<Rooms.size();++i)if(Rooms[i]==room)return i+1;return 0;}
}
