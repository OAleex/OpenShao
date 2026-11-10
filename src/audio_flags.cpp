// XGetAudioFlags (0x2ACD43): low word stereo 0, mono 1, surround 2, matching the host output; no AC3/DTS bits.
#include "audio_output.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"

X86_EXTERN_FUNC(__imp__sub_002ACD43);
X86_FUNC(sub_002ACD43) {
    __imp__sub_002ACD43(ctx, base);
    const uint32_t flags = ctx.eax;
    const auto mode=openshao::audio::output::Current().EffectiveMode();
    ctx.eax = mode==openshao::audio::output::Mode::Mono ? 1 : mode==openshao::audio::output::Mode::Surround ? 2 : 0;
    LOG_FIRST(1, "audio: console audio flags %08X reported as native %s", flags,openshao::audio::output::Name(mode));
}
