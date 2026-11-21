#include "campaign_moves_catalog.h"
#include "campaign_moves_database.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <unordered_map>

using namespace openshao::campaign_moves;

namespace {

constexpr size_t HeaderSize = 0x20;
constexpr size_t NodeSize = 0x30;
constexpr size_t FirstNode = HeaderSize + 4 * 4;
constexpr uint32_t Absent = 0xFFFFFFFFu;
int failures = 0;

void Check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

void Write(std::span<uint8_t> bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}

uint32_t Read(std::span<const uint8_t> bytes, size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        return Absent;
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i)
        value |= uint32_t{bytes[offset + i]} << (8 * i);
    return value;
}

std::vector<uint8_t> Fixture()
{
    std::vector<uint8_t> raw(FirstNode + 4 * NodeSize);
    const std::array<uint32_t, 8> header{
        0x4450264Du, 0xFE0C1713u, 4, 4, Absent, 0x98800C00u, 47, 88,
    };
    for (size_t i = 0; i < header.size(); ++i)
        Write(raw, 4 * i, header[i]);

    constexpr std::array<uint32_t, 4> fighters{0, 1, 88, 82};
    for (size_t i = 0; i < fighters.size(); ++i) {
        const uint32_t id = 0x2100u + static_cast<uint32_t>(i);
        Write(raw, HeaderSize + 4 * i, id);
        const size_t start = FirstNode + i * NodeSize;
        const std::array<uint32_t, 12> fields{
            id, 3, 0, 0, 0, 0x12000108u, fighters[i],
            Absent, Absent, Absent, Absent, Absent,
        };
        for (size_t field = 0; field < fields.size(); ++field)
            Write(raw, start + 4 * field, fields[field]);
    }
    return raw;
}

void CheckRejected(const std::vector<uint8_t>& raw, std::span<const uint32_t> crcs,
                   const char* message)
{
    const auto original = raw;
    ExtendedDatabase result{{1, 2, 3}, {4, 5, 6, 7}};
    std::string error;
    Check(!ExtendDatabase(raw, crcs, result, error), message);
    Check(result.bytes.empty() && result.extra_root_ids == std::array<uint32_t, 4>{}
              && !error.empty(),
          "failure clears previous output and supplies an error");
    Check(raw == original, "failure preserves input bytes");
}

void TestMalformed(std::span<const uint32_t> crcs)
{
    CheckRejected(std::vector<uint8_t>(8), crcs, "reject truncated header");

    auto raw = Fixture();
    raw[0] = 'X';
    CheckRejected(raw, crcs, "reject malformed signature");

    raw = Fixture();
    raw.resize(FirstNode - 1);
    CheckRejected(raw, crcs, "reject truncated root IDs");

    raw = Fixture();
    raw.pop_back();
    CheckRejected(raw, crcs, "reject truncated fixed node");

    raw = Fixture();
    Write(raw, FirstNode + 3 * NodeSize + 0xC, 1);
    CheckRejected(raw, crcs, "reject truncated child IDs");

    raw = Fixture();
    Write(raw, FirstNode + 0xC, Absent);
    CheckRejected(raw, crcs, "reject oversized child count");

    raw = Fixture();
    Write(raw, FirstNode + NodeSize, 0x2100);
    CheckRejected(raw, crcs, "reject duplicate node ID");

    raw = Fixture();
    Write(raw, HeaderSize + 4, 0x2100);
    CheckRejected(raw, crcs, "reject duplicate root ID");

    raw = Fixture();
    Write(raw, FirstNode + NodeSize + 0x18, 0);
    CheckRejected(raw, crcs, "reject duplicate fighter identity");

    raw = Fixture();
    Write(raw, FirstNode + 2 * NodeSize + 0x18, 100);
    CheckRejected(raw, crcs, "reject database missing a required campaign fighter root");

    raw = Fixture();
    Write(raw, HeaderSize + 4, 0x99);
    CheckRejected(raw, crcs, "reject missing root reference");

    raw = Fixture();
    Write(raw, FirstNode, 0xFFF00000);
    Write(raw, HeaderSize, 0xFFF00000);
    CheckRejected(raw, crcs, "reject generated ID collision");

    raw = Fixture();
    Write(raw, FirstNode + 3 * NodeSize + 0xC, 1);
    raw.resize(raw.size() + 4);
    Write(raw, raw.size() - 4, 0x99);
    CheckRejected(raw, crcs, "reject missing child reference");

    raw = Fixture();
    Write(raw, 8, 5);
    Write(raw, FirstNode + 3 * NodeSize + 0xC, 1);
    const size_t child = raw.size() + 4;
    raw.resize(child + NodeSize);
    Write(raw, child - 4, 0x22);
    Write(raw, child, 0x22);
    Write(raw, child + 0x10, 0x99);
    CheckRejected(raw, crcs, "reject missing parent reference");

    Write(raw, child + 0x10, 0x2102);
    CheckRejected(raw, crcs, "reject child attached to a different parent");

    Write(raw, child + 0x10, 0x2103);
    Write(raw, child + 0xC, 1);
    raw.resize(raw.size() + 4);
    Write(raw, raw.size() - 4, 0x22);
    CheckRejected(raw, crcs, "reject cyclic child reference");

    raw = Fixture();
    Write(raw, 8, 4097);
    CheckRejected(raw, crcs, "reject oversized node count");

    raw = Fixture();
    Write(raw, 0xC, 33);
    CheckRejected(raw, crcs, "reject oversized root count");

    raw = Fixture();
    raw.resize(1024 * 1024 + 1);
    CheckRejected(raw, crcs, "reject oversized input");

    CheckRejected(Fixture(), crcs.first(1750), "reject insufficient CRC table");

    std::vector<uint32_t> ambiguous(crcs.begin(), crcs.end());
    ambiguous[795] = ambiguous[10];
    CheckRejected(Fixture(), ambiguous, "reject CRC that resolves to a different text");

    ambiguous[795] = Absent;
    CheckRejected(Fixture(), ambiguous, "reject CRC treated as absent by native import");
}

