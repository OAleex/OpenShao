#include "save_pages.h"
#include <algorithm>
#include <cstring>

namespace openshao::save_pages {
namespace {
constexpr char Magic[8] = {'O', 'S', 'P', 'A', 'G', 'E', '0', '1'};
constexpr size_t HeaderBytes = 8 + 4 + 4 + 8;
uint64_t Hash(std::span<const uint8_t> bytes) {
    uint64_t value = 1469598103934665603ull;
    for (const uint8_t byte : bytes) value = (value ^ byte) * 1099511628211ull;
    return value;
}
template <class T> void Put(std::string& out, T value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}
template <class T> T Get(std::span<const uint8_t> bytes, size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
}

std::vector<uint8_t> EmptyRegion() {
    std::vector<uint8_t> region(RegionBytes, 0);
    for (uint32_t slot = 0; slot < SlotsPerPage; ++slot)
        std::fill_n(region.begin() + slot * RecordBytes + EmptyMarker, 4, uint8_t(0xFF));
    return region;
}

std::string EncodePage(uint32_t page, std::span<const uint8_t> region) {
    std::string out(Magic, sizeof(Magic));
    Put<uint32_t>(out, page);
    Put<uint32_t>(out, uint32_t(region.size()));
    Put<uint64_t>(out, Hash(region));
    out.append(reinterpret_cast<const char*>(region.data()), region.size());
    return out;
}

bool DecodePage(std::span<const uint8_t> file, uint32_t page, std::vector<uint8_t>& region) {
    if (file.size() != HeaderBytes + RegionBytes || std::memcmp(file.data(), Magic, sizeof(Magic))) return false;
    if (Get<uint32_t>(file, 8) != page || Get<uint32_t>(file, 12) != RegionBytes) return false;
    const auto body = file.subspan(HeaderBytes);
    if (Get<uint64_t>(file, 16) != Hash(body)) return false;
    region.assign(body.begin(), body.end());
    return true;
}

uint32_t Step(uint32_t page, int delta) {
    const int count = int(PageCount);
    const int index = (int(std::clamp<uint32_t>(page, 1, PageCount)) - 1 + delta % count + count) % count;
    return uint32_t(index + 1);
}

uint32_t GlobalSlot(uint32_t page, uint32_t slot) {
    if (page < 1 || page > PageCount || slot < 1 || slot > SlotsPerPage) return 0;
    return (page - 1) * SlotsPerPage + slot;
}

namespace { constexpr uint8_t Press = 0x80, Rest = 0x30; }

bool ClearSlot(std::span<uint8_t> region, uint32_t slot) {
    if (region.size() != RegionBytes || slot < 1 || slot > SlotsPerPage) return false;
    const auto record = region.subspan((slot - 1) * RecordBytes, RecordBytes);
    std::fill(record.begin(), record.end(), uint8_t(0));
    std::fill_n(record.begin() + EmptyMarker, 4, uint8_t(0xFF));
    return true;
}

bool Edge::Update(uint8_t value) {
    if (held_) { held_ = value > Rest; return false; }
    if (value < Press) return false;
    held_ = true;
    return true;
}
void Edge::Resume(uint8_t value) { held_ = value > Rest; }

void Triggers::Resume(uint8_t left, uint8_t right) {
    left_ = left > Rest;
    right_ = right > Rest;
}

int Triggers::Update(uint8_t left, uint8_t right) {
    int delta = 0;
    if (left_) left_ = left > Rest; else if (left >= Press) { left_ = true; delta -= 1; }
    if (right_) right_ = right > Rest; else if (right >= Press) { right_ = true; delta += 1; }
    return delta;
}

const std::vector<uint8_t>& Book::Page(uint32_t page) {
    auto& entry = pages_[page];
    if (!entry) {
        if (loader_) entry = loader_(page);
        if (!entry || entry->size() != RegionBytes) entry = EmptyRegion();
    }
    return *entry;
}

bool Book::Ensure(std::span<uint8_t> region) {
    if (writing_ || region.size() != RegionBytes || wanted_ == shown_) return false;
    pages_[shown_].emplace(region.begin(), region.end());
    const auto& target = Page(wanted_);
    std::copy(target.begin(), target.end(), region.begin());
    shown_ = wanted_;
    return true;
}

void Book::Commit(std::span<const uint8_t> region) {
    if (shown_ != 1 && region.size() == RegionBytes) pages_[shown_].emplace(region.begin(), region.end());
}

void Book::ReadFailed(std::span<uint8_t> region) {
    if (shown_ != 1 && pages_[1] && region.size() == RegionBytes)
        std::copy(pages_[1]->begin(), pages_[1]->end(), region.begin());
    shown_ = 1;
}

std::optional<std::pair<uint32_t, std::vector<uint8_t>>> Book::BeginWrite(std::span<uint8_t> region) {
    if (shown_ == 1 || region.size() != RegionBytes || !pages_[1]) return std::nullopt;
    std::vector<uint8_t> content(region.begin(), region.end());
    pages_[shown_] = content;
    std::copy(pages_[1]->begin(), pages_[1]->end(), region.begin());
    writing_ = true;
    return std::pair{shown_, std::move(content)};
}

void Book::EndWrite(std::span<uint8_t> region) {
    if (!writing_) return;
    writing_ = false;
    const auto& content = *pages_[shown_];
    std::copy(content.begin(), content.end(), region.begin());
}
}
