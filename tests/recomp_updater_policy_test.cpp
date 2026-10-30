#include "updater.h"
#include "mods/http.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using namespace openshao;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int64_t Now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void Write(const fs::path& path, std::string_view value) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << value;
    Require(bool(out), "Cannot create updater policy fixture");
}

std::string Metadata(std::string_view version) {
    return "{\"tag_name\":\"v" + std::string(version) +
           "\",\"html_url\":\"https://github.com/OAleex/OpenShao/releases/latest\",\"assets\":[]}";
}

void SetCheckTime(const fs::path& cache, int64_t attempted, int64_t blocked = 0) {
    Write(cache / "check-state.txt", std::to_string(attempted) + "\n" + std::to_string(blocked) + "\n");
}

update::View Wait(update::Updater& updater) {
    const auto until = std::chrono::steady_clock::now() + 5s;
    for (;;) {
        auto state = updater.Get();
        if (state.state != update::State::Checking && state.state != update::State::Downloading)
            return state;
        Require(std::chrono::steady_clock::now() < until, "Update worker did not finish");
        std::this_thread::sleep_for(5ms);
    }
}

struct Workspace {
    fs::path root;
    Workspace()
        : root(fs::temp_directory_path() /
               ("openshao-update-policy-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(root);
    }
    ~Workspace() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
};

void CachedManualChecksAndRestart(const fs::path& root) {
    const auto cache = root / "cached" / "cache";
    std::atomic<int> requests{0};
    auto fetch = [&](std::string_view url, const fs::path& destination, uint64_t,
                     const std::function<bool(uint64_t, uint64_t)>&) {
        Require(url == update::kLatestReleaseUrl, "Check contacted an unexpected endpoint");
        ++requests;
        Write(destination, Metadata("0.4.2"));
    };

    {
        update::Updater updater(root / "install", cache, "0.4.1", fetch);
        Require(updater.Check(true) == update::CheckResult::Started,
                "First manual click must start a genuine network check");
        auto view = Wait(updater);
        Require(view.state == update::State::Available && view.release.version == "0.4.2",
                "A successful check must offer the newer release");
        Require(updater.Check(true) == update::CheckResult::Cached,
                "Repeated manual click must acknowledge the cached result");
        Require(updater.Get().state == update::State::Available &&
                updater.Get().release.version == "0.4.2" && updater.Get().error.empty(),
                "A repeated check must keep the last result without an unnecessary warning");
        updater.Check(false);
        for (int i = 0; i < 8; ++i) updater.Tick();
        Require(requests == 1, "Repeated manual or automatic checks must reuse the last result");
    }

    {
        update::Updater restarted(root / "install", cache, "0.4.1", fetch);
        Require(restarted.Get().state == update::State::Available &&
                    restarted.Get().release.version == "0.4.2",
                "A restarted launcher must load its last valid release result");
        Require(restarted.Check(true) == update::CheckResult::Cached,
                "Manual click after restart must acknowledge persisted cache");
        Require(restarted.Get().state == update::State::Available &&
                restarted.Get().error.empty(),
                "The manual cooldown must silently reuse the cached result after restart");
        Require(requests == 1, "Restarting within the cooldown must not call GitHub again");
    }
}

void IntervalBoundaries(const fs::path& root) {
    const auto autoCache = root / "automatic" / "cache";
    const auto manualCache = root / "manual" / "cache";
    const auto now = Now();
    std::atomic<int> autoRequests{0};
    std::atomic<int> manualRequests{0};
    auto makeFetch = [&](std::atomic<int>& counter) {
        return [&counter](std::string_view, const fs::path& destination, uint64_t,
                          const std::function<bool(uint64_t, uint64_t)>&) {
            ++counter;
            Write(destination, Metadata("0.4.1"));
        };
    };
    Write(autoCache / "latest-release.json", Metadata("0.4.1"));
    SetCheckTime(autoCache, now - 7200 + 120);
    {
        update::Updater updater(root / "install", autoCache, "0.4.1", makeFetch(autoRequests));
        updater.Tick();
        Require(autoRequests == 0, "Automatic Tick must wait for the full two-hour interval");
    }
    SetCheckTime(autoCache, now - 7200 - 120);
    {
        update::Updater updater(root / "install", autoCache, "0.4.1", makeFetch(autoRequests));
        updater.Tick();
        Require(Wait(updater).state == update::State::Current, "Automatic refresh must finish normally");
        Require(autoRequests == 1, "Automatic Tick must refresh a two-hour-old result");
    }

    Write(manualCache / "latest-release.json", Metadata("0.4.1"));
    SetCheckTime(manualCache, now - 900 + 120);
    {
        update::Updater updater(root / "install", manualCache, "0.4.1", makeFetch(manualRequests));
        Require(updater.Check(true) == update::CheckResult::Cached,
                "Manual cooldown must acknowledge the click without a request");
        Require(manualRequests == 0, "Manual check must wait for the full 15-minute cooldown");
        Require(updater.Get().state == update::State::Current && updater.Get().error.empty(),
                "Manual cooldown must leave the last up-to-date status unchanged");
    }
    SetCheckTime(manualCache, now - 900 - 120);
    {
        update::Updater updater(root / "install", manualCache, "0.4.1", makeFetch(manualRequests));
        Require(updater.Check(true) == update::CheckResult::Started,
                "Manual check past the interval must start a real request");
        Require(Wait(updater).state == update::State::Current, "Eligible manual refresh must finish normally");
        Require(manualRequests == 1, "Manual check must refresh after 15 minutes");
    }
}

void ConcurrentChecks(const fs::path& root) {
    const auto cache = root / "concurrent" / "cache";
    std::atomic<int> calls{0};
    std::atomic<bool> release{false};
    update::Updater updater(root / "install", cache, "0.4.1",
        [&](std::string_view, const fs::path& destination, uint64_t,
            const std::function<bool(uint64_t, uint64_t)>&) {
            ++calls;
            const auto deadline = std::chrono::steady_clock::now() + 2s;
            while (!release && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(5ms);
            Write(destination, Metadata("0.4.1"));
        });
    Require(updater.Check(true) == update::CheckResult::Started,
            "First concurrent check must start a request");
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!calls && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
    const auto before = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) {
        Require(updater.Check(true) == update::CheckResult::Busy,
                "A click during an active check must be recognized as busy");
        updater.Check(false);
        updater.Tick();
    }
    const auto elapsed = std::chrono::steady_clock::now() - before;
    release = true;
    const auto result = Wait(updater);
    Require(result.state == update::State::Current, "Concurrent check fixture must complete");
    Require(calls == 1, "Concurrent check requests must not duplicate a network call");
    Require(elapsed < 1s, "Repeated checks must not block the launcher while the first request is active");
}

void CancelDuringCheck(const fs::path& root) {
    const auto cache = root / "cancel" / "cache";
    std::atomic<int> polls{0};
    std::atomic<bool> cancellationObserved{false};
    update::Updater updater(root / "install", cache, "0.4.1",
        [&](std::string_view url, const fs::path&, uint64_t,
            const std::function<bool(uint64_t, uint64_t)>& progress) {
            Require(url == update::kLatestReleaseUrl, "Cancelled check contacted an unexpected endpoint");
            Require(bool(progress), "Metadata fetch must receive a cancellation callback");
            const auto deadline = std::chrono::steady_clock::now() + 3s;
            while (std::chrono::steady_clock::now() < deadline) {
                ++polls;
                if (!progress(0, 1)) {
                    cancellationObserved = true;
                    throw std::runtime_error("Synthetic metadata transfer cancelled");
                }
                std::this_thread::sleep_for(5ms);
            }
            throw std::runtime_error("Synthetic metadata transfer did not observe cancellation");
        });

    updater.Check(true);
    const auto startedBy = std::chrono::steady_clock::now() + 2s;
    while (polls < 3 && std::chrono::steady_clock::now() < startedBy)
        std::this_thread::sleep_for(5ms);
    Require(polls >= 3, "Simulated metadata transfer must be running before cancellation");
    const auto started = std::chrono::steady_clock::now();
    updater.Cancel();
    const auto view = Wait(updater);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    Require(cancellationObserved, "Metadata transfer must observe the cancellation callback");
    Require(view.state == update::State::Idle && view.error.empty(),
            "Cancelled metadata check must return to Idle without displaying an error");
    Require(elapsed < 1s, "Cancelled metadata check must finish promptly");
    Require(!fs::exists(cache / "latest-release.json"),
            "Cancelled metadata check must not publish a release result");
}

void RateLimitBackoff(const fs::path& root, int code, int64_t retryAt) {
    const auto cache = root / ("limit-" + std::to_string(code));
    std::atomic<int> calls{0};
    auto fetch = [&](std::string_view, const fs::path&, uint64_t,
                     const std::function<bool(uint64_t, uint64_t)>&) {
        ++calls;
        throw mods::HttpError(code, retryAt);
    };
    {
        update::Updater updater(root / "install", cache, "0.4.1", fetch);
        Require(updater.Check(true) == update::CheckResult::Started,
                "First rate-limited request must be performed normally");
        const auto result = Wait(updater);
        Require(result.state == update::State::Failed, "Rate limit must show check failure");
        Require(result.error == "Unable to check for updates right now.",
                "API errors should be presented without HTTP codes or countdowns");
        Require(updater.Check(true) == update::CheckResult::Cached,
                "Rate-limit retry must acknowledge existing cooldown without a network request");
        Require(calls == 1, "A repeated rate-limited check must not call GitHub again");
        Require(updater.Get().error == result.error && updater.Get().state == update::State::Failed,
                "A blocked update check must keep a simple, stable message");
    }
    {
        update::Updater restarted(root / "install", cache, "0.4.1", fetch);
        Require(restarted.Check(true) == update::CheckResult::Cached,
                "A restart must preserve the rate-limit backoff");
        restarted.Tick();
        Require(calls == 1, "The API backoff must persist across launcher restarts");
    }
    std::ifstream file(cache / "check-state.txt");
    int64_t attempted = 0, persistedBlock = 0;
    Require(bool(file >> attempted >> persistedBlock), "Rate-limit state must be persisted");
    Require(persistedBlock >= (retryAt ? retryAt : Now()),
            "Rate-limit wait must respect the retry time or a safe fallback");
}

void FailedRefreshPreservesCache(const fs::path& root) {
    const auto cache = root / "failure" / "cache";
    Write(cache / "latest-release.json", Metadata("0.4.2"));
    {
        update::Updater updater(root / "install", cache, "0.4.1",
            [&](std::string_view, const fs::path& destination, uint64_t,
                const std::function<bool(uint64_t, uint64_t)>&) {
                Write(destination, "{invalid metadata");
            });
        updater.Check(true);
        const auto view = Wait(updater);
        Require(view.state == update::State::Available && view.release.version == "0.4.2" &&
                    view.error.empty(),
                "Failed refresh must keep the cached available version visible without a warning");
    }
    update::Updater restarted(root / "install", cache, "0.4.1",
        [&](std::string_view, const fs::path&, uint64_t,
            const std::function<bool(uint64_t, uint64_t)>&) {
            throw std::runtime_error("Cached data unexpectedly triggered a network request");
        });
    Require(restarted.Get().state == update::State::Available &&
                restarted.Get().release.version == "0.4.2",
            "An invalid response must not overwrite the last valid cached release");
}
void FailedCurrentRefreshKeepsStatus(const fs::path& root) {
    const auto cache = root / "cached-current" / "cache";
    Write(cache / "latest-release.json", Metadata("0.4.1"));
    std::atomic<int> calls{0};
    update::Updater updater(root / "install", cache, "0.4.1",
        [&](std::string_view, const fs::path&, uint64_t,
            const std::function<bool(uint64_t, uint64_t)>&) {
            ++calls;
            throw mods::HttpError(429, Now() + 1800);
        });
    updater.Check(true);
    const auto view = Wait(updater);
    Require(view.state == update::State::Current && view.error.empty(),
            "When GitHub rejects a refresh, cached up-to-date status should remain visible");
    updater.Check(true);
    Require(calls == 1 && updater.Get().state == update::State::Current,
            "A cached result must be reused during a rate-limit window without warnings");
}
}

int main() {
    try {
        Workspace workspace;
        CachedManualChecksAndRestart(workspace.root);
        IntervalBoundaries(workspace.root);
        ConcurrentChecks(workspace.root);
        CancelDuringCheck(workspace.root);
        RateLimitBackoff(workspace.root, 429, Now() + 3600);
        RateLimitBackoff(workspace.root, 403, 0);
        FailedRefreshPreservesCache(workspace.root);
        FailedCurrentRefreshKeepsStatus(workspace.root);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: update cooldowns, persisted cache, auto cadence, concurrency, cancellation and HTTP backoff\n";
    return 0;
}
