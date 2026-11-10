#include "restart.h"
#include "kernel/kernel.h"
#include <atomic>
#include <string_view>

namespace openshao::restart {
namespace {std::atomic<bool> resume{false};}
void BeginResume(bool enabled){resume.store(enabled);}
bool Resuming(){return resume.load();}
void MainMenuReady(){if(resume.exchange(false))LOGR("Language restart: main menu ready");}

}

X86_EXTERN_FUNC(__imp__sub_00128DE0);
X86_FUNC(sub_00128DE0) {
    const auto path=kernel::R32(ctx.esp+4);std::string name;
    for(unsigned i=0;path && i<260 && path<=UINT32_MAX-i && mem::IsCommitted(path+i);++i) {
        const char c=kernel::G<char>(path+i);if(!c)break;
        name+=c=='\\'?'/':c>='a' && c<='z'?char(c-'a'+'A'):c;
    }
    const bool startup=name.ends_with("/FRONT/MOVIES/MIDWAY.SFD") || name.ends_with("/FRONT/MOVIES/OPENING.SFD");
    if(openshao::restart::Resuming() && startup) {
        LOGR("Language restart: skipped startup movie %s",name.c_str());
        // The retail movie wrapper treats four as normal end-of-playback. Its caller cleans the arguments.
        kernel::Return(ctx,4,0);return;
    }
    __imp__sub_00128DE0(ctx,base);
    if(startup) {
        // Give the logo the saved brightness, as Press START does.
        const auto saved=ctx;
        kernel::CallGuest(ctx,0x123A50,{kernel::R32(0x4DF5C0)});
        ctx=saved;
    }
}

X86_EXTERN_FUNC(__imp__sub_00013391);
X86_FUNC(sub_00013391) {
    if(!openshao::restart::Resuming()){__imp__sub_00013391(ctx,base);return;}
    // Route 1 skips the title but still needs its profile setup.
    const auto previous=kernel::R32(0x514738);
    kernel::W32(0x514738,1);
    __imp__sub_00013391(ctx,base);
    kernel::W32(0x514738,previous);
    const auto saved=ctx;
    kernel::CallGuest(ctx,0x146C9,{});
    ctx=saved;
    LOGR("Language restart: native profile initialized; save_space_ready=%u",kernel::G<uint8_t>(0x59E5F4));
    openshao::restart::MainMenuReady();
}
