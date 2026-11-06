#include "texture_pack_file.h"
#include "mods/json.h"
#include "mods/sha256.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>

namespace openshao::texpack {
namespace {
constexpr size_t kHeaderSize = 128, kEntrySize = 40;
constexpr uint64_t kAlign = 16, kMaxMeta = 1u << 20, kMaxLogo = 4 + 256 * 256 * 4, kMaxTextures = 100000;
constexpr uint32_t kMaxSide = 8192, kMaxLevels = 14;
constexpr uint16_t kMajor = 1;

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t U32(const uint8_t* p) { return uint32_t(U16(p)) | uint32_t(U16(p + 2)) << 16; }
uint64_t U64(const uint8_t* p) { return uint64_t(U32(p)) | uint64_t(U32(p + 4)) << 32; }

bool Fail(std::string& error, const char* reason) {
    error = reason;
    return false;
}

struct Section {
    uint64_t offset = 0, size = 0;
};

// Whether [offset, offset + size) lies in a file of `file_size` bytes, without overflow
bool Inside(uint64_t offset, uint64_t size, uint64_t file_size) {
    return offset <= file_size && size <= file_size - offset;
}

bool ReadAt(const std::filesystem::path& path, uint64_t offset, uint8_t* out, uint64_t size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(std::streamoff(offset));
    return bool(in) && bool(in.read(reinterpret_cast<char*>(out), std::streamsize(size)));
}

// Number of code points of well formed UTF-8 without control characters; false when it is not
bool Utf8Length(const std::string& text, size_t& points) {
    points = 0;
    for (size_t i = 0; i < text.size();) {
        const uint8_t c = uint8_t(text[i]);
        size_t extra;
        uint32_t code;
        if (c < 0x80) extra = 0, code = c;
        else if ((c & 0xE0) == 0xC0) extra = 1, code = c & 0x1F;
        else if ((c & 0xF0) == 0xE0) extra = 2, code = c & 0x0F;
        else if ((c & 0xF8) == 0xF0) extra = 3, code = c & 0x07;
        else return false;
        for (size_t k = 1; k <= extra; ++k) {
            if (i + k >= text.size() || (uint8_t(text[i + k]) & 0xC0) != 0x80) return false;
            code = code << 6 | (uint8_t(text[i + k]) & 0x3F);
        }
        const uint32_t least[] = {0, 0x80, 0x800, 0x10000};
        if (code < least[extra] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
        if (code < 0x20 || code == 0x7F) return false;
        i += extra + 1;
        ++points;
    }
    return true;
}

bool Text(const mods::Json& object, const char* key, size_t most, bool required, std::string& out, std::string& error) {
    if (!object.Has(key)) {
        if (required) error = std::string("information: \"") + key + "\" is missing";
        return !required;
    }
    const auto* text = std::get_if<std::string>(&object.At(key).value);
    size_t points = 0;
    if (!text || !Utf8Length(*text, points) || points > most || (required && points == 0)) {
        error = std::string("information: \"") + key + "\" is not valid text within its limit";
        return false;
    }
    out = *text;
    return true;
}

bool List(const mods::Json& object, const char* key, size_t most_items, size_t most_points, std::vector<std::string>& out,
          std::string& error) {
    const auto* items = object.Has(key) ? std::get_if<mods::Json::Array>(&object.At(key).value) : nullptr;
    if (!items || items->empty() || items->size() > most_items) {
        error = std::string("information: \"") + key + "\" is not a list of the allowed size";
        return false;
    }
    for (const auto& item : *items) {
        const auto* text = std::get_if<std::string>(&item.value);
        size_t points = 0;
        if (!text || !Utf8Length(*text, points) || points == 0 || points > most_points) {
            error = std::string("information: an item of \"") + key + "\" is not valid text within its limit";
            return false;
        }
        out.push_back(*text);
    }
    return true;
}

bool IdChars(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '-';
    });
}

bool VersionChars(const std::string& version) {
    // digits separated by two dots, as the versions of the project
    int dots = 0;
    bool digit = false;
    for (char c : version) {
        if (c >= '0' && c <= '9') digit = true;
        else if (c == '.' && digit && dots < 2) ++dots, digit = false;
        else return false;
    }
    return digit && dots == 2;
}

constexpr size_t kMaxPreload = 4096;

// Sixteen hexadecimal digits, as a hash is written everywhere
bool Hash16(const std::string& text, uint64_t& value) {
    if (text.size() != 16) return false;
    value = 0;
    for (char c : text) {
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return false;
        value = value << 4 | uint64_t(digit);
    }
    return true;
}

bool ReadInfo(const std::vector<uint8_t>& bytes, uint32_t count, PackInfo& info, std::string& error) {
    mods::Json root;
    try {
        root = mods::Json::Parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    } catch (const std::exception&) {
        return Fail(error, "information: not valid JSON");
    }
    if (!std::holds_alternative<mods::Json::Object>(root.value)) return Fail(error, "information: not a JSON object");
    std::string type;
    if (!Text(root, "id", 64, true, info.id, error) || !Text(root, "name", 64, true, info.name, error) ||
        !Text(root, "version", 32, true, info.version, error) || !Text(root, "summary", 200, true, info.summary, error) ||
        !Text(root, "min_recomp", 32, true, info.min_recomp, error) || !Text(root, "type", 32, true, type, error) ||
        !List(root, "authors", 16, 64, info.authors, error) || !List(root, "features", 32, 64, info.features, error) ||
        !Text(root, "description", 4000, false, info.description, error) ||
        !Text(root, "website", 256, false, info.website, error) || !Text(root, "license", 128, false, info.license, error) ||
        !Text(root, "credits", 4000, false, info.credits, error))
        return false;
    if (!IdChars(info.id)) return Fail(error, "information: \"id\" has characters outside letters, digits, _ . -");
    if (!VersionChars(info.version) || !VersionChars(info.min_recomp))
        return Fail(error, "information: a version is not three numbers separated by dots");
    if (type != "texture") return Fail(error, "information: \"type\" is not \"texture\"");
    if (std::find(info.features.begin(), info.features.end(), "hdtextures") == info.features.end())
        return Fail(error, "information: \"features\" lacks \"hdtextures\"");
    if (root.Has("preload")) {
        const auto* items = std::get_if<mods::Json::Array>(&root.At("preload").value);
        if (!items || items->size() > kMaxPreload) return Fail(error, "information: \"preload\" is not a list within its limit");
        for (const auto& item : *items) {
            const auto* text = std::get_if<std::string>(&item.value);
            uint64_t hash;
            if (!text || !Hash16(*text, hash)) return Fail(error, "information: \"preload\" has an item that is not a hash of sixteen hexadecimal digits");
            info.preload.push_back(hash);
        }
    }
    if (root.Has("texture_count")) {
        const auto* number = std::get_if<int64_t>(&root.At("texture_count").value);
        if (!number || *number != int64_t(count)) return Fail(error, "information: \"texture_count\" differs from the header");
    }
    return true;
}

bool ReadLogo(const std::vector<uint8_t>& bytes, std::optional<Logo>& logo) {
    if (bytes.size() < 4) return false;
    const uint32_t width = U16(bytes.data()), height = U16(bytes.data() + 2);
    if (!width || !height || width > 256 || height > 256 || bytes.size() != 4 + size_t(width) * height * 4) return false;
    logo = Logo{uint16_t(width), uint16_t(height), std::vector<uint8_t>(bytes.begin() + 4, bytes.end())};
    return true;
}

std::string Hex(const uint8_t* bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < count; ++i) out += {digits[bytes[i] >> 4], digits[bytes[i] & 15]};
    return out;
}

