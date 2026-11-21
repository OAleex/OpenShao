#include "campaign_moves.h"

#include "campaign_moves_catalog.h"
#include "campaign_moves_database.h"
#include "survival_session.h"
#include "kernel/kernel.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

X86_EXTERN_FUNC(__imp__sub_000150C9);
X86_EXTERN_FUNC(__imp__sub_00013779);
X86_EXTERN_FUNC(__imp__sub_00013B9D);
X86_EXTERN_FUNC(__imp__sub_0001658C);
X86_EXTERN_FUNC(__imp__sub_00013A12);
X86_EXTERN_FUNC(__imp__sub_000131FC);
X86_EXTERN_FUNC(__imp__sub_000116CC);
X86_EXTERN_FUNC(__imp__sub_00011D16);
X86_EXTERN_FUNC(__imp__sub_00013954);
X86_EXTERN_FUNC(__imp__sub_000113C0);
X86_EXTERN_FUNC(__imp__sub_000111C2);
X86_EXTERN_FUNC(__imp__sub_00012FD1);

namespace openshao::campaign_moves {
namespace {

constexpr uint32_t Database = 0x4EFA88;
constexpr uint32_t MaxBytes = 1024 * 1024;
constexpr std::array<uint32_t, 4> OriginalFighters{0, 1, 88, 82};
struct ImportedState {
    uint32_t block = 0, size = 0, root_table = 0, root_slot = 0;
    std::array<uint32_t, 4> original{}, extra{};
    std::vector<uint32_t> references;
    std::array<uint32_t, ExtraNodeCount> extra_nodes{};
} imported;
bool importing = false;

bool Readable(uint32_t at, uint32_t size) {
    if (!at || !size || uint64_t{at} + size > UINT32_MAX) return false;
    const uint32_t last = at + size - 1;
    for (uint32_t page = at & ~0xFFFu;; page += 0x1000) {
        if (!mem::IsCommitted(std::max(page, at))) return false;
        if (page >= (last & ~0xFFFu)) return true;
    }
}

bool Campaign() {
    const auto state = kernel::R32(0x5ACEBC);
    return !survival::ActiveSession() && Readable(state, 0x8C) && kernel::R32(state + 0x88) <= 1;
}

uint32_t SelectedHero(uint32_t player) {
    const auto state = kernel::R32(0x5ACEBC);
    if (!Readable(state, 0xD4) || player > 1 || player > kernel::R32(state + 0x88))
        return UINT32_MAX;
    // The disc callback can run before the actor type reflects this selection.
    return kernel::R32(state + 0xCC + player * 4);
}

uint32_t Hero(uint32_t player) {
    if (player > 1) return UINT32_MAX;
    // cModelInterface::GetObjectPointer(player + 1), native 0x144350.
    const auto actor = kernel::R32(0x64D230 + player * 12);
    return Readable(actor, 0x1E00) ? kernel::R16(actor + 0x1DFE) : UINT32_MAX;
}

uint32_t ActiveHero() { return Hero(kernel::R32(0x4EFA60)); }

uint32_t CombatRank(uint32_t player, uint32_t skill) {
    const auto state = kernel::R32(0x5ACEBC);
    const bool combat = skill < 4 || (skill >= 13 && skill <= 22);
    if (!combat || player > 1 || !Campaign() || player > kernel::R32(state + 0x88) ||
        !ExtraFighter(Hero(player))) return UINT32_MAX;
    // Versus startup (0xB0F70) grants attack tiers and combo skills; read them without changing saved progress.
    return kernel::G<uint8_t>(0x4F3390 + skill);
}

bool Current() {
    return Campaign() && imported.block && kernel::R32(Database) == imported.block &&
        Readable(imported.block, imported.size) &&
        kernel::R32(imported.block + 0x14) == imported.size &&
        kernel::R32(imported.block + 0x10) == imported.root_table;
}

uint32_t Call(X86Context& ctx, uint32_t fn, std::initializer_list<uint32_t> args) {
    const auto saved = ctx;
    const auto result = kernel::CallGuest(ctx, fn, args);
    ctx = saved;
    return result;
}

bool InBlock(const ImportedState& state, uint32_t at, uint32_t bytes) {
    return at >= state.block && uint64_t{at} + bytes <= uint64_t{state.block} + state.size;
}

bool Capture(uint32_t size, const std::array<uint32_t, 4>& ids) {
    ImportedState candidate;
    candidate.block = kernel::R32(Database);
    candidate.size = size;
    if (!Readable(candidate.block, size) || size < 0x20 || size > MaxBytes) return false;
    const auto block = candidate.block;
    if (kernel::R32(block + 0x14) != size) return false;
    const auto count = kernel::R32(block + 8), roots = kernel::R32(block + 0xC);
    if (count > 4096 || roots != 8 || count < roots) return false;
    candidate.root_table = kernel::R32(block + 0x10);
    if (candidate.root_table != block + 0x20 || !InBlock(candidate, candidate.root_table, roots * 4)) return false;
    const auto index = kernel::R32(Database + 4);
    if (!Readable(index, count * 4)) return false;
    std::vector<uint32_t> nodes;
    nodes.reserve(count);
    uint32_t cursor = block + 0x20 + roots * 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (!InBlock(candidate, cursor, 0x30) || kernel::R32(index + i * 4) != cursor) return false;
        const auto children = kernel::R32(cursor + 0xC);
        if (children > count || !InBlock(candidate, cursor + 0x30, children * 4)) return false;
        nodes.push_back(cursor);
        cursor += 0x30 + children * 4;
    }
    if (cursor != block + size) return false;
    const auto node_pointer = [&](uint32_t at) { return std::binary_search(nodes.begin(), nodes.end(), at); };
    for (const auto node : nodes) {
        const auto parent = kernel::R32(node + 0x10), children = kernel::R32(node + 0xC);
        if (parent && !node_pointer(parent)) return false;
        if (children && kernel::R32(node + 0x14) != node + 0x30) return false;
        for (uint32_t i = 0; i < children; ++i) {
            const auto child = kernel::R32(node + 0x30 + i * 4);
            if (!node_pointer(child) || kernel::R32(child + 0x10) != node) return false;
        }
    }
    std::vector<std::pair<uint32_t, uint32_t>> captions;
    size_t extra_index = 0;
    for (uint32_t i = 0; i < roots; ++i) {
        const auto root = kernel::R32(candidate.root_table + i * 4);
        if (!node_pointer(root) || kernel::R32(root + 4) != 3 || kernel::R32(root + 0x10)) return false;
        const auto fighter = kernel::R32(root + 0x18);
        if (i < 4) {
            const auto found = std::find(OriginalFighters.begin(), OriginalFighters.end(), fighter);
            if (found == OriginalFighters.end()) return false;
            auto& slot = candidate.original[found - OriginalFighters.begin()];
            if (slot) return false;
            slot = root;
            if (fighter == 0) candidate.root_slot = candidate.root_table + i * 4;
        } else {
            const auto profile_index = i - 4;
            const auto& profile = Profiles[profile_index];
            if (fighter != profile.fighter || kernel::R32(root) != ids[profile_index] ||
                kernel::R32(root + 0xC) != 5) return false;
            candidate.extra[profile_index] = root;
            const auto first = std::lower_bound(nodes.begin(), nodes.end(), root);
            if (static_cast<size_t>(nodes.end() - first) < profile.NodeCount()) return false;
            for (uint32_t k = 0; k < profile.NodeCount(); ++k) {
                const auto node = first[k];
                if (kernel::R32(node) != ids[profile_index] + k) return false;
                candidate.extra_nodes[extra_index++] = node;
                if (kernel::R32(node + 4) != 0) continue;
                if (kernel::R32(node + 0xC) || kernel::R32(node + 0x28) != 0 ||
                    kernel::R32(node + 0x2C) != 1 || kernel::R32(node + 0x20) != UINT32_MAX ||
                    kernel::R32(node + 0x24) != UINT32_MAX) return false;
                candidate.references.push_back(node);
            }
            const auto specials = kernel::R32(root + 0x30);
            const auto combos = kernel::R32(root + 0x38);
            if (kernel::R32(combos + 0x18) != 463 ||
                kernel::R32(combos + 0xC) != profile.combos.size() ||
                kernel::R32(specials + 0x18) != 461 || kernel::R32(specials + 0xC) != 4) return false;
            const auto add_caption = [&](uint32_t parent, size_t child, size_t row, const GuideMove& move) {
                if (child >= kernel::R32(parent + 0xC)) return false;
                const auto leaf = kernel::R32(parent + 0x30 + static_cast<uint32_t>(child) * 4);
                if (kernel::R32(leaf + 4) != 0) return false;
                const auto label = GuideLabel(profile_index, row);
                captions.emplace_back(leaf + 0x18, label);
                if (!move.description) captions.emplace_back(leaf + 0x1C, label + 1);
                return true;
            };
            for (size_t row = 0; row < profile.combos.size(); ++row) {
                if (!add_caption(combos, row, row, profile.combos[row])) return false;
            }
            std::array<size_t, 3> variant_child{1, 1, 1};
            size_t row = profile.combos.size();
            for (const auto& move : profile.variants) {
                const auto group = kernel::R32(specials + 0x30 + move.button * 4);
                if (!add_caption(group, variant_child[move.button]++, row++, move)) return false;
            }
            const auto throws = kernel::R32(specials + 0x3C);
            size_t throw_child = profile.throw_count;
            for (const auto& move : profile.throws) {
                if (!add_caption(throws, throw_child++, row++, move)) return false;
            }
        }
    }
    if (std::find(candidate.original.begin(), candidate.original.end(), 0) != candidate.original.end()) return false;
    // The importer resolves only disc CRCs: apply guide captions after relocation, keep native localized ones.
    for (const auto& [address, label] : captions) kernel::W32(address, label);
    imported = std::move(candidate);
    return true;
}

bool Prepare(X86Context& ctx, ExtendedDatabase& extended, uint32_t event) {
    if (!Campaign() || (!ExtraFighter(SelectedHero(0)) && !ExtraFighter(SelectedHero(1))) ||
        !Readable(event, 0x14)) return false;
    const auto source = kernel::R32(event + 8), size = kernel::R32(event + 0x10);
    const auto text = kernel::R32(0x59E3E0), text_bytes = kernel::R32(0x59E3E4);
    if (size > MaxBytes || !Readable(source, size) || text_bytes % 4 || text_bytes > MaxBytes || !Readable(text, text_bytes)) return false;
    std::string error;
    if (!ExtendDatabase({kernel::g_base + source, size},
                        {reinterpret_cast<const uint32_t*>(kernel::g_base + text), text_bytes / 4}, extended, error)) return false;
    const auto pool = kernel::R32(event + 4), pool_id = pool & 0x7F;
    const auto permanent_pool = kernel::R32(0x4A8660), permanent_id = permanent_pool & 0x7F;
    const auto alignment = kernel::R32(0x62EC70 + pool_id * 0x68);
    const auto permanent_alignment = kernel::R32(0x62EC70 + permanent_id * 0x68);
    const auto aligned = [](uint32_t bytes, uint32_t alignment) {
        return (uint64_t{bytes} + alignment - 1) & ~(uint64_t{alignment} - 1);
    };
    if (!alignment || (alignment & (alignment - 1)) || !permanent_alignment || (permanent_alignment & (permanent_alignment - 1))) return false;
    const auto expanded = static_cast<uint32_t>(extended.bytes.size());
    const auto* header = extended.bytes.data();
    const uint32_t nodes = uint32_t{header[8]} | (uint32_t{header[9]} << 8) |
        (uint32_t{header[10]} << 16) | (uint32_t{header[11]} << 24);
    const auto temporary = aligned(expanded, alignment);
    const auto permanent = aligned(expanded, permanent_alignment) + aligned(nodes * 4, permanent_alignment);
    if (Call(ctx, 0x102FF0, {pool}) < temporary + (pool_id == permanent_id ? permanent : 0)) return false;
    return pool_id == permanent_id || Call(ctx, 0x102FF0, {permanent_pool}) >= permanent;
}

bool ExtraNode(uint32_t node) {
    return Current() && std::find(imported.extra_nodes.begin(), imported.extra_nodes.end(), node) != imported.extra_nodes.end();
}

void RejectPurchase(X86Context& ctx) {
    const auto dialog = kernel::R32(0x59E45C);
    if (Readable(dialog, 0x71)) kernel::G<uint8_t>(dialog + 0x70) = 0;
    kernel::Return(ctx, 0, 0);
}

}

