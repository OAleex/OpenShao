#include "launcher_text.h"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include "launcher_settings.h"
#include "launcher_text_data.h"
#include "mods/json.h"

namespace openshao::launcher::text {
namespace {
std::string Lower(std::string_view value) {
    std::string out(value);
    for (auto& c : out) {
        if (c == '_') c = '-';
        else if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return out;
}
std::string_view Primary(std::string_view code) { return code.substr(0, code.find('-')); }
const std::string* Find(const Catalog& catalog, std::string_view key) {
    const auto found = std::lower_bound(catalog.strings.begin(), catalog.strings.end(), key,
                                        [](const auto& entry, std::string_view k) { return entry.first < k; });
    return found != catalog.strings.end() && found->first == key ? &found->second : nullptr;
}
std::vector<std::string_view> Placeholders(std::string_view text) {
    std::vector<std::string_view> out;
    for (size_t at = text.find('{'); at != text.npos; at = text.find('{', at + 1)) {
        const auto end = text.find('}', at);
        if (end == text.npos) break;
        out.push_back(text.substr(at, end - at + 1));
    }
    std::sort(out.begin(), out.end());
    return out;
}
int GameOrder(std::string_view code) {
    for (size_t i = 0; i < languages.size(); ++i) if (Lower(languages[i]) == Lower(code)) return int(i);
    return int(languages.size());
}
struct Embedded {
    std::vector<Catalog> catalogs;
    std::vector<Language> list;
    Embedded() {
        for (const auto json : EmbeddedCatalogs) catalogs.push_back(Parse(json));
        std::stable_sort(catalogs.begin(), catalogs.end(), [](const Catalog& a, const Catalog& b) {
            const int x = GameOrder(a.code), y = GameOrder(b.code);
            return x != y ? x < y : a.code < b.code;
        });
        for (const auto& catalog : catalogs) list.push_back({catalog.code, catalog.name});
    }
    const Catalog* ByCode(std::string_view code) const {
        for (const auto& catalog : catalogs) if (Lower(catalog.code) == Lower(code)) return &catalog;
        return nullptr;
    }
};
const Embedded& Data() { static const Embedded data; return data; }
const Catalog* English() { return Data().ByCode("en"); }
std::atomic<const Catalog*> selected{nullptr};
const Catalog* Active() {
    const auto* catalog = selected.load();
    return catalog ? catalog : English();
}
}

Catalog Parse(std::string_view json) {
    const auto root = mods::Json::Parse(json);
    if (!root.Has("language") || !root.Has("name") || !root.Has("strings")) throw std::runtime_error("Launcher text needs language, name and strings");
    Catalog catalog{root.At("language").Text(), root.At("name").Text(), {}};
    if (catalog.code.empty() || catalog.name.empty()) throw std::runtime_error("Launcher text has an empty language or name");
    const auto& strings = root.At("strings");
    if (!std::holds_alternative<mods::Json::Object>(strings.value)) throw std::runtime_error("Launcher strings must be an object");
    for (const auto& [key, value] : std::get<mods::Json::Object>(strings.value)) {
        if (!std::holds_alternative<std::string>(value.value)) throw std::runtime_error("Launcher string " + key + " must be text");
        catalog.strings.emplace_back(key, value.Text());
    }
    // The JSON object map is already sorted by key.
    return catalog;
}

std::string Validate(std::span<const Catalog> catalogs) {
    const Catalog* english = nullptr;
    for (const auto& catalog : catalogs) if (catalog.code == "en") english = &catalog;
    if (!english) return "The English launcher text is missing";
    for (const auto& catalog : catalogs) {
        if (std::count_if(catalogs.begin(), catalogs.end(), [&](const Catalog& c) { return Lower(c.code) == Lower(catalog.code); }) != 1)
            return "Launcher language " + catalog.code + " is defined more than once";
        for (const auto& [key, value] : english->strings) {
            const auto* translated = Find(catalog, key);
            if (!translated || translated->empty()) return catalog.code + " is missing " + key;
            if (Placeholders(*translated) != Placeholders(value)) return catalog.code + " changes the placeholders of " + key;
        }
        for (const auto& [key, value] : catalog.strings)
            if (!Find(*english, key)) return catalog.code + " has the unknown key " + key;
    }
    return {};
}

const std::vector<Language>& Languages() { return Data().list; }
bool Supported(std::string_view code) { return Data().ByCode(code) != nullptr; }

std::string FromLocales(std::span<const std::string> locales) {
    for (const auto& locale : locales) {
        const auto wanted = Lower(locale);
        if (wanted.empty()) continue;
        for (const auto& language : Languages()) if (Lower(language.code) == wanted) return language.code;
        for (const auto& language : Languages()) {
            const auto code = Lower(language.code);
            if (Primary(code) == Primary(wanted)) return language.code;
        }
    }
    return "en";
}

void Select(std::string_view code) {
    const auto* catalog = Data().ByCode(code);
    selected.store(catalog ? catalog : English());
}
const std::string& Current() { return Active()->code; }

const std::string& Get(std::string_view key) {
    if (const auto* value = Find(*Active(), key)) return *value;
    if (const auto* value = Find(*English(), key)) return *value;
    static thread_local std::string missing;
    missing.assign(key);
    return missing;
}
const char* C(std::string_view key) { return Get(key).c_str(); }

std::string Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    const std::string_view source = Get(key);
    std::string out;
    out.reserve(source.size());
    // One pass, so inserted values are never formatted again.
    for (size_t at = 0; at < source.size();) {
        const auto open = source.find('{', at), close = open == source.npos ? open : source.find('}', open);
        if (close == source.npos) { out.append(source.substr(at)); break; }
        out.append(source.substr(at, open - at));
        const auto name = source.substr(open + 1, close - open - 1);
        const auto value = std::find_if(values.begin(), values.end(), [&](const auto& entry) { return entry.first == name; });
        if (value != values.end()) out.append(value->second);
        else out.append(source.substr(open, close - open + 1));
        at = close + 1;
    }
    return out;
}
}
