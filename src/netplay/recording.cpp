#include "netplay/recording.h"
#include <array>
#include <cstring>

namespace openshao::netplay {
namespace {
constexpr char kMagic[8] = {'O', 'S', 'N', 'R', 'E', 'C', 0x1A, 0};
constexpr uint32_t kBlockTag = 0x304B4C42;   // "BLK0"
constexpr uint32_t kFooterTag = 0x30444E45;  // "END0"
const std::array<uint32_t, 256> kCrcTable = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = c & 1 ? 0x82F63B78u ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    return table;
}();
void Put32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (i * 8)));
}
uint32_t Get32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
std::FILE* OpenFile(const std::filesystem::path& path, bool write) {
#ifdef _WIN32
    return _wfopen(path.c_str(), write ? L"wb" : L"rb");
#else
    return std::fopen(path.c_str(), write ? "wb" : "rb");
#endif
}
}  // namespace

uint32_t Crc32c(const void* data, size_t size, uint32_t crc) {
    crc = ~crc;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) crc = kCrcTable[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

RecordingWriter::~RecordingWriter() {
    if (file_) std::fclose(file_);
}

bool RecordingWriter::Open(const std::filesystem::path& path, const std::string& metadata, std::string& error) {
    if (metadata.size() > kMaxMetadata) { error = "recording metadata is too large"; return false; }
    file_ = OpenFile(path, true);
    if (!file_) { error = "cannot create the recording file"; return false; }
    std::vector<uint8_t> head(kMagic, kMagic + 8);
    Put32(head, kRecordingVersion);
    Put32(head, uint32_t(metadata.size()));
    head.insert(head.end(), metadata.begin(), metadata.end());
    Put32(head, Crc32c(metadata.data(), metadata.size()));
    if (std::fwrite(head.data(), 1, head.size(), file_) != head.size() || std::fflush(file_)) {
        error = "cannot write the recording header";
        std::fclose(file_); file_ = nullptr;
        return false;
    }
    return true;
}

bool RecordingWriter::Append(const FrameSample& frame) {
    if (!file_ || failed_) return false;
    if (pending_.empty()) block_first_ = frames_;
    const auto at = pending_.size();
    pending_.resize(at + kFrameBytes);
    Encode(frame, pending_.data() + at);
    ++frames_;
    return pending_.size() / kFrameBytes < kBlockFrames || FlushBlock();
}

bool RecordingWriter::FlushBlock() {
    if (pending_.empty()) return true;
    std::vector<uint8_t> block;
    block.reserve(pending_.size() + 16);
    Put32(block, kBlockTag);
    Put32(block, block_first_);
    Put32(block, uint32_t(pending_.size() / kFrameBytes));
    block.insert(block.end(), pending_.begin(), pending_.end());
    const uint32_t crc = Crc32c(block.data(), block.size());
    Put32(block, crc);
    chain_ = Crc32c(&crc, 4, chain_);
    pending_.clear();
    // Flush each block so a crash keeps every completed block readable for diagnosis.
    if (std::fwrite(block.data(), 1, block.size(), file_) != block.size() || std::fflush(file_)) failed_ = true;
    return !failed_;
}

bool RecordingWriter::Finish(std::string& error) {
    if (!file_) { error = "the recording is not open"; return false; }
    bool ok = FlushBlock();
    std::vector<uint8_t> footer;
    Put32(footer, kFooterTag);
    Put32(footer, frames_);
    Put32(footer, chain_);
    Put32(footer, Crc32c(footer.data(), footer.size()));
    ok = ok && !failed_ && std::fwrite(footer.data(), 1, footer.size(), file_) == footer.size();
    ok = std::fclose(file_) == 0 && ok;
    file_ = nullptr;
    if (!ok) error = "cannot complete the recording file";
    return ok;
}

RecordingReader::~RecordingReader() = default;

bool RecordingReader::Open(const std::filesystem::path& path, std::string& error) {
    metadata_.clear(); frames_.clear();
    std::FILE* file = OpenFile(path, false);
    if (!file) { error = "cannot open the recording"; return false; }
    std::vector<uint8_t> data;
    uint8_t chunk[1 << 16];
    for (size_t n; (n = std::fread(chunk, 1, sizeof(chunk), file)) > 0;) data.insert(data.end(), chunk, chunk + n);
    const bool read_error = std::ferror(file) != 0;
    std::fclose(file);
    if (read_error) { error = "cannot read the recording"; return false; }
    size_t at = 0;
    auto need = [&](size_t n) { return data.size() - at >= n; };
    if (!need(16) || std::memcmp(data.data(), kMagic, 8)) { error = "not an OpenShao recording"; return false; }
    if (Get32(&data[8]) != kRecordingVersion) { error = "unsupported recording version"; return false; }
    const uint32_t meta = Get32(&data[12]);
    at = 16;
    if (meta > kMaxMetadata || !need(size_t(meta) + 4)) { error = "truncated recording header"; return false; }
    if (Crc32c(&data[at], meta) != Get32(&data[at + meta])) { error = "recording header checksum mismatch"; return false; }
    metadata_.assign(reinterpret_cast<const char*>(&data[at]), meta);
    at += size_t(meta) + 4;
    uint32_t chain = 0;
    for (;;) {
        if (!need(4)) { error = "recording has no footer (incomplete)"; return false; }
        const uint32_t tag = Get32(&data[at]);
        if (tag == kFooterTag) {
            if (!need(16) || Crc32c(&data[at], 12) != Get32(&data[at + 12])) { error = "recording footer checksum mismatch"; return false; }
            if (Get32(&data[at + 4]) != Frames()) { error = "recording footer frame count mismatch"; return false; }
            if (Get32(&data[at + 8]) != chain) { error = "recording block chain mismatch"; return false; }
            if (data.size() != at + 16) { error = "data after the recording footer"; return false; }
            return true;
        }
        if (tag != kBlockTag || !need(12)) { error = "corrupt recording block"; return false; }
        const uint32_t first = Get32(&data[at + 4]), count = Get32(&data[at + 8]);
        if (first != Frames() || !count || count > kBlockFrames) { error = "recording blocks out of order"; return false; }
        const size_t size = 12 + size_t(count) * kFrameBytes;
        if (!need(size + 4)) { error = "truncated recording block"; return false; }
        const uint32_t crc = Crc32c(&data[at], size);
        if (crc != Get32(&data[at + size])) { error = "recording block checksum mismatch"; return false; }
        chain = Crc32c(&crc, 4, chain);
        for (uint32_t i = 0; i < count; ++i) {
            FrameSample check;
            if (!Decode(&data[at + 12 + size_t(i) * kFrameBytes], check)) { error = "invalid input sample in recording"; return false; }
        }
        frames_.insert(frames_.end(), data.begin() + std::ptrdiff_t(at + 12), data.begin() + std::ptrdiff_t(at + size));
        at += size + 4;
    }
}

bool RecordingReader::Frame(uint32_t index, FrameSample& out) const {
    if (index >= Frames()) return false;
    return Decode(&frames_[size_t(index) * kFrameBytes], out);
}
}  // namespace openshao::netplay
