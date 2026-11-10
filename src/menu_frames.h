#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include "render_frame_state.h"

// Output-only menu frames between 60 Hz updates: matching queued records are blended.
namespace openshao::frames::menu {
inline constexpr uint32_t PolygonQueue=0x63F5E0,SpriteQueue=0x639760;
inline constexpr size_t PolygonBytes=0xD804,PolygonRecord=0x6C,PolygonLimit=512;
inline constexpr size_t SpriteBytes=0x588,SpriteRecord=0x2C,SpriteLimit=32;
inline constexpr size_t TextRecord=0x54,TextPool=0x5408;
// Positions farther apart than this belong to different elements, not one movement.
inline constexpr float MaxStep=160.f;

using Polygons=std::array<uint8_t,PolygonBytes>;
using Sprites=std::array<uint8_t,SpriteBytes>;

inline uint32_t U32(const uint8_t* p){uint32_t v;std::memcpy(&v,p,4);return v;}
inline float F32(const uint8_t* p){float v;std::memcpy(&v,p,4);return v;}
inline void SetF32(uint8_t* p,float v){std::memcpy(p,&v,4);}

// Leaves the record unchanged on a jump.
inline bool BlendFloats(const uint8_t* from,const uint8_t* to,uint8_t* out,std::span<const size_t> offsets,float t) {
    for(auto offset:offsets) {
        const float a=F32(from+offset),b=F32(to+offset);
        if(!std::isfinite(a) || !std::isfinite(b) || std::abs(b-a)>MaxStep)return false;
    }
    for(auto offset:offsets)SetF32(out+offset,F32(from+offset)+(F32(to+offset)-F32(from+offset))*t);
    return true;
}
inline void BlendBytes(const uint8_t* from,const uint8_t* to,uint8_t* out,size_t offset,size_t count,float t) {
    for(size_t i=offset;i<offset+count;++i)out[i]=uint8_t(std::lround(from[i]+(int(to[i])-int(from[i]))*t));
}

// Triangle record: +0 used, +5 textured, vertices +0x10/+0x20/+0x30, colours +0x58, UVs +0x40, texture +0x68.
inline bool PolygonCount(std::span<const uint8_t> queue,uint32_t& count) {
    if(queue.size()!=PolygonBytes)return false;
    count=U32(queue.data());
    return count<=PolygonLimit;
}
inline unsigned BlendPolygons(std::span<const uint8_t> from,std::span<const uint8_t> to,std::span<uint8_t> out,float t) {
    uint32_t a=0,b=0;
    if(!PolygonCount(from,a) || !PolygonCount(to,b) || out.size()!=PolygonBytes)return 0;
    std::memcpy(out.data(),to.data(),PolygonBytes);
    if(a!=b)return 0;
    static constexpr size_t xy[]={0x10,0x14,0x20,0x24,0x30,0x34};
    unsigned blended=0;
    for(uint32_t i=0;i<b;++i) {
        const auto* p=from.data()+4+i*PolygonRecord;const auto* c=to.data()+4+i*PolygonRecord;auto* o=out.data()+4+i*PolygonRecord;
        // Different textures, sampling modes or UVs mean a different element or animation frame.
        if(p[0]!=c[0] || p[5]!=c[5] || std::memcmp(p+0x68,c+0x68,4) || std::memcmp(p+0x40,c+0x40,0x18))continue;
        if(!BlendFloats(p,c,o,xy,t))continue;
        BlendBytes(p,c,o,0x58,12,t);
        ++blended;
    }
    return blended;
}

// Rectangle record: +0 flag, +1 enabled, x0/x1/y0/y1 at +4..+0x10, colours +0x14..+0x23, texture +0x28.
inline unsigned BlendSprites(std::span<const uint8_t> from,std::span<const uint8_t> to,std::span<uint8_t> out,float t) {
    if(from.size()!=SpriteBytes || to.size()!=SpriteBytes || out.size()!=SpriteBytes)return 0;
    std::memcpy(out.data(),to.data(),SpriteBytes);
    const auto a=U32(from.data()),b=U32(to.data());
    if(a!=b || b>SpriteLimit)return 0;
    static constexpr size_t rect[]={4,8,0xC,0x10};
    unsigned blended=0;
    for(uint32_t i=0;i<b;++i) {
        const auto* p=from.data()+4+i*SpriteRecord;const auto* c=to.data()+4+i*SpriteRecord;auto* o=out.data()+4+i*SpriteRecord;
        if(p[0]!=c[0] || p[1]!=c[1] || std::memcmp(p+0x28,c+0x28,4))continue;
        if(!BlendFloats(p,c,o,rect,t))continue;
        BlendBytes(p,c,o,0x14,16,t);
        ++blended;
    }
    return blended;
}

// Text record: x at +0, y at +4, colour at +0x30, string pointer at +0x34, scale at +0x3C/+0x40.
inline const char* TextAt(std::span<const uint8_t> queue,uint32_t address,const uint8_t* row) {
    const auto text=U32(row+0x34),used=U32(queue.data()+4);
    if(text<address+TextPool || text>=address+TextPool+used)return nullptr;
    return reinterpret_cast<const char*>(queue.data()+(text-address));
}
inline unsigned BlendText(std::span<const uint8_t> from,std::span<const uint8_t> to,std::span<uint8_t> out,uint32_t address,float t) {
    if(out.size()!=to.size() || !ValidFontQueue(from,address) || !ValidFontQueue(to,address))return 0;
    std::memcpy(out.data(),to.data(),to.size());
    const auto a=U32(from.data()),b=U32(to.data());
    static constexpr size_t xy[]={0,4};
    unsigned blended=0;
    for(uint32_t row=0;row<std::min(a,b);++row) {
        const auto* p=from.data()+8+row*TextRecord;const auto* c=to.data()+8+row*TextRecord;auto* o=out.data()+8+row*TextRecord;
        const char* pt=TextAt(from,address,p);const char* ct=TextAt(to,address,c);
        if(!pt || !ct || std::strcmp(pt,ct) || std::memcmp(p+0x38,c+0x38,12))continue;
        if(!BlendFloats(p,c,o,xy,t))continue;
        BlendBytes(p,c,o,0x30,4,t);
        ++blended;
    }
    return blended;
}
}
