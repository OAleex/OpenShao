#include "host.h"
#include "difficulty.h"
#include "launcher.h"
#include "menu_text.h"
#include "runtime_paths.h"
#include "kernel/kernel.h"
#include <array>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include "linux/linux_platform.h"
#endif

namespace {
constexpr std::array<const char*,12> Keys{
    "undefined", "reconnect_port", "reconnect", "reconnect_accept", "load_corrupt", "save_corrupt",
    "storage_space", "disc_error", "unsaved_progress", "no", "yes", "navigation"};

uint32_t Store(const std::string& key,const std::vector<uint8_t>& bytes) {
    static std::mutex mutex;
    static std::unordered_map<std::string,uint32_t> addresses;
    std::lock_guard lock(mutex);
    if(const auto found=addresses.find(key);found!=addresses.end())return found->second;
    const auto address=mem::AllocSystem(uint32_t(bytes.size()));
    if(!address)return 0;
    std::memcpy(kernel::HostPtr(address),bytes.data(),bytes.size());
    addresses.emplace(key,address);return address;
}
std::string Text(std::string_view id) {
    const auto& locale=openshao::launcher::Current().language;
    return openshao::menu::EncodeNative(openshao::menu::Catalog::Builtin().Get(locale,id),locale=="pt-BR",255);
}
uint32_t Table() {
    std::vector<uint8_t> bytes;
    const auto word=[&](uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(uint8_t(value>>(i*8)));};
    word(uint32_t(Keys.size()));
    for(const auto* key:Keys) {
        const auto text=Text("system."+std::string(key));
        word(0);word(uint32_t(text.size()+1));
        bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0);
    }
    return Store("table:"+openshao::launcher::Current().language,bytes);
}
}

X86_EXTERN_FUNC(__imp__sub_00015C4F);
X86_FUNC(sub_00015C4F) {
    const auto resource=kernel::R32(ctx.esp+4);
    if(resource && resource<=UINT32_MAX-11 && mem::IsCommitted(resource+11)) {
        const auto original=kernel::R32(resource+8);
        if(original && original<=UINT32_MAX-3 && mem::IsCommitted(original+3) && kernel::R32(original)==Keys.size()) {
            if(const auto translated=Table()) {
                // Feed serialized text to the normal loader; the original resource stays unchanged.
                struct Restore {uint32_t slot,value;~Restore(){kernel::W32(slot,value);}} restore{resource+8,original};
                kernel::W32(resource+8,translated);
                __imp__sub_00015C4F(ctx,base);
                LOGR("System messages loaded from JSON: %s",openshao::launcher::Current().language.c_str());
                return;
            }
        }
    }
    __imp__sub_00015C4F(ctx,base);
}

X86_EXTERN_FUNC(__imp__sub_00152A30);
X86_FUNC(sub_00152A30) {
    if(openshao::difficulty::SaveListText(ctx,base,openshao::difficulty::TextCall::Plain,__imp__sub_00152A30))return;
    const auto slot=ctx.esp+20,original=kernel::R32(slot);
    if(original==0x4AAC04) {
        const auto text=Text("system.wait");
        std::vector<uint8_t> bytes(text.begin(),text.end());bytes.push_back(0);
        if(const auto address=Store("wait:"+openshao::launcher::Current().language,bytes)) {
            struct Restore {uint32_t slot,value;~Restore(){kernel::W32(slot,value);}} restore{slot,original};
            kernel::W32(slot,address);
            __imp__sub_00152A30(ctx,base);return;
        }
    }
    __imp__sub_00152A30(ctx,base);
}

X86_EXTERN_FUNC(__imp__sub_00015C45);
X86_FUNC(sub_00015C45) {
    // Opening a folder keeps progress: skip the dashboard-reboot warning.
    const auto slot=ctx.esp+4,original=kernel::R32(slot);
    struct Restore {uint32_t slot,value;~Restore(){kernel::W32(slot,value);}} restore{slot,original};
    kernel::W32(slot,1);
    __imp__sub_00015C45(ctx,base);
}

X86_EXTERN_FUNC(__imp__sub_0011B960);
X86_FUNC(sub_0011B960) {
    if(kernel::R32(ctx.esp)!=0xA0EAE){__imp__sub_0011B960(ctx,base);return;}
    // Xbox storage management: show the save directory instead of rebooting.
    if(!openshao::options.background) {
#ifdef _WIN32
        ShellExecuteW(nullptr,L"open",openshao::paths::SaveRoot().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#else
        linux_platform::OpenFolder(openshao::paths::SaveRoot());
#endif
    }
    LOGR("Storage management: saves folder requested; background=%u",unsigned(openshao::options.background));
    kernel::Return(ctx,0,0);
}
