#include "fighter_proportions.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr uint32_t StatePointer = 0x5ACEBC, State = 0x700000, Character = 0x701000, Actor = 0x702000;
constexpr uint32_t UpdateReturn = 0x12B8E4, CinematicReturn = 0x110E50;
constexpr uint32_t Stride = 0x390, Local = 0xA0, Rest = 0xF0, Matrix = 0x60, Name = 0x348;
std::array<uint8_t, 16 << 20> memory{};
std::vector<std::string> logged;
unsigned failures = 0;

void Check(bool passed, const char* reason) {
    if (passed) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", reason);
}

struct BoneSpec { const char* name; int parent; std::array<float, 3> rest, local; };

// Male offsets as written by Liu Kang's shared Story clips (measured in game).
const std::vector<BoneSpec> KitanaStory{
    {"pelvis", -1, {0, -445, 0}, {0, -445, 0}},
    {"abdomen", 0, {0, -500.26f, 0}, {0, -55, 0}},
    {"chest", 1, {0, -569.80f, 0}, {0, -76, 0}},
    {"neck", 2, {0, -682.50f, 12.79f}, {0, -125, 0}},
    {"head", 3, {0, -720.33f, 15.10f}, {0, -52, 0}},
    {"head_end", 4, {0, -794.96f, 3.77f}, {0, -74.63f, -11.33f}},
    {"eyebrow", 5, {0, -759.25f, -39.22f}, {0, 35, -42}},
    {"eye_lids", 5, {0, -751.49f, -30.67f}, {0, 43, -34}},
    {"jaw", 5, {0, -722.12f, -8.84f}, {0, 72, -12}},
};
const std::vector<BoneSpec> CageStory{
    {"pelvis", -1, {0, -445, 0}, {0, -445, 0}},
    {"chest", 0, {0, -521.07f, 0}, {0, -76, 0}},
    {"head_end", 1, {0, -594.10f, -11.33f}, {0, -72.15f, -11.33f}},
    {"Object01", 2, {0, -547.94f, -39.00f}, {0, 0, 0}},
};

struct Model { uint32_t base, bones; std::vector<BoneSpec> spec; };

Model Build(uint32_t base, const std::vector<BoneSpec>& spec) {
    Model model{base, base + 0x1000, spec};
    kernel::W32(base + 0x28, model.bones);
    kernel::W32(base + 0x30, uint32_t(spec.size()));
    for (size_t i = 0; i < spec.size(); ++i) {
        const auto bone = model.bones + uint32_t(i) * Stride;
        std::memset(kernel::HostPtr(bone), 0, Stride);
        kernel::W32(bone, spec[i].parent < 0 ? base + 0x50 : model.bones + uint32_t(spec[i].parent) * Stride);
        std::memcpy(kernel::HostPtr(bone + Name), spec[i].name, std::strlen(spec[i].name));
        for (uint32_t c = 0; c < 3; ++c) {
            kernel::G<float>(bone + Rest + c * 4) = spec[i].rest[c];
            kernel::G<float>(bone + Local + c * 4) = spec[i].local[c];
        }
        for (uint32_t c = 0; c < 16; ++c) kernel::G<float>(bone + Matrix + c * 4) = c % 5 ? 0.25f : 1.f;
    }
    return model;
}

std::array<float, 3> LocalOf(const Model& model, const char* name) {
    for (size_t i = 0; i < model.spec.size(); ++i) {
        if (std::strcmp(model.spec[i].name, name)) continue;
        const auto at = model.bones + uint32_t(i) * Stride + Local;
        return {kernel::G<float>(at), kernel::G<float>(at + 4), kernel::G<float>(at + 8)};
    }
    return {NAN, NAN, NAN};
}
std::array<float, 3> RestOffset(const Model& model, const char* name) {
    for (const auto& bone : model.spec) {
        if (std::strcmp(bone.name, name)) continue;
        const auto& parent = model.spec[size_t(bone.parent)].rest;
        return {bone.rest[0] - parent[0], bone.rest[1] - parent[1], bone.rest[2] - parent[2]};
    }
    return {NAN, NAN, NAN};
}
bool Near(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return std::fabs(a[0] - b[0]) < 0.01f && std::fabs(a[1] - b[1]) < 0.01f && std::fabs(a[2] - b[2]) < 0.01f;
}
bool Unchanged(const Model& model, const char* name) {
    for (const auto& bone : model.spec)
        if (!std::strcmp(bone.name, name)) return Near(LocalOf(model, name), bone.local);
    return false;
}
bool MatricesUnchanged(const Model& model) {
    for (size_t i = 0; i < model.spec.size(); ++i)
        for (uint32_t c = 0; c < 16; ++c)
            if (kernel::G<float>(model.bones + uint32_t(i) * Stride + Matrix + c * 4) != (c % 5 ? 0.25f : 1.f))
                return false;
    return true;
}