uint32_t Levels(uint32_t width, uint32_t height) {
    uint32_t full = 1;
    for (uint32_t side = std::max(width, height); side > 1; side >>= 1) ++full;
    return full;
}

bool ReadEntries(const std::vector<uint8_t>& bytes, uint32_t count, uint64_t data_size, std::vector<FileEntry>& entries,
                 std::string& error) {
    entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* p = bytes.data() + size_t(i) * kEntrySize;
        FileEntry e;
        e.hash = U64(p);
        e.width = U16(p + 8);
        e.height = U16(p + 10);
        e.orig_width = U16(p + 12);
        e.orig_height = U16(p + 14);
        e.levels = p[16];
        const uint8_t format = p[17], flags = p[18], compression = p[19];
        e.data_offset = U64(p + 24);
        e.data_size = U64(p + 32);
        if (i && e.hash <= entries.back().hash) return Fail(error, "index: hashes are not strictly ascending");
        if (e.width < 4 || e.height < 4 || e.width > kMaxSide || e.height > kMaxSide || (e.width & 3) || (e.height & 3))
            return Fail(error, "index: a texture has a size that is not a multiple of 4 from 4 to 8192");
        if (!e.orig_width || !e.orig_height) return Fail(error, "index: a texture has no original size");
        if (uint64_t(e.width) * e.orig_height != uint64_t(e.height) * e.orig_width)
            return Fail(error, "index: a texture does not have the proportion of the original");
        if (!e.levels || e.levels > kMaxLevels || e.levels > Levels(e.width, e.height))
            return Fail(error, "index: a texture has a number of levels it cannot have");
        if (format != uint8_t(BcFormat::Bc1) && format != uint8_t(BcFormat::Bc3)) return Fail(error, "index: unknown format");
        if (flags & ~1u) return Fail(error, "index: unknown flags");
        if (compression) return Fail(error, "index: compressed data is not supported");
        if (U32(p + 20)) return Fail(error, "index: a reserved field is not zero");
        e.format = BcFormat(format);
        e.alpha = flags & 1;
        uint64_t total = 0;
        for (uint32_t level = 0; level < e.levels; ++level) total += LevelSize(e.width, e.height, level, e.format);
        if (e.data_size != total) return Fail(error, "index: the size of a texture does not match its levels");
        if (e.data_offset % kAlign) return Fail(error, "index: a texture is not aligned");
        if (!Inside(e.data_offset, e.data_size, data_size)) return Fail(error, "index: a texture is outside the data");
        entries.push_back(e);
    }
    std::vector<size_t> order(entries.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return entries[a].data_offset < entries[b].data_offset; });
    for (size_t k = 1; k < order.size(); ++k) {
        const auto& before = entries[order[k - 1]];
        if (entries[order[k]].data_offset < before.data_offset + before.data_size)
            return Fail(error, "index: the data of two textures overlap");
    }
    return true;
}
}