bool ExtraFighter(uint32_t fighter) {
    return std::any_of(Profiles.begin(), Profiles.end(), [&](const auto& profile) { return profile.fighter == fighter; });
}

uint32_t RootFor(uint32_t fighter) {
    if (!Current()) return 0;
    for (size_t i = 0; i < Profiles.size(); ++i)
        if (Profiles[i].fighter == fighter) return imported.extra[i];
    for (size_t i = 0; i < OriginalFighters.size(); ++i)
        if (OriginalFighters[i] == fighter) return imported.original[i];
    return 0;
}

bool ReferenceNode(uint32_t node) {
    return Current() && std::find(imported.references.begin(), imported.references.end(), node) != imported.references.end();
}

namespace {
const GuideMove* FindGuideMove(uint32_t label) {
    if (label < GuideTextBase || label >= GuideTextBase + GuideTextStride * Profiles.size() || !Current())
        return nullptr;
    const auto offset = label - GuideTextBase;
    const auto& profile = Profiles[offset / GuideTextStride];
    size_t row = (offset % GuideTextStride) / 2;
    if (ActiveHero() != profile.fighter) return nullptr;
    for (const auto entries : {profile.combos, profile.variants, profile.throws}) {
        if (row < entries.size()) return &entries[row];
        row -= entries.size();
    }
    return nullptr;
}

bool ExtraMovesOpen() { return Current() && ExtraFighter(ActiveHero()); }

struct TextSize {float width = 0, height = 0;};
TextSize MeasureText(X86Context& ctx) {
    const auto text = kernel::R32(ctx.esp + 20), flags = kernel::R32(ctx.esp + 16);
    if (!text) return {};
    // The original menu measures the widest line with the selected font.
    auto measure = ctx;
    measure.esp -= 8;
    const auto dimensions = measure.esp;
    constexpr auto full_size = std::bit_cast<uint32_t>(1.f);
    kernel::CallGuest(measure, 0x152F60, {0, 0, flags, text, full_size, full_size, dimensions, dimensions + 4});
    return {float(static_cast<int32_t>(kernel::R32(dimensions))),
            float(static_cast<int32_t>(kernel::R32(dimensions + 4)))};
}

float FittedTextScale(X86Context& ctx, float preferred, float available) {
    if (available <= 0 || !std::isfinite(preferred) || preferred <= 0) return preferred;
    const auto width = MeasureText(ctx).width;
    return width > 0 ? std::min(preferred, available / width) : preferred;
}

bool WrapReferenceTitle(X86Context& ctx, float available, float preferred = 1.f,
                        float max_height = INFINITY, unsigned max_length = 255) {
    const auto address = kernel::R32(ctx.esp + 20);
    if (!address || available <= 0) return false;
    std::string text;
    bool complete = false;
    for (unsigned i = 0; i < max_length; ++i) {
        if (i > UINT32_MAX - address) return false;
        if (!Readable(address + i, 1)) return false;
        const char glyph = char(kernel::G<uint8_t>(address + i));
        if (!glyph) {complete = true;break;}
        text.push_back(glyph);
    }
    if (!complete || text.empty()) return false;
    const auto first_line = text.find_first_of("\r\n");
    const size_t end = first_line == std::string::npos ? text.size() : first_line;
    const auto original_size = MeasureText(ctx);
    const float original_scale = original_size.width > 0 ?
        std::min(preferred, available / original_size.width) : preferred;
    if (original_scale >= preferred || end < 3) return false;
    float best_scale = original_scale;
    float best_width = original_size.width;
    size_t split = std::string::npos;
    for (size_t i = 1; i + 1 < end; ++i) {
        if (text[i] != ' ' || text[i-1] == ' ' || text[i+1] == ' ') continue;
        kernel::G<uint8_t>(address + uint32_t(i)) = '\n';
        const auto measured = MeasureText(ctx);
        const unsigned lines = 2 + unsigned(std::count(text.begin(), text.end(), '\n'));
        const float width_scale = measured.width > 0 ? available / measured.width : preferred;
        const float height_scale = original_size.height > 0 ? max_height / (original_size.height * lines) : preferred;
        const float scale = std::min({preferred, width_scale, height_scale});
        kernel::G<uint8_t>(address + uint32_t(i)) = ' ';
        if (scale > best_scale + .0001f ||
            (scale >= best_scale - .0001f && measured.width < best_width - .1f)) {
            best_scale = scale;
            best_width = measured.width;
            split = i;
        }
    }
    if (split == std::string::npos) return false;
    kernel::G<uint8_t>(address + uint32_t(split)) = '\n';
    return true;
}
}

