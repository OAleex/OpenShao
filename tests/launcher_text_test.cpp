#include <iostream>
#include <string>
#include <vector>
#include "launcher_settings.h"
#include "launcher_text.h"
#include "launcher_text_data.h"

namespace {
// The launcher atlas loads Basic Latin and Latin-1 Supplement only.
bool Latin1(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { ++i; continue; }
        if ((c & 0xE0) != 0xC0 || i + 1 >= text.size()) return false;
        const unsigned value = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
        if (value > 0xFF) return false;
        i += 2;
    }
    return true;
}
}

int main() {
    using namespace openshao::launcher;
    int failures = 0;
    const auto check = [&](bool good, const std::string& message) { if (!good) { ++failures; std::cerr << message << '\n'; } };

    std::vector<text::Catalog> catalogs;
    for (const auto json : text::EmbeddedCatalogs) {
        try { catalogs.push_back(text::Parse(json)); }
        catch (const std::exception& e) { check(false, std::string("Invalid launcher text: ") + e.what()); }
    }
    const auto error = text::Validate(catalogs);
    check(error.empty(), "Launcher catalogs must share the English keys and placeholders: " + error);
    for (const auto& catalog : catalogs) {
        for (const auto& [key, value] : catalog.strings)
            check(Latin1(value), catalog.code + " " + key + " uses a character outside the launcher font range");
        check(Latin1(catalog.name), catalog.code + " name uses a character outside the launcher font range");
    }

    // The launcher offers the same languages as the game, in the game's order.
    const auto& list = text::Languages();
    check(list.size() == languages.size(), "Every game language needs one launcher text file and no other file may exist");
    for (size_t i = 0; i < list.size() && i < languages.size(); ++i)
        check(list[i].code == languages[i], "Launcher languages must follow the game order at " + std::to_string(i));
    for (const auto& language : list) check(text::Supported(language.code), language.code + " must be supported");

    struct Case { std::vector<std::string> locales; const char* expected; };
    const Case cases[] = {
        {{"pt_BR"}, "pt-BR"}, {{"pt-PT"}, "pt-BR"}, {{"de-AT"}, "de"}, {{"ja-JP", "fr-CA"}, "fr"},
        {{}, "en"}, {{"zh-CN"}, "en"}, {{"EN-gb"}, "en"}, {{"es-419"}, "es"}, {{"it"}, "it"}, {{"", "fr"}, "fr"},
    };
    for (const auto& c : cases) {
        const auto result = text::FromLocales(c.locales);
        check(result == c.expected, "System locale mapping returned " + result + " instead of " + c.expected);
    }

    text::Select("de");
    check(text::Current() == "de" && text::Get("nav.play") == "Spielen", "Selecting German must change the captions");
    check(text::Format("play.release_notes", {{"version", "9.8.7"}}) == "Versionshinweise (9.8.7)", "Placeholders must be replaced");
    text::Select("pt-br");
    check(text::Current() == "pt-BR", "Language codes are case-insensitive");
    text::Select("xx");
    check(text::Current() == "en" && text::Get("nav.play") == "Play", "An unknown language falls back to English");
    check(text::Get("no.such.key") == "no.such.key", "A missing key shows the key");
    check(text::Format("mods.and", {{"first", "A"}, {"second", "{first}"}}) == "A and {first}", "Values are not formatted again");
    check(text::Format("mods.by", {{"details", "{authors}"}, {"authors", "B"}}) == "{authors}, by B", "Earlier values are not formatted again");

    if (failures) std::cerr << failures << " launcher text check(s) failed\n";
    return failures ? 1 : 0;
}
