// Improved Difficulty: Easy/Monk profiles, Monk save marker, totem return points, reinforcements.
namespace openshao::difficulty {
namespace {
constexpr uint32_t StatePointer = 0x5ACEBC, StateDifficulty = 0xD4, RecordBytes = 0x51AC;
constexpr uint32_t SaveManager = 0x59E600, SlotBase = 0x131C, SlotCount = 9;
constexpr uint32_t RoomId = 0x505970;
constexpr uint32_t ObjectHeap = 0x64D218, HeapTable = 0x62EC60, HeapStride = 0x68;
constexpr uint32_t AttackDelay = 0x514314, HistoryExpiry = 0x514318;
constexpr uint32_t AiManager = 0x6965C4, SpawnManager = 0x6DBFB0, ActorManager = 0x510040;
constexpr uint32_t ActorPool = 0x5142A4, GroupCount = 0x5142B4, GroupMembers = 0x5142B8;
constexpr uint32_t Residency = 0x6CA69C, RespawnCallback = 0x1D2220;
constexpr uint32_t HeroObjects = 0x5CE880, HeroStride = 0x3238;
constexpr uint32_t SelectorReturn = 0x460CE, BuilderReturn = 0x45A2F;
constexpr uint32_t SpawnRecord = 0xA6C, SpawnDescriptor = 0x74, SpawnAux = 0x10, SpawnRecords = 50;
constexpr uint32_t NormalLabel = 0x170, HardLabel = 0x171;

Mode mode = Mode::Normal;
bool load_monk = false;      // the slot just copied into the live state carried the Monk marker
bool has_totem = false;      // the Monk campaign has a totem save to return to
bool menu_expanded = false;  // the four-choice selector is open
uint32_t popup_menu = 0;
Progress progress;
std::array<int32_t, 2> totem_health{-1, -1};  // heroes' health at the last Monk totem save
std::map<int32_t, uint16_t> expanded;
uint32_t menu_storage = 0;

uint8_t R8(uint32_t address) { return kernel::G<uint8_t>(address); }
void W8(uint32_t address, uint8_t value) { kernel::G<uint8_t>(address) = value; }
bool Valid(uint32_t address, uint32_t size) {
    return address && address <= 0xFFFFFFFFu - size && mem::IsCommitted(address) && mem::IsCommitted(address + size - 1);
}
bool Enabled() { return launcher::Current().improved_difficulty; }
bool PtBr() { return mods::SameLanguage(launcher::Current().language, "pt-BR"); }
std::string_view Text(std::string_view id) {
    return menu::Catalog::Builtin().Get(launcher::Current().language, id);
}
uint32_t GuestString(const std::string& text) {
    static std::unordered_map<std::string, uint32_t> strings;
    if (const auto found = strings.find(text); found != strings.end()) return found->second;
    if (strings.size() >= 256 || text.size() > 95) return 0;
    const uint32_t address = mem::AllocSystem(96);
    if (!address) return 0;
    std::memset(kernel::HostPtr(address), 0, 96);
    std::memcpy(kernel::HostPtr(address), text.data(), text.size());
    strings.emplace(text, address);
    return address;
}
uint32_t SlotRecord(uint32_t slot) {
    const uint32_t buffer = kernel::R32(SaveManager);
    if (!buffer || slot < 1 || slot > SlotCount) return 0;
    const uint32_t record = buffer + SlotBase + (slot - 1) * RecordBytes;
    return Valid(record, RecordBytes) ? record : 0;
}
uint64_t RecordPrint(uint32_t record) {
    return Fingerprint(static_cast<const uint8_t*>(kernel::HostPtr(record)), RecordBytes, StateDifficulty);
}
bool IsHero(uint32_t object) {
    return object == HeroObjects + HeroStride || object == HeroObjects + 2 * HeroStride;
}

// Totem clearance beside the saves, keyed by page slot (page 1 = native 1..9) and record fingerprint.
std::filesystem::path ProgressFile() { return paths::SaveRoot() / "openshao-monk-progress.txt"; }
// Each line: slot, fingerprint, cleared rooms, then the heroes' health at the totem (-1 unknown).
std::bitset<512> LoadRooms(uint32_t slot, uint64_t print, std::array<int32_t, 2>& health) {
    health = {-1, -1};
    std::ifstream file(ProgressFile());
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        uint32_t stored = 0; std::string hash, rooms;
        if (!(fields >> stored >> hash >> rooms) || stored != slot) continue;
        if (hash != std::to_string(print)) return {};
        int32_t first = -1, second = -1;
        if (fields >> first) { health[0] = first; if (fields >> second) health[1] = second; }
        return ParseRooms(rooms);
    }
    return {};
}
void SaveRooms(uint32_t slot, uint64_t print, const std::bitset<512>& rooms, const std::array<int32_t, 2>& health) {
    std::error_code error;
    const auto path = ProgressFile();
    auto temporary = path;
    temporary += ".tmp";
    std::vector<std::string> lines;
    {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream fields(line);
            uint32_t stored = 0;
            if ((fields >> stored) && stored != slot && stored >= 1 &&
                stored <= save_pages::PageCount * save_pages::SlotsPerPage) lines.push_back(line);
        }
    }
    lines.push_back(std::to_string(slot) + " " + std::to_string(print) + " " + SerializeRooms(rooms) + " " +
                    std::to_string(health[0]) + " " + std::to_string(health[1]));
    {
        std::ofstream file(temporary, std::ios::trunc);
        for (const auto& line : lines) file << line << '\n';
        if (!file) { LOGW("openshao difficulty: cannot write Monk progress"); return; }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) LOGW("openshao difficulty: cannot replace Monk progress: %s", error.message().c_str());
}

