#include "support_report.h"
#include "diagnostic_privacy.h"
#include <toml++/toml.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dbghelp.h>
#else
#include <SDL3/SDL.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace openshao::support {
namespace {
struct State {
    uint32_t status, ready, code, worker_pid;
    uint64_t address, image_base;
    uint32_t dump_ready, thread_id, context_valid, reserved;
    char padding[16];
    alignas(16) unsigned char native[4032];
};
static_assert(sizeof(State) == 4096 && std::atomic_ref<uint32_t>::is_always_lock_free);
State* monitor = nullptr;
std::string Utf8(const fs::path& path) { const auto s=path.u8string(); return {s.begin(),s.end()}; }
fs::path Path(const std::string& s) { return fs::path(std::u8string(s.begin(),s.end())); }
struct Mapping {
    State* state = nullptr;
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE, map = nullptr;
    bool Open(const fs::path& path) {
        file = CreateFileW(path.c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file,&length) || length.QuadPart != sizeof(State)) return false;
        map=CreateFileMappingW(file,nullptr,PAGE_READWRITE,0,sizeof(State),nullptr);
        if (map) state=static_cast<State*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(State)));
        return state != nullptr;
    }
    ~Mapping() { if(state) UnmapViewOfFile(state); if(map) CloseHandle(map); if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); }
#else
    int file = -1;
    bool Open(const fs::path& path) {
        file=open(path.c_str(),O_RDWR|O_CLOEXEC); if(file<0) return false;
        if(lseek(file,0,SEEK_END)!=sizeof(State)) return false;
        void* p=mmap(nullptr,sizeof(State),PROT_READ|PROT_WRITE,MAP_SHARED,file,0);
        if(p!=MAP_FAILED) state=static_cast<State*>(p);
        return state != nullptr;
    }
    ~Mapping() { if(state) munmap(state,sizeof(State)); if(file>=0) close(file); }