bool ReferenceCaption(uint32_t label, std::string& text) {
    const auto* move = FindGuideMove(label);
    if (!move) return false;
    const bool description = (label & 1) != 0;
    text = move->name;
    if (description) {
        text += '\n';
        text += move->command;
    }
    return true;
}

uint32_t ReferenceTitleSource(uint32_t label) {
    const auto* move = FindGuideMove(label);
    if (!move) return 0;
    if (move->title_source) return move->title_source;
    return (label & 1) ? 0 : move->description;
}

std::string_view ReferenceTranslationKey(uint32_t label) {
    const auto* move = FindGuideMove(label);
    return move ? move->key : std::string_view{};
}

bool LocalizedReferenceTitle(uint32_t address, std::string& title) {
    std::string line;
    for (uint32_t i = 0; i < 160; ++i) {
        if (uint64_t{address} + i > UINT32_MAX || !Readable(address + i, 1)) return false;
        const auto glyph = static_cast<char>(kernel::G<uint8_t>(address + i));
        if (!glyph || glyph == '\n' || glyph == '\r') {
            if (const auto colon = line.find(':'); colon != std::string::npos) line.erase(0, colon + 1);
            const auto first = line.find_first_not_of(" \t");
            const auto last = line.find_last_not_of(" .\t");
            if (first == std::string::npos || last == std::string::npos || first > last) return false;
            line = line.substr(first, last - first + 1);
            // The native list copies the label into a 64-byte local buffer.
            if (line.size() > 63) return false;
            title = std::move(line);
            return true;
        }
        line += glyph;
    }
    return false;
}

