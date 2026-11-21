#include "kernel/kernel.h"
#include "campaign_moves.h"
#include "campaign_moves_catalog.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

X86_EXTERN_FUNC(sub_000150C9);
X86_EXTERN_FUNC(sub_00013779);
X86_EXTERN_FUNC(sub_00013B9D);
X86_EXTERN_FUNC(sub_0001658C);
X86_EXTERN_FUNC(sub_00013A12);
X86_EXTERN_FUNC(sub_000131FC);
X86_EXTERN_FUNC(sub_000116CC);
X86_EXTERN_FUNC(sub_00011D16);
X86_EXTERN_FUNC(sub_00013954);
X86_EXTERN_FUNC(sub_000113C0);
X86_EXTERN_FUNC(sub_000111C2);
X86_EXTERN_FUNC(sub_00012FD1);

namespace {

constexpr uint32_t Database = 0x4EFA88, Player = 0x4EFA60, PurchaseNode = 0x4EFB38;
constexpr uint32_t Request = 0x59E38C, Secondary = 0x59E3A0, Delay = 0x59E3B8;
constexpr uint32_t LiveRequest = 0x59E3BC, Modal = 0x59E45C;
constexpr uint32_t State = 0x700000, Actor1 = 0x706000, Actor2 = 0x708000;
constexpr uint32_t Text = 0x710000, Event = 0x715000, PoolEnd = 0x770000;
constexpr uint32_t PoolBegin = 0x730000, Stack = 0x780000, Dialog = 0x790000;
constexpr uint32_t SourceType = 0x400, ReturnAddress = 0x9EE5F;
constexpr uint32_t OriginalSize = 0x20 + 4 * 4 + 4 * (0x34 + 0x30);
constexpr uint32_t ExtendedSize = OriginalSize + openshao::campaign_moves::ExtraNodeCount * 0x34;
constexpr std::array<uint32_t, 4> BaseHeroes{0, 1, 88, 82};
constexpr std::array<uint32_t, 4> ExtraHeroes{83, 84, 89, 90};
std::array<uint8_t, 8 << 20> memory{};
unsigned failures = 0;
bool survival = false, bad_relocation = false, nested_resolver = false;
uint32_t allocation_failure = 0, source = 0, high = PoolEnd, low = PoolBegin;
uint32_t source_size = OriginalSize;
uint32_t permanent_type = 0, permanent_space = 0;
uint32_t import_event = 0, import_size = 0, import_count = 0, original_frees = 0;
uint32_t extension_allocations = 0, native_previews = 0;
std::array<unsigned, 2> native_ranks{};
uint32_t measured_width = 0;
bool measure_by_lines = false;
std::array<uint32_t, 7> fitted_row{};

using kernel::R32;
using kernel::W32;

void Check(bool passed, const char* reason) {
    if (passed) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", reason);
}

uint32_t Round(uint32_t bytes) { return (bytes + 15) & ~15u; }

uint32_t NativeAllocate(uint32_t type, uint32_t bytes) {
    Check(type == permanent_type || type == SourceType, "allocator receives a native pool type");
    if (Round(bytes) > high - low) return 0;
    if (type == SourceType) return high -= Round(bytes);
    const auto address = low;
    low += Round(bytes);
    return address;
}

void NativeFree(uint32_t type, uint32_t bytes) {
    if (type == SourceType) {
        if (high == source) ++original_frees;
        Check((high == source && bytes == source_size) ||
              (high < source && high + Round(bytes) == source && bytes == ExtendedSize),
              "source release pops the top temporary block with its own size");
        high += Round(bytes);
    } else {
        low -= Round(bytes);
    }
}

X86Context Frame(uint32_t arg1 = 0, uint32_t arg2 = 0, uint32_t object = Database) {
    X86Context ctx{};
    ctx.ecx = object;
    ctx.esp = Stack;
    ctx.esi = 0x12345678;
    W32(Stack, ReturnAddress);
    W32(Stack + 4, arg1);
    W32(Stack + 8, arg2);
    return ctx;
}

void Heroes(uint32_t first, uint32_t second) {
    W32(State + 0xCC, first);
    W32(State + 0xD0, second);
    kernel::W16(Actor1 + 0x1DFE, static_cast<uint16_t>(first));
    kernel::W16(Actor2 + 0x1DFE, static_cast<uint16_t>(second));
}

uint32_t ActiveHero() { return kernel::R16((R32(Player) ? Actor2 : Actor1) + 0x1DFE); }

uint32_t NativeRoot(uint32_t fighter) {
    if (std::find(BaseHeroes.begin(), BaseHeroes.end(), fighter) == BaseHeroes.end()) fighter = 0;
    const auto block = R32(Database), roots = R32(block + 0x10);
    for (uint32_t i = 0; i < R32(block + 0xC); ++i) {
        const auto root = R32(roots + i * 4);
        if (R32(root + 0x18) == fighter) return root;
    }
    return 0;
}

uint32_t Child(uint32_t node, unsigned index = 0) { return R32(R32(node + 0x14) + index * 4); }

uint32_t Resolve(uint32_t fighter) {
    auto ctx = Frame(fighter);
    sub_00013B9D(ctx, memory.data());
    Check(ctx.esp == Stack + 8, "root resolver uses RET4");
    return ctx.eax;
}

void Reset() {
    memory.fill(0);
    measure_by_lines = false;
    survival = bad_relocation = nested_resolver = false;
    allocation_failure = import_count = original_frees = extension_allocations = native_previews = 0;
    high = PoolEnd;
    low = PoolBegin;
    native_ranks = {};
    permanent_type = permanent_space = 0;
    W32(0x5ACEBC, State);
    W32(State + 0x88, 1);
    W32(0x64D230, Actor1);
    W32(0x64D23C, Actor2);
    W32(0x62EC70, 16);
    W32(Modal, Dialog);
    Heroes(83, 0);
    W32(0x59E3E0, Text);
    W32(0x59E3E4, 1751 * 4);
    for (uint32_t i = 0; i < 1751; ++i) W32(Text + i * 4, 10000 + i);
}

void LoadOriginal(uint32_t padding = 0) {
    source_size = OriginalSize + padding;
    source = NativeAllocate(SourceType, source_size);
    std::fill_n(memory.data() + source, source_size, 0);
    std::fill_n(memory.data() + source + OriginalSize, padding, 0xA5);
    W32(Event, 0x2DD);
    W32(Event + 4, SourceType);
    W32(Event + 8, source);
    W32(Event + 0xC, 0x6A00);
    W32(Event + 0x10, source_size);
    W32(source, 0x4450264D);
    W32(source + 8, 8);
    W32(source + 0xC, 4);
    uint32_t node = source + 0x30;
    for (uint32_t i = 0; i < 4; ++i) {
        W32(source + 0x20 + i * 4, 0x10 + i);
        W32(node, 0x10 + i);
        W32(node + 4, 3);
        W32(node + 0xC, 1);
        W32(node + 0x18, BaseHeroes[i]);
        W32(node + 0x30, 0x20 + i);
        node += 0x34;
        W32(node, 0x20 + i);
        W32(node + 4, 2);
        W32(node + 0x10, 0x10 + i);
        W32(node + 0x18, 10461);
        node += 0x30;
    }
}

void Import() {
    const std::vector<uint8_t> data(memory.data() + source, memory.data() + source + source_size);
    std::array<uint8_t, 0x14> event{};
    std::copy_n(memory.data() + Event, event.size(), event.begin());
    auto ctx = Frame(Event);
    W32(Stack + 8, 0x1234ABCD);
    W32(Stack + 12, 0x5678DCBA);
    const auto before = import_count;
    sub_000150C9(ctx, memory.data());
    Check(import_count == before + 1, "adapter invokes native import once");
    Check(ctx.eax == 1 && ctx.esp == Stack + 8 && ctx.esi == 0x12345678,
          "adapter preserves native return, registers and RET4");
    Check(R32(Stack + 8) == 0x1234ABCD && R32(Stack + 12) == 0x5678DCBA,
          "live caller frame stays intact after native RET4");
    Check(std::equal(data.begin(), data.end(), memory.data() + source), "original source bytes stay intact");
    Check(std::equal(event.begin(), event.end(), memory.data() + Event), "original event bytes stay intact");
    Check(high == PoolEnd && original_frees == import_count, "original source released exactly once per import");
}

void Cleanup() {
    auto ctx = Frame();
    sub_00013779(ctx, memory.data());
    Check(low == PoolBegin && high == PoolEnd && ctx.esp == Stack + 4,
          "cleanup restores both pool ends and native stack");
}

void Menu(uint32_t player, uint32_t expected_root) {
    W32(Player, player);
    const auto old_root = R32(R32(Database) + 0x20);
    auto ctx = Frame();
    sub_0001658C(ctx, memory.data());
    Check(R32(Database + 0xB0) == Child(expected_root), "selected player opens their own tree");
    Check(ctx.esp == Stack + 4, "menu reset keeps the tail-call stack contract");
    ctx = Frame();
    sub_00013A12(ctx, memory.data());
    Check(ctx.esp == Stack + 4, "acquired update keeps native stack contract");
    Check(R32(R32(Database) + 0x20) == old_root && R32(old_root + 0x18) == 0,
          "menu scopes restore original Liu Kang slot and label");
    Check(R32(State + 0xCC + player * 4) == ActiveHero(), "menu leaves player identity unchanged");
}

}

