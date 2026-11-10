#include "menu_renderer.h"
#include "menu_text.h"
#include "launcher.h"
#include "kernel/kernel.h"
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <tuple>

static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
static float ratio=4.f/3;
struct Draw {std::array<uint32_t,8> args;uint32_t font;};
static std::vector<Draw> draws;
static std::vector<float> question_bounds;
static std::vector<std::array<float,2>> acute_bounds;
struct NativeIcon {std::array<uint32_t,5> args;};
static std::vector<NativeIcon> icons;
static std::vector<uint32_t> icon_ids;
static bool icons_available=true;
namespace kernel {
uint8_t* g_base=memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count) {
    uint32_t result=0;
    if(fn==0x152DC0)result=uint32_t(std::strlen(static_cast<const char*>(HostPtr(args[0]))))*14;
    else if(fn==0x16992 && count==1) {icon_ids.push_back(args[0]);result=icons_available?0x700000+args[0]*4:0;}
    else if(fn==0x152960 && count==5) {NativeIcon icon;std::copy_n(args,5,icon.args.begin());icons.push_back(icon);}
    else if(fn==0x1EEF90 && count==8) {
        Draw draw;std::copy_n(args,8,draw.args.begin());draw.font=ctx.ecx;draws.push_back(draw);
        if(std::strchr(static_cast<const char*>(HostPtr(args[3])),'?')) {
            const auto rect=0x66C0E8+0x6444+70*16;
            question_bounds.push_back(std::bit_cast<float>(R32(rect+12)));
        }
        if(std::strchr(static_cast<const char*>(HostPtr(args[3])),char(0xC1))) {
            const auto rect=0x66C0E8+0x6444+79*16;
            acute_bounds.push_back({std::bit_cast<float>(R32(rect+4)),std::bit_cast<float>(R32(rect+12))});
        }
    } else std::abort();
    ctx.eax=0xDEADBEEF;ctx.ecx=0xBAD;ctx.esp=0xBAD;return result;
}
}
namespace mem {uint32_t AllocSystem(uint32_t,uint32_t){return 0x710000;}}
namespace openshao::launcher {const Settings& Current(){return settings;}}
namespace openshao::aspect {float Current(){return ratio;}}
static void Float(uint32_t a,float value){kernel::W32(a,std::bit_cast<uint32_t>(value));}
static float Read(uint32_t a){return std::bit_cast<float>(kernel::R32(a));}
static void Quad(uint32_t index,float x,float y,float width,float height,unsigned alpha=254) {
    const unsigned corners[2][3]={{0,2,3},{0,1,3}};
    for(unsigned t=0;t<2;++t)for(unsigned v=0;v<3;++v) {
        const auto record=0x63F5E4+(index+t)*0x6C,corner=corners[t][v];
        Float(record+0x10+v*16,x+(corner%2?width:0));Float(record+0x14+v*16,y+(corner/2?height:0));
        Float(record+0x40+v*4,corner%2?1.f:0.f);Float(record+0x4C+v*4,corner/2?1.f:0.f);
        kernel::W32(record+0x58+v*4,(alpha<<24)|0xFFFFFF);
    }
    kernel::W32(0x63F5E0,index+2);
}
int main() {
    using namespace openshao::menu;
    unsigned failures=0;
    auto check=[&](bool ok,const char* reason){if(!ok){++failures;std::cerr<<reason<<'\n';}};
    X86Context ctx{};ctx.esp=0x3000;ctx.eax=123;ctx.ecx=456;
    for(unsigned clip=0;clip<=6;++clip)check(MainBackdropClip(clip),"Main's intro and focus clips keep the lake backdrop");
    for(unsigned clip:{8u,10u,12u,14u,16u,18u})check(MainBackdropClip(clip),"Main's return clips restore the lake backdrop at once");
    for(unsigned clip:{7u,9u,11u,13u,15u,17u,19u})check(!MainBackdropClip(clip),"Main's leave clips keep the native backdrop shutdown");
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"})for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2}) {
        settings.language=locale;ratio=aspect;draws.clear();
        BeginScene();Quad(0,90,176,256,60,76);
        check(ReplaceGraphic(ctx,0xD760EEC1,0,2),"A known main-menu label must be replaced");
        check(kernel::R32(0x63F5E0)==0,"Replacement must remove only its own queued artwork");
        EndScene(ctx);
        check(draws.size()==14 && draws.back().font==0x66C0E8,"Menu text must use the native decorative font");
        check((draws.back().args[2]>>24)==76,"Inactive menu opacity must follow the original animation");
        check(ctx.esp==0x3000 && ctx.eax==123 && ctx.ecx==456,"Font calls must preserve guest register state");
        for(const uint32_t heading:{0xC7AE190Cu,0x64E46E7Eu,0xDA6ACBC4u,0xA7BE574Au,0x36C56E9Du,0x86ABCE30u,0x0D41E9C5u,0u}) {
        BeginScene();draws.clear();Quad(0,52,36,256,60);
        if(heading)check(ReplaceGraphic(ctx,heading,0,2),"Known submenu headings must be replaced");
        else TitleWidth(55+NativeLabel(ctx,55,33,Catalog::Builtin().Get(locale,"recomp.title"),1.4f,254,true));
        check((draws.back().args[2]&0xFFFFFF)==0x48C2E8,"All submenu headings must use the approved gold");
        const float right=std::bit_cast<float>(draws.back().args[0])+std::strlen(static_cast<const char*>(kernel::HostPtr(draws.back().args[3])))*14*std::bit_cast<float>(draws.back().args[5]);
        const auto fit=openshao::aspect::CanvasScale(ratio);
        Quad(0,8,72,512,10);
        check(!ReplaceGraphic(ctx,0x92295647,0,2),"The original arrow texture must be retained");
        for(unsigned i=0;i<2;++i)for(unsigned v=0;v<3;++v) {
            const auto x=0x63F5E4+i*0x6C+0x10+v*16;
            Float(x,(Read(x)+.5f-320)*fit.x+320-.5f);
        }
        EndScene(ctx);
        check(std::abs(Read(0x63F5E4+0x10)-8)<.01f,"The title underline must retain its native left origin instead of floating inside the fitted 4:3 canvas");
        check(kernel::R32(0x63F5E0)==4,"Arrow layout must split the line from the original cap");
        const auto cap=0x63F5E4+2*0x6C;
        check(std::abs(Read(cap+0x30)-((right+12-320)*fit.x+320))<.01f,"Arrow tip must follow the translated title");
        check(std::abs((Read(cap+0x30)-Read(cap+0x10))-64*fit.x)<.01f,"Arrow cap proportions must remain unchanged");
        check(std::abs(Read(cap+0x40)-.875f)<.001f,"Arrow cap must retain the final 64 source pixels");
        check(ViewportArrow(0) && ViewportArrow(1) && ViewportArrow(2) && ViewportArrow(3) && !ViewportArrow(4) && !ViewportArrow(512),
              "Only the underline's four triangles must bypass the fitted canvas clip");
        BeginFrame();
        check(!ViewportArrow(0) && !ViewportArrow(3),"Arrow clipping provenance must not survive a queue flush");
        }
    }
    Quad(0,90,176,256,60);
    check(!ReplaceGraphic(ctx,0x12345678,0,2) && kernel::R32(0x63F5E0)==2,"Unknown sprites must remain intact");
    const auto question_rect=0x66C0E8+0x6444+70*16;
    kernel::W32(0x66C0E8+0x6424,256);kernel::W32(0x66C0E8+0x6428,512);
    Float(question_rect,210.f/256);Float(question_rect+4,106.f/512);
    Float(question_rect+8,220.f/256);Float(question_rect+12,126.f/512);
    question_bounds.clear();NativeLabel(ctx,55,453,"Keep these settings? 12",.72f,254,false,true,550);
    check(!question_bounds.empty() && std::all_of(question_bounds.begin(),question_bounds.end(),[](float bottom){return std::abs(bottom-124.5f/512)<.00001f;}),
          "Question marks must sample through their final dot without sampling the next atlas row at y=125");
    check(Read(question_rect+12)==126.f/512,"Rendering a port label must restore the original font metadata for other guest callers");
    const auto acute_rect=0x66C0E8+0x6444+79*16;
    Float(acute_rect,147.f/256);Float(acute_rect+4,127.f/512);
    Float(acute_rect+8,165.f/256);Float(acute_rect+12,147.f/512);
    settings.language="pt-BR";acute_bounds.clear();
    NativeLabel(ctx,93,377,"Sair para a \xC3\x81rea de Trabalho",1.1f,254);
    check(acute_bounds.size()==14 && std::all_of(acute_bounds.begin(),acute_bounds.end(),[](const auto& bounds){
              return bounds[0]==125.5f/512 && bounds[1]==145.5f/512;
          }),"Uppercase acute A must include the accent at atlas row 125 and stop before the next glyph row at 147");
    check(Read(acute_rect+4)==127.f/512 && Read(acute_rect+12)==147.f/512,
          "The acute A correction must restore the original font metadata after all halo and foreground draws");
    Float(acute_rect+4,126.f/512);acute_bounds.clear();
    NativeLabel(ctx,93,377,"\xC3\x81",1.1f,254);
    check(!acute_bounds.empty() && acute_bounds.back()[0]==126.f/512 && acute_bounds.back()[1]==147.f/512,
          "A different font layout must retain its own accented glyph bounds");
    Float(acute_rect+4,127.f/512);
    settings.language="pt-br";draws.clear();
    NativeLabel(ctx,55,453,"N\xC3\xA3o",.72f,254,false,true,550);
    check(!draws.empty() && std::string(static_cast<const char*>(kernel::HostPtr(draws.back().args[3])))==std::string("N\xE4o"),
          "Lowercase pt-br configuration must retain the native Brazilian Portuguese tilde mapping");
    settings.language="fr";
    for(const auto [crc,limit]:{std::pair{0x30F2B293u,318.f},std::pair{0xF7900E18u,345.f}}) {
        draws.clear();Quad(0,90,176,256,60);
        check(ReplaceGraphic(ctx,crc,0,2),"Long localized settings captions must be rendered");
        const auto& draw=draws.back();
        const float edge=std::bit_cast<float>(draw.args[0])+std::strlen(static_cast<const char*>(kernel::HostPtr(draw.args[3])))*14*std::bit_cast<float>(draw.args[5]);
        check(edge<=limit+.01f,"Settings captions must leave a clear gap before values and volume gems");
    }
    for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2}) {
        ratio=aspect;const auto fit=openshao::aspect::CanvasScale(ratio);
        BeginFrame();BeginScene();Quad(0,8,72,512,10);ReplaceGraphic(ctx,0x92295647,0,2);
        for(unsigned i=0;i<2;++i)for(unsigned v=0;v<3;++v) {
            const auto x=0x63F5E4+i*0x6C+0x10+v*16;
            Float(x,(Read(x)+.5f-320)*fit.x+320-.5f);
        }
        EndScene(ctx);
        check(kernel::R32(0x63F5E0)==4 && Read(0x63F5E4+0x10)==8 &&
              std::abs(Read(0x63F5E4+2*0x6C+0x30)-((520+.5f-320)*fit.x+320-.5f))<.01f,
              "Native headings outside the text catalog must retain their fitted tip while using the same viewport underline origin");
    }
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"})for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2}) {
        settings.language=locale;ratio=aspect;icons.clear();icon_ids.clear();
        NativeButton(ctx,70,410,Button::Back);NativeButton(ctx,450,410,Button::Accept);
        check(icon_ids==std::vector<uint32_t>{0x5B3,0x5B2},"Footer prompts must resolve the original B and A icon resources");
        check(icons.size()==2 && icons[0].args==std::array<uint32_t,5>{70,410,0x60,2,0x700000+0x5B3*4} && icons[1].args==std::array<uint32_t,5>{450,410,0x60,2,0x700000+0x5B2*4},"Button prompts must use the retail icon draw path and preserve original size and tint");
        check(ctx.esp==0x3000 && ctx.eax==123 && ctx.ecx==456,"Resolving and drawing native icons must preserve the guest context");
    }
    icons.clear();icon_ids.clear();NativeButton(ctx,450,410,Button::Preview);
    check(icon_ids==std::vector<uint32_t>{0x5B5} && icons.size()==1,
          "Voice previews must draw the original X icon through its localized native token");
    icons_available=false;icons.clear();NativeButton(ctx,70,410,Button::Back);
    check(icons.empty(),"An unavailable icon resource must not be passed to the guest renderer");
    icons_available=true;icons.clear();icon_ids.clear();NativeButton(ctx,250,410,Button::Adjust);
    check(icon_ids.empty() && icons.size()==1 && icons[0].args[0]==250 && icons[0].args[1]==410 && icons[0].args[2]==0x60 && icons[0].args[3]==2 &&
          std::string(static_cast<const char*>(kernel::HostPtr(icons[0].args[4])))=="<>",
        "The adjustment hint must draw the exact native gallery direction glyph through the icon font instead of keyboard direction words");
    check(MainHighlight(6,6)==5 && MainHighlight(15,6)==4 && MainHighlight(17,5)==5 && MainHighlight(8,0)==0 && MainHighlight(19,6)==5,
        "Main-menu clip states must identify the authored highlight during selection, exit, return and attract transitions");
    for(const auto* locale:openshao::launcher::languages) {
        settings.language=locale;draws.clear();Quad(0,90,232,256,60,254);
        MainExtraLabel(ctx,0,2,true,true,9);
        check(!draws.empty(),"Game Mode must draw a localized entry from the Versus animation anchor");
        if(!draws.empty())check(std::abs(std::bit_cast<float>(draws.back().args[1])-225)<.01f &&
            std::string(static_cast<const char*>(kernel::HostPtr(draws.back().args[3]))).size()>0,
            "Game Mode must occupy the raised fourth row without clipping its native text");
    }
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"})for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2}) {
        settings.language=locale;ratio=aspect;
        for(const auto [source,selected,anchor,expected]:{std::tuple{38u,false,false,38u},std::tuple{127u,false,true,38u},std::tuple{38u,true,false,127u},std::tuple{127u,true,true,127u}}) {
            draws.clear();Quad(0,-110,88,256,60,source);MainExtraLabel(ctx,0,2,selected,anchor);
            check(!draws.empty(),"The extra menu row must draw from the current native Kontent frame");
            if(!draws.empty()) {
                const auto& draw=draws.back();
                check(std::abs(std::bit_cast<float>(draw.args[0])+107)<.01f && std::abs(std::bit_cast<float>(draw.args[1])-85)<.01f,
                    "The extra row must inherit timeline movement and remain one row below Kontent");
                check((draw.args[2]>>24)==expected,"The extra row must inherit transition opacity while retaining its own highlight");
            }
            check(kernel::R32(0x63F5E0)==2 && ctx.esp==0x3000 && ctx.eax==123 && ctx.ecx==456,
                "Following the native animation must preserve the source artwork queue and guest context");
        }
        draws.clear();Quad(0,90,316,256,60,0);MainExtraLabel(ctx,0,2,true,false);
        check(draws.empty(),"The extra row must disappear when its native animation anchor is transparent");
    }
    Quad(0,90,316,256,60,127);DimMenuLabel(0,2,true);
    check((kernel::R32(0x63F5E4+0x58)>>24)==38 && (kernel::R32(0x63F5E4+0x58)&0xFFFFFF)==0xFFFFFF,
        "Removing the borrowed native highlight must preserve the transition fade and RGB");
    Quad(0,90,316,256,60,38);DimMenuLabel(0,2,false);
    check((kernel::R32(0x63F5E4+0x58)>>24)==38,"Inactive native labels must retain their animation opacity");
    draws.clear();MainExtraLabel(ctx,2,2,true,false);MainExtraLabel(ctx,0,513,true,false);
    check(draws.empty(),"Missing or invalid native frames must not produce a floating extra label");
    BeginScene();Quad(0,110,72,256,20,76);MainRowAnchor(0,2);
    draws.clear();Quad(0,110,80,256,20,76);MainExtraLabel(ctx,0,2,false,false);EndScene(ctx);
    check(!draws.empty() && std::abs(std::bit_cast<float>(draws.back().args[1])-37)<.01f,
        "The extra row must follow compressed native line spacing during the transition");
    BeginScene();draws.clear();Quad(0,110,80,256,20,76);MainExtraLabel(ctx,0,2,false,false);
    Quad(0,110,72,256,20,76);MainRowAnchor(0,2);EndScene(ctx);
    check(!draws.empty() && std::abs(std::bit_cast<float>(draws.back().args[1])-37)<.01f,
        "Animated row spacing must work when Kontent is drawn before its neighboring caption");
    BeginScene();draws.clear();Quad(0,90,316,256,60,76);MainExtraLabel(ctx,0,2,false,false);EndScene(ctx);
    check(!draws.empty() && std::abs(std::bit_cast<float>(draws.back().args[1])-313)<.01f,
        "Animation anchors must not leak into the next scene or steady menu layout");
    for(const auto* locale:openshao::launcher::languages)for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2})for(const float step:{8.f,28.f}) {
        settings.language=locale;ratio=aspect;
        BeginScene();Quad(0,90,316-step,256,60,76);MainRowAnchor(0,2);
        draws.clear();Quad(0,90,316,256,60,76);
        MainExtraLabel(ctx,0,2,false,false,6);MainExtraLabel(ctx,0,2,false,false,7);MainExtraLabel(ctx,0,2,true,false,8);EndScene(ctx);
        check(draws.size()==42 && (draws[13].args[2]>>24)==76 && (draws[27].args[2]>>24)==76 && (draws.back().args[2]>>24)==254,
              "Recompilation settings, Enhancements and Exit must keep independent highlights at every aspect ratio");
        if(!draws.empty()) {
            const auto& exit=draws.back();
            check(std::abs(std::bit_cast<float>(draws[27].args[1])-(251+3*step))<.01f &&
                  std::string(static_cast<const char*>(kernel::HostPtr(draws[27].args[3])))==EncodeNative(Catalog::Builtin().Get(locale,"recomp.enhancements"),std::string_view(locale)=="pt-BR"),
                  "Enhancements must follow the native spacing and localized text");
            check(std::abs(std::bit_cast<float>(exit.args[1])-(245+4*step))<.01f &&
                  std::string(static_cast<const char*>(kernel::HostPtr(exit.args[3])))==EncodeNative(Catalog::Builtin().Get(locale,"main.exit_desktop"),std::string_view(locale)=="pt-BR"),
                  "Exit must follow the native spacing and localized text, including compressed entrance and return frames");
        }
    }
    for(const auto* locale:openshao::launcher::languages)for(const float aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2}) {
        settings.language=locale;ratio=aspect;draws.clear();
        const char notice[]="A long localized two-controller notice \xE9";
        std::memcpy(kernel::HostPtr(0x720000),notice,sizeof(notice));
        MainControllerNotice(ctx,0x720000,60,.8f);
        check(draws.size()==14,"The controller notice must retain its native caption style");
        if(!draws.empty()) {
            const auto& draw=draws.back();
            const float x=std::bit_cast<float>(draw.args[0]),y=std::bit_cast<float>(draw.args[1]);
            const float edge=x+std::strlen(notice)*14*std::bit_cast<float>(draw.args[5]);
            check(x==410 && y==184 && edge<=600.01f && (draw.args[2]>>24)==120 && draw.args[3]==0x720000,
                  "Localized controller notices must fit the marked area above the statue, preserving native glyphs and opacity");
        }
        check(ctx.esp==0x3000 && ctx.eax==123 && ctx.ecx==456,"Rendering the controller notice must preserve guest registers");
    }
    draws.clear();MainControllerNotice(ctx,0,60,.8f);
    check(draws.empty(),"An unavailable native controller caption must not be rendered");
    return failures?1:0;
}
