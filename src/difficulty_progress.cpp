#include "difficulty.h"
#include <cstring>
#include <sstream>

namespace openshao::difficulty {
std::string SerializeRooms(const std::bitset<512>& rooms) {
    std::string result;
    for (size_t room = 0; room < rooms.size(); ++room)
        if (rooms.test(room)) {
            if (!result.empty()) result += ',';
            result += std::to_string(room);
        }
    return result.empty() ? "-" : result;
}
std::bitset<512> ParseRooms(std::string_view text) {
    std::bitset<512> rooms;
    size_t value = 0;
    bool digits = false;
    for (size_t i = 0; i <= text.size(); ++i) {
        const char c = i < text.size() ? text[i] : ',';
        if (c >= '0' && c <= '9') {
            value = value * 10 + size_t(c - '0');
            digits = true;
            if (value >= rooms.size()) return {};
        } else if (c == ',') {
            if (digits) rooms.set(value);
            value = 0; digits = false;
        } else if (c != '-') return {};
    }
    return rooms;
}
uint64_t Fingerprint(const uint8_t* record, size_t size, size_t difficulty_offset) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < size; ++i) {
        // The serialized difficulty marker is excluded so the stamped and normalized records agree.
        const uint8_t byte = i >= difficulty_offset && i < difficulty_offset + 4 ? 0 : record[i];
        hash = (hash ^ byte) * 0x100000001B3ull;
    }
    return hash;
}

namespace {
constexpr char MonkMagic[8] = {'O', 'S', 'M', 'O', 'N', 'K', '0', '1'};
constexpr size_t MonkHeader = 8 + 4 + 8, MonkEntry = 4 + 8 + 64 + 4 + 4, MonkMaxSlots = 4096;
uint64_t Fnv(std::span<const uint8_t> bytes) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (const uint8_t byte : bytes) hash = (hash ^ byte) * 0x100000001B3ull;
    return hash;
}
// Hides the values from casual reading; the checksum, not this, rejects edited files.
void Scramble(std::span<uint8_t> body) {
    uint64_t state = 0x9E3779B97F4A7C15ull;
    for (auto& byte : body) {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        byte ^= uint8_t(state >> 32);
    }
}
template <class T> void Put(std::vector<uint8_t>& out, T value) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(value));
}
template <class T> T Get(const uint8_t* bytes) { T value{}; std::memcpy(&value, bytes, sizeof(value)); return value; }
}

std::string EncodeMonkSlots(std::span<const MonkSlot> slots) {
    std::vector<uint8_t> body;
    body.reserve(slots.size() * MonkEntry);
    for (const auto& slot : slots) {
        Put<uint32_t>(body, slot.slot);
        Put<uint64_t>(body, slot.print);
        for (size_t byte = 0; byte < 64; ++byte) {
            uint8_t bits = 0;
            for (size_t bit = 0; bit < 8; ++bit) if (slot.rooms.test(byte * 8 + bit)) bits |= uint8_t(1u << bit);
            body.push_back(bits);
        }
        Put<int32_t>(body, slot.health[0]);
        Put<int32_t>(body, slot.health[1]);
    }
    Scramble(body);
    std::vector<uint8_t> out(MonkMagic, MonkMagic + sizeof(MonkMagic));
    Put<uint32_t>(out, uint32_t(slots.size()));
    Put<uint64_t>(out, Fnv(body));
    out.insert(out.end(), body.begin(), body.end());
    return std::string(out.begin(), out.end());
}

bool DecodeMonkSlots(std::span<const uint8_t> file, std::vector<MonkSlot>& slots) {
    if (file.size() < MonkHeader || std::memcmp(file.data(), MonkMagic, sizeof(MonkMagic))) return false;
    const uint32_t count = Get<uint32_t>(file.data() + 8);
    if (count > MonkMaxSlots || file.size() != MonkHeader + size_t(count) * MonkEntry) return false;
    std::vector<uint8_t> body(file.begin() + MonkHeader, file.end());
    if (Get<uint64_t>(file.data() + 12) != Fnv(body)) return false;
    Scramble(body);
    std::vector<MonkSlot> decoded(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* entry = body.data() + size_t(i) * MonkEntry;
        auto& slot = decoded[i];
        slot.slot = Get<uint32_t>(entry);
        slot.print = Get<uint64_t>(entry + 4);
        for (size_t bit = 0; bit < 512; ++bit) if (entry[12 + bit / 8] & (1u << (bit % 8))) slot.rooms.set(bit);
        slot.health = {Get<int32_t>(entry + 76), Get<int32_t>(entry + 80)};
    }
    slots = std::move(decoded);
    return true;
}

bool ParseMonkText(std::string_view text, std::vector<MonkSlot>& slots) {
    slots.clear();
    bool complete = true;
    std::istringstream lines{std::string(text)};
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        std::istringstream fields(line);
        MonkSlot slot;
        std::string rooms;
        if (!(fields >> slot.slot >> slot.print >> rooms)) { complete = false; continue; }
        slot.rooms = ParseRooms(rooms);
        int32_t first = -1, second = -1;
        if (fields >> first) { slot.health[0] = first; if (fields >> second) slot.health[1] = second; }
        slots.push_back(slot);
    }
    return complete;
}
}
