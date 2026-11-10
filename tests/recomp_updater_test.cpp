// Checks release parsing, package reading, installation and the update worker without network access.
#include "updater.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "file_ops.h"
#include "mods/sha256.h"
namespace fs = std::filesystem;
using namespace openshao;
void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string Get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
void Put(const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; }
std::string Release(const std::string& tag, const std::string& digest, bool checksums, uint64_t size = 4) {
    const auto name = update::PackageName(tag.starts_with('v') ? tag.substr(1) : tag);
    std::string assets = "{\"name\":\"" + name + "\",\"size\":" + std::to_string(size) + ",\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/" + tag + "/" + name + "\"" +
                         (digest.empty() ? "" : ",\"digest\":\"" + digest + "\"") + "}";
    assets += ",{\"name\":\"OpenShao-9.9.9-Other-arm64.zip\",\"size\":1,\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/x/other\"}";
    if (checksums) assets += ",{\"name\":\"checksums.txt\",\"size\":10,\"browser_download_url\":\"https://github.com/OAleex/OpenShao/releases/download/" + tag + "/checksums.txt\"}";
    return "{\"tag_name\":\"" + tag + "\",\"html_url\":\"https://github.com/OAleex/OpenShao/releases/tag/" + tag +
           "\",\"draft\":false,\"prerelease\":false,\"body\":\"Fixes \\u00e7 \\ud83d\\ude00\",\"assets\":[" + assets + "]}";
}
int main(int argc, char** argv) {
    const auto base = fs::temp_directory_path() / ("openshao-updater-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Require(update::ReleaseNotesUrl("0.4.1") == "https://github.com/OAleex/OpenShao/releases/tag/v0.4.1",
                "Release notes must open the installed version instead of the latest release");
        const std::string hash(64, 'a');
        auto parsed = update::ParseLatest(Release("v0.3.1", "sha256:" + std::string(64, 'A'), true));
        Require(parsed.version == "0.3.1", "The tag prefix must be removed");
        Require(parsed.package_name == update::PackageName("0.3.1") && parsed.package_url.ends_with(parsed.package_name), "The platform package must be selected");
        Require(parsed.sha256 == hash && parsed.size == 4, "The asset digest and size must be read");
        Require(parsed.checksums_url.ends_with("/checksums.txt"), "The checksum list must be found");
        parsed = update::ParseLatest(Release("0.4.0", "", false));
        Require(parsed.sha256.empty() && parsed.checksums_url.empty() && !parsed.package_url.empty(), "Missing digests must stay empty");
        bool rejected = false;
        try { update::ParseLatest("{\"tag_name\":\"latest\"}"); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "A tag that is not a version must be rejected");
        const auto foreign = update::ParseLatest("{\"tag_name\":\"v1.0.0\",\"assets\":[{\"name\":\"" + update::PackageName("1.0.0") +
                                                 "\",\"browser_download_url\":\"https://example.com/" + update::PackageName("1.0.0") + "\"}]}");
        Require(foreign.package_url.empty(), "Packages outside the project's releases must be ignored");
        Require(update::ChecksumFor(hash + "  other.zip\r\n" + std::string(64, 'B') + " *" + update::PackageName("0.4.0") + "\n", update::PackageName("0.4.0")) == std::string(64, 'b'),
                "Checksum listings must match the file name");
        Require(update::ChecksumFor("not a hash  file\n", "file").empty(), "Invalid checksum rows must be ignored");

        // Real packages created by tools/package_release.py, passed by tests/updater_test.py.
        if (argc > 1) {
            const fs::path package = argv[1];
            const auto files = update::ReadPackage(package);
            Require(files.contains(update::ExecutableName()) && files.contains("readme.txt") && files.contains("licenses/LICENSE.txt"),
                    "The package must contain the executable, readme and licenses");
            const auto install = base / "install folder";
            Put(install / update::ExecutableName(), "old executable");
            Put(install / "readme.txt", "old readme");
            Put(install / "keep.txt", "user file");
            update::Install(install, files);
            const auto& exe = files.at(update::ExecutableName());
            Require(Get(install / update::ExecutableName()) == std::string(exe.begin(), exe.end()), "The executable must be replaced");
            Require(Get(install / (update::ExecutableName() + ".old")) == "old executable", "The previous executable must stay until the next start");
            Require(Get(install / "keep.txt") == "user file", "Files outside the package must stay");
            Require(fs::exists(install / "licenses/LICENSE.txt"), "New package folders must be created");
#ifndef _WIN32
            Require((fs::status(install / update::ExecutableName()).permissions() & fs::perms::owner_exec) != fs::perms::none, "The executable must stay executable");
#endif
            update::RemovePreviousFiles(install);
            Require(!fs::exists(install / (update::ExecutableName() + ".old")) && !fs::exists(install / "readme.txt.old"), "Previous files must be removed on the next start");
            Require(Get(install / "keep.txt") == "user file", "Cleanup must keep other files");

            // A failed installation restores the files it replaced.
            const auto broken = base / "broken";
            Put(broken / update::ExecutableName(), "running");
            Put(broken / "licenses", "a file where the package needs a folder");
            bool failed = false;
            try { update::Install(broken, files); } catch (const std::exception&) { failed = true; }
            Require(failed, "A conflicting package must fail");
            Require(Get(broken / update::ExecutableName()) == "running", "A failed update must restore the executable");
            Require(!fs::exists(broken / (update::ExecutableName() + ".new")), "A failed update must remove staged files");
            Require(Get(broken / "licenses") == "a file where the package needs a folder", "A failed update must keep existing files");

            // The worker checks, downloads, verifies and installs through an injected transfer.
            const auto release_dir = base / "release";
            const auto copy = release_dir / update::PackageName("9.0.0");
            fs::create_directories(release_dir);
            fs::copy_file(package, copy);
            const auto digest = mods::Sha256File(copy);
            for (const bool corrupt : {true, false}) {
                const auto target = base / (corrupt ? "corrupt" : "worker");
                Put(target / update::ExecutableName(), "old");
                update::Updater updater(target, base / (corrupt ? "cache-corrupt" : "cache"), "0.3.0",
                    [&](std::string_view url, const fs::path& file, uint64_t maximum, const std::function<bool(uint64_t, uint64_t)>& progress) {
                        if (url == update::kLatestReleaseUrl) { Put(file, Release("v9.0.0", "", true, fs::file_size(copy))); return; }
                        if (url.ends_with("/checksums.txt")) { Put(file, (corrupt ? std::string(64, 'c') : digest) + "  " + update::PackageName("9.0.0") + "\n"); return; }
                        Require(fs::file_size(copy) <= maximum, "The package must fit the declared limit");
                        fs::copy_file(copy, file, fs::copy_options::overwrite_existing);
                        if (progress) progress(fs::file_size(copy), fs::file_size(copy));
                    });
                auto wait = [&](update::State busy) {
                    for (int i = 0; i < 500 && updater.Get().state == busy; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return updater.Get();
                };
                updater.Check();
                auto view = wait(update::State::Checking);
                Require(view.state == update::State::Available && view.release.version == "9.0.0", "A newer release must be offered: " + view.error);
                updater.Download();
                view = wait(update::State::Downloading);
                if (corrupt) {
                    Require(view.state == update::State::Failed && view.error.find("SHA-256") != std::string::npos, "A checksum mismatch must stop the update");
                    Require(Get(target / update::ExecutableName()) == "old", "A rejected package must not change files");
                } else {
                    Require(view.state == update::State::Ready, "A verified package must be installed: " + view.error);
                    Require(Get(target / update::ExecutableName()) == std::string(exe.begin(), exe.end()), "The worker must install the package");
                }
            }
            update::Updater current(base / "current", base / "cache-current", "9.0.0",
                [&](std::string_view, const fs::path& file, uint64_t, const std::function<bool(uint64_t, uint64_t)>&) { Put(file, Release("v9.0.0", "", true)); });
            current.Check();
            for (int i = 0; i < 500 && current.Get().state == update::State::Checking; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Require(current.Get().state == update::State::Current, "The installed version must not be offered again");
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        std::error_code ignored; fs::remove_all(base, ignored);
        return 1;
    }
    std::error_code ignored; fs::remove_all(base, ignored);
    std::cout << "PASS: release metadata, package formats, verified installation, rollback and cleanup\n";
    return 0;
}
