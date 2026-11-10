#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include "core/input.h"
struct X86Context { uint32_t esp=0x1100,eax=0,eip=0; };
std::array<uint8_t,4096> bytes;
namespace mem { bool IsCommitted(uint32_t p) {return p>=0x1000 && p<0x2000;} }
namespace kernel {
uint8_t* HostPtr(uint32_t p) {return bytes.data()+p-0x1000;}
uint32_t R32(uint32_t p) {uint32_t v;std::memcpy(&v,HostPtr(p),4);return v;}
void W32(uint32_t p,uint32_t v) {std::memcpy(HostPtr(p),&v,4);}
void W16(uint32_t p,uint16_t v) {std::memcpy(HostPtr(p),&v,2);}
void Return(X86Context& c,uint32_t value,int count) {c.eax=value;c.esp+=4*(1+count);}
}
using namespace kernel;
namespace input { Pad state{true}; Pad Poll(int){return state;} }
namespace openshao::controller { bool motor=true; bool HasRumble(int){return motor;} }
std::mutex g_ports_lock;
struct Handle {uint32_t port;uint32_t packet=0;input::Pad last;};
std::unordered_map<uint32_t,Handle> g_handles{{123,{0}}};
void __imp__sub_0048E398(X86Context& c,uint8_t*) {Return(c,1167,2);}
std::vector<std::string> logs;
template<class... Args> void Trace(const char* format,Args... args) {
    char text[512];std::snprintf(text,sizeof(text),format,args...);logs.emplace_back(text);
}
#define LOGI(...) Trace(__VA_ARGS__)
#define LOG_FIRST(...) ((void)0)
#define X86_FUNC(name) void name(X86Context& ctx,uint8_t* base)
#include "capabilities_test.inl"
void SyncRetailFrontendMode() {}
namespace openshao::save_pages { void HoldAnswer(input::Pad&) {} }
#include "input_state_test.inl"
static bool Call(uint32_t handle,uint32_t destination,uint32_t expected) {
    X86Context c; W32(c.esp,0x87654321);W32(c.esp+4,handle);W32(c.esp+8,destination);
    sub_0048E398(c,nullptr);
    return c.eax==expected && c.esp==0x110C;
}
int main() {
    bytes.fill(0xCD);
    if (!Call(123,0x1201,0)) {std::puts("FAIL: connected host handle must return capabilities, not 1167");return 1;}
    auto* out=HostPtr(0x1201);
    if (out[-1]!=0xCD || out[25]!=0xCD || out[0]!=1 || out[1] || out[2] || out[3]!=255 || out[4]) return 1;
    for(int i=5;i<13;++i) if(out[i]!=255) return 1;
    for(int i=13;i<21;i+=2) if(out[i]!=255 || out[i+1]!=127) return 1;
    for(int i=21;i<25;++i) if(out[i]!=255) return 1;
    openshao::controller::motor=false;
    if(!Call(123,0x1201,0)) return 1;
    for(int i=21;i<25;++i) if(out[i]) return 1;
    bytes.fill(0xCD); auto snapshot=bytes;
    if(!Call(999,0x1201,6) || std::memcmp(HostPtr(0x1200),snapshot.data()+0x200,27)) return 1;
    input::state.connected=false;
    if(!Call(123,0x1201,1167) || std::memcmp(HostPtr(0x1200),snapshot.data()+0x200,27)) return 1;
    input::state.connected=true;
    if(!Call(123,0x1FF0,87)) return 1;
    // Exercise the adapted XInputGetState body, including the report written to guest memory.
    bytes.fill(0xCD);X86Context state_context;W32(state_context.esp,0x12345678);
    const uint32_t state_args[]={123,0x1401};
    input::state={true};g_handles[123]={0};
    if(ReadInputState(state_context,state_args)!=0) return 1;
    const auto initial_packet=R32(0x1401);
    logs.clear();input::state.analog[1]=255;
    if(ReadInputState(state_context,state_args)!=0 || HostPtr(0x1401)[7]!=255 ||
            R32(0x1401)!=initial_packet+1 || HostPtr(0x1400)[0]!=0xCD || HostPtr(0x1417)[0]!=0xCD) return 1;
    if(logs.size()!=1 || logs[0].find("B=255")==std::string::npos) {
        std::puts("FAIL: an isolated B press must be recorded in the guest-delivery log");return 1;
    }
    logs.clear();input::state.analog[1]=0;
    if(ReadInputState(state_context,state_args)!=0 || HostPtr(0x1401)[7] ||
            R32(0x1401)!=initial_packet+2 || logs.size()!=1 || logs[0].find("B=0")==std::string::npos) {
        std::puts("FAIL: B release must reach the report and the log");return 1;
    }
    logs.clear();ReadInputState(state_context,state_args);
    if(!logs.empty() || R32(0x1401)!=initial_packet+2) return 1;
    for(unsigned button=2;button<8;++button) {
        input::state.analog[button]=127;logs.clear();ReadInputState(state_context,state_args);
        if(HostPtr(0x1401)[6+button]!=127 || logs.size()!=1) return 1;
        input::state.analog[button]=0;logs.clear();ReadInputState(state_context,state_args);
        if(HostPtr(0x1401)[6+button] || logs.size()!=1) return 1;
    }
    std::puts("PASS: packed 25-byte capabilities, four axes, motors, canaries and rejected handles/buffers");
    std::puts("PASS: guest B press/release, all analog button transitions, packet counts and delivery logs");
}