bool FitReferenceList(X86Context& ctx) {
    // The row label shares the left panel with its icon. Use the native divider and inset.
    if (kernel::R32(ctx.esp) != 0x9CC55 || !ExtraMovesOpen() || ctx.ebp != Database) return false;
    const auto x = static_cast<int32_t>(kernel::R32(ctx.esp + 4));
    const auto right = static_cast<int32_t>(kernel::R32(Database + 0x14));
    const auto inset = static_cast<int32_t>(kernel::R32(Database + 0x3C));
    const auto available = float(right - x - inset);
    // This is the native 64-byte row copy; its later measurement also sees the newline and expands the row pitch.
    WrapReferenceTitle(ctx, available, 1.f, INFINITY, 64);
    const float scale = FittedTextScale(ctx, 1.f, available);
    if (scale >= 1.f) return false;
    const auto fitted = std::bit_cast<uint32_t>(scale);
    Call(ctx, 0x152AE0, {kernel::R32(ctx.esp + 4), kernel::R32(ctx.esp + 8), kernel::R32(ctx.esp + 12),
                         kernel::R32(ctx.esp + 16), kernel::R32(ctx.esp + 20), fitted, fitted});
    kernel::Return(ctx, ctx.eax, 0);
    return true;
}

void FitReferenceDetails(X86Context& ctx, uint8_t* base, void (*draw)(X86Context&, uint8_t*)) {
    const auto caller = kernel::R32(ctx.esp);
    if ((caller != 0x9CFFA && caller != 0x9D027 && caller != 0x9D0A5) || !ExtraMovesOpen()) {
        draw(ctx, base);
        return;
    }
    const auto sx_at = ctx.esp + 24, sy_at = ctx.esp + 28;
    const auto sx_bits = kernel::R32(sx_at), sy_bits = kernel::R32(sy_at);
    const float sx = std::bit_cast<float>(sx_bits);
    constexpr float description_width = 502.f;
    const auto text_at = ctx.esp + 20, original_text = kernel::R32(text_at);
    bool wrapped = false;
    if (caller != 0x9CFFA && std::isfinite(sx) && sx > 0 && FittedTextScale(ctx, sx, description_width) < sx) {
        std::string source;
        for (unsigned i = 0; i < 511 && original_text; ++i) {
            if (!Readable(original_text + i, 1)) { source.clear(); break; }
            const char glyph = char(kernel::G<uint8_t>(original_text + i));
            if (!glyph) break;
            source.push_back(glyph);
        }
        if (!source.empty() && source.size() < 511) {
            static thread_local uint32_t buffer = 0;
            if (!buffer) buffer = mem::AllocSystem(512);
            if (buffer) {
                std::memcpy(kernel::HostPtr(buffer), source.c_str(), source.size() + 1);
                kernel::W32(text_at, buffer);
                const float sy = std::bit_cast<float>(sy_bits);
                const int32_t gap = int32_t(kernel::R32(Database + 0x44)) - int32_t(kernel::R32(Database + 0x48));
                // Titles can sit below their command: leave the panel's lower border clear in either layout.
                const auto y = int32_t(kernel::R32(ctx.esp + 8));
                const float bottom_room = std::clamp(406.f - float(y), 0.f, 96.f);
                const float room = caller == 0x9D027 && gap >= 0 ?
                    std::min(std::max(0.f, float(gap - 4)), bottom_room) : bottom_room;
                wrapped = room > 0 && std::isfinite(sy) && sy > 0 &&
                    WrapReferenceTitle(ctx, description_width, sx, room * sx / sy);
                if (!wrapped) kernel::W32(text_at, original_text);
            }
        }
    }
    const float scale = FittedTextScale(ctx, sx, description_width);
    if (scale < sx) {
        kernel::W32(sx_at, std::bit_cast<uint32_t>(scale));
        kernel::W32(sy_at, std::bit_cast<uint32_t>(std::bit_cast<float>(sy_bits) * scale / sx));
    }
    draw(ctx, base);
    if (wrapped) kernel::W32(text_at, original_text);
    if (scale < sx) {
        kernel::W32(sx_at, sx_bits);
        kernel::W32(sy_at, sy_bits);
    }
}

