#include "language_archive.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace openshao::language {
namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
constexpr uint64_t kAlignment = 0x800;
constexpr uint64_t kPackedMaximum = 0x3fffff;
constexpr std::array<const char*, 8> kVolumes{
    "GAMEDATA.WAD", "GAMEDATA.WAE", "GAMEDATA.WAF", "GAMEDATA.WAG",
    "GAMEDATA.WAH", "GAMEDATA.WAI", "GAMEDATA.WAJ", "GAMEDATA.WAK"};
constexpr std::array<uint32_t, 23> kEnglishAndSharedRecords{
    7027, 7041, 7042, 7043, 7048, 7080, 7081, 7108, 7109, 7132, 7133,
    7154, 7155, 7175, 7176, 7187, 7213, 7214, 7232, 7233, 7244, 7271, 7272};

[[noreturn]] void Fail(const std::string& message) { throw std::runtime_error(message); }
void Require(bool condition, const std::string& message) { if (!condition) Fail(message); }
uint32_t Get(std::span<const uint8_t> data, size_t at) {
    Require(at <= data.size() && data.size()-at >= 4, "Truncated archive header");
    return uint32_t(data[at]) | uint32_t(data[at+1]) << 8 |
           uint32_t(data[at+2]) << 16 | uint32_t(data[at+3]) << 24;
}
void Put(std::span<uint8_t> data, size_t at, uint32_t value) {
    Require(at <= data.size() && data.size()-at >= 4, "Archive table write overflow");
    for (unsigned i = 0; i != 4; ++i) data[at+i] = uint8_t(value >> (8*i));
}
bool Magic(std::span<const uint8_t> data, std::string_view magic) {
    return data.size() >= magic.size() && std::equal(magic.begin(), magic.end(), data.begin());
}
uint64_t Align(uint64_t value) { return (value + kAlignment-1) & ~(kAlignment-1); }
std::string Lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
    return value;
}
struct Record {
    uint32_t index = 0, original_a = 0, original_b = 0;
    uint64_t original_offset = 0, original_size = 0, output_offset = 0, output_size = 0;
    bool packed = false;
    std::shared_ptr<const VirtualFile> replacement;
};
struct Voice {
    fs::path path;
    uint64_t size = 0;
    bool bound = false;
};
struct Context {
    const SourceRead& read;
    const SourceSize& size;
    uint64_t stride = 0;
    std::array<uint64_t, 8> source_sizes{};
    Context(const SourceRead& source_read, const SourceSize& source_size) : read(source_read), size(source_size) {}

    void Read(uint64_t offset, std::span<uint8_t> output) const {
        const auto volume = offset / stride, position = offset % stride;
        Require(volume < kVolumes.size() && position <= source_sizes[volume] &&
                output.size() <= source_sizes[volume]-position && output.size() <= stride-position,
                "Original archive read exceeds a volume boundary");
        Require(output.empty() || read(kVolumes[volume], position, output), "Cannot read original game archive");
    }
    void AddOriginal(VirtualFile& out, uint64_t at, uint64_t offset, uint64_t amount) const {
        const auto name = std::string(kVolumes[offset / stride]);
        const auto source = read;
        out.Add(at, amount, offset % stride, [source, name](uint64_t pos, std::span<uint8_t> bytes) {
            return source(name, pos, bytes);
        });
    }
};

