#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace openshao::frames {
inline constexpr std::array<uint32_t,3> FontAddresses{0x66C0E8,0x6644E0,0x673CF0};
using FontQueue=std::array<uint8_t,0x6408>;
using Fonts=std::array<FontQueue,3>;
inline bool ValidFontQueue(std::span<const uint8_t> bytes,uint32_t address) {
    if(bytes.size()!=0x6408)return false;
    auto read=[&](size_t offset){uint32_t value;std::memcpy(&value,bytes.data()+offset,4);return value;};
    const auto count=read(0),used=read(4);
    if(count>256 || used>0x1000)return false;
    for(uint32_t row=0;row<count;++row) {
        const auto text=read(8+row*0x54+0x34);
        if(text<address+0x5408 || text>=address+0x5408+used)return false;
        const auto offset=text-address;
        if(!std::memchr(bytes.data()+offset,0,0x5408+used-offset))return false;
    }
    return true;
}
}