namespace mem {
bool IsCommitted(uint32_t address) { return address >= 0x10000 && address < memory.size(); }
uint32_t AllocSystem(uint32_t bytes, uint32_t) {Check(bytes == 512, "wrapped detail uses one bounded guest scratch allocation");return 0x7C0000;}
}

namespace openshao::survival {
bool ActiveSession() { return ::survival; }
}

namespace kernel {
uint8_t* g_base = memory.data();
uint32_t CallGuest(X86Context& ctx, uint32_t fn, const uint32_t* args, size_t count) {
    for (size_t i = count; i > 0; --i) {
        ctx.esp -= 4;
        W32(ctx.esp, args[i - 1]);
    }
    ctx.esp -= 4;
    W32(ctx.esp, 0xFFFFFFF0);
    ctx.eax = ctx.ecx = ctx.esp = 0xBAD;
    switch (fn) {
    case 0x152F60:
        Check(count == 8 && (args[3] == Text || args[3] == 0x7C0000) && args[4] == std::bit_cast<uint32_t>(1.f) &&
              args[5] == args[4], "native measures the complete caption at full size");
        if (measure_by_lines) {
            uint32_t line = 0, widest = 0;
            for (unsigned i = 0; i < 512; ++i) {
                const char glyph = char(kernel::G<uint8_t>(args[3] + i));
                if (!glyph) break;
                if (glyph == '\n' || glyph == '\r') {widest = std::max(widest, line);line = 0;}
                else ++line;
            }
            W32(args[6], std::max(widest, line) * 10);
        } else W32(args[6], measured_width);
        W32(args[7], 20);
        return 0;
    case 0x152AE0:
        Check(count == fitted_row.size(), "the fitted row uses the native scaled draw");
        std::copy_n(args, fitted_row.size(), fitted_row.begin());
        return 0;
    case 0x102FF0:
        Check(count == 1, "native pool free-space call ABI");
        if (permanent_type && args[0] == permanent_type) return permanent_space;
        return high - low;
    case 0x1050A0:
        Check(count == 6 && args[0] == SourceType && args[1] == ExtendedSize && args[2] == 1,
              "temporary allocation uses original pool and expanded byte count");
        ++extension_allocations;
        return allocation_failure ? (allocation_failure == 1 ? 0 : UINT32_MAX) : NativeAllocate(args[0], args[1]);
    case 0x1031A0:
        Check(count == 5 && args[0] == SourceType && args[1] == source_size,
              "adapter releases original source through native free ABI");
        NativeFree(args[0], args[1]);
        return 0xBAD;
    default:
        std::abort();
    }
}
}

