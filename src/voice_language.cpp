#include "launcher.h"
#include "kernel/kernel.h"

// Console language initializes the text tables. Only this shared-bank caller requests voice data.
X86_FUNC(sub_0010F0E0) {
    const auto language=kernel::R32(ctx.esp)==0xCF575?openshao::launcher::VoiceLanguageIndex():openshao::launcher::Language();
    kernel::Return(ctx,language,0);
}

X86_FUNC(sub_00014A75) {
    const auto caller=kernel::R32(ctx.esp);
    // The retail bank remapper and movie audio selector are independent of text/font selection.
    const bool voices=caller==0xCD707 || caller==0x1B4CF8;
    const auto language=voices?openshao::launcher::VoiceLanguageIndex():kernel::R32(0x5B04AC);
    kernel::Return(ctx,language,0);
}
