#include "cheat_menu.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <array>
#include <cfloat>
#include <string>
#include <string_view>
#include <imgui.h>
#include "launcher.h"
#include "launcher_fonts.h"
#include "menu_text.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "core/log.h"

namespace openshao::cheat {

void FillPlayerHealth();

namespace guest {
constexpr uint32_t kBaseHealth       = 300;
constexpr uint32_t kEntityComponent  = 0x200;
constexpr uint32_t kCompHealth       = 0x368;
constexpr uint32_t kCompHealthMirror = 0x36C;
constexpr uint32_t kEntityBase0      = 0x5D1AB8;
constexpr uint32_t kEntityStride     = 0x3238;
constexpr uint32_t kXPPlayer1        = 0x81798240;
constexpr uint32_t kXPPlayer2        = 0x81798290;
constexpr uint32_t kEnergyP1         = 0x4E65A8;
constexpr uint32_t kEnergyP2         = 0x4E65CC;
constexpr uint32_t kFatalityP1       = 0x81798254;
constexpr uint32_t kFatalityP2       = 0x817982A4;
// Fatality, Multality and Brutality share one meter of three 400-point bars.
constexpr float    kFatalityFull     = 1200.0f;

void Apply(const CheatState& st) {
    if (!mem::Base()) return;

    if (st.infinite_health) FillPlayerHealth();

    for (int p = 0; p < 2; ++p) {
        if (!(p == 0 ? st.infinite_xp_p1 : st.infinite_xp_p2)) continue;
        constexpr uint32_t kMaxXP = 999999;
        const uint32_t addr = p == 0 ? kXPPlayer1 : kXPPlayer2;
        if (!mem::IsCommitted(addr)) continue;
        if (kernel::R32(addr) < kMaxXP) kernel::W32(addr, kMaxXP);
    }

    for (int p = 0; p < 2; ++p) {
        if (!(p == 0 ? st.infinite_energy_p1 : st.infinite_energy_p2)) continue;
        constexpr uint32_t kFullEnergyBits = 0x42C80000; // 100.0f
        const uint32_t addr = p == 0 ? kEnergyP1 : kEnergyP2;
        if (!mem::IsCommitted(addr)) continue;
        float cur;
        const uint32_t raw = kernel::R32(addr);
        std::memcpy(&cur, &raw, 4);
        if (cur >= 0.0f && cur < 100.0f) kernel::W32(addr, kFullEnergyBits);
    }

    for (int p = 0; p < 2; ++p) {
        if (!(p == 0 ? st.infinite_fatality_p1 : st.infinite_fatality_p2)) continue;
        const uint32_t addr = p == 0 ? kFatalityP1 : kFatalityP2;
        if (!mem::IsCommitted(addr)) continue;
        float cur;
        const uint32_t raw = kernel::R32(addr);
        std::memcpy(&cur, &raw, 4);
        if (cur >= 0.0f && cur < kFatalityFull) {
            const float full = kFatalityFull;
            uint32_t bits;
            std::memcpy(&bits, &full, 4);
            kernel::W32(addr, bits);
        }
    }
}
}

namespace {
std::atomic<bool> enabled{false};
std::atomic<bool> visible{false};
std::atomic<int> hotkey{0x77}; // VK_F8
std::atomic<int> menu_x{100};
std::atomic<int> menu_y{5};
ImFont* font_regular = nullptr;
ImFont* font_bold = nullptr;
// Pixel sizes the menu typeface is baked at. Text always shrinks from the nearest larger
// size, never grows, so it stays sharp from 720p to 4K.
constexpr float kBakedSizes[] = {16.f, 24.f, 32.f, 48.f, 64.f, 96.f};
constexpr int kBakedCount = int(std::size(kBakedSizes));
ImFont* ladder_regular[kBakedCount] = {};
ImFont* ladder_bold[kBakedCount] = {};
bool ladder_ready = false;
CheatState state{};

enum class Screen { Root, Player1, Player2, Settings, Count };
enum class Kind { Toggle, Submenu, Slider, Action };
struct Entry {
    const char* key;
    Kind kind;
    bool* toggle;
    Screen target;
    int axis;
};
struct Page {
    const char* title;
    const Entry* entries;
    int count;
};

const Entry root_entries[] = {
    {"cheat.infinite_health", Kind::Toggle,  &state.infinite_health, Screen::Root,     0},
    {"cheat.player1",         Kind::Submenu, nullptr,                Screen::Player1,  0},
    {"cheat.player2",         Kind::Submenu, nullptr,                Screen::Player2,  0},
    {"cheat.settings",        Kind::Submenu, nullptr,                Screen::Settings, 0},
};
const Entry player1_entries[] = {
    {"cheat.infinite_xp",        Kind::Toggle, &state.infinite_xp_p1,       Screen::Root, 0},
    {"cheat.infinite_energy",    Kind::Toggle, &state.infinite_energy_p1,   Screen::Root, 0},
    {"cheat.infinite_fatality",  Kind::Toggle, &state.infinite_fatality_p1, Screen::Root, 0},
};
const Entry player2_entries[] = {
    {"cheat.infinite_xp",        Kind::Toggle, &state.infinite_xp_p2,       Screen::Root, 0},
    {"cheat.infinite_energy",    Kind::Toggle, &state.infinite_energy_p2,   Screen::Root, 0},
    {"cheat.infinite_fatality",  Kind::Toggle, &state.infinite_fatality_p2, Screen::Root, 0},
};
const Entry settings_entries[] = {
    {"cheat.position_x",     Kind::Slider, nullptr, Screen::Root, 0},
    {"cheat.position_y",     Kind::Slider, nullptr, Screen::Root, 1},
    {"cheat.reset_position", Kind::Action, nullptr, Screen::Root, 0},
};
const Page pages[] = {
    {"cheat.title",    root_entries,     int(std::size(root_entries))},
    {"cheat.player1",  player1_entries,  int(std::size(player1_entries))},
    {"cheat.player2",  player2_entries,  int(std::size(player2_entries))},
    {"cheat.settings", settings_entries, int(std::size(settings_entries))},
};

constexpr int kDefaultX = 100, kDefaultY = 5, kPositionStep = 5;
Screen screen = Screen::Root;
int selection[int(Screen::Count)] = {};
std::atomic<bool> nav_up{false}, nav_down{false}, nav_left{false}, nav_right{false};
std::atomic<bool> nav_confirm{false}, nav_back{false};
}

bool Enabled() { return enabled.load(std::memory_order_relaxed); }
void SetEnabled(bool value) { enabled.store(value, std::memory_order_relaxed); }
bool Visible() { return visible.load(std::memory_order_relaxed); }
void Toggle() { visible.store(!Visible(), std::memory_order_relaxed); }
void SetHotkey(int vk) { hotkey.store(vk, std::memory_order_relaxed); }
int Hotkey() { return hotkey.load(std::memory_order_relaxed); }
void SetFonts(ImFont* regular, ImFont* bold) { font_regular = regular; font_bold = bold; }

void InitFonts() {
    if (ladder_ready || !ImGui::GetCurrentContext()) return;
    ImGuiIO& io = ImGui::GetIO();
    // Fonts can only be added before the backend builds the atlas.
    if (io.Fonts->IsBuilt()) return;
    // The default font stays first so existing overlays keep their look.
    if (io.Fonts->Fonts.Size == 0) io.Fonts->AddFontDefault();
    namespace fonts = launcher::fonts;
    const auto bake = [&](const unsigned char* data, size_t size, ImFont** out) {
        for (int i = 0; i < kBakedCount; ++i) {
            ImFontConfig config;
            config.FontDataOwnedByAtlas = false;
            config.OversampleH = 2;
            config.PixelSnapH = false;
            out[i] = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), int(size), kBakedSizes[i],
                                                    &config, io.Fonts->GetGlyphRangesDefault());
        }
    };
    bake(fonts::kMedium, fonts::kMediumSize, ladder_regular);
    bake(fonts::kSemiBold, fonts::kSemiBoldSize, ladder_bold);
    ladder_ready = true;
}
void SetMenuPosition(int x_percent, int y_percent) {
    menu_x.store(std::clamp(x_percent, 0, 100), std::memory_order_relaxed);
    menu_y.store(std::clamp(y_percent, 0, 100), std::memory_order_relaxed);
}
const CheatState& State() { return state; }

