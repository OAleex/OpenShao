#include "strip_orientation.h"
#include "pipeline_precompile.h"
#include "pipeline_assets.h"
#include "async_load_guard.h"
#include "runtime_paths.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <cstdlib>

namespace openshao::async_load {
[[noreturn]] void Stop(const char* reason, uint32_t record, uint32_t requested, uint32_t destination) {
    const uint32_t room = kernel::R32(0x505970), phase = kernel::R32(0x695304);
    const bool valid_record = CommittedSpan(record, 0x34);
    const uint32_t resource = valid_record ? kernel::R32(record) : 0;
    const uint32_t type = valid_record ? kernel::R32(record + 4) : 0;
    const uint32_t expected = valid_record ? kernel::R32(record + 0x10) : 0;
    const uint32_t compressed = valid_record ? kernel::R32(record + 0x30) : 0;
    uint32_t pool_available = 0, pool_fragmented = 0;
    if (valid_record) {
        const uint32_t pool = 0x62EC60 + (type & 0x7Fu) * 0x68;
        pool_fragmented = kernel::R32(pool + 0x1C);
        if (!pool_fragmented)
            pool_available = kernel::R32(pool + 0x0C) - kernel::R32(pool + 8);
    }
    LOGE("async asset load refused: %s; room=%X phase=%u record=%08X resource=%08X pool=%08X request=%u output=%08X expected=%u compressed=%u free-linear=%u fragmented=%u",
         reason, room, phase, record, resource, type, requested, destination, expected, compressed,
         pool_available, pool_fragmented != 0);
    paths::ReportError(L"OpenShao could not load a required game resource safely. Restart the game and attach the generated diagnostic ZIP to the issue if the problem repeats.");
    kernel::Shutdown(62);
    std::abort();
}
}

#include "strip_orientation_hooks.inl"
#include "pipeline_asset_hooks.inl"
#include "portal_effect_hooks.inl"