X86_FUNC(__imp__sub_000150C9) {
    ++import_count;
    import_event = R32(ctx.esp + 4);
    import_size = R32(import_event + 0x10);
    Check(R32(ctx.esp) == ReturnAddress && ctx.ecx == Database, "native import sees its original call site and object");
    Check(R32(import_event) == 0x2DD && R32(import_event + 0xC) == 0x6A00, "native import event preserves metadata");
    const auto block = NativeAllocate(permanent_type, import_size);
    std::copy_n(base + R32(import_event + 8), import_size, base + block);
    const auto roots = R32(block + 0xC), nodes = R32(block + 8);
    const auto index = NativeAllocate(permanent_type, nodes * 4);
    W32(Database, block);
    W32(Database + 4, index);
    W32(block + 0x10, block + 0x20);
    W32(block + 0x14, import_size);
    uint32_t cursor = block + 0x20 + roots * 4;
    for (uint32_t i = 0; i < nodes; ++i) {
        W32(index + i * 4, cursor);
        cursor += 0x30 + 4 * R32(cursor + 0xC);
    }
    const auto pointer_for = [&](uint32_t id) {
        for (uint32_t i = 0; i < nodes; ++i) {
            const auto node = R32(index + i * 4);
            if (R32(node) == id) return node;
        }
        return 0u;
    };
    for (uint32_t i = 0; i < roots; ++i) {
        const auto slot = block + 0x20 + i * 4;
        W32(slot, pointer_for(R32(slot)));
    }
    for (uint32_t i = 0; i < nodes; ++i) {
        const auto node = R32(index + i * 4), count = R32(node + 0xC);
        W32(node + 0x10, pointer_for(R32(node + 0x10)));
        if (count) W32(node + 0x14, node + 0x30);
        for (uint32_t k = 0; k < count; ++k) W32(node + 0x30 + k * 4, pointer_for(R32(node + 0x30 + k * 4)));
        if (R32(node + 4) != 3) {
            for (const auto offset : {0x18u, 0x1Cu})
                if (R32(node + offset) >= 10000 && R32(node + offset) < 11751)
                    W32(node + offset, R32(node + offset) - 10000);
        }
    }
    NativeFree(R32(import_event + 4), import_size);
    for (uint32_t player = 0; player <= (R32(State + 0x88) == 1 ? 1u : 0u); ++player) {
        auto nested = ctx;
        nested.esp -= 12;
        W32(nested.esp, player ? 0x9ECC0 : 0x9EC42);
        W32(nested.esp + 4, player);
        W32(nested.esp + 8, Child(NativeRoot(R32(State + 0xCC + player * 4))));
        sub_000131FC(nested, base);
        Check(nested.esp == ctx.esp, "rank initialization keeps RET8");
    }
    if (bad_relocation && roots == 8) W32(Child(R32(block + 0x30)) + 0x30, block + import_size);
    kernel::G<uint8_t>(Database + 8) = 1;
    kernel::Return(ctx, 1, 1);
}

X86_FUNC(__imp__sub_00013779) {
    const auto block = R32(Database);
    NativeFree(permanent_type, R32(block + 8) * 4);
    NativeFree(permanent_type, R32(block + 0x14));
    std::fill_n(base + block, R32(block + 0x14), 0);
    W32(Database, 0);
    W32(Database + 4, 0);
    kernel::G<uint8_t>(Database + 8) = 0;
    kernel::Return(ctx, 0, 0);
}

X86_FUNC(__imp__sub_00013B9D) {
    const auto fighter = R32(ctx.esp + 4), root = NativeRoot(fighter);
    if (nested_resolver) {
        nested_resolver = false;
        auto nested = ctx;
        nested.esp -= 16;
        W32(nested.esp + 4, 0);
        sub_00013B9D(nested, base);
        Check(R32(nested.eax + 0x18) == 0 && R32(nested.eax) == 0x10,
              "nested Liu Kang resolution temporarily uses its original root");
        nested = ctx;
        nested.esp -= 16;
        W32(nested.esp + 4, 1);
        sub_00013B9D(nested, base);
        Check(nested.eax && R32(nested.eax + 0x18) == 1,
              "nested Kung Lao resolution keeps its original root available");
        W32(nested.esp - 4, 84);
        nested.esp -= 8;
        sub_00013B9D(nested, base);
        Check(R32(nested.eax + 0x18) == 84, "nested extra resolution returns its own root");
        Check(NativeRoot(fighter) == root, "nested resolver restores outer native root view");
    }
    kernel::Return(ctx, root, 1);
}

X86_FUNC(__imp__sub_0001658C) {
    W32(ctx.ecx + 0xB0, Child(NativeRoot(ActiveHero())));
    sub_000113C0(ctx, base);
}

X86_FUNC(__imp__sub_00013A12) {
    (void)base;
    const auto root = NativeRoot(ActiveHero());
    if (R32(root) >= 0xFFF00000) {
        for (unsigned button = 0; button < 3; ++button) W32(Child(Child(Child(root), button)) + 0x2C, 0);
    }
    kernel::Return(ctx, 0, 0);
}

