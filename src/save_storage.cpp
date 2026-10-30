#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <chrono>
#include "data_home.h"
#endif
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <vector>
#include "save_storage.h"

namespace fs = std::filesystem;
namespace openshao::storage {
namespace {
fs::path g_root;
std::mutex g_backups;
std::atomic<unsigned> g_sequence{0};
#ifndef _WIN32
uint32_t ErrnoStatus(int error) {
    if (error == ENOSPC) return kDiskFull;
    if (error == EACCES || error == EPERM || error == EBUSY) return 0xC0000022;
    return kIoError;
}
std::wstring Unique() {
    const auto ticks = std::chrono::system_clock::now().time_since_epoch().count();
    return std::to_wstring(ticks) + L"-" + std::to_wstring(getpid()) + L"-" + std::to_wstring(g_sequence++);
}
std::wstring Upper(std::wstring s) { for (auto& c : s) c = wchar_t(std::towupper(c)); return s; }
uint32_t SyncPath(const fs::path& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return ErrnoStatus(errno);
    const auto status = fsync(fd) == 0 ? 0 : ErrnoStatus(errno);
    close(fd); return status;
}
#else
uint32_t WinStatus(DWORD error) {
    if (error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL) return kDiskFull;
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) return 0xC0000022;
    return kIoError;
}
std::wstring Unique() {
    FILETIME t; GetSystemTimeAsFileTime(&t);
    const uint64_t ticks = (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    return std::to_wstring(ticks) + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(g_sequence++);
}
std::wstring Upper(std::wstring s) { for (auto& c : s) c = wchar_t(std::towupper(c)); return s; }
uint32_t SyncPath(const fs::path& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return WinStatus(GetLastError());
    const auto status = FlushFileBuffers(h) ? 0 : WinStatus(GetLastError());
    CloseHandle(h); return status;
}
#endif
bool SameBytes(const fs::path& a, const fs::path& b) {
    std::ifstream x(a, std::ios::binary), y(b, std::ios::binary);
    char xb[65536], yb[65536];
    do {
        x.read(xb, sizeof xb); y.read(yb, sizeof yb);
        if (x.gcount() != y.gcount() || !std::equal(xb, xb + x.gcount(), yb)) return false;
    } while (x && y);
    return x.eof() && y.eof();
}
}

uint32_t Flush(FILE* file) {
    if (!file) return 0xC0000008;
    if (std::fflush(file) != 0) return errno == ENOSPC ? kDiskFull : kIoError;
#ifdef _WIN32
    // fflush only drains the CRT buffer. Commit also asks Windows to flush the file.
    if (_commit(_fileno(file)) != 0) return errno == ENOSPC ? kDiskFull : kIoError;
#else
    if (fsync(fileno(file)) != 0) return errno == ENOSPC ? kDiskFull : kIoError;
#endif
    return 0;
}
void SetDataRoot(const fs::path& root) { g_root = fs::absolute(root).lexically_normal(); }
bool IsSave(const fs::path& file) {
    if (g_root.empty()) return false;
    const auto relative = fs::absolute(file).lexically_normal().lexically_relative(g_root);
    auto p = relative.begin();
    if (p == relative.end() || Upper(p->wstring()) != L"E") return false;
    ++p;
    return p != relative.end() && Upper(p->wstring()) == L"UDATA";
}
uint32_t BeforeWrite(const fs::path& file) {
    if (!IsSave(file)) return 0;
    std::lock_guard<std::mutex> guard(g_backups);
    std::error_code ec;
    const auto exists = fs::exists(file, ec);
    if (ec) return kIoError;
    if (!exists) return 0;
    const auto size = fs::file_size(file, ec);
    if (ec) return kIoError;
    if (!size) return 0;
    const auto relative = fs::absolute(file).lexically_normal().lexically_relative(g_root);
    const auto folder = g_root / ".backups" / relative;
    fs::create_directories(folder, ec);
    if (ec) return ec == std::errc::no_space_on_device ? kDiskFull : kIoError;
    const auto id = Unique();
    const auto temp = folder / (id + L".tmp"), final = folder / (id + L".bak");
#ifdef _WIN32
    if (!CopyFileW(file.c_str(), temp.c_str(), TRUE)) return WinStatus(GetLastError());
#else
    fs::copy_file(file, temp, fs::copy_options::none, ec);
    if (ec) return ErrnoStatus(ec.value());
#endif
    uint32_t status = SyncPath(temp);
    if (!status && !SameBytes(file, temp)) status = kIoError;
#ifdef _WIN32
    if (!status && !MoveFileExW(temp.c_str(), final.c_str(), MOVEFILE_WRITE_THROUGH)) status = WinStatus(GetLastError());
#else
    if (!status && (fs::exists(final, ec) || std::rename(temp.c_str(), final.c_str()) != 0)) status = ErrnoStatus(errno);
#endif
    if (status) { fs::remove(temp, ec); return status; }
    std::vector<fs::path> versions;
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == L".bak") versions.push_back(it->path());
    std::sort(versions.begin(), versions.end());
    for (size_t i = 0; i + 3 < versions.size(); ++i) fs::remove(versions[i], ec);
    return 0;
}