uint64_t LevelSize(uint32_t width, uint32_t height, uint32_t level, BcFormat format) {
    const uint64_t w = std::max(1u, width >> level), h = std::max(1u, height >> level);
    return ((w + 3) / 4) * ((h + 3) / 4) * (format == BcFormat::Bc1 ? 8 : 16);
}

uint32_t FirstLevel(const FileEntry& entry, Quality quality) {
    if (quality == Quality::High) return 0;
    uint32_t skip = 0;
    while ((uint32_t(entry.width) >> skip) > 2u * entry.orig_width && skip + 1 < entry.levels) ++skip;
    return skip;
}

std::unique_ptr<PackFile> PackFile::Open(const std::filesystem::path& path, std::string& error) {
    error.clear();
    std::error_code code;
    const uint64_t file_size = std::filesystem::file_size(path, code);
    if (code) {
        error = "the file cannot be read";
        return nullptr;
    }
    if (file_size < kHeaderSize) {
        error = "header: the file is shorter than a header";
        return nullptr;
    }
    uint8_t header[kHeaderSize];
    if (!ReadAt(path, 0, header, kHeaderSize)) {
        error = "header: cannot be read";
        return nullptr;
    }
    if (std::memcmp(header, "OSTPACK\0", 8)) {
        error = "header: not a texture pack file";
        return nullptr;
    }
    if (U16(header + 8) != kMajor) {
        error = "header: a version of the format this program does not know";
        return nullptr;
    }
    if (U32(header + 12) != kHeaderSize || U32(header + 92) != 0) {
        error = "header: size or flags are not valid";
        return nullptr;
    }
    if (U64(header + 16) != file_size) {
        error = "header: the file size is not the size of the file";
        return nullptr;
    }
    const Section meta{U64(header + 24), U64(header + 32)}, logo{U64(header + 40), U64(header + 48)},
        index{U64(header + 56), U64(header + 64)}, data{U64(header + 72), U64(header + 80)};
    const uint32_t count = U32(header + 88);
    if (count > kMaxTextures || meta.size == 0 || meta.size > kMaxMeta || logo.size > kMaxLogo ||
        index.size != uint64_t(count) * kEntrySize || (count && !data.size) || (!logo.size && logo.offset)) {
        error = "header: a count or size is outside its limit";
        return nullptr;
    }
    uint64_t end = kHeaderSize;
    for (const Section* s : {&meta, &logo, &index, &data}) {
        if (s == &logo && !logo.size) continue;
        if (s == &data && !data.size && !count) continue;
        if (s->offset % kAlign || s->offset < end || !Inside(s->offset, s->size, file_size)) {
            error = "header: a section is misaligned, overlaps another or lies outside the file";
            return nullptr;
        }
        end = s->offset + s->size;
    }

    std::unique_ptr<PackFile> pack(new PackFile);
    pack->path_ = path;
    pack->file_size_ = file_size;
    pack->data_offset_ = data.offset;
    pack->data_size_ = data.size;

    std::vector<uint8_t> bytes(size_t(index.size));
    if (index.size && !ReadAt(path, index.offset, bytes.data(), index.size)) {
        error = "index: cannot be read";
        return nullptr;
    }
    if (mods::Sha256(bytes) != Hex(header + 96, 32)) {
        error = "index: its checksum differs from the header";
        return nullptr;
    }
    if (!ReadEntries(bytes, count, data.size, pack->entries_, error)) return nullptr;

    bytes.assign(size_t(meta.size), 0);
    if (!ReadAt(path, meta.offset, bytes.data(), meta.size)) {
        error = "information: cannot be read";
        return nullptr;
    }
    if (!ReadInfo(bytes, count, pack->info_, error)) return nullptr;
    {  // only what the pack has a replacement for, once, in the order given
        std::vector<uint64_t> kept;
        for (uint64_t hash : pack->info_.preload)
            if (pack->Find(hash) && std::find(kept.begin(), kept.end(), hash) == kept.end()) kept.push_back(hash);
        pack->info_.preload = std::move(kept);
    }

    if (logo.size) {
        bytes.assign(size_t(logo.size), 0);
        // a logo that cannot be read or is not valid is dropped; the pack stays valid
        if (ReadAt(path, logo.offset, bytes.data(), logo.size)) ReadLogo(bytes, pack->logo_);
    }
    return pack;
}

