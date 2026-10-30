"""Exercise the generated native crash handler before any guest thread exists."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True, type=Path)
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--main', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = args.main.read_text(encoding='utf-8')
crash = source[source.index('LONG WINAPI WriteCrashDump('):source.index('\nvoid DumpThreads()')]
paths = (root/'src/runtime_paths.cpp').read_text(encoding='utf-8')
report = paths[paths.index('void ReportError('):paths.rfind('\n}')]
harness = r'''
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "core/log.h"
#include "support_report.h"
namespace fs = std::filesystem;
namespace kernel {
struct Thread { unsigned id; int* ctx; };
inline unsigned char* g_base = nullptr;
Thread* Current() { return nullptr; }
void LogGuestStack(int, int) {}
}
namespace openshao { struct { bool background = true; } options; }
namespace openshao::support {
bool exception_forwarded=false;
bool NotifyCrash(uint32_t code,uint64_t address,void* pointer) noexcept {
    auto* e=static_cast<EXCEPTION_POINTERS*>(pointer);
    exception_forwarded=e && e->ContextRecord && e->ExceptionRecord && code==EXCEPTION_ACCESS_VIOLATION && address==0x1234000;
    return false;
}
void StopMonitor() noexcept {}
Report Generate(const Source&) noexcept { return {}; }
void ShowReport(const Report&,const std::string&) {}
}
namespace openshao::paths {
fs::path root;
const fs::path& Root() { return root; }
fs::path Crashes() { return root; }
std::string Utf8(const fs::path& value) { auto bytes=value.u8string(); return {bytes.begin(),bytes.end()}; }
std::wstring ErrorText(const std::wstring& reason, const fs::path&) { return reason; }
void ReportError(const std::wstring&, const fs::path& = {});
REPORT
}
namespace app {
struct { const wchar_t* name = L"OpenShao"; } kGame;
CRASH
}
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    openshao::paths::root=argv[1];
    fs::create_directories(openshao::paths::root);
    logx::Init(openshao::paths::root.string(), "test");
    CONTEXT context{}; RtlCaptureContext(&context);
    EXCEPTION_RECORD record{};
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    record.ExceptionAddress=reinterpret_cast<void*>(0x1234000);
    record.NumberParameters=2;
    record.ExceptionInformation[0]=0;
    record.ExceptionInformation[1]=0x5678000;
    EXCEPTION_POINTERS exception{&record,&context};
    app::WriteCrashDump(&exception);
    openshao::paths::ReportError(L"startup-regression-reason");
    logx::Flush();
    std::ifstream input(logx::Path());
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    bool ok=true;
    if(!openshao::support::exception_forwarded) {
        std::cerr << "FAIL: the native handler must forward the original exception context to the report worker\n";ok=false;
    }
    if (text.find("C0000005")==std::string::npos) {
        std::cerr << "FAIL: a native startup exception must log its code without a guest thread\n"; ok=false;
    }
    if (text.find("startup-regression-reason")==std::string::npos) {
        std::cerr << "FAIL: suppressed startup dialogs must still record their reason\n"; ok=false;
    }
    bool dump=false;
    for (const auto& entry:fs::directory_iterator(openshao::paths::root))
        if(entry.path().extension()==".dmp" && entry.file_size()>0)dump=true;
    if(!dump) { std::cerr << "FAIL: native startup failures must still produce a dump\n"; ok=false; }
    return ok ? 0 : 1;
}
'''.replace('REPORT', report).replace('CRASH', crash)
with tempfile.TemporaryDirectory(prefix='openshao-startup-diagnostic-') as folder:
    work=Path(folder)
    cpp=work/'test.cpp';cpp.write_text(harness,encoding='utf-8')
    exe=work/'test.exe'
    subprocess.run([str(args.compiler),'-std=c++20','-DNDEBUG','-D_CRT_SECURE_NO_WARNINGS','-DOPENSHAO_RECOMP_VERSION="test"',
                    '-fuse-ld=lld','-I'+str(root/'src'),'-I'+str(args.sdk/'runtime'),str(cpp),
                    str(args.sdk/'runtime/core/log.cpp'),'-ldbghelp','-lshell32','-luser32',
                    '-o',str(exe)],check=True,capture_output=True,text=True)
    run=subprocess.run([str(exe),str(work/'data')],capture_output=True,text=True,timeout=20,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    print(run.stdout,end='');print(run.stderr,end='')
    if run.returncode:raise SystemExit(run.returncode)
print('PASS: pre-game exceptions and suppressed error reasons remain diagnosable')
