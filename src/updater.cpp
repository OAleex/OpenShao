#include "updater.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <miniz.h>
#include "file_ops.h"
#include "mods/catalog.h"
#include "mods/http.h"
#include "mods/json.h"
#include "mods/sha256.h"

namespace fs = std::filesystem;
namespace openshao::update {
namespace {
constexpr uint64_t kMaximumPackage = 512ull * 1024 * 1024;
constexpr const char* kCleanupList = ".openshao-update-cleanup";
constexpr int64_t kAutomaticInterval = 2 * 60 * 60;
constexpr int64_t kManualInterval = 15 * 60;
constexpr int64_t kMaximumPersistedWait = 48 * 60 * 60;
int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
bool IsHex64(std::string_view text) {
    return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == text.npos;
}
std::vector<uint8_t> ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + files::Utf8(path));
    return {std::istreambuf_iterator<char>(file), {}};
}
std::string SafeName(std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    while (name.starts_with("./")) name.erase(0, 2);
    if (name.empty() || name.back() == '/') return {};
    if (!files::SafeRelative(name)) throw std::runtime_error("The update package contains an unsafe path: " + name);
    return name;
}
Files ReadZip(const std::vector<uint8_t>& bytes) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) throw std::runtime_error("The update package is not a valid ZIP file");
    Files result;
    try {
        for (mz_uint i = 0, count = mz_zip_reader_get_num_files(&zip); i < count; ++i) {
            mz_zip_archive_file_stat info{};
            if (!mz_zip_reader_file_stat(&zip, i, &info)) throw std::runtime_error("The update package is damaged");
            if (info.m_is_directory) continue;
            const auto name = SafeName(info.m_filename);
            if (name.empty()) continue;
            if (info.m_uncomp_size > kMaximumPackage) throw std::runtime_error("The update package is too large");
            std::vector<uint8_t> data(size_t(info.m_uncomp_size));
            if (!mz_zip_reader_extract_to_mem(&zip, i, data.data(), data.size(), 0)) throw std::runtime_error("The update package is damaged");
            result[name] = std::move(data);
        }
    } catch (...) { mz_zip_reader_end(&zip); throw; }
    mz_zip_reader_end(&zip);
    return result;
}
std::vector<uint8_t> Gunzip(const std::vector<uint8_t>& bytes) {
    // RFC 1952: fixed header, optional fields, raw deflate data, CRC-32 and size trailer.
    if (bytes.size() < 18 || bytes[0] != 0x1f || bytes[1] != 0x8b || bytes[2] != 8) throw std::runtime_error("The update package is not a gzip file");
    const uint8_t flags = bytes[3];
    size_t at = 10;
    auto need = [&](size_t count) { if (at + count > bytes.size()) throw std::runtime_error("The update package is damaged"); };
    if (flags & 4) { need(2); at += 2 + (bytes[at] | (bytes[at + 1] << 8)); }
    for (uint8_t bit : {uint8_t(8), uint8_t(16)})
        if (flags & bit) { while (true) { need(1); if (!bytes[at++]) break; } }
    if (flags & 2) at += 2;
    need(8);
    size_t size = 0;
    void* data = tinfl_decompress_mem_to_heap(bytes.data() + at, bytes.size() - at - 8, &size, 0);
    if (!data) throw std::runtime_error("The update package is damaged");
    std::vector<uint8_t> out(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    mz_free(data);
    const size_t tail = bytes.size() - 8;
    const uint32_t crc = bytes[tail] | (bytes[tail + 1] << 8) | (bytes[tail + 2] << 16) | (uint32_t(bytes[tail + 3]) << 24);
    const uint32_t length = bytes[tail + 4] | (bytes[tail + 5] << 8) | (bytes[tail + 6] << 16) | (uint32_t(bytes[tail + 7]) << 24);
    if (uint32_t(mz_crc32(MZ_CRC32_INIT, out.data(), out.size())) != crc || uint32_t(out.size()) != length)
        throw std::runtime_error("The update package failed its integrity check");
    return out;
}
uint64_t Octal(const uint8_t* field, size_t size) {
    uint64_t value = 0;
    for (size_t i = 0; i < size && field[i]; ++i) {
        if (field[i] == ' ') continue;
        if (field[i] < '0' || field[i] > '7') throw std::runtime_error("The update package has an invalid size field");
        value = value * 8 + (field[i] - '0');
    }
    return value;
}
Files ReadTar(const std::vector<uint8_t>& bytes) {
    Files result;
    std::string long_name;
    size_t at = 0;
    while (at + 512 <= bytes.size()) {
        const uint8_t* header = bytes.data() + at;
        if (std::all_of(header, header + 512, [](uint8_t b) { return b == 0; })) break;
        const uint64_t size = Octal(header + 124, 12);
        const char type = char(header[156]);
        at += 512;
        if (size > bytes.size() - at) throw std::runtime_error("The update package is damaged");
        const auto body = bytes.begin() + ptrdiff_t(at);
        if (type == 'x') {
            // PAX records: "<length> key=value\n"; only the path matters here.
            std::string records(body, body + ptrdiff_t(size));
            for (size_t p = 0; p < records.size();) {
                const auto space = records.find(' ', p);
                if (space == records.npos) break;
                const size_t length = std::strtoul(records.c_str() + p, nullptr, 10);
                if (!length || p + length > records.size()) break;
                const auto record = records.substr(space + 1, p + length - space - 2);
                if (record.starts_with("path=")) long_name = record.substr(5);
                p += length;
            }
        } else if (type == '0' || type == 0) {
            std::string name = long_name;
            if (name.empty()) {
                const std::string prefix(reinterpret_cast<const char*>(header + 345), strnlen(reinterpret_cast<const char*>(header + 345), 155));
                name.assign(reinterpret_cast<const char*>(header), strnlen(reinterpret_cast<const char*>(header), 100));
                if (!prefix.empty()) name = prefix + "/" + name;
            }
            long_name.clear();
            constexpr std::string_view legacy_prefix = "OpenShao/";
            if (name.starts_with(legacy_prefix)) name.erase(0, legacy_prefix.size());
            if (const auto safe = SafeName(name); !safe.empty()) result[safe] = std::vector<uint8_t>(body, body + ptrdiff_t(size));
        } else {
            long_name.clear();
        }
        at += size_t((size + 511) / 512 * 512);
    }
    return result;
}
void Write(const fs::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    file.close();
    if (!file) throw std::runtime_error("Cannot write " + files::Utf8(path) + "; check free space and folder permissions");
}
void MoveReplacing(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace " + files::Utf8(to) + " (Windows error " + std::to_string(GetLastError()) + ")");
#else
    fs::rename(from, to);
#endif
}
}