void ReadPack(const PackSelection& selection, std::vector<Record>& records,
              std::map<std::string, Voice>& voices, ArchiveStats& stats, uint64_t stride) {
    const auto& pack=selection.directory;
    const auto resources = pack / "gamedata";
    if (selection.text && fs::exists(resources)) {
        Require(fs::is_directory(resources) && !fs::is_symlink(fs::symlink_status(resources)),
                "Language gamedata must be a regular directory");
        for (const auto& item : fs::directory_iterator(resources)) {
            Require(item.is_regular_file() && !item.is_symlink(), "Invalid language resource file");
            const auto name = Lower(item.path().filename().string());
            const auto extension = Lower(item.path().extension().string());
            Require((extension == ".bin" || extension == ".ewdf") && name.size() == 11+extension.size() &&
                    name.starts_with("record") &&
                    std::all_of(name.begin()+6, name.begin()+11, [](char c) { return c >= '0' && c <= '9'; }),
                    "Language resources must use recordNNNNN.bin or recordNNNNN.ewdf");
            const auto index = static_cast<uint32_t>(std::stoul(name.substr(6, 5)));
            Require(index < records.size(), "Language resource record is outside the original archive");
            auto& record = records[index];
            Require(!record.replacement, "Duplicate language resource record");
            const auto bytes = item.file_size();
            Require(bytes > 0 && bytes <= stride, "Language resource size exceeds a volume");
            if (extension == ".ewdf") {
                Require(bytes >= 25 && bytes <= kPackedMaximum,
                        "Prepared EWDF size cannot be represented in the PWF table");
                std::ifstream input(item.path(), std::ios::binary);
                Bytes prefix(static_cast<size_t>(bytes));
                input.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
                Require(bool(input) && Magic(prefix, "EWDF"), "Prepared resource has no EWDF header");
                const auto decoded_size = Get(prefix, 4), payload = Get(prefix, 8);
                Require(decoded_size > 0 && decoded_size <= kPackedMaximum &&
                        payload >= 17 && payload < bytes && payload <= 4096 &&
                        std::find(prefix.begin()+16, prefix.begin()+payload, uint8_t(0)) != prefix.begin()+payload,
                        "Prepared EWDF header has invalid resource bounds");
                Bytes decoded(decoded_size);
                mz_stream inflater{};
                inflater.next_in = prefix.data()+payload;
                inflater.avail_in = static_cast<unsigned>(bytes-payload);
                inflater.next_out = decoded.data();
                inflater.avail_out = decoded_size;
                Require(mz_inflateInit2(&inflater, -MZ_DEFAULT_WINDOW_BITS) == MZ_OK,
                        "Cannot initialize EWDF validation");
                const auto status = mz_inflate(&inflater, MZ_FINISH);
                const auto consumed = inflater.total_in, produced = inflater.total_out;
                mz_inflateEnd(&inflater);
                Require(status == MZ_STREAM_END && produced == decoded_size,
                        "Prepared EWDF DEFLATE payload is invalid");
                const uint64_t trailer = payload+consumed;
                Require(trailer <= bytes && bytes-trailer >= 8 && bytes-trailer-8 < 16 &&
                        Get(prefix, size_t(trailer)) == mz_crc32(MZ_CRC32_INIT, decoded.data(), decoded.size()) &&
                        Get(prefix, size_t(trailer)+4) == decoded_size &&
                        std::all_of(prefix.begin()+static_cast<ptrdiff_t>(trailer)+8, prefix.end(),
                                    [](uint8_t byte) { return byte == 0; }),
                        "Prepared EWDF trailer checksum, size, or padding is invalid");
                // Decode raw PWF records here to avoid retaining compressed-buffer slack.
                auto data = std::make_shared<VirtualFile>();
                data->AddBytes(0, std::move(decoded)); data->SetSize(decoded_size); data->Seal();
                record.replacement = std::move(data);
                record.output_size = decoded_size;
            } else {
                auto data = std::make_shared<VirtualFile>();
                data->Add(0, bytes, 0, PackFileReader(item.path())); data->SetSize(bytes); data->Seal();
                record.replacement = std::move(data);
                record.output_size = bytes;
            }
            record.packed = false;
            ++stats.resources;
        }
    }
    const auto audio = pack / "audio/voices";
    if (selection.voices && fs::exists(audio)) {
        Require(fs::is_directory(audio) && !fs::is_symlink(fs::symlink_status(audio)),
                "Language voices must be a regular directory");
        for (const auto& item : fs::directory_iterator(audio)) {
            Require(item.is_regular_file() && !item.is_symlink(), "Invalid language voice file");
            const auto name = Lower(item.path().filename().string());
            Require(Lower(item.path().extension().string()) == ".adx", "Unexpected file in language voices directory");
            const auto bytes = item.file_size();
            Require(bytes > 0 && bytes <= stride, "Language voice size exceeds a volume");
            Require(voices.emplace(name, Voice{item.path(), bytes}).second, "Duplicate case-insensitive language voice name");
        }
    }
}

