#include "http.h"
#include <fstream>
#include <array>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>
#include "version.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <dlfcn.h>
#endif
namespace openshao::mods {
HttpError::HttpError(int code, int64_t retry)
    : std::runtime_error("Mod server returned HTTP " + std::to_string(code)), status(code), retry_at(retry) {}
namespace {
int64_t SecondsNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string_view Trim(std::string_view s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == s.npos) return {};
    return s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
}
int64_t Number(std::string_view value) {
    value = Trim(value);
    int64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && result >= 0 ? result : 0;
}
int64_t RetryAt(std::string_view value, bool reset) {
    value = Trim(value);
    if (value.empty()) return 0;
    if (reset) return Number(value);
    if (value.find_first_not_of("0123456789") == value.npos)
        return SecondsNow() + std::min<int64_t>(Number(value), 86400);
    std::tm date{};
    std::istringstream input{std::string(value)};
    input.imbue(std::locale::classic());
    input >> std::get_time(&date, "%a, %d %b %Y %H:%M:%S GMT");
    if (input.fail()) return 0;
#ifdef _WIN32
    return _mkgmtime(&date);
#else
    return timegm(&date);
#endif
}
struct Backoff {
    int64_t retry_after = 0, reset = 0;
    void Add(std::string_view key, std::string_view value) {
        if (key == "retry-after") retry_after = RetryAt(value, false);
        if (key == "x-ratelimit-reset") reset = RetryAt(value, true);
    }
    int64_t Until() const { return std::max(retry_after, reset); }
};
struct Output {
    std::ofstream file;uint64_t done=0,maximum=0;Progress progress;std::string error;
    Output(const std::filesystem::path& path,uint64_t max,const Progress& p):file(path,std::ios::binary),maximum(max),progress(p) {
        if(!file)throw std::runtime_error("Cannot create download file; check folder permissions");
    }
    void Write(const char* bytes,size_t n) {
        if(n>maximum-done)throw std::runtime_error("Download exceeds its declared size");
        file.write(bytes,n);if(!file)throw std::runtime_error("Cannot save download; check free disk space");
        done+=n;Report(progress,"Downloading",done,maximum);
    }
    void Close() {file.close();if(!file)throw std::runtime_error("Cannot finish download; check free disk space");}
};
#ifdef _WIN32
struct Internet {
    HINTERNET handle=nullptr;
    explicit Internet(HINTERNET h):handle(h) {if(!h)throw std::runtime_error("Network request failed (Windows error "+std::to_string(GetLastError())+")");}
    ~Internet(){WinHttpCloseHandle(handle);}
};
std::string Header(HINTERNET request, const wchar_t* key) {
    DWORD bytes = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, key, nullptr, &bytes, WINHTTP_NO_HEADER_INDEX);
    if (!bytes || bytes > 4096) return {};
    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, key, buffer.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) return {};
    return std::string(buffer.begin(), buffer.begin() + bytes / sizeof(wchar_t) -
        (bytes >= sizeof(wchar_t) && buffer[bytes / sizeof(wchar_t) - 1] == L'\0' ? 1 : 0));
}
#else
struct Curl {
    void* library=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);
    using Easy=void;
    Easy* (*init)()=nullptr;void (*cleanup)(Easy*)=nullptr;int (*setopt)(Easy*,int,...)=nullptr;
    int (*perform)(Easy*)=nullptr;int (*getinfo)(Easy*,int,...)=nullptr;const char* (*strerror)(int)=nullptr;
    Curl() {
        if(!library)throw std::runtime_error("Install libcurl.so.4 to download mods");
        init=reinterpret_cast<decltype(init)>(dlsym(library,"curl_easy_init"));
        cleanup=reinterpret_cast<decltype(cleanup)>(dlsym(library,"curl_easy_cleanup"));
        setopt=reinterpret_cast<decltype(setopt)>(dlsym(library,"curl_easy_setopt"));
        perform=reinterpret_cast<decltype(perform)>(dlsym(library,"curl_easy_perform"));
        getinfo=reinterpret_cast<decltype(getinfo)>(dlsym(library,"curl_easy_getinfo"));
        strerror=reinterpret_cast<decltype(strerror)>(dlsym(library,"curl_easy_strerror"));
        if(!init||!cleanup||!setopt||!perform||!getinfo||!strerror){dlclose(library);throw std::runtime_error("The installed libcurl is incompatible");}
    }
    ~Curl(){if(library)dlclose(library);}
};
size_t Receive(char* bytes,size_t size,size_t count,void* user) {
    auto& out=*static_cast<Output*>(user);
    try {if(count&&size>SIZE_MAX/count)throw std::runtime_error("Invalid download chunk");out.Write(bytes,size*count);return size*count;}
    catch(const std::exception& e){out.error=e.what();return 0;}
}
size_t ReceiveHeader(char* bytes,size_t size,size_t count,void* user) {
    if (count && size > SIZE_MAX / count) return 0;
    auto& backoff = *static_cast<Backoff*>(user);
    std::string_view line(bytes, size * count);
    const auto colon = line.find(':');
    if (colon != line.npos && colon < 64) {
        std::string key(line.substr(0, colon));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        backoff.Add(key, line.substr(colon + 1));
    }
    return size * count;
}
int Tick(void* user,int64_t,int64_t,int64_t,int64_t) {
    auto& out=*static_cast<Output*>(user);
    try{Report(out.progress,"Downloading",out.done,out.maximum);return 0;}catch(const std::exception& e){out.error=e.what();return 1;}
}
#endif
}
void Download(std::string_view url,const std::filesystem::path& file,uint64_t maximum,const Progress& progress) {
    if(!url.starts_with("https://")||url.find_first_of("\r\n")!=url.npos)throw std::runtime_error("Only HTTPS mod downloads are supported");
    Output out(file,maximum,progress);Report(progress,"Connecting");
#ifdef _WIN32
    const std::wstring wide(url.begin(),url.end());URL_COMPONENTS parts{sizeof(parts)};
    parts.dwHostNameLength=DWORD(-1);parts.dwUrlPathLength=DWORD(-1);parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(wide.c_str(),DWORD(wide.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)throw std::runtime_error("Invalid HTTPS URL");
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    const std::string agent = std::string("OpenShao/") + RecompVersion;
    Internet session(WinHttpOpen(std::wstring(agent.begin(), agent.end()).c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    WinHttpSetTimeouts(session.handle,3000,3000,3000,3000);
    Internet connection(WinHttpConnect(session.handle,host.c_str(),parts.nPort,0));
    Internet request(WinHttpOpenRequest(connection.handle,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request.handle,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.handle,nullptr))
        throw std::runtime_error("Cannot download mods; check your connection (Windows error "+std::to_string(GetLastError())+")");
    DWORD code=0,length=sizeof(code);
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error("Cannot read HTTP response status");
    if(code!=200) {
        Backoff backoff;
        backoff.Add("retry-after", Header(request.handle, L"Retry-After"));
        backoff.Add("x-ratelimit-reset", Header(request.handle, L"X-RateLimit-Reset"));
        throw HttpError(int(code), backoff.Until());
    }
    std::array<char,65536> buffer;
    while(true){DWORD got=0;if(!WinHttpReadData(request.handle,buffer.data(),DWORD(buffer.size()),&got))throw std::runtime_error("Mod download was interrupted");if(!got)break;out.Write(buffer.data(),got);}
#else
    Curl curl;auto* easy=curl.init();if(!easy)throw std::runtime_error("Cannot create HTTPS request");
    Backoff backoff;
    try {
        const std::string address(url);
        auto set=[&](int option,auto value){if(curl.setopt(easy,option,value))throw std::runtime_error("Cannot configure HTTPS request");};
        // Stable libcurl easy-option ABI; no development headers or link dependency.
        const std::string agent = std::string("OpenShao/") + RecompVersion;
        set(10002,address.c_str());set(10018,agent.c_str());set(52,1L);set(68,5L);
        set(181,2L);set(182,2L);set(99,1L);set(78,10L);set(19,1L);set(20,20L);
        set(20011,&Receive);set(10001,&out);set(43,0L);set(20219,&Tick);set(10057,&out);
        set(20079,&ReceiveHeader);set(10029,&backoff);
        const auto status=curl.perform(easy);long code=0;curl.getinfo(easy,0x200002,&code);
        if(!out.error.empty())throw std::runtime_error(out.error);
        if(status)throw std::runtime_error(std::string("Mod download failed: ")+curl.strerror(status));
        if(code!=200)throw HttpError(int(code),backoff.Until());
    }catch(...){curl.cleanup(easy);throw;}
    curl.cleanup(easy);
#endif
    out.Close();
}
}
