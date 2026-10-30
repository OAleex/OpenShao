#include "menu_frames.h"
#include <iostream>
#include <vector>

namespace m=openshao::frames::menu;
namespace {
void Put32(uint8_t* p,uint32_t v){std::memcpy(p,&v,4);}
void Triangle(m::Polygons& q,uint32_t index,float dx,float dy,uint32_t texture,uint8_t alpha) {
    auto* r=q.data()+4+index*m::PolygonRecord;
    r[0]=1;r[5]=1;
    const float xy[3][2]={{10,10},{50,10},{10,50}};
    for(int v=0;v<3;++v){m::SetF32(r+0x10+v*16,xy[v][0]+dx);m::SetF32(r+0x14+v*16,xy[v][1]+dy);}
    for(int v=0;v<3;++v)r[0x58+v*4+3]=alpha;
    Put32(r+0x68,texture);
}
}

int main() {
    unsigned failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    m::Polygons a{},b{},out{};
    Put32(a.data(),2);Put32(b.data(),2);
    Triangle(a,0,0,0,7,0);Triangle(b,0,20,-10,7,200);
    Triangle(a,1,0,0,8,255);Triangle(b,1,0,0,9,255);
    check(m::BlendPolygons(a,b,out,.5f)==1,"Only triangles with the same texture must blend");
    const auto* r=out.data()+4;
    check(m::F32(r+0x10)==20 && m::F32(r+0x14)==5,"A moving triangle must be drawn halfway between updates");
    check(r[0x58+3]==100,"Fading colours must be drawn halfway between updates");
    check(std::memcmp(out.data()+4+m::PolygonRecord,b.data()+4+m::PolygonRecord,m::PolygonRecord)==0,
          "A triangle that changed texture must keep its current record");
    Triangle(b,0,400,0,7,200);
    check(m::BlendPolygons(a,b,out,.5f)==0 && m::F32(out.data()+4+0x10)==410,"A jump across the screen must not be blended");
    Put32(b.data(),1);
    check(m::BlendPolygons(a,b,out,.5f)==0 && out==b,"A changed triangle count must keep the current queue");
    Put32(b.data(),600);
    check(m::BlendPolygons(a,b,out,.5f)==0,"An invalid triangle count must be rejected");

    m::Sprites sa{},sb{},so{};
    Put32(sa.data(),1);Put32(sb.data(),1);
    auto* s0=sa.data()+4;auto* s1=sb.data()+4;
    s0[1]=s1[1]=1;Put32(s0+0x28,3);Put32(s1+0x28,3);
    m::SetF32(s0+4,0);m::SetF32(s1+4,40);m::SetF32(s0+8,10);m::SetF32(s1+8,50);
    check(m::BlendSprites(sa,sb,so,.5f)==1 && m::F32(so.data()+8)==20 && m::F32(so.data()+12)==30,
          "A moving rectangle must be drawn halfway between updates");

    constexpr uint32_t address=openshao::frames::FontAddresses[0];
    auto text=[&](float x,const char* s) {
        openshao::frames::FontQueue q{};
        Put32(q.data(),1);Put32(q.data()+4,uint32_t(std::strlen(s)+1));
        std::memcpy(q.data()+m::TextPool,s,std::strlen(s)+1);
        m::SetF32(q.data()+8,x);m::SetF32(q.data()+12,100);
        Put32(q.data()+8+0x34,address+m::TextPool);
        return q;
    };
    auto ta=text(0,"START"),tb=text(30,"START"),to=tb;
    check(m::BlendText(ta,tb,to,address,.5f)==1 && m::F32(to.data()+8)==15,"Moving text must be drawn halfway between updates");
    auto tc=text(30,"OPTIONS");
    check(m::BlendText(ta,tc,to,address,.5f)==0 && m::F32(to.data()+8)==30,"Different text must not be blended");
    return failures?1:0;
}
