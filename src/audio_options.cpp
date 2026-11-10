#include "audio_output.h"
#include "audio_options.h"
#include "launcher.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <algorithm>

namespace {
namespace output=openshao::audio::output;
constexpr uint32_t Committed=0x4DF5D0,Draft=0x4E5790;
output::Mode Decode(uint32_t mode) { return mode>=2 ? output::Mode::Surround : output::Mode(mode); }
uint32_t MovieMode(output::Mode mode) { return mode==output::Mode::Surround ? 3 : uint32_t(mode); }
void ApplyMode(X86Context& ctx,uint32_t mode) {
    const auto selected=Decode(mode);
    output::SetMode(selected);
    const auto saved=ctx;
    kernel::CallGuest(ctx,0x1B1900,{MovieMode(selected)});
    ctx=saved;
    const auto state=output::Current();
    LOGI("audio: requested %s, effective %s, PCM carrier=%u",output::Name(selected),output::Name(state.EffectiveMode()),state.channels);
}

}

void openshao::audio::options::Apply(X86Context& ctx,int mode) {
    const auto selected=std::clamp(mode,0,2);
    kernel::G<uint8_t>(Committed)=uint8_t(selected);
    kernel::G<uint8_t>(Draft)=uint8_t(selected);
    ApplyMode(ctx,uint32_t(selected));
}

// Digital mode selects the six-channel movie decoder; the host outputs PCM.
X86_FUNC(sub_00129E40) {
    const auto selected=Decode(uint32_t(std::clamp(openshao::launcher::Current().audio_output,0,2)));
    output::SetMode(selected);
    kernel::G<uint8_t>(Committed)=uint8_t(selected);
    kernel::G<uint8_t>(Draft)=uint8_t(selected);
    kernel::Return(ctx,MovieMode(selected),0);
}
X86_EXTERN_FUNC(__imp__sub_001B1900);
X86_FUNC(sub_001B1900) {
    const uint32_t argument=ctx.esp+4,requested=kernel::R32(argument);
    const auto selected=Decode(requested);
    output::SetMode(selected);
    kernel::W32(argument,MovieMode(selected));
    __imp__sub_001B1900(ctx,base);
    kernel::W32(argument,requested);
}
X86_FUNC(sub_001B1920) {
    const uint32_t caller=kernel::R32(ctx.esp);
    const auto mode=output::Current().mode;
    kernel::Return(ctx,caller==0x4A51F ? uint32_t(mode) : MovieMode(mode),0);
}
