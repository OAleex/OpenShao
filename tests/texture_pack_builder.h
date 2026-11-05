#pragma once
#include "mods/sha256.h"
#include "texture_pack_file.h"
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// Builds texture pack files for the tests, as the specification lays them out
using Bytes = std::vector<uint8_t>;

inline void Put(Bytes& b, size_t at, uint64_t value, int size) {
    for (int i = 0; i < size; ++i) b[at + size_t(i)] = uint8_t(value >> (8 * i));
}
inline uint64_t Get(const Bytes& b, size_t at, int size) {
    uint64_t v = 0;
    for (int i = 0; i < size; ++i) v |= uint64_t(b[at + size_t(i)]) << (8 * i);
    return v;
}
inline void Pad(Bytes& b) { while (b.size() % 16) b.push_back(0); }

struct Tex {
    uint64_t hash;
    uint16_t w, h, ow, oh;
    uint8_t levels, format, flags, fill;
};

inline const char* kInfo =
    R"({"id":"Test_Pack-1.0","name":"Test pack","version":"1.0.0","authors":["Someone","Another"],)"
    R"("summary":"A pack for the tests.","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture",)"
    R"("texture_count":3,"description":"Longer text.","website":"https://example.org"})";

// The information of a pack of `count` textures
inline std::string InfoFor(size_t count) {
    return std::string(R"({"id":"Test_Pack-1.0","name":"Test pack","version":"1.0.0","authors":["Someone"],)"
                       R"("summary":"A pack for the tests.","min_recomp":"0.4.1","features":["hdtextures"],"type":"texture",)"
                       R"("texture_count":)") + std::to_string(count) + "}";
}

inline uint64_t Level(uint16_t w, uint16_t h, uint32_t l, uint8_t format) {
    return openshao::texpack::LevelSize(w, h, l, openshao::texpack::BcFormat(format));
}

// A valid pack of three textures and a 2 x 2 logo, laid out as the specification says
inline Bytes Build(const std::vector<Tex>& textures, const std::string& info = kInfo, bool logo = true) {
    Bytes file(128, 0), meta(info.begin(), info.end());
    std::memcpy(file.data(), "OSTPACK\0", 8);
    Put(file, 8, 1, 2);
    Put(file, 10, 0, 2);
    Put(file, 12, 128, 4);
    Bytes logo_bytes;
    if (logo) {
        logo_bytes = {2, 0, 2, 0};
        for (int i = 0; i < 16; ++i) logo_bytes.push_back(uint8_t(i * 10));
    }
    Bytes index, data;
    for (const Tex& t : textures) {
        Bytes e(40, 0);
        Put(e, 0, t.hash, 8);
        Put(e, 8, t.w, 2);
        Put(e, 10, t.h, 2);
        Put(e, 12, t.ow, 2);
        Put(e, 14, t.oh, 2);
        e[16] = t.levels;
        e[17] = t.format;
        e[18] = t.flags;
        uint64_t size = 0;
        for (uint32_t l = 0; l < t.levels; ++l) {
            const uint64_t n = Level(t.w, t.h, l, t.format);
            data.insert(data.end(), size_t(n), uint8_t(t.fill + l));
            size += n;
        }
        Put(e, 24, data.size() - size, 8);
        Put(e, 32, size, 8);
        Pad(data);
        index.insert(index.end(), e.begin(), e.end());
    }
    auto place = [&](const Bytes& part, size_t field) {
        Pad(file);
        Put(file, field, file.size(), 8);
        Put(file, field + 8, part.size(), 8);
        file.insert(file.end(), part.begin(), part.end());
    };
    place(meta, 24);
    if (logo) place(logo_bytes, 40);
    place(index, 56);
    place(data, 72);
    Put(file, 88, textures.size(), 4);
    const std::string hash = openshao::mods::Sha256(index);
    for (size_t i = 0; i < 32; ++i) file[96 + i] = uint8_t(std::stoi(hash.substr(i * 2, 2), nullptr, 16));
    Put(file, 16, file.size(), 8);
    return file;
}

inline void FixIndexHash(Bytes& file) {
    const size_t at = size_t(Get(file, 56, 8)), size = size_t(Get(file, 64, 8));
    const std::string hash = openshao::mods::Sha256(std::span<const uint8_t>(file.data() + at, size));
    for (size_t i = 0; i < 32; ++i) file[96 + i] = uint8_t(std::stoi(hash.substr(i * 2, 2), nullptr, 16));
}

inline std::vector<Tex> Three() {
    return {{0x10, 64, 64, 16, 16, 7, 2, 1, 0x20},   // 4x of a 16 x 16 texture, with alpha
            {0x20, 32, 16, 32, 16, 6, 1, 0, 0x40},   // as large as the game's, BC1
            {0x30, 128, 128, 32, 32, 8, 2, 0, 0x60}};
}