bool PrepareDirectory(const fs::path& legacy, const fs::path& destination, std::string& error) {
    fs::path staging, parked_empty;
    try {
        if (fs::exists(destination)) {
            if (!fs::is_directory(destination)) throw std::runtime_error("save destination is not a directory");
            // An empty destination must not hide an unmigrated legacy campaign.
            if (legacy.empty() || !fs::exists(legacy) || fs::is_empty(legacy)) return true;
            if (!fs::is_empty(destination)) return true;
        }
        fs::create_directories(destination.parent_path());
        staging = destination.parent_path() / (destination.filename().wstring() + L".migrating-" + Unique());
        fs::create_directory(staging);
        if (!legacy.empty() && fs::exists(legacy)) {
            if (!fs::is_directory(legacy)) throw std::runtime_error("legacy save source is not a directory");
            for (const auto& e : fs::recursive_directory_iterator(legacy)) {
                auto relative = e.path().lexically_relative(legacy);
                if (relative == ".lock") continue;
                if (e.is_symlink()) throw std::runtime_error("save migration refuses symbolic links");
                const auto target = staging / relative;
                if (e.is_directory()) fs::create_directory(target);
                else if (e.is_regular_file()) {
                    fs::copy_file(e.path(), target);
                    if (SyncPath(target) || !SameBytes(e.path(), target)) throw std::runtime_error("save migration copy could not be verified");
                }
            }
        }
        if (fs::exists(destination)) {
            parked_empty = destination.parent_path() / (destination.filename().wstring() + L".empty-" + Unique());
            fs::rename(destination, parked_empty);
        }
#ifdef _WIN32
        if (!MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("cannot publish migrated save directory (Windows error " + std::to_string(GetLastError()) + ")");
#else
        if (fs::exists(destination) || std::rename(staging.c_str(), destination.c_str()) != 0)
            throw std::runtime_error("cannot publish migrated save directory (errno " + std::to_string(errno) + ")");
#endif
        if (!parked_empty.empty()) { std::error_code ignored; fs::remove(parked_empty, ignored); }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        if (!staging.empty()) { std::error_code ignored; fs::remove_all(staging, ignored); }
        if (!parked_empty.empty()) {
            std::error_code ignored;
            if (!fs::exists(destination)) fs::rename(parked_empty, destination, ignored);
        }
        return false;
    }
}
#ifndef _WIN32
fs::path SavedGamesRoot(std::string& error, bool create) {
    // The XDG data directory keeps saves beside the application data, separate from settings.
    try {
        const auto root = UserDataHome() / "OpenShao" / "saves";
        if (create) fs::create_directories(root);
        return root;
    } catch (const std::exception& e) { error = e.what(); return {}; }
}
DirectoryLock::~DirectoryLock() {
    if (handle_) { const int fd = int(reinterpret_cast<intptr_t>(handle_)) - 1; flock(fd, LOCK_UN); close(fd); }
}
bool DirectoryLock::Acquire(const fs::path& root, std::string& error, bool* in_use) {
    const int fd = open((root / ".lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) {
        if (in_use) *in_use = fd >= 0 && errno == EWOULDBLOCK;
        if (fd >= 0) close(fd);
        error = "cannot lock save directory; another game may be using it (errno " + std::to_string(errno) + ")";
        return false;
    }
    handle_ = reinterpret_cast<void*>(intptr_t(fd) + 1); return true;
}
#else
fs::path SavedGamesRoot(std::string& error, bool create) {
    PWSTR path = nullptr;
    // A fresh profile may not have created Saved Games yet. Resolve its location
    // without touching disk; PrepareDirectory creates the selected save root.
    const auto result = SHGetKnownFolderPath(FOLDERID_SavedGames, create ? KF_FLAG_CREATE : KF_FLAG_DONT_VERIFY, nullptr, &path);
    if (FAILED(result)) { error = "Windows Saved Games folder unavailable (" + std::to_string(result) + ")"; return {}; }
    fs::path out(path); CoTaskMemFree(path); return out / L"OpenShao";
}
DirectoryLock::~DirectoryLock() { if (handle_) CloseHandle(handle_); }
bool DirectoryLock::Acquire(const fs::path& root, std::string& error, bool* in_use) {
    HANDLE h = CreateFileW((root / ".lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (in_use) *in_use = code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION;
        error = "cannot lock save directory; another game may be using it (Windows error " + std::to_string(code) + ")";
        return false;
    }
    handle_ = h; return true;
}
#endif
}
