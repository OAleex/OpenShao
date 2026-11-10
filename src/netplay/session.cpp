#include "netplay/session.h"
#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "core/log.h"
#include "host.h"
#include "kernel/kernel.h"
#include "mods/json.h"
#include "mods/sha256.h"
#include "netplay/frame_input.h"
#include "netplay/recording.h"
#include "netplay/state_hash.h"
#include "runtime_paths.h"

namespace openshao::netplay::session {
namespace fs = std::filesystem;
namespace {
enum class Mode { Off, Record, Replay };
struct State {
    std::mutex lock;
    Settings settings;
    Mode mode = Mode::Off;
    bool started = false, finished = false, replay_done = false, exit_requested = false;
    RecordingWriter writer;
    RecordingReader reader;
    FrameInputs inputs;
    std::FILE* report = nullptr;
    uint32_t frame = 0;  // boundaries passed; the published sample has this index
    unsigned hash_every = 0;
    uint64_t snapshots = 0;
};
std::atomic<input::Pad (*)(int)> g_live{nullptr};
// Lets ordinary play read the live devices without taking the session lock.
std::atomic<bool> g_requested{false};
input::Pad Neutral(int) { return {}; }
input::Pad (*Live())(int) { const auto fn = g_live.load(); return fn ? fn : Neutral; }
// Never destroyed: guest threads may still poll while the process exits.
State& S() { static auto* state = new State; return *state; }

std::string Hex64(uint64_t v) { char b[17]; std::snprintf(b, sizeof(b), "%016" PRIx64, v); return b; }
std::string Escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (uint8_t(c) < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", unsigned(uint8_t(c))); out += b; }
        else out += c;
    }
    return out;
}
std::string Utf8(const fs::path& p) { const auto u = p.u8string(); return {u.begin(), u.end()}; }
fs::path ExePath() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const auto n = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    buffer.resize(n);
    return buffer;
#else
    std::error_code ec;
    return fs::read_symlink("/proc/self/exe", ec);
#endif
}
std::string FileDigest(const fs::path& file) {
    try { return mods::Sha256File(file); } catch (const std::exception&) { return "unreadable"; }
}
// SHA-256 over sorted relative paths and file digests; an absent folder has its own value.
// Lock files are skipped: the running game holds them open exclusively.
std::string FolderFingerprint(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return "absent";
    std::vector<std::pair<std::string, std::string>> files;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->path().filename() == ".lock") continue;
        auto relative = Utf8(it->path().lexically_relative(root));
        std::replace(relative.begin(), relative.end(), '\\', '/');
        files.emplace_back(relative, FileDigest(it->path()));
    }
    if (ec) return "unreadable";
    std::sort(files.begin(), files.end());
    std::string listing;
    for (const auto& [name, digest] : files) listing += name + '\0' + digest + '\n';
    return mods::Sha256({reinterpret_cast<const uint8_t*>(listing.data()), listing.size()});
}
std::string FileFingerprint(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec) ? FileDigest(file) : "absent";
}
// The conditions a replay must reproduce exactly; "skip" fields are informational.
std::map<std::string, std::string> Conditions(const fs::path& save_directory) {
    return {
        {"executable_sha256", FileFingerprint(ExePath())},
        {"settings_sha256", FileFingerprint(paths::Config())},
        {"saves_fingerprint", FolderFingerprint(save_directory)},
        {"xbox_cache_fingerprint", FolderFingerprint(paths::Cache() / "xbox")},
        {"mods_fingerprint", FolderFingerprint(paths::Mods())},
    };
}
std::string MetadataJson(const std::map<std::string, std::string>& conditions, unsigned hash_every) {
    std::string json = "{\"format\":1,\"hash_every\":" + std::to_string(hash_every) + ",\"conditions\":{";
    bool first = true;
    for (const auto& [key, value] : conditions) {
        json += (first ? "\"" : ",\"") + key + "\":\"" + Escape(value) + "\"";
        first = false;
    }
    return json + "}}";
}
FrameSample LiveSample(input::Pad (*live)(int)) {
    FrameSample sample{};
    for (unsigned port = 0; port < kPorts; ++port) sample[port] = FromPad(live(int(port)));
    return sample;
}
uint32_t ImageEnd() {
    // XBE header at the image base: +0x104 base address, +0x10C image size.
    const uint32_t base = kernel::R32(0x10104), size = kernel::R32(0x1010C);
    return base == 0x10000 && size < 0x7FF00000u ? base + size : base;
}
void WriteSnapshot(State& s) {
    MemoryView view{[](uint32_t page) -> const uint8_t* {
                        return mem::IsCommitted(page) ? kernel::g_base + page : nullptr;
                    }, 0x10000, ImageEnd()};
    const auto blocks = HashBlocks(view);
    std::string line = "{\"frame\":" + std::to_string(s.frame) + ",\"total\":\"" + Hex64(Combine(blocks)) + "\",\"blocks\":[";
    line.reserve(blocks.size() * 40 + 64);
    for (size_t i = 0; i < blocks.size(); ++i) {
        char entry[64];
        std::snprintf(entry, sizeof(entry), "%s[%u,\"%s\",\"%016" PRIx64 "\"]", i ? "," : "", blocks[i].va,
                      KindName(blocks[i].kind), blocks[i].hash);
        line += entry;
    }
    line += "]}\n";
    std::fwrite(line.data(), 1, line.size(), s.report);
    std::fflush(s.report);
    ++s.snapshots;
}
std::FILE* OpenWrite(const fs::path& path) {
#ifdef _WIN32
    return _wfopen(path.c_str(), L"wb");
#else
    return std::fopen(path.c_str(), "wb");
#endif
}
void FinishLocked(State& s) {
    if (s.finished || !s.started) return;
    s.finished = true;
    std::string error;
    if (s.mode == Mode::Record && !s.writer.Finish(error)) LOGE("netplay: %s", error.c_str());
    if (s.report) {
        const auto recorded = s.mode == Mode::Record ? s.writer.Frames() : s.reader.Frames();
        const std::string footer = "{\"end\":true,\"mode\":\"" + std::string(s.mode == Mode::Record ? "record" : "replay") +
            "\",\"frames\":" + std::to_string(s.frame) + ",\"recording_frames\":" + std::to_string(recorded) +
            ",\"snapshots\":" + std::to_string(s.snapshots) + ",\"complete\":" +
            (s.mode == Mode::Record || s.replay_done ? "true" : "false") + "}\n";
        std::fwrite(footer.data(), 1, footer.size(), s.report);
        std::fclose(s.report);
        s.report = nullptr;
    }
    LOGR("netplay: %s finished at frame %u, %" PRIu64 " snapshots", s.mode == Mode::Record ? "recording" : "replay", s.frame, s.snapshots);
    logx::Flush();
}
}  // namespace

