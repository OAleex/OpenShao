#pragma once
#include <cstdint>
#include "kernel/memory.h"

namespace openshao::async_load {
constexpr bool GuestSpan(uint32_t address, uint32_t bytes) {
    return address >= mem::kUserLow && bytes && uint64_t(address) + bytes <= 0x100000000ull;
}

inline bool CommittedSpan(uint32_t address, uint32_t bytes) {
    if (!GuestSpan(address, bytes)) return false;
    const uint64_t end = uint64_t(address) + bytes;
    for (uint64_t page = uint64_t(address) & ~uint64_t(mem::kPage - 1); page < end; page += mem::kPage)
        if (!mem::IsCommitted(uint32_t(page))) return false;
    return true;
}

[[noreturn]] void Stop(const char* reason, uint32_t record, uint32_t requested, uint32_t destination);
}
