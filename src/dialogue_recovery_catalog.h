#pragma once
#include <cstdint>
#include <span>

namespace openshao::dialogue {
enum class TakeMode { Any, Solo, Koop };
struct ContextTake {
    uint32_t sound;
    TakeMode mode = TakeMode::Any;
    bool reaction = false;
    bool once = false;
};
struct ContextFamily {
    uint32_t room, original;
    int16_t actor, alternate_actor;
    std::span<const ContextTake> takes;
};
// Follow Baraka's phase-3 speech (A0553378) with one reaction; preserve the earlier 07 taunts.
inline constexpr ContextTake BarakaLateReactions[]{
    {0xF26D1CDF},{0xB5CD660F},{0x88AD4FBF},{0x37CA2251},{0x706A5881},
    {0x4D0A7131},{0x91BD29E5},{0xD61D5335},{0xEB7D7A85},
};
// Alternate 08 takes follow the phase-3 speech in solo, or the original 08_1 in Ko-op.
inline constexpr uint32_t BarakaSpokenAfter[]{0x9F25265E, 0xA2450FEE};
// Match existing speakers and encounters; direct sound IDs retain language-pack bank mapping.
inline constexpr ContextTake CageWork[]{{0x34179BEB},{0xBB576E4B}};
inline constexpr ContextTake KanoTaunts[]{
    {0x7D0C8867},{0x3AACF2B7},
    {0xDB7B83D3, TakeMode::Solo},{0x9CDBF903, TakeMode::Solo},
    {0x1EDCBD5D, TakeMode::Any, true},{0x597CC78D, TakeMode::Any, true},
    {0x641CEE3D, TakeMode::Any, true},
};
inline constexpr ContextTake ReptileTaunts[]{
    {0xE8D4E56F},{0xAF749FBF},{0xD4A5AA7A, TakeMode::Any, false, true},
    {0x9305D0AA, TakeMode::Any, false, true},{0x1C45250A},
    // lf_17_05_1/2/3/9/12/13: taunt takes outside the native group (Living Forest bank).
    {0x8B04D055},{0xCCA4AA85},{0xF1C48335},{0xBB749B94},{0x4FE2748B},{0x72825D3B},
};
// These takes address both monks: once each in Ko-op combat, after Mileena's entrance.
inline constexpr ContextTake MileenaForestKoop[]{
    {0xBEAE2A44, TakeMode::Koop, false, true},
    {0xF90E5094, TakeMode::Koop, false, true},
};
inline constexpr ContextTake ScorpionTaunts[]{
    {0xBDBB26C9},{0x784BCBC8},
    {0xD32CA20D, TakeMode::Koop},{0x948CD8DD, TakeMode::Koop},
    {0xA9ECF16D},{0x2763861E},{0x60C3FCCE},
    // nr_04_3/9/10: taunt takes outside the native group (Netherrealm bank).
    {0x0F9BFAD9},{0x452BE278},{0x9C60D6C1},
};
inline constexpr ContextTake LiuProjectile[]{{0x02DD3E0D}};
inline constexpr ContextTake KungProjectile[]{{0x6FAC5C27}};
inline constexpr ContextTake ShaoKoopCommands[]{
    {0x584D7D7D, TakeMode::Koop},{0xC4F60FC3, TakeMode::Koop},
    {0xEA6DA16D, TakeMode::Koop},{0xD9F33F7B, TakeMode::Koop},
};
inline constexpr ContextFamily ContextFamilies[]{
    {104, 0xC3FF4851, 71, -1, CageWork},
    {72, 0x898DD582, 54, -1, KanoTaunts},
    {144, 0x1569801B, 59, -1, ReptileTaunts},
    {132, 0x2DB6EB65, 66, -1, MileenaForestKoop},
    {160, 0xB9E947A4, 52, 55, ScorpionTaunts},
    {15, 0x457D44DD, 0, -1, LiuProjectile},
    {15, 0x280C26F7, 1, -1, KungProjectile},
    {73, 0x56E516CA, 62, -1, ShaoKoopCommands},
    {73, 0x8D3BE233, 58, -1, ShaoKoopCommands},
};
}
