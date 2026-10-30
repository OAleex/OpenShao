#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include "support_report.h"
#include "core/log.h"

LONG WINAPI Crash(EXCEPTION_POINTERS* e) {
    return openshao::support::NotifyCrash(e->ExceptionRecord->ExceptionCode,
        reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress),e) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
int wmain(int argc,wchar_t** argv) {
    if(argc==3 && std::wstring(argv[1])==L"--support-worker") return openshao::support::RunWorker(argv[2]);
    if(argc!=3) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(Crash);
    const std::filesystem::path data=argv[1];
    logx::Init((data/"logs").string(),"openshao");
    LOGR("OpenShao version: test; GPU: synthetic renderer; renderer=d3d12");
    logx::Flush();
    if(!openshao::support::StartMonitor({data,logx::Path(),"test"})) return 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    const std::wstring mode=argv[2];
    if(mode==L"clean") return 0;
    if(mode==L"abrupt") std::_Exit(42);
    if(mode==L"external") { Sleep(INFINITE); return 12; }
    if(mode==L"failfast") RaiseFailFastException(nullptr,nullptr,0);
    RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    return 11;
}
