#include "../src/audio_options.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>

static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
static unsigned applied=0;
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t function,const uint32_t* args,size_t count) {
    if (function!=0x1B1900 || count!=1) std::abort();
    W32(0x64CFCC,args[0]); ++applied;
    ctx.eax=0xBAD; ctx.ecx=0xBAD; ctx.esp=0xBAD;
    return 0;
}
}
namespace openshao::launcher { const Settings& Current() {return settings;} }
namespace logx { void Write(Level,const char*,...) {} }
X86_FUNC(__imp__sub_001B1900) {kernel::W32(0x64CFCC,kernel::R32(ctx.esp+4));kernel::Return(ctx,0,1);}
static void Check(bool ok,const char* why) {
    if (!ok) {std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}
}
int main() {
    using namespace openshao::audio;
    X86Context ctx{};ctx.esp=0x3000;ctx.ecx=0x5985E8;ctx.eax=123;
    kernel::W32(0x4DF5C4,0x3F666666);kernel::W32(0x4E5784,0x3F666666);
    options::Apply(ctx,2);
    Check(output::Current().mode==output::Mode::Surround && kernel::R32(0x64CFCC)==3,
          "Saving Surround must select the native six-channel movie route");
    Check(kernel::G<uint8_t>(0x4DF5D0)==2 && kernel::G<uint8_t>(0x4E5790)==2,
          "The committed mode must be synchronized with the title's audio state");
    Check(ctx.esp==0x3000 && ctx.ecx==0x5985E8 && ctx.eax==123,
          "Applying output must preserve the caller's guest context");
    options::Apply(ctx,0);
    Check(output::Current().mode==output::Mode::Mono && kernel::R32(0x64CFCC)==0,
          "Saving Mono must combine both sides through the native output mixer");
    options::Apply(ctx,1);
    Check(output::Current().mode==output::Mode::Stereo && kernel::R32(0x64CFCC)==1 && applied==3,
          "Saving Stereo must restore two-channel routing with exactly one title update");
    Check(kernel::R32(0x4DF5C4)==0x3F666666 && kernel::R32(0x4E5784)==0x3F666666,
          "Output changes must preserve the retail volume controls");
    settings.audio_output=2;ctx.esp=0x3000;sub_00129E40(ctx,memory.data());
    Check(ctx.eax==3 && output::Current().mode==output::Mode::Surround,
          "Startup must load the saved output mode and enable its movie decoder");
    ctx.esp=0x3000;kernel::W32(ctx.esp,0x4A51F);sub_001B1920(ctx,memory.data());
    Check(ctx.eax==2,"Retail options must retain a normalized output value without exposing the internal movie mode");
    ctx.esp=0x3000;kernel::W32(ctx.esp,0x128B88);sub_001B1920(ctx,memory.data());
    Check(ctx.eax==3,"Movie callers must retain the existing multichannel decoder");
    settings={};ctx.esp=0x3000;sub_00129E40(ctx,memory.data());
    Check(ctx.eax==1 && output::Current().mode==output::Mode::Stereo,"A new configuration must start in Stereo");
    std::puts("PASS: saved output modes, movie routing, context preservation and stereo default");
}