void RebuildAfs(Context& context, Record& record,
                std::map<std::string, Voice>& voices, ArchiveStats& stats) {
    Require(!record.replacement && !record.packed && record.original_size >= 16,
            "Selected voice bank conflicts with a whole-resource replacement");
    std::array<uint8_t, 8> prefix{};
    context.Read(record.original_offset, prefix);
    Require(Magic(prefix, std::string_view("AFS\0", 4)), "Selected original voice bank is not AFS");
    const auto count = Get(prefix, 4);
    Require(count > 0 && count <= 32768, "Invalid AFS member count");
    const uint64_t table_bytes = 16 + uint64_t(count)*8;
    Require(table_bytes <= record.original_size, "AFS table exceeds the original bank");
    Bytes table(static_cast<size_t>(table_bytes));
    context.Read(record.original_offset, table);
    const uint64_t names_at = Get(table, 8+size_t(count)*8), names_size = Get(table, 12+size_t(count)*8);
    Require(names_at <= record.original_size && names_size <= record.original_size-names_at &&
            names_size >= uint64_t(count)*48 && names_size <= 16*1024*1024,
            "AFS filename table exceeds the original bank");
    Bytes names(static_cast<size_t>(names_size));
    context.Read(record.original_offset+names_at, names);
    struct Member { uint64_t original_offset, size, output_offset; Voice* replacement; };
    std::vector<Member> members;
    bool changed = false;
    uint64_t cursor = Align(table_bytes);
    for (uint32_t i = 0; i != count; ++i) {
        const uint64_t at = Get(table, 8+size_t(i)*8), bytes = Get(table, 12+size_t(i)*8);
        Require(at >= table_bytes && at <= record.original_size && bytes <= record.original_size-at,
                "AFS member exceeds the original bank");
        const auto begin = names.begin()+size_t(i)*48;
        const auto end = std::find(begin, begin+32, uint8_t(0));
        const std::string name(begin, end);
        const auto match = voices.find(Lower(name));
        Voice* voice = match == voices.end() ? nullptr : &match->second;
        const uint64_t replacement_size = voice ? voice->size : bytes;
        Require(cursor <= context.stride && replacement_size <= context.stride-cursor,
                "Translated AFS bank exceeds a volume");
        members.push_back({record.original_offset+at, replacement_size, cursor, voice});
        Put(table, 8+size_t(i)*8, static_cast<uint32_t>(cursor));
        Put(table, 12+size_t(i)*8, static_cast<uint32_t>(replacement_size));
        Put(names, size_t(i)*48+44, static_cast<uint32_t>(replacement_size));
        cursor = Align(cursor+replacement_size);
        if (voice) { changed = true; voice->bound = true; ++stats.voice_occurrences; }
    }
    if (!changed) return;
    Require(cursor <= context.stride && names_size <= context.stride-cursor,
            "Translated AFS filename table exceeds a volume");
    Put(table, 8+size_t(count)*8, static_cast<uint32_t>(cursor));
    const uint64_t rebuilt_size = Align(cursor+names_size);
    Require(rebuilt_size <= context.stride, "Translated AFS bank exceeds a volume");
    auto data = std::make_shared<VirtualFile>();
    data->AddBytes(0, std::move(table));
    for (const auto& member : members) {
        if (member.replacement) data->Add(member.output_offset, member.size, 0, PackFileReader(member.replacement->path));
        else context.AddOriginal(*data, member.output_offset, member.original_offset, member.size);
    }
    data->AddBytes(cursor, std::move(names)); data->SetSize(rebuilt_size); data->Seal();
    record.replacement = std::move(data);
    record.output_size = rebuilt_size;
}

void Encode(Bytes& table, const Record& record) {
    uint32_t a, b;
    Require(record.output_offset <= UINT32_MAX && record.output_size <= UINT32_MAX,
            "Translated record exceeds the PWF address space");
    if (record.packed) {
        Require(record.output_offset % kAlignment == 0 && record.output_offset/kAlignment <= 0x1fffff &&
                record.output_size == record.original_size && record.output_size <= kPackedMaximum,
                "Translated EWDF cannot be represented in the PWF table");
        // a bits 22..31 + b bits 0..11: stored size; b bits 12..30: decoded capacity in 8-byte units.
        a = (record.original_a & 0xffe00000) | static_cast<uint32_t>(record.output_offset/kAlignment);
        b = record.original_b;
    } else {
        a = static_cast<uint32_t>(record.output_offset);
        b = static_cast<uint32_t>(record.output_size);
        Require(!(b & 0x80000000), "Plain translated record exceeds the PWF size field");
    }
    Put(table, size_t(record.index)*8, a);
    Put(table, size_t(record.index)*8+4, b);
}
}