std::string ExecutableName() {
#ifdef _WIN32
    return "OpenShao.exe";
#else
    return "OpenShao";
#endif
}

std::string PackageName(std::string_view version) {
#ifdef _WIN32
    return "OpenShao-" + std::string(version) + "-Windows-x64.zip";
#else
    return "OpenShao-" + std::string(version) + "-Linux-x64.tar.gz";
#endif
}

Release ParseLatest(std::string_view text) {
    const auto json = mods::Json::Parse(text);
    if (!json.Has("tag_name")) throw std::runtime_error("The release information is incomplete");
    Release release;
    release.version = json.At("tag_name").Text();
    if (release.version.starts_with('v') || release.version.starts_with('V')) release.version.erase(0, 1);
    mods::CompareVersions(release.version, release.version);
    release.page = json.Has("html_url") ? json.At("html_url").Text() : std::string(kReleasesPage);
    release.package_name = PackageName(release.version);
    if (json.Has("assets")) for (const auto& asset : json.At("assets").Items()) {
        if (!asset.Has("name") || !asset.Has("browser_download_url")) continue;
        const auto& name = asset.At("name").Text();
        const auto& url = asset.At("browser_download_url").Text();
        if (!url.starts_with("https://github.com/OAleex/OpenShao/releases/download/")) continue;
        if (name == "checksums.txt") release.checksums_url = url;
        if (name != release.package_name) continue;
        release.package_url = url;
        if (asset.Has("size")) release.size = uint64_t(std::max<int64_t>(0, asset.At("size").Number()));
        if (asset.Has("digest") && std::holds_alternative<std::string>(asset.At("digest").value)) {
            const auto digest = Lower(asset.At("digest").Text());
            if (digest.starts_with("sha256:") && IsHex64(digest.substr(7))) release.sha256 = digest.substr(7);
        }
    }
    return release;
}