void Rebuild(const Model& model, uint16_t fighter, uint32_t mode, uint32_t caller) {
    kernel::W32(StatePointer, State);
    kernel::W32(State + 0x88, mode);
    kernel::W32(Character + 0xC4, model.base);
    kernel::W32(Character + 0xD0, Actor);
    kernel::W16(Actor + 0x1DFE, fighter);
    X86Context ctx{};
    ctx.esi = Character;
    openshao::fighter_proportions::BeforePoseRebuild(ctx, caller, model.base);
}
}

namespace mem {
bool IsCommitted(uint32_t address) { return address >= 0x10000 && address < memory.size(); }
}
namespace kernel {
uint8_t* g_base = memory.data();
}
namespace logx {
void Write(Level, const char* format, ...) {
    char text[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    logged.emplace_back(text);
}
}

int main() {
    {
        auto kitana = Build(0x800000, KitanaStory);
        Rebuild(kitana, 89, 0, UpdateReturn);
        for (const char* bone : {"chest", "neck", "head", "eyebrow", "eye_lids", "jaw"})
            Check(Near(LocalOf(kitana, bone), RestOffset(kitana, bone)), "Kitana Story bone keeps Liu Kang offset");
        for (const char* bone : {"pelvis", "abdomen", "head_end"})
            Check(Unchanged(kitana, bone), "Kitana bone outside the list changed");
        Check(MatricesUnchanged(kitana), "animated rotation changed");
        Check(logged.size() == 6, "each corrected Kitana bone is logged once");
        Rebuild(kitana, 89, 1, UpdateReturn);
        Check(logged.size() == 6, "a corrected bone is logged again");
    }
    {
        auto kitana = Build(0x900000, KitanaStory);
        Rebuild(kitana, 89, 2, UpdateReturn);
        for (const auto& bone : KitanaStory) Check(Unchanged(kitana, bone.name), "Versus Kitana changed");
    }
    {
        auto kitana = Build(0xA00000, KitanaStory);
        Rebuild(kitana, 89, 2, UpdateReturn);
        Rebuild(kitana, 89, 0, CinematicReturn);
        Check(Near(LocalOf(kitana, "neck"), RestOffset(kitana, "neck")), "cinematic caller skips a known Kitana");
    }
    {
        auto kitana = Build(0xB00000, KitanaStory);
        Rebuild(kitana, 89, 0, CinematicReturn);
        Check(Unchanged(kitana, "neck"), "an unidentified model changed");
    }
    {
        auto liu_kang = Build(0xC00000, KitanaStory);
        Rebuild(liu_kang, 0, 0, UpdateReturn);
        for (const auto& bone : KitanaStory) Check(Unchanged(liu_kang, bone.name), "Liu Kang changed");
    }
    {
        auto reused = Build(0xD00000, KitanaStory);
        Rebuild(reused, 89, 0, UpdateReturn);
        reused = Build(0xD00000, KitanaStory);
        Rebuild(reused, 83, 0, UpdateReturn);
        Rebuild(reused, 83, 0, CinematicReturn);
        Check(Unchanged(reused, "neck"), "a model reused by another fighter kept Kitana's correction");
    }
    {
        auto cage = Build(0xE00000, CageStory);
        Rebuild(cage, 90, 0, UpdateReturn);
        Check(Near(LocalOf(cage, "Object01"), RestOffset(cage, "Object01")), "Solo Cage sunglasses stay zeroed");
        Check(Unchanged(cage, "chest"), "Cage bone outside the list changed");
    }
    {
        auto cage = Build(0xF00000, CageStory);
        kernel::G<float>(cage.bones + 3 * Stride + Local + 4) = 20.f;
        Rebuild(cage, 90, 0, UpdateReturn);
        Check(Near(LocalOf(cage, "Object01"), {0, 20, 0}), "Cage clip motion of the sunglasses was overridden");
    }
    {
        // Ko-op player 2 plays Kung Lao's cinematic clips, which leave Object01 at (0, 1, -1).
        auto cage = Build(0x500000, CageStory);
        kernel::G<float>(cage.bones + 3 * Stride + Local + 4) = 1.f;
        kernel::G<float>(cage.bones + 3 * Stride + Local + 8) = -1.f;
        Rebuild(cage, 90, 1, UpdateReturn);
        Check(Near(LocalOf(cage, "Object01"), RestOffset(cage, "Object01")), "Ko-op Cage sunglasses stay collapsed");
    }
    {
        auto cage = Build(0x600000, CageStory);
        Rebuild(cage, 90, 2, UpdateReturn);
        Check(Unchanged(cage, "Object01"), "Versus Cage changed");
    }
    if (failures) return 1;
    std::puts("fighter proportions: all checks passed");
    return 0;
}