void ApplyGlobals(Mode selected) {
    const auto profile = ProfileFor(selected);
    if (profile.attack_delay >= 0) kernel::W32(AttackDelay, uint32_t(profile.attack_delay));
    if (profile.history >= 0) kernel::W32(HistoryExpiry, uint32_t(profile.history));
}
void SyncRoom() {
    if (progress.Enter(kernel::R32(RoomId))) expanded.clear();
}
bool Resident(uint32_t type) {
    for (uint32_t i = 0; i < 6; ++i) if (kernel::R32(Residency + i * 4) == type) return true;
    return false;
}
// Another ordinary family already authored for this room, so its resources are loaded.
uint32_t Variant(uint32_t manager, uint32_t type) {
    for (uint32_t record = 0; record < SpawnRecords; ++record) {
        const uint32_t base = manager + record * SpawnRecord;
        const uint32_t count = std::min<uint32_t>(kernel::R16(base + 0xA68), 20);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t descriptor = base + 0x18 + i * SpawnDescriptor;
            const uint32_t candidate = kernel::R32(descriptor);
            if (candidate != type && Reinforceable(candidate) && !R8(descriptor + 0x58) && Resident(candidate)) return descriptor;
        }
    }
    return 0;
}
// Object heap free bytes and largest block, read like the native report (0x102FF0).
struct HeapRoom { uint32_t free = 0, largest = 0; };
HeapRoom ObjectHeapRoom() {
    const uint32_t heap = HeapTable + (kernel::R32(ObjectHeap) & 0x7F) * HeapStride;
    if (!kernel::R32(heap + 0x1C)) {
        const uint32_t span = kernel::R32(heap + 0x0C) - kernel::R32(heap + 0x08);
        return {span, span};
    }
    HeapRoom room;
    uint32_t node = kernel::R32(heap + 0x20);
    for (unsigned guard = 0; node && guard < 65536; ++guard, node = kernel::R32(node + 0x04)) {
        const uint32_t start = kernel::R32(node + 0x08);
        if (!start) continue;
        const uint32_t size = kernel::R32(node + 0x0C) - start;
        room.free += size;
        room.largest = std::max(room.largest, size);
    }
    return room;
}

// Appends reinforcements to the native queue so the group's own clear condition counts them.
void Reinforce(uint32_t manager, int32_t group) {
    const uint32_t record = manager + uint32_t(group) * SpawnRecord;
    const uint16_t members = kernel::R16(record + 0xA68);
    if (!members || members > 5 || !R8(record + 0xA6A)) return;
    if (const auto found = expanded.find(group); found != expanded.end() && found->second == members) return;
    const uint32_t ai = kernel::R32(AiManager);
    if (!ai || R8(ai + 0x32F)) return;
    for (uint32_t i = 0; i < members; ++i) {
        const uint32_t descriptor = record + 0x18 + i * SpawnDescriptor;
        if (!Reinforceable(kernel::R32(descriptor)) || R8(descriptor + 0x58)) return;
    }
    const unsigned wanted = Reinforcements(kernel::R32(RoomId), group, members, kernel::R16(ActorManager + 0x12),
                                          kernel::R32(ActorPool), kernel::R32(GroupMembers));
    if (!wanted) return;
    const HeapRoom heap = ObjectHeapRoom();
    const unsigned extra = HeapExtras(wanted, heap.free, heap.largest);
    if (extra < wanted)
        LOGI("openshao difficulty: room %u group %d heap free %uk largest %uk allows %u of %u",
             kernel::R32(RoomId), group, heap.free >> 10, heap.largest >> 10, extra, wanted);
    if (!extra) return;
    const uint16_t delay = kernel::R16(record + 0x18 + (members - 1) * SpawnDescriptor + 0x5A);
    for (uint32_t e = 0; e < extra; ++e) {
        const uint32_t source = record + 0x18 + (e % members) * SpawnDescriptor;
        const uint32_t target = record + 0x18 + (members + e) * SpawnDescriptor;
        const uint32_t variant = e == 1 ? Variant(manager, kernel::R32(record + 0x18)) : 0;
        std::memmove(kernel::HostPtr(target), kernel::HostPtr(variant ? variant : source), SpawnDescriptor);
        std::memmove(kernel::HostPtr(record + 0x928 + (members + e) * SpawnAux),
                     kernel::HostPtr(record + 0x928 + (e % members) * SpawnAux), SpawnAux);
        // Keep the encounter's own spawn point and timing; offset sideways so bodies do not overlap.
        kernel::W32(target + 0x18, kernel::R32(source + 0x18) + uint32_t(e % 2 ? -60 : 60));
        kernel::W32(target + 0x1C, kernel::R32(source + 0x1C));
        kernel::W32(target + 0x20, kernel::R32(source + 0x20));
        kernel::W32(target + 0x24, kernel::R32(source + 0x24));
        W8(target + 0x58, 0);
        kernel::W16(target + 0x5A, delay);
    }
    kernel::W16(record + 0xA68, uint16_t(members + extra));
    expanded[group] = uint16_t(members + extra);
    LOGI("openshao difficulty: room %u group %d reinforced %u+%u (heap free %uk largest %uk)", kernel::R32(RoomId), group,
         members, extra, heap.free >> 10, heap.largest >> 10);
}
float Measure(X86Context& ctx, const std::string& encoded) {
    const uint32_t address = GuestString(encoded);
    if (!address) return 0;
    const auto saved = ctx;
    const auto width = kernel::CallGuest(ctx, 0x152DC0, {address, 2});
    ctx = saved;
    return float(int32_t(width));
}
void Centered(X86Context& ctx, float y, std::string_view id, float scale) {
    const auto text = Text(id);
    if (text.empty()) return;
    const float width = Measure(ctx, menu::EncodeNative(text, PtBr()));
    // The drawn line is a little wider than the native measure; keep a margin inside the canvas.
    const float fitted = menu::FitScale(width, scale, 540);
    if (fitted <= 0) return;
    menu::NativeLabel(ctx, 320 - width * fitted / 2, y, text, fitted, 254, false, true, 540);
}
bool EnsureMenu() {
    if (menu_storage) return true;
    menu_storage = mem::AllocSystem(4 + 4 * 16);
    if (!menu_storage) return false;
    const uint32_t labels[] = {EasyLabel, NormalLabel, HardLabel, MonkLabel};
    kernel::W32(menu_storage, 4);
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t row = menu_storage + 4 + i * 16;
        kernel::W32(row, 0); kernel::W32(row + 4, labels[i]); kernel::W32(row + 8, 0); kernel::W32(row + 12, 0);
    }
    return true;
}
struct Restore32 {
    uint32_t address, value;
    ~Restore32() { kernel::W32(address, value); }
};
}