#endif
};
uint32_t Load(uint32_t& value) { return std::atomic_ref(value).load(std::memory_order_acquire); }
void Store(uint32_t& value,uint32_t next) { std::atomic_ref(value).store(next,std::memory_order_release); }
#ifdef _WIN32
struct NativeCrash { EXCEPTION_RECORD record; CONTEXT context; };
static_assert(sizeof(NativeCrash)<=sizeof(State::native));
struct CapturedDump { fs::path path; std::string status; };
CapturedDump CaptureDump(HANDLE process,DWORD pid,const State& state,const fs::path& data) {
    if(!process || WaitForSingleObject(process,0)!=WAIT_TIMEOUT) return {{},"process_already_exited"};
    fs::path partial;
    try {
        const auto folder=data/"crashes";fs::create_directories(folder);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto dump=folder/("OpenShao-"+std::to_string(stamp)+"-"+std::to_string(pid)+".dmp");
        partial=dump;partial+=".partial";
        const auto file=CreateFileW(partial.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) return {{},"cannot_create_dump"};
        NativeCrash native{};
        if(state.context_valid) std::memcpy(&native,state.native,sizeof(native));
        native.record.ExceptionRecord=nullptr;
        EXCEPTION_POINTERS pointers{&native.record,&native.context};
        MINIDUMP_EXCEPTION_INFORMATION info{state.thread_id,&pointers,FALSE};
        const auto type=MINIDUMP_TYPE(MiniDumpWithDataSegs|MiniDumpWithHandleData|MiniDumpWithThreadInfo|
            MiniDumpWithUnloadedModules|MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithFullMemoryInfo|MiniDumpFilterModulePaths);
        const bool written=MiniDumpWriteDump(process,pid,file,type,state.context_valid?&info:nullptr,nullptr,nullptr)!=FALSE;
        const auto error=written?ERROR_SUCCESS:GetLastError();
        CloseHandle(file);
        if(!written) { std::error_code ec;fs::remove(partial,ec);return {{},"capture_failed_"+std::to_string(error)}; }
        fs::rename(partial,dump);
        return {dump,"captured"};
    } catch(...) { std::error_code ec;if(!partial.empty()) fs::remove(partial,ec);return {{},"capture_failed"}; }
}
uint64_t Started(HANDLE process) {
    FILETIME created{},exit{},kernel{},user{};
    if(!GetProcessTimes(process,&created,&exit,&kernel,&user)) return 0;
    return (uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
bool Interactive() {
    wchar_t current[256]{},input[256]{};DWORD needed=0;
    const auto desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    const bool same=desktop && GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,current,sizeof(current),&needed) &&
        GetUserObjectInformationW(desktop,UOI_NAME,input,sizeof(input),&needed) && std::wstring(current)==input;
    if(desktop) CloseDesktop(desktop); return same;
}
#else
uint64_t Started(uint32_t pid, bool* zombie = nullptr) {
    std::ifstream file("/proc/"+std::to_string(pid)+"/stat");std::string text;std::getline(file,text);
    const auto end=text.rfind(')');if(end==std::string::npos) return 0;
    std::istringstream fields(text.substr(end+1));std::string field;
    for(int i=0;i<=19;++i) { if(!(fields>>field)) return 0; if(i==0 && zombie) *zombie=field=="Z"; }
    try { return std::stoull(field); } catch(...) { return 0; }
}
#endif
}
void StopMonitor() noexcept {
    if(monitor) { uint32_t running=0; std::atomic_ref(monitor->status).compare_exchange_strong(running,1,std::memory_order_release); }
}
bool NotifyCrash(uint32_t code,uint64_t address,void* native_exception) noexcept {
    if(!monitor || Load(monitor->status)==1) return false;
    monitor->code=code;monitor->address=address;
#ifdef _WIN32
    monitor->thread_id=GetCurrentThreadId();monitor->context_valid=0;
    const auto* exception=static_cast<const EXCEPTION_POINTERS*>(native_exception);
    if(exception && exception->ExceptionRecord && exception->ContextRecord) {
        std::memcpy(monitor->native+offsetof(NativeCrash,record),exception->ExceptionRecord,sizeof(EXCEPTION_RECORD));
        std::memcpy(monitor->native+offsetof(NativeCrash,context),exception->ContextRecord,sizeof(CONTEXT));
        monitor->context_valid=1;
    }
#endif
    Store(monitor->status,2);
#ifdef _WIN32
    // Keep the faulting process alive until the worker copied it (bounded by a deadline).
    const auto deadline=GetTickCount64()+10000;
    while(!Load(monitor->dump_ready) && GetTickCount64()<deadline) Sleep(10);
#endif
    return true;
}
bool StartMonitor(const Source& source) {
    if(monitor) return true;
    fs::path task,state;
    try {
#ifdef _WIN32
        const auto pid=GetCurrentProcessId();const auto started=Started(GetCurrentProcess());
        std::wstring executable(32768,L'\0');auto count=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!count || count==executable.size()) return false;executable.resize(count);
#else
        const auto pid=getpid();const auto started=Started(pid);
        const auto executable=fs::read_symlink("/proc/self/exe").string();
#endif
        const auto folder=source.data/"cache"/"support";fs::create_directories(folder);
        task=folder/("worker-"+std::to_string(pid)+"-"+std::to_string(started)+".toml");state=task;state.replace_extension("state");
        { std::ofstream out(state,std::ios::binary);State empty{};out.write(reinterpret_cast<const char*>(&empty),sizeof(empty));if(!out) return false; }
        auto mapping=std::make_unique<Mapping>();if(!mapping->Open(state)) throw std::runtime_error("Cannot map report worker state");
#ifdef _WIN32
        mapping->state->image_base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
#endif
        toml::table values{{"data",Utf8(source.data)},{"session_log",Utf8(source.session_log)},{"version",source.version},
            {"kind","crash"},{"parent_pid",int64_t(pid)},{"parent_started",int64_t(started)}, {"state",Utf8(state)},{"show_dialog",true}};
        { std::ofstream out(task);out<<values;out.close();if(!out) throw std::runtime_error("Cannot write report worker request"); }
#ifdef _WIN32
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        wchar_t desktop[256]{};DWORD needed=0;
        if(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&needed)) startup.lpDesktop=desktop;
        std::wstring command=L"\""+executable+L"\" --support-worker \""+task.wstring()+L"\"";
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))
            throw std::runtime_error("Cannot start report worker");
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
#else
        const auto child=fork();
        if(child<0) throw std::runtime_error("Cannot start report worker");
        if(child==0) { setsid();execl(executable.c_str(),executable.c_str(),"--support-worker",task.c_str(),nullptr);_exit(127); }
