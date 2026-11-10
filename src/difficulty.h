#pragma once
#include <bitset>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>

struct X86Context;
namespace openshao::difficulty {
// Easy/Normal/Hard are native 0..2; Monk runs on Hard and is stored as 3 only in save records.
enum class Mode : uint8_t { Easy = 0, Normal = 1, Hard = 2, Monk = 3 };
void DrawPopup(X86Context& ctx, uint8_t* base);
inline constexpr uint32_t SerializedMonk = 3;
inline constexpr uint32_t EasyLabel = 0xFFFFFE00, MonkLabel = 0xFFFFFE01;

struct Profile {
    uint32_t native;          // value passed to the original setter
    float incoming;           // multiplier for enemy combat hits on the heroes
    int32_t attack_delay;     // shared group attack-admission base, or -1 to keep native
    int32_t history;          // repeated-action history expiry base, or -1 to keep native
    float healing;            // multiplier for partial health recovery of the heroes
    float experience;         // multiplier for experience gained
};
constexpr Profile ProfileFor(Mode mode) {
    switch (mode) {
    case Mode::Easy: return {0, .6f, -1, -1, 1.f, 1.f};
    case Mode::Normal: return {1, 1.f, -1, -1, 1.f, 1.f};
    case Mode::Hard: return {2, 1.f, -1, -1, 1.f, 1.f};
    case Mode::Monk: return {2, 1.6f, 12, 140, .2f, .35f};
    }
    return {1, 1.f, -1, -1, 1.f, 1.f};
}
// Maps the expanded selector's native argument (row + 1) to its logical mode.
constexpr std::optional<Mode> MenuChoice(uint32_t argument) {
    switch (argument) {
    case 1: return Mode::Easy;
    case 2: return Mode::Normal;
    case 3: return Mode::Hard;
    case 4: return Mode::Monk;
    }
    return std::nullopt;
}
constexpr std::optional<Mode> NativeMode(uint32_t value) {
    if (value <= 3) return Mode(value);
    return std::nullopt;
}
// Scales a positive incoming hit. Any positive hit keeps at least one point.
constexpr int32_t ScaleDamage(int32_t damage, float factor) {
    if (damage <= 0 || factor == 1.f) return damage;
    const float scaled = float(damage) * factor + .5f;
    if (scaled >= 2147483647.f) return 2147483647;
    return scaled < 1.f ? 1 : int32_t(scaled);
}
// Scales a living hero's recovery and cancels full restores; pickups scale before the clamp; revivals stay native.
constexpr int32_t ScaleHealing(int32_t current, int32_t requested, int32_t maximum, float factor, bool pickup = false) {
    if (factor == 1.f || current <= 1 || requested <= current || current >= maximum) return requested;
    if (requested >= maximum && !pickup) return current;
    const float gain = float(requested - current) * factor;
    const int32_t amount = gain < 1.f ? 1 : int32_t(gain), missing = maximum - current;
    return current + (amount < missing ? amount : missing);
}
// Scales experience gains; spending and other decreases keep their native value.
constexpr int32_t ScaleExperience(int32_t current, int32_t requested, float factor) {
    if (factor == 1.f || requested <= current) return requested;
    const float gain = float(requested - current) * factor + .5f;
    return current + (gain < 1.f ? 1 : int32_t(gain));
}
// Ordinary enemy families that may get Monk reinforcements (no corpse props or large families).
constexpr bool Reinforceable(uint32_t type) {
    switch (type) {
    case 4: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
    case 19: case 26: case 27: case 30: case 44: case 46: return true;
    }
    return false;
}
// Wu Shi - Map 01 (room 96) is the tutorial and keeps its native encounters.
inline constexpr uint32_t TutorialRoom = 96;
// Tombs - Map 8 also keeps the original enemy count.
inline constexpr uint32_t TombsMap8Room = 7;
// Extra members: one for a single enemy or a pair, two for larger groups.
constexpr unsigned Reinforcements(uint32_t room, unsigned members, unsigned live_actors, unsigned actor_pool = 12,
                                  unsigned group_limit = 7, unsigned reserve = 3) {
    if (room == TutorialRoom || room == TombsMap8Room) return 0;
    if (!members || members > 5 || members >= group_limit) return 0;
    unsigned extra = members <= 2 ? 1 : 2;
    if (extra > group_limit - members) extra = group_limit - members;
    const unsigned used = live_actors + members + reserve;
    if (used >= actor_pool) return 0;
    if (extra > actor_pool - used) extra = actor_pool - used;
    return extra;
}

// Each extra actor allocates from the object heap (0x64D218); stop well before it runs out.
inline constexpr uint32_t HeapPerExtra = 384 * 1024, HeapReserve = 1024 * 1024, HeapBlock = 256 * 1024;
constexpr unsigned HeapExtras(unsigned extra, uint32_t free, uint32_t largest) {
    if (largest < HeapBlock || free <= HeapReserve) return 0;
    const uint32_t fits = (free - HeapReserve) / HeapPerExtra;
    return extra < fits ? extra : unsigned(fits);
}

// Monk room visits; the old exit-lock bits stay in the file so existing saves load.
class Progress {
    std::bitset<512> working_, committed_;
    std::set<int32_t> visit_;
    uint32_t room_ = 0xFFFFFFFF;
public:
    void Reset() { working_.reset(); committed_.reset(); visit_.clear(); room_ = 0xFFFFFFFF; }
    // Returns true when the room changed and the visit was reset.
    bool Enter(uint32_t room) {
        if (room == room_) return false;
        room_ = room; visit_.clear(); return true;
    }
    void Track(int32_t group) { visit_.insert(group); }
    bool Tracked(int32_t group) const { return visit_.contains(group); }
    bool HasEncounters() const { return !visit_.empty(); }
    bool Exempt(uint32_t room) const { return room < working_.size() && working_.test(room); }
    void Leave(uint32_t room) { if (room < working_.size() && !visit_.empty()) working_.set(room); }
    void Commit() { committed_ = working_; }
    void Rollback() { working_ = committed_; visit_.clear(); }
    void Load(const std::bitset<512>& rooms) { working_ = committed_ = rooms; visit_.clear(); }
    const std::bitset<512>& Committed() const { return committed_; }
    const std::bitset<512>& Working() const { return working_; }
};
std::string SerializeRooms(const std::bitset<512>& rooms);
std::bitset<512> ParseRooms(std::string_view text);
uint64_t Fingerprint(const uint8_t* record, size_t size, size_t difficulty_offset);

Mode Current();
bool MenuExpanded();
uint32_t Label(uint32_t id, uint32_t caller);  // host string replacing a native text id, or 0
// Save lists: lays out columns, adds difficulty; true when the call belongs to a slot list.
enum class TextCall { Left, Centered, Plain };
bool SaveListText(X86Context& ctx, uint8_t* base, TextCall call, void (*original)(X86Context&, uint8_t*));

void KillPanel(X86Context& ctx, uint8_t* base, void (*original)(X86Context&, uint8_t*));
// Lowers the native footer of the expanded difficulty popup; called by the text draw hooks.
void PopupFooter(X86Context& ctx);
// Debug: refills every living hero through the native setter, bypassing Monk rules.
unsigned RefillHeroes(X86Context& ctx);
// Debug: lets the next health writes through the Monk healing rules (Player 2 health split).
void SetDebugRefill(bool enabled);
// Death/retry checkpoint restores (0x14A39) so far; dialogue extras replay after each one.
uint32_t CheckpointRestores();
// Save slots loaded or written this session; 0 means the campaign has never touched a save.
uint32_t SaveSlotTransfers();
}