namespace {
// Load Game (0x447F0) uses scaled text, the in-game list (0x9F4B0) plain text; 640x480 canvas.
enum class Column : uint8_t { Header, Name, Level, Progress, Time };
struct SlotCall { uint32_t caller; Column column; bool menu; };
constexpr SlotCall SlotCalls[] = {
    {0x44897, Column::Header, true},
    {0x449FC, Column::Name, true}, {0x44B00, Column::Name, true}, {0x44C4B, Column::Name, true},
    {0x44A5B, Column::Level, true}, {0x44B5D, Column::Level, true}, {0x44CA8, Column::Level, true},
    {0x44AAC, Column::Progress, true}, {0x44BAC, Column::Progress, true}, {0x44CF7, Column::Progress, true},
    {0x44BFB, Column::Time, true},
    {0x9F641, Column::Level, false}, {0x9F663, Column::Progress, false}, {0x9F685, Column::Time, false},
};
// Column centres and widths; the name column keeps its native left edge.
struct Layout { float level, progress, time, difficulty, width[5], right; };
constexpr Layout MenuLayout{280, 380, 468, 565, {110, 110, 80, 60, 100}, 630};
constexpr Layout SaveLayout{292, 378, 440, 506, {0, 104, 40, 60, 58}, 539};
constexpr float MenuScale = .85f, SaveScale = .85f;

float FloatArg(X86Context& ctx, uint32_t offset) { return std::bit_cast<float>(kernel::R32(ctx.esp + offset)); }
void SetFloatArg(X86Context& ctx, uint32_t offset, float value) { kernel::W32(ctx.esp + offset, std::bit_cast<uint32_t>(value)); }
float TextWidth(X86Context& ctx, uint32_t text, uint32_t flags) {
    if (!text) return 0;
    const auto saved = ctx;
    const float width = float(int32_t(kernel::CallGuest(ctx, 0x152DC0, {text, flags})));
    ctx = saved;
    return width;
}
void DrawText(X86Context& ctx, TextCall call, float x, uint32_t y, uint32_t color, uint32_t flags, uint32_t text, float sx, float sy) {
    const auto saved = ctx;
    const uint32_t ix = uint32_t(int32_t(std::lround(x)));
    if (call == TextCall::Plain) kernel::CallGuest(ctx, 0x152A30, {ix, y, color, flags, text});
    else kernel::CallGuest(ctx, call == TextCall::Left ? 0x152AE0 : 0x152CE0,
                           {ix, y, color, flags, text, std::bit_cast<uint32_t>(sx), std::bit_cast<uint32_t>(sy)});
    ctx = saved;
}
uint32_t SlotDifficultyText(X86Context& ctx, uint32_t slot) {
    const uint32_t record = SlotRecord(slot);
    if (!record || R8(record + 0x84) != 1) return 0;
    const uint32_t value = kernel::R32(record + StateDifficulty);
    if (value == 0) return Label(EasyLabel, 0);
    if (value == SerializedMonk) return Label(MonkLabel, 0);
    if (value != 1 && value != 2) return 0;
    const auto saved = ctx;
    const uint32_t text = kernel::CallGuest(ctx, 0x16992, {value == 1 ? NormalLabel : HardLabel});
    ctx = saved;
    return text;
}
uint32_t HeaderText(std::string_view id) {
    const auto text = Text(id);
    return text.empty() ? 0 : GuestString(menu::EncodeNative(text, PtBr()));
}
}