bool CorrectCaption(uint32_t label, uint32_t address, std::string& corrected) {
    const auto fighter = ActiveHero();
    if (fighter == 90 && (label == 1291 || label == 1292) && ExtraNode(kernel::R32(Database + 0xB0))) {
        // The scene played by cag1_fat is Groin Bashing. cag2_fat is Torso Rip.
        corrected = label == 1291 ? "{ < < < *" : "} } > > *";
        return true;
    }
    const bool combo = (label == 997 && fighter == 83) || (label == 1017 && fighter == 90);
    if (!(combo || (label == 610 && fighter == 89) || (label == 903 && fighter == 84)) ||
        !ExtraNode(kernel::R32(Database + 0xB0))) return false;
    std::string text;
    constexpr uint32_t Limit = 4096;
    for (uint32_t i = 0; i < Limit; ++i) {
        if (uint64_t{address} + i > UINT32_MAX || !Readable(address + i, 1)) return false;
        const char c = static_cast<char>(kernel::G<uint8_t>(address + i));
        if (!c) {
            if (combo) {
                const auto last_line = text.rfind('\n');
                if (last_line == std::string::npos) return false;
                text.replace(last_line + 1, std::string::npos, label == 997 ? "~, ^" : "*, *, |");
                corrected = std::move(text);
                return true;
            }
            if (label == 903) {
                // The disc's Reptile Power Slam text repeats the launcher input; the throw follow-up uses Power.
                const auto grapple = text.find('^');
                const auto power = text.find('~');
                if (grapple == std::string::npos || power == std::string::npos || power < grapple ||
                    text.find('$') >= grapple || text.find('~', power + 1) != std::string::npos) return false;
                text[power] = '|';
                corrected = std::move(text);
                return true;
            }
            size_t match = std::string::npos;
            for (size_t j = 0; j < text.size(); ++j) {
                if (text[j] != '$') continue;
                auto k = j + 1;
                while (k < text.size() && text[k] == ' ') ++k;
                if (k == text.size() || text[k++] != '+') continue;
                while (k < text.size() && text[k] == ' ') ++k;
                if (k == text.size() || (text[k] != '~' && text[k] != '|')) continue;
                if (match != std::string::npos) return false;
                match = k;
            }
            if (match == std::string::npos) return false;
            text[match] = '*';
            corrected = std::move(text);
            return true;
        }
        text.push_back(c);
    }
    return false;
}

