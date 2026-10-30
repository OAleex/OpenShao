#include "pipeline_precompile.h"
#include "kernel/kernel.h"

namespace openshao::pipelines {
// The same room identifier the room-specific geometry repairs use.
uint32_t CurrentRoom() { return kernel::R32(0x505970); }
}
