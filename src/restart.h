#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
namespace openshao::launcher {struct Settings;}

namespace openshao::restart {
void GameSource(const std::filesystem::path& source,bool disc);
std::filesystem::path CurrentGameSource();
void SaveDirectory(const std::filesystem::path& directory);
const std::filesystem::path& ActiveSaveDirectory();  // the folder holding the console drives
bool Prepare(const launcher::Settings& settings,std::string& error);
bool Commit(std::string& error);
void Cancel();
bool ContinueStartup(std::string& error);
bool Resuming();
void BeginResume(bool enabled);
void MainMenuReady();
}