void ResetImportedState() { imported = {}; }

ScopedRootView::ScopedRootView(uint32_t fighter) {
    const auto root = RootFor(fighter);
    if (!root || (fighter != 0 && !ExtraFighter(fighter))) return;
    const auto previous = kernel::R32(imported.root_slot);
    if (!InBlock(imported, root, 0x30) || !InBlock(imported, previous, 0x30) ||
        kernel::R32(root + 4) != 3 || kernel::R32(previous + 4) != 3) return;
    slot_ = imported.root_slot;
    previous_root_ = previous;
    label_ = root + 0x18;
    previous_label_ = kernel::R32(label_);
    kernel::W32(slot_, root);
    kernel::W32(label_, 0);
}

ScopedRootView::~ScopedRootView() {
    if (label_) kernel::W32(label_, previous_label_);
    if (slot_) kernel::W32(slot_, previous_root_);
}

}

X86_FUNC(sub_000111C2) {
    const auto rank = openshao::campaign_moves::CombatRank(kernel::R32(ctx.esp + 4), kernel::R32(ctx.esp + 8));
    if (rank != UINT32_MAX) { kernel::Return(ctx, rank, 0); return; }
    __imp__sub_000111C2(ctx, base);
}

X86_FUNC(sub_00012FD1) {
    const auto flags = kernel::R32(ctx.esp + 8);
    uint32_t skill = UINT32_MAX;
    if (flags & 0x10000000u) skill = 0;
    else if (flags & 0x20000000u) skill = 1;
    else if (flags & 0x40000000u) skill = 2;
    else if (flags & 0x80000000u) skill = 3;
    auto rank = openshao::campaign_moves::CombatRank(kernel::R32(ctx.esp + 4), skill);
    if (rank == UINT32_MAX) { __imp__sub_00012FD1(ctx, base); return; }
    // The native transition checker counts combo tiers from zero.
    if (skill == 3 && rank) --rank;
    kernel::Return(ctx, rank, 0);
}

