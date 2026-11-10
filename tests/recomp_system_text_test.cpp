#include "host.h"
#include "launcher.h"
#include "menu_text.h"
#include "runtime_paths.h"
#include "kernel/kernel.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
static std::vector<std::string> loaded;
static uint32_t loaded_count=0,next_address=0x700000;
static std::string drawn;
static unsigned reboots=0;
static uint32_t storage_mode=0;
namespace openshao {Options options;}
namespace openshao::launcher {const Settings& Current(){return settings;}}
namespace openshao::paths {const std::filesystem::path& SaveRoot(){static const std::filesystem::path root="unused";return root;}}
#ifndef _WIN32
namespace linux_platform {void OpenFolder(const std::filesystem::path&){std::abort();}}
#endif
namespace kernel {uint8_t* g_base=memory.data();}
namespace mem {
bool IsCommitted(uint32_t address){return address<memory.size();}
uint32_t AllocSystem(uint32_t size,uint32_t){const auto address=next_address;next_address+=(size+15)&~15u;return next_address<memory.size()?address:0;}
}
namespace logx {void Write(Level,const char*,...) {}}
#include "difficulty.h"
static unsigned slot_difficulties=0;
namespace openshao::difficulty {bool SaveListText(X86Context& ctx,uint8_t* base,TextCall,void(*original)(X86Context&,uint8_t*)){
    if(kernel::R32(ctx.esp)!=0x9F685)return false;
    original(ctx,base);++slot_difficulties;return true;}}
X86_FUNC(__imp__sub_00015C4F) {
    const auto resource=kernel::R32(ctx.esp+4);auto cursor=kernel::R32(resource+8);
    loaded_count=kernel::R32(cursor);cursor+=4;loaded.clear();
    if(loaded_count==12)for(unsigned i=0;i<12;++i) {
        const auto length=kernel::R32(cursor+4);cursor+=8;
        loaded.emplace_back(static_cast<const char*>(kernel::HostPtr(cursor)),length-1);cursor+=length;
    }
    kernel::Return(ctx,1,0);
}
X86_FUNC(__imp__sub_00152A30){drawn=static_cast<const char*>(kernel::HostPtr(kernel::R32(ctx.esp+20)));kernel::Return(ctx,77,0);}
X86_FUNC(__imp__sub_0011B960){++reboots;kernel::Return(ctx,99,0);}
X86_FUNC(__imp__sub_00015C45){storage_mode=kernel::R32(ctx.esp+4);kernel::Return(ctx,1,0);}
X86_WEAK_FUNC(sub_00015C45,__imp__sub_00015C45)
X86_EXTERN_FUNC(sub_00015C4F);
X86_EXTERN_FUNC(sub_00152A30);
X86_EXTERN_FUNC(sub_0011B960);
X86_EXTERN_FUNC(sub_00015C45);

int main() {
    unsigned failures=0;const auto check=[&](bool good,const char* message){if(!good){++failures;std::cerr<<message<<'\n';}};
    X86Context ctx{};
    const std::array<const char*,12> ids{"undefined","reconnect_port","reconnect","reconnect_accept","load_corrupt","save_corrupt","storage_space","disc_error","unsaved_progress","no","yes","navigation"};
    for(const auto* locale:openshao::launcher::languages) {
        settings.language=locale;
        kernel::W32(0x2008,0x3000);kernel::W32(0x3000,12);ctx.esp=0x1000;kernel::W32(0x1004,0x2000);
        sub_00015C4F(ctx,memory.data());
        check(loaded.size()==12,"The native system loader must receive all twelve JSON entries");
        for(size_t i=0;i<loaded.size();++i)
            check(loaded[i]==openshao::menu::EncodeNative(openshao::menu::Catalog::Builtin().Get(locale,"system."+std::string(ids[i])),settings.language=="pt-BR",255),
                  "Every system message must use the selected text language and its native glyph encoding");
        check(kernel::R32(0x2008)==0x3000 && kernel::R32(0x3000)==12 && ctx.eax==1 && ctx.esp==0x1004,
              "System localization must restore the original resource pointer and preserve native allocation ownership and return conventions");
        ctx.esp=0x1000;kernel::W32(0x1014,0x4AAC04);sub_00152A30(ctx,memory.data());
        check(drawn==openshao::menu::EncodeNative(openshao::menu::Catalog::Builtin().Get(locale,"system.wait"),settings.language=="pt-BR",255),
              "The executable's Please Wait caption must follow all six text languages");
        check(kernel::R32(0x1014)==0x4AAC04 && ctx.eax==77 && ctx.esp==0x1004,
              "Wait localization must restore its draw argument and native return contract");
    }
    kernel::W32(0x3000,13);ctx.esp=0x1000;sub_00015C4F(ctx,memory.data());
    check(loaded_count==13,"Unknown system-table layouts must retain their original loader path");
    std::memcpy(memory.data()+0x4000,"Unrelated text",15);ctx.esp=0x1000;kernel::W32(0x1014,0x4000);sub_00152A30(ctx,memory.data());
    check(drawn=="Unrelated text","Other native text draws must remain untouched");
    drawn.clear();ctx.esp=0x1000;kernel::W32(0x1000,0x9F685);kernel::W32(0x1014,0x4000);sub_00152A30(ctx,memory.data());
    check(drawn=="Unrelated text" && slot_difficulties==1 && ctx.esp==0x1004,"Save slot rows must be routed through the slot list layout");
    kernel::W32(0x1000,0);
    ctx.esp=0x1000;kernel::W32(0x1004,0);sub_00015C45(ctx,memory.data());
    check(storage_mode==1 && kernel::R32(0x1004)==0 && ctx.esp==0x1004 && ctx.eax==1,
          "The PC storage warning must offer its folder action directly without a destructive Xbox dashboard confirmation");
    openshao::options.background=true;ctx.esp=0x1000;kernel::W32(0x1000,0xA0EAE);sub_0011B960(ctx,memory.data());
    check(reboots==0 && ctx.esp==0x1004,"Storage management must never launch the Xbox dashboard or open a folder during background tests");
    ctx.esp=0x1000;kernel::W32(0x1000,0x123456);sub_0011B960(ctx,memory.data());
    check(reboots==1 && ctx.eax==99,"Unrelated native restart requests must keep their original behavior");
    return failures?1:0;
}
