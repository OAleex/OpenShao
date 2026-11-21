#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace openshao::campaign_moves {

struct GuideMove {
    std::string_view name;
    std::string_view command;
    uint16_t description = 0;
    std::string_view key = {};
    uint8_t button = 0;
    uint16_t title_source = 0;
};

// Commands use the original Xbox attack glyphs. See Prima's Versus move list.
inline constexpr std::array BarakaCombos{
    GuideMove{"Blade Flip", "~, ^", 997},
};
inline constexpr std::array ReptileCombos{
    GuideMove{"Double Kick", "*, *, *, |", 0, "moves.double_kick"},
    GuideMove{"Lizard Rush", "~, *, *, |", 0, "moves.lizard_rush"},
    GuideMove{"Green Lunge", "~, |", 0, "moves.green_lunge"},
    GuideMove{"Acid Summon", "~, ~, |", 0, "moves.acid_summon"},
};
inline constexpr std::array KitanaCombos{
    GuideMove{"Fan Flurry", "*, *, *, |", 1012},
    GuideMove{"Wind Wave", "~, ~, |", 1013},
    GuideMove{"Quick Draw Double Backflip", "*, *, *, ~", 0, "moves.double_backflip"},
};
inline constexpr std::array JohnnyCombos{
    GuideMove{"Nut Punch", "*, *, |", 1017},
    GuideMove{"Roundhouse of Doom", "~, ~, |", 0, "moves.roundhouse"},
    GuideMove{"Run Ender Launcher", "|, ~", 0, "moves.run_launcher"},
    GuideMove{"Light Run Ender", "|, *", 0, "moves.light_run"},
    GuideMove{"Heavy Run Ender", "|, |", 1019},
};
inline constexpr std::array BarakaVariants{
    GuideMove{"Jump Spark", "$ + *, *", 0, "moves.jump_spark"},
    GuideMove{"Blade Spin", "=, $ + ~", 0, "moves.blade_spin", 1},
};
inline constexpr std::array ReptileVariants{
    GuideMove{"Slow Homing Orb", "$ + | (3 s)", 0, "moves.homing_orb", 2},
};
inline constexpr std::array KitanaVariants{
    GuideMove{"Air Fan Toss", "=, $ + |", 0, "moves.air_fan", 2},
};
inline constexpr std::array JohnnyVariants{
    GuideMove{"Quick Draw", "$ + *, *, *", 0, "moves.quick_draw"},
};
inline constexpr std::array KitanaThrows{
    GuideMove{"Head Smack", "$ + ^, *", 0, "moves.head_smack", 0, 876},
    GuideMove{"Flip Up", "$ + ^, ~", 0, "moves.flip_up", 0, 877},
    GuideMove{"Power Hit", "$ + ^, |", 0, "moves.power_hit"},
};
inline constexpr std::array JohnnyThrows{
    GuideMove{"Head Smack", "$ + ^, *", 0, "moves.head_smack", 0, 876},
    GuideMove{"Flip Up", "$ + ^, ~", 0, "moves.flip_up", 0, 877},
    GuideMove{"Ground Slam", "$ + ^, |", 0, "moves.ground_slam"},
};

struct Profile {
    uint32_t fighter;
    std::array<uint16_t, 3> descriptions;
    std::span<const GuideMove> combos;
    std::span<const GuideMove> variants;
    std::span<const GuideMove> throws;
    std::array<uint16_t, 4> basic;
    uint16_t throw_label;
    uint16_t throw_description;
    uint8_t throw_count;
    uint16_t fatality_label;
    uint8_t fatality_count;

    constexpr uint32_t NodeCount() const {
        // Root and specials (8), throws, 16 Basic Moves, Combo/Fatality groups and Platform with 7 abilities.
        return 35 + throw_count + static_cast<uint32_t>(combos.size() + variants.size() + throws.size()) + fatality_count;
    }
};

inline constexpr std::array<Profile, 4> Profiles{{
    {83, {595, 695, 795}, BarakaCombos, BarakaVariants, {}, {335, 338, 337, 345}, 845, 895, 5, 1122, 2},
    {84, {600, 700, 800}, ReptileCombos, ReptileVariants, {}, {335, 336, 337, 345}, 850, 900, 5, 1134, 2},
    {89, {610, 710, 810}, KitanaCombos, KitanaVariants, KitanaThrows, {335, 338, 334, 339}, 860, 910, 1, 1158, 2},
    {90, {615, 715, 815}, JohnnyCombos, JohnnyVariants, JohnnyThrows, {333, 336, 337, 339}, 865, 915, 1, 1170, 3},
}};

inline constexpr uint32_t ExtraNodeCount = [] {
    uint32_t count = 0;
    for (const auto& profile : Profiles) count += profile.NodeCount();
    return count;
}();

// Missing text CRCs are resolved after native import using this host range.
inline constexpr uint32_t GuideTextBase = 0xFFFFFC00;
inline constexpr uint32_t GuideTextStride = 32;
static_assert([] {
    for (const auto& profile : Profiles) {
        if (profile.combos.size() + profile.variants.size() + profile.throws.size() > GuideTextStride / 2)
            return false;
        for (const auto& variant : profile.variants)
            if (variant.button >= 3) return false;
    }
    return true;
}());
inline constexpr uint32_t GuideLabel(size_t profile, size_t row) {
    return GuideTextBase + static_cast<uint32_t>(profile * GuideTextStride + row * 2);
}

inline constexpr std::array<uint8_t, 3> SpecialSkills{0, 2, 1};

}
