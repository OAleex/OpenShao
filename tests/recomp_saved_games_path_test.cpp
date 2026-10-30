#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
static fs::path saved_games;
static HRESULT WINAPI MissingKnownFolder(REFKNOWNFOLDERID, DWORD flags, HANDLE, PWSTR* result) {
    *result = nullptr;
    if (!(flags & (KF_FLAG_DONT_VERIFY | KF_FLAG_CREATE))) return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
    if (flags & KF_FLAG_CREATE) fs::create_directories(saved_games);
    const auto text = saved_games.wstring();
    *result = static_cast<PWSTR>(CoTaskMemAlloc((text.size() + 1) * sizeof(wchar_t)));
    std::wcscpy(*result, text.c_str());
    return S_OK;
}
// The OS lookup is isolated because deleting the real user's known folder is unsafe.
#define SHGetKnownFolderPath MissingKnownFolder
#include "../src/save_storage.cpp"
#undef SHGetKnownFolderPath

int main() {
    const auto root = fs::temp_directory_path() / (L"openshao-missing-known-folder-" + std::to_wstring(GetCurrentProcessId()));
    saved_games = root / L"Saved Games";
    std::string error;
    const auto result = openshao::storage::SavedGamesRoot(error, false);
    if (result != saved_games / L"OpenShao" || fs::exists(root)) {
        std::cerr << "FAIL: a missing Saved Games folder must resolve without creating it or preventing startup: " << error << '\n';
        return 1;
    }
    const auto created = openshao::storage::SavedGamesRoot(error, true);
    const bool ok = created == saved_games / L"OpenShao" && fs::is_directory(saved_games);
    fs::remove_all(root);
    if (!ok) { std::cerr << "FAIL: explicit creation must still create the known folder\n"; return 1; }
    std::cout << "PASS: missing Saved Games does not block path resolution or mutate read-only queries\n";
}
