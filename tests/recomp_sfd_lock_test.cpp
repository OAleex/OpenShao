#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "sfd_lock_compat.h"
#include "sfd_filter_compat.h"

namespace gpu {
static TextureDesc movie{};
bool Describe(uint32_t resource, TextureDesc& out) {
    if (resource != 0x1000) return false;
    out = movie;
    return true;
}
}

static bool SamplesEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    constexpr size_t n=64, bytes=64;
    if (a.size() != b.size()) return false;
    for (size_t i=0;i<n;++i) {
        size_t pos=(a.size()-bytes)*i/(n-1);
        if (!std::equal(a.begin()+pos,a.begin()+pos+bytes,b.begin()+pos)) return false;
    }
    return true;
}

int main() {
    using openshao::sfd::NeedsWriteInvalidation;
    int failures=0;
    auto check=[&](bool good, const char* message) {
        if (!good) { std::fprintf(stderr,"FAIL: %s\n",message); ++failures; }
    };
    gpu::movie={.data=0x2000,.format=0x12,.width=640,.height=480,.depth=1,.levels=1,
                .pitch=640*4,.cube=false,.linear=true};
    check(NeedsWriteInvalidation(0x1000,0,0,0,0),"movie ARGB writable lock invalidates texture");
    gpu::movie.format=0x24;gpu::movie.pitch=640*2;
    check(NeedsWriteInvalidation(0x1000,0,0,0,0),"movie YUY2 writable lock invalidates texture");
    gpu::movie.height=288;
    check(NeedsWriteInvalidation(0x1000,0,0,0,0),"widescreen movie writable lock invalidates texture");
    gpu::movie.format=0x12;gpu::movie.pitch=640*4;gpu::movie.height=480;
    check(!NeedsWriteInvalidation(0x9999,0,0,0,0),"unknown texture is not invalidated");
    check(!NeedsWriteInvalidation(0x1000,0,0,0,0x10),"read-only/other flagged lock is not invalidated");
    check(!NeedsWriteInvalidation(0x1000,0,1,0,0),"other mip level is not invalidated");
    check(!NeedsWriteInvalidation(0x1000,0,0,1,0),"partial rectangle is not invalidated");
    gpu::movie.width=256;
    check(!NeedsWriteInvalidation(0x1000,0,0,0,0),"other textures are not invalidated");
    gpu::movie.width=640;gpu::movie.linear=false;
    check(!NeedsWriteInvalidation(0x1000,0,0,0,0),"swizzled texture is not invalidated");
    gpu::movie.linear=true;

    // An MPEG macroblock can change outside the SDK's 64 sampled regions, leaving the
    // LockRect signature equal; MarkDirty bumps the epoch to force the upload.
    std::vector<uint8_t> a(size_t(640)*480*4,0x40), b=a;
    bool found=false;
    for (int y=16;y<450 && !found;y+=16) for (int x=16;x<610 && !found;x+=16) {
        b=a;
        for (int yy=y; yy<y+16; ++yy)
            std::fill_n(b.begin()+(size_t(yy)*640+x)*4,16*4,uint8_t(0xC0));
        if (SamplesEqual(a,b)) found=true;
    }
    check(found,"found changed movie macroblock invisible to sampled signature");
    uint64_t epoch=7;
    uint64_t before=epoch;
    if (NeedsWriteInvalidation(0x1000,0,0,0,0)) ++epoch;
    check(epoch != before && SamplesEqual(a,b),
          "writable movie lock advances dirty epoch even when sampled pixels match");
    openshao::sfd::SetSoftDisplayFilter(0);
    check(openshao::sfd::PresentationTaps()==1,"normal gameplay uses unfiltered digital output");
    openshao::sfd::SetSoftDisplayFilter(1);
    check(openshao::sfd::PresentationTaps()==2,"original SFD soft display request enables movie-only smoothing");
    openshao::sfd::SetSoftDisplayFilter(0);
    check(openshao::sfd::PresentationTaps()==1,"finishing SFD restores unfiltered gameplay");
    if (!failures) std::puts("PASS: SFD writable LockRect invalidates partial frames missed by sampled checksums");
    return failures ? 1 : 0;
}
