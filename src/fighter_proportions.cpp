#include "fighter_proportions.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>

// Story clips shared with Liu Kang write his bone offsets into other skeletons; restore the fighter's own.
namespace openshao::fighter_proportions {
namespace {
constexpr uint32_t CharacterUpdateReturn = 0x12B8E4;  // cPxCharacter::Update -> 0x107880
constexpr uint32_t State = 0x5ACEBC, GameMode = 0x88;  // 0 Solo, 1 Ko-op, 2 Versus
constexpr uint32_t BoneStride = 0x390, BoneName = 0x348;
constexpr uint32_t BoneLocalTranslation = 0xA0;  // copied into the local matrix by 0x107880
constexpr uint32_t BoneRestTranslation = 0xF0;   // model-space rest position from cPxBone::Create

constexpr size_t MaxBones = 6;
struct Profile {
    uint16_t fighter;
    bool collapsed_only;  // restore only offsets a clip collapsed below a quarter of the rest length
    std::array<const char*, MaxBones> bones;
};
constexpr std::array<Profile, 2> Profiles{{
    {89, false, {"chest", "neck", "head", "eyebrow", "eye_lids", "jaw"}},  // Kitana: neck and forehead
    {90, true, {"Object01"}},                                              // Johnny Cage: sunglasses
}};

struct Fighter {
    uint32_t bones = 0, count = 0;
    const Profile* profile = nullptr;
    std::array<uint32_t, MaxBones> bone{}, parent{};
    uint32_t logged = 0;
};
// Identified on the character update path, then also corrected for cinematic callers.
std::unordered_map<uint32_t, Fighter> fighters;

bool Valid(uint32_t p, size_t n) {
    return p >= 0x10000 && n && uint64_t(p) + n < 0x100000000ull && mem::IsCommitted(p) &&
           mem::IsCommitted(p + uint32_t(n) - 1);
}
bool NameIs(uint32_t bone, const char* name) {
    char text[16]{};
    std::memcpy(text, kernel::HostPtr(bone + BoneName), sizeof(text) - 1);
    return std::strcmp(text, name) == 0;
}
const Profile* Find(uint16_t fighter) {
    for (const auto& profile : Profiles)
        if (profile.fighter == fighter) return &profile;
    return nullptr;
}
bool StoryMode() {
    const auto state = kernel::R32(State);
    return Valid(state, GameMode + 4) && kernel::R32(state + GameMode) <= 1;
}
bool Bones(uint32_t model, uint32_t& bones, uint32_t& count) {
    if (!Valid(model, 0x34)) return false;
    bones = kernel::R32(model + 0x28);
    count = kernel::R32(model + 0x30);
    return count && count <= 512 && Valid(bones, count * BoneStride);
}

void Identify(const X86Context& ctx, uint32_t model) {
    if (!Valid(ctx.esi, 0xD4) || kernel::R32(ctx.esi + 0xC4) != model) return;
    const auto actor = kernel::R32(ctx.esi + 0xD0);
    const auto* profile = Valid(actor, 0x1E00) ? Find(kernel::R16(actor + 0x1DFE)) : nullptr;
    uint32_t bones = 0, count = 0;
    if (!profile || !Bones(model, bones, count)) { fighters.erase(model); return; }
    auto& entry = fighters[model];
    if (entry.profile == profile && entry.bones == bones && entry.count == count) return;
    entry = {bones, count, profile};
    for (size_t i = 0; i < profile->bones.size(); ++i) {
        if (!profile->bones[i]) continue;
        for (uint32_t b = 0; b < count; ++b) {
            const auto bone = bones + b * BoneStride;
            if (!NameIs(bone, profile->bones[i])) continue;
            const auto parent = kernel::R32(bone);
            if (parent >= bones && parent < bones + count * BoneStride) {
                entry.bone[i] = bone;
                entry.parent[i] = parent;
            }
            break;
        }
    }
}
}

void BeforePoseRebuild(const X86Context& ctx, uint32_t caller, uint32_t model) {
    if (caller == CharacterUpdateReturn) Identify(ctx, model);
    const auto found = fighters.find(model);
    if (found == fighters.end() || !StoryMode()) return;
    auto& entry = found->second;
    uint32_t bones = 0, count = 0;
    if (!Bones(model, bones, count) || bones != entry.bones || count != entry.count) {
        fighters.erase(found);
        return;
    }
    for (size_t i = 0; i < entry.bone.size(); ++i) {
        const auto bone = entry.bone[i];
        if (!bone) continue;
        std::array<float, 3> rest{};
        float animated = 0, target = 0;
        for (uint32_t c = 0; c < 3; ++c) {
            rest[c] = kernel::G<float>(bone + BoneRestTranslation + c * 4) -
                      kernel::G<float>(entry.parent[i] + BoneRestTranslation + c * 4);
            const float a = kernel::G<float>(bone + BoneLocalTranslation + c * 4);
            animated += a * a;
            target += rest[c] * rest[c];
        }
        if (!std::isfinite(target) || target < 1.f) continue;
        if (entry.profile->collapsed_only && animated * 16.f > target) continue;
        if (!(entry.logged & (1u << i)) && std::fabs(std::sqrt(animated) - std::sqrt(target)) > 0.5f) {
            entry.logged |= 1u << i;
            LOGR("Fighter proportions: type %u %s %.2f -> %.2f (rest offset)", unsigned(entry.profile->fighter),
                 entry.profile->bones[i], std::sqrt(animated), std::sqrt(target));
        }
        for (uint32_t c = 0; c < 3; ++c) kernel::G<float>(bone + BoneLocalTranslation + c * 4) = rest[c];
    }
}
}
