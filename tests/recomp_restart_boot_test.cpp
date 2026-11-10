#include "restart.h"
#include "kernel/kernel.h"
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
static std::array<uint8_t,8<<20> memory{};
static unsigned movies=0,route=99,profiles=0;
static unsigned brightness_calls=0,movies_at_brightness=0;
static uint32_t brightness=0;
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count) {
    if(fn==0x123A50) {
        if(count!=1)std::abort();
        ++brightness_calls;movies_at_brightness=movies;brightness=args[0];
        ctx.esp=0xBAD;ctx.eax=0xBAD;ctx.ebx=0xBAD;return 0;
    }
    if(fn!=0x146C9 || count || R32(0x6C930C)!=0x598430)std::abort();
    ++profiles;G<uint8_t>(0x59E5F4)=1;W32(0x4DF5C0,0x3F000000);
    ctx.esp=0xBAD;ctx.eax=0xBAD;return 0;
}
}
namespace mem {bool IsCommitted(uint32_t address){return address<memory.size();}}
namespace logx {void Write(Level,const char*,...) {}}
X86_FUNC(__imp__sub_00128DE0){++movies;kernel::Return(ctx,9,0);}
X86_EXTERN_FUNC(sub_00128DE0);
X86_FUNC(__imp__sub_00013391){route=kernel::R32(0x514738);kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,route==1?0x598430:0x5982C8);kernel::Return(ctx,0,0);}
X86_EXTERN_FUNC(sub_00013391);
int main() {
    using namespace openshao::restart;
    unsigned failed=0;auto check=[&](bool value,const char* message){if(!value){++failed;std::cerr<<message<<'\n';}};
    X86Context ctx{};
    auto movie=[&](const char* name){ctx.esp=0x1000;ctx.ebx=0x11223344;std::memcpy(memory.data()+0x2000,name,std::strlen(name)+1);kernel::W32(0x1004,0x2000);sub_00128DE0(ctx,memory.data());};
    BeginResume(false);movie("D:\\FRONT\\MOVIES\\MIDWAY.SFD");
    check(movies==1 && ctx.eax==9,"Normal startup must retain original movie playback");
    BeginResume(true);movie("D:\\FRONT\\MOVIES\\MIDWAY.SFD");
    check(movies==1 && ctx.eax==4 && ctx.esp==0x1004,"A language restart must skip the logo through the native end-of-playback result and preserve cdecl stack cleanup");
    movie("d:/front/movies/opening.sfd");check(movies==1 && ctx.eax==4,"The restart must skip the opening movie regardless of path letter case");
    movie("D:\\FRONT\\MOVIES\\END006.SFD");check(movies==2,"A restart must not suppress unrelated ending or gallery videos");
    kernel::W32(0x514738,0);ctx.esp=0x1000;sub_00013391(ctx,memory.data());
    check(route==1 && kernel::R32(0x6C930C)==0x598430,"A restart must use the retail return-to-main route without injecting player input");
    check(kernel::R32(0x514738)==0 && !Resuming(),"The startup route and one-shot flag must be restored after frontend initialization");
    check(profiles==1 && kernel::G<uint8_t>(0x59E5F4)==1,
          "A fresh language-restart process must initialize the native profile and save-space state before exposing the main menu");
    check(ctx.esp==0x1004 && ctx.eax==0,"Native profile initialization must preserve the frontend callback's return registers and stack");
    ctx.esp=0x1000;sub_00013391(ctx,memory.data());
    check(route==0 && kernel::R32(0x6C930C)==0x5982C8,"Later normal frontend startup must retain its original route");
    check(profiles==1,"Ordinary frontend returns must not reload the profile or repeat storage dialogs");
    movie("D:\\FRONT\\MOVIES\\OPENING.SFD");check(movies==3,"Later opening/movie playback must use the original path after the one-shot restart finishes");
    for(const auto value:{0.f,.23f,.5f,1.f}) {
        const auto bits=std::bit_cast<uint32_t>(value);
        kernel::W32(0x4DF5C0,bits);
        for(const char* path:{"D:\\FRONT\\MOVIES\\MIDWAY.SFD","d:/front/movies/opening.sfd"}) {
            const auto previous_calls=brightness_calls,previous_movies=movies;
            movie(path);
            check(brightness_calls==previous_calls+1 && brightness==bits,
                  "The logo after a startup movie must use saved brightness, including zero");
            check(movies_at_brightness==previous_movies+1,
                  "Startup brightness must apply after original movie playback and cleanup");
            check(kernel::R32(0x4DF5C0)==bits,"Drawing the startup logo must not change the saved preference");
            check(ctx.esp==0x1004 && ctx.eax==9 && ctx.ebx==0x11223344,
                  "Applying logo brightness must preserve the movie result, stack and registers");
        }
    }
    const auto previous_calls=brightness_calls;
    movie("D:\\FRONT\\MOVIES\\END006.SFD");
    check(brightness_calls==previous_calls,"Other movie exits must retain their native brightness behavior");
    BeginResume(true);movie("D:\\FRONT\\MOVIES\\MIDWAY.SFD");
    check(brightness_calls==previous_calls && ctx.eax==4,"Skipped restart movies must not apply a new gamma ramp");
    return failed?1:0;
}
