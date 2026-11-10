#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "netplay/state_hash.h"

using namespace openshao::netplay;
static unsigned failures = 0;
static void Check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}
// Synthetic guest memory: only the listed pages exist.
struct Fake {
    std::map<uint32_t, std::vector<uint8_t>> pages;
    void Commit(uint32_t va, uint8_t fill) { pages[va].assign(kHashPage, fill); }
    MemoryView View(uint32_t image_begin = 0x10000, uint32_t image_end = 0x10000) {
        return {[this](uint32_t va) -> const uint8_t* {
                    const auto it = pages.find(va);
                    return it == pages.end() ? nullptr : it->second.data();
                }, image_begin, image_end};
    }
};
static const BlockHash* Find(const std::vector<BlockHash>& blocks, uint32_t va) {
    for (const auto& b : blocks) if (b.va == va) return &b;
    return nullptr;
}
int main() {
    auto text = [](const char* s) { return Xxh64(s, std::strlen(s)); };
    Check(text("") == 0xEF46DB3751D8E999ull, "XXH64 of the empty input matches the reference");
    Check(text("a") == 0xD24EC4F1A98C6E5Bull, "XXH64 of one byte matches the reference");
    Check(text("abc") == 0x44BC2CF5AD770999ull, "XXH64 of a short input matches the reference");
    Check(text("Nobody inspects the spammish repetition") == 0xFBCEA83C8A378BF1ull,
          "XXH64 of a 39-byte input (four-lane path) matches the reference");
    Check(Xxh64("abc", 3, 1) != Xxh64("abc", 3, 0), "the seed changes the hash");

    Fake memory;
    memory.Commit(0x00010000, 1);       // image
    memory.Commit(0x00400000, 2);       // allocation
    memory.Commit(0x80000000, 3);       // kernel data
    memory.Commit(0x80001000, 3);
    memory.Commit(0x80200000, 4);       // physical
    memory.Commit(0xB0000000, 5);       // system
    memory.Commit(0xD0010000, 6);       // stack
    memory.Commit(0xF0000000, 7);       // physical alias: never scanned
    memory.Commit(0xC0000000, 8);       // outside every range
    auto blocks = HashBlocks(memory.View(0x10000, 0x20000));
    std::set<uint32_t> found;
    for (const auto& b : blocks) found.insert(b.va);
    Check(found == std::set<uint32_t>{0x10000, 0x400000, 0x80000000, 0x80200000, 0xB0000000, 0xD0010000},
          "only committed blocks inside the scanned ranges are hashed, without the 0xF0000000 alias");
    Check(Find(blocks, 0x10000)->kind == RegionKind::Image && Find(blocks, 0x400000)->kind == RegionKind::Virtual &&
          Find(blocks, 0x80000000)->kind == RegionKind::Kernel && Find(blocks, 0x80200000)->kind == RegionKind::Physical &&
          Find(blocks, 0xB0000000)->kind == RegionKind::System && Find(blocks, 0xD0010000)->kind == RegionKind::Stack,
          "each block is labeled with its region");
    Check(std::string(KindName(RegionKind::Kernel)) == "kernel", "region names are stable report keys");

    const auto baseline = blocks;
    const auto total = Combine(blocks);
    Check(Combine(HashBlocks(memory.View(0x10000, 0x20000))) == total, "hashing unchanged memory is repeatable");

    memory.pages[0x80200000][123] ^= 1;
    blocks = HashBlocks(memory.View(0x10000, 0x20000));
    unsigned changed = 0;
    for (size_t i = 0; i < blocks.size(); ++i) changed += blocks[i].hash != baseline[i].hash;
    Check(changed == 1 && Find(blocks, 0x80200000)->hash != Find(baseline, 0x80200000)->hash && Combine(blocks) != total,
          "one changed byte changes exactly its own block and the total");
    memory.pages[0x80200000][123] ^= 1;

    // Same bytes, different committed page: the commit set is part of the hash.
    memory.Commit(0x80001000 + 0x1000, 0);
    blocks = HashBlocks(memory.View(0x10000, 0x20000));
    Check(Find(blocks, 0x80000000)->hash != Find(baseline, 0x80000000)->hash, "committing a zero page changes its block");
    memory.pages.erase(0x80002000);

    // Moving content between pages of one block is a difference.
    memory.pages.erase(0x80001000);
    memory.Commit(0x80003000, 3);
    blocks = HashBlocks(memory.View(0x10000, 0x20000));
    Check(Find(blocks, 0x80000000)->hash != Find(baseline, 0x80000000)->hash, "the page position is part of the block hash");

    std::printf("Netplay state hash: %u failures\n", failures);
    return failures ? 1 : 0;
}
