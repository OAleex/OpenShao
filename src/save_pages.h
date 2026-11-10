#pragma once
// Ten pages of nine native slots: page 1 is the native file, pages 2..10 live beside it.
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct X86Context;
namespace input { struct Pad; }

namespace openshao::save_pages {
constexpr uint32_t PageCount = 10, SlotsPerPage = 9;
constexpr uint32_t SlotBase = 0x131C, RecordBytes = 0x51AC, RegionBytes = SlotsPerPage * RecordBytes;
constexpr uint32_t EmptyMarker = 0xD8;  // AI override field, -1 in a never-used record

std::vector<uint8_t> EmptyRegion();
std::string EncodePage(uint32_t page, std::span<const uint8_t> region);
// Accepts only a well-formed file for that page; the region is untouched on failure.
bool DecodePage(std::span<const uint8_t> file, uint32_t page, std::vector<uint8_t>& region);
uint32_t Step(uint32_t page, int delta);  // wraps around 1..PageCount
uint32_t GlobalSlot(uint32_t page, uint32_t slot);  // 1..PageCount*SlotsPerPage, 0 when invalid
// Turns one slot of a page region into a never-used native record.
bool ClearSlot(std::span<uint8_t> region, uint32_t slot);

// Edge detection for one analog button: true once per press.
class Edge {
public:
    bool Update(uint8_t value);
    void Resume(uint8_t value);  // a button already held when a list opens waits for its release
private:
    bool held_ = true;
};

// Edge detection for the two triggers; returns -1, +1 or 0 once per press.
class Triggers {
public:
    int Update(uint8_t left, uint8_t right);
    // A list that opens again ignores a trigger that is already held (it blocks in gameplay).
    void Resume(uint8_t left, uint8_t right);
private:
    bool left_ = true, right_ = true;  // true while held (or not yet seen released)
};

// Which page the save buffer holds, the cached content of the others and the page the player wants.
class Book {
public:
    using Loader = std::function<std::optional<std::vector<uint8_t>>(uint32_t page)>;
    explicit Book(Loader loader = {}) : loader_(std::move(loader)) {}
    uint32_t Shown() const { return shown_; }
    uint32_t Wanted() const { return wanted_; }
    void Want(uint32_t page) { if (page >= 1 && page <= PageCount) wanted_ = page; }
    // Puts the wanted page into the buffer region; true when the region changed.
    bool Ensure(std::span<uint8_t> region);
    // The buffer holds the native file; other pages keep their cache.
    void AfterRead() { shown_ = 1; pages_[1].reset(); }
    // A failed read may leave another page in the buffer; page 1 is put back before it counts as page 1.
    void ReadFailed(std::span<uint8_t> region);
    // The shown page changed outside a native write (a deleted slot): keep the new content.
    void Commit(std::span<const uint8_t> region);
    // Writing with another page shown: the region temporarily gets page 1; returns the page to store.
    std::optional<std::pair<uint32_t, std::vector<uint8_t>>> BeginWrite(std::span<uint8_t> region);
    void EndWrite(std::span<uint8_t> region);
private:
    const std::vector<uint8_t>& Page(uint32_t page);
    Loader loader_;
    std::array<std::optional<std::vector<uint8_t>>, PageCount + 1> pages_{};
    uint32_t shown_ = 1, wanted_ = 1;
    bool writing_ = false;
};

// Runtime glue used by the difficulty save hooks.
uint32_t ShownPage();
void BeforeCopyIn(X86Context& ctx, uint8_t* base, uint32_t caller);
void AfterCopyIn(uint32_t caller);
void BeforeCopyOut(X86Context& ctx, uint8_t* base);
void AfterCopyOut();
void HoldAnswer(::input::Pad& pad);
}