// Footer draw calls of the shared popup routine 0x45F30, lowered below the four rows.
bool popup_footer = false;
constexpr std::array<uint32_t, 5> FooterCalls{0x45F93, 0x4600E, 0x45FD3, 0x4604F, 0x46097};
constexpr uint32_t FooterDrop = 10;
void PopupFooter(X86Context& ctx) {
    if (!popup_footer) return;
    const uint32_t caller = kernel::R32(ctx.esp);
    if (std::find(FooterCalls.begin(), FooterCalls.end(), caller) == FooterCalls.end()) return;
    kernel::W32(ctx.esp + 8, kernel::R32(ctx.esp + 8) + FooterDrop);
}
bool SaveListText(X86Context& ctx, uint8_t* base, TextCall call, void (*original)(X86Context&, uint8_t*)) {
    PopupFooter(ctx);
    const uint32_t caller = kernel::R32(ctx.esp);
    const SlotCall* found = nullptr;
    for (const auto& item : SlotCalls) if (item.caller == caller) { found = &item; break; }
    if (!found) return false;
    const TextCall expected = !found->menu ? TextCall::Plain
                            : found->column == Column::Header || found->column == Column::Name ? TextCall::Left : TextCall::Centered;
    if (call != expected) return false;
    const Layout& layout = found->menu ? MenuLayout : SaveLayout;
    const uint32_t slot = ctx.esi, y = kernel::R32(ctx.esp + 8), color = kernel::R32(ctx.esp + 12), flags = kernel::R32(ctx.esp + 16);
    const uint32_t text = kernel::R32(ctx.esp + 20);
    if (found->column == Column::Header) {
        // Replace the single spaced header with one caption per column.
        const float sx = FloatArg(ctx, 24), sy = FloatArg(ctx, 28);
        const struct { const char* id; float x; float width; bool left; } captions[] = {
            {"load.player", 0x69 + 6, layout.width[0], true}, {"load.level", layout.level, layout.width[1], false},
            {"load.progress", layout.progress, layout.width[2], false}, {"load.time", layout.time, layout.width[3], false},
            {"load.difficulty", layout.difficulty, layout.width[4], false}};
        for (const auto& caption : captions) {
            const uint32_t label = HeaderText(caption.id);
            if (!label) continue;
            const float fit = menu::FitScale(TextWidth(ctx, label, flags), sx * MenuScale, caption.width);
            if (fit <= 0) continue;
            DrawText(ctx, caption.left ? TextCall::Left : TextCall::Centered, caption.x, y, color, flags, label, fit, sy * fit / sx);
        }
        kernel::Return(ctx, 0, 0);
        return true;
    }
    const float centre = found->column == Column::Level ? layout.level : found->column == Column::Progress ? layout.progress : layout.time;
    const unsigned index = found->column == Column::Level ? 1 : found->column == Column::Progress ? 2 : found->column == Column::Time ? 3 : 0;
    float sx = 1, sy = 1;
    if (found->menu) {
        sx = FloatArg(ctx, 24); sy = FloatArg(ctx, 28);
        const float scaled = sx * MenuScale;
        const float fit = index ? menu::FitScale(TextWidth(ctx, text, flags), scaled, layout.width[index]) : scaled;
        const float applied = fit > 0 ? fit : scaled;
        sy = sy * applied / sx;
        sx = applied;
        SetFloatArg(ctx, 24, sx);
        SetFloatArg(ctx, 28, sy);
    }
    if (!found->menu) {
        // The in-game list's plain text cannot fit: use the scaled centered call.
        const float fit = menu::FitScale(TextWidth(ctx, text, flags), SaveScale, layout.width[index]);
        if (fit > 0) DrawText(ctx, TextCall::Centered, centre, y, color, flags, text, fit, fit);
        kernel::Return(ctx, 0, 0);
        if (found->column != Column::Time) return true;
        if (const uint32_t label = SlotDifficultyText(ctx, slot)) {
            const float scale = menu::FitScale(TextWidth(ctx, label, flags), SaveScale, layout.width[4]);
            const float width = TextWidth(ctx, label, flags) * scale;
            if (scale > 0) DrawText(ctx, TextCall::Centered, std::min(layout.difficulty, layout.right - width / 2), y, color, flags, label, scale, scale);
        }
        return true;
    }
    if (found->column != Column::Name) kernel::W32(ctx.esp + 4, uint32_t(int32_t(std::lround(centre))));
    original(ctx, base);
    if (found->column != Column::Time) return true;
    const uint32_t label = SlotDifficultyText(ctx, slot);
    if (!label) return true;
    const float width = TextWidth(ctx, label, flags);
    const float fit = menu::FitScale(width, sx, layout.width[4]);
    if (fit > 0) DrawText(ctx, TextCall::Centered, layout.difficulty, y, color, flags, label, fit, sy * fit / sx);
    return true;
}
Mode Current() { return mode; }
bool MenuExpanded() { return menu_expanded; }
uint32_t Label(uint32_t id, uint32_t caller) {
    (void)caller;
    if (id != EasyLabel && id != MonkLabel) return 0;
    const auto text = Text(id == EasyLabel ? "single_player.easy" : "single_player.monk");
    if (text.empty()) return 0;
    return GuestString(menu::EncodeNative(text, PtBr()));
}
}

using namespace openshao::difficulty;

// Difficulty setter. The expanded selector passes row+1; loads pass the live record's value.
X86_EXTERN_FUNC(__imp__sub_00011C4E);
X86_FUNC(sub_00011C4E) {
    const uint32_t from = kernel::R32(ctx.esp), slot = ctx.esp + 4, argument = kernel::R32(slot);
    const bool selector = from == SelectorReturn;
    std::optional<Mode> chosen;
    if (selector && menu_expanded) chosen = MenuChoice(argument);
    else if (argument == 2 && load_monk) chosen = Mode::Monk;
    else chosen = NativeMode(argument);
    load_monk = false;
    if (selector) menu_expanded = false;
    if (!chosen) { __imp__sub_00011C4E(ctx, base); return; }
    mode = *chosen;
    {
        Restore32 restore{slot, argument};
        kernel::W32(slot, ProfileFor(mode).native);
        __imp__sub_00011C4E(ctx, base);
    }
    ApplyGlobals(mode);
    if (selector) { progress.Reset(); expanded.clear(); has_totem = false; totem_health = {-1, -1}; }
    LOGI("openshao difficulty: mode %u from %08X", unsigned(mode), from);
}

