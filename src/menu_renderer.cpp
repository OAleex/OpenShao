#include "menu_renderer.h"
#include "menu_text.h"
#include "aspect_ratio.h"
#include "launcher.h"
#include "mods/languages.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "core/log.h"
#include <array>
#include <bit>
#include <bitset>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace openshao::menu {
namespace {
constexpr uint32_t Queue=0x63F5E4,Count=0x63F5E0,Record=0x6C;
struct MainEntry {bool visible=false;float left=0,top=0;unsigned alpha=0;};
struct Arrow {uint32_t index;float left;};
struct Scene {float title_right=0;std::vector<Arrow> arrows;bool main_anchor=false;float main_row_top=0;std::array<MainEntry,3> extras;};
thread_local std::vector<Scene> scenes;
thread_local std::bitset<512> viewport_arrows;
float ReadFloat(uint32_t address){return std::bit_cast<float>(kernel::R32(address));}
void WriteFloat(uint32_t address,float value){kernel::W32(address,std::bit_cast<uint32_t>(value));}
struct QuestionBounds {
    static constexpr uint32_t Rect=0x66C0E8+0x6444+70*16;
    uint32_t saved=0;
    bool changed=false;
    explicit QuestionBounds(bool needed) {
        // The retail '?' rectangle overlaps ink from the next atlas row. Stop at the final dot's pixel center.
        if(needed && kernel::R32(0x66C0E8+0x6424)==256 && kernel::R32(0x66C0E8+0x6428)==512 &&
           ReadFloat(Rect)==210.f/256 && ReadFloat(Rect+4)==106.f/512 &&
           ReadFloat(Rect+8)==220.f/256 && ReadFloat(Rect+12)==126.f/512) {
            saved=kernel::R32(Rect+12);WriteFloat(Rect+12,124.5f/512);changed=true;
        }
    }
    ~QuestionBounds(){if(changed)kernel::W32(Rect+12,saved);}
};
struct AcuteBounds {
    static constexpr uint32_t Rect=0x66C0E8+0x6444+79*16;
    uint32_t top=0,bottom=0;
    bool changed=false;
    explicit AcuteBounds(bool needed) {
        // The uppercase acute A ink starts two rows before its authored rectangle.
        // Shift the sampling window to pixel centers without changing the glyph's size or the label baseline.
        if(needed && kernel::R32(0x66C0E8+0x6424)==256 && kernel::R32(0x66C0E8+0x6428)==512 &&
           ReadFloat(Rect)==147.f/256 && ReadFloat(Rect+4)==127.f/512 &&
           ReadFloat(Rect+8)==165.f/256 && ReadFloat(Rect+12)==147.f/512) {
            top=kernel::R32(Rect+4);bottom=kernel::R32(Rect+12);
            WriteFloat(Rect+4,125.5f/512);WriteFloat(Rect+12,145.5f/512);changed=true;
        }
    }
    ~AcuteBounds(){if(changed){kernel::W32(Rect+4,top);kernel::W32(Rect+12,bottom);}}
};
uint32_t StringAddress(const std::string& text) {
    static uint32_t storage=0;
    static std::unordered_map<std::string,uint32_t> strings;
    if(const auto found=strings.find(text);found!=strings.end())return found->second;
    if(strings.size()>=4096 || text.size()>95)return 0;
    if(!storage)storage=mem::AllocSystem(4096*96);
    if(!storage)return 0;
    const uint32_t address=storage+uint32_t(strings.size())*96;
    std::memset(kernel::HostPtr(address),0,96);
    std::memcpy(kernel::HostPtr(address),text.data(),text.size());
    strings.emplace(text,address);return address;
}
float Measure(X86Context& ctx,uint32_t text) {
    const auto saved=ctx;
    const auto result=kernel::CallGuest(ctx,0x152DC0,{text,2});ctx=saved;
    return float(result);
}
void Text(X86Context& ctx,float x,float y,uint32_t text,uint32_t abgr,float scale) {
    const auto saved=ctx;ctx.ecx=0x66C0E8;
    kernel::CallGuest(ctx,0x1EEF90,{std::bit_cast<uint32_t>(x),std::bit_cast<uint32_t>(y),abgr,text,0,
        std::bit_cast<uint32_t>(scale),std::bit_cast<uint32_t>(scale),0});
    ctx=saved;
}
void Label(X86Context& ctx,float x,float y,uint32_t text,unsigned alpha,float scale,bool title) {
    const auto color=[&](unsigned rgb,float opacity){return (unsigned(std::lround(alpha*opacity))<<24)|rgb;};
    // The guest font supplies the glyphs. Layered low-opacity masks reproduce the menu's soft red halo.
    for(const auto [dx,dy]:{std::pair{-2,0},{2,0},{0,-2},{0,2},{-2,-2},{2,2},{-2,2},{2,-2}})
        Text(ctx,x+dx,y+dy,text,color(0x08107D,.095f),scale);
    Text(ctx,x+2,y+2,text,color(0x000002,.67f),scale);
    for(const auto [dx,dy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}})
        Text(ctx,x+dx,y+dy,text,color(0x010309,.55f),scale);
    Text(ctx,x,y,text,color(title?0x48C2E8:0xFFFFFF,1),scale);
}
void ResizeArrow(Arrow arrow,float tip) {
    const auto index=arrow.index;
    const auto end=kernel::R32(Count);
    if(index+2>end || end>510)return;
    const auto source=Queue+index*Record;
    std::array<uint8_t,Record> original{};
    std::memcpy(original.data(),kernel::HostPtr(source),Record);
    const auto fit=aspect::CanvasScale(aspect::Current());
    const float fitted_left=ReadFloat(source+0x10),left=arrow.left,top=ReadFloat(source+0x14),right=ReadFloat(source+0x30),bottom=ReadFloat(source+0x34);
    const float u0=ReadFloat(source+0x40),u1=ReadFloat(source+0x48),v0=ReadFloat(source+0x4C),v1=ReadFloat(source+0x50);
    if(!std::isfinite(left+fitted_left+top+right+bottom+tip) || right-fitted_left<64*fit.x || bottom<=top || u1<=u0)return;
    const float target=tip?(tip-320)*fit.x+320:right,cap=(right-fitted_left)/8;
    if(target<=left+cap)return;
    const unsigned corners[2][3]={{0,2,3},{0,1,3}};
    // Split the original quad: resize the line, retain the final 64 texture pixels at their original density.
    for(unsigned part=0;part<2;++part)for(unsigned triangle=0;triangle<2;++triangle) {
        const auto record=Queue+(part?end+triangle:index+triangle)*Record;
        viewport_arrows.set(part?end+triangle:index+triangle);
        std::memcpy(kernel::HostPtr(record),original.data(),Record);
        for(unsigned vertex=0;vertex<3;++vertex) {
            const auto corner=corners[triangle][vertex];
            const bool r=corner%2,b=corner/2;
            WriteFloat(record+0x10+vertex*16,part?(r?target:target-cap):(r?target-cap:left));
            WriteFloat(record+0x14+vertex*16,b?bottom:top);
            WriteFloat(record+0x40+vertex*4,part?(r?u1:u0+(u1-u0)*.875f):(r?u0+(u1-u0)*.875f:u0));
            WriteFloat(record+0x4C+vertex*4,b?v1:v0);
        }
    }
    kernel::W32(Count,end+2);
}
}
void BeginFrame(){viewport_arrows.reset();}
std::bitset<512> FrameArrows(){return viewport_arrows;}
void SetFrameArrows(const std::bitset<512>& arrows){viewport_arrows=arrows;}
bool ViewportArrow(uint32_t index){return index<viewport_arrows.size() && viewport_arrows[index];}
void BeginScene(){scenes.emplace_back();}
void MainRowAnchor(uint32_t first,uint32_t end) {
    if(scenes.empty() || first>=end || end>512)return;
    float top=INFINITY;
    for(auto index=first;index<end;++index)for(unsigned v=0;v<3;++v) {
        const float y=ReadFloat(Queue+index*Record+0x14+v*16);
        if(!std::isfinite(y))return;
        top=std::min(top,y);
    }
    scenes.back().main_anchor=true;scenes.back().main_row_top=top;
}
unsigned MainHighlight(uint32_t state,unsigned selection) {
    // Retail states 1..6 select rows; states 7..18 pair each row's exit and return clips.
    if(state>=1 && state<=6)return state-1;
    if(state>=7 && state<=18)return (state-7)/2;
    return std::min(selection,5u);
}
void MainExtraLabel(X86Context& ctx,uint32_t first,uint32_t end,bool selected,bool anchor_selected,unsigned row) {
    if(first>=end || end>512 || row<6 || row>8)return;
    float left=INFINITY,top=INFINITY;unsigned alpha=0;
    for(auto index=first;index<end;++index)for(unsigned v=0;v<3;++v) {
        const auto record=Queue+index*Record;
        const float x=ReadFloat(record+0x10+v*16),y=ReadFloat(record+0x14+v*16);
        if(!std::isfinite(x+y))return;
        left=std::min(left,x);top=std::min(top,y);
        alpha=std::max(alpha,kernel::R32(record+0x58+v*4)>>24);
    }
    const unsigned source=anchor_selected?254:76,target=selected?254:76;
    alpha=std::min(254u,(alpha*target+source/2)/source);
    if(!alpha)return;
    if(!scenes.empty()){scenes.back().extras[row-6]={true,left,top,alpha};return;}
    constexpr std::string_view labels[]={"recomp.title","recomp.enhancements","main.exit_desktop"};
    const auto text=Catalog::Builtin().Get(launcher::Current().language,labels[row-6]);
    if(!text.empty())NativeLabel(ctx,left+3,top+5+28.f*(row-5),text,1.1f,alpha,false,false,510);
}
void DimMenuLabel(uint32_t first,uint32_t end,bool selected) {
    if(!selected || first>=end || end>512)return;
    for(auto index=first;index<end;++index)for(unsigned v=0;v<3;++v) {
        const auto address=Queue+index*Record+0x58+v*4,color=kernel::R32(address);
        kernel::W32(address,(color&0xFFFFFF)|((((color>>24)*76+127)/254)<<24));
    }
}
void NativeButton(X86Context& ctx,int x,int y,Button button) {
    const auto saved=ctx;
    // Retail Options resolves these tokens through the active language table and icon font.
    // The native font maps '<' and '>' to its horizontal direction icons (glyphs 136 and 135).
    const auto text=button==Button::Adjust?StringAddress("<>"):kernel::CallGuest(ctx,0x16992,{button==Button::Accept?0x5B2u:button==Button::Preview?0x5B5u:0x5B3u});
    ctx=saved;
    if(text)kernel::CallGuest(ctx,0x152960,{uint32_t(x),uint32_t(y),0x60,2,text});
    ctx=saved;
}
void MainControllerNotice(X86Context& ctx,uint32_t text,unsigned alpha,float scale) {
    if(!text)return;
    // Retain the retail localized string and fade, fitted above the statue on the main-menu canvas.
    scale=FitScale(Measure(ctx,text),scale,190);
    if(scale>0)Label(ctx,410,184,text,std::min(254u,alpha*2),scale,false);
}
void TitleWidth(float right){if(!scenes.empty())scenes.back().title_right=right;}
float NativeLabel(X86Context& ctx,float x,float y,std::string_view text,float scale,unsigned alpha,bool title,bool compact,float available) {
    const auto address=StringAddress(EncodeNative(text,mods::SameLanguage(launcher::Current().language,"pt-BR")));
    if(!address)return 0;
    const float width=Measure(ctx,address);scale=FitScale(width,scale,available);
    if(scale<=0)return 0;
    const QuestionBounds question(text.find('?')!=std::string_view::npos);
    const AcuteBounds acute(text.find("\xC3\x81")!=std::string_view::npos);
    if(compact) {
        Text(ctx,x+1,y+1,address,(alpha<<24)|0x000002,scale);
        Text(ctx,x,y,address,(alpha<<24)|(title?0x48C2E8:0xFFFFFF),scale);
    } else Label(ctx,x,y,address,alpha,scale,title);
    return width*scale;
}
void EndScene(X86Context& ctx) {
    if(scenes.empty())return;
    const auto state=std::move(scenes.back());scenes.pop_back();
    for(auto arrow:state.arrows)ResizeArrow(arrow,state.title_right?state.title_right+12:0);
    constexpr std::string_view labels[]={"recomp.title","recomp.enhancements","main.exit_desktop"};
    for(unsigned index=0;index<state.extras.size();++index)if(state.extras[index].visible) {
        // Wait for both captions because the native timeline can draw them in either order.
        const auto& extra=state.extras[index];
        const float step=state.main_anchor?extra.top-state.main_row_top:28.f;
        const auto text=Catalog::Builtin().Get(launcher::Current().language,labels[index]);
        if(!text.empty())NativeLabel(ctx,extra.left+3,extra.top+5+step*(index+1),text,1.1f,extra.alpha,false,false,510);
    }
}
bool ReplaceGraphic(X86Context& ctx,uint32_t crc,uint32_t first,uint32_t end) {
    if(first>=end || end>512)return false;
    // Capture the authored origin before the Flash wrapper fits the quad into its 4:3 canvas.
    if(crc==0x92295647 && end==first+2 && !scenes.empty())scenes.back().arrows.push_back({first,ReadFloat(Queue+first*Record+0x10)});
    const auto& catalog=Catalog::Builtin();const auto id=catalog.Graphic(crc);
    if(id.empty())return false;
    const auto& locale=launcher::Current().language;
    const auto text=catalog.Get(locale,id);if(text.empty())return false;
    const auto address=StringAddress(EncodeNative(text,mods::SameLanguage(locale,"pt-BR")));if(!address)return false;
    const float measured=Measure(ctx,address);if(!std::isfinite(measured) || measured<=0)return false;
    float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
    unsigned alpha=0;
    for(auto index=first;index<end;++index)for(unsigned v=0;v<3;++v) {
        const auto record=Queue+index*Record;
        const auto x=ReadFloat(record+0x10+v*16),y=ReadFloat(record+0x14+v*16);
        if(!std::isfinite(x+y))return false;
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
        alpha=std::max(alpha,kernel::R32(record+0x58+v*4)>>24);
    }
    const bool title=id.ends_with(".title");
    float x=left+3,y=top+5,scale=0;
    if(title) {
        const auto layout=LayoutTitle(measured);scale=layout.scale;
        // Retain the original timeline position while sizing the label against the canonical canvas.
        x=left+3;y=top-3;
        scale=FitScale(measured,scale,608-x-12);
        if(!scenes.empty())scenes.back().title_right=x+measured*scale;
    } else {
        // Native values begin at x=328; volume gems begin at x=363 on the 640-pixel canvas.
        const float available=id.starts_with("gameplay.")?318-x:id.starts_with("audio.")?345-x:std::min(420.f,608-x);
        scale=FitScale(measured,1.1f,available);
    }
    if(scale<=0)return false;
    const QuestionBounds question(text.find('?')!=std::string_view::npos);
    const AcuteBounds acute(text.find("\xC3\x81")!=std::string_view::npos);
    if(alpha)Label(ctx,x,y,address,alpha,scale,title);
    kernel::W32(Count,first);return true;
}
}