std::string ChecksumFor(std::string_view listing, std::string_view file) {
    std::istringstream lines{std::string(listing)};
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto space = line.find(' ');
        if (space == line.npos) continue;
        auto name = line.substr(space);
        name.erase(0, name.find_first_not_of(" *"));
        const auto hash = Lower(line.substr(0, space));
        if (name == file && IsHex64(hash)) return hash;
    }
    return {};
}

Files ReadPackage(const fs::path& archive) {
    const auto bytes = ReadFile(archive);
    auto files = bytes.size() > 2 && bytes[0] == 0x1f && bytes[1] == 0x8b ? ReadTar(Gunzip(bytes)) : ReadZip(bytes);
    if (!files.contains(ExecutableName())) throw std::runtime_error("The update package does not contain " + ExecutableName());
    return files;
}

void Install(const fs::path& folder, const Files& package) {
    struct Step { fs::path target, staged, previous; bool existed = false, replaced = false; };
    std::vector<Step> steps;
    std::string cleanup;
    try {
        // Stage every file before replacing anything, so failures can roll back.
        for (const auto& [name, data] : package) {
            Step step;
            step.target = folder / files::FromUtf8(name);
            step.staged = step.target; step.staged += ".new";
            step.previous = step.target; step.previous += ".old";
            fs::create_directories(step.target.parent_path());
            Write(step.staged, data);
#ifndef _WIN32
            if (name == ExecutableName()) fs::permissions(step.staged, fs::perms(0755), fs::perm_options::replace);
#endif
            steps.push_back(step);
        }
        for (auto& step : steps) {
            std::error_code ec;
            fs::remove(step.previous, ec);
            step.existed = fs::exists(step.target);
            if (step.existed) {
                // Windows allows renaming a running executable, but not replacing it.
                MoveReplacing(step.target, step.previous);
                cleanup += files::Utf8(step.previous) + "\n";
            }
            step.replaced = true;
            MoveReplacing(step.staged, step.target);
        }
    } catch (...) {
        for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
            std::error_code ec;
            if (it->replaced) try {
                if (it->existed && fs::exists(it->previous)) MoveReplacing(it->previous, it->target);
                else if (!it->existed) fs::remove(it->target, ec);
            } catch (...) {}
            fs::remove(it->staged, ec);
        }
        throw;
    }
    std::ofstream(folder / kCleanupList, std::ios::binary | std::ios::app) << cleanup;
}

void RemovePreviousFiles(const fs::path& folder) {
    std::ifstream list(folder / kCleanupList, std::ios::binary);
    if (!list) return;
    std::vector<std::string> kept;
    for (std::string line; std::getline(list, line);) {
        if (line.empty()) continue;
        const auto path = files::FromUtf8(line);
        if (path.extension() != ".old" || !files::Within(path, folder)) continue;
        std::error_code ec;
        if (!fs::remove(path, ec) && fs::exists(path)) kept.push_back(line);
    }
    list.close();
    std::error_code ec;
    if (kept.empty()) fs::remove(folder / kCleanupList, ec);
    else { std::ofstream out(folder / kCleanupList, std::ios::binary | std::ios::trunc); for (const auto& line : kept) out << line << "\n"; }
}

