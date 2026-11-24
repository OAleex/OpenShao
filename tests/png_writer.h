#pragma once
#include <cstdint>
#include <string>
#include <vector>

// A PNG writer for the tests: RGBA, 8 bits, no compression (stored deflate blocks). `extra` adds a text chunk, which changes the file
// and not the pixels, as saving the same picture again with another program does.
namespace pngtest {
inline uint32_t Crc(const uint8_t* data, size_t size, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

inline void Put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(uint8_t(x >> s));
}

inline void Chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body) {
    Put32(out, uint32_t(body.size()));
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), body.begin(), body.end());
    out.insert(out.end(), td.begin(), td.end());
    Put32(out, Crc(td.data(), td.size()));
}

inline std::vector<uint8_t> Png(uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, const std::string& extra = {}) {
    std::vector<uint8_t> out{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    Put32(ihdr, w);
    Put32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8 bits, RGBA, deflate, no filter method, no interlace
    Chunk(out, "IHDR", ihdr);
    if (!extra.empty()) Chunk(out, "tEXt", std::vector<uint8_t>(extra.begin(), extra.end()));
    std::vector<uint8_t> raw;
    for (uint32_t y = 0; y < h; ++y) {
        raw.push_back(0);  // filter: none
        raw.insert(raw.end(), rgba.begin() + size_t(y) * w * 4, rgba.begin() + size_t(y + 1) * w * 4);
    }
    std::vector<uint8_t> z{0x78, 0x01};
    for (size_t pos = 0; pos < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(uint8_t(n & 0xFF));
        z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n & 0xFF));
        z.push_back(uint8_t((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    Put32(z, (b << 16) | a);
    Chunk(out, "IDAT", z);
    Chunk(out, "IEND", {});
    return out;
}
}