namespace {
std::atomic<bool> statue_full_heal{false};
std::atomic<int64_t> statue_heal_until_ms{0};
uint32_t max_health = guest::kBaseHealth;
uint32_t candidate_health = 0;
std::chrono::steady_clock::time_point candidate_since;
constexpr int64_t kStatueHealMs = 1000;
// The statue writes a deliberately oversized value so the true maximum does not need to be known;
// the game pulls health back to its real maximum once the burst ends.
constexpr uint32_t kStatueOverfill = 5000;
uint32_t seen_max[2] = {guest::kBaseHealth, guest::kBaseHealth};

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool PlayerHealthAddress(uint32_t& address) {
    if (!mem::Base()) return false;
    const uint32_t comp = kernel::R32(guest::kEntityBase0 + guest::kEntityComponent);
    if (comp < 0x1000000 || !mem::IsCommitted(comp + guest::kCompHealthMirror)) return false;
    address = comp + guest::kCompHealth;
    return true;
}
}

void SetStatueFullHeal(bool enabled) { statue_full_heal.store(enabled, std::memory_order_relaxed); }
bool StatueFullHeal() { return statue_full_heal.load(std::memory_order_relaxed); }

// The loading screen briefly reports bogus values (1280 was seen for ~2 s), so a
// value only counts as the maximum after it has stayed unchanged for a few seconds.
void TrackPlayerHealth() {
    uint32_t address;
    if (!PlayerHealthAddress(address)) return;
    const uint32_t current = kernel::R32(address);
    const auto now = std::chrono::steady_clock::now();
    if (current != candidate_health) {
        candidate_health = current;
        candidate_since = now;
        return;
    }
    if (current > max_health && current <= 5000 && now - candidate_since >= std::chrono::seconds(4))
        max_health = current;
}

