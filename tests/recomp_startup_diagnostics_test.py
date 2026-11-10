"""Exercise fallback and monitored native/guest crash diagnostics."""
import argparse
from pathlib import Path
import struct
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
inline int context=0;
inline Thread thread{0x14,&context};
inline bool guest=false;
Thread* Current() { return guest ? &thread : nullptr; }
void LogGuestStack(int, int) { LOGR("guest-context-marker ECX=0186B800"); }
}
namespace openshao { struct { bool background = true; } options; }
namespace openshao::support {
bool exception_forwarded=false;
bool monitor_available=false, diagnostics_before_handoff=false;
bool NotifyCrash(uint32_t code,uint64_t address,void* pointer) noexcept {
    auto* e=static_cast<EXCEPTION_POINTERS*>(pointer);
    exception_forwarded=e && e->ContextRecord && e->ExceptionRecord && code==EXCEPTION_ACCESS_VIOLATION && address==0x1234000;
    std::ifstream input(logx::Path());
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    diagnostics_before_handoff=text.find("C0000005")!=std::string::npos &&
        (!kernel::guest || text.find("guest-context-marker ECX=0186B800")!=std::string::npos);
    return monitor_available;
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
    kernel::guest=true;
    openshao::support::monitor_available=true;
    const auto handled=app::WriteCrashDump(&exception);
    if(handled!=EXCEPTION_EXECUTE_HANDLER || !openshao::support::diagnostics_before_handoff) {
        std::cerr << "FAIL: monitored guest failures must flush their registers and stack before the report worker snapshots the log\n";
        ok=false;
    }
    return ok ? 0 : 1;
}
'''.replace('REPORT', report).replace('CRASH', crash)
with tempfile.TemporaryDirectory(prefix='openshao-startup-diagnostic-') as folder:
    work=Path(folder)
    cpp=work/'test.cpp';cpp.write_text(harness,encoding='utf-8')
    exe=work/'test.exe'
    subprocess.run([str(args.compiler),'-std=c++20','-DNDEBUG','-D_CRT_SECURE_NO_WARNINGS','-DOPENSHAO_RECOMP_VERSION="test"',
                    '-fuse-ld=lld','-I'+str(root/'src/sdk_compat'),'-I'+str(root/'src'),'-I'+str(args.sdk/'runtime'),str(cpp),
                    str(args.sdk/'runtime/core/log.cpp'),'-ldbghelp','-lshell32','-luser32',
                    '-o',str(exe)],check=True,capture_output=True,text=True)
    run=subprocess.run([str(exe),str(work/'data')],capture_output=True,text=True,timeout=20,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    print(run.stdout,end='');print(run.stderr,end='')
    if run.returncode:raise SystemExit(run.returncode)
    dump=next((work/'data').glob('*.dmp')).read_bytes()
    stream_count,table=struct.unpack_from('<II',dump,8)
    stream_types={struct.unpack_from('<I',dump,table+i*12)[0] for i in range(stream_count)}
    assert 16 in stream_types,'Fallback crash dumps must also preserve memory protection and allocation state'
print('PASS: fallback and monitored native/guest failures preserve crash diagnostics')