#endif
        for(int i=0;i<300 && !Load(mapping->state->ready);++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(!Load(mapping->state->ready)) { Store(mapping->state->status,1);return false; }
        monitor=mapping->state;
        // Retain the mapping until process teardown, including late exception handlers.
        mapping.release();std::atexit(StopMonitor);return true;
    } catch(...) {
        std::error_code ec;if(!task.empty()) fs::remove(task,ec);if(!state.empty()) fs::remove(state,ec);return false;
    }
}
int RunWorker(const fs::path& task) noexcept {
    fs::path state;
    try {
        const auto values=toml::parse_file(task.string());
        Source source{Path(values["data"].value_or(std::string{})),Path(values["session_log"].value_or(std::string{})),
            values["version"].value_or(std::string{}),"crash",{}};
        const auto pid=values["parent_pid"].value_or<int64_t>(0);const auto started=values["parent_started"].value_or<int64_t>(0);
        state=Path(values["state"].value_or(std::string{}));const bool show=values["show_dialog"].value_or(true);
        if(pid<=0 || started<=0 || source.data.empty() || state.empty()) return 10;
        bool crashed=false;uint32_t code=0;uint64_t address=0,image_base=0;
#ifdef _WIN32
        DWORD exit_code=STILL_ACTIVE;
        CapturedDump captured;
#endif
        {
            Mapping mapping;if(!mapping.Open(state)) return 10;
#ifdef _WIN32
            auto process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_DUP_HANDLE,FALSE,DWORD(pid));
            if(!process) process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
            if(process && Started(process)!=uint64_t(started)) { CloseHandle(process);return 10; }
#else
            const auto actual=Started(pid);if(actual && actual!=uint64_t(started)) return 10;
#endif
#ifdef _WIN32
            mapping.state->worker_pid=GetCurrentProcessId();
#else
            mapping.state->worker_pid=getpid();
#endif
            Store(mapping.state->ready,1);
            for(;;) {
                const auto status=Load(mapping.state->status);
                if(status==1) break;
#ifdef _WIN32
                const bool alive=process && WaitForSingleObject(process,100)==WAIT_TIMEOUT;
#else
                bool zombie=false;const bool alive=Started(pid,&zombie)==uint64_t(started) && !zombie;
                if(alive) std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
                if(!alive) { crashed=Load(mapping.state->status)!=1;break; }
                if(status==2) {
#ifdef _WIN32
                    captured=CaptureDump(process,DWORD(pid),*mapping.state,source.data);
                    Store(mapping.state->dump_ready,1);
                    WaitForSingleObject(process,2000);
#else
                    for(int i=0;i<20 && Started(pid);++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
                    crashed=true;break;
                }
            }
            code=mapping.state->code;address=mapping.state->address;image_base=mapping.state->image_base;
#ifdef _WIN32
            if(process) { GetExitCodeProcess(process,&exit_code);CloseHandle(process); }
#endif
        }
        std::error_code ec;fs::remove(task,ec);fs::remove(state,ec);
        if(!crashed) return 0;
        char reason[240]{};
#ifdef _WIN32
        if(code) std::snprintf(reason,sizeof(reason),"OpenShao stopped unexpectedly. Native exception %08X at %016llX.",code,static_cast<unsigned long long>(address));
#else
        if(code) std::snprintf(reason,sizeof(reason),"OpenShao stopped unexpectedly (signal %u).",code);
#endif
        else std::snprintf(reason,sizeof(reason),"OpenShao stopped unexpectedly before a normal shutdown.");
        source.reason=reason;
#ifdef _WIN32
        if(exit_code!=STILL_ACTIVE) { char status[64]{};std::snprintf(status,sizeof(status)," Process exit code: %08lX.",exit_code);source.reason+=status; }
        source.dump=captured.path;
        source.dump_status=captured.status.empty()?"process_already_exited":captured.status;
#else
        source.dump_status="native_dump_not_available";
#endif
        if(image_base) { char image[96]{};std::snprintf(image,sizeof(image)," Executable image base: %016llX.",static_cast<unsigned long long>(image_base));source.reason+=image; }
        const auto report=Generate(source);
        if(show) ShowReport(report,source.reason);
        return report.zip.empty()?10:0;
    } catch(...) { return 10; }
}
void OpenReport(const fs::path& zip) {
#ifdef _WIN32
    if(!Interactive()) return;
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    PIDLIST_ABSOLUTE item=nullptr;
    if(SUCCEEDED(SHParseDisplayName(zip.c_str(),nullptr,&item,0,nullptr))) {
        const auto result=SHOpenFolderAndSelectItems(item,0,nullptr,0);CoTaskMemFree(item);
        if(SUCCEEDED(result)) { if(SUCCEEDED(initialized)) CoUninitialize(); return; }
    }
    ShellExecuteW(nullptr,L"open",zip.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    if(SUCCEEDED(initialized)) CoUninitialize();
#else
    const auto child=fork();
    if(child==0) { if(fork()!=0) _exit(0);setsid();execlp("xdg-open","xdg-open",zip.parent_path().c_str(),nullptr);_exit(127); }
    if(child>0) waitpid(child,nullptr,0);
#endif
}
void ShowReport(const Report& report,const std::string& reason) {
    std::string message=privacy::Redact(reason);
    if(!report.zip.empty()) message+="\n\nDiagnostic ZIP:\n"+Utf8(report.zip)+
        "\n\nClick OK to open the folder with this ZIP selected.";
    else message+="\n\nThe diagnostic ZIP could not be created: "+report.error;
    if(report.dump_included) message+="\nThe crash dump is included in the ZIP.";
    message+="\n\nTo report this problem, open OpenShao > Support > Report a bug.\nAttach the diagnostic ZIP to your GitHub issue.";
#ifdef _WIN32
    const auto wide=Path(message).wstring();
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(Interactive() && foreground==GetCurrentProcessId()) ClipCursor(nullptr);
    MessageBoxW(nullptr,wide.c_str(),L"OpenShao",MB_OK|MB_ICONERROR);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"OpenShao",message.c_str(),nullptr);
#endif
    if(!report.zip.empty()) OpenReport(report.zip);
}
}