// Shared by the Infinite Health cheat (every frame) and the save statue (for one second).
void FillPlayerHealth() {
    uint32_t* max_seen = seen_max;
    for (uint32_t p = 0; p < 2; ++p) {
        const uint32_t ent = guest::kEntityBase0 + p * guest::kEntityStride;
        if (!mem::IsCommitted(ent + guest::kEntityComponent)) continue;
        const uint32_t comp = kernel::R32(ent + guest::kEntityComponent);
        if (comp < 0x1000000 || !mem::IsCommitted(comp + guest::kCompHealthMirror)) continue;
        const uint32_t cur = kernel::R32(comp + guest::kCompHealth);
        if (cur > 0 && cur <= 100000 && cur > max_seen[p]) max_seen[p] = cur;
        const uint32_t target = std::max(max_seen[p], max_health);
        if (cur > 0 && cur < target) {
            kernel::W32(comp + guest::kCompHealth, target);
            kernel::W32(comp + guest::kCompHealthMirror, target);
        }
    }
}

void OnStatueUsed() {
    statue_heal_until_ms.store(NowMs() + kStatueHealMs, std::memory_order_relaxed);
}

namespace {
template <class Fn> void ForEachPlayerHealth(Fn fn) {
    for (uint32_t p = 0; p < 2; ++p) {
        const uint32_t ent = guest::kEntityBase0 + p * guest::kEntityStride;
        if (!mem::IsCommitted(ent + guest::kEntityComponent)) continue;
        const uint32_t comp = kernel::R32(ent + guest::kEntityComponent);
        if (comp < 0x1000000 || !mem::IsCommitted(comp + guest::kCompHealthMirror)) continue;
        fn(p, comp);
    }
}

void TickStatueHeal() {
    if (NowMs() < statue_heal_until_ms.load(std::memory_order_relaxed)) {
        ForEachPlayerHealth([](uint32_t, uint32_t comp) {
            const uint32_t cur = kernel::R32(comp + guest::kCompHealth);
            if (cur > 0 && cur < kStatueOverfill) {
                kernel::W32(comp + guest::kCompHealth, kStatueOverfill);
                kernel::W32(comp + guest::kCompHealthMirror, kStatueOverfill);
            }
        });
    }
}
}