void TestExtension(const std::vector<uint8_t>& raw, std::span<const uint32_t> crcs,
                   size_t original_nodes)
{
    const auto original = raw;
    ExtendedDatabase result;
    std::string error = "stale error";
    if (!ExtendDatabase(raw, crcs, result, error)) {
        Check(false, "extend a valid move database");
        return;
    }
    Check(error.empty(), "success clears previous errors");
    Check(raw == original, "success preserves input bytes");

    const std::span<const uint8_t> bytes = result.bytes;
    Check(Read(bytes, 8) == original_nodes + ExtraNodeCount && Read(bytes, 0xC) == 8,
          "extension adds four roots and their reference nodes");
    for (size_t offset : {0u, 4u, 0x10u, 0x14u, 0x18u, 0x1Cu})
        Check(Read(bytes, offset) == Read(raw, offset), "opaque header fields survive");
    constexpr size_t ExtendedFirstNode = HeaderSize + 8 * 4;
    Check(bytes.size() >= raw.size() + 16, "extended node data fits");
    if (bytes.size() < raw.size() + 16)
        return;
    Check(std::ranges::equal(std::span(raw).subspan(HeaderSize, 16),
                             bytes.subspan(HeaderSize, 16)),
          "original root IDs survive in order");
    Check(std::ranges::equal(std::span(raw).subspan(FirstNode),
                             bytes.subspan(ExtendedFirstNode, raw.size() - FirstNode)),
          "original node fields and child IDs survive byte-for-byte");

    std::unordered_map<uint32_t, size_t> nodes;
    size_t offset = ExtendedFirstNode;
    for (size_t i = 0; i < original_nodes + ExtraNodeCount; ++i) {
        Check(nodes.emplace(Read(bytes, offset), offset).second, "node IDs are unique");
        offset += NodeSize + 4 * Read(bytes, offset + 0xC);
    }
    Check(offset == bytes.size(), "every byte belongs to the known layout");
    const auto findNode = [&nodes](uint32_t id) {
        const auto found = nodes.find(id);
        return found == nodes.end() ? size_t{0} : found->second;
    };
    const auto childOf = [&](size_t parent, size_t index) {
        return findNode(Read(bytes, parent + NodeSize + 4 * index));
    };

    constexpr std::array<uint32_t, 4> fighters{83, 84, 89, 90};
    constexpr std::array<std::array<size_t, 3>, 4> labels{{
        {545, 645, 745}, {550, 650, 750}, {560, 660, 760}, {565, 665, 765},
    }};
    constexpr std::array<std::array<size_t, 3>, 4> descriptions{{
        {595, 695, 795}, {600, 700, 800}, {610, 710, 810}, {615, 715, 815},
    }};
    constexpr std::array<size_t, 3> buttons{466, 467, 468};
    Check(SpecialSkills == std::array<uint8_t, 3>{0, 2, 1},
          "quick, launch and power use their native skill order");
    for (size_t i = 0; i < fighters.size(); ++i) {
        const uint32_t root_id = result.extra_root_ids[i];
        Check((root_id & 0xFFF00000u) == 0xFFF00000u, "extra root uses reserved ID namespace");
        Check(Read(bytes, HeaderSize + 4 * (4 + i)) == root_id, "extra root ID is published");
        const size_t root = findNode(root_id);
        Check(root != 0 && Read(bytes, root + 4) == 3 && Read(bytes, root + 0xC) == 5
                  && Read(bytes, root + 0x10) == 0 && Read(bytes, root + 0x18) == fighters[i],
              "fighter root has its reference categories and no parent");
        const auto platform = childOf(root, 4);
        Check(Read(bytes, platform + 0x18) == crcs[462] &&
              Read(bytes, platform + 0x1C) == crcs[512] &&
              Read(bytes, platform + 4) == 1 && Read(bytes, platform + 0xC) == 7,
              "each extra has the original seven Platform abilities");
        if (Read(bytes, platform + 0xC) == 7) {
            for (size_t skill = 0; skill < 7; ++skill) {
                const auto leaf = childOf(platform, skill);
                Check(Read(bytes, leaf + 0x18) == crcs[1396 + skill] &&
                      Read(bytes, leaf + 0x1C) == crcs[1403 + skill] &&
                      Read(bytes, leaf + 0x10) == Read(bytes, platform) &&
                      Read(bytes, leaf + 0x20) == Absent &&
                      Read(bytes, leaf + 0x24) == Absent &&
                      Read(bytes, leaf + 0x28) == 0 && Read(bytes, leaf + 0x2C) == 1,
                      "platform references retain localized names and instructions without grants or purchases");
            }
        }
        const auto combos = childOf(root, 2);
        constexpr std::array combo_counts{1u, 4u, 3u, 5u};
        Check(Read(bytes, combos + 0x18) == crcs[463] && Read(bytes, combos + 4) == 1 &&
              Read(bytes, combos + 0xC) == combo_counts[i],
              "each fighter has its own nonempty Combos reference section");
        for (size_t row = 0; row < combo_counts[i]; ++row) {
            const auto leaf = childOf(combos, row);
            Check(leaf != 0 && Read(bytes, leaf + 0x10) == Read(bytes, combos) &&
                  Read(bytes, leaf + 0x28) == 0 && Read(bytes, leaf + 0x2C) == 1 &&
                  Read(bytes, leaf + 0x20) == Absent && Read(bytes, leaf + 0x24) == Absent,
                  "combo leaves are linked, readable references without XP or previews");
        }
        const auto basic = childOf(root, 1), fatalities = childOf(root, 3);
        Check(Read(bytes, basic + 0x18) == crcs[402] && Read(bytes, basic + 0xC) == 3,
              "basic moves show ground, air and running groups");
        Check(Read(bytes, fatalities + 0x18) == crcs[400] && Read(bytes, fatalities + 0xC) >= 2,
              "the original named finishers have a reference section");
        if (i == 0) {
            Check(Read(bytes, fatalities + 0xC) == 2 && Read(bytes, childOf(fatalities, 0) + 0x18) == crcs[1122],
                  "Baraka lists the two registered finishers starting with Blade Lift");
        }
        const size_t specials = childOf(root, 0);
        Check(specials != 0 && Read(bytes, specials + 4) == 2
                  && Read(bytes, specials + 0xC) == 4 && Read(bytes, specials + 0x10) == root_id
                  && Read(bytes, specials + 0x18) == crcs[461]
                  && Read(bytes, specials + 0x1C) == crcs[510],
              "Specials category has the original label and four button groups");
        for (size_t j = 0; j < buttons.size(); ++j) {
            const size_t button = childOf(specials, j);
            constexpr std::array<std::array<uint32_t, 3>, 4> expected_counts{{
                {2, 2, 1}, {1, 1, 2}, {1, 1, 2}, {2, 1, 1},
            }};
            Check(button != 0 && Read(bytes, button + 4) == 1 && Read(bytes, button + 0xC) == expected_counts[i][j]
                      && Read(bytes, button + 0x10) == Read(bytes, specials)
                      && Read(bytes, button + 0x18) == crcs[buttons[j]]
                      && Read(bytes, button + 0x1C) == crcs[516 + j],
                  "button category includes documented variants alongside the original move");
            const size_t leaf = childOf(button, 0);
            Check(leaf != 0 && Read(bytes, leaf + 4) == 0 && Read(bytes, leaf + 0xC) == 0
                      && Read(bytes, leaf + 0x10) == Read(bytes, button)
                      && Read(bytes, leaf + 0x18) == crcs[labels[i][j]]
                      && Read(bytes, leaf + 0x1C) == crcs[descriptions[i][j]],
                  "reference leaf uses the authored first-rank label and description");
            Check(Read(bytes, leaf + 0x20) == Absent && Read(bytes, leaf + 0x24) == Absent
                      && Read(bytes, leaf + 0x28) == 0 && Read(bytes, leaf + 0x2C) == 1,
                  "reference leaf has no previews or XP cost and is acquired");
        }
    }
}

void TestOriginalChild(std::span<const uint32_t> crcs)
{
    auto raw = Fixture();
    Write(raw, 8, 5);
    Write(raw, FirstNode + 3 * NodeSize + 0xC, 1);
    const size_t child = raw.size() + 4;
    raw.resize(child + NodeSize);
    Write(raw, child - 4, 0x22);
    Write(raw, child, 0x22);
    Write(raw, child + 0x10, 0x2103);
    Write(raw, child + 0x14, 0x12345678);
    TestExtension(raw, crcs, 5);
}

}

int main()
{
    std::array<uint32_t, 1751> crcs{};
    std::iota(crcs.begin(), crcs.end(), 0x10000000u);
    TestExtension(Fixture(), crcs, 4);
    TestOriginalChild(crcs);
    TestMalformed(crcs);
    return failures == 0 ? 0 : 1;
}
