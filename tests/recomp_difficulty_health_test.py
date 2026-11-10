"""Run the translated kill-recovery and health-orb paths through the Monk hooks."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
NAMES = {'0001371A', '00014D81', '000148A4', '00012405', '00013E9A', '00153CD0'}


def native_functions(folder):
    found = {}
    for path in folder.glob('x86_recomp.*.cpp'):
        text = path.read_text(encoding='utf-8')
        for name in NAMES - found.keys():
            start = text.find(f'X86_FUNC(__imp__sub_{name}) {{')
            if start >= 0:
                found[name] = text[start:text.index('\n}', start) + 2]
        if found.keys() == NAMES:
            return found
    raise RuntimeError('Missing translated health functions: ' + ', '.join(sorted(NAMES - found.keys())))


HARNESS = r'''
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "difficulty.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace openshao::difficulty;
alignas(64) std::array<uint8_t,8<<20> guest{};
namespace kernel { uint8_t* g_base = guest.data(); }
constexpr uint32_t HeroObjects=0x5CE880, HeroStride=0x3238;
constexpr uint32_t First=0x710000, Second=0x711000, State=0x700000;
Mode mode=Mode::Monk;
bool has_totem=false;
std::array<int32_t,2> totem_health{-1,-1};
unsigned game_mode=0, failures=0;
struct Restore32 {uint32_t address,value;~Restore32(){kernel::W32(address,value);}};
void Check(bool ok,const char* why) {if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",why);}}
X86_FUNC(sub_000138F0) {ctx.eax=game_mode;ctx.esp+=4;}
X86_FUNC(sub_00015358) {ctx.eax=State;ctx.esp+=4;}
X86_FUNC(sub_0016B210) {ctx.eax=First;ctx.esp+=4;}
X86_FUNC(sub_0016B220) {ctx.eax=Second;ctx.esp+=4;}
X86_FUNC(sub_0016B2B0) {ctx.eax=X86_MEM32(X86_MEM32(ctx.esp+4)+0x200);ctx.esp+=8;}
X86_FUNC(sub_00016942) {ctx.eax=game_mode==1;ctx.esp+=4;}
X86_FUNC(sub_00016C4E) {ctx.eax=X86_MEM32(State+0x90)*X86_MEM32(State+0x94);ctx.esp+=4;}
X86_FUNC(sub_000EB38C) {ctx.eax=uint32_t(int32_t(X87_ST(0)));x87_pop(ctx);ctx.esp+=4;}
X86_FUNC(sub_00153AA0) {ctx.eax=0;ctx.esp+=4;}
X86_FUNC(sub_00153C30) {ctx.eax=0;ctx.esp+=4;}
X86_FUNC(sub_001D02B0) {ctx.esp+=8;}
X86_FUNC(sub_00013F4E) {ctx.esp+=24;}
void x86_icall(X86Context& ctx,uint8_t*,uint32_t) {x87_push(ctx,10.0);ctx.esp+=4;}

void Init(Mode difficulty,int health,unsigned players=0) {
    guest.fill(0);mode=difficulty;game_mode=players;has_totem=false;
    auto* base=guest.data();
    X86_MEM32(HeroObjects+HeroStride+0x200)=First;
    X86_MEM32(HeroObjects+2*HeroStride+0x200)=Second;
    X86_MEM32(First+0x368)=health;X86_MEM32(Second+0x368)=health;
    X86_MEM32(First+0x370)=500;X86_MEM32(Second+0x370)=500;
    X86_MEM32(State+0x90)=1;X86_MEM32(State+0x94)=players==1?1000:500;
    X86_MEMF32(0x496B98)=.5f;X86_MEMF32(0x49D538)=.01f;
    X86_MEM32(0x720000)=0x720100;
    X86_MEM32(0x72011C)=0x123456;
}
X86Context Context(uint32_t caller=0x12345678) {
    X86Context ctx{};ctx.esp=0x8000;ctx.ecx=0x720000;
    kernel::W32(ctx.esp,caller);kernel::W32(ctx.esp+4,HeroObjects+HeroStride);
    ctx.esi=0x11223344;ctx.edi=0x55667788;ctx.ebx=0x44332211;
    return ctx;
}
'''

MAIN = r'''
int main() {
    auto* base=guest.data();
    Init(Mode::Hard,100);
    auto reference_orb=Context();sub_00012405(reference_orb,base);
    const int native_orb_gain=int(kernel::R32(First+0x368))-100;
    Check(native_orb_gain>0,"the native orb fixture grants health before applying Monk rules");
    for(Mode selected:{Mode::Easy,Mode::Normal,Mode::Hard,Mode::Monk}) {
        const int gain=selected==Mode::Monk?10:50;
        const int orb_gain=selected==Mode::Monk?std::max(1,int(native_orb_gain*.2f)):native_orb_gain;
        for(int start:{100,449,450,451,489,490,499}) {
            Init(selected,start);
            auto ctx=Context();sub_0001371A(ctx,base);
            const int expected=std::min(start+gain,500);
            if(int(kernel::R32(First+0x368))!=expected)
                std::fprintf(stderr,"kill mode %u: %d -> %u, expected %d\n",unsigned(selected),start,kernel::R32(First+0x368),expected);
            Check(int(kernel::R32(First+0x368))==expected,"native kill recovery scales its original gain before the maximum clamp");
            Check(X86_MEMF32(State+0x8C)==float(expected),"native stored health agrees with the hero after a kill reward");
            Check(ctx.esp==0x8004 && ctx.esi==0x11223344 && ctx.edi==0x55667788 && ctx.ebx==0x44332211,
                  "kill recovery preserves its native stack and callee-saved registers");
        }
        Init(selected,450);
        for(int kill=0;kill<6;++kill){auto ctx=Context();sub_0001371A(ctx,base);}
        Check(kernel::R32(First+0x368)==500,"repeated native kills fill the final health segment");
        Init(selected,100);
        auto kill=Context();sub_0001371A(kill,base);
        auto totem=Context();totem.ecx=First;kernel::W32(totem.esp+4,500);
        sub_00153CD0(totem,base);
        Check(kernel::R32(First+0x368)==unsigned(selected==Mode::Monk?110:500),
              "the kill-recovery bypass is released before the next totem restore");
        Init(selected,400);
        auto damage=Context();kernel::W32(damage.esp+4,uint32_t(-50));sub_00014D81(damage,base);
        Check(kernel::R32(First+0x368)==350,"negative scripted health adjustments retain their native value");
        Init(selected,499);
        auto orb=Context();sub_00012405(orb,base);
        Check(kernel::R32(First+0x368)==500 && orb.esp==0x8008,
              "the native health orb fills the final point and keeps its ABI");
        Init(selected,100);
        auto partial=Context();sub_00012405(partial,base);
        Check(kernel::R32(First+0x368)==unsigned(100+orb_gain),"native health orbs retain difficulty-specific recovery");
        // The add-health route must release its temporary bypass before a full restore.
        auto restored=Context();restored.ecx=First;kernel::W32(restored.esp+4,500);
        sub_00153CD0(restored,base);
        Check(kernel::R32(First+0x368)==unsigned(selected==Mode::Monk?100+orb_gain:500),
              "Monk totem restores remain blocked after a kill or orb");
        Init(selected,495,1);
        auto coop=Context();sub_0001371A(coop,base);
        Check(kernel::R32(First+0x368)==500 && kernel::R32(Second+0x368)==500,
              "native shared-health recovery can fill both heroes in Ko-op");
    }
    std::printf("Native health paths: %u failures\n",failures);
    return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--generated', required=True, type=Path)
    args = parser.parse_args()
    functions = native_functions(args.generated)
    source = (ROOT/'src/difficulty_hooks.inl').read_text(encoding='utf-8')
    start = source.index('// Health setter.')
    hooks = source[start:source.index('// Experience setter ', start)]
    supplied = set(re.findall(r'X86_FUNC\((sub_[A-F0-9]+)\)', hooks))
    declarations = '\n'.join(f'X86_EXTERN_FUNC(sub_{name});' for name in NAMES)
    native = '\n'.join(functions.values())
    for name in NAMES:
        if 'sub_'+name not in supplied:
            native += f'\nX86_FUNC(sub_{name}) {{__imp__sub_{name}(ctx,base);}}\n'
    env = os.environ.copy()
    if os.name == 'nt':
        result = subprocess.run(['cmd.exe','/d','/c','call',str(ROOT/'build/vs_env.cmd')],
                                capture_output=True,text=True,check=True)
        for line in result.stdout.splitlines():
            if '=' in line and not line.startswith('='):
                key,value=line.split('=',1);env[key]=value
    with tempfile.TemporaryDirectory(prefix='openshao-health-paths-') as temporary:
        folder=Path(temporary)
        cpp=folder/'health.cpp';exe=folder/('health.exe' if os.name=='nt' else 'health')
        cpp.write_text('\n'.join((HARNESS,declarations,native,hooks,MAIN)),encoding='utf-8')
        command=[str(args.compiler),'-std=c++20','-O1','-fno-strict-aliasing','-fuse-ld=lld',
                 '-I'+str(ROOT/'src/sdk_compat'),'-I'+str(ROOT/'src'),'-I'+str(args.sdk/'runtime'),
                 str(cpp),'-o',str(exe)]
        result=subprocess.run(command,env=env,capture_output=True,text=True)
        if not result.returncode:
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20,
                                  creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        print(result.stdout,end='');print(result.stderr,end='')
        raise SystemExit(result.returncode)


if __name__=='__main__':
    main()