static bool PlanSelectedArchives(std::span<const PackSelection> packs,
    const SourceRead& read, const SourceSize& size, std::span<const uint32_t> selected_records,
    ArchiveView& files, ArchiveStats& stats, std::string& error) {
    stats = {}; files = {};
    error.clear();
    Context context{read, size};
    try {
        Require(bool(read) && bool(size), "Original archive reader is unavailable");
        for(const auto& pack:packs)Require(fs::is_directory(pack.directory), "Mod resource directory is unavailable");
        for (size_t i = 0; i != kVolumes.size(); ++i) context.source_sizes[i] = size(kVolumes[i]);
        Require(context.source_sizes[0] >= 32, "Original PWF header is missing");
        Bytes header(32);
        Require(read(kVolumes[0], 0, header) && Magic(header, "PWF "), "Original game archive is not PWF");
        const auto count = Get(header, 12);
        context.stride = Get(header, 16);
        Require(count > 0 && count <= 65536 && context.stride >= kAlignment &&
                context.stride % kAlignment == 0 && context.stride*kVolumes.size() <= uint64_t(UINT32_MAX)+1 &&
                Get(header, 20) == kAlignment,
                "Unsupported original PWF directory layout");
        Bytes table(size_t(count)*8);
        Require(32+table.size() <= context.source_sizes[0] && 32+table.size() <= context.stride,
                "Original PWF directory exceeds its volume");
        context.Read(32, table);
        std::vector<Record> records(count);
        for (uint32_t i = 0; i != count; ++i) {
            auto& record = records[i];
            record.index = i;
            record.original_a = Get(table, size_t(i)*8);
            record.original_b = Get(table, size_t(i)*8+4);
            record.packed = bool(record.original_b & 0x80000000);
            record.original_offset = record.packed ? uint64_t(record.original_a & 0x1fffff)*kAlignment : record.original_a;
            record.original_size = record.packed ? ((record.original_a>>22)|(record.original_b<<10))&0x3fffff : record.original_b;
            record.output_size = record.original_size;
            const auto volume = record.original_offset/context.stride, at = record.original_offset%context.stride;
            Require(volume < kVolumes.size() && at <= context.source_sizes[volume] &&
                    record.original_size <= context.source_sizes[volume]-at && record.original_size <= context.stride-at,
                    "Original PWF record exceeds its volume");
            Require(!record.original_size || record.original_offset >= 32+table.size(),
                    "Original PWF record overlaps its directory");
        }
        ArchiveStats result;
        std::map<std::string, Voice> voices;
        for(const auto& pack:packs)ReadPack(pack, records, voices, result, context.stride);
        std::set<uint32_t> selected;
        for (const auto index : selected_records) {
            Require(index < count && selected.insert(index).second, "Invalid selected AFS record index");
        }
        if (!voices.empty()) {
            for (const auto index : selected) RebuildAfs(context, records[index], voices, result);
        }
        for (const auto& [name, voice] : voices) {
            if (voice.bound) ++result.voice_names;
            else result.unbound_voices.push_back(name);
        }

        std::vector<Record*> order;
        for (auto& record : records) order.push_back(&record);
        std::stable_sort(order.begin(), order.end(), [](const Record* a, const Record* b) {
            return std::tie(a->original_offset, a->index) < std::tie(b->original_offset, b->index);
        });
        uint64_t cursor = Align(32+table.size());
        std::map<std::tuple<uint64_t, uint64_t, bool>, uint64_t> original_extents;
        for (auto* record : order) {
            const auto key = std::make_tuple(record->original_offset, record->original_size, record->packed);
            const auto duplicate = original_extents.find(key);
            if (!record->replacement && duplicate != original_extents.end()) {
                record->output_offset = duplicate->second;
                Encode(table, *record);
                continue;
            }
            cursor = Align(cursor);
            if (record->output_size > context.stride-cursor%context.stride)
                cursor = (cursor/context.stride+1)*context.stride;
            Require(cursor/context.stride < kVolumes.size() && record->output_size <= context.stride,
                    "Translated game archives exceed the eight supported volumes");
            record->output_offset = cursor;
            if (!record->replacement) original_extents.emplace(key, cursor);
            cursor += record->output_size;
            Encode(table, *record);
        }
        Put(header, 4, static_cast<uint32_t>(cursor));
        std::array<std::shared_ptr<VirtualFile>, 8> output_files;
        for (auto& file : output_files) file = std::make_shared<VirtualFile>();
        output_files[0]->AddBytes(0, std::move(header));
        output_files[0]->AddBytes(32, std::move(table));
        std::set<std::pair<uint64_t, uint64_t>> written;
        for (const auto* record : order) {
            if (!record->output_size || !written.emplace(record->output_offset, record->output_size).second) continue;
            const auto volume = record->output_offset/context.stride, at = record->output_offset%context.stride;
            auto& file = *output_files[volume];
            if (!record->replacement) context.AddOriginal(file, at, record->original_offset, record->output_size);
            else file.Add(at, record->output_size, 0, record->replacement->Reader());
        }
        // Empty records can point to aligned EOF positions beyond the last payload.
        std::array<uint64_t, 8> physical_ends{};
        physical_ends[0] = 32+uint64_t(count)*8;
        for (const auto& record : records) {
            const auto volume = record.output_offset/context.stride;
            physical_ends[volume] = std::max(physical_ends[volume], record.output_offset%context.stride+record.output_size);
        }
        for (size_t i = 0; i != output_files.size(); ++i) {
            output_files[i]->SetSize(physical_ends[i]); output_files[i]->Seal();
            files[i] = std::move(output_files[i]);
        }
        stats = std::move(result);
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        files = {};
        return false;
    }
}
bool PlanArchivesForPacks(std::span<const fs::path> packs,const SourceRead& read,const SourceSize& size,
    std::span<const uint32_t> selected,ArchiveView& files,ArchiveStats& stats,std::string& error) {
    std::vector<PackSelection> layers;for(const auto& pack:packs)layers.push_back({pack});
    return PlanSelectedArchives(layers,read,size,selected,files,stats,error);
}
bool PlanArchivesForPacks(std::span<const PackSelection> packs,const SourceRead& read,const SourceSize& size,
    ArchiveView& files,ArchiveStats& stats,std::string& error) {
    return PlanSelectedArchives(packs,read,size,kEnglishAndSharedRecords,files,stats,error);
}
bool PlanArchivesForRecords(const fs::path& pack,const SourceRead& read,const SourceSize& size,
    std::span<const uint32_t> selected,ArchiveView& files,ArchiveStats& stats,std::string& error) {
    const std::array<fs::path,1> packs{pack};
    return PlanArchivesForPacks(packs,read,size,selected,files,stats,error);
}
bool PlanArchivesForPacks(std::span<const fs::path> packs,const SourceRead& read,const SourceSize& size,
    ArchiveView& files,ArchiveStats& stats,std::string& error) {
    return PlanArchivesForPacks(packs,read,size,kEnglishAndSharedRecords,files,stats,error);
}
bool PlanArchives(const fs::path& pack, const SourceRead& read, const SourceSize& size,
                  ArchiveView& files, ArchiveStats& stats, std::string& error) {
    return PlanArchivesForRecords(pack, read, size, kEnglishAndSharedRecords, files, stats, error);
}