// Expanded new-game selector: four rows from host-owned guest data instead of the retail table.
X86_EXTERN_FUNC(__imp__sub_001B0F70);
X86_FUNC(sub_001B0F70) {
    if (kernel::R32(ctx.esp) != BuilderReturn) { __imp__sub_001B0F70(ctx, base); return; }
    menu_expanded = false;
    if (!Enabled() || !EnsureMenu()) { __imp__sub_001B0F70(ctx, base); return; }
    const uint32_t menu = ctx.ecx, top = ctx.esp + 0x14, visible = ctx.esp + 0x1C;
    {
        Restore32 count{ctx.esp + 4, kernel::R32(ctx.esp + 4)}, table{ctx.esp + 8, kernel::R32(ctx.esp + 8)}, y{top, kernel::R32(top)},
            rows{visible, kernel::R32(visible)};
        kernel::W32(ctx.esp + 4, menu_storage);
        kernel::W32(ctx.esp + 8, menu_storage + 4);
        // The retail popup shows two rows at a time; show all four without scrolling.
        kernel::W32(visible, 4);
        // Raise the list (and its title) so four rows fit above the lowered button hints.
        kernel::W32(top, kernel::R32(top) - 20);
        __imp__sub_001B0F70(ctx, base);
    }
    menu_expanded = true;
    popup_menu = menu;
}

// Difficulty popup input and hints; the description follows the highlighted row.
X86_EXTERN_FUNC(__imp__sub_00011D8E);
void openshao::difficulty::DrawPopup(X86Context& ctx,uint8_t* base) {
    const uint32_t menu = ctx.ecx;
    popup_footer = menu_expanded && menu == popup_menu;
    __imp__sub_00011D8E(ctx, base);
    popup_footer = false;
    if (!menu_expanded || menu != popup_menu || kernel::R32(menu + 0x3C) != 0) return;
    const uint32_t popup = kernel::R32(menu + 0x10);
    if (!Valid(popup, 0x2C) || kernel::R32(popup + 0x28) != 1) return;
    static constexpr const char* lines[4][2] = {
        {"difficulty.easy_1", "difficulty.easy_2"}, {"difficulty.normal_1", "difficulty.normal_2"},
        {"difficulty.hard_1", "difficulty.hard_2"}, {"difficulty.monk_1", "difficulty.monk_2"}};
    const uint32_t row = kernel::R32(popup + 0x0C);
    if (row >= 4) return;
    Centered(ctx, 344, lines[row][0], .8f);
    Centered(ctx, 366, lines[row][1], .8f);
    if (row == 3) {
        Centered(ctx, 388, "difficulty.monk_3", .8f);
        Centered(ctx, 410, "difficulty.monk_4", .8f);
    }
}
X86_FUNC(sub_00011D8E) {
    if(openshao::survival::PopupInput(ctx,base))return;
    openshao::difficulty::DrawPopup(ctx,base);
}

// Return addresses of the damage calls in the native guard handler (0x74351 and 0x745C8).
constexpr bool IsGuardHit(uint32_t from) { return from == 0x74356 || from == 0x745CD; }

// Enemy hits on the heroes: scale the nominal value so block, armor and death stay native.
X86_EXTERN_FUNC(__imp__sub_00016014);
X86_FUNC(sub_00016014) {
    const uint32_t attacker = kernel::R32(ctx.esp + 4), target = kernel::R32(ctx.esp + 8), slot = ctx.esp + 12;
    const int32_t damage = int32_t(kernel::R32(slot));
    // Debug option: the players' hits on enemies are multiplied, in any difficulty and in Ko-op.
    if (const float outgoing = openshao::ed_nexus::PlayerDamage();
        outgoing > 1.f && damage > 0 && IsHero(attacker) && !IsHero(target)) {
        Restore32 restore{slot, uint32_t(damage)};
        kernel::W32(slot, uint32_t(ScaleDamage(damage, outgoing)));
        __imp__sub_00016014(ctx, base);
        return;
    }
    const float factor = ProfileFor(mode).incoming;
    // Guarded hits (chip damage) keep the native Hard value: guarding is Monk's main defense.
    const uint32_t from = kernel::R32(ctx.esp);
    if (factor == 1.f || damage <= 0 || !IsHero(target) || IsHero(attacker) || IsGuardHit(from)) {
        __imp__sub_00016014(ctx, base);
        return;
    }
    Restore32 restore{slot, uint32_t(damage)};
    kernel::W32(slot, uint32_t(ScaleDamage(damage, factor)));
    __imp__sub_00016014(ctx, base);
}

