#include "texture_pack_match.h"

namespace openshao::texpack {
namespace {
constexpr uint32_t kFormatP8 = 0x0B;  // D3DFMT_P8: indices into a palette, not colours
}

bool CanReplace(const gpu::TextureDesc& desc, const gpu::FormatInfo& format) {
    if (desc.cube || desc.depth != 1 || desc.linear || format.depth || desc.format == kFormatP8) return false;
    return format.host == gpu::Host::BGRA8 || format.host == gpu::Host::BC1 || format.host == gpu::Host::BC2 ||
           format.host == gpu::Host::BC3;
}

std::optional<SourceEntry> Match(const PackSource& source, const gpu::TextureDesc& desc, uint64_t hash) {
    auto entry = source.Find(hash);
    if (entry && uint64_t(entry->width) * desc.height != uint64_t(entry->height) * desc.width) return std::nullopt;
    return entry;
}
}
