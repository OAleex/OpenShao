#include "aspect_ratio.h"
#include "launcher.h"
#include "menu_renderer.h"
#include "survival_mode.h"
#include "app/app.h"
#include "core/log.h"
#include <array>
#include <bit>
#include <bitset>
#include <cstring>
#include <cstdio>
#include <cstdlib>
struct X86Context {uint32_t esp=0x1000,eax=0,ecx=0,guard=0;};
namespace openshao::menu {
static uint32_t viewport_arrow=512;
static uint32_t controller_notice=0;
static unsigned notice_alpha=0;
static float notice_scale=0;
void MainControllerNotice(X86Context&,uint32_t text,unsigned alpha,float scale){controller_notice=text;notice_alpha=alpha;notice_scale=scale;}
void BeginFrame(){viewport_arrow=512;}
bool ViewportArrow(uint32_t index){return index==viewport_arrow;}
std::bitset<512> FrameArrows(){std::bitset<512> arrows;if(viewport_arrow<512)arrows.set(viewport_arrow);return arrows;}
void SetFrameArrows(const std::bitset<512>& arrows){viewport_arrow=512;for(uint32_t i=0;i<512;++i)if(arrows[i]){viewport_arrow=i;break;}}
void BeginScene(){}
void EndScene(X86Context&){}
bool ReplaceGraphic(X86Context&,uint32_t,uint32_t,uint32_t){return false;}
}
namespace openshao::recomp {
static bool active=false,extra_selected=false;
bool Active(){return active;}
bool ExtraSelected(){return extra_selected;}
unsigned Selection(){return 0;}
unsigned SelectorY(){return 188;}
bool Graphic(X86Context&,uint32_t,uint32_t,uint32_t,uint32_t){return false;}
}
namespace openshao::survival {
static bool medallion_menu=false;
static bool setup_popup=false;
static bool transitioning=false;
static bool character_selection=false;
static uint32_t player_mode=0;
bool MedallionMenu(){return medallion_menu;}
bool Transitioning(){return transitioning;}
bool SetupPopup(){return setup_popup;}
bool CharacterSelection(){return character_selection;}
uint32_t PlayerMode(){return player_mode;}
}
static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
namespace openshao::launcher {const Settings& Current(){return settings;}}
namespace app {Mode Output(){return {1920,1080};}}
namespace logx {void Write(Level,const char*,...) {}}
namespace kernel {
uint8_t* HostPtr(uint32_t address){return memory.data()+address;}
uint32_t R32(uint32_t address){uint32_t value;std::memcpy(&value,HostPtr(address),4);return value;}
void W32(uint32_t address,uint32_t value){std::memcpy(HostPtr(address),&value,4);}
void Return(X86Context& ctx,uint32_t value,int args){ctx.eax=value;ctx.esp+=4+args*4;}
}
#define X86_FUNC(name) void name(X86Context& ctx,uint8_t* base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
static unsigned sprites=0;
static bool expanded_red=false;
static bool tutorial_drawing=false;
static bool pause_drawing=false;
static bool cinematic_drawing=false;
static bool health_drawing=false;
static bool might_drawing=false;
static bool might_right=false;
static bool artwork_drawing=false,artwork_clipped=false;
static uint32_t queued_text_y=0;
static float queued_text_scale=0;
static std::array<float,8> tutorial_drawn{};
static uint32_t gui_sprite_result=0x5100;
static float prompt_drawn_x=0;
static float prompt_drawn_y=0;
static bool prompt_clipped=false;
static bool queue_fixture=false;
static float sprite_vertices[4]={-.5f,-.5f,639.5f,479.5f};
static void OriginalResult(X86Context& ctx,unsigned args=0){ctx.eax=0x1234;ctx.ecx=0x5678;ctx.guard=0x9876;ctx.esp+=4+args*4;}
X86_FUNC(__imp__sub_0011B290){kernel::W32(0x2020,std::bit_cast<uint32_t>(1.f));OriginalResult(ctx);}
X86_FUNC(__imp__sub_0015AFA0){OriginalResult(ctx);}
X86_FUNC(__imp__sub_0011D000){
    struct V {float x,y,z;uint32_t color,uv;};
    V fill[4]={{-320.5f,-240.5f,120,0x407F0000,0},{-320.5f,239.5f,120,0x407F0000,0},
               {319.5f,-240.5f,120,0x407F0000,0},{319.5f,239.5f,120,0x407F0000,0}};
    if(tutorial_drawing)for(unsigned i=0;i<4;++i) {
        const bool portrait=openshao::aspect::tutorial_sprite==openshao::aspect::TutorialSprite::Portrait;
        fill[i].x=i<2?(portrait?112.f:-320.5f):(portrait?351.f:111.5f);
        fill[i].y=i%2?(portrait?240.5f:239.5f):(portrait?70.5f:116.5f);fill[i].color=0x7F004488;
    }
    if(pause_drawing)for(unsigned i=0;i<4;++i) {
        fill[i].x=i<2?-280.5f:279.5f;fill[i].y=i%2?149.5f:-150.5f;
        fill[i].color=0x7F004488;fill[i].uv=0x12345678;
    }
    if(health_drawing)for(unsigned i=0;i<4;++i) {
        fill[i].x=i<2?-290.5f:109.5f;fill[i].y=i%2?-150.5f:-210.5f;
        fill[i].color=0x7F808080;fill[i].uv=0x12345678;
    }
    if(might_drawing)for(unsigned i=0;i<4;++i) {
        fill[i].x=(might_right?230.f:-245.f)+(i<2?-25.5f:25.5f);
        fill[i].y=i%2?160.5f:-160.5f;
        fill[i].color=0x7F808080;fill[i].uv=0x12345678;
    }
    if(cinematic_drawing)for(unsigned i=0;i<4;++i) {
        fill[i].y=i%2?-168.5f:-240.5f;fill[i].color=0x7F000000;
    }
    if(artwork_drawing)for(unsigned i=0;i<4;++i) {
        fill[i].x=i<2?-288.f:300.f;fill[i].y=i%2?240.f:-240.f;fill[i].color=0x7F808080;
    }
    expanded_red=openshao::aspect::ExpandViewportTint(fill,4,20);
    if(artwork_drawing) {
        struct State {uint32_t scissor_count=0;std::array<uint32_t,32> scissors{};} state;
        openshao::aspect::WithCanvasClip(state,[&]{artwork_clipped=state.scissor_count!=0;});
    }
    for(unsigned i=0;i<4;++i){tutorial_drawn[i*2]=fill[i].x;tutorial_drawn[i*2+1]=fill[i].y;}
    ++sprites;OriginalResult(ctx,1);
}
X86_FUNC(__imp__sub_00016866){OriginalResult(ctx,13);ctx.eax=gui_sprite_result;}
X86_FUNC(__imp__sub_000165C3){OriginalResult(ctx,1);}
X86_FUNC(__imp__sub_000152A9){OriginalResult(ctx,1);}
X86_FUNC(sub_001EE580);
static void QueueFixture() {
    X86Context enqueue;enqueue.esp=0x1800;enqueue.ecx=0x7800;
    kernel::W32(enqueue.esp+4,0x7A00);sub_001EE580(enqueue,nullptr);
}
X86_FUNC(__imp__sub_00014A2A){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_00015321){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_00013DB4){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_00012E14){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_00011DD4){QueueFixture();OriginalResult(ctx);}
static bool combo_bonus_fixture=false;
X86_FUNC(sub_00152CE0);
X86_FUNC(__imp__sub_000133A0){
    if(combo_bonus_fixture) {
        X86Context popup;kernel::W32(popup.esp,0x7C480);sub_00152CE0(popup,nullptr);
    } else QueueFixture();
    OriginalResult(ctx);
}
X86_FUNC(__imp__sub_000149FD){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_00016267){QueueFixture();OriginalResult(ctx);}
X86_FUNC(__imp__sub_000124C3){
    kernel::W32(0x7A00,std::bit_cast<uint32_t>(float(int32_t(kernel::R32(ctx.esp+4)))));
    kernel::W32(0x7A04,std::bit_cast<uint32_t>(float(int32_t(kernel::R32(ctx.esp+8)))));
    QueueFixture();QueueFixture();OriginalResult(ctx);
}
X86_FUNC(__imp__sub_00152CE0){
    queued_text_y=kernel::R32(ctx.esp+8);
    queued_text_scale=std::bit_cast<float>(kernel::R32(ctx.esp+28));
    if(queue_fixture) {
        kernel::W32(0x7A00,std::bit_cast<uint32_t>(float(int32_t(kernel::R32(ctx.esp+4)))));
        kernel::W32(0x7A04,std::bit_cast<uint32_t>(float(int32_t(kernel::R32(ctx.esp+8)))));
        kernel::W32(0x7A34,kernel::R32(ctx.esp+20));
    }
    QueueFixture();OriginalResult(ctx);
}
X86_FUNC(__imp__sub_000123BA){QueueFixture();OriginalResult(ctx,4);}
X86_FUNC(__imp__sub_001EE580){
    const auto queue=ctx.ecx,index=kernel::R32(queue),command=kernel::R32(ctx.esp+4);
    std::memcpy(kernel::HostPtr(queue+8+index*0x54),kernel::HostPtr(command),0x54);
    kernel::W32(queue,index+1);OriginalResult(ctx,1);ctx.eax=1;
}
X86_FUNC(__imp__sub_001EEC70){
    const auto command=kernel::R32(ctx.esp+4),index=kernel::R32(ctx.esp+8);
    std::memcpy(kernel::HostPtr(command),kernel::HostPtr(ctx.ecx+8+index*0x54),0x54);
    OriginalResult(ctx,2);ctx.eax=command;
}
X86_FUNC(__imp__sub_00152AE0){
    queued_text_y=kernel::R32(ctx.esp+8);
    if(queue_fixture) {
        kernel::W32(0x7A00,std::bit_cast<uint32_t>(float(kernel::R32(ctx.esp+4))));
        kernel::W32(0x7A04,std::bit_cast<uint32_t>(float(queued_text_y)));
        kernel::W32(0x7A34,kernel::R32(ctx.esp+20));QueueFixture();
    }
    OriginalResult(ctx);
}
X86_FUNC(__imp__sub_001EF210){
    // The retail font clamps authored X to 40 before it emits its glyph quad.
    // Anchor the submitted quad after that clamp, so ultrawide positions survive.
    const auto command=kernel::R32(ctx.esp+4);
    const float x=std::max(40.f,std::bit_cast<float>(kernel::R32(command)))-320;
    const float y=std::bit_cast<float>(kernel::R32(command+4))-240;
    struct Glyph {float x,y,z;uint32_t color,uv;};
    Glyph glyph[4]={{x,y,0,0xFFFFFFFF,0},{x,y+16,0,0xFFFFFFFF,0},{x+10,y,0,0xFFFFFFFF,0},{x+10,y+16,0,0xFFFFFFFF,0}};
    openshao::aspect::ExpandViewportTint(glyph,4,20);
    prompt_drawn_x=glyph[0].x+320;prompt_drawn_y=glyph[0].y+240;
    struct State {uint32_t scissor_count=0;std::array<uint32_t,32> scissors{};} state;
    openshao::aspect::WithCanvasClip(state,[&]{prompt_clipped=state.scissor_count!=0;});
    OriginalResult(ctx,1);
}
X86_FUNC(__imp__sub_0011D140){OriginalResult(ctx);}
X86_FUNC(__imp__sub_0012F930){OriginalResult(ctx);}
X86_FUNC(__imp__sub_0013FD70){OriginalResult(ctx);}
static std::array<float,4> rectangle_drawn{};
X86_FUNC(__imp__sub_0012FDE0){std::memcpy(rectangle_drawn.data(),kernel::HostPtr(ctx.ecx+4),16);OriginalResult(ctx);}
X86_FUNC(sub_0013FFB0);
static bool modal_mesh=false;
static bool gallery_mesh=false;
static uint32_t selector_argument_y=0;
static unsigned selector_draws=0;
static unsigned auxiliary_glint_draws=0;
static unsigned courtyard_draws=0;
X86_FUNC(__imp__sub_001B2C40){
    if(ctx.ecx==0x592D20){selector_argument_y=kernel::R32(ctx.esp+20);++selector_draws;}
    if(ctx.ecx==0x5929D8)++auxiliary_glint_draws;
    if(ctx.ecx==0x58E4F0)++courtyard_draws;
    if(modal_mesh) {
        const float rectangles[5][4]={{-.1f,.03f,645,146},{-.1f,326,645,478.1f},
                                      {-.15f,146,80,326},{560,146,644.99f,326},{80,146,560,326}};
        for(unsigned r=0;r<5;++r)for(unsigned triangle=0;triangle<2;++triangle) {
            const auto index=kernel::R32(0x63F5E0),record=0x63F5E4+index*0x6C;
            const auto& b=rectangles[r];
            const float xy[2][6]={{b[0],b[1],b[0],b[3],b[2],b[3]},
                                  {b[0],b[1],b[2],b[1],b[2],b[3]}};
            for(unsigned v=0;v<3;++v) {
                for(unsigned axis=0;axis<2;++axis)kernel::W32(record+0x10+v*16+axis*4,std::bit_cast<uint32_t>(xy[triangle][v*2+axis]));
                kernel::W32(record+0x18+v*16,std::bit_cast<uint32_t>(.05f));
                kernel::W32(record+0x58+v*4,r==4?0xB2042084:0xB2000000);
            }
            kernel::W32(record+0x68,0);kernel::W32(0x63F5E0,index+1);
            openshao::aspect::CanvasScope canvas;
            openshao::aspect::CorrectUiVertices(kernel::HostPtr(record+0x10),3,16);
            // The real textured-triangle wrapper records this provenance.
        }
        OriginalResult(ctx,6);return;
    }
    const auto saved=ctx;sub_0013FFB0(ctx,base);ctx=saved;OriginalResult(ctx,6);
}
static bool polygon_clipped=false,polygon_scissored=false;
X86_FUNC(__imp__sub_0013F980){
    polygon_clipped=openshao::aspect::canvas_depth!=0;
    struct State {uint32_t scissor_count=0;std::array<uint32_t,32> scissors{};} state;
    openshao::aspect::WithCanvasClip(state,[&]{polygon_scissored=state.scissor_count!=0;});
    OriginalResult(ctx);
}
X86_FUNC(__imp__sub_00100B44){
    const auto matrix=kernel::R32(ctx.esp+4);
    for(unsigned i=0;i<16;++i)kernel::W32(matrix+i*4,std::bit_cast<uint32_t>(i==0?2.f/640:i==5?2.f/480:float(i)));
    OriginalResult(ctx,5);
}
static float mesh_top=-.5f;
static bool gold_order_mesh=false;
X86_FUNC(__imp__sub_0013FFB0){
    const unsigned index=kernel::R32(0x63F5E0);
    if(gallery_mesh) {
        const unsigned corners[2][3]={{0,2,3},{0,1,3}};
        for(unsigned t=0;t<2;++t) {
            const auto record=0x63F5E4+(index+t)*0x6C;
            std::memset(kernel::HostPtr(record),0,0x6C);
            kernel::W32(record,gold_order_mesh?kernel::R32(0x7B4C):0x01010101);
            kernel::W32(record+0x68,0x81870000);
            for(unsigned v=0;v<3;++v) {
                const unsigned c=corners[t][v];
                const float x=c%2?639.5f:-.5f,y=c/2?479.5f:mesh_top;
                kernel::W32(record+0x10+v*16,std::bit_cast<uint32_t>(x));
                kernel::W32(record+0x14+v*16,std::bit_cast<uint32_t>(y));
                kernel::W32(record+0x18+v*16,std::bit_cast<uint32_t>(.75f));
                kernel::W32(record+0x40+v*4,std::bit_cast<uint32_t>(float(c%2)));
                kernel::W32(record+0x4C+v*4,std::bit_cast<uint32_t>(float(c/2)));
                kernel::W32(record+0x58+v*4,0xFF7F7F7F);
            }
        }
        kernel::W32(0x63F5E0,index+2);OriginalResult(ctx);return;
    }
    const float positions[12]={-.5f,-.5f,7,8,639.5f,-.5f,9,10,-.5f,479.5f,11,12};
    std::memcpy(kernel::HostPtr(0x63F5E4+index*0x6C+0x10),positions,sizeof(positions));
    kernel::W32(0x63F5E0,index+1);OriginalResult(ctx);
}
X86_FUNC(__imp__sub_00141BF0){
    const auto quad=kernel::R32(ctx.esp+4);
    for(unsigned i=0;i<24;++i)kernel::W32(quad+i*4,0xCD000000+i);
    OriginalResult(ctx);
}
#include "difficulty.h"
namespace openshao::difficulty {bool SaveListText(X86Context&,uint8_t*,TextCall,void(*)(X86Context&,uint8_t*)){return false;}
void KillPanel(X86Context& ctx,uint8_t* base,void(*original)(X86Context&,uint8_t*)){original(ctx,base);}}
#include "aspect_hooks.inl"
X86_FUNC(sub_0012FDE0);
X86_FUNC(sub_001B2C40);
X86_FUNC(sub_00016866);
X86_FUNC(sub_000165C3);
X86_FUNC(sub_00152AE0);
X86_FUNC(sub_001EF210);
static void Check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static bool Near(float a,float b){return std::abs(a-b)<.0001f;}
static float F32(uint32_t address){return std::bit_cast<float>(kernel::R32(address));}
int main(){
    // A retail concept viewer has a 588x480 image and a separate footer.
    // Project the submitted vertices to pixels rather than duplicating its fit helper.
    kernel::W32(0x59847C,0x7200);kernel::W32(0x74E4,16);
    kernel::W32(0x5985E4,0);kernel::W32(0x5984B0,0x5000);
    kernel::W32(0x4008,0x5000);artwork_drawing=true;
    kernel::W32(0x5000+0x1D8,0x7000);
    kernel::W32(0x7004,588u|(480u<<16));
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;
        if(settings.aspect_ratio=="auto")openshao::aspect::Output(640,960);
        const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
        X86Context art;kernel::W32(art.esp+4,0x4000);
        {openshao::aspect::CanvasScope canvas;sub_0011D000(art,nullptr);}
        const float left=tutorial_drawn[0]*scale.x+320,right=tutorial_drawn[4]*scale.x+320;
        const float top=tutorial_drawn[1]*scale.y+240,bottom=tutorial_drawn[3]*scale.y+240;
        Check(Near(left+right,640) && left>=0 && right<=640 && top>=0 && bottom<=480-64*scale.y,
              "opened artwork is centered and contained above its separate footer in every aspect mode");
        const float pixel_ratio=(right-left)*openshao::aspect::Current()/(4.f/3)/(bottom-top);
        Check(Near(pixel_ratio,588.f/480) && !artwork_clipped,
              "artwork retains its authored display proportions without the old canvas clip");
        Check(art.esp==0x1008 && art.eax==0x1234,"artwork rendering preserves the native return ABI");
        for(const unsigned native_y:{255u,277u}) {
            X86Context title;
            kernel::W32(title.esp,0x4C8BE);kernel::W32(title.esp+8,native_y);
            kernel::W32(title.esp+24,std::bit_cast<uint32_t>(.9f));
            kernel::W32(title.esp+28,std::bit_cast<uint32_t>(.9f));
            sub_00152CE0(title,nullptr);
            const float image_scale=(tutorial_drawn[3]-tutorial_drawn[1])/480;
            const float projected_y=(float(queued_text_y)-240)*scale.y+240;
            Check(std::abs(projected_y-(top+(bottom-top)*float(native_y)/480))<=.51f &&
                  Near(queued_text_scale,.9f*image_scale),
                  "the localized technique title follows the fitted artwork and its baked commands");
            Check(kernel::R32(0x1008)==native_y && F32(0x101C)==.9f,
                  "the technique hook restores native text arguments");
        }
    }
    for(const auto [caller,expected]:{std::pair{0x4C822u,448u},std::pair{0x4C857u,446u},
                                      std::pair{0x4C942u,448u},std::pair{0x4C973u,446u}}) {
        X86Context text;kernel::W32(text.esp,caller);kernel::W32(text.esp+4,50);kernel::W32(text.esp+8,413);
        kernel::W32(text.esp+20,0x7800);kernel::W32(0x7800,0x00420041);
        sub_00152AE0(text,nullptr);
        Check(queued_text_y==expected+240 && kernel::R32(0x1008)==413,
              "viewer footer moves both text layers and restores native caller arguments");
    }
    kernel::W32(0x7900,std::bit_cast<uint32_t>(50.f));kernel::W32(0x7904,std::bit_cast<uint32_t>(686.f));
    kernel::W32(0x7934,0x7800);X86Context footer;kernel::W32(footer.esp+4,0x7900);
    {openshao::aspect::CanvasScope canvas;sub_001EF210(footer,nullptr);}
    Check(!prompt_clipped,"deferred artwork footer stays visible below the fitted canvas in portrait mode");
    kernel::W32(0x63F5E0,0);X86Context arrows;arrows.ecx=0x58EB80;
    sub_001B2C40(arrows,nullptr);
    Check(kernel::R32(0x63F5E0)==0,"thumbnail page arrows do not show behind the open artwork");
    kernel::W32(0x74E4,9);X86Context unrelated;kernel::W32(unrelated.esp+4,0x4000);
    sub_0011D000(unrelated,nullptr);
    Check(tutorial_drawn[0]==-288 && tutorial_drawn[4]==300,
          "the same native image owner keeps its original geometry outside the open viewer");
    artwork_drawing=false;kernel::W32(0x5984B0,0);kernel::W32(0x59847C,0);sprites=0;
    // The retail campaign bar callback identifies the complete health widget,
    // independently of its texture, color, current fill and animation position.
    settings.aspect_ratio="16:9";health_drawing=true;
    kernel::W32(0x4008,0x5000);kernel::W32(0x5000+0x1FC,0x12EF5);
    X86Context health;kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(expanded_red && Near(tutorial_drawn[0],-290.5f-320.f/3) &&
          Near(tutorial_drawn[4]-tutorial_drawn[0],400) && Near(tutorial_drawn[1],-210.5f),
          "campaign health stays at the viewport left edge without changing widget proportions");
    settings.aspect_ratio="auto";openshao::aspect::Output(1000,1500);
    health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(expanded_red && Near(tutorial_drawn[1],-450.5f),
          "campaign health stays at the top of a tall Automatic viewport");
    // Ko-op: each player's pieces follow their own corner; a quad crossing the
    // center widens by the same amount on both sides (21:9 spreads by 240).
    settings.aspect_ratio="21:9";kernel::W32(0x5ACEBC,0x790000);kernel::W32(0x790088,2);
    kernel::W32(0x5000+0x1FC,0x12FFE);
    health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(!expanded_red && Near(tutorial_drawn[0],-290.5f),"Versus keeps its native HUD composition");
    kernel::W32(0x790088,1);
    for(const auto callback:{0x12FFEu,0x13ED6u,0x127E3u,0x15AFBu,0x11AB4u}) {
        kernel::W32(0x5000+0x1FC,callback);
        health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
        Check(expanded_red && Near(tutorial_drawn[0],-290.5f-240) && Near(tutorial_drawn[4],109.5f+240) &&
              Near(tutorial_drawn[1],-210.5f),"Ko-op pieces split toward their own top corners");
    }
    kernel::W32(0x5000+0x1FC,0x12E14);
    health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(expanded_red && Near(tutorial_drawn[0],-290.5f-240) && Near(tutorial_drawn[4],109.5f+240*109.5f/213) &&
          Near(tutorial_drawn[1],-210.5f),"the shared Ko-op bar stretches linearly so its fill stays proportional");
    settings.aspect_ratio="original";kernel::W32(0x5000+0x1FC,0x12FFE);
    health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(Near(tutorial_drawn[0],-290.5f) && Near(tutorial_drawn[4],109.5f),"Ko-op keeps its native layout at 4:3");
    settings.aspect_ratio="auto";openshao::aspect::Output(1000,1500);kernel::W32(0x5000+0x1FC,0x12E14);
    health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
    Check(Near(tutorial_drawn[0],-290.5f) && Near(tutorial_drawn[1],-450.5f),"Ko-op stays at the top of a tall viewport");
    kernel::W32(0x5ACEBC,0);kernel::W32(0x790088,0);
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;openshao::aspect::Output(480,800);
        const float ratio=openshao::aspect::Current();
        const float left=-320.f*(1/openshao::aspect::CanvasScale(ratio).x-1);
        for(const auto callback:{0x15321u,0x161FEu}) {
            kernel::W32(0x5000+0x1FC,callback);
            health={};kernel::W32(health.esp+4,0x4000);sub_0011D000(health,nullptr);
            Check(Near(tutorial_drawn[0],-290.5f+(callback==0x15321?left:0)) &&
                  Near(tutorial_drawn[1],-210.5f) && Near(tutorial_drawn[6]-tutorial_drawn[0],400),
                  "common enemy bars follow the left edge while boss bars retain their position, size and baseline in every aspect");
        }
    }
    kernel::W32(0x5000+0x1FC,0);health_drawing=false;sprites=0;
    // The fill, target marker and input glyph use the meter callbacks. The
    // three frame pieces have a separate callback for each player.
    might_drawing=true;
    for(const auto [mode,shift]:{std::pair{"original",0.f},std::pair{"16:9",320.f/3},std::pair{"16:10",64.f},
                                 std::pair{"21:9",240.f},std::pair{"32:9",1600.f/3},std::pair{"auto",0.f}}) {
        settings.aspect_ratio=mode;openshao::aspect::Output(1000,1500);
        for(const auto callback:{0x12E6Eu,0x166BDu,0x12FA4u,0x12789u,0x11D93u,0x15CE5u,0x11244u,0x158A8u}) {
            might_right=callback==0x11D93u || callback==0x15CE5u || callback==0x158A8u;
            kernel::W32(0x5000+0x1FC,callback);
            X86Context meter;kernel::W32(meter.esp+4,0x4000);sub_0011D000(meter,nullptr);
            const float native_x=might_right?204.5f:-270.5f;
            Check(Near(tutorial_drawn[0],native_x+(might_right?shift:-shift)) &&
                  Near(tutorial_drawn[4]-tutorial_drawn[0],51.f) &&
                  Near(tutorial_drawn[1],-160.5f) && Near(tutorial_drawn[3],160.5f) &&
                  meter.esp==0x1008,
                  "Test Your Might frame, meter and input glyph move together without stretching or vertical movement");
        }
        kernel::W32(0x5000+0x1FC,0x161FE);
        X86Context other;kernel::W32(other.esp+4,0x4000);sub_0011D000(other,nullptr);
        Check(!expanded_red && Near(tutorial_drawn[0],might_right?204.5f:-270.5f),
              "other HUD callbacks keep their native centered-canvas placement");
    }
    might_drawing=false;might_right=false;kernel::W32(0x5000+0x1FC,0);sprites=0;

    settings.aspect_ratio="32:9";
    kernel::W32(0x639224,0x3000);kernel::W32(0x3308,0x2000);
    kernel::W32(0x201C,0xCAFEBABE);kernel::W32(0x2024,0xDEADBEEF);kernel::W32(0x202C,0x3F800000);
    X86Context ctx;sub_0011B290(ctx,nullptr);
    Check(Near(F32(0x2020),32.f/9) && kernel::R32(0x201C)==0xCAFEBABE && kernel::R32(0x2024)==0xDEADBEEF && kernel::R32(0x202C)==0x3F800000,
          "camera changes only the retail aspect field, preserving adjacent data and FOV");
    Check(ctx.esp==0x1004 && ctx.eax==0x1234 && ctx.ecx==0x5678 && ctx.guard==0x9876,"camera hook preserves the original return context");
    settings.aspect_ratio="auto";openshao::aspect::Output(3440,1440);ctx={};sub_0015AFA0(ctx,nullptr);
    Check(Near(F32(0x2020),3440.f/1440),"automatic mode follows real output dimensions on the guest thread");
    openshao::aspect::Output(1000,1500);ctx={};sub_0015AFA0(ctx,nullptr);
    Check(Near(F32(0x2020),1000.f/1500),"automatic projection follows a window aspect change");
    settings.aspect_ratio="16:9";ctx={};kernel::W32(ctx.esp+4,0x4000);kernel::W32(0x4008,0x5000);
    sub_0011D000(ctx,nullptr);
    Check(sprites==1 && Near(sprite_vertices[0],-.5f) && !openshao::aspect::canvas_depth,
          "screen sprite coordinates remain in their original centered projection space");
    for(const auto caller:{0x11C9D3u,0x12F8E1u}) {
        ctx={};kernel::W32(ctx.esp,caller);kernel::W32(ctx.esp+4,0x7000);sub_00100B44(ctx,nullptr);
        Check(Near(F32(0x7000),2.f/640*.75f) && Near(F32(0x7014),2.f/480) && Near(F32(0x7028),10) && ctx.esp==0x1018,
              "audited UI orthographic matrices preserve Z and stdcall ABI while correcting aspect");
    }
    ctx={};kernel::W32(ctx.esp,0x11BC2E);sub_00100B44(ctx,nullptr);
    Check(Near(F32(0x7000),2.f/640),"unrelated matrix creation retains the original projection");
    struct State {uint32_t scissor_count=0;std::array<uint32_t,32> scissors{};} state;
    {
        openshao::aspect::CanvasScope canvas;
        openshao::aspect::WithCanvasClip(state,[&]{Check(state.scissor_count==1 && state.scissors[0]==80 && state.scissors[2]==560,"UI draws retain their original canvas clipping");});
        Check(state.scissor_count==0 && state.scissors[0]==0,"UI clipping is restored after submission");
        state.scissor_count=1;state.scissors={160,120,480,360};
        openshao::aspect::WithCanvasClip(state,[&]{Check(state.scissors[0]==200 && state.scissors[2]==440 && state.scissors[1]==120,"existing scissors are transformed with the canvas");});
        Check(state.scissors[0]==160 && state.scissors[2]==480,"existing scissors are restored exactly");
    }
    kernel::W32(0x6CE8A0,0x5000);ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
    Check(sprites==1 && ctx.esp==0x1008,"widened cutscenes omit only their original cinematic mask sprites with RET4");
    for(const char* ratio:{"original","16:9","16:10","21:9","32:9"}) {
        settings.aspect_ratio=ratio;
        const auto before=sprites;
        ctx={};sub_0011D000(ctx,nullptr);
        Check(sprites==before && ctx.esp==0x1008,"Disabled cinematic bars omit only mask sprites at every aspect ratio");
        settings=openshao::launcher::Read(toml::parse("[enhancements]\ncinematic_bars=true\n"));settings.aspect_ratio=ratio;
        cinematic_drawing=true;ctx={};sub_0011D000(ctx,nullptr);cinematic_drawing=false;
        const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
        Check(sprites==before+1 && Near(tutorial_drawn[0],-320.f/scale.x-.5f) &&
              Near(tutorial_drawn[4],320.f/scale.x-.5f) && Near(tutorial_drawn[1],-240.f/scale.y-.5f) &&
              Near(tutorial_drawn[3],-168.f/scale.y-.5f),
              "Enabled cinematic bars fill the viewport width and retain their native animation and height");
        settings=openshao::launcher::Read({});
    }
    settings.aspect_ratio="16:9";kernel::W32(0x63F5E0,1);kernel::W32(0x63F5E4+0x10,0x3F800000);
    ctx={};sub_0013FFB0(ctx,nullptr);const auto poly=0x63F5E4+0x6C+0x10;
    Check(Near(F32(poly),79.5f) && Near(F32(poly+16),559.5f) && Near(F32(poly+8),7) &&
          kernel::R32(0x63F5E4+0x10)==0x3F800000,"only newly queued Flash positions change; existing polygons, Z and metadata remain intact");
    ctx={};ctx.ecx=0x63F5E4+0x6C;sub_0013F980(ctx,nullptr);
    Check(polygon_clipped && !openshao::aspect::canvas_depth,"only Flash-origin polygons activate canvas clipping during deferred rendering");
    openshao::menu::viewport_arrow=1;
    {
        openshao::aspect::CanvasScope parent;
        ctx={};ctx.ecx=0x63F5E4+0x6C;sub_0013F980(ctx,nullptr);
        Check(!polygon_scissored && !openshao::aspect::viewport_text_depth,
              "header arrows bypass the fitted scissor even inside a parent canvas and restore draw scope");
        ctx={};ctx.ecx=0x63F5E4;sub_0013F980(ctx,nullptr);
        Check(polygon_scissored,"the arrow exemption must not leak into the next polygon");
    }
    ctx={};ctx.ecx=0x63F5E4;sub_0013F980(ctx,nullptr);
    Check(!polygon_clipped,"other queued polygons retain their native clipping");
    ctx={};sub_0013FD70(ctx,nullptr);ctx={};ctx.ecx=0x63F5E4+0x6C;sub_0013F980(ctx,nullptr);
    Check(!polygon_clipped,"polygon provenance is cleared after the queue flush");
    Check(!openshao::menu::ViewportArrow(1),"arrow clipping exemptions are cleared with the polygon queue");
    float world[2]={-.5f,-.5f};openshao::aspect::CorrectUiVertices(world,1,8);
    Check(world[0]==-.5f && world[1]==-.5f,"unscoped world and post-processing vertices remain unchanged");
    // The engine's immediate rectangle queue includes the loading fill. Its
    // fitted border is a separate screen sprite, so both must use one canvas.
    const float rect[4]={168,328,380,396};
    std::memcpy(kernel::HostPtr(0x7104),rect,16);
    ctx={};ctx.ecx=0x7100;sub_0012FDE0(ctx,nullptr);
    Check(Near(rectangle_drawn[0],206) && Near(rectangle_drawn[1],326) && Near(rectangle_drawn[2],380),"loading fill uses the same horizontal scale as its sprite border");
    Check(Near(F32(0x7104),168) && Near(F32(0x7108),328),"rectangle queue entries are restored after drawing");
    struct TintVertex {float x,y,z;uint32_t color,uv;};
    std::array<TintVertex,4> tint{{{-320.5f,-240.5f,40,0x4B010101,0x12345678},{-320.5f,239.5f,40,0x4B010101,0x12345678},
                                 {319.5f,-240.5f,40,0x4B010101,0x12345678},{319.5f,239.5f,40,0x4B010101,0x12345678}}};
    const auto authored=tint;
    Check(!openshao::aspect::ExpandViewportTint(tint.data(),4,20),"unscoped full-sized geometry is never reinterpreted as UI");
    {
        openshao::aspect::SpriteScope sprite;
        Check(openshao::aspect::ExpandViewportTint(tint.data(),4,20) && Near((tint[0].x+.5f)*.75f,-320) && Near((tint[2].x+.5f)*.75f,320),
              "full-canvas black tints cover the physical viewport after the fitted sprite projection");
        Check(tint[0].color==authored[0].color && tint[0].uv==authored[0].uv && tint[0].z==authored[0].z,"tint expansion preserves alpha, texture coordinates and ordering");
        tint=authored;for(auto& v:tint)v.color=0x7F7F7F7F;
        Check(!openshao::aspect::ExpandViewportTint(tint.data(),4,20),"full-screen artwork retains its original aspect policy");
        tint=authored;for(auto& v:tint)v.color=0x4B00FFFF;
        Check(!openshao::aspect::ExpandViewportTint(tint.data(),4,20),"colored sprites are not mistaken for black viewport tints");
        tint=authored;tint[2].x=80;
        Check(!openshao::aspect::ExpandViewportTint(tint.data(),4,20),"partial screen-sprite widgets retain fitted canvas clipping");
        // Retail transition masks use an oversized 1024x1024 sprite instead
        // of the pause menu's exact 640x480 rectangle.
        tint=authored;
        for(unsigned i=0;i<4;++i) {
            tint[i].x=i<2?-512.f:512.f;tint[i].y=i%2?528.f:-496.f;
            tint[i].color=0x01000000;
        }
        Check(openshao::aspect::ExpandViewportTint(tint.data(),4,20) &&
              (tint[0].x+.5f)*.75f<=-320 && (tint[2].x+.5f)*.75f>=320,
              "oversized black transition masks cover the viewport and bypass fitted clipping");
        for(const auto mode:{"original","16:10","21:9","32:9","auto"}) {
            settings.aspect_ratio=mode;
            if(settings.aspect_ratio=="auto")openshao::aspect::Output(1000,1500);
            tint=authored;
            for(unsigned i=0;i<4;++i) {
                tint[i].x=i<2?-512.f:512.f;tint[i].y=i%2?528.f:-496.f;tint[i].color=0x01000000;
            }
            const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
            Check(openshao::aspect::ExpandViewportTint(tint.data(),4,20) &&
                  (tint[0].x+.5f)*scale.x<=-320 && (tint[2].x+.5f)*scale.x>=320 &&
                  (tint[0].y+.5f)*scale.y<=-240 && (tint[1].y+.5f)*scale.y>=240,
                  "oversized masks retain viewport coverage at fixed, original and portrait automatic ratios");
            Check(tint[0].color==0x01000000 && tint[0].uv==authored[0].uv && tint[0].z==authored[0].z,
                  "transition-mask resizing preserves opacity, texture sampling and depth");
        }
        settings.aspect_ratio="16:9";
        tint=authored;tint[0].x=-400;
        Check(!openshao::aspect::ExpandViewportTint(tint.data(),4,20),"nonrectangular black artwork does not become a viewport mask");
    }
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x593A40;sub_001B2C40(ctx,nullptr);
    Check(Near(F32(0x63F5F4),-.5f) && Near(F32(0x63F5F8),-80.5f) && Near(F32(0x63F604),639.5f) && !openshao::aspect::flash_polygons[0],
          "the native sky animation fills widescreen with proportional cover scaling");
    // These are the retail scene instances used by character selection,
    // Options, Kontent and save selection; their text is drawn separately.
    for(const uint32_t background:{0x58E4F0u,0x591628u,0x58F210u}) {
        kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=background;sub_001B2C40(ctx,nullptr);
        Check(Near(F32(0x63F5F4),-.5f) && Near(F32(0x63F5F8),-80.5f) && !openshao::aspect::flash_polygons[0],
              "submenu scenery covers the viewport without fitted canvas clipping");
        Check(!openshao::aspect::backdrop_depth && ctx.esp==0x101C && ctx.eax==0x1234,
              "background routing restores scope and preserves the animation ABI");
    }
    // Level Select's frame and vortex must match its fitted arena preview.
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x5936F8;sub_001B2C40(ctx,nullptr);
    Check(Near(F32(0x63F5F4),79.5f) && Near(F32(0x63F5F8),-.5f) && openshao::aspect::flash_polygons[0] &&
          !openshao::aspect::backdrop_depth && ctx.esp==0x101C && ctx.eax==0x1234,
          "Level Select scenery keeps the 4:3 composition of its arena preview");
    for(const uint32_t foreground:{0x591CB8u,0x5933B0u,0x58F8A0u,0x594F50u,0x592000u}) {
        kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=foreground;sub_001B2C40(ctx,nullptr);
        Check(Near(F32(0x63F5F4),79.5f) && Near(F32(0x63F5F8),-.5f) && openshao::aspect::flash_polygons[0],
              "map, selector, character art and gauge retain proportional interface fitting");
    }
    // Retail animation-library entries identify the blurred scenery and its
    // foreground scenery layer independently of the menu's fitted text.
    kernel::W32(0x7600+0x2E8,0x7A00);kernel::W32(0x7A08,5);kernel::W32(0x7A0C,0x7B00);
    kernel::W32(0x7B48,0x001C000D);
    for(const uint32_t resource:{0x6280E35Fu,0xC46465E6u,0x9587CD64u,0x14BA29FAu,
                                 0xAB8EFAE9u,0xFEE6AC1Fu,0xB3940E03u,0x137124EAu,0x2BD63191u,
                                 0x25055CA7u,0xA3DF797Du}) {
        kernel::W32(0x7B4C,resource);
        struct Sample {const char* mode;float left,top,right;};
        for(const auto sample:{Sample{"original",-.5f,-.5f,639.5f},Sample{"16:9",-.5f,-80.5f,639.5f},
                               Sample{"16:10",-.5f,-48.5f,639.5f},Sample{"21:9",-.5f,-180.5f,639.5f},
                               Sample{"32:9",-.5f,-400.5f,639.5f},Sample{"auto",-320.5f,-.5f,959.5f}}) {
            settings.aspect_ratio=sample.mode;
            if(settings.aspect_ratio=="auto")openshao::aspect::Output(1000,1500);
            kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x7600;kernel::W32(ctx.esp+12,0x001C000D);
            sub_001B2C40(ctx,nullptr);
            Check(Near(F32(0x63F5F4),sample.left) && Near(F32(0x63F5F8),sample.top) &&
                  Near(F32(0x63F604),sample.right) && !openshao::aspect::flash_polygons[0],
                  "animated menu scenery covers every fixed ratio and a portrait automatic output without fitted clipping");
            Check(Near(F32(0x63F5FC),7) && ctx.eax==0x1234 && ctx.esp==0x101C,
                  "transition scenery retains its depth and original animation return context");
        }
    }
    settings.aspect_ratio="16:9";kernel::W32(0x7B4C,0x71960991);
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x7600;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
    Check(Near(F32(0x63F5F4),79.5f) && Near(F32(0x63F5F8),-.5f) && openshao::aspect::flash_polygons[0],
          "other textures in the same animated menu retain their fitted layout");
    // Gallery margins must keep the native texel density, including when
    // automatic output changes orientation. Frame slots stay on one canvas.
    kernel::W32(0x58F210+0x2E8,0x7A00);kernel::W32(0x7B4C,0x3BF25039);gallery_mesh=true;
    struct GallerySample {const char* mode;unsigned width,height;};
    for(const auto sample:{GallerySample{"original",1280,720},{"16:9",1280,720},{"16:10",1280,720},
                           {"21:9",1280,720},{"32:9",1920,1080},{"auto",1280,720},
                           {"auto",3440,1440},{"auto",640,960},{"auto",800,600}}) {
        settings.aspect_ratio=sample.mode;openshao::aspect::Output(sample.width,sample.height);
        kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x58F210;kernel::W32(ctx.esp+12,0x001C000D);
        sub_001B2C40(ctx,nullptr);
        const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
        const uint32_t count=kernel::R32(0x63F5E0);
        Check(count>=2 && count<=512 && count%2==0,"gallery output fits the native polygon queue");
        // Each triangle edge must sample the same number of authored pixels
        // per display pixel. Stretching only the borders fails this condition.
        for(unsigned i=0;i<count;++i) {
            const uint32_t record=0x63F5E4+i*0x6C;
            for(unsigned v=1;v<3;++v) {
                const float dx=std::abs(F32(record+0x10+v*16)-F32(record+0x10));
                const float dy=std::abs(F32(record+0x14+v*16)-F32(record+0x14));
                const float du=std::abs(F32(record+0x40+v*4)-F32(record+0x40));
                const float dv=std::abs(F32(record+0x4C+v*4)-F32(record+0x4C));
                Check(std::abs(dx-du*640*scale.x)<.001f && std::abs(dy-dv*480*scale.y)<.001f,
                      "gallery decoration preserves texel density instead of stretching the margins");
            }
            Check(Near(F32(record+0x18),.75f) && kernel::R32(record+0x58)==0xFF7F7F7F &&
                  kernel::R32(record+0x68)==0x81870000,
                  "gallery repetition preserves depth, color and the native texture");
        }
        const float left=319.5f-320*scale.x,top=239.5f-240*scale.y;
        const float right=319.5f+320*scale.x,bottom=239.5f+240*scale.y;
        const auto center=0x63F5E4;
        Check(Near(F32(center+0x10),left) && Near(F32(center+0x14),top) &&
              Near(F32(center+0x30),right) && Near(F32(center+0x34),bottom) &&
              Near(F32(center+0x40),0) && Near(F32(center+0x4C),0),
              "the complete original wall retains exactly the thumbnail and selection canvas transform");
        if(scale.x==1 && scale.y==1) {
            Check(count==2,"4:3 preserves the original wall without generating decoration");continue;
        }
        const bool horizontal=scale.x<1;
        const float low=horizontal?left:top,high=horizontal?right:bottom,edge=horizontal?639.5f:479.5f;
        float left_edge=low,right_edge=high,left_uv=0,right_uv=1;
        for(unsigned i=2;i<count;i+=2) {
            const uint32_t record=0x63F5E4+i*0x6C;
            const float start=F32(record+(horizontal?0x10:0x14)),end=F32(record+(horizontal?0x30:0x34));
            const float uv_start=F32(record+(horizontal?0x40:0x4C)),uv_end=F32(record+(horizontal?0x48:0x54));
            const float border=horizontal?.1f:64.f/480;
            if(end<=low+.001f) {
                Check(Near(end,left_edge) && Near(uv_end,left_uv) && std::min(uv_start,uv_end)>=-.0001f &&
                      std::max(uv_start,uv_end)<=border+.0001f,"left/top repetitions touch and sample only decorative pixels");
                left_edge=start;left_uv=uv_start;
            } else {
                Check(start>=high-.001f && Near(start,right_edge) && Near(uv_start,right_uv) && std::min(uv_start,uv_end)>=1-border-.0001f &&
                      std::max(uv_start,uv_end)<=1.0001f,"right/bottom repetitions touch and sample only decorative pixels");
                right_edge=end;right_uv=uv_end;
            }
            Check(!openshao::aspect::flash_polygons[i] && !openshao::aspect::flash_polygons[i+1],
                  "decorative repetitions render without the fitted-canvas scissor");
        }
        Check(Near(left_edge,-.5f) && Near(right_edge,edge),"partial outer tiles cover the complete viewport without gaps");
        // Capacity is preflighted: exactly enough slots succeeds; one fewer
        // leaves every adjacent queue record untouched and uses the safe fit.
        const auto first=512-count;
        kernel::W32(0x63F5E0,first);ctx={};ctx.ecx=0x58F210;kernel::W32(ctx.esp+12,0x001C000D);
        sub_001B2C40(ctx,nullptr);
        Check(kernel::R32(0x63F5E0)==512,"gallery repetitions use an exactly sized polygon queue");
        kernel::W32(0x63F5E0,first+1);kernel::W32(0x63F5E4+(first+3)*0x6C,0xCAFEBABE);
        ctx={};ctx.ecx=0x58F210;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
        Check(kernel::R32(0x63F5E0)==first+3 && kernel::R32(0x63F5E4+(first+3)*0x6C)==0xCAFEBABE,
              "insufficient polygon space does not emit a partial background or overwrite adjacent records");
    }
    settings.aspect_ratio="32:9";
    kernel::W32(0x63F5E0,508);kernel::W32(0x63F5E4+510*0x6C,0xCAFEBABE);
    ctx={};ctx.ecx=0x58F210;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x63F5E0)==510 && kernel::R32(0x63F5E4+510*0x6C)==0xCAFEBABE &&
          Near(F32(0x63F5E4+508*0x6C+0x10),199.5f),
          "a full native polygon queue safely keeps gallery alignment without overwriting adjacent records");
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x7600;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x63F5E0)==2 && openshao::aspect::flash_polygons[0],
          "gallery margin expansion is restricted to the original gallery wall scene");
    settings.aspect_ratio="16:9";openshao::aspect::Output(1280,720);
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x5936F8;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x63F5E0)>2 && Near(F32(0x63F5E4+0x10),79.5f) && Near(F32(0x63F5E4+0x14),-.5f) &&
          !openshao::aspect::flash_polygons[0],
          "the Level Select wall keeps its 4:3 size and extends to the viewport with decorative repeats");
    // The arena-entry snapshot comes from the Level Select control (another
    // scene) and starts at y=0.047; it extends like the wall.
    mesh_top=0.046875f;kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598C90);
    kernel::W32(0x63F5E0,0);ctx={};ctx.ecx=0x7600;kernel::W32(ctx.esp+12,0x001C000D);sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x63F5E0)>2 && Near(F32(0x63F5E4+0x10),79.5f) && !openshao::aspect::flash_polygons[0],
          "the arena-entry snapshot extends past the 4:3 box with the same decorative repeats");
    kernel::W32(0x6C9308,0);kernel::W32(0x6C930C,0);mesh_top=-.5f;
    gallery_mesh=false;kernel::W32(0x58F210+0x2E8,0);settings.aspect_ratio="16:9";
    kernel::W32(0x7B4C,0x6280E35F);kernel::W32(0x63F5E0,0);ctx={};kernel::W32(ctx.esp+12,0x001C000D);
    sub_0013FFB0(ctx,nullptr);
    Check(Near(F32(0x63F5F4),79.5f) && openshao::aspect::flash_polygons[0],
          "animation resource lookup does not leak into unrelated queued draws");
    kernel::W32(0x63F5E0,0);openshao::aspect::flash_polygons.fill(true);modal_mesh=true;
    ctx={};ctx.ecx=0x7200;sub_001B2C40(ctx,nullptr);modal_mesh=false;
    Check(Near(F32(0x63F5F4),-.5f) && Near(F32(0x63F5F8),-.5f) &&
          Near(F32(0x63F5F4+32),639.5f) && !openshao::aspect::flash_polygons[0],
          "the outer edges of a multipart modal shade cover the complete viewport");
    const auto panel=0x63F5E4+8*0x6C;
    Check(Near(F32(panel+0x10),139.875f) && Near(F32(panel+0x14),146) && openshao::aspect::flash_polygons[8],
          "modal shade expansion leaves the dialog panel and inner cutout on the fitted canvas");
    Check(kernel::R32(0x63F5E4+0x58)==0xB2000000 && Near(F32(0x63F5E4+0x18),.05f),
          "modal anchoring preserves the original opacity and draw order");
    settings.aspect_ratio="original";kernel::W32(0x63F5E0,0);modal_mesh=true;
    ctx={};ctx.ecx=0x7200;sub_001B2C40(ctx,nullptr);modal_mesh=false;
    Check(Near(F32(0x63F5F4),-.1f) && Near(F32(0x63F5F8),.03f),"original presentation retains the authored modal vertices");
    settings.aspect_ratio="16:9";
    ctx={};kernel::W32(ctx.esp+4,0x4000);kernel::W32(0x4000,0x7500);kernel::W32(0x4008,0x5100);
    kernel::W32(0x7504,530u|(123u<<16));
    for(const auto [offset,value]:{std::pair{0x2Cu,.83203125f},std::pair{0x30u,.55078125f},
                                  std::pair{0x44u,.85546875f},std::pair{0x48u,.57421875f}})
        kernel::W32(0x7500+offset,std::bit_cast<uint32_t>(value));
    sub_0011D000(ctx,nullptr);
    Check(expanded_red,"the native solid-fill sprite expands Game Over's red overlay without changing its color");
    ctx={};kernel::W32(0x752C,0);kernel::W32(0x7544,std::bit_cast<uint32_t>(1.f));sub_0011D000(ctx,nullptr);
    Check(!expanded_red,"full-screen textured artwork remains fitted rather than being treated as a solid fill");
    // Native GUI image factory and screen-sprite pass: a tutorial's blue band ends at its
    // portrait; only these message-manager image roles may escape fitting.
    tutorial_drawing=true;
    // Retail portrait: centered at 551.5, 239 authored pixels wide. Its old
    // right edge is 31.5 pixels beyond the stage, independently of aspect.
    kernel::W32(0x5100+0x1D8,0x7E00);kernel::W32(0x7E04,239u|(170u<<16));
    kernel::W32(0x5100+0xC0,std::bit_cast<uint32_t>(551.5f));
    kernel::W32(0x4008,0x5100);kernel::W32(0x4000,0x7500);
    kernel::W32(0x752C,0);kernel::W32(0x7544,std::bit_cast<uint32_t>(1.f));
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;
        if(settings.aspect_ratio=="auto")openshao::aspect::Output(3440,1440);
        const float extra=settings.aspect_ratio=="original"?0:settings.aspect_ratio=="16:9"?320.f/3:
            settings.aspect_ratio=="16:10"?64:settings.aspect_ratio=="21:9"?240:
            settings.aspect_ratio=="32:9"?1600.f/3:760.f/3;
        for(const uint32_t image:{0xA9u,0x9Du,0x9Eu}) {
            ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,image);
            sub_00016866(ctx,nullptr);
            Check(ctx.eax==0x5100 && ctx.esp==0x1038,"tutorial image tracking preserves the native factory result and RET52");
            ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
            // The portrait factory runs after the panel factory in the game.
            if(image==0xA9) {
                X86Context portrait;portrait.ecx=0x4E6E20;kernel::W32(portrait.esp+4,0xE);kernel::W32(portrait.esp+8,0x9D);
                sub_00016866(portrait,nullptr);
                ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,image);sub_00016866(ctx,nullptr);
            }
            ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
            Check(expanded_red && Near(tutorial_drawn[0],image==0xA9?-320.5f-extra:80.5f+extra) &&
                  Near(tutorial_drawn[4],(image==0xA9?80.5f:319.5f)+extra) &&
                  Near(tutorial_drawn[1],image==0xA9?116.5f:70.5f) && Near(tutorial_drawn[7],image==0xA9?239.5f:240.5f),
                  "tutorial panel closes the half-pixel seam and both portrait layers retain their full width inside the screen");
        }
        ctx={};ctx.ecx=0x4E6E20;sub_000165C3(ctx,nullptr);
        ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
        Check(!expanded_red && Near(tutorial_drawn[0],-320.5f),"clearing the native message image cache also clears tutorial sprite roles");
    }
    settings.aspect_ratio="16:9";
    for(const auto [manager,image]:{std::pair{0x4EF3C8u,0xA9u},std::pair{0x4E6E20u,0x99u}}) {
        ctx={};ctx.ecx=manager;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,image);sub_00016866(ctx,nullptr);
        ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
        Check(!expanded_red,"other GUI managers and message artwork retain their existing layout");
    }
    settings.aspect_ratio="16:9";
    for(const auto owner:{0x5100u,0x5200u}) {
        gui_sprite_result=owner;kernel::W32(owner+0x1D8,0x7E00);kernel::W32(owner+0xC0,std::bit_cast<uint32_t>(551.5f));
        ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,0x9D);sub_00016866(ctx,nullptr);
    }
    for(const auto owner:{0x5100u,0x5200u}) {
        kernel::W32(0x4008,owner);ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
        Check(expanded_red && Near(tutorial_drawn[0],80.5f+320.f/3),
              "every cached instance of a tutorial image receives the same viewport placement");
    }
    gui_sprite_result=0x5100;kernel::W32(0x4008,0x5100);
    tutorial_drawing=false;
    // Pausing a message frees its image cache. Later GUI allocations can reuse
    // those sprite addresses for the pause backdrop, frame and submenu pieces.
    pause_drawing=true;
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;
        for(const auto output:{std::pair{1920u,1080u},std::pair{900u,1600u}}) {
            openshao::aspect::Output(output.first,output.second);
            for(const auto [manager,image]:{std::pair{0x4EF3C8u,0xA9u},std::pair{0x4EF3C8u,0x9Du},
                                           std::pair{0x4EF3C8u,0x40u},std::pair{0x4E6E20u,0x99u}}) {
                ctx={};ctx.ecx=0x4E6E20;sub_000165C3(ctx,nullptr);
                gui_sprite_result=0x5100;kernel::W32(0x5100+0x1FC,0);
                ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,0x9D);
                sub_00016866(ctx,nullptr);
                ctx={};ctx.ecx=manager;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,image);
                sub_00016866(ctx,nullptr);
                ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
                Check(!expanded_red && Near(tutorial_drawn[0],-280.5f) && Near(tutorial_drawn[4],279.5f) &&
                      Near(tutorial_drawn[1],-150.5f) && Near(tutorial_drawn[3],149.5f),
                      "pause and submenu sprites reusing a message address retain native geometry in every aspect");
            }
            for(const uint32_t image:{0xA9u,0x9Du,0x9Eu}) {
                ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,image);
                sub_00016866(ctx,nullptr);
                ctx={};ctx.ecx=0x4E6E20;sub_000152A9(ctx,nullptr);
                Check(ctx.eax==0x1234 && ctx.ecx==0x5678 && ctx.guard==0x9876 && ctx.esp==0x1008,
                      "destroying message sprites preserves the original result and stack cleanup");
                ctx={};kernel::W32(ctx.esp+4,0x4000);sub_0011D000(ctx,nullptr);
                Check(!expanded_red && Near(tutorial_drawn[0],-280.5f) && Near(tutorial_drawn[4],279.5f),
                      "destroyed tutorial sprites cannot deform a subsequent pause or debug menu");
                Check(openshao::aspect::tutorial_portrait_inset==0,
                      "destroying the message cache also retires its portrait inset");
            }
        }
    }
    pause_drawing=false;
    ctx={};ctx.ecx=0x4E6E20;sub_000165C3(ctx,nullptr);
    settings.aspect_ratio="auto";openshao::aspect::Output(1080,720);
    ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,0x9D);sub_00016866(ctx,nullptr);
    std::array<TintVertex,4> band{{{-320.5f,116.5f,121,0x3C7F7F7F,0x1234},{-320.5f,239.5f,121,0x3C7F7F7F,0x1234},
                                  {111.5f,116.5f,121,0x3C7F7F7F,0x1234},{111.5f,239.5f,121,0x3C7F7F7F,0x1234}}};
    std::array<TintVertex,4> portrait{{{112,70.5f,122,0x3C7F7F7F,0x5678},{112,240.5f,122,0x3C7F7F7F,0x5678},
                                      {351,70.5f,122,0x3C7F7F7F,0x5678},{351,240.5f,122,0x3C7F7F7F,0x5678}}};
    {openshao::aspect::SpriteScope sprite(false,openshao::aspect::TutorialSprite::Panel);openshao::aspect::ExpandViewportTint(band.data(),4,20);}
    {openshao::aspect::SpriteScope sprite(false,openshao::aspect::TutorialSprite::Portrait);openshao::aspect::ExpandViewportTint(portrait.data(),4,20);}
    const auto tutorial_scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
    Check(Near(band[2].x,portrait[0].x) && Near((portrait[2].x+.5f)*tutorial_scale.x,320),
          "a fractional 3:2 output has a shared band/portrait edge and no right-side portrait clipping");
    Check(band[0].color==0x3C7F7F7F && portrait[0].uv==0x5678 && portrait[0].z==122 && portrait[0].y==70.5f,
          "seam closure preserves native opacity, atlas coordinates, layer order and portrait height");
    ctx={};ctx.ecx=0x4E6E20;sub_000165C3(ctx,nullptr);
    Check(openshao::aspect::tutorial_portrait_inset==0,"message reset clears the previous portrait's inset");
    settings.aspect_ratio="auto";openshao::aspect::Output(1000,1500);
    band=std::array<TintVertex,4>{{{-320.5f,116.5f,121,0x3C7F7F7F,0x1234},{-320.5f,239.5f,121,0x3C7F7F7F,0x1234},
                                  {111.5f,116.5f,121,0x3C7F7F7F,0x1234},{111.5f,239.5f,121,0x3C7F7F7F,0x1234}}};
    {openshao::aspect::SpriteScope sprite(false,openshao::aspect::TutorialSprite::Panel);openshao::aspect::ExpandViewportTint(band.data(),4,20);}
    Check(Near(band[0].y,356.5f) && Near(band[1].y,479.5f) && band[0].uv==0x1234,
          "the tutorial band follows the bottom of a tall viewport with its authored height and atlas coordinates");

    // Exercise the actual native producer -> queue slot -> copied command ->
    // deferred font hook. A new unanchored command must erase the slot's tag.
    auto render_queue=[&](uint32_t index=0) {
        X86Context copy;copy.ecx=0x7800;kernel::W32(copy.esp+4,0x7D00);kernel::W32(copy.esp+8,index);
        sub_001EEC70(copy,nullptr);
        openshao::aspect::CanvasScope fonts;
        X86Context draw;kernel::W32(draw.esp+4,0x7D00);sub_001EF210(draw,nullptr);
        Check(Near(F32(0x7D00),F32(0x7A00)) && Near(F32(0x7D04),F32(0x7A04)) && draw.esp==0x1008,
              "anchoring restores the copied font command and preserves its native return context");
    };
    for(const auto [mode,shift]:{std::pair{"original",0.f},std::pair{"16:9",320.f/3},std::pair{"16:10",64.f},
                                 std::pair{"21:9",240.f},std::pair{"32:9",1600.f/3},std::pair{"auto",0.f}}) {
        settings.aspect_ratio=mode;openshao::aspect::Output(1000,1500);
        for(const auto [caller,x,y]:{std::array{0x8A810u,75u,412u},std::array{0x8A810u,550u,412u},
                                     std::array{0x8AB0Cu,320u,40u}}) {
            kernel::W32(0x7800,0);
            queue_fixture=true;
            ctx={};kernel::W32(ctx.esp,caller);kernel::W32(ctx.esp+4,x);kernel::W32(ctx.esp+8,y);
            kernel::W32(ctx.esp+20,0x7C00);sub_00152CE0(ctx,nullptr);
            queue_fixture=false;
            render_queue();
            const float expected_x=x==75?75.f-shift:x==550?550.f+shift:320.f;
            Check(std::abs(prompt_drawn_x-expected_x)<.001f && Near(prompt_drawn_y,float(y)) &&
                  (x==320 || shift==0 || !prompt_clipped),
                  "the deferred Test Your Might timer follows either frame while the title stays centered");
        }
    }
    kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(100.f));
    kernel::W32(0x7A04,std::bit_cast<uint32_t>(60.f));kernel::W32(0x7A34,0x7C00);kernel::W32(0x7C00,0x1234);
    ctx={};sub_00014A2A(ctx,nullptr);render_queue();
    Check(Near(prompt_drawn_x,100) && Near(prompt_drawn_y,-180) && !prompt_clipped,
          "deferred health labels remain attached to their bar after a tall-window resize");
    settings.aspect_ratio="16:9";kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(100.f));
    ctx={};sub_00014A2A(ctx,nullptr);render_queue();
    Check(Near(prompt_drawn_x,100.f-320.f/3) && !prompt_clipped,"solo labels survive the original font's left clamp in widescreen");
    kernel::W32(0x5ACEBC,0x790000);kernel::W32(0x790088,1);
    for(const float x:{100.f,450.f}) {
        kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(x));
        ctx={};sub_00012E14(ctx,nullptr);render_queue();
        Check(Near(prompt_drawn_x,x+(x<320?-1:1)*320.f/3) && !prompt_clipped,"Ko-op labels follow their player's top corner");
    }
    kernel::W32(0x5ACEBC,0);kernel::W32(0x790088,0);
    // The game's Danger producer sends a foreground and outline through the
    // deferred font queue, tagged independently for both players.
    kernel::W32(0x5ACEBC,0x790000);
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;openshao::aspect::Output(1000,1500);
        for(const auto game_mode:{0u,1u,2u})for(const auto player:{1u,2u}) {
            kernel::W32(0x790088,game_mode);
            const auto anchor=player==1?openshao::aspect::UiAnchor::TopLeft:openshao::aspect::UiAnchor::TopRight;
            const auto offset=openshao::aspect::AnchorOffset(anchor,openshao::aspect::Current());
            const float x=player==1?208.f:432.f;
            kernel::W32(0x7800,0);
            X86Context danger;kernel::W32(danger.esp+4,uint32_t(x));kernel::W32(danger.esp+8,108);
            kernel::W32(danger.esp+16,player);sub_000124C3(danger,nullptr);
            Check(kernel::R32(0x7800)==2 && danger.esp==0x1004 &&
                  openshao::aspect::text_layout==openshao::aspect::TextLayout::None,
                  "the two Danger text layers retain the original call and restore their scope");
            for(unsigned layer=0;layer<2;++layer) {
                render_queue(layer);
                Check(Near(prompt_drawn_x,x+(game_mode==2?0:offset.x)) &&
                      Near(prompt_drawn_y,108.f+(game_mode==2?0:offset.y)),
                      "Danger foreground and outline follow the player's HUD, preserving Versus and 4:3");
            }
        }
    }
    kernel::W32(0x790088,0);kernel::W32(0x5ACEBC,0);
    for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
        settings.aspect_ratio=mode;openshao::aspect::Output(480,800);
        const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
        const float offset=320.f*(1/scale.x-1);
        kernel::W32(0x7A04,std::bit_cast<uint32_t>(404.f));
        kernel::W32(0x7A00,std::bit_cast<uint32_t>(49.f));kernel::W32(0x7800,0);
        ctx={};sub_00015321(ctx,nullptr);render_queue();
        Check(Near(prompt_drawn_x,49-offset) && Near(prompt_drawn_y,404) && !prompt_clipped,
              "the common enemy's deferred name follows its bar without moving the boss baseline");
        kernel::W32(0x7A00,std::bit_cast<uint32_t>(500.f));kernel::W32(0x7800,0);
        ctx={};sub_00013DB4(ctx,nullptr);render_queue();
        Check(Near(prompt_drawn_x,500+offset) && Near(prompt_drawn_y,404) && !prompt_clipped,
              "player two's common enemy name retains its right-edge placement");
    }
    kernel::W32(0x7A00,std::bit_cast<uint32_t>(100.f));
    kernel::W32(0x7800,0);QueueFixture();render_queue();
    Check(Near(prompt_drawn_x,100) && prompt_clipped,"ordinary text reusing a health queue slot keeps its original layout and clipping");
    // Combo praise, count and multiplier glyphs keep their offset from solo health,
    // including long labels crossing the canvas midpoint.
    kernel::W32(0x5ACEBC,0x790000);kernel::W32(0x790088,0);
    for(const auto producer:{sub_000133A0,sub_000149FD,sub_00016267}) {
        for(const auto mode:{"original","16:9","16:10","21:9","32:9","auto"}) {
            settings.aspect_ratio=mode;
            for(const auto output:{std::pair{3440u,1440u},std::pair{480u,800u}}) {
                openshao::aspect::Output(output.first,output.second);
                const auto offset=openshao::aspect::AnchorOffset(openshao::aspect::UiAnchor::TopLeft,openshao::aspect::Current());
                for(const float x:{125.f,215.f,350.f}) {
                    kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(x));
                    kernel::W32(0x7A04,std::bit_cast<uint32_t>(145.f));
                    ctx={};producer(ctx,nullptr);render_queue();
                    Check(Near(prompt_drawn_x,x+offset.x) && Near(prompt_drawn_y,145+offset.y) && !prompt_clipped,
                          "solo combo praise, hits and multiplier follow the same corner as health in every aspect");
                    Check(ctx.eax==0x1234 && ctx.ecx==0x5678 && ctx.guard==0x9876 && ctx.esp==0x1004 &&
                          openshao::aspect::text_layout==openshao::aspect::TextLayout::None,
                          "combo producer anchoring preserves native return state and restores the enclosing layout");
                }
            }
        }
        settings.aspect_ratio="32:9";
        kernel::W32(0x790088,2);kernel::W32(0x7800,0);
        ctx={};producer(ctx,nullptr);render_queue();
        Check(Near(prompt_drawn_x,350) && Near(prompt_drawn_y,145) && prompt_clipped,
              "Versus retains its original combo composition");
        kernel::W32(0x790088,1);
        for(const float x:{100.f,350.f}) {
            kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(x));
            ctx={};producer(ctx,nullptr);render_queue();
            Check(Near(prompt_drawn_x,x<320?x-320.f*(32.f/12-1):x+320.f*(32.f/12-1)-openshao::aspect::KoopComboInset) &&
                  Near(prompt_drawn_y,145) && !prompt_clipped,
                  "each Ko-op combo follows its player's top corner");
        }
        kernel::W32(0x7A00,std::bit_cast<uint32_t>(350.f));
        kernel::W32(0x790088,0);
    }
    settings.aspect_ratio="16:9";
    kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(320.f));kernel::W32(0x7A04,std::bit_cast<uint32_t>(240.f));
    combo_bonus_fixture=true;ctx={};sub_000133A0(ctx,nullptr);combo_bonus_fixture=false;render_queue();
    Check(Near(prompt_drawn_x,320) && Near(prompt_drawn_y,240) && prompt_clipped,
          "the combo producer's separate central experience reward stays centered");
    kernel::W32(0x5ACEBC,0x790000);kernel::W32(0x790088,1);
    for(const float x:{320.f,476.f}) {
        kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(x));
        combo_bonus_fixture=true;ctx={};sub_000133A0(ctx,nullptr);combo_bonus_fixture=false;render_queue();
        Check(Near(prompt_drawn_x,x==320?320:x+320.f/3-openshao::aspect::KoopComboInset) && (x==320)==prompt_clipped,
              "in Ko-op the shared call site keeps a centered reward and moves player two's count");
    }
    for(const uint32_t mode:{0u,1u}) {
        kernel::W32(0x790088,mode);kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(476.f));
        ctx={};sub_00011DD4(ctx,nullptr);render_queue();
        Check(mode?Near(prompt_drawn_x,476+320.f/3-openshao::aspect::KoopComboInset) && !prompt_clipped:Near(prompt_drawn_x,476) && prompt_clipped,
              "player two's Ko-op count follows the right corner and the producer is untouched outside Ko-op");
    }
    kernel::W32(0x5ACEBC,0);kernel::W32(0x790088,0);
    kernel::W32(0x7A00,std::bit_cast<uint32_t>(320.f));
    kernel::W32(0x7800,0);QueueFixture();render_queue();
    Check(Near(prompt_drawn_x,320) && prompt_clipped,"unrelated text reusing a combo queue slot keeps the original layout");
    settings.aspect_ratio="auto";openshao::aspect::Output(1000,1500);
    kernel::W32(0x7800,0);kernel::W32(0x7A00,std::bit_cast<uint32_t>(60.f));kernel::W32(0x7A04,std::bit_cast<uint32_t>(380.f));
    kernel::W32(0x4E6E20+0x6D4,6);ctx={};ctx.ecx=0x4E6E20;sub_000123BA(ctx,nullptr);render_queue();
    Check(Near(prompt_drawn_x,60) && Near(prompt_drawn_y,620) && !prompt_clipped,
          "tutorial body text follows its band after the native producer has returned");
    settings.aspect_ratio="21:9";render_queue();
    Check(Near(prompt_drawn_x,-180) && Near(prompt_drawn_y,380),"queued tutorial text resolves anchoring again when the output ratio changes");
    openshao::aspect::rendered_text={};
    settings.aspect_ratio="16:9";
    kernel::W32(0x4E6E20+0x6D4,6);kernel::W32(0x7C00,0x3D);kernel::W32(0x7C80,0x3D);
    ctx={};ctx.ecx=0x4E6E20;kernel::W32(ctx.esp+4,0xE);kernel::W32(ctx.esp+8,0x9D);sub_00016866(ctx,nullptr);
    kernel::W32(0x7800,0);queue_fixture=true;
    ctx={};kernel::W32(ctx.esp,0x86714);kernel::W32(ctx.esp+4,590);kernel::W32(ctx.esp+8,396);
    kernel::W32(ctx.esp+20,0x7C00);sub_00152AE0(ctx,nullptr);
    queue_fixture=false;render_queue();
    Check(Near(prompt_drawn_x,590.f+320.f/3-31.5f) && !prompt_clipped,
          "the deferred tutorial confirmation glyph follows the portrait beyond the old canvas edge");
    settings.aspect_ratio="auto";openshao::aspect::Output(1000,1500);render_queue();
    Check(Near(prompt_drawn_x,558.5f) && Near(prompt_drawn_y,636) && !prompt_clipped,
          "the confirmation glyph follows the portrait to the bottom of a tall viewport");
    kernel::W32(0x7800,0);QueueFixture();render_queue();
    Check(Near(prompt_drawn_x,590) && Near(prompt_drawn_y,396) && prompt_clipped,
          "an unrelated identical glyph at identical coordinates never inherits a tutorial anchor");
    openshao::aspect::rendered_text={};settings.aspect_ratio="16:9";
    ctx={};kernel::W32(ctx.esp+4,0x6000);kernel::W32(ctx.esp+8,640);kernel::W32(ctx.esp+12,480);
    kernel::W32(0x50FB04,640);kernel::W32(0x50FB08,480);sub_00141BF0(ctx,nullptr);
    Check(Near(F32(0x6000),79.5f) && Near(F32(0x6018),559.5f) && kernel::R32(0x6010)==0xCD000004 && ctx.esp==0x1004,
          "movie fitting preserves its source UVs and the original cdecl return context");
    // The Versus arena-name producer stays silent under the Survival difficulty popup
    // (its caption overlapped the description at y=345).
    auto arena_caption=[&](uint32_t caller,bool popup,uint32_t top,uint32_t parent){
        openshao::survival::setup_popup=popup;
        const uint32_t depth=top==0x596810?2:1;
        kernel::W32(0x6C9308,depth);
        kernel::W32(0x6C9308+depth*4,top);
        if(depth>1)kernel::W32(0x6C9308+(depth-1)*4,parent);
        kernel::W32(0x7800,0);
        queued_text_y=0;
        X86Context caption;kernel::W32(caption.esp,caller);
        kernel::W32(caption.esp+4,320);kernel::W32(caption.esp+8,345);
        kernel::W32(caption.esp+20,0x7C00);
        sub_00152CE0(caption,nullptr);
        return std::array<uint32_t,3>{kernel::R32(0x7800),queued_text_y,caption.esp};
    };
    Check(arena_caption(0x45F0D,true,0x596810,0x598DF0)==std::array<uint32_t,3>{0,0,0x1004} &&
          arena_caption(0x4F97B,true,0x596810,0x598DF0)==std::array<uint32_t,3>{0,0,0x1004} &&
          arena_caption(0x4F97B,true,0x598DF0,0x598430)==std::array<uint32_t,3>{0,0,0x1004},
          "the exact native Versus ROOFTOPS caption must not reach the font queue during Survival difficulty");
    Check(arena_caption(0x45F0E,true,0x596810,0x598DF0)[0]==1 &&
          arena_caption(0x4F97C,true,0x596810,0x598DF0)[0]==1 &&
          arena_caption(0x45F0D,false,0x596810,0x598DF0)[0]==1 &&
          arena_caption(0x45F0D,true,0x596810,0x598430)[0]==1 &&
          arena_caption(0x45F0D,true,0x598430,0)[0]==1,
          "unrelated text, normal Versus and other menus preserve all native centered font draws");
    openshao::survival::setup_popup=false;
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    kernel::W32(0x59843C,0x7200);kernel::W32(0x7200+0x2A0,6);
    kernel::W32(0x592D20+0xC,0);
    ctx={};ctx.ecx=0x598430;sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x592D20+0xC)==1,
          "The rendered main menu must reactivate the single native white selector after retail navigation disables it");
    kernel::W32(0x592D20+0xC,0);kernel::W32(0x7200+0x2A0,19);
    ctx={};ctx.ecx=0x598430;sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x592D20+0xC)==0,
          "The native exit movie must not reactivate a selection sparkle over its transition");
    kernel::W32(0x7200+0x2A0,6);openshao::survival::setup_popup=true;
    ctx={};ctx.ecx=0x598430;sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x592D20+0xC)==0,
          "The Survival setup popup must not reactivate the underlying main-menu sparkle");
    openshao::survival::setup_popup=false;
    for(const auto aspect:{4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,3.f/2})for(unsigned pal=0;pal<2;++pal)for(unsigned row=0;row<10;++row) {
        settings.aspect_ratio="auto";openshao::aspect::Output(unsigned(aspect*480),480);
        kernel::W32(0x598468,row);kernel::W32(0x4DF4DC,28);kernel::W32(0x5CE864,pal);
        openshao::recomp::extra_selected=row>=6;
        const auto step=28+pal*2,original_y=155+pal*10+(row%7)*step;
        ctx={};ctx.ecx=0x592D20;kernel::W32(ctx.esp+20,original_y);sub_001B2C40(ctx,nullptr);
        const unsigned visual=row==9?3:row>=3?row+1:row;
        Check(selector_argument_y==133+pal*10+visual*(step-6) && kernel::R32(0x1014)==original_y && ctx.esp==0x101C,
              "all ten main-menu sparkles must follow the compact visual rows and restore the guest argument");
    }
    openshao::survival::medallion_menu=true;kernel::W32(0x6C930C,0x598430);
    // The Graphic mock returns false: test both the unfiltered native batch and the
    // filtered composition without an aspect requeue.
    const uint32_t saved_main_scene=kernel::R32(0x59843C);
    kernel::W32(0x59843C,0x7600);
    kernel::W32(0x7600+0x2E8,0x7A00);
    kernel::W32(0x7A08,5);kernel::W32(0x7A0C,0x7B00);
    kernel::W32(0x7B48,0x001C000D);
    gallery_mesh=true;gold_order_mesh=true;
    for(const bool native_gold:{true,false}) {
        kernel::W32(0x63F5E0,0);
        openshao::aspect::animation_scene=0x7600;
        constexpr std::array<uint32_t,3> parts{0x59FB5186u,0x71960991u,0x4599E8C7u};
        const unsigned start=native_gold?0u:1u;
        for(unsigned part=start;part<parts.size();++part) {
            kernel::W32(0x7B4C,parts[part]);
            ctx={};kernel::W32(ctx.esp+12,0x001C000D);sub_0013FFB0(ctx,nullptr);
            const unsigned first=(part-start)*2;
            Check(kernel::R32(0x63F5E0)==first+2 &&
                  kernel::R32(0x63F5E4+first*0x6C)==parts[part] &&
                  kernel::R32(0x63F5E4+(first+1)*0x6C)==parts[part],
                  "the aspect hook keeps each native ornament pair in its original queue order");
        }
        const unsigned native_count=unsigned(parts.size()-start)*2;
        const auto before_selector=selector_draws;
        kernel::W32(0x7B4C,0xE1E1E1E1u);
        kernel::W32(0x598468,0);kernel::W32(0x5CE864,0);
        ctx={};ctx.ecx=0x592D20;kernel::W32(ctx.esp+20,155);sub_001B2C40(ctx,nullptr);
        Check(kernel::R32(0x63F5E0)==native_count+2 &&
              kernel::R32(0x63F5E4+native_count*0x6C)==0xE1E1E1E1u &&
              kernel::R32(0x63F5E4+(native_count+1)*0x6C)==0xE1E1E1E1u &&
              selector_draws==before_selector+1 && selector_argument_y==133 && ctx.esp==0x101C,
              "Survival draws the single white selector after the native ornament without requeueing bronze");
        if(!native_gold)for(unsigned i=0;i<native_count+2;++i)
            Check(kernel::R32(0x63F5E4+i*0x6C)!=0x59FB5186u,
                  "an already filtered Survival batch stays free of the bronze pointer after the white selector");
    }
    gallery_mesh=false;gold_order_mesh=false;openshao::aspect::animation_scene=0;
    kernel::W32(0x59843C,saved_main_scene);
    for(unsigned row=0;row<4;++row) {
        kernel::W32(0x598468,row);kernel::W32(0x5CE864,0);
        ctx={};ctx.ecx=0x592D20;kernel::W32(ctx.esp+20,155+row*28);sub_001B2C40(ctx,nullptr);
        const unsigned visual=openshao::menu::MainVisualRow(row),step=kernel::R32(0x4DF4DC);
        const int expected=int(155+row*28+(visual-row)*step)+int(-openshao::menu::MainMenuRise+openshao::menu::MainListAdjustment(visual));
        Check(int(selector_argument_y)==expected && kernel::R32(0x1014)==155+row*28,
              "the Survival white glint uses Main's own row placement, since its options are Main's rows");
    }
    openshao::survival::transitioning=true;
    kernel::W32(0x7200+0x2A0,2);kernel::W32(0x592D20+0xC,0);
    ctx={};ctx.ecx=0x598430;sub_001B2C40(ctx,nullptr);
    Check(kernel::R32(0x592D20+0xC)==1,
          "the native player-focus movie cannot turn off the only white selection sparkle");
    const auto previous_survival_draws=selector_draws;
    kernel::W32(0x598468,1);
    ctx={};ctx.ecx=0x592D20;kernel::W32(ctx.esp+20,183);sub_001B2C40(ctx,nullptr);
    Check(selector_draws==previous_survival_draws+1 &&
          int(selector_argument_y)==183+int(-openshao::menu::MainMenuRise+openshao::menu::MainListAdjustment(1)) && ctx.esp==0x101C,
          "Ko-op retains the white sparkle during selection motion instead of losing focus");
    openshao::survival::transitioning=false;
    ctx={};ctx.ecx=0x5929D8;sub_001B2C40(ctx,nullptr);
    Check(auxiliary_glint_draws==0,"the Survival Main overlay must not add a second random sparkle");
    openshao::survival::medallion_menu=false;
    kernel::W32(0x6C930C,0x598430);
    ctx={};ctx.ecx=0x5929D8;sub_001B2C40(ctx,nullptr);
    Check(auxiliary_glint_draws==0 && ctx.esp==0x101C,
          "the native main menu suppresses the independent sparkle that duplicates its selection cursor");
    kernel::W32(0x6C930C,0x598DF0);
    ctx={};ctx.ecx=0x5929D8;sub_001B2C40(ctx,nullptr);
    Check(auxiliary_glint_draws==1,
          "menu sparkle filtering must leave other native screens and effects intact");
    kernel::W32(0x58E4F0+0xC,1);kernel::W32(0x58E4F0+0x2A0,4);
    const auto courtyard_before=courtyard_draws;
    ctx={};ctx.ecx=0x58E4F0;sub_001B2C40(ctx,nullptr);
    Check(courtyard_draws==courtyard_before+1 && kernel::R32(0x58E4F0+0xC)==1,"the fighter screen keeps drawing its courtyard backdrop");
    kernel::W32(0x6C930C,0x598430);
    ctx={};ctx.ecx=0x58E4F0;sub_001B2C40(ctx,nullptr);
    Check(courtyard_draws==courtyard_before+1 && kernel::R32(0x58E4F0+0xC)==0 && kernel::R32(0x58E4F0+0x2A0)==0 && ctx.esp==0x101C,
          "a fighter courtyard left running is stopped instead of showing through Main's backdrop");
    kernel::W32(0x6C930C,0x598430);
    const auto before_characters=selector_draws;
    for(unsigned depth:{1u,2u}) {
        kernel::W32(0x6C9308,depth);kernel::W32(0x6C930C,0x598DF0);
        kernel::W32(0x6C9310,0x596810);
        ctx={};ctx.ecx=0x592D20;kernel::W32(ctx.esp+20,235);sub_001B2C40(ctx,nullptr);
        Check(selector_draws==before_characters && ctx.esp==0x101C,
              "character selection and its difficulty popup must not draw the main-menu sparkle and retain native stack cleanup");
    }
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    ctx={};kernel::W32(ctx.esp,0x48F0C);kernel::W32(ctx.esp+20,0x7000);
    kernel::W32(ctx.esp+12,60);kernel::W32(ctx.esp+24,std::bit_cast<uint32_t>(.8f));
    sub_00152AE0(ctx,nullptr);
    Check(openshao::menu::controller_notice==0x7000 && openshao::menu::notice_alpha==60 && Near(openshao::menu::notice_scale,.8f) && ctx.esp==0x1004,
          "the retail controller notice must use the separate main-menu notice area while preserving its localized string and cdecl ABI");
    openshao::menu::controller_notice=0;
    ctx={};kernel::W32(ctx.esp,0x48F0D);kernel::W32(ctx.esp+20,0x7000);sub_00152AE0(ctx,nullptr);
    Check(!openshao::menu::controller_notice,"other native text must retain its own layout");
    std::puts("PASS: retail camera ABI, resize, screen sprites, cinematic masks, queued Flash UI and movie vertices");
}