// Health setter. Monk reduces pickups and partial recovery, and cancels full restores.
bool debug_refill = false;
bool scaled_recovery = false;
X86_EXTERN_FUNC(__imp__sub_00153CD0);
X86_FUNC(sub_00153CD0) {
    if (debug_refill || scaled_recovery) { __imp__sub_00153CD0(ctx, base); return; }
    const float factor = ProfileFor(mode).healing;
    const uint32_t component = ctx.ecx, slot = ctx.esp + 4;
    if (factor == 1.f || !component ||
        (component != kernel::R32(HeroObjects + HeroStride + 0x200) && component != kernel::R32(HeroObjects + 2 * HeroStride + 0x200))) {
        __imp__sub_00153CD0(ctx, base);
        return;
    }
    // Continue (0x38F50) refills before the totem restore; in Monk heroes return with their totem-save health.
    const uint32_t from = kernel::R32(ctx.esp);
    if (from == 0x38F67 || from == 0x38F98 || from == 0x38FA6) {
        const int hero = component == kernel::R32(HeroObjects + HeroStride + 0x200) ? 0 : 1;
        const int32_t saved = totem_health[hero], maximum = int32_t(kernel::R32(component + 0x370));
        if (has_totem && saved > 0) {
            Restore32 restore{slot, kernel::R32(slot)};
            kernel::W32(slot, uint32_t(std::clamp(saved, 1, std::max(maximum, 1))));
            __imp__sub_00153CD0(ctx, base);
            return;
        }
        __imp__sub_00153CD0(ctx, base);
        return;
    }
    const int32_t requested = int32_t(kernel::R32(slot));
    // Pickup 0xAF6D0 adds to current health first: reaching max this way is still a pickup.
    const bool pickup = from == 0xAF713;
    const int32_t scaled = ScaleHealing(int32_t(kernel::R32(component + 0x368)), requested,
                                       int32_t(kernel::R32(component + 0x370)), factor, pickup);
    if (scaled == requested) { __imp__sub_00153CD0(ctx, base); return; }
    Restore32 restore{slot, uint32_t(requested)};
    kernel::W32(slot, uint32_t(scaled));
    __imp__sub_00153CD0(ctx, base);
}

// Add-health (0xD4BA0, kill reward 0x1D7A0 and scripts): scale the gain once before the native clamp and Ko-op split.
X86_EXTERN_FUNC(__imp__sub_00014D81);
X86_FUNC(sub_00014D81) {
    const float factor = ProfileFor(mode).healing;
    const uint32_t slot = ctx.esp + 4;
    const int32_t amount = int32_t(kernel::R32(slot));
    if (factor == 1.f || amount <= 0) { __imp__sub_00014D81(ctx, base); return; }
    Restore32 restore{slot, uint32_t(amount)};
    kernel::W32(slot, uint32_t(std::max(1, int32_t(float(amount) * factor))));
    struct RestoreRecovery {
        bool previous;
        ~RestoreRecovery() { scaled_recovery = previous; }
    } restore_recovery{scaled_recovery};
    scaled_recovery = true;
    __imp__sub_00014D81(ctx, base);
}

// Experience setter (state + player*0x50 + 0x48F0): gains are scaled, purchases keep their value.
X86_EXTERN_FUNC(__imp__sub_00015CD1);
X86_FUNC(sub_00015CD1) {
    const float factor = ProfileFor(mode).experience;
    const uint32_t live = kernel::R32(StatePointer), player = kernel::R32(ctx.esp + 4), slot = ctx.esp + 8;
    if (factor == 1.f || !live || player > 4) { __imp__sub_00015CD1(ctx, base); return; }
    const int32_t requested = int32_t(kernel::R32(slot));
    const int32_t current = int32_t(kernel::R32(live + player * 0x50 + 0x48F0));
    Restore32 restore{slot, uint32_t(requested)};
    kernel::W32(slot, uint32_t(ScaleExperience(current, requested, factor)));
    __imp__sub_00015CD1(ctx, base);
}

void openshao::difficulty::SetDebugRefill(bool enabled) { debug_refill = enabled; }
unsigned openshao::difficulty::RefillHeroes(X86Context& ctx) {
    unsigned refilled = 0;
    for (uint32_t hero = 1; hero <= 2; ++hero) {
        const uint32_t component = kernel::R32(HeroObjects + hero * HeroStride + 0x200);
        if (!Valid(component, 0x374)) continue;
        const int32_t current = int32_t(kernel::R32(component + 0x368)), maximum = int32_t(kernel::R32(component + 0x370));
        if (current <= 0 || maximum <= 0 || current >= maximum) continue;
        const auto saved = ctx;
        ctx.ecx = component;
        debug_refill = true;
        kernel::CallGuest(ctx, 0x153CD0, {uint32_t(maximum)});
        debug_refill = false;
        ctx = saved;
        ++refilled;
    }
    return refilled;
}

// Experience pickup ctor (pos, a, b, amount): the popup shows the scaled gain; +0x180 stays native.
X86_EXTERN_FUNC(__imp__sub_000160A0);
X86_FUNC(sub_000160A0) {
    const float factor = ProfileFor(mode).experience;
    const uint32_t slot = ctx.esp + 0x10;
    const int32_t amount = int32_t(kernel::R32(slot));
    if (factor == 1.f || amount <= 0) { __imp__sub_000160A0(ctx, base); return; }
    kernel::W32(slot, uint32_t(ScaleExperience(0, amount, factor)));
    __imp__sub_000160A0(ctx, base);
    if (ctx.eax) kernel::W32(ctx.eax + 0x180, uint32_t(amount));
}

// Kill chain, combo bonus and Ko-op team attack add straight to the slot: Monk scales them here.
static int32_t ScaledGain(int32_t amount) {
    const float factor = ProfileFor(mode).experience;
    return factor == 1.f || amount <= 0 ? amount : ScaleExperience(0, amount, factor);
}
static uint32_t ExperienceSlot(uint32_t player) {
    const uint32_t live = kernel::R32(StatePointer);
    if (!live || player < 1 || player > 4) return 0;
    const uint32_t slot = live + player * 0x50 + 0x48F0;
    return Valid(slot, 4) ? slot : 0;
}
// Lowers the slot by the part of a direct gain that Monk does not grant.
static void TrimGain(uint32_t player, int32_t gain) {
    const uint32_t slot = ExperienceSlot(player);
    if (!slot || gain <= 0) return;
    kernel::W32(slot, uint32_t(int32_t(kernel::R32(slot)) - (gain - ScaledGain(gain))));
}
static uint32_t KillRecord(uint32_t actor) {
    const uint32_t owner = Valid(actor, 0x2B70) ? kernel::R32(actor + 0x2B6C) : 0;
    const uint32_t record = Valid(owner, 0x54) ? kernel::R32(owner + 0x50) : 0;
    return Valid(record, 0x174) ? record : 0;
}

