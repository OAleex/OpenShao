"""Exercise native model construction and visibility with dirty guest stack locals."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def original_functions(folder):
    names = {'00144C20', '00144970', '0015DAE0'}
    found = {}
    for path in folder.glob('x86_recomp.*.cpp'):
        text = path.read_text(encoding='utf-8')
        for name in names - found.keys():
            start = text.find(f'X86_FUNC(__imp__sub_{name}) {{')
            if start >= 0:
                end = text.index('\n}', start) + 2
                found[name] = text[start:end]
        if names == found.keys():
            break
    if names != found.keys():
        raise RuntimeError('Generated model functions are missing: ' + ', '.join(sorted(names - found.keys())))
    return found


parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True, type=Path)
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--generated', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
functions = original_functions(args.generated)
native = '#include "cpu/x86_ops.h"\nX86_EXTERN_FUNC(sub_00102BE0);\nX86_EXTERN_FUNC(sub_0015DAE0);\n'
for name in ('00144C20', '0015DAE0', '00144970'):
    native += functions[name] + f'\nX86_WEAK_FUNC(sub_{name}, __imp__sub_{name})\n'
harness = r'''
#include "cpu/x86_ops.h"
#include <array>
#include <cstdio>
#include <cstdlib>

X86_EXTERN_FUNC(sub_00144C20);
X86_EXTERN_FUNC(sub_00144970);
alignas(64) std::array<uint8_t,0x10000> guest;
unsigned expected_bytes=24;
void Check(bool condition,const char* reason) {
    if(!condition) { std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1); }
}
X86_FUNC(sub_00102BE0) {
    Check(X86_MEM32(ctx.esp+4)==expected_bytes && X86_MEM32(ctx.esp+8)==0x33,
          "model initialization preserves allocation size and flags");
    ctx.eax=0x2000;
    ctx.esp+=4;
}
X86Context CallContext(uint8_t* base,uint32_t object,uint32_t argument) {
    X86Context ctx{};
    ctx.esp=0x8000;ctx.ecx=object;
    ctx.ebx=0x12345678;ctx.esi=0x34567890;ctx.edi=0x56789012;ctx.ebp=0x78901234;
    X86_MEM32(0x8000)=0x11223344;X86_MEM32(0x8004)=argument;
    return ctx;
}
void CheckReturn(const X86Context& ctx) {
    Check(ctx.esp==0x8008 && ctx.ebx==0x12345678 && ctx.esi==0x34567890 &&
          ctx.edi==0x56789012 && ctx.ebp==0x78901234,
          "model routines preserve stack cleanup and callee-saved registers");
}
void UnboundInstances(uint32_t dirty_pointer) {
    guest.fill(0xA5);
    auto* base=guest.data();
    X86_MEM32(0x1100)=0x1000;X86_MEM32(0x1004)=3;
    auto ctx=CallContext(base,0x1100,0x33);
    X86_MEM32(0x7FF8)=0xA5B6C7FF;
    X86_MEM32(0x7FFC)=dirty_pointer;
    sub_00144C20(ctx,base);
    CheckReturn(ctx);
    Check(X86_MEM32(0x1114)==0x2000,"the native constructor retains its allocated instance table");
    for(unsigned slot=0;slot<3;++slot) {
        Check(X86_MEM32(0x2004+slot*8)==0,
              "new model slots must not inherit object pointers from dirty guest stack locals");
        Check(X86_MEM32(0x2000+slot*8)==0,
              "new model slots start with a fully initialized false binding flag");
    }
    for(uint32_t hidden:{0u,1u}) {
        ctx=CallContext(base,0x1100,hidden);
        sub_00144970(ctx,base);
        CheckReturn(ctx);
    }
    Check(X86_MEM8(0x70)==0xA5 && X86_MEM8(0x1FFF)==0xA5 && X86_MEM8(0x2018)==0xA5,
          "unbound visibility leaves the null object and neighboring memory untouched");

    // A bound model respects its own flag when the caller removes the override.
    X86_MEM32(0x2008)=1;X86_MEM32(0x200C)=0x3000;
    X86_MEM32(0x2010)=0;X86_MEM32(0x2014)=0x3200;
    X86_MEM8(0x3070)=0xCC;X86_MEM8(0x3270)=0xCC;
    ctx=CallContext(base,0x1100,0);
    sub_00144970(ctx,base);
    CheckReturn(ctx);
    Check(X86_MEM8(0x3070)==0 && X86_MEM8(0x3270)==1,
          "bound objects retain their individual visibility flags");
    ctx=CallContext(base,0x1100,1);
    sub_00144970(ctx,base);
    CheckReturn(ctx);
    Check(X86_MEM8(0x3070)==1 && X86_MEM8(0x3270)==1,
          "the visibility override still applies to every bound object");
}
int main() {
    UnboundInstances(0x0186B800);
    UnboundInstances(0xFFFFFFFF);
    auto* base=guest.data();
    X86_MEM32(0x1004)=0;expected_bytes=0;
    const auto before=guest;
    auto ctx=CallContext(base,0x1100,0x33);
    sub_00144C20(ctx,base);
    CheckReturn(ctx);
    for(unsigned offset=0;offset<24;++offset)
        Check(guest[0x2000+offset]==before[0x2000+offset],"empty model tables do not write instance records");
    std::puts("PASS: native model construction ignores dirty locals and preserves bound visibility and ABI");
}
'''
with tempfile.TemporaryDirectory(prefix='openshao-model-instances-') as folder:
    work = Path(folder)
    original = work/'original.cpp'
    original.write_text(native, encoding='utf-8')
    test = work/'test.cpp'
    test.write_text(harness, encoding='utf-8')
    exe = work/('test.exe' if os.name == 'nt' else 'test')
    command = [str(args.compiler), '-std=c++20', '-O1', '-fno-strict-aliasing', '-fuse-ld=lld',
               '-I'+str(root/'src/sdk_compat'), '-I'+str(args.sdk/'runtime'), str(original), str(test)]
    hook = root/'src/model_instances.cpp'
    if hook.exists():
        command.append(str(hook))
    command += ['-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    if build.returncode:
        print(build.stdout, end='')
        print(build.stderr, end='')
        raise SystemExit(build.returncode)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20,
                         creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    print(run.stdout, end='')
    print(run.stderr, end='')
    if run.returncode:
        raise SystemExit(run.returncode)
