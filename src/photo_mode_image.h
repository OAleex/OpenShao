#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace openshao::photo_mode {
struct Photo {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgb;
};

// The renderer copies the game frame at render size; the screen stretches it to the display aspect.
inline Photo DisplayPhoto(const std::vector<uint8_t>& bgra, uint32_t width, uint32_t height, float aspect) {
    Photo photo;
    if (!width || !height || bgra.size() < size_t(width) * height * 4) return photo;
    uint32_t shown = width;
    if (std::isfinite(aspect) && aspect > 0) shown = std::max(1u, uint32_t(std::lround(height * aspect)));
    photo.width = shown;
    photo.height = height;
    photo.rgb.resize(size_t(shown) * height * 3);
    const float step = float(width) / float(shown);
    for (uint32_t x = 0; x < shown; ++x) {
        const float source = std::clamp((x + .5f) * step - .5f, 0.f, float(width - 1));
        const uint32_t left = uint32_t(source), right = std::min(left + 1, width - 1);
        const float blend = source - float(left);
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* a = &bgra[(size_t(y) * width + left) * 4];
            const uint8_t* b = &bgra[(size_t(y) * width + right) * 4];
            uint8_t* out = &photo.rgb[(size_t(y) * shown + x) * 3];
            for (unsigned c = 0; c < 3; ++c)
                out[c] = uint8_t(std::lround(a[2 - c] + (b[2 - c] - a[2 - c]) * blend));
        }
    }
    return photo;
}

// Box-filtered copy at most max_width wide, for the saved-photo notification.
inline Photo Thumbnail(const Photo& photo, uint32_t max_width) {
    Photo thumb;
    if (!photo.width || !photo.height || !max_width || photo.rgb.size() < size_t(photo.width) * photo.height * 3) return thumb;
    thumb.width = std::min(photo.width, max_width);
    thumb.height = std::max(1u, uint32_t(std::lround(double(photo.height) * thumb.width / photo.width)));
    thumb.rgb.resize(size_t(thumb.width) * thumb.height * 3);
    for (uint32_t y = 0; y < thumb.height; ++y) {
        const uint32_t top = uint32_t(uint64_t(y) * photo.height / thumb.height);
        const uint32_t bottom = std::max(top + 1, uint32_t(uint64_t(y + 1) * photo.height / thumb.height));
        for (uint32_t x = 0; x < thumb.width; ++x) {
            const uint32_t left = uint32_t(uint64_t(x) * photo.width / thumb.width);
            const uint32_t right = std::max(left + 1, uint32_t(uint64_t(x + 1) * photo.width / thumb.width));
            uint64_t sum[3] = {};
            for (uint32_t sy = top; sy < bottom; ++sy)
                for (uint32_t sx = left; sx < right; ++sx)
                    for (unsigned c = 0; c < 3; ++c) sum[c] += photo.rgb[(size_t(sy) * photo.width + sx) * 3 + c];
            const uint64_t count = uint64_t(bottom - top) * (right - left);
            for (unsigned c = 0; c < 3; ++c)
                thumb.rgb[(size_t(y) * thumb.width + x) * 3 + c] = uint8_t((sum[c] + count / 2) / count);
        }
    }
    return thumb;
}

// Drops whole UTF-8 characters from the middle until fits() accepts the text, keeping its start and its end.
template <class Fits> std::string EllipsizeMiddle(const std::string& text, Fits fits) {
    if (fits(text)) return text;
    std::vector<size_t> starts;
    for (size_t i = 0; i < text.size(); ++i) if ((uint8_t(text[i]) & 0xC0) != 0x80) starts.push_back(i);
    for (size_t keep = starts.size(); keep-- > 0;) {
        const size_t head = (keep + 1) / 2, tail = keep - head;
        std::string result = text.substr(0, head < starts.size() ? starts[head] : text.size()) + "...";
        if (tail) result += text.substr(starts[starts.size() - tail]);
        if (fits(result)) return result;
    }
    return "...";
}

// OpenShao_2026-11-18_13-05-12.png, then _2, _3 for photos taken within the same second.
inline std::string PhotoName(const std::tm& time, unsigned index) {
    char name[64];
    std::snprintf(name, sizeof name, "OpenShao_%04d-%02d-%02d_%02d-%02d-%02d", time.tm_year + 1900, time.tm_mon + 1,
                  time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
    std::string result = name;
    if (index > 1) result += "_" + std::to_string(index);
    return result + ".png";
}
}