bool Relaunch(const fs::path& folder, std::string& error) {
    const auto executable = folder / ExecutableName();
#ifdef _WIN32
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    int count = 0;
    if (wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int i = 1; i < count; ++i) {
            std::wstring argument = argv[i], quoted = L"\"";
            for (size_t j = 0; j < argument.size(); ++j) {
                size_t slashes = 0;
                while (j < argument.size() && argument[j] == L'\\') { ++slashes; ++j; }
                if (j == argument.size()) { quoted.append(slashes * 2, L'\\'); break; }
                if (argument[j] == L'"') quoted.append(slashes * 2 + 1, L'\\');
                else quoted.append(slashes, L'\\');
                quoted += argument[j];
            }
            command += L" " + quoted + L"\"";
        }
        LocalFree(argv);
    }
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup, &process)) {
        error = "Cannot start the updated game (Windows error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    std::vector<std::string> arguments;
    std::ifstream line("/proc/self/cmdline", std::ios::binary);
    for (std::string argument; std::getline(line, argument, '\0');) arguments.push_back(argument);
    if (arguments.empty()) arguments.push_back(files::Utf8(executable));
    arguments[0] = files::Utf8(executable);
    std::vector<char*> pointers;
    for (auto& argument : arguments) pointers.push_back(argument.data());
    pointers.push_back(nullptr);
    // exec keeps this process; file locks opened with O_CLOEXEC are released for the new image.
    execv(pointers[0], pointers.data());
    error = std::string("Cannot start the updated game: ") + std::strerror(errno);
    return false;
#endif
}

