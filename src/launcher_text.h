#pragma once
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Launcher captions: one embedded resources/launcher/<code>.json per language, independent of the game language.
namespace openshao::launcher::text {
struct Language {
    std::string code, name;  // name is the language's own name
};
struct Catalog {
    std::string code, name;
    std::vector<std::pair<std::string, std::string>> strings;  // sorted by key
};

// Parses one catalog file. Throws std::runtime_error on invalid content.
Catalog Parse(std::string_view json);
// Checks that every catalog has the English keys and placeholders. Returns an empty string when valid.
std::string Validate(std::span<const Catalog> catalogs);

// Embedded languages: the game's language order first, then any other file by code.
const std::vector<Language>& Languages();
bool Supported(std::string_view code);
// Picks a language from system locales ("pt_BR", "de"): exact match, then primary subtag, else "en".
std::string FromLocales(std::span<const std::string> locales);

void Select(std::string_view code);
const std::string& Current();
// The caption in the current language, then English, then the key itself.
const std::string& Get(std::string_view key);
const char* C(std::string_view key);
// Replaces {name} placeholders in the current caption.
std::string Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values);
}