X86_FUNC(sub_000150C9) {
    using namespace openshao::campaign_moves;
    ResetImportedState();
    const auto event = kernel::R32(ctx.esp + 4);
    ExtendedDatabase extended;
    bool prepared = false;
    try { prepared = Prepare(ctx, extended, event); } catch (const std::exception&) {}
    if (!prepared) { __imp__sub_000150C9(ctx, base); return; }
    const auto size = static_cast<uint32_t>(extended.bytes.size()), pool = kernel::R32(event + 4);
    const auto source_size = kernel::R32(event + 0x10);
    const auto allocation = Call(ctx, 0x1050A0, {pool, size, 1, 0x49F4D8, 0x49F5BC, 0xB8B});
    if (!allocation || allocation == UINT32_MAX) { __imp__sub_000150C9(ctx, base); return; }
    if (!Readable(allocation, size) || !Readable(ctx.esp - 0x40, 0x40)) {
        Call(ctx, 0x1031A0, {pool, size, 0x49F4D8, 0x49F5BC, 0xBCA});
        __imp__sub_000150C9(ctx, base);
        return;
    }
    std::memcpy(base + allocation, extended.bytes.data(), size);
    // Reserve the event above the temporary frame so native stack growth cannot overwrite it.
    const auto stack = ctx.esp;
    std::array<uint8_t, 0x40> scratch;
    std::memcpy(scratch.data(), base + stack - 0x40, scratch.size());
    const auto isolated_event = stack - 0x20;
    std::memcpy(base + isolated_event, base + event, 0x14);
    kernel::W32(isolated_event + 8, allocation);
    kernel::W32(isolated_event + 0x10, size);
    ctx.esp = stack - 0x40;
    kernel::W32(ctx.esp, kernel::R32(stack));
    kernel::W32(ctx.esp + 4, isolated_event);
    importing = true;
    __imp__sub_000150C9(ctx, base);
    importing = false;
    ctx.esp = stack + 8;
    std::memcpy(base + stack - 0x40, scratch.data(), scratch.size());
    Call(ctx, 0x1031A0, {pool, source_size, 0x49F4D8, 0x49F5BC, 0xBCA});
    try { Capture(size, extended.extra_root_ids); } catch (const std::exception&) {}
}