// Used by offline regression fixtures. The runtime mounts PlanArchives directly.
bool BuildArchivesForRecords(const fs::path& pack, const fs::path& output,
    const SourceRead& read, const SourceSize& size, std::span<const uint32_t> selected,
    ArchiveStats& stats, std::string& error) {
    std::vector<fs::path> created;
    try {
        Require(!fs::exists(output) || (fs::is_directory(output) &&
                !fs::is_symlink(fs::symlink_status(output)) && fs::is_empty(output)),
                "Language archive output directory must be new or empty");
        ArchiveView files;
        if (!PlanArchivesForRecords(pack, read, size, selected, files, stats, error)) return false;
        fs::create_directories(output);
        Bytes buffer(1024*1024);
        for (size_t i = 0; i < files.size(); ++i) {
            const auto path = output / kVolumes[i];
            Require(!fs::exists(path), "Language archive output already exists");
            std::ofstream stream(path, std::ios::binary); created.push_back(path);
            for (uint64_t at = 0; at < files[i]->Size();) {
                const auto amount = size_t(std::min<uint64_t>(buffer.size(), files[i]->Size() - at));
                size_t count;
                Require(files[i]->Read(at, std::span(buffer).first(amount), count) && count == amount,
                        "Cannot read virtual language archive");
                stream.write(reinterpret_cast<const char*>(buffer.data()), amount);
                Require(bool(stream), "Cannot write offline language archive"); at += amount;
            }
            stream.close(); Require(bool(stream), "Cannot finish offline language archive");
        }
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        for (const auto& path : created) { std::error_code ignored; fs::remove(path, ignored); }
        return false;
    }
}
}
