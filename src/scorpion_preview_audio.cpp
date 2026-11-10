#include "scorpion_preview_audio.h"
#include "scorpion_preview_adx.h"
#include "scorpion_preview_mix.h"
#include "language_pack.h"
#include "restart.h"
#include "kernel/disc.h"
#include "core/log.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openshao::scorpion::audio_preview {
namespace {
constexpr std::array<const char*, 8> kVolumes{
    "GAMEDATA.WAD", "GAMEDATA.WAE", "GAMEDATA.WAF", "GAMEDATA.WAG",
    "GAMEDATA.WAH", "GAMEDATA.WAI", "GAMEDATA.WAJ", "GAMEDATA.WAK"};
constexpr uint32_t kScorpionBank = 7027;
constexpr uint32_t kFramesPerSecond = 48000;

struct Clip {
    std::vector<float> stereo;
    std::atomic<size_t> next_frame{0};
};
std::atomic<std::shared_ptr<Clip>> playing;

uint32_t Little32(std::span<const uint8_t> bytes, size_t offset) {
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
           uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}
bool SameAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char c){return c >= 'A' && c <= 'Z' ? char(c + 'a' - 'A') : c;};
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}
bool ReadGame(std::string_view name, uint64_t at, std::span<uint8_t> out) {
    if (const auto overlay = language::OverlayFile({std::string(name)})) {
        size_t count = 0;
        return overlay->Read(at, out, count) && count == out.size();
    }
    if (const auto* disc = kernel::MountedDisc()) {
        const auto* entry = disc->Find(name);
        return entry && !entry->dir && at <= entry->size && out.size() <= entry->size - at &&
               disc->Read(*entry, at, out.data(), uint32_t(out.size())) == out.size();
    }
    auto source = restart::CurrentGameSource();
    std::error_code ec;
    if (!std::filesystem::is_directory(source, ec)) source = source.parent_path();
    if (source.empty() || !std::filesystem::is_directory(source, ec)) return false;
    auto file = source / std::string(name);
    if (!std::filesystem::is_regular_file(file, ec)) {
        for (const auto& item : std::filesystem::directory_iterator(source, ec)) {
            if (ec) break;
            if (SameAscii(item.path().filename().string(), name) && item.is_regular_file(ec)) {
                file = item.path(); break;
            }
        }
    }
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || at > size || out.size() > size - at) return false;
    std::ifstream input(file, std::ios::binary);
    if (!input || at > uint64_t(INT64_MAX)) return false;
    input.seekg(std::streamoff(at));
    input.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
    return bool(input);
}

bool ReadVoice(unsigned index, std::vector<uint8_t>& adx) {
    if (index >= 4) return false;
    std::array<uint8_t, 32> pwf{};
    if (!ReadGame("GAMEDATA.WAD", 0, pwf) ||
        std::string_view(reinterpret_cast<const char*>(pwf.data()), 4) != "PWF ") return false;
    const uint32_t count = Little32(pwf, 12), stride = Little32(pwf, 16);
    if (count <= kScorpionBank || count > 65536 || stride < 2048 || stride % 2048 != 0 ||
        stride > UINT32_MAX / 8 || Little32(pwf, 20) != 2048) return false;
    std::array<uint8_t, 8> record{};
    if (!ReadGame("GAMEDATA.WAD", 32 + kScorpionBank * 8, record)) return false;
    const uint32_t a = Little32(record, 0), b = Little32(record, 4);
    const bool packed = (b & 0x80000000u) != 0;
    const uint64_t address = packed ? uint64_t(a & 0x1FFFFF) * 2048 : a;
    const uint32_t length = packed ? ((a >> 22) | (b << 10)) & 0x3FFFFF : b;
    const auto volume = address / stride, at = address % stride;
    if (volume >= kVolumes.size() || length < 64 || length > 4 * 1024 * 1024 ||
        uint64_t(length) > stride - at) return false;
    std::array<uint8_t, 40> afs{};
    if (!ReadGame(kVolumes[volume], at, afs) ||
        std::string_view(reinterpret_cast<const char*>(afs.data()), 4) != std::string_view("AFS\0",4) ||
        Little32(afs, 4) < 4) return false;
    const uint32_t offset = Little32(afs, 8 + index * 8);
    const uint32_t size = Little32(afs, 12 + index * 8);
    if (offset < 40 || size < 40 || size > 1024 * 1024 ||
        offset > length || size > length - offset) return false;
    adx.resize(size);
    return ReadGame(kVolumes[volume], at + offset, adx);
}
}

void Stop() { playing.store(nullptr, std::memory_order_release); }

bool Play(unsigned index) {
    Stop();
    std::vector<uint8_t> adx;
    DecodedVoice decoded;
    if (!ReadVoice(index, adx) || !DecodeVoice(adx, decoded)) {
        LOGE("Scorpion preview: original ADX %u could not be read or decoded", index);
        return false;
    }
    const auto clip = std::make_shared<Clip>();
    const auto count = (uint64_t(decoded.samples.size()) * kFramesPerSecond + decoded.sample_rate - 1) / decoded.sample_rate;
    clip->stereo.resize(size_t(count * 2));
    for (size_t frame = 0; frame < count; ++frame) {
        const double position = double(frame) * decoded.sample_rate / kFramesPerSecond;
        const size_t left = std::min(size_t(position), decoded.samples.size() - 1);
        const size_t right = std::min(left + 1, decoded.samples.size() - 1);
        const double fraction = position - left;
        const float sample = float(((1. - fraction) * decoded.samples[left] + fraction * decoded.samples[right]) / 32768.);
        clip->stereo[frame * 2] = sample;
        clip->stereo[frame * 2 + 1] = sample;
    }
    playing.store(clip, std::memory_order_release);
    LOGR("Scorpion preview: ADX %u ready; samples=%zu rate=%u", index, decoded.samples.size(), decoded.sample_rate);
    return true;
}

void Mix(float* pcm, uint32_t frames,unsigned channels) {
    const auto clip = playing.load(std::memory_order_acquire);
    if (!clip || !pcm) return;
    const size_t start = clip->next_frame.fetch_add(frames, std::memory_order_relaxed);
    const size_t remaining = clip->stereo.size() / 2;
    if (start >= remaining) return;
    const size_t limit = std::min<size_t>(frames, remaining - start);
    // Both streams share the system output; reserve headroom for the preview.
    MixMenuVoice(pcm, clip->stereo.data() + start * 2, limit,channels);
    if (start + limit == remaining) LOGR("Scorpion preview: ADX playback completed (%zu frames)", remaining);
}
}