const FileEntry* PackFile::Find(uint64_t hash) const {
    const auto it = std::lower_bound(entries_.begin(), entries_.end(), hash,
                                     [](const FileEntry& entry, uint64_t value) { return entry.hash < value; });
    return it != entries_.end() && it->hash == hash ? &*it : nullptr;
}

bool PackFile::ReadLevels(const FileEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const {
    if (first_level >= entry.levels) return false;
    uint64_t skipped = 0;
    for (uint32_t level = 0; level < first_level; ++level) skipped += LevelSize(entry.width, entry.height, level, entry.format);
    // the entry came from this file, but the offsets are checked again where they are used
    if (skipped >= entry.data_size || !Inside(entry.data_offset, entry.data_size, data_size_)) return false;
    const uint64_t size = entry.data_size - skipped, start = data_offset_ + entry.data_offset + skipped;
    if (!Inside(start, size, file_size_)) return false;
    out.resize(size_t(size));
    return ReadAt(path_, start, out.data(), size);
}

namespace {
class FileSource final : public PackSource {
public:
    explicit FileSource(std::shared_ptr<const PackFile> pack) : pack_(std::move(pack)) {}
    std::optional<SourceEntry> Find(uint64_t hash) const override {
        const FileEntry* e = pack_->Find(hash);
        if (!e) return std::nullopt;
        return SourceEntry{e->hash, e->width, e->height, e->levels, e->format, FirstLevel(*e, Quality::Medium)};
    }
    bool ReadLevels(const SourceEntry& entry, uint32_t first_level, std::vector<uint8_t>& out) const override {
        const FileEntry* e = pack_->Find(entry.hash);
        return e && pack_->ReadLevels(*e, first_level, out);
    }
    std::vector<uint64_t> Preload() const override { return pack_->info().preload; }

private:
    std::shared_ptr<const PackFile> pack_;
};
}

std::shared_ptr<const PackSource> AsSource(std::shared_ptr<const PackFile> pack) {
    return std::make_shared<FileSource>(std::move(pack));
}
}
