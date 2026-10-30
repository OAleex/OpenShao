#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace kernel {uint8_t* g_base=nullptr;}
namespace {
std::vector<uint8_t> memory(0x700000);
constexpr uint32_t Input=0x67C658, Preferences=0x4000, Versus=0x5000;
bool natural_completion=false,native_pause_allowed=true;
unsigned failures=0;
void Check(bool ok,const char* message) {
    if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",message);}
}
void Setup(uint32_t mode=2,uint32_t state=1) {
    std::fill(memory.begin(),memory.end(),0);
    kernel::W32(0x5ACEBC,Preferences);kernel::W32(Preferences+0x88,mode);
    kernel::W32(0x5ACEB8,Versus);kernel::W32(Versus+0x14,state);
    kernel::W32(Input+0x1C,3);
    natural_completion=false;native_pause_allowed=true;
}
void Buttons(unsigned port,uint16_t held,uint16_t pressed,uint16_t repeated) {
    const auto pad=Input+0x24+port*0x98;
    kernel::W16(pad,held);kernel::W16(pad+2,pressed);kernel::W16(pad+6,repeated);
    uint16_t combined=0,repeats=0;
    for(unsigned i=0;i<4;++i) {
        combined|=kernel::R16(Input+0x24+i*0x98);
        repeats|=kernel::R16(Input+0x2A+i*0x98);
    }
    const auto previous=kernel::R16(Input+0x10);
    kernel::W16(Input+0x10,combined);
    kernel::W16(Input+0x12,uint16_t(combined&~previous));
    kernel::W16(Input+0x16,repeats);
}
}
// Retail boundaries: the intro returns true on either player's Start or its
// natural end; the pause predicate does not consume the native input snapshot.
X86_FUNC(__imp__sub_000140B6) {
    const bool done=natural_completion || (kernel::R16(Input+0x26)&0x800) ||
        (kernel::R16(Input+0xBE)&0x800);
    ctx.eax=0xAABBCC00u|unsigned(done);ctx.ecx=0x12345678;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00011104) {
    ctx.eax=0xAABBCC00u|unsigned(native_pause_allowed);ctx.ecx=0x87654321;ctx.esp+=4;
}
// Match the generated game's weak imports so the regression also exercises
// the unfixed retail boundaries before the host hooks are linked.
X86_WEAK_FUNC(sub_000140B6,__imp__sub_000140B6)
X86_WEAK_FUNC(sub_00011104,__imp__sub_00011104)
namespace {
bool CompleteIntro() {
    X86Context ctx{};ctx.esp=0x1000;ctx.ecx=0x6000;ctx.ebx=0xDEADBEEF;
    sub_000140B6(ctx,memory.data());
    Check(ctx.esp==0x1004 && ctx.ecx==0x12345678 && ctx.ebx==0xDEADBEEF &&
          (ctx.eax&0xFFFFFF00)==0xAABBCC00,"intro hook preserves the native return context");
    return (ctx.eax&255)!=0;
}
bool CanPause() {
    X86Context ctx{};ctx.esp=0x1000;ctx.ebx=0xDEADBEEF;
    sub_00011104(ctx,memory.data());
    Check(ctx.esp==0x1004 && ctx.ecx==0x87654321 && ctx.ebx==0xDEADBEEF &&
          (ctx.eax&0xFFFFFF00)==0xAABBCC00,"pause gate preserves the native return context");
    return (ctx.eax&255)!=0;
}
bool PauseRequested() {return (kernel::R16(Input+0x12)&0x900) && CanPause();}
}
int main() {
    kernel::g_base=memory.data();
    for(unsigned player:{0u,1u}) {
        Setup();Buttons(player,0x1840,0x1840,0x1840);
        kernel::W16(Input+0x14,0x20);kernel::W16(Input+0x28+player*0x98,0x4000);
        Check(!PauseRequested(),"Start cannot pause an active Versus intro before its next simulation update");
        Check(CompleteIntro(),"either player's Start still completes the original intro");
        Check(kernel::R16(Input+0x26+player*0x98)==0x1040 &&
              kernel::R16(Input+0x2A+player*0x98)==0x1040 && kernel::R16(Input+0x12)==0x1040 &&
              kernel::R16(Input+0x16)==0x1040,
              "skipping consumes Start from player and aggregate press/repeat events only");
        Check(kernel::R16(Input+0x24+player*0x98)==0x1840 && kernel::R16(Input+0x14)==0x20 &&
              kernel::R16(Input+0x28+player*0x98)==0x4000,
              "skip consumption preserves held buttons, release events and unrelated controls");
        kernel::W32(Versus+0x14,2);
        Check(!PauseRequested(),"intro-to-round transition cannot reuse the skip as a pause");
        for(unsigned state:{2u,3u,4u}) {
            kernel::W32(Versus+0x14,state);
            Buttons(player,0x800,0,0x800);
            Check(!PauseRequested(),"holding the skipped Start cannot generate a new pause press");
        }
        Buttons(player,0,0,0);Buttons(player,0x800,0x800,0x800);
        Check(PauseRequested(),"a new Start press after release still pauses the fight");
        native_pause_allowed=false;
        Check(!PauseRequested(),"the host cannot bypass an original pause restriction");
    }
    Setup();natural_completion=true;Buttons(0,0x40,0x40,0x40);
    Check(CompleteIntro() && kernel::R16(Input+0x12)==0x40,"natural intro completion preserves other input");
    kernel::W32(Versus+0x14,4);Buttons(0,0x800,0x800,0x800);
    Check(PauseRequested(),"normal completion leaves the next Start press available for pause");
    Setup();Buttons(0,0x900,0x900,0x900);
    Check(PauseRequested(),"the original Back pause remains available during the intro");
    for(unsigned mode:{0u,1u}) {
        Setup(mode);Buttons(0,0x800,0x800,0x800);
        Check(CompleteIntro() && kernel::R16(Input+0x12)==0x800 && CanPause(),
              "solo and Ko-op retain their original intro and pause input");
    }
    Setup(2,4);Buttons(0,0x800,0x800,0x800);
    Check(CompleteIntro() && kernel::R16(Input+0x12)==0x800,"an out-of-state callback cannot consume fighting input");
    Setup();kernel::W32(0x5ACEB8,0);Buttons(0,0x800,0x800,0x800);
    Check(CompleteIntro() && CanPause() && kernel::R16(Input+0x12)==0x800,
          "an absent Versus manager leaves the native callbacks unchanged");
    if(failures)return 1;
    std::puts("PASS: Versus intro skip, held Start, new-press pause, other input and native ABI");
}