X86_FUNC(__imp__sub_000131FC) {
    (void)base;
    const auto player = R32(ctx.esp + 4), node = R32(ctx.esp + 8);
    ++native_ranks[player];
    const auto selected = R32(State + 0xCC + player * 4);
    if (R32(R32(Database) + 0xC) == 8 &&
        std::find(ExtraHeroes.begin(), ExtraHeroes.end(), selected) != ExtraHeroes.end()) {
        Check(R32(node + 4) == 0 && R32(node + 0x2C) == 1,
              "extra initialization applies only an acquired own base-special leaf");
        const auto button = R32(node + 0x10), section = R32(button + 0x10), root = R32(section + 0x10);
        Check(R32(section + 0x18) == 461 && R32(root + 0x18) == selected,
              "base-special initialization never borrows another fighter's tree");
        constexpr std::array<uint8_t, 3> skills{0, 2, 1};
        const auto button_label = R32(button + 0x18);
        if (button_label >= 466 && button_label <= 468) {
            auto& rank = kernel::G<uint8_t>(State + 0x1C4 + player * 28 + skills[button_label - 466]);
            if (!rank) rank = 1;
        }
    }
    kernel::Return(ctx, 0, 2);
}

X86_FUNC(__imp__sub_000111C2) {
    const auto player = R32(ctx.esp + 4), skill = R32(ctx.esp + 8);
    kernel::Return(ctx, memory[State + 0x1C4 + player * 28 + skill], 0);
}
X86_WEAK_FUNC(sub_000111C2, __imp__sub_000111C2)
X86_FUNC(__imp__sub_00012FD1) {
    const auto player = R32(ctx.esp + 4), flags = R32(ctx.esp + 8);
    uint32_t rank = 1;
    if (flags & 0x10000000) rank = memory[State + 0x1C4 + player * 28];
    else if (flags & 0x20000000) rank = memory[State + 0x1C5 + player * 28];
    else if (flags & 0x40000000) rank = memory[State + 0x1C6 + player * 28];
    else if (flags & 0x80000000) rank = std::max(0, memory[State + 0x1C7 + player * 28] - 1);
    kernel::Return(ctx, rank, 0);
}
X86_WEAK_FUNC(sub_00012FD1, __imp__sub_00012FD1)
X86_FUNC(__imp__sub_000116CC) { (void)base; kernel::Return(ctx, 1, 1); }
X86_FUNC(__imp__sub_00011D16) {
    (void)base;
    W32(State + 0x100, R32(State + 0x100) - 10);
    kernel::G<uint8_t>(State + 0x1C4)++;
    kernel::G<uint8_t>(Dialog + 0x70) = 1;
    kernel::Return(ctx, 1, 0);
}
X86_FUNC(__imp__sub_00013954) { __imp__sub_00011D16(ctx, base); }
X86_FUNC(__imp__sub_000113C0) {
    (void)base;
    ++native_previews;
    W32(Request, 1234);
    kernel::Return(ctx, 0, 0);
}

