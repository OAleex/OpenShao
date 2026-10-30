#include "diagnostic_privacy.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <regex>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace openshao::privacy {
namespace {
bool Word(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 128; }
std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
const std::vector<std::string>& Identities() {
    static const auto values = [] {
        std::vector<std::string> result;
        for (const auto* key : {"USERNAME", "USER", "COMPUTERNAME", "USERDOMAIN", "LOGNAME"}) {
#ifdef _WIN32
            const auto count=GetEnvironmentVariableA(key,nullptr,0);
            if(count && count<=32768) {
                std::string value(count,'\0');
                const auto copied=GetEnvironmentVariableA(key,value.data(),count);
                if(copied && copied<count) { value.resize(copied);result.push_back(Lower(std::move(value))); }
            }
#else
            if (const auto* value = std::getenv(key); value && *value) result.push_back(Lower(value));
#endif
        }
#ifdef _WIN32
        char name[256]{}; DWORD size = sizeof(name);
        if (GetComputerNameA(name, &size)) result.push_back(Lower(name));
        size = sizeof(name);
        if (GetUserNameA(name, &size)) result.push_back(Lower(name));
        auto unicode = [&](const wchar_t* value) {
            const int count=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
            if(count>1) { std::string text(count,'\0');WideCharToMultiByte(CP_UTF8,0,value,-1,text.data(),count,nullptr,nullptr);
                text.pop_back();result.push_back(Lower(std::move(text))); }
        };
        wchar_t wide[256]{};size=256;
        if(GetComputerNameW(wide,&size)) unicode(wide);
        size=256;if(GetUserNameW(wide,&size)) unicode(wide);
#else
        char name[256]{};
        if (!gethostname(name, sizeof(name)-1)) result.push_back(Lower(name));
#endif
        return result;
    }();
    return values;
}
}
std::string Redact(std::string_view input) {
    std::string text(input);
    static const std::pair<std::regex, const char*> rules[] = {
        {std::regex(R"((https?|ftp)://[^\s<>"']+)", std::regex::icase), "<URL>"},
        {std::regex(R"(\b[A-Z0-9._%+\-]+@[A-Z0-9.\-]+\.[A-Z]{2,}\b)", std::regex::icase), "<EMAIL>"},
        {std::regex(R"((authorization["']?\s*[:=]\s*)([^\r\n]+))", std::regex::icase), "$1<REDACTED>"},
        {std::regex(R"((\b(token|password|passwd|secret|api[_-]?key|access[_-]?key)["']?\s*[=:]\s*)("(\\.|[^"\\\r\n])*"|'(\\.|[^'\\\r\n])*'|[^\s&;,]+))", std::regex::icase), "$1<REDACTED>"},
        {std::regex(R"((\b(username|computername|hostname|account|userdomain)["']?\s*[:=]\s*)[^\r\n;,]+)", std::regex::icase), "$1<IDENTITY>"},
        // Paths may contain spaces. Prefer losing the rest of a path-bearing line to leaking a profile name.
        {std::regex(R"(([A-Z]:[\\/]|\\\\|//)[^\r\n"<>|]*)", std::regex::icase), "<PATH>"},
        {std::regex(R"((^|[\s="'(])/(?!/)[^\r\n"<>]*)"), "$1<PATH>"},
        {std::regex(R"(\b([0-9a-f]{1,4}:){2,}[0-9a-f:]*\b|::[0-9a-f:]+\b)", std::regex::icase), "<IP>"}
    };
    for (const auto& [pattern, replacement] : rules) text = std::regex_replace(text, pattern, replacement);
    static const std::regex ipv4(R"(\b\d{1,3}(\.\d{1,3}){3}\b)");
    std::string filtered; size_t copied = 0;
    for (std::sregex_iterator i(text.begin(), text.end(), ipv4), end; i != end; ++i) {
        const auto candidate = i->str(); unsigned a,b,c,d;
        if (std::sscanf(candidate.c_str(), "%u.%u.%u.%u", &a,&b,&c,&d) == 4 && a<=255 && b<=255 && c<=255 && d<=255) {
            filtered.append(text, copied, size_t(i->position())-copied); filtered += "<IP>";
            copied = size_t(i->position()+i->length());
        }
    }
    filtered.append(text, copied, std::string::npos); text = std::move(filtered);
    for (const auto& identity : Identities()) {
        if (identity.empty()) continue;
        auto lower = Lower(text); size_t start = 0;
        while ((start = lower.find(identity, start)) != std::string::npos) {
            const auto end = start+identity.size();
            if ((start == 0 || !Word(text[start-1])) && (end == text.size() || !Word(text[end]))) {
                text.replace(start, identity.size(), "<IDENTITY>"); lower = Lower(text); start += 10;
            } else start = end;
        }
    }
    return text;
}
}
