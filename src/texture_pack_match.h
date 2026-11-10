#pragma once
#include <cstdint>
#include <optional>

#include "gpu/texture.h"
#include "texture_pack_source.h"

// Which of the game's textures a texture pack may replace, and the pack's entry for one. It depends on the description of
// the texture and not on a graphics API, so each renderer applies it the same way.
namespace openshao::texpack {
// A 2D texture that is not linear, depth or palette based, and is held on the host as BGRA8, BC1, BC2 or BC3.
bool CanReplace(const gpu::TextureDesc& desc, const gpu::FormatInfo& format);

// The entry for a texture whose level 0 bytes hash to `hash`. Empty when the pack has none, or when its picture has another
// aspect ratio than the game's: a replacement is a redraw of the same picture.
std::optional<SourceEntry> Match(const PackSource& source, const gpu::TextureDesc& desc, uint64_t hash);
}