int main() {
    for (const auto hero : ExtraHeroes) {
        Reset(); Heroes(hero, 0);
        W32(State + 0x1C4, 0x01010101);
        W32(State + 0x1E0, 0x02020202);
        W32(0x4F3390, 0x05050505);
        std::fill_n(memory.begin() + 0x4F3390 + 13, 10, 1);
        const auto saved_ranks = std::vector<uint8_t>(memory.begin() + State + 0x1C4, memory.begin() + State + 0x1FC);
        for (uint32_t player = 0; player < 2; ++player) {
            for (uint32_t skill = 0; skill < 27; ++skill) {
                auto ctx = Frame(player, skill);
                sub_000111C2(ctx, memory.data());
                auto expected = uint32_t(memory[State + 0x1C4 + player * 28 + skill]);
                if (player == 0 && skill < 4) expected = 5;
                if (player == 0 && skill >= 13 && skill <= 22) expected = 1;
                Check(ctx.eax == expected && ctx.esp == Stack + 4,
                      "extra combat ranks and melee follow-ups match Versus while platform skills and the Story partner use saved progression");
            }
            for (uint32_t group = 0; group < 4; ++group) {
                auto ctx = Frame(player, 0x10000000u << group);
                sub_00012FD1(ctx, memory.data());
                Check(ctx.eax == (player ? (group == 3 ? 1u : 2u) : (group == 3 ? 4u : 5u)) && ctx.esp == Stack + 4,
                      "native transition tiers admit the full extra chains including the combo rank-minus-one convention");
            }
        }
        Check(std::equal(saved_ranks.begin(), saved_ranks.end(), memory.begin() + State + 0x1C4),
              "combat queries preserve both players' saved rank bytes");
        for (unsigned scope = 0; scope < 3; ++scope) {
            survival = scope == 0;
            W32(State + 0x88, scope == 1 ? 2 : 0);
            const uint32_t player = scope == 2 ? 1 : 0;
            auto ctx = Frame(player, 3);
            sub_000111C2(ctx, memory.data());
            Check(ctx.eax == (player ? 2u : 1u), "Survival, Versus and inactive players retain native rank lookup");
        }
    }

    {
        Reset(); Heroes(90, 83); LoadOriginal(); Import(); Menu(0, Resolve(90));
        std::string corrected;
        Check(openshao::campaign_moves::CorrectCaption(1291, 0, corrected) && corrected == "{ < < < *",
              "Cage Groin Bashing uses the owner-verified cag1_fat scene command");
        Check(openshao::campaign_moves::CorrectCaption(1292, 0, corrected) && corrected == "} } > > *",
              "Cage Torso Rip uses the owner-verified cag2_fat scene command");
        W32(Player, 1);
        Check(!openshao::campaign_moves::CorrectCaption(1291, 0, corrected), "other fighters do not inherit Cage commands");
        W32(Player, 0); survival = true;
        Check(!openshao::campaign_moves::CorrectCaption(1291, 0, corrected), "Cage caption correction is scoped to the campaign reference");
        survival = false; Cleanup();
    }
    {
        using namespace openshao::campaign_moves;
        Reset(); Heroes(84, 89); LoadOriginal(); Import();
        Menu(0, Resolve(84));
        const auto combos = Child(Resolve(84), 2), leaf = Child(combos);
        const auto label = R32(leaf + 0x18), desc = R32(leaf + 0x1C);
        std::string caption;
        Check(R32(combos + 0x18) == 463 && ReferenceNode(leaf), "relocated Reptile combo is a reference");
        Check(ReferenceCaption(label, caption) && caption == "Double Kick", "combo list shows its move name");
        Check(ReferenceCaption(desc, caption) && caption == "Double Kick\n*, *, *, |", "combo opens its input sequence");
        const auto power_slam = Child(Child(Child(Resolve(84)), 3), 3);
        W32(Database + 0xB0, power_slam);
        constexpr uint32_t native_caption = 0x7A0000;
        constexpr char wrong_command[] = "Power Slam\n$ + ^ then ~";
        std::copy_n(wrong_command, sizeof(wrong_command), memory.begin() + native_caption);
        Check(CorrectCaption(903, native_caption, caption) && caption == "Power Slam\n$ + ^ then |",
              "Reptile's Power Slam uses the Power follow-up rather than the repeated Launch input");
        W32(leaf + 0x2C, 0);
        Menu(0, Resolve(84));
        Check(R32(leaf + 0x2C) == 1, "native rank refresh does not lock a reference combo");
        auto ctx = Frame(leaf);
        sub_000116CC(ctx, memory.data());
        Check(ctx.eax == 0, "a reference combo cannot be purchased");
        Menu(1, Resolve(89));
        const auto fan = Child(Child(Resolve(89), 2));
        Check(R32(fan + 0x1C) == 1012, "a matching combo description stays in the native locale");
        const auto specials = Child(Resolve(89));
        const auto air_fan = Child(Child(specials, 2), 1);
        Check(ReferenceNode(air_fan) && ReferenceCaption(R32(air_fan + 0x1C), caption) &&
              caption == "Air Fan Toss\n=, $ + |", "the airborne special has its own reference command");
        const auto backflip = Child(Child(Resolve(89), 2), 2);
        Check(ReferenceCaption(R32(backflip + 0x18), caption) && caption == "Quick Draw Double Backflip",
              "the full guide name remains available for the fitted list");
        Check(ReferenceCaption(R32(backflip + 0x1C), caption) &&
              caption == "Quick Draw Double Backflip\n*, *, *, ~", "the detail keeps the full move name");
        Check(ReferenceTitleSource(R32(fan + 0x18)) == 1012 &&
              !ReferenceTranslationKey(R32(backflip + 0x18)).empty(),
              "the menu reuses native text first and has a localized fallback for a guide-only move");
        const auto throws = Child(specials, 3);
        const auto head_smack = Child(throws, 1), flip_up = Child(throws, 2);
        Check(ReferenceTitleSource(R32(head_smack + 0x18)) == 876 &&
              ReferenceTitleSource(R32(head_smack + 0x1C)) == 876,
              "the shared Head Smack name reuses native translations in both list and details");
        Check(ReferenceTitleSource(R32(flip_up + 0x18)) == 877 &&
              ReferenceTitleSource(R32(flip_up + 0x1C)) == 877,
              "the shared Flip Up name reuses native translations in both list and details");
        constexpr char localized_title[] = "Kombo: Rota\xE7\xE3o Fraca.\r\nCommand";
        std::copy_n(localized_title, sizeof(localized_title), memory.begin() + Text);
        Check(LocalizedReferenceTitle(Text, caption) && caption == "Rota\xE7\xE3o Fraca",
              "the localized row preserves the native accented glyphs");

        ctx = Frame(105, 180);
        ctx.ebp = Database;
        W32(Stack, 0x9CC55);
        W32(Stack + 12, 0x809F9FF0);
        W32(Stack + 16, 1);
        W32(Stack + 20, Text);
        W32(Database + 0xC, 62);
        W32(Database + 0x14, 315);
        W32(Database + 0x3C, 10);
        measured_width = 200;
        Check(!FitReferenceList(ctx) && ctx.esp == Stack, "a row within the margin retains its native draw");
        measured_width = 250;
        Check(FitReferenceList(ctx) && ctx.esp == Stack + 4, "an overflowing row takes the scaled draw");
        Check(fitted_row == std::array<uint32_t, 7>{105, 180, 0x809F9FF0, 1, Text,
              std::bit_cast<uint32_t>(.8f), std::bit_cast<uint32_t>(.8f)},
              "the scaled row keeps its position and fits the column with a margin");

        ctx = Frame(320, 350);
        W32(Stack, 0x9D027);
        W32(Stack + 24, std::bit_cast<uint32_t>(1.f));
        W32(Stack + 28, std::bit_cast<uint32_t>(.8f));
        measured_width = 1004;
        FitReferenceDetails(ctx, memory.data(), [](X86Context& draw, uint8_t*) {
            Check(R32(draw.esp + 4) == 320 && R32(draw.esp + 24) == std::bit_cast<uint32_t>(.5f) &&
                  R32(draw.esp + 28) == std::bit_cast<uint32_t>(.4f),
                  "the overflowing description fits proportionally around its original center");
            kernel::Return(draw, 0, 0);
        });
        Check(ctx.esp == Stack + 4 && R32(Stack + 24) == std::bit_cast<uint32_t>(1.f) &&
              R32(Stack + 28) == std::bit_cast<uint32_t>(.8f), "the description restores its caller arguments");
        Check(!ReferenceCaption(label, caption), "other players do not inherit custom combo labels");
        Cleanup();
    }
    {
        Reset();Heroes(89,83);LoadOriginal();Import();Menu(0,Resolve(89));
        measure_by_lines = true;
        const auto write = [](const std::string& title) {
            std::fill_n(memory.data() + Text, 512, 0);
            std::copy(title.begin(), title.end(), memory.begin() + Text);
        };
        const auto row = [&] {
            auto call = Frame(105,180);
            call.ebp = Database;
            W32(Stack,0x9CC55);W32(Stack+12,0x809F9FF0);
            W32(Stack+16,1);W32(Stack+20,Text);
            W32(Database+0x14,315);W32(Database+0x3C,10);
            return call;
        };
        write("Short Kick");
        auto ctx = row();
        Check(!openshao::campaign_moves::FitReferenceList(ctx) && std::strcmp(reinterpret_cast<const char*>(memory.data()+Text),"Short Kick")==0,
              "short list items keep their original single line and font scale");
        write("Quick Draw Double Backflip");
        ctx = row();
        Check(!openshao::campaign_moves::FitReferenceList(ctx) && std::strcmp(reinterpret_cast<const char*>(memory.data()+Text),
              "Quick Draw\nDouble Backflip")==0,
              "a long list title wraps at a word boundary before reducing its font");
        write("IncrediblyLongCharacterName Final");
        ctx = row();
        Check(openshao::campaign_moves::FitReferenceList(ctx) && std::strcmp(reinterpret_cast<const char*>(memory.data()+Text),
              "IncrediblyLongCharacterName\nFinal")==0 &&
              std::bit_cast<float>(fitted_row[5]) < 1.f && std::bit_cast<float>(fitted_row[5]) > .5f,
              "when the wrapped title remains too wide the scaled native draw preserves the newline");
        write("Supercalifragilisticexpialidocious");
        ctx = row();
        Check(openshao::campaign_moves::FitReferenceList(ctx) && std::strchr(reinterpret_cast<const char*>(memory.data()+Text),'\n')==nullptr,
              "one unbreakable word retains the safe proportional shrink fallback");
        const std::string max_label="Move "+std::string(58,'X');
        write(max_label);memory[Text+64]=0xA5;
        ctx=row();
        openshao::campaign_moves::FitReferenceList(ctx);
        Check(memory[Text+64]==0xA5 && std::strlen(reinterpret_cast<const char*>(memory.data()+Text))==63,
              "word wrapping preserves the guest's 64-byte label copy and its terminating NUL");
        const std::string accented="Rota\xE7\xE3o Muito Longa para Tr\xE1s";
        write(accented);ctx=row();
        openshao::campaign_moves::FitReferenceList(ctx);
        std::string unwrapped(reinterpret_cast<const char*>(memory.data()+Text));
        std::replace(unwrapped.begin(),unwrapped.end(),'\n',' ');
        Check(unwrapped==accented,
              "word wrapping keeps every native-language accent byte and never cuts a multibyte sequence");

        const std::string long_name="Quick Draw Double Backflip Mega Rapid Advanced Counter";
        write(long_name);
        W32(Database+0x44,72);W32(Database+0x48,0);
        ctx=Frame(320,350);W32(Stack,0x9D027);W32(Stack+20,Text);
        W32(Stack+24,std::bit_cast<uint32_t>(1.f));W32(Stack+28,std::bit_cast<uint32_t>(.8f));
        openshao::campaign_moves::FitReferenceDetails(ctx,memory.data(),[](X86Context& draw,uint8_t*) {
            const auto wrapped=R32(draw.esp+20);
            Check(wrapped != Text && wrapped != 0 && std::strchr(reinterpret_cast<const char*>(memory.data()+wrapped),'\n'),
                  "the detail name wraps in a temporary guest buffer after the native command was separated");
            Check(R32(draw.esp+24)==std::bit_cast<uint32_t>(1.f),
                  "the wrapped detail can retain its original font size when it fits horizontally and vertically");
            kernel::Return(draw,0,0);
        });
        Check(R32(Stack+20)==Text && R32(Stack+24)==std::bit_cast<uint32_t>(1.f) &&
              R32(Stack+28)==std::bit_cast<uint32_t>(.8f) &&
              std::strcmp(reinterpret_cast<const char*>(memory.data()+Text),long_name.c_str())==0,
              "the detail restores both scales and the original translated title pointer and bytes");

        W32(Database+0x44,0);W32(Database+0x48,30);
        ctx=Frame(320,350);W32(Stack,0x9D027);W32(Stack+20,Text);
        W32(Stack+24,std::bit_cast<uint32_t>(1.f));W32(Stack+28,std::bit_cast<uint32_t>(.8f));
        openshao::campaign_moves::FitReferenceDetails(ctx,memory.data(),[](X86Context& draw,uint8_t*) {
            Check(R32(draw.esp+20)!=Text && std::strchr(reinterpret_cast<const char*>(memory.data()+R32(draw.esp+20)),'\n'),
                  "a detail title below its command can wrap while respecting the bottom panel border");
            kernel::Return(draw,0,0);
        });
        Check(R32(Stack+20)==Text,"the below-command detail restores the original text pointer");

        W32(Database+0x44,12);
        W32(Database+0x48,0);
        ctx=Frame(320,350);W32(Stack,0x9D027);W32(Stack+20,Text);
        W32(Stack+24,std::bit_cast<uint32_t>(1.f));W32(Stack+28,std::bit_cast<uint32_t>(.8f));
        openshao::campaign_moves::FitReferenceDetails(ctx,memory.data(),[](X86Context& draw,uint8_t*) {
            Check(R32(draw.esp+20)==Text && std::bit_cast<float>(R32(draw.esp+24))<1.f,
                  "insufficient vertical space keeps the safer single-line scaled detail");
            kernel::Return(draw,0,0);
        });
        Check(R32(Stack+20)==Text && std::strcmp(reinterpret_cast<const char*>(memory.data()+Text),long_name.c_str())==0,
              "vertical fallback never modifies the cached original description");
        ctx=Frame(320,350);W32(Stack,0x9CFFA);W32(Stack+20,Text);
        W32(Stack+24,std::bit_cast<uint32_t>(1.f));W32(Stack+28,std::bit_cast<uint32_t>(.8f));
        openshao::campaign_moves::FitReferenceDetails(ctx,memory.data(),[](X86Context& draw,uint8_t*) {
            Check(R32(draw.esp+20)==Text,"the native button-command line is never split into multiple lines");
            kernel::Return(draw,0,0);
        });
        Cleanup();
    }
    {
        Reset(); Heroes(89, 83); LoadOriginal(); Import();
        Menu(0, Resolve(89));
        constexpr uint32_t caption = 0x7A0000;
        const auto write = [&](const std::string& text) {
            std::copy(text.begin(), text.end(), memory.begin() + caption);
            memory[caption + text.size()] = 0;
        };
        std::string corrected;
        write("Katana description $ + ~");
        Check(openshao::campaign_moves::CorrectCaption(610, caption, corrected) &&
              corrected == "Katana description $ + *", "English Air Jump retains its native description and corrects Y to X");
        write("Descri\xE7\xE3o completa $ + |.");
        Check(openshao::campaign_moves::CorrectCaption(610, caption, corrected) &&
              corrected == "Descri\xE7\xE3o completa $ + *.", "Portuguese Air Jump retains accents and corrects B to X after a language change");
        Check(!openshao::campaign_moves::CorrectCaption(710, caption, corrected), "other descriptions stay native");
        write("Ambiguous $+| and $+~");
        Check(!openshao::campaign_moves::CorrectCaption(610, caption, corrected), "ambiguous command sequences stay native");
        write("Description $+|");
        W32(Player, 1);
        Check(!openshao::campaign_moves::CorrectCaption(610, caption, corrected), "another active fighter stays native");
        W32(Player, 0); survival = true;
        Check(!openshao::campaign_moves::CorrectCaption(610, caption, corrected), "Survival stays native");
        survival = false;
        W32(Database + 0xB0, 0);
        Check(!openshao::campaign_moves::CorrectCaption(610, caption, corrected), "outside the extra tree stays native");
        Cleanup();
    }

    for (unsigned fixture = 1; fixture <= 3; ++fixture) {
        Reset();
        const bool pending_actor = fixture & 1;
        const bool padded = fixture & 2;
        W32(State + 0x88, 0);
        const uint32_t initial_ranks = pending_actor ? 0x01010101 : 0x01020300;
        W32(State + 0x1C4, initial_ranks);
        if (pending_actor) kernel::W16(Actor1 + 0x1DFE, 0);
        if (padded) W32(0x59E3E4, 8192);
        LoadOriginal(padded ? 1004 : 0);
        Import();
        Check(import_event != Event && import_size == ExtendedSize,
              "extend selected fighter's database before actor initialization and with disc padding");
        Check(native_ranks[0] == 0 && native_ranks[1] == 0 && R32(State + 0x1C4) == initial_ranks,
              "import preserves native startup and loaded ranks without borrowing an initializer");
        Check(kernel::R16(Actor1 + 0x1DFE) == (pending_actor ? 0 : 83),
              "import leaves the actor identity alone");
        kernel::W16(Actor1 + 0x1DFE, 83);
        Check(R32(Resolve(83) + 0x18) == 83, "relocated database exposes the selected fighter");
        Cleanup();
    }

    Reset();
    W32(State + 0x88, 0);
    Heroes(0, 83);
    LoadOriginal();
    Import();
    Check(import_event == Event && native_ranks[0] == 1 && native_ranks[1] == 0,
          "inactive solo slot cannot trigger a campaign database extension");
    Cleanup();

    Reset();
    for (unsigned cycle = 0; cycle < 20; ++cycle) {
        LoadOriginal();
        Import();
        Check(import_event != Event && import_size == ExtendedSize, "native importer receives an isolated extended event");
        Check(native_ranks[0] == 0 && native_ranks[1] == cycle + 1,
              "augmented import suppresses borrowed ranks and preserves original-player initialization");
        const auto block = R32(Database);
        const auto base_root = R32(block + 0x20);
        Check(R32(block + 0xC) == 8, "native relocated database exposes eight roots");
        if (R32(block + 0xC) != 8) { Cleanup(); break; }
        std::array<uint32_t, 4> extras{};
        nested_resolver = true;
        for (uint32_t i = 0; i < ExtraHeroes.size(); ++i) {
            const auto root = R32(block + 0x30 + i * 4);
            extras[i] = root;
            Check(Resolve(ExtraHeroes[i]) == root && R32(root + 0x18) == ExtraHeroes[i],
                  "explicit resolver uses real relocated extra identities and restores their labels");
        }
        Check(Resolve(0) == base_root, "base root is unchanged after resolving extras");
        Menu(0, extras[0]);
        Menu(1, base_root);
        Heroes(83, 84);
        Menu(1, extras[1]);
        Menu(0, extras[0]);
        Heroes(83, 0);
        const auto leaf = Child(Child(Child(extras[0])));
        Check(R32(leaf + 0x2C) == 1, "acquired update keeps the reference leaf acquired");
        W32(State + 0x100, 100);
        W32(State + 0x1C4, 0x06050403);
        W32(PurchaseNode, leaf);
        auto ctx = Frame(leaf);
        sub_000116CC(ctx, memory.data());
        Check((ctx.eax & 0xFF) == 0 && ctx.esp == Stack + 8, "reference leaf purchase check returns false with RET4");
        for (auto purchase : {sub_00011D16, sub_00013954}) {
            ctx = Frame(0x710000);
            kernel::G<uint8_t>(Dialog + 0x70) = 1;
            purchase(ctx, memory.data());
            Check(ctx.eax == 0 && ctx.esp == Stack + 4 && !kernel::G<uint8_t>(Dialog + 0x70),
                  "reference purchase follows native failure ABI and modal state");
        }
        Check(R32(State + 0x100) == 100 && R32(State + 0x1C4) == 0x06050403, "references preserve XP and ranks");
        W32(PurchaseNode, Child(base_root));
        for (auto purchase : {sub_00011D16, sub_00013954}) {
            ctx = Frame(0x710000);
            purchase(ctx, memory.data());
            Check(ctx.eax == 1 && ctx.esp == Stack + 4, "ordinary purchases retain native callback ABI");
        }
        Check(R32(State + 0x100) == 80 && R32(State + 0x1C4) == 0x06050405,
              "ordinary nodes retain native XP debit and rank mutation");
        W32(Database + 0xB0, leaf);
        W32(Request, 1234);
        W32(Secondary, 42);
        W32(LiveRequest, 1234);
        kernel::W16(Delay, 30);
        const auto previews = native_previews;
        ctx = Frame();
        sub_000113C0(ctx, memory.data());
        Check(native_previews == previews && !R32(Request) && R32(Secondary) == UINT32_MAX && !kernel::R16(Delay),
              "unsupported preview schedules native teardown without a replacement");
        Check(R32(LiveRequest) == 1234, "native updater retains the live preview handle for teardown");
        survival = true;
        Check(Resolve(83) == base_root, "a cached campaign database uses native roots in Survival");
        ctx = Frame(leaf);
        sub_000116CC(ctx, memory.data());
        Check(ctx.eax == 1, "reference purchase predicate remains native in Survival");
        ctx = Frame();
        sub_000113C0(ctx, memory.data());
        Check(native_previews == previews + 1 && R32(Request) == 1234, "preview selection remains native in Survival");
        survival = false;
        W32(State + 0x88, 2);
        Check(Resolve(83) == base_root, "a cached campaign database uses native roots in Versus");
        W32(State + 0x88, 1);
        Cleanup();
        ctx = Frame(leaf);
        sub_000116CC(ctx, memory.data());
        Check(ctx.eax == 1, "cleanup drops reference identities before guest addresses are reused");
    }

    for (uint32_t hero : ExtraHeroes) {
        Reset();
        Heroes(hero, hero == 83 ? 84 : 83);
        W32(State + 0x1C4, 0x01010101);
        W32(State + 0x1E0, 0x01020300);
        LoadOriginal();
        Import();
        Check(native_ranks[0] == 0 && native_ranks[1] == 0 && R32(State + 0x1C4) == 0x01010101 &&
              R32(State + 0x1E0) == 0x01020300,
              "both extra players retain all four initial ranks");
        Menu(0, Resolve(hero));
        Menu(1, Resolve(hero == 83 ? 84 : 83));
        for (unsigned player = 0; player < 2; ++player) {
            W32(Player, player);
            const auto root = Resolve(ActiveHero());
            Check(R32(root + 0xC) == 5, "both extra players have the Platform reference section");
            if (R32(root + 0xC) != 5) continue;
            const auto platform = Child(root, 4);
            Check(R32(platform + 0x18) == 462 && R32(platform + 0xC) == 7,
                  "the relocated Platform section exposes all seven abilities");
            for (unsigned skill = 0; skill < 7; ++skill) {
                const auto leaf = Child(platform, skill);
                Check(openshao::campaign_moves::ReferenceNode(leaf) &&
                      R32(leaf + 0x18) == 1396 + skill && R32(leaf + 0x1C) == 1403 + skill,
                      "platform abilities use original localized text and remain consultation leaves");
            }
        }
        Check(R32(State + 0x1C4) == 0x01010101 && R32(State + 0x1E0) == 0x01020300,
              "opening platform references does not grant either player's campaign skills");
        Cleanup();
    }

    Reset();
    permanent_type = 2;
    permanent_space = 0x400;
    W32(0x4A8660, permanent_type);
    W32(0x62EC70 + permanent_type * 0x68, 128);
    LoadOriginal();
    Import();
    Check(import_event == Event && extension_allocations == 0,
          "capacity check uses the native permanent pool before allocating the replacement");
    Cleanup();

    for (unsigned scenario = 0; scenario < 8; ++scenario) {
        Reset();
        LoadOriginal();
        if (scenario == 0) W32(0x59E3E4, 1751 * 4 - 1);
        if (scenario == 1) allocation_failure = 1;
        if (scenario == 2) allocation_failure = 0; // Native -1 failure is tested below with a constrained pool.
        if (scenario == 3) survival = true;
        if (scenario == 4) W32(State + 0x88, 2);
        if (scenario == 5) Heroes(88, 82);
        if (scenario == 6) low = high - 0x400;
        if (scenario == 7) allocation_failure = 2;
        if (scenario == 2) W32(source, 0);
        const auto initial_low = low;
        Import();
        Check(import_event == Event && import_size == OriginalSize && native_ranks[0] == 1,
              "unsupported modes and failed preparation delegate the untouched original import and ranks");
        auto ctx = Frame();
        sub_00013779(ctx, memory.data());
        Check(low == initial_low, "fallback cleanup restores the original permanent pool cursor");
    }

    Reset();
    LoadOriginal();
    W32(source + 0x20, 0x11);
    W32(source + 0x24, 0x10);
    Import();
    nested_resolver = true;
    Resolve(83);
    const auto block = R32(Database), root_table = R32(block + 0x10);
    const auto original_lk_slot = root_table + 4, original_lk_root = R32(original_lk_slot);
    W32(original_lk_slot, 0x7A0000);
    Check(Resolve(83) == R32(original_lk_slot), "invalid original root slot prevents a temporary root view");
    W32(original_lk_slot, original_lk_root);
    Cleanup();

    Reset();
    LoadOriginal();
    bad_relocation = true;
    Import();
    Check(Resolve(83) == R32(R32(Database) + 0x20), "out-of-block relocated children prevent activation of extra roots");
    Cleanup();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
