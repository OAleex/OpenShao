#include "campaign_moves_database.h"

#include "campaign_moves_catalog.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace openshao::campaign_moves {
namespace {

constexpr size_t HeaderSize = 0x20;
constexpr size_t NodeSize = 0x30;
constexpr size_t WordSize = 4;
constexpr size_t MaxBytes = 1024 * 1024;
constexpr uint32_t MaxNodes = 4096;
constexpr uint32_t MaxRoots = 32;
constexpr size_t TextCount = 1751;
constexpr uint32_t Magic = 0x4450264Du; // M&PD
constexpr uint32_t Absent = 0xFFFFFFFFu;
constexpr uint32_t FirstExtraId = 0xFFF00000u;
constexpr uint32_t ExtraRootCount = static_cast<uint32_t>(Profiles.size());
constexpr size_t ExtraBytes = ExtraNodeCount * (NodeSize + WordSize);

constexpr std::array<uint32_t, 4> RequiredFighters{0, 1, 82, 88};
constexpr uint32_t FighterType = 3;
constexpr uint32_t SectionType = 2;
constexpr uint32_t ButtonType = 1;
constexpr uint32_t LeafType = 0;
constexpr uint16_t SpecialsLabel = 461;
constexpr uint16_t SpecialsDescription = 510;
constexpr std::array<uint16_t, 3> ButtonLabels{466, 467, 468};
constexpr std::array<uint16_t, 3> ButtonDescriptions{516, 517, 518};
constexpr std::array<uint16_t, 4> QuickLabels{545, 550, 560, 565};
constexpr std::array<uint16_t, 4> ComboLabels{946, 951, 961, 966};
constexpr std::array<uint16_t, 3> BasicSections{403, 404, 405};
constexpr std::array<uint16_t, 3> BasicDescriptions{494, 504, 499};
constexpr std::array<std::array<uint16_t, 4>, 3> BasicLabels{{
    {482, 484, 483, 485}, {490, 492, 491, 493}, {486, 488, 487, 489},
}};
constexpr std::array<std::array<uint16_t, 4>, 2> BasicCommands{{
    {505, 507, 506, 508}, {500, 502, 501, 503},
}};

struct Node {
    uint32_t type;
    uint32_t parent;
    uint32_t label;
    std::span<const uint8_t> children;
};

uint32_t ReadWord(std::span<const uint8_t, WordSize> bytes)
{
    uint32_t value = 0;
    for (size_t i = 0; i < bytes.size(); ++i)
        value |= uint32_t{bytes[i]} << (8 * i);
    return value;
}

void WriteWord(std::span<uint8_t, WordSize> bytes, uint32_t value)
{
    for (size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<uint8_t>(value >> (8 * i));
}

void AppendWord(std::vector<uint8_t>& bytes, uint32_t value)
{
    std::array<uint8_t, WordSize> word{};
    WriteWord(word, value);
    bytes.insert(bytes.end(), word.begin(), word.end());
}

void AppendNode(std::vector<uint8_t>& bytes, uint32_t id, uint32_t type,
                uint32_t parent, uint32_t label, uint32_t description,
                std::span<const uint32_t> children)
{
    const bool reference = type == LeafType;
    const std::array<uint32_t, 12> fields{
        id, type, 0, static_cast<uint32_t>(children.size()), parent, 0,
        label, description, Absent, Absent, reference ? 0u : Absent,
        reference ? 1u : Absent,
    };
    for (uint32_t field : fields)
        AppendWord(bytes, field);
    for (uint32_t child : children)
        AppendWord(bytes, child);
}

bool ResolvesTo(std::span<const uint32_t> crcs, size_t index)
{
    const uint32_t crc = crcs[index];
    const auto end = crcs.begin() + index;
    // Native import uses the first matching CRC and treats zero and -1 as absent.
    return crc != 0 && crc != Absent && std::find(crcs.begin(), end, crc) == end;
}

size_t LeafLabel(size_t profile, size_t button)
{
    return QuickLabels[profile] + 100 * button;
}

bool AppendExtraTree(std::vector<uint8_t>& bytes, std::span<const uint32_t> crcs,
                     size_t index, uint32_t root)
{
    const auto& profile = Profiles[index];
    bool valid = true;
    const auto text = [&](size_t label) {
        valid = valid && ResolvesTo(crcs, label);
        return crcs[label];
    };
    const uint32_t specials = root + 1;
    uint32_t next = root + 2;
    std::array<uint32_t, 4> buttons{};
    for (size_t button = 0; button < ButtonLabels.size(); ++button) {
        buttons[button] = next;
        next += 2 + static_cast<uint32_t>(std::count_if(profile.variants.begin(), profile.variants.end(),
            [&](const GuideMove& move) { return move.button == button; }));
    }
    const uint32_t throws = buttons[3] = next;
    const uint32_t throw_count = profile.throw_count + static_cast<uint32_t>(profile.throws.size());
    const uint32_t basic = throws + 1 + throw_count;
    const uint32_t combos = basic + 16;
    const uint32_t fatalities = combos + 1 + static_cast<uint32_t>(profile.combos.size());
    const uint32_t platform = fatalities + 1 + profile.fatality_count;
    AppendNode(bytes, root, FighterType, 0, profile.fighter, Absent,
               std::array{specials, basic, combos, fatalities, platform});
    AppendNode(bytes, specials, SectionType, root, text(SpecialsLabel),
               text(SpecialsDescription), buttons);
    std::array<uint32_t, 7> leaves{};
    const auto children = [&](uint32_t parent, size_t count) {
        for (size_t i = 0; i < count; ++i) leaves[i] = parent + 1 + static_cast<uint32_t>(i);
        return std::span(leaves).first(count);
    };
    for (size_t button = 0; button < ButtonLabels.size(); ++button) {
        const uint32_t id = buttons[button];
        const auto count = buttons[button + 1] - id - 1;
        AppendNode(bytes, id, ButtonType, specials, text(ButtonLabels[button]),
                   text(ButtonDescriptions[button]), children(id, count));
        AppendNode(bytes, id + 1, LeafType, id, text(LeafLabel(index, button)),
                   text(profile.descriptions[button]), {});
        for (uint32_t row = 1; row < count; ++row)
            AppendNode(bytes, id + 1 + row, LeafType, id, text(LeafLabel(index, button)), text(510), {});
    }
    AppendNode(bytes, throws, ButtonType, specials, text(469), text(519), children(throws, throw_count));
    for (uint32_t i = 0; i < profile.throw_count; ++i)
        AppendNode(bytes, throws + 1 + i, LeafType, throws,
                   text(profile.throw_label + i), text(profile.throw_description + i), {});
    for (uint32_t i = profile.throw_count; i < throw_count; ++i)
        AppendNode(bytes, throws + 1 + i, LeafType, throws, text(profile.throw_label), text(519), {});

    const std::array<uint32_t, 3> attacks{basic + 1, basic + 6, basic + 11};
    AppendNode(bytes, basic, SectionType, root, text(402), text(509), attacks);
    for (size_t group = 0; group < attacks.size(); ++group) {
        const auto section = attacks[group];
        AppendNode(bytes, section, ButtonType, basic, text(BasicSections[group]),
                   text(BasicDescriptions[group]), children(section, 4));
        for (uint32_t i = 0; i < 4; ++i)
            AppendNode(bytes, section + 1 + i, LeafType, section, text(BasicLabels[group][i]),
                       text(group == 0 ? profile.basic[i] : BasicCommands[group - 1][i]), {});
    }

    AppendNode(bytes, combos, ButtonType, root, text(463), text(511), children(combos, profile.combos.size()));
    for (uint32_t i = 0; i < profile.combos.size(); ++i) {
        const auto description = profile.combos[i].description;
        AppendNode(bytes, combos + 1 + i, LeafType, combos, text(ComboLabels[index] + i),
                   description ? text(description) : text(511), {});
    }
    AppendNode(bytes, fatalities, ButtonType, root, text(400), text(514), children(fatalities, profile.fatality_count));
    for (uint32_t i = 0; i < profile.fatality_count; ++i) {
        const auto label = profile.fatality_label + i;
        AppendNode(bytes, fatalities + 1 + i, LeafType, fatalities, text(label), text(label + 120), {});
    }
    AppendNode(bytes, platform, ButtonType, root, text(462), text(512), children(platform, 7));
    for (uint32_t i = 0; i < 7; ++i)
        AppendNode(bytes, platform + 1 + i, LeafType, platform, text(1396 + i), text(1403 + i), {});
    return valid;
}

}

bool ExtendDatabase(std::span<const uint8_t> raw,
                    std::span<const uint32_t> text_crcs,
                    ExtendedDatabase& result, std::string& error)
{
    const auto reject = [&](const char* message) {
        result = {};
        error = message;
        return false;
    };
    if (raw.size() < HeaderSize || raw.size() > MaxBytes)
        return reject("Move database size is outside supported bounds");
    if (ReadWord(raw.first<WordSize>()) != Magic)
        return reject("Invalid move database signature");

    const uint32_t node_count = ReadWord(raw.subspan<8, WordSize>());
    const uint32_t root_count = ReadWord(raw.subspan<0xC, WordSize>());
    if (root_count == 0 || root_count > MaxRoots - ExtraRootCount
        || node_count < root_count || node_count > MaxNodes - ExtraNodeCount)
        return reject("Move database counts are outside supported bounds");
    const size_t first_node = HeaderSize + WordSize * root_count;
    if (first_node > raw.size())
        return reject("Truncated move database root IDs");

    auto remaining = raw.subspan(first_node);
    std::unordered_map<uint32_t, Node> nodes;
    nodes.reserve(node_count);
    for (uint32_t i = 0; i < node_count; ++i) {
        if (remaining.size() < NodeSize)
            return reject("Truncated move database node");
        const auto fixed = remaining.first<NodeSize>();
        const uint32_t id = ReadWord(fixed.first<WordSize>());
        const uint32_t child_count = ReadWord(fixed.subspan<0xC, WordSize>());
        remaining = remaining.subspan(NodeSize);
        if (child_count > node_count || child_count > remaining.size() / WordSize)
            return reject("Invalid move database child count");
        const size_t child_bytes = WordSize * child_count;
        const Node node{ReadWord(fixed.subspan<4, WordSize>()),
                        ReadWord(fixed.subspan<0x10, WordSize>()),
                        ReadWord(fixed.subspan<0x18, WordSize>()),
                        remaining.first(child_bytes)};
        if (!nodes.emplace(id, node).second)
            return reject("Duplicate move database node ID");
        remaining = remaining.subspan(child_bytes);
    }
    // Disc reads can pad past the declared nodes; the native importer ignores that padding.
    raw = raw.first(raw.size() - remaining.size());
    if (raw.size() > MaxBytes - ExtraBytes)
        return reject("Extended move database size is outside supported bounds");
    for (uint32_t id = FirstExtraId; id < FirstExtraId + ExtraNodeCount; ++id) {
        if (nodes.contains(id))
            return reject("Move database node ID collides with an extra node");
    }

    std::unordered_set<uint32_t> fighters;
    for (const auto& profile : Profiles)
        fighters.insert(profile.fighter);
    std::unordered_set<uint32_t> visited;
    visited.reserve(node_count);
    std::vector<uint32_t> pending;
    pending.reserve(node_count);
    const auto root_ids = raw.subspan(HeaderSize, first_node - HeaderSize);
    for (size_t offset = 0; offset < root_ids.size(); offset += WordSize) {
        const uint32_t id = ReadWord(root_ids.subspan(offset).first<WordSize>());
        const auto node = nodes.find(id);
        if (node == nodes.end() || node->second.type != FighterType || node->second.parent != 0)
            return reject("Invalid move database root");
        if (!visited.insert(id).second || !fighters.insert(node->second.label).second)
            return reject("Duplicate move database root or fighter identity");
        pending.push_back(id);
    }
    for (uint32_t fighter : RequiredFighters) {
        if (!fighters.contains(fighter))
            return reject("Missing original campaign fighter root");
    }
    while (!pending.empty()) {
        const uint32_t id = pending.back();
        pending.pop_back();
        const auto children = nodes.at(id).children;
        for (size_t offset = 0; offset < children.size(); offset += WordSize) {
            const uint32_t child_id = ReadWord(children.subspan(offset).first<WordSize>());
            const auto child = nodes.find(child_id);
            if (id == 0 || child == nodes.end() || child->second.parent != id)
                return reject("Invalid move database child or parent reference");
            if (!visited.insert(child_id).second)
                return reject("Repeated or cyclic move database node reference");
            pending.push_back(child_id);
        }
    }
    if (visited.size() != nodes.size())
        return reject("Unreachable move database node or cycle");

    if (text_crcs.size() < TextCount)
        return reject("Truncated move database text CRC table");
    ExtendedDatabase extended;
    auto& bytes = extended.bytes;
    bytes.reserve(raw.size() + ExtraBytes);
    bytes.insert(bytes.end(), raw.begin(), raw.begin() + first_node);
    const std::span header{bytes.data(), HeaderSize};
    WriteWord(header.subspan<8, WordSize>(), node_count + ExtraNodeCount);
    WriteWord(header.subspan<0xC, WordSize>(), root_count + ExtraRootCount);
    uint32_t id = FirstExtraId;
    for (uint32_t i = 0; i < ExtraRootCount; ++i) {
        extended.extra_root_ids[i] = id;
        AppendWord(bytes, id);
        id += Profiles[i].NodeCount();
    }
    bytes.insert(bytes.end(), raw.begin() + first_node, raw.end());
    for (size_t i = 0; i < Profiles.size(); ++i) {
        if (!AppendExtraTree(bytes, text_crcs, i, extended.extra_root_ids[i]))
            return reject("Invalid move reference text CRC");
    }
    result = std::move(extended);
    error.clear();
    return true;
}

}