X86_FUNC(sub_00013779) {
    openshao::campaign_moves::ResetImportedState();
    __imp__sub_00013779(ctx, base);
}

X86_FUNC(sub_00013B9D) {
    openshao::campaign_moves::ScopedRootView view(kernel::R32(ctx.esp + 4));
    __imp__sub_00013B9D(ctx, base);
}

X86_FUNC(sub_0001658C) {
    openshao::campaign_moves::ScopedRootView view(openshao::campaign_moves::ActiveHero());
    __imp__sub_0001658C(ctx, base);
}

X86_FUNC(sub_00013A12) {
    using namespace openshao::campaign_moves;
    ScopedRootView view(ActiveHero());
    __imp__sub_00013A12(ctx, base);
    if (Current()) for (const auto leaf : imported.references) kernel::W32(leaf + 0x2C, 1);
}

X86_FUNC(sub_000131FC) {
    using namespace openshao::campaign_moves;
    if (importing && ExtraFighter(SelectedHero(kernel::R32(ctx.esp + 4)))) {
        // Native startup sets new-game ranks and loads keep saved ones; the fallback tree would add foreign ranks.
        kernel::Return(ctx, ctx.eax, 2);
        return;
    }
    __imp__sub_000131FC(ctx, base);
}

X86_FUNC(sub_000116CC) {
    if (openshao::campaign_moves::ReferenceNode(kernel::R32(ctx.esp + 4))) {
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000116CC(ctx, base);
}

X86_FUNC(sub_00011D16) {
    if (openshao::campaign_moves::ReferenceNode(kernel::R32(0x4EFB38))) {
        openshao::campaign_moves::RejectPurchase(ctx);
        return;
    }
    __imp__sub_00011D16(ctx, base);
}

X86_FUNC(sub_00013954) {
    if (openshao::campaign_moves::ReferenceNode(kernel::R32(0x4EFB38))) {
        openshao::campaign_moves::RejectPurchase(ctx);
        return;
    }
    __imp__sub_00013954(ctx, base);
}

X86_FUNC(sub_000113C0) {
    using namespace openshao::campaign_moves;
    if (Current() && ExtraFighter(ActiveHero()) &&
        (ExtraNode(kernel::R32(ctx.ecx + 0xB0)) || !kernel::R32(ctx.ecx + 0xB0))) {
        kernel::W32(0x59E38C, 0);
        kernel::W32(0x59E3A0, UINT32_MAX);
        kernel::W16(0x59E3B8, 0);
        kernel::Return(ctx, ctx.eax, 0);
        return;
    }
    __imp__sub_000113C0(ctx, base);
}