// Kill-chain award 0x7DD60: slot += (kills + combo/10) * value (cached at +0xBC).
X86_EXTERN_FUNC(__imp__sub_000146B0);
X86_FUNC(sub_000146B0) {
    const uint32_t record = ctx.ecx;
    if (ProfileFor(mode).experience != 1.f && Valid(record, 0xC0))
        kernel::W32(record + 0xBC, uint32_t(ScaledGain(int32_t(kernel::R32(record + 0xBC)))));
    __imp__sub_000146B0(ctx, base);
}

// Chain value 0x7BBF0, scaled only for the kill HUD (0x7DF60) and the fatality line (0x80610).
X86_EXTERN_FUNC(__imp__sub_0001264E);
X86_FUNC(sub_0001264E) {
    const uint32_t from = kernel::R32(ctx.esp);
    __imp__sub_0001264E(ctx, base);
    if (from == 0x7DFAA || from == 0x80657) ctx.eax = uint32_t(ScaledGain(int32_t(ctx.eax)));
}

X86_EXTERN_FUNC(__imp__sub_00012CA2);
X86_FUNC(sub_00012CA2) {
    const uint32_t from = kernel::R32(ctx.esp), record = ctx.ecx;
    __imp__sub_00012CA2(ctx, base);
    if (from != 0x7DA0C || ProfileFor(mode).experience == 1.f || !Valid(record, 0xC0)) return;
    const int32_t value = int32_t(kernel::R32(record + 0xBC));
    if (value > 0) ctx.eax = uint32_t(int32_t(ctx.eax) / value * ScaledGain(value));
}

// Fatality line 0x80610: "+N" = bonus at +0x114 (granted via the setter) + chain value.
X86_EXTERN_FUNC(__imp__sub_00011D43);
X86_FUNC(sub_00011D43) {
    const uint32_t record = ProfileFor(mode).experience != 1.f ? KillRecord(kernel::R32(ctx.esp + 4)) : 0;
    if (!record) { __imp__sub_00011D43(ctx, base); return; }
    const uint32_t bonus = kernel::R32(record + 0x114);
    Restore32 restore{record + 0x114, bonus};
    kernel::W32(record + 0x114, uint32_t(ScaledGain(int32_t(bonus))));
    __imp__sub_00011D43(ctx, base);
}

void openshao::difficulty::KillPanel(X86Context& ctx, uint8_t* base, void (*original)(X86Context&, uint8_t*)) {
    const uint32_t record = ProfileFor(mode).experience != 1.f ? KillRecord(kernel::R32(ctx.esp + 4)) : 0;
    if (!record) { original(ctx, base); return; }
    const uint32_t bonus = kernel::R32(record + 0x114);
    Restore32 restore{record + 0x114, bonus};
    kernel::W32(record + 0x114, uint32_t(ScaledGain(int32_t(bonus))));
    original(ctx, base);
}

// Combo bonus, 0x7DE90 (actor): a timed button press adds 1000 straight to the slot.
X86_EXTERN_FUNC(__imp__sub_00015A06);
X86_FUNC(sub_00015A06) {
    const uint32_t actor = kernel::R32(ctx.esp + 4);
    const uint32_t player = ProfileFor(mode).experience != 1.f && Valid(actor, 0x30CC) ? kernel::R32(actor + 0x68) : 0;
    const uint32_t slot = ExperienceSlot(player);
    const int32_t before = slot ? int32_t(kernel::R32(slot)) : 0;
    __imp__sub_00015A06(ctx, base);
    if (slot) TrimGain(player, int32_t(kernel::R32(slot)) - before);
}

// Its banner, 0x7C451: sprintf(buffer, "%s %d", label, 1000).
X86_EXTERN_FUNC(__imp__sub_000EB2C2);
X86_FUNC(sub_000EB2C2) {
    if (kernel::R32(ctx.esp) != 0x7C456 || ProfileFor(mode).experience == 1.f) { __imp__sub_000EB2C2(ctx, base); return; }
    const uint32_t slot = ctx.esp + 0x10, amount = kernel::R32(slot);
    Restore32 restore{slot, amount};
    kernel::W32(slot, uint32_t(ScaledGain(int32_t(amount))));
    __imp__sub_000EB2C2(ctx, base);
}

// Ko-op team attack 0x1E350 (value, kills, &first, &second) adds both shares to the slots.
X86_EXTERN_FUNC(__imp__sub_00015F51);
X86_FUNC(sub_00015F51) {
    const uint32_t pair = ctx.ecx, first = kernel::R32(ctx.esp + 12), second = kernel::R32(ctx.esp + 16);
    if (ProfileFor(mode).experience == 1.f || !Valid(pair, 8) || !Valid(first, 4) || !Valid(second, 4)) {
        __imp__sub_00015F51(ctx, base);
        return;
    }
    const uint32_t heroes[] = {kernel::R32(pair), kernel::R32(pair + 4)};
    __imp__sub_00015F51(ctx, base);
    const uint32_t shares[] = {first, second};
    for (uint32_t i = 0; i < 2; ++i) {
        const int32_t gain = int32_t(kernel::R32(shares[i]));
        if (gain <= 0 || !Valid(heroes[i], 0x6C)) continue;
        TrimGain(kernel::R32(heroes[i] + 0x68), gain);
        kernel::W32(shares[i], uint32_t(ScaledGain(gain)));
    }
}