bool Message(uint32_t message, uintptr_t key, intptr_t flags) {
    constexpr uint32_t keydown = 0x100, keyup = 0x101;
    if (!Enabled()) return false;
    const int hk = Hotkey();
    if (int(key) != hk || (message != keydown && message != keyup)) return false;
    if (message == keydown && !(uintptr_t(flags) & (uintptr_t(1) << 30)))
        Toggle();
    return true;
}

bool HandleKey(unsigned virtual_key, bool down) {
    if (!Visible()) return false;
    if (!down) return true;
    switch (virtual_key) {
    case 38: nav_up.store(true); break;      // VK_UP
    case 40: nav_down.store(true); break;    // VK_DOWN
    case 37: nav_left.store(true); break;    // VK_LEFT
    case 39: nav_right.store(true); break;   // VK_RIGHT
    case 13: nav_confirm.store(true); break; // VK_RETURN
    case 8: nav_back.store(true); break;     // VK_BACK
    }
    return true;
}

namespace {
std::string_view Tr(std::string_view id) {
    const auto text = menu::Catalog::Builtin().Get(launcher::Current().language, id);
    return text.empty() ? id : text;
}

void SavePosition(int x, int y) {
    auto next = launcher::Current();
    next.cheat_menu_x = std::clamp(x, 0, 100);
    next.cheat_menu_y = std::clamp(y, 0, 100);
    std::string error;
    if (!launcher::ApplyInGame(next, true, error)) LOGE("Cheat menu position was not saved: %s", error.c_str());
}

void Adjust(const Entry& entry, int direction) {
    if (entry.kind == Kind::Toggle) { *entry.toggle = !*entry.toggle; return; }
    if (entry.kind != Kind::Slider) return;
    int x = menu_x.load(), y = menu_y.load();
    (entry.axis == 0 ? x : y) += direction * kPositionStep;
    SavePosition(x, y);
}

void Activate(const Entry& entry) {
    switch (entry.kind) {
    case Kind::Toggle: *entry.toggle = !*entry.toggle; break;
    case Kind::Submenu: screen = entry.target; selection[int(screen)] = 0; break;
    case Kind::Action: SavePosition(kDefaultX, kDefaultY); break;
    case Kind::Slider: break;
    }
}

void ProcessInput() {
    const Page& page = pages[int(screen)];
    int& index = selection[int(screen)];
    if (nav_up.exchange(false)) index = (index - 1 + page.count) % page.count;
    if (nav_down.exchange(false)) index = (index + 1) % page.count;
    if (nav_left.exchange(false)) Adjust(page.entries[index], -1);
    if (nav_right.exchange(false)) Adjust(page.entries[index], +1);
    if (nav_confirm.exchange(false)) Activate(page.entries[index]);
    if (nav_back.exchange(false)) {
        if (screen != Screen::Root) screen = Screen::Root;
        else Toggle();
    }
}

ImVec2 TextSize(ImFont* font, float size, std::string_view text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.f, text.data(), text.data() + text.size());
}

void Text(ImDrawList* list, ImFont* font, float size, ImVec2 position, ImU32 color, std::string_view text) {
    list->AddText(font, size, position, color, text.data(), text.data() + text.size());
}