Updater::Updater(fs::path install, fs::path cache, std::string current, Fetch fetch)
    : install_(std::move(install)), cache_(std::move(cache)), current_(std::move(current)), fetch_(std::move(fetch)) {
    if (!fetch_) fetch_ = [](std::string_view url, const fs::path& file, uint64_t maximum, const std::function<bool(uint64_t, uint64_t)>& progress) {
        mods::Download(url, file, maximum, [progress](std::string_view, uint64_t done, uint64_t total) { return !progress || progress(done, total); });
    };
    try {
        std::ifstream state(cache_ / "check-state.txt", std::ios::binary);
        int64_t attempted = 0, blocked = 0;
        if (state >> attempted >> blocked) {
            const auto now = NowSeconds();
            if (attempted >= 0 && attempted <= now + kMaximumPersistedWait) last_check_ = attempted;
            if (blocked > now && blocked <= now + kMaximumPersistedWait) blocked_until_ = blocked;
        }
    } catch (...) {}
    try {
        const auto bytes = ReadFile(cache_ / "latest-release.json");
        auto release = ParseLatest(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        view_.release = std::move(release);
        view_.state = mods::CompareVersions(view_.release.version, current_) > 0 ? State::Available : State::Current;
    } catch (...) {}
}
Updater::~Updater() { Cancel(); if (worker_.joinable()) worker_.join(); }
View Updater::Get() const { std::lock_guard lock(mutex_); return view_; }
void Updater::Cancel() { cancel_ = true; }
void Updater::SaveCheckState() const {
    fs::create_directories(cache_);
    const auto temporary = cache_ / "check-state.tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << last_check_ << '\n' << blocked_until_ << '\n';
        file.close();
        if (!file) throw std::runtime_error("Cannot save update check cache");
    }
    MoveReplacing(temporary, cache_ / "check-state.txt");
}
void Updater::Start(std::function<void()> job) {
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    worker_ = std::jthread([this, job = std::move(job)] {
        try { job(); }
        catch (const std::exception& e) {
            std::lock_guard lock(mutex_);
            if (cancel_) {
                view_.state = view_.release.version.empty() ? State::Idle :
                    (mods::CompareVersions(view_.release.version, current_) > 0 ? State::Available : State::Current);
                view_.error.clear();
            } else if (view_.state == State::Checking) {
                view_.state = view_.release.version.empty() ? State::Failed :
                    (mods::CompareVersions(view_.release.version, current_) > 0 ? State::Available : State::Current);
                view_.error = view_.state == State::Failed ? "Unable to check for updates right now." : "";
            } else {
                view_.state = State::Failed;
                view_.error = e.what();
            }
        }
    });
}
void Updater::Tick() { Check(false); }
CheckResult Updater::Check(bool manual) {
    {
        std::lock_guard lock(mutex_);
        if (view_.state == State::Checking || view_.state == State::Downloading || view_.state == State::Ready)
            return CheckResult::Busy;
        const auto now = NowSeconds();
        const int64_t next = std::max(blocked_until_, last_check_ > 0 ? last_check_ + (manual ? kManualInterval : kAutomaticInterval) : int64_t(0));
        if (now < next) return CheckResult::Cached;
        last_check_ = now;
        view_.error.clear();
        view_.state = State::Checking;
    }
    Start([this] {
        fs::create_directories(cache_);
        try { SaveCheckState(); } catch (...) {}
        const auto temporary = cache_ / "latest-release.pending.json";
        try {
            fetch_(kLatestReleaseUrl, temporary, 4 * 1024 * 1024,
                [this](uint64_t, uint64_t) { return !cancel_.load(); });
        } catch (const mods::HttpError& error) {
            if (error.status == 403 || error.status == 429) {
                {
                    std::lock_guard lock(mutex_);
                    const auto now = NowSeconds();
                    blocked_until_ = std::clamp(std::max(now + 60, error.retry_at), now + 60,
                                                now + kMaximumPersistedWait);
                }
                try { SaveCheckState(); } catch (...) {}
            }
            std::error_code ignored; fs::remove(temporary, ignored);
            if (error.status == 403 || error.status == 429)
                throw std::runtime_error("Unable to check for updates right now.");
            throw;
        }
        const auto bytes = ReadFile(temporary);
        auto release = ParseLatest(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        try { MoveReplacing(temporary, cache_ / "latest-release.json"); } catch (...) {}
        std::lock_guard lock(mutex_);
        view_.release = std::move(release);
        view_.state = mods::CompareVersions(view_.release.version, current_) > 0 ? State::Available : State::Current;
    });
    return CheckResult::Started;
}
void Updater::Download() {
    Release release;
    {
        std::lock_guard lock(mutex_);
        if (view_.state != State::Available && view_.state != State::Failed) return;
        if (view_.release.version.empty()) return;
        release = view_.release;
        view_.state = State::Downloading; view_.error.clear(); view_.done = 0; view_.total = release.size;
    }
    Start([this, release]() mutable {
        if (release.package_url.empty())
            throw std::runtime_error("Version " + release.version + " has no package for this system yet. Download it from the release page.");
        fs::create_directories(cache_);
        if (release.sha256.empty()) {
            if (release.checksums_url.empty()) throw std::runtime_error("The release has no checksum for its package. Download it from the release page.");
            const auto list = cache_ / "checksums.txt";
            fetch_(release.checksums_url, list, 1024 * 1024, {});
            const auto bytes = ReadFile(list);
            release.sha256 = ChecksumFor(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), release.package_name);
            if (release.sha256.empty()) throw std::runtime_error("The release checksum list does not include " + release.package_name);
        }
        const auto package = cache_ / release.package_name;
        fetch_(release.package_url, package, release.size ? release.size : kMaximumPackage, [this](uint64_t done, uint64_t total) {
            std::lock_guard lock(mutex_);
            view_.done = done; if (total) view_.total = total;
            return !cancel_.load();
        });
        if (cancel_) throw std::runtime_error("Update cancelled");
        if (mods::Sha256File(package) != release.sha256) {
            std::error_code ec; fs::remove(package, ec);
            throw std::runtime_error("The downloaded update failed its SHA-256 check. Try again later.");
        }
        Install(install_, ReadPackage(package));
        std::error_code ec; fs::remove(package, ec);
        std::lock_guard lock(mutex_);
        view_.state = State::Ready;
    });
}
}
