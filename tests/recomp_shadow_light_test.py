"""Exercise the translated shadow-light update, its first sample and later smoothing."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', required=True, type=Path)
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--generated', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
names = {'0012D3B0', '0014B0D0', '0012CBF0', '0011E130', '0010C9A0'}
functions = {}
for path in args.generated.glob('x86_recomp.*.cpp'):
    text = path.read_text(encoding='utf-8')
    for name in names - functions.keys():
        start = text.find(f'X86_FUNC(__imp__sub_{name}) {{')
        if start >= 0:
            end = text.index('\n}', start) + 2
            functions[name] = text[start:end]
    if functions.keys() == names:
        break
if functions.keys() != names:
    raise RuntimeError('Missing translated shadow routines: ' + ', '.join(names - functions.keys()))
calls = set(re.findall(r'\bsub_([0-9A-F]+)\(ctx', '\n'.join(functions.values())))
native = '#include "cpu/x86_ops.h"\n'
native += ''.join(f'X86_EXTERN_FUNC(sub_{name});\n' for name in sorted(names | calls))
native += '\n'.join(functions[name] + f'\nX86_WEAK_FUNC(sub_{name}, __imp__sub_{name})\n'
                    for name in sorted(names))
harness = r'''
#include "kernel/kernel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace kernel { uint8_t* g_base = nullptr; }
namespace {
std::vector<uint8_t> memory(0x700000);
constexpr uint32_t Model=0x10000, Light=0x20000, Second=0x21000, Stack=0x60000;
unsigned failures=0;
void Check(bool passed,const char* message) {
    if (!passed) { ++failures;std::fprintf(stderr,"FAIL: %s\n",message); }
}
void Reset() {
    std::fill(memory.begin(),memory.end(),0);
    kernel::g_base=memory.data();
    kernel::G<float>(0x496454)=1;
    kernel::G<float>(0x496B98)=0.5f;
    kernel::G<float>(0x4ABBE0)=1.6f;
    kernel::G<float>(0x49EED8)=10000;
    kernel::G<float>(0x632B68)=2000;
    kernel::G<float>(0x632B6C)=348;
    kernel::G<float>(0x632B70)=15;
    kernel::G<float>(Model+0x3c)=-2777;
    kernel::G<float>(Model+0x40)=-1771;
    kernel::G<float>(Model+0x44)=940;
    kernel::G<float>(Model+0x1ac)=10000;
    kernel::W32(Model+0xf4,Light);
    kernel::G<float>(Light+0x5c)=-1800;
    kernel::G<float>(Light+0x60)=-3423;
    kernel::G<float>(Light+0x64)=442;
    kernel::G<float>(Light+0x9c)=0.75f;
    kernel::G<float>(Light+0xa0)=3844;
}
X86Context Context() {
    X86Context ctx{};
    ctx.esp=Stack;ctx.ecx=Model;
    ctx.ebx=0x12345678;ctx.esi=0x34567890;ctx.edi=0x56789012;ctx.ebp=0x78901234;
    kernel::W32(Stack,0x11223344);
    return ctx;
}
void CheckReturn(const X86Context& ctx) {
    Check(ctx.esp==Stack+4 && ctx.ebx==0x12345678 && ctx.esi==0x34567890 &&
          ctx.edi==0x56789012 && ctx.ebp==0x78901234,
          "shadow update preserves stack cleanup and callee-saved registers");
}
}
X86_FUNC(sub_0014E700) {
    std::fprintf(stderr,"FAIL: translated shadow routine reached a native assertion\n");
    std::exit(2);
}
void x86_icall(X86Context& ctx,uint8_t*,uint32_t target) {
    Check(target==0x123456,"fallback queries the native world-level interface");
    ctx.eax=0x32000;
    ctx.esp+=4;
}
X86_EXTERN_FUNC(sub_0012D3B0);
X86_EXTERN_FUNC(__imp__sub_0012D3B0);
void Update() {
    auto ctx=Context();
    sub_0012D3B0(ctx,kernel::g_base);
    CheckReturn(ctx);
}
void FirstLight(bool paired,bool relative_height) {
    Reset();
    if (relative_height) kernel::G<uint8_t>(Light)=4;
    if (paired) {
        kernel::W32(Model+0xfc,Second);
        kernel::G<float>(Model+0xf8)=30;
        kernel::G<float>(Model+0x100)=20;
        kernel::G<float>(Second+0x5c)=300;
        kernel::G<float>(Second+0x60)=-2400;
        kernel::G<float>(Second+0x64)=900;
        kernel::G<float>(Second+0x9c)=0.5;
        kernel::G<float>(Second+0xa0)=5000;
    }
    Update();
    Check(std::fabs(kernel::G<float>(Model+0x168)+1800)<0.01f &&
          std::fabs(kernel::G<float>(Model+0x170)-442)<0.01f,
          "first shadow uses the selected native light without easing from the world origin");
    Check(std::fabs(kernel::G<float>(Model+0x16c)+3771)<0.01f,
          "first shadow retains native relative-height and height-offset rules");
    Check(std::fabs(kernel::G<float>(Model+0x1ac)-3844)<0.02f,
          "first shadow starts with its light's projection radius");
    Check(std::fabs(kernel::G<float>(Model+0x1a8)-0.05f)<0.00001f,
          "first shadow preserves the native opacity fade");
}
void ExistingLight() {
    Reset();
    kernel::G<float>(Model+0x168)=-1500;
    kernel::G<float>(Model+0x16c)=-3600;
    kernel::G<float>(Model+0x170)=600;
    kernel::G<float>(Model+0x1ac)=4000;
    const auto initial=memory;
    auto native_ctx=Context();
    __imp__sub_0012D3B0(native_ctx,kernel::g_base);
    const auto expected=memory;
    memory=initial;kernel::g_base=memory.data();
    auto ctx=Context();
    sub_0012D3B0(ctx,kernel::g_base);
    CheckReturn(ctx);
    Check(memory==expected,"existing lights retain the complete translated update and smoothing");
    Check(ctx.eax==native_ctx.eax && ctx.edx==native_ctx.edx && ctx.ecx==native_ctx.ecx,
          "the override preserves native return and caller-saved register results");
}
void FrozenLight() {
    Reset();kernel::W32(0x6C92EC,1);Update();
    Check(kernel::G<float>(Model+0x168)==0 && kernel::G<float>(Model+0x16c)==0 &&
          kernel::G<float>(Model+0x170)==0 && kernel::G<float>(Model+0x1ac)==10000,
          "frozen updates leave the initial shadow state untouched");
    kernel::W32(0x6C92EC,0);Update();
    Check(std::fabs(kernel::G<float>(Model+0x168)+1800)<0.01f &&
          std::fabs(kernel::G<float>(Model+0x16c)+3771)<0.01f,
          "the first active update initializes a previously frozen shadow");
}
void NativeFirstReference() {
    const auto initial=memory;
    kernel::G<float>(0x632B70)=1;
    auto ctx=Context();
    __imp__sub_0012D3B0(ctx,kernel::g_base);
    const std::array<float,4> expected={kernel::G<float>(Model+0x168),
        kernel::G<float>(Model+0x16c),kernel::G<float>(Model+0x170),kernel::G<float>(Model+0x1ac)};
    memory=initial;kernel::g_base=memory.data();Update();
    for (unsigned i=0;i<3;++i)
        Check(std::fabs(kernel::G<float>(Model+0x168+i*4)-expected[i])<0.01f,
              "first sample matches the native target for fallback and distant lights");
    Check(std::fabs(kernel::G<float>(Model+0x1ac)-expected[3])<0.02f,
          "first radius matches the native target for fallback and distant lights");
}
void FallbackAndDistantLights() {
    Reset();kernel::W32(Model+0xf4,0);
    kernel::W32(0x639224,0x30000);kernel::W32(0x30000,0x31000);
    kernel::W32(0x31010,0x123456);kernel::W32(0x32900,0x33000);
    kernel::G<float>(0x33070)=-1;
    kernel::G<float>(0x632B74)=10000;kernel::G<float>(0x632B78)=2000;
    NativeFirstReference();
    Reset();kernel::G<float>(Light+0x5c)=-18000;
    NativeFirstReference();
}
void InitializedOrigin() {
    Reset();kernel::G<float>(0x632B6C)=0;
    kernel::G<float>(Model+0x40)=2000;
    kernel::G<float>(Light+0x5c)=0;kernel::G<float>(Light+0x60)=0;
    kernel::G<float>(Light+0x64)=0;kernel::G<float>(Light+0xa0)=10000;
    Update();
    kernel::G<float>(Light+0x5c)=150;
    const auto initial=memory;
    auto ctx=Context();__imp__sub_0012D3B0(ctx,kernel::g_base);
    const auto expected=memory;
    memory=initial;kernel::g_base=memory.data();Update();
    Check(memory==expected,"a valid initialized light at the world origin retains later smoothing");
}
int main() {
    FirstLight(false,false);
    FirstLight(false,true);
    FirstLight(true,false);
    ExistingLight();FrozenLight();
    FallbackAndDistantLights();InitializedOrigin();
    if (failures) return 1;
    std::puts("PASS: native first shadow light, paired/fallback/distant lights, height, fade, freeze, origin, smoothing and ABI");
}
'''
with tempfile.TemporaryDirectory(prefix='openshao-shadow-light-') as folder:
    work = Path(folder)
    original = work/'original.cpp'
    original.write_text(native, encoding='utf-8')
    test = work/'test.cpp'
    test.write_text(harness, encoding='utf-8')
    exe = work/('test.exe' if os.name == 'nt' else 'test')
    command = [str(args.compiler), '-std=c++20', '-O1', '-fno-strict-aliasing', '-fuse-ld=lld',
               '-I'+str(root/'src/sdk_compat'), '-I'+str(args.sdk/'runtime'), str(original), str(test)]
    hook = root/'src/shadow_light.cpp'
    if hook.exists():
        command.append(str(hook))
    command += ['-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    if build.returncode:
        print(build.stdout, end='');print(build.stderr, end='')
        raise SystemExit(build.returncode)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20,
                         creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    print(run.stdout, end='');print(run.stderr, end='')
    raise SystemExit(run.returncode)
