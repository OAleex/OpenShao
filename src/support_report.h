#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace openshao::support {
struct Source {
    std::filesystem::path data, session_log;
    std::string version, kind = "manual", reason;
    std::filesystem::path dump;
    std::string dump_status;
};
struct Report {
    std::filesystem::path zip;
    std::string error;
    bool dump_included = false;
};
Report Generate(const Source& source) noexcept;
void ShowReport(const Report& report, const std::string& reason);
void OpenReport(const std::filesystem::path& zip);
bool StartMonitor(const Source& source);
// Publishes preallocated exception data and waits briefly for the independent dump writer.
// native_exception is EXCEPTION_POINTERS on Windows; it is unused on other platforms.
bool NotifyCrash(uint32_t code, uint64_t address = 0, void* native_exception = nullptr) noexcept;
void StopMonitor() noexcept;
int RunWorker(const std::filesystem::path& task) noexcept;
}
