#pragma once
#include <string>
#include <toml++/toml.hpp>
#include "launcher_settings.h"

namespace openshao::launcher {
bool Show();
bool DiscordPresenceEnabled();
bool ShouldPersist();
void Load(const void* table);
void Save(void* table);
bool Persist(const toml::table& table, std::string& error);
unsigned Language();
unsigned VoiceLanguageIndex();
float AudioGain();
const Settings& Current();
bool ApplyInGame(const Settings& settings,bool save,std::string& error);
Settings Saved();
bool LanguageAvailable(std::string_view language);
}
