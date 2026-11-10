#include "save_pages.h"
#include <cstdio>
#include <cstring>
#include <map>

using namespace openshao::save_pages;
namespace {
unsigned failures = 0;
void Check(bool value, const char* text) { if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", text); } }
std::vector<uint8_t> Filled(uint8_t value) { return std::vector<uint8_t>(RegionBytes, value); }
uint32_t Marker(const std::vector<uint8_t>& region, uint32_t slot) {
    uint32_t value = 0;
    std::memcpy(&value, region.data() + slot * RecordBytes + EmptyMarker, 4);
    return value;
}
}

int main() {
    const auto empty = EmptyRegion();
    Check(empty.size() == RegionBytes && Marker(empty, 0) == 0xFFFFFFFFu && Marker(empty, 8) == 0xFFFFFFFFu &&
          empty[0x84] == 0 && empty[0] == 0, "an empty page holds nine never-used native records");

    const auto page = Filled(7);
    const auto file = EncodePage(4, page);
    std::vector<uint8_t> decoded, bytes(file.begin(), file.end());
    Check(DecodePage(bytes, 4, decoded) && decoded == page, "a page file round-trips");
    Check(!DecodePage(bytes, 5, decoded), "a page file only loads as its own page");
    bytes[bytes.size() - 1] ^= 1;
    decoded.clear();
    Check(!DecodePage(bytes, 4, decoded) && decoded.empty(), "a damaged page file is refused");
    Check(!DecodePage({}, 4, decoded), "an empty file is refused");

    Check(Step(1, -1) == 10 && Step(10, 1) == 1 && Step(3, 1) == 4 && Step(3, -1) == 2,
          "paging wraps around the ten pages");
    Check(GlobalSlot(1, 1) == 1 && GlobalSlot(1, 9) == 9 && GlobalSlot(2, 1) == 10 && GlobalSlot(10, 9) == 90 &&
          !GlobalSlot(0, 1) && !GlobalSlot(1, 10), "page 1 keeps the native slot numbers");

    Triggers triggers;
    Check(triggers.Update(255, 0) == 0, "a trigger held before the first frame does nothing");
    Check(triggers.Update(0, 0) == 0 && triggers.Update(255, 0) == -1 && triggers.Update(255, 0) == 0,
          "the left trigger turns back once per press");
    Check(triggers.Update(0, 200) == 1 && triggers.Update(0, 90) == 0 && triggers.Update(0, 0) == 0 &&
          triggers.Update(0, 140) == 1, "the right trigger turns forward once per press");
    triggers.Resume(0, 255);
    Check(triggers.Update(0, 255) == 0 && triggers.Update(0, 0) == 0 && triggers.Update(0, 255) == 1,
          "a trigger still held when a list opens again waits for its release");
    triggers.Resume(0, 0);
    Check(triggers.Update(255, 0) == -1, "a released trigger works on the first frame after a list opens");

    std::map<uint32_t, std::vector<uint8_t>> disk{{2, Filled(2)}};
    Book book([&](uint32_t number) -> std::optional<std::vector<uint8_t>> {
        if (const auto found = disk.find(number); found != disk.end()) return found->second;
        return std::nullopt;
    });
    auto buffer = Filled(1);
    Check(!book.Ensure(buffer) && book.Shown() == 1, "page 1 is the native buffer");
    book.Want(2);
    Check(book.Ensure(buffer) && buffer == Filled(2) && book.Shown() == 2, "a stored page is swapped in");
    book.Want(3);
    Check(book.Ensure(buffer) && buffer == empty, "a page never saved shows empty slots");
    buffer[5] = 9;  // a save into page 3
    auto pending = book.BeginWrite(buffer);
    Check(pending && pending->first == 3 && pending->second[5] == 9 && buffer == Filled(1),
          "the native write receives page 1 and the shown page is handed back for its own file");
    book.Want(1);
    Check(!book.Ensure(buffer), "no swap happens in the middle of a native write");
    book.EndWrite(buffer);
    Check(buffer[5] == 9 && book.Shown() == 3, "the shown page returns after the write");
    Check(book.Ensure(buffer) && buffer == Filled(1), "page 1 comes back unchanged");
    book.Want(3);
    Check(book.Ensure(buffer) && buffer[5] == 9, "a page keeps its new save in memory");

    buffer = Filled(4);  // the native file is read again (or a new, empty buffer)
    book.AfterRead();
    Check(book.Shown() == 1 && book.Wanted() == 3, "after a native read the buffer is page 1 but the wanted page stays");
    Check(book.Ensure(buffer) && buffer[5] == 9, "the wanted page is restored after the read");
    book.Want(1);
    Check(book.Ensure(buffer) && buffer == Filled(4), "page 1 reflects the file just read");
    Check(!book.BeginWrite(buffer), "a page 1 write goes to the native file unchanged");

    book.Want(2);
    Check(book.Ensure(buffer) && buffer == Filled(2), "page 2 is shown again");
    book.ReadFailed(buffer);
    Check(book.Shown() == 1 && buffer == Filled(4), "a failed read never leaves another page posing as page 1");
    // A session that ends with page 10 shown and a new, empty buffer must not lose page 10.
    book.Want(10);
    book.Ensure(buffer);
    buffer[7] = 3;
    auto saved = book.BeginWrite(buffer);
    book.EndWrite(buffer);
    buffer = Filled(0);
    book.AfterRead();
    Check(saved && saved->first == 10 && book.Ensure(buffer) && buffer[7] == 3,
          "a page saved before the buffer is replaced comes back with its save");
    // Deleting a slot on a page other than 1 keeps the new content for that page.
    book.Want(4);
    book.Ensure(buffer);
    buffer[RecordBytes + 0x84] = 1;
    buffer[RecordBytes + 0x10] = 5;
    Check(ClearSlot(buffer, 2) && buffer[RecordBytes + 0x84] == 0 && buffer[RecordBytes + 0x10] == 0 &&
          Marker(buffer, 1) == 0xFFFFFFFFu, "a deleted slot becomes a never-used record");
    Check(!ClearSlot(buffer, 0) && !ClearSlot(buffer, 10), "only slots 1 to 9 can be deleted");
    book.Commit(buffer);
    book.Want(1);
    book.Ensure(buffer);
    buffer = Filled(0);
    book.AfterRead();
    book.Want(4);
    Check(book.Ensure(buffer) && Marker(buffer, 1) == 0xFFFFFFFFu, "a deleted slot stays deleted after the buffer changes");

    Edge x;
    Check(!x.Update(255) && !x.Update(0) && x.Update(255) && !x.Update(255), "X answers once per press");
    x.Resume(255);
    Check(!x.Update(255) && !x.Update(0) && x.Update(200), "X held when a list opens waits for its release");
    book.Want(0);
    book.Want(11);
    Check(book.Wanted() == 4, "only pages 1 to 10 can be wanted");

    if (failures) return 1;
    std::puts("save pages: all checks passed");
    return 0;
}
