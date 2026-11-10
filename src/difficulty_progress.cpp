#include "difficulty.h"

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
}