uint32_t save_slot_transfers = 0;
uint32_t openshao::difficulty::SaveSlotTransfers() { return save_slot_transfers; }

// Save-slot copy-in. A Monk record is normalized to native Hard and becomes the return point.
X86_EXTERN_FUNC(__imp__sub_00015038);
X86_FUNC(sub_00015038) {
    ++save_slot_transfers;
    const uint32_t slot = kernel::R32(ctx.esp + 4), caller = kernel::R32(ctx.esp);
    openshao::save_pages::BeforeCopyIn(ctx, base, caller);
    openshao::ed_nexus::ResetPlayer2();
    __imp__sub_00015038(ctx, base);
    openshao::save_pages::AfterCopyIn(caller);
    openshao::survival::unlock::CampaignLoaded();
    load_monk = false;
    const uint32_t live = kernel::R32(StatePointer);
    if (!Valid(live, 2 * RecordBytes) || kernel::R32(live + StateDifficulty) != SerializedMonk) return;
    kernel::W32(live + StateDifficulty, 2);
    load_monk = true;
    has_totem = true;
    std::memcpy(kernel::HostPtr(live + RecordBytes), kernel::HostPtr(live), RecordBytes);
    const uint32_t record = SlotRecord(slot);
    totem_health = {-1, -1};
    const uint32_t key = openshao::save_pages::GlobalSlot(openshao::save_pages::ShownPage(), slot);
    progress.Load(record ? LoadRooms(key, RecordPrint(record), totem_health) : std::bitset<512>{});
    expanded.clear();
    LOGI("openshao difficulty: Monk slot %u loaded", slot);
}

// Save-slot copy-out. Runs before the native CRC and signature are generated for the write.
X86_EXTERN_FUNC(__imp__sub_00011663);
X86_FUNC(sub_00011663) {
    ++save_slot_transfers;
    const uint32_t slot = kernel::R32(ctx.esp + 4);
    openshao::save_pages::BeforeCopyOut(ctx, base);
    openshao::ed_nexus::Player2BeforeSave();
    __imp__sub_00011663(ctx, base);
    openshao::survival::unlock::NativeSaveSlotPrepared();
    openshao::ed_nexus::Player2AfterSave();
    openshao::save_pages::AfterCopyOut();
    if (mode != Mode::Monk) return;
    const uint32_t record = SlotRecord(slot), live = kernel::R32(StatePointer);
    if (!record) { LOGW("openshao difficulty: Monk save slot %u unavailable", slot); return; }
    kernel::W32(record + StateDifficulty, SerializedMonk);
    progress.Commit();
    has_totem = true;
    if (Valid(live, 2 * RecordBytes)) {
        std::memcpy(kernel::HostPtr(live + RecordBytes), kernel::HostPtr(record), RecordBytes);
        kernel::W32(live + RecordBytes + StateDifficulty, 2);
    }
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t component = kernel::R32(HeroObjects + (i + 1) * HeroStride + 0x200);
        totem_health[i] = Valid(component, 0x374) ? int32_t(kernel::R32(component + 0x368)) : -1;
    }
    SaveRooms(openshao::save_pages::GlobalSlot(openshao::save_pages::ShownPage(), slot), RecordPrint(record), progress.Committed(), totem_health);
    LOGI("openshao difficulty: Monk totem save in slot %u", slot);
}

// New Monk campaigns keep only the start checkpoint until the first totem save.
X86_EXTERN_FUNC(__imp__sub_000154DE);
X86_FUNC(sub_000154DE) {
    if (mode == Mode::Monk && has_totem) {
        LOG_FIRST(8, "openshao difficulty: map checkpoint kept at the totem");
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000154DE(ctx, base);
    if (mode == Mode::Monk) {
        has_totem = true;
        LOGI("openshao difficulty: Monk campaign start is the return point");
    }
}

// Checkpoint restore after death or retry.
uint32_t checkpoint_restores = 0;
uint32_t openshao::difficulty::CheckpointRestores() { return checkpoint_restores; }
X86_EXTERN_FUNC(__imp__sub_00014A39);
X86_FUNC(sub_00014A39) {
    __imp__sub_00014A39(ctx, base);
    ++checkpoint_restores;
    if (mode != Mode::Monk) return;
    progress.Rollback();
    expanded.clear();
}

// Encounter activation shared by callspawn, callspawnrc and callspawnmerge.
X86_EXTERN_FUNC(__imp__sub_001D2600);
X86_FUNC(sub_001D2600) {
    if (mode != Mode::Monk) { __imp__sub_001D2600(ctx, base); return; }
    SyncRoom();
    const int32_t group = int32_t(kernel::R32(ctx.esp + 4));
    const uint32_t merge = kernel::R32(ctx.esp + 8), relative = kernel::R32(ctx.esp + 12);
    if (ctx.ecx == SpawnManager && group >= 0 && uint32_t(group) < SpawnRecords && merge == 0xFFFFFFFF && !relative)
        Reinforce(ctx.ecx, group);
    __imp__sub_001D2600(ctx, base);
    const uint32_t created = ctx.eax;
    if (group >= 0 && Valid(created, 0x4C) && kernel::R32(created + 0x48) != RespawnCallback) progress.Track(group);
}
