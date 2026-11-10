#include "raster_compat.h"
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <dxgiformat.h>
#include "gpu/vertex.h"

namespace openshao {
bool UnitSpriteTexcoords(const gpu::Assembled& a) {
    if (!a.pretransformed || a.topology != gpu::Topology::Triangles || a.vertex_count < 3 || a.vertex_count > 6) return false;
    auto read = [&](unsigned r, unsigned v, float* value) {
        const auto& e=a.elements[r];
        if (e.format!=DXGI_FORMAT_R32G32B32A32_FLOAT || e.buffer>=a.buffers.size()) return false;
        const auto& b=a.buffers[e.buffer];
        if (!b.src || !b.stride || uint64_t(v)*b.stride+e.offset+16>b.bytes) return false;
        std::memcpy(value,b.src+size_t(v)*b.stride+e.offset,16);
        return std::all_of(value,value+4,[](float x) { return std::isfinite(x); });
    };
    if (!(a.regs&1)) return false;
    float lo[2]={1e20f,1e20f}, hi[2]={-1e20f,-1e20f};
    for (unsigned v=0;v<a.vertex_count;++v) {
        float p[4]; if (!read(0,v,p) || (std::fabs(p[3])>1e-5f && std::fabs(p[3]-1)>1e-5f)) return false;
        for (int i=0;i<2;++i) { lo[i]=std::min(lo[i],p[i]); hi[i]=std::max(hi[i],p[i]); }
    }
    for (unsigned v=0;v<a.vertex_count;++v) {
        float p[4]; read(0,v,p);
        for (int i=0;i<2;++i) if (std::min(std::fabs(p[i]-lo[i]),std::fabs(p[i]-hi[i]))>1e-4f) return false;
    }
    bool found = false;
    for (unsigned r = 9; r < 13; ++r) {
        if (!(a.regs & (1u << r))) continue;
        float uvlo[2]={1e20f,1e20f}, uvhi[2]={-1e20f,-1e20f};
        for (unsigned v = 0; v < a.vertex_count; ++v) {
            float uv[4]; if (!read(r,v,uv) || std::fabs(uv[3]-1)>1e-5f) return false;
            for (int i=0;i<2;++i) { uvlo[i]=std::min(uvlo[i],uv[i]); uvhi[i]=std::max(uvhi[i],uv[i]); }
        }
        for (int i=0;i<2;++i) {
            // A retail frame reaches -1.9998126 from -3.0: tolerate that sub-texel error.
            constexpr float tolerance=1.f/4096;
            const float tile=std::floor(uvlo[i]+tolerance);
            if (uvlo[i]<tile-tolerance || uvhi[i]>tile+1+tolerance) return false;
        }
        found = true;
    }
    return found;
}
namespace {
void Replace(std::string& s, const std::string& before, const std::string& after) {
    auto pos=s.find(before);
    if (pos==std::string::npos || s.find(before,pos+before.size())!=std::string::npos)
        throw std::runtime_error("Unexpected SDK quad shader layout");
    s.replace(pos,before.size(),after);
}
const char* fields=R"(    nointerpolation float4 atlas0 : TEXCOORD6;
    nointerpolation float4 atlas1 : TEXCOORD7;
    nointerpolation float4 atlas2 : TEXCOORD8;
    nointerpolation float4 atlas3 : TEXCOORD9;
)";
}
std::string AtlasQuadHlsl(std::string s) {
    const std::string anchor="    noperspective float4 e : TEXCOORD5;\n";
    Replace(s,anchor,anchor+fields);
    if (s.find("    return o;\n")!=std::string::npos) {
        // Snap sprites to the NV2A 1/16-pixel grid before scaling, or tile gaps become columns.
        const std::string screen="    o.e.xy = p.xy;\n";
        if (s.find(screen)!=std::string::npos)
            Replace(s,screen,"    p.xy = trunc(p.xy * 16) / 16;\n"+screen);
        // Quads without the geometry stage retain their exact existing UVs.
        Replace(s,"    return o;\n",
            "    o.atlas0=o.atlas1=o.atlas2=o.atlas3=float4(-1e20,-1e20,1e20,1e20);\n    return o;\n");
    } else {
        const std::string at="float4 main(VOut i, bool front";
        Replace(s,at,R"(float4 atlas_coord(float4 t, float4 bounds) {
    t.xy=clamp(t.xy,bounds.xy,bounds.zw);
    // One repeated tile (including mirrored negative UVs) is the same image
    // after an integer offset. Sampling it clamped avoids the opposite edge.
    if (all(abs(bounds)<1e10)) t.xy-=floor(bounds.xy+1.0/4096);
    return t;
}
)"+at);
        for (int i=0;i<4;++i) {
            auto index=std::to_string(i);
            const auto coord="unstretch(i.tc"+index+", i.e)";
            // A PS can omit a stage entirely.
            if (s.find(coord)!=std::string::npos)
                Replace(s,coord,"atlas_coord("+coord+", i.atlas"+index+")");
        }
    }
    return s;
}
std::string AtlasBoundsHlsl() {
    return std::string(R"(
struct VOut {
    float4 pos : SV_Position;
    float4 d0 : COLOR0;
    float4 d1 : COLOR1;
    float4 b0 : COLOR2;
    float4 b1 : COLOR3;
    float4 tc0 : TEXCOORD0;
    float4 tc1 : TEXCOORD1;
    float4 tc2 : TEXCOORD2;
    float4 tc3 : TEXCOORD3;
    float fogamt : TEXCOORD4;
    noperspective float4 e : TEXCOORD5;
)")+fields+R"(};
// b1 is the existing DrawConstants buffer; the render target extent is first.
cbuffer SpriteSurface : register(b1) { float4 sprite_surface; };
float4 bounds(float2 p0,float2 p1,float2 p2,float4 t0,float4 t1,float4 t2) {
    float4 unrestricted=float4(-1e20,-1e20,1e20,1e20);
    if (any(abs(float3(t0.w,t1.w,t2.w)-1)>1e-5)) return unrestricted;
    float2 uvlo=min(t0.xy,min(t1.xy,t2.xy)), uvhi=max(t0.xy,max(t1.xy,t2.xy));
    const float tolerance=1.0/4096;
    float2 tile=floor(uvlo+tolerance);
    if (any(uvlo<tile-tolerance)||any(uvhi>tile+1+tolerance)) return unrestricted;
    float2 lo=min(p0,min(p1,p2)), hi=max(p0,max(p1,p2));
    float2 a=p1-p0, b=p2-p0;
    float det=a.x*b.y-a.y*b.x;
    if (abs(det)<1e-6) return unrestricted;
    // Only axis aligned sprite triangles qualify. Other geometry keeps its UVs.
    float2 c0=min(abs(p0-lo),abs(p0-hi)), c1=min(abs(p1-lo),abs(p1-hi)), c2=min(abs(p2-lo),abs(p2-hi));
    if (any(c0>1e-4)||any(c1>1e-4)||any(c2>1e-4)) return unrestricted;
    float2 dx=((t1.xy-t0.xy)*b.y-(t2.xy-t0.xy)*a.y)/det;
    float2 dy=((t2.xy-t0.xy)*a.x-(t1.xy-t0.xy)*b.x)/det;
    // A quarter turn swaps the screen axes. It still samples one atlas tile,
    // including the negative V tile used by the brightness frame's right side.
    bool aligned=abs(dx.y)<=1e-6&&abs(dy.x)<=1e-6;
    bool rotated=abs(dx.x)<=1e-6&&abs(dy.y)<=1e-6;
    if (!aligned&&!rotated) return unrestricted;
    float2 first=ceil(lo-1e-4), last=ceil(hi-1e-4)-1;
    if (any(last<first)) return unrestricted;
    float2 begin=t0.xy+dx*(first.x-p0.x)+dy*(first.y-p0.y);
    float2 end=t0.xy+dx*(last.x-p0.x)+dy*(last.y-p0.y);
    return float4(min(begin,end),max(begin,end));
}
[maxvertexcount(3)]
void main(triangle VOut v[3],inout TriangleStream<VOut> output) {
    float4 b0=v[0].atlas0,b1=v[0].atlas1,b2=v[0].atlas2,b3=v[0].atlas3;
    if (all(abs(float3(v[0].pos.w,v[1].pos.w,v[2].pos.w)-1)<1e-5)) {
        b0=bounds(v[0].e.xy,v[1].e.xy,v[2].e.xy,v[0].tc0,v[1].tc0,v[2].tc0);
        b1=bounds(v[0].e.xy,v[1].e.xy,v[2].e.xy,v[0].tc1,v[1].tc1,v[2].tc1);
        b2=bounds(v[0].e.xy,v[1].e.xy,v[2].e.xy,v[0].tc2,v[1].tc2,v[2].tc2);
        b3=bounds(v[0].e.xy,v[1].e.xy,v[2].e.xy,v[0].tc3,v[1].tc3,v[2].tc3);
    }
    for (int i=0;i<3;++i) {
        VOut o=v[i]; o.atlas0=b0; o.atlas1=b1; o.atlas2=b2; o.atlas3=b3;
        if (all(abs(b0)<1e10) && all(sprite_surface.xy>0) && abs(o.pos.w-1)<1e-5) {
            // Cover samples ceil(left)..ceil(right)-1 with whole host pixels (closes transparent joins);
            // e.zw tracks the changed edge so unstretch keeps UVs.
            float2 edge=ceil(o.e.zw-1e-4)-0.5;
            o.pos.xy+=(edge-o.e.zw)*float2(2/sprite_surface.x,-2/sprite_surface.y)*o.pos.w;
            o.e.zw=edge;
        }
        output.Append(o);
    }
}
)";
}
}