void Configure(const Settings& settings) {
    auto& s = S();
    std::lock_guard guard(s.lock);
    s.settings = settings;
    s.mode = !settings.record.empty() ? Mode::Record : !settings.replay.empty() ? Mode::Replay : Mode::Off;
    g_requested.store(s.mode != Mode::Off);
}
void SetLiveSource(input::Pad (*live)(int)) { g_live.store(live); }
bool Requested() { auto& s = S(); std::lock_guard guard(s.lock); return s.mode != Mode::Off; }
bool Active() { auto& s = S(); std::lock_guard guard(s.lock); return s.started && !s.finished; }
bool Replaying() { auto& s = S(); std::lock_guard guard(s.lock); return s.mode == Mode::Replay && s.started; }

bool Start(const fs::path& save_directory, std::string& error) {
    auto& s = S();
    std::lock_guard guard(s.lock);
    if (s.mode == Mode::Off || s.started) return true;
    const auto conditions = Conditions(save_directory);
    if (s.mode == Mode::Record) {
        s.hash_every = s.settings.hash_every;
        if (!s.writer.Open(s.settings.record, MetadataJson(conditions, s.hash_every), error)) return false;
    } else {
        if (!s.reader.Open(s.settings.replay, error)) return false;
        mods::Json meta;
        try { meta = mods::Json::Parse(s.reader.Metadata()); } catch (...) { error = "invalid recording metadata"; return false; }
        if (!meta.Has("conditions") || !meta.Has("hash_every")) { error = "invalid recording metadata"; return false; }
        std::string mismatched;
        for (const auto& [key, value] : conditions) {
            const auto& recorded = meta.At("conditions");
            if (!recorded.Has(key) || recorded.At(key).Text() != value) mismatched += (mismatched.empty() ? "" : ", ") + key;
        }
        if (!mismatched.empty()) { error = "replay conditions differ from the recording: " + mismatched; return false; }
        if (!s.reader.Frames()) { error = "the recording has no frames"; return false; }
        s.hash_every = s.settings.hash_every ? s.settings.hash_every : unsigned(meta.At("hash_every").Number());
    }
    if (!s.settings.hash_out.empty()) {
        s.report = OpenWrite(s.settings.hash_out);
        if (!s.report) { error = "cannot create the hash report"; return false; }
        const std::string head = "{\"format\":1,\"mode\":\"" + std::string(s.mode == Mode::Record ? "record" : "replay") +
            "\",\"hash_every\":" + std::to_string(s.hash_every) + ",\"block\":" + std::to_string(kHashBlock) + "}\n";
        std::fwrite(head.data(), 1, head.size(), s.report);
    }
    FrameSample first{};
    if (s.mode == Mode::Record) {
        first = LiveSample(Live());
        if (!s.writer.Append(first)) { error = "cannot write the first frame"; return false; }
    } else {
        s.reader.Frame(0, first);
    }
    s.inputs.Publish(0, first);
    s.started = true;
    std::atexit([] { Finish(); });
    LOGR("netplay: %s started (hash every %u frames)", s.mode == Mode::Record ? "recording" : "replay", s.hash_every);
    return true;
}

input::Pad Poll(int port) {
    if (!g_requested.load(std::memory_order_relaxed)) return Live()(port);
    auto& s = S();
    {
        std::lock_guard guard(s.lock);
        // Before Start a replay shows a disconnected pad; a recording passes live input through.
        if (!s.started) return s.mode == Mode::Replay ? input::Pad{} : Live()(port);
    }
    return s.inputs.Pad(port);
}

void FrameBoundary() {
    auto& s = S();
    bool exit = false;
    {
        std::lock_guard guard(s.lock);
        if (!s.started || s.finished) return;
        ++s.frame;
        if (s.report && s.hash_every && s.frame % s.hash_every == 0) WriteSnapshot(s);
        FrameSample next{};
        if (s.mode == Mode::Record) {
            next = LiveSample(Live());
            if (!s.writer.Append(next)) LOGE("netplay: cannot write frame %u", s.frame);
            s.inputs.Publish(s.frame, next);
        } else if (s.frame < s.reader.Frames()) {
            s.reader.Frame(s.frame, next);
            s.inputs.Publish(s.frame, next);
        } else if (!s.exit_requested) {
            // Every recorded sample was consumed: the replay ends here.
            s.replay_done = true;
            s.exit_requested = exit = true;
            FinishLocked(s);
        }
    }
    if (exit && !RequestExit()) std::exit(0);
}

void Finish() {
    auto& s = S();
    std::lock_guard guard(s.lock);
    FinishLocked(s);
}
}  // namespace openshao::netplay::session
