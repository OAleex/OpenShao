#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "netplay/frame_input.h"

namespace openshao::netplay {
uint32_t Crc32c(const void* data, size_t size, uint32_t crc = 0);

// .smrec layout, little endian:
//   "OSNREC\x1A\0", u32 version, u32 metadata size, metadata (UTF-8 JSON), u32 CRC32C(metadata)
//   blocks:  u32 'BLK0', u32 first frame, u32 count (1..kBlockFrames), count * kFrameBytes, u32 CRC32C(block)
//   footer:  u32 'END0', u32 frame count, u32 CRC32C of the block CRCs, u32 CRC32C(footer)
// A recording without its footer is incomplete and cannot be replayed.
inline constexpr uint32_t kRecordingVersion = 1;
inline constexpr uint32_t kBlockFrames = 256;
inline constexpr uint32_t kMaxMetadata = 1u << 20;

class RecordingWriter {
public:
    ~RecordingWriter();
    bool Open(const std::filesystem::path& path, const std::string& metadata, std::string& error);
    bool Append(const FrameSample& frame);
    bool Finish(std::string& error);
    uint32_t Frames() const { return frames_; }
    bool IsOpen() const { return file_ != nullptr; }
private:
    bool FlushBlock();
    std::FILE* file_ = nullptr;
    std::vector<uint8_t> pending_;
    uint32_t frames_ = 0, block_first_ = 0, chain_ = 0;
    bool failed_ = false;
};

class RecordingReader {
public:
    ~RecordingReader();
    // Reads the whole file and checks every block and the footer before replay starts.
    bool Open(const std::filesystem::path& path, std::string& error);
    const std::string& Metadata() const { return metadata_; }
    uint32_t Frames() const { return uint32_t(frames_.size() / kFrameBytes); }
    bool Frame(uint32_t index, FrameSample& out) const;
private:
    std::string metadata_;
    std::vector<uint8_t> frames_;
};
}  // namespace openshao::netplay
