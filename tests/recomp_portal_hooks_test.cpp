#include "portal_effect.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static std::vector<uint8_t> memory(0xA00000);
struct X86Context {uint32_t esp=0x800000,eax=0,ecx=0;};
namespace kernel {
uint32_t R32(uint32_t a){Check(a<=memory.size()-4,"valid guest read");uint32_t v;std::memcpy(&v,memory.data()+a,4);return v;}
void W32(uint32_t a,uint32_t v){Check(a<=memory.size()-4,"valid guest write");std::memcpy(memory.data()+a,&v,4);}
void* HostPtr(uint32_t a){Check(a<memory.size(),"valid guest span");return memory.data()+a;}
}
#define X86_FUNC(name) void name(X86Context&ctx,uint8_t*base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
#define LOG_FIRST(...) ((void)0)
static uint32_t lookup_result=0,ctor_result=0,allocation_result=0;
static constexpr uint32_t descriptor=0x910000,allocation=0x920000,core=allocation+0x18;
static constexpr uint32_t renderer=0x930000,pointers=0x940000,particle=0x950000,cursor=0x960000;
static unsigned original_draws=0;
static uint32_t emitted_bytes=80;
static std::array<uint8_t,80> source_vertices{};
X86_FUNC(__imp__sub_00105E10){ctx.eax=allocation_result;ctx.esp+=4;}
X86_FUNC(__imp__sub_001ADD80){ctx.eax=lookup_result;ctx.esp+=4;}
X86_FUNC(__imp__sub_001ED400){ctx.eax=ctor_result;ctx.esp+=4;}
X86_FUNC(__imp__sub_001282F0){++original_draws;std::memcpy(kernel::HostPtr(cursor),source_vertices.data(),80);
    kernel::W32(renderer+0x34,cursor+emitted_bytes);ctx.eax=123;ctx.ecx=456;ctx.esp+=8;}
#include "portal_effect_hooks.inl"

static void Lookup(unsigned bank,unsigned effect){X86Context c;kernel::W32(c.esp+4,bank);kernel::W32(c.esp+8,effect);kernel::W32(c.esp+12,descriptor);
    sub_001ADD80(c,nullptr);Check(c.esp==0x800004&&c.eax==lookup_result,"original lookup result and stack are retained");}
static void Create(uint32_t result){X86Context c;kernel::W32(c.esp+4,descriptor);ctor_result=result;
    sub_001ED400(c,nullptr);Check(c.esp==0x800004&&c.eax==result,"original constructor result and stack are retained");}
static void Draw(bool repaired){X86Context c;c.ecx=renderer;kernel::W32(c.esp+4,0);kernel::W32(renderer+0x34,cursor);const auto count=original_draws;
    sub_001282F0(c,nullptr);Check(original_draws==count+1&&c.eax==123&&c.ecx==456&&c.esp==0x800008,"draw executes once with original return registers and stack");
    const auto* actual=static_cast<const uint8_t*>(kernel::HostPtr(cursor));
    if(!repaired){Check(std::memcmp(actual,source_vertices.data(),80)==0,"unmatched or skipped particles remain exact");return;}
    Check(std::memcmp(actual,source_vertices.data(),80)!=0,"identified portal geometry is corrected without a diagnostic flag");
    for(unsigned i=0;i<4;++i){std::array<float,3> p;std::memcpy(p.data(),actual+i*20,12);Check(std::abs(p[2])<1e-5,"corrected portal faces camera");
        Check(std::memcmp(actual+i*20+12,source_vertices.data()+i*20+12,8)==0,"draw retains color and UV");}}
int main(){
    kernel::W32(0x505970,0xA3);kernel::W32(0x7068B8,0x900000);lookup_result=0x900000+105*0xEC;
    kernel::W32(renderer,pointers);kernel::W32(pointers,particle);kernel::W32(particle+0x1C+0x84,0x80);kernel::W32(particle+0x1C+0xE4,core);
    kernel::W32(0x639224,0x970000);kernel::W32(0x970304,0x980000);kernel::W32(0x980348,std::bit_cast<uint32_t>(-10.0f));
    for(unsigned j=0;j<16;++j)kernel::W32(renderer+0x40+j*4,std::bit_cast<uint32_t>(j%5==0?1.0f:0.0f));
    const std::array<std::array<float,3>,4> p{{{0,-2,-1},{0,-2,1},{0,2,1},{0,2,-1}}};
    for(unsigned i=0;i<4;++i){std::memcpy(source_vertices.data()+i*20,p[i].data(),12);for(unsigned j=12;j<20;++j)source_vertices[i*20+j]=uint8_t(i+j);}
    Lookup(2,105);Create(allocation);Draw(true);
    for(const uint32_t room:{0xA1u,0xA2u,0xA4u,0xA5u,0xA6u,0xBBu,0xBEu,0xBFu}){kernel::W32(0x505970,room);Draw(true);}
    for(const uint32_t room:{0x40u,0xA0u,0xC0u}){kernel::W32(0x505970,room);Draw(false);}
    kernel::W32(0x505970,0xA3);
    kernel::W32(particle+0x1C+0x84,0x20080);Draw(false);kernel::W32(particle+0x1C+0x84,0x80);
    emitted_bytes=40;Draw(false);emitted_bytes=80;
    allocation_result=allocation;X86Context reused;sub_00105E10(reused,nullptr);Draw(false);
    Lookup(3,105);Create(allocation);Draw(false);
    Lookup(2,104);Create(allocation);Draw(false);
    lookup_result=0;Lookup(2,105);Create(allocation);Draw(false);
    lookup_result=0x900000+105*0xEC;Lookup(2,105);Create(0);Draw(false);
    kernel::W32(descriptor+0xE8,core);Lookup(2,105);Create(0);Draw(true);
    Lookup(2,104);Create(0);Draw(false);
    std::puts("PASS: original portal identity, arena range/rotation guards, allocation reuse, failures and ABI");
}
