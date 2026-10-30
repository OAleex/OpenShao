#include "http.h"
#include <fstream>
#include <array>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <dlfcn.h>
#endif
namespace openshao::mods {
namespace {
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
    Internet session(WinHttpOpen(L"OpenShao/0.4.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    WinHttpSetTimeouts(session.handle,3000,3000,3000,3000);
    Internet connection(WinHttpConnect(session.handle,host.c_str(),parts.nPort,0));
    Internet request(WinHttpOpenRequest(connection.handle,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request.handle,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.handle,nullptr))
        throw std::runtime_error("Cannot download mods; check your connection (Windows error "+std::to_string(GetLastError())+")");
    DWORD code=0,length=sizeof(code);
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX)||code!=200)
        throw std::runtime_error("Mod server returned HTTP "+std::to_string(code));
    std::array<char,65536> buffer;
    while(true){DWORD got=0;if(!WinHttpReadData(request.handle,buffer.data(),DWORD(buffer.size()),&got))throw std::runtime_error("Mod download was interrupted");if(!got)break;out.Write(buffer.data(),got);}
#else
    Curl curl;auto* easy=curl.init();if(!easy)throw std::runtime_error("Cannot create HTTPS request");
    try {
        const std::string address(url);
        auto set=[&](int option,auto value){if(curl.setopt(easy,option,value))throw std::runtime_error("Cannot configure HTTPS request");};
        // Stable libcurl easy-option ABI; no development headers or link dependency.
        set(10002,address.c_str());set(10018,"OpenShao/0.4.0");set(52,1L);set(68,5L);
        set(181,2L);set(182,2L);set(99,1L);set(78,10L);set(19,1L);set(20,20L);
        set(20011,&Receive);set(10001,&out);set(43,0L);set(20219,&Tick);set(10057,&out);
        const auto status=curl.perform(easy);long code=0;curl.getinfo(easy,0x200002,&code);
        if(!out.error.empty())throw std::runtime_error(out.error);
        if(status)throw std::runtime_error(std::string("Mod download failed: ")+curl.strerror(status));
        if(code!=200)throw std::runtime_error("Mod server returned HTTP "+std::to_string(code));
    }catch(...){curl.cleanup(easy);throw;}
    curl.cleanup(easy);
#endif
    out.Close();
}
}
