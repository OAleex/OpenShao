#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "core/log.h"
#include "support_report.h"

static void Environment(const char* name, const char* value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, 1);
#endif
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    Environment("USERNAME", "PrivateAccount");
    Environment("USER", "PrivateAccount");
    Environment("COMPUTERNAME", "PRIVATE-PC");
    Environment("USERDOMAIN", "PRIVATE-DOMAIN");
    Environment("USERPROFILE", "C:\\Users\\Private Person");
    logx::Init(argv[1], "privacy");
    LOGR("Native exception C0000005; renderer=d3d12; GPU=AMD Radeon; driver=31.0.21925.1001");
    LOGR("account PrivateAccount / privateaccount; computer PRIVATE-PC; domain PRIVATE-DOMAIN");
    LOGR("Path: C:\\Users\\Private Person\\Downloads\\secret folder\\OpenShao.exe");
    LOGR("Path: /home/private-person/games/secret.iso");
    LOGR("Network: \\\\PRIVATE-SERVER\\private-share\\game.iso");
    LOGR("Email: private.person@example.com; address=192.168.1.51; IPv6=2001:db8::abcd");
    LOGR("Authorization: Bearer private-token-value");
    LOGR("token=private-query-token&password=private-password");
    LOGR("Download https://example.com/private?token=secret");
    LOGR("Share //PRIVATE-SERVER/private-share/game.iso; loopback ::1");
    LOGR("Loopback ::1");
    LOGR("password=\"private value-END-TOKEN\"");
    LOGR("JSON: {\"token\": \"PRIVATE-JSON-CREDENTIAL\", \"hostname\": \"OLD-PRIVATE-HOST\"}");
    logx::Flush();
    std::ifstream input(logx::Path(), std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    for (const auto* value : {"PrivateAccount", "privateaccount", "PRIVATE-PC", "PRIVATE-DOMAIN", "Private Person",
            "private-person", "PRIVATE-SERVER", "private-share", "secret.iso", "private.person@example.com", "192.168.1.51",
            "2001:db8::abcd", "::1", "PRIVATE-JSON-CREDENTIAL", "OLD-PRIVATE-HOST", "value-END-TOKEN", "private-token-value", "private-query-token", "private-password", "https://example.com"}) {
        if (text.find(value) != std::string::npos) {
            std::cerr << "FAIL: a private account, machine, path or network value survived in the local log\n";
            return 1;
        }
    }
    for (const auto* value : {"C0000005", "renderer=d3d12", "AMD Radeon", "31.0.21925.1001"}) {
        if (text.find(value) == std::string::npos) {
            std::cerr << "FAIL: privacy filtering removed required technical diagnostics\n";
            return 1;
        }
    }
    const auto original=std::filesystem::current_path();
    const auto isolated=std::filesystem::absolute(argv[1])/"unavailable-data";
    std::filesystem::create_directories(isolated);
    std::filesystem::current_path(isolated);
    const auto report=openshao::support::Generate({{}, {}, "test"});
    const auto zip=report.zip.empty()?std::filesystem::path{}:std::filesystem::absolute(report.zip);
    std::filesystem::current_path(original);
    const bool fallback=report.zip.is_absolute() && zip.parent_path()==std::filesystem::temp_directory_path()/"OpenShao"/"reports";
    if(!zip.empty()) std::filesystem::remove(zip);
    if(!fallback) { std::cerr << "FAIL: unavailable application data must use temporary reports, without writing into the game folder\n";return 1; }
    std::cout << "PASS: local logs remove personal identifiers while retaining technical diagnostics\n";
}