void DrawMenu() {
    const auto* viewport = ImGui::GetMainViewport();
    ImDrawList* list = ImGui::GetForegroundDrawList();
    ImFont* fallback_regular = font_regular ? font_regular : ImGui::GetFont();
    ImFont* fallback_bold = font_bold ? font_bold : fallback_regular;
    // Pixels per framebuffer unit, so text is picked for the real render size on high-DPI displays.
    const float density = ImGui::GetIO().DisplayFramebufferScale.y;
    const auto face = [&](bool heavy, float size) {
        if (!ladder_ready) return heavy ? fallback_bold : fallback_regular;
        const float pixels = size * density;
        int index = kBakedCount - 1;
        for (int i = 0; i < kBakedCount; ++i) if (kBakedSizes[i] >= pixels - 0.01f) { index = i; break; }
        ImFont* font = (heavy ? ladder_bold : ladder_regular)[index];
        return font ? font : (heavy ? fallback_bold : fallback_regular);
    };
    const float s = std::max(0.75f, viewport->Size.y / 1080.f);
    const Page& page = pages[int(screen)];
    const int selected = selection[int(screen)];

    const float width = 380 * s, header = 84 * s, bar = 36 * s, row = 42 * s, footer = 30 * s, margin = 20 * s;
    const float height = header + bar + row * page.count + footer;
    const float x = viewport->Pos.x + margin + std::max(0.f, viewport->Size.x - width - 2 * margin) * menu_x.load() / 100.f;
    const float y = viewport->Pos.y + margin + std::max(0.f, viewport->Size.y - height - 2 * margin) * menu_y.load() / 100.f;

    list->AddRectFilled({x + 5 * s, y + 6 * s}, {x + width + 5 * s, y + height + 6 * s}, IM_COL32(0, 0, 0, 110), 4 * s);
    list->AddRectFilledMultiColor({x, y}, {x + width, y + header},
        IM_COL32(58, 132, 220, 255), IM_COL32(58, 132, 220, 255), IM_COL32(16, 72, 165, 255), IM_COL32(16, 72, 165, 255));

    const auto title = Tr(page.title);
    const float title_size = 46 * s;
    ImFont* bold = face(true, title_size);
    const ImVec2 title_extent = TextSize(bold, title_size, title);
    const ImVec2 title_position{x + (width - title_extent.x) / 2, y + (header - title_extent.y) / 2};
    Text(list, bold, title_size, {title_position.x + 2 * s, title_position.y + 2 * s}, IM_COL32(0, 0, 0, 90), title);
    Text(list, bold, title_size, title_position, IM_COL32(255, 255, 255, 255), title);

    const float bar_y = y + header;
    list->AddRectFilled({x, bar_y}, {x + width, bar_y + bar}, IM_COL32(0, 0, 0, 255));
    const float bar_text = 19 * s;
    ImFont* regular = face(false, bar_text);
    const auto select_text = Tr("cheat.select");
    Text(list, regular, bar_text, {x + 14 * s, bar_y + (bar - TextSize(regular, bar_text, select_text).y) / 2},
         IM_COL32(48, 150, 255, 255), select_text);
    const std::string counter = std::to_string(selected + 1) + " / " + std::to_string(page.count);
    Text(list, regular, bar_text, {x + width - 14 * s - TextSize(regular, bar_text, counter).x,
         bar_y + (bar - TextSize(regular, bar_text, counter).y) / 2}, IM_COL32(255, 255, 255, 255), counter);

    const float text_size = 24 * s;
    ImFont* row_regular = face(false, text_size);
    ImFont* row_bold = face(true, text_size);
    for (int i = 0; i < page.count; ++i) {
        const Entry& entry = page.entries[i];
        const bool active = i == selected;
        const float top = bar_y + bar + row * i;
        list->AddRectFilled({x, top}, {x + width, top + row}, active ? IM_COL32(255, 255, 255, 255) : IM_COL32(23, 33, 49, 238));
        const ImU32 ink = active ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
        ImFont* font = active ? row_bold : row_regular;
        const auto label = Tr(entry.key);
        Text(list, font, text_size, {x + 16 * s, top + (row - TextSize(font, text_size, label).y) / 2}, ink, label);

        const float right = x + width - 16 * s, middle = top + row / 2;
        if (entry.kind == Kind::Submenu) {
            const ImVec2 c{right - 5 * s, middle};
            list->AddPolyline(std::array<ImVec2, 3>{ImVec2{c.x - 5 * s, c.y - 9 * s}, ImVec2{c.x + 4 * s, c.y}, ImVec2{c.x - 5 * s, c.y + 9 * s}}.data(),
                              3, ink, ImDrawFlags_None, 2.6f * s);
        } else if (entry.kind == Kind::Toggle) {
            const bool on = *entry.toggle;
            const auto value = Tr(on ? "value.on" : "value.off");
            const ImU32 color = on ? (active ? IM_COL32(0, 128, 58, 255) : IM_COL32(88, 222, 130, 255))
                                   : (active ? IM_COL32(110, 110, 110, 255) : IM_COL32(150, 160, 176, 255));
            Text(list, font, text_size, {right - TextSize(font, text_size, value).x, top + (row - TextSize(font, text_size, value).y) / 2}, color, value);
        } else if (entry.kind == Kind::Slider) {
            const std::string value = std::to_string(entry.axis == 0 ? menu_x.load() : menu_y.load()) + "%";
            const float arrow = 6 * s;
            const float value_width = TextSize(font, text_size, value).x;
            const float value_right = right - arrow * 2 - 12 * s;
            Text(list, font, text_size, {value_right - value_width, top + (row - TextSize(font, text_size, value).y) / 2}, ink, value);
            if (active) {
                const float lx = value_right - value_width - 12 * s, rx = right - arrow;
                list->AddTriangleFilled({lx, middle - arrow}, {lx, middle + arrow}, {lx - arrow * 1.4f, middle}, ink);
                list->AddTriangleFilled({rx, middle - arrow}, {rx, middle + arrow}, {rx + arrow * 1.4f, middle}, ink);
            }
        }
    }

    const float footer_y = bar_y + bar + row * page.count;
    list->AddRectFilled({x, footer_y}, {x + width, footer_y + footer}, IM_COL32(0, 0, 0, 255));
    const auto hint = Tr("cheat.hint");
    float hint_size = 15 * s;
    const float hint_width = TextSize(face(false, hint_size), hint_size, hint).x;
    if (hint_width > width - 20 * s) hint_size *= (width - 20 * s) / hint_width;
    ImFont* hint_font = face(false, hint_size);
    const ImVec2 hint_extent = TextSize(hint_font, hint_size, hint);
    Text(list, hint_font, hint_size, {x + (width - hint_extent.x) / 2, footer_y + (footer - hint_extent.y) / 2}, IM_COL32(168, 176, 190, 255), hint);
    list->AddRect({x, y}, {x + width, y + height}, IM_COL32(0, 0, 0, 220), 0.f, ImDrawFlags_None, 1.5f * s);
}
}

void Draw(double) {
    TickStatueHeal();
    if (!Enabled()) return;
    guest::Apply(state);
    if (!Visible()) { screen = Screen::Root; return; }
    ProcessInput();
    if (!Visible()) return;
    DrawMenu();
}
}

X86_EXTERN_FUNC(__imp__sub_000133C3);
X86_FUNC(sub_000133C3) {
    openshao::cheat::TrackPlayerHealth();
    __imp__sub_000133C3(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_0014F4A0);
X86_FUNC(sub_0014F4A0) {
    // Reaction 0xF is the player using the save statue. Read it before the call:
    // the original returns with the stack pointer past the return address.
    const bool statue = X86_MEM32(ctx.esp + 0x08) == 0xF;
    __imp__sub_0014F4A0(ctx, base);
    if (statue && openshao::cheat::StatueFullHeal())
        openshao::cheat::OnStatueUsed();
}
