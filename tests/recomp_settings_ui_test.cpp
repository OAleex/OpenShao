#include "kernel/kernel.h"
#include "../src/recomp_settings.cpp"
#include <array>
#include <iostream>

static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
static std::vector<uint32_t> sounds;
static unsigned starts=0;
static unsigned survival_opens=0;
static bool setup_popup=false;
static bool survival_overlay=false;
static bool survival_players=false;
static unsigned survival_overlay_draws=0;
namespace openshao::survival {void Open(X86Context&,uint8_t*){++survival_opens;} bool MainMenu(X86Context&){return false;} bool SetupPopup(){return setup_popup;}}
namespace openshao::survival {
bool MedallionMenu(){return survival_overlay;}
bool OverlayShown(){return survival_overlay;}
float OrnamentExtension(){return survival_players?-128.f:-156.f;}
bool Transitioning(){return false;}
bool HideMainCaption(std::string_view id){return survival_overlay && id.starts_with("main.") && id!="main.single_player";}
void DrawMenu(X86Context&,unsigned){++survival_overlay_draws;}
bool MenuInput(X86Context&,uint8_t*,bool){return false;}
}
static bool persist_fails=false,prepare_fails=false,commit_fails=false;
static unsigned persisted=0,prepared=0,committed=0,cancelled=0;
static openshao::launcher::Settings last_saved;
static std::vector<std::string> save_order;
static unsigned exit_requests=0;
static bool exit_succeeds=true;
static std::vector<bool> previews;
static unsigned preview_stops=0;
static bool preview_succeeds=true;
static std::vector<uint32_t> native_close_calls;
static unsigned native_preferences_applied=0;
namespace openshao::scorpion {
bool Preview(X86Context&,bool enabled){previews.push_back(enabled);return preview_succeeds;}
void StopPreview(X86Context&){++preview_stops;}
}
namespace openshao {bool RequestExit(){++exit_requests;return exit_succeeds;}}
struct Caption {std::string text;float x,y;unsigned alpha;};
static std::vector<Caption> captions;
namespace kernel {
uint8_t* g_base=memory.data();
unsigned main_backdrop_starts=0;
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count) {
    if(fn==0x1B1240 && count==1) {
        const auto scene=R32(ctx.ecx+12);W32(scene+0x2A0,R32(args[0]));W32(scene+0x33C,R32(scene+0x24+R32(args[0])*16));G<uint8_t>(scene+0x340)=0;++starts;
    } else if(fn==0x100100 || fn==0x100200 || fn==0x100300)sounds.push_back(fn);
    else if(fn==0x14CFA || fn==0x166DB || fn==0x11EAB)native_close_calls.push_back(fn);
    else if(fn==0x13A08 && count==0)++main_backdrop_starts;
    else if(fn!=0x1B29B0 && fn!=0x1B2390) { std::cerr<<"Unexpected native UI call: "<<std::hex<<fn<<'\n'; std::abort(); }
    ctx.eax=0xBAD;ctx.ecx=0xBAD;ctx.esp=0xBAD;return 0;
}
}
namespace openshao::launcher {
const Settings& Current(){return settings;}
Settings Saved(){return settings;}
bool LanguageAvailable(std::string_view){return true;}
bool ApplyInGame(const Settings& s,bool save,std::string& error){
    if(save){save_order.push_back("persist");if(persist_fails){error="Fixture write failure";return false;}last_saved=s;++persisted;}
    const auto language=settings.language,voice=settings.voice_language;settings=s;settings.language=language;settings.voice_language=voice;return true;
}
}
namespace openshao::restart {
bool Prepare(const launcher::Settings&,std::string& error){++prepared;save_order.push_back("prepare");if(prepare_fails){error="Fixture preflight failure";return false;}return true;}
bool Commit(std::string& error){++committed;save_order.push_back("commit");if(commit_fails){error="Fixture close failure";return false;}return true;}
void Cancel(){++cancelled;}
}
namespace openshao::performance {bool Visible(){return false;}}
namespace openshao::preferences {
void ReadActive(launcher::Settings&){}
void ApplyNow(X86Context&,const launcher::Settings&){++native_preferences_applied;}
}
namespace logx {void Write(Level,const char*,...) {}}
static std::vector<int> applied_audio_modes;
namespace openshao::audio::options {
void Apply(X86Context&,int mode) {applied_audio_modes.push_back(mode);}
}
namespace openshao::menu {
float NativeLabel(X86Context&,float x,float y,std::string_view text,float,unsigned alpha,bool,bool,float) {
    captions.push_back({std::string(text),x,y,alpha});return 100;
}
void NativeButton(X86Context&,int,int,Button){}
void TitleWidth(float){}
unsigned MainHighlight(uint32_t state,unsigned selected){return state>=1 && state<=6?state-1:std::min(selected,5u);}
void MainRowAnchor(uint32_t,uint32_t){}
void MainExtraLabel(X86Context&,uint32_t,uint32_t,bool,bool,unsigned){}
void DimMenuLabel(uint32_t first,uint32_t end,bool selected) {
    if(selected)for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v){const auto a=0x63F5E4+i*0x6C+0x58+v*4;kernel::W32(a,(kernel::R32(a)&0xFFFFFF)|((kernel::R32(a)>>24)*76/254<<24));}
}
}
X86_FUNC(__imp__sub_00015C86){kernel::Return(ctx,0,2);}
X86_FUNC(__imp__sub_00014679){kernel::Return(ctx,0,2);}
X86_FUNC(__imp__sub_00011D84){kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_00011EAB){kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_000131CF){kernel::Return(ctx,0,0);}
X86_FUNC(__imp__sub_001B2200){kernel::Return(ctx,0,1);}
static void Quad(float x,float y,float width,float height,unsigned alpha=254) {
    for(unsigned t=0;t<2;++t)std::memset(kernel::HostPtr(0x63F5E4+t*0x6C+0x40),0,0x18);
    for(unsigned t=0;t<2;++t)for(unsigned v=0;v<3;++v) {
        const auto a=0x63F5E4+t*0x6C;
        kernel::W32(a+0x10+v*16,std::bit_cast<uint32_t>(x+(v==2?width:0)));
        kernel::W32(a+0x14+v*16,std::bit_cast<uint32_t>(y+(v==1?height:0)));
        kernel::W32(a+0x58+v*4,(alpha<<24)|0xFFFFFF);
    }
    kernel::W32(0x63F5E0,2);
}
static float LowerCap(X86Context& ctx,uint32_t scene,float x,float y) {
    Quad(x,y,128,120);
    // The lower cap uses the same texture as the upper one, vertically flipped.
    for(unsigned i=0;i<2;++i) {
        const auto address=0x63F5E4+i*0x6C;
        kernel::W32(address+0x4C,std::bit_cast<uint32_t>(1.f));
        kernel::W32(address+0x50,0);
    }
    openshao::recomp::Graphic(ctx,scene,0x4599E8C7,0,2);
    return std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
}
int main() {
    std::set_terminate([] { try { if(auto e=std::current_exception())std::rethrow_exception(e); } catch(const std::exception& e) { std::fprintf(stderr,"Uncaught UI test exception: %s\n",e.what()); } std::abort(); });
    using namespace openshao::recomp;
    int failures=0;auto check=[&](bool ok,const char* why){if(!ok){++failures;std::cerr<<why<<'\n';}};
    X86Context ctx{};ctx.esp=0x3000;ctx.ecx=0x598430;
    kernel::W32(0x59843C,0x5000);kernel::W32(0x5000+0x2A0,6);kernel::W32(0x5000+0x33C,77);
    kernel::W32(0x598468,5);kernel::W32(ctx.esp+4,0x4000);kernel::W32(ctx.esp+8,0x4004);
    kernel::W32(0x6CA3C0,0x100100);kernel::W32(0x6CA3B8,0x100200);kernel::W32(0x6CA3BC,0x100300);
    survival_overlay=true;kernel::W32(0x59847C,0x5000);
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    Quad(90,163,256,60);
    check(!Graphic(ctx,0x5000,0xD760EEC1,0,2) && kernel::R32(0x63F5E0)==2 && survival_overlay_draws==1,
          "The Survival pages keep Main's Single Player row for their first option and name it once");
    Quad(90,163,256,60);
    check(Graphic(ctx,0x5000,0xF063C950,0,2) && kernel::R32(0x63F5E0)==0 && survival_overlay_draws==1,
          "The Survival Main overlay hides the other native main captions without duplicating its own labels");
    Quad(90,163,256,60);
    check(!Graphic(ctx,0x7100,0xF063C950,0,2),
          "The Main-only Survival overlay must not intercept an unrelated Kontent scene");
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    Quad(20,107,128,120);
    check(!Graphic(ctx,0x5000,0x4599E8C7,0,2) && kernel::R32(0x63F5E0)==2,
          "The one-option Survival overlay retains its original upper medallion");
    const float survival_upper=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
    LowerCap(ctx,0x5000,20,310);
    const float solo_lower=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
    check(kernel::R32(0x63F5E0)==2 && solo_lower>survival_upper,
          "The one-option Survival overlay retains the unscaled flipped lower half of the original ornament");
    Quad(31,305.6f,64,78.6f);
    check(!Graphic(ctx,0x5000,0x71960991,0,2) && kernel::R32(0x63F5E0)==2,
          "The one-option ornament must keep the fitted native link between the original upper and lower halves");
    Quad(31,227,64,78.6f);
    check(Graphic(ctx,0x5000,0x71960991,0,2) && kernel::R32(0x63F5E0)==0,
          "A full-length Main shaft section must not hang below the short Survival ornament");
    Quad(31,320,64,60);
    check(!Graphic(ctx,0x5000,0x49B7A657,0,2) && kernel::R32(0x63F5E0)==2,
          "The native medallion joint remains visible rather than leaving the emblem cut in half");
    survival_players=true;
    LowerCap(ctx,0x5000,20,310);
    const float players_lower=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
    check(kernel::R32(0x63F5E0)==2 &&
          std::abs(players_lower-solo_lower-28.f)<.01f,
          "The two-option page keeps the original full emblem and extends its native lower half by one row");
    Quad(31,305.6f,64,78.6f);
    check(!Graphic(ctx,0x5000,0x71960991,0,2) && kernel::R32(0x63F5E0)==2,
          "The two-option page preserves the connected, fitted original shaft segment");
    Quad(31,227,64,78.6f);
    check(Graphic(ctx,0x5000,0x71960991,0,2) && kernel::R32(0x63F5E0)==0,
          "The two-option page must not leave surplus shaft below the lower half of its emblem");
    Quad(20,107,128,120);
    check(!Graphic(ctx,0x5000,0x4599E8C7,0,2) && kernel::R32(0x63F5E0)==2 &&
          std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-survival_upper)<.01f,
          "Both short Survival pages retain the identical retail upper medallion placement");
    // The small bronze bitmap is not part of the complete carved emblem. Only
    // the native white glint marks the focused row on either short page.
    for(unsigned row=0;row<4;++row) {
        kernel::W32(0x598468,row);
        Quad(106,23.f+6.f*row,48,60);
        check(Graphic(ctx,0x5000,0x59FB5186,0,2) && kernel::R32(0x63F5E0)==0,
              "The Survival pages must omit the redundant bronze selection sprite for current and future choices");
    }
    survival_players=false;
    survival_overlay=false;
    Quad(20,107,128,120);
    check(!Graphic(ctx,0x5000,0x4599E8C7,0,2) &&
          std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-survival_upper)<.01f,
          "Returning to the root Main preserves its original upper medallion artwork and placement");
    LowerCap(ctx,0x5000,20,310);
    check(kernel::R32(0x63F5E0)==2,
          "Only the root Main retains the intact long ornament and original lower cap");
    kernel::W32(0x4DF4D8,1);survival_overlay=true;
    ctx.ecx=0x598430;sub_000131CF(ctx,memory.data());
    check(kernel::R32(0x4DF4D8)==0xA8C,
          "The Main attract countdown must be held while either Survival submenu is visible");
    survival_players=true;kernel::W32(0x4DF4D8,1);sub_000131CF(ctx,memory.data());
    check(kernel::R32(0x4DF4D8)==0xA8C,
          "The Ko-op player count page must not unexpectedly launch the native title demo after idle");
    survival_overlay=false;survival_players=false;
    kernel::W32(0x4DF4D8,1);sub_000131CF(ctx,memory.data());
    check(kernel::R32(0x4DF4D8)==1,
          "The root Main attract countdown must retain its unmodified retail behavior");
    {
        const auto scene=kernel::R32(0x59843C),clip=scene?kernel::R32(scene+0x2A0):0;
        if(scene) {
            for(const auto [frame_clip,starts]:{std::pair{8u,1u},{3u,1u},{7u,0u},{11u,0u}}) {
                kernel::W32(scene+0x2A0,frame_clip);kernel::main_backdrop_starts=0;sub_000131CF(ctx,memory.data());
                check(kernel::main_backdrop_starts==starts,"Main's lake backdrop is kept on in return and focus clips but not in leave clips");
            }
            kernel::W32(scene+0x2A0,clip);
        }
    }
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    kernel::W32(0x598468,5);
    for(const auto [row,direction,wanted]:{std::tuple{2u,0x4000u,9u},std::tuple{9u,0x4000u,3u},
            std::tuple{3u,0x1000u,9u},std::tuple{9u,0x1000u,2u}}) {
        ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x598468,row);kernel::W32(0x4000,direction);
        check(MainInput(ctx,memory.data()) && kernel::R32(0x598468)==wanted,
              "Game Mode must be reachable between Versus and Load in either navigation direction");
    }
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x598468,9);kernel::W32(0x4000,0);kernel::W32(0x4004,0x40);
    check(MainInput(ctx,memory.data()) && survival_opens==1 && !Active(),
          "Accepting Game Mode must open the game-mode popup without opening recompilation settings");
    starts=0;sounds.clear();extra_selected=active=closing=false;kernel::W32(0x598468,5);kernel::W32(0x4004,0);kernel::W32(0x5000+0x2A0,6);kernel::W32(0x5000+0x33C,77);
    ctx.esp=0x3000;ctx.ecx=0x598430;
    kernel::W32(0x4000,0x4000);
    check(MainInput(ctx,memory.data()) && ExtraSelected() && kernel::R32(0x598468)==6,"Moving down from Kontent must select the recompilation settings row");
    check(starts==0 && kernel::R32(0x5000+0x33C)==77,"Adjacent Kontent and recomp selections must not restart the shared native clip");
    check(sounds.size()==1 && sounds.back()==0x100100,"Crossing the added menu boundary must play the native navigation sound once");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4000,0x1000);MainInput(ctx,memory.data());
    check(!ExtraSelected() && kernel::R32(0x598468)==5 && starts==0,"Returning to Kontent must retain the running native animation");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4000,0x4000);MainInput(ctx,memory.data());
    ctx.esp=0x3000;ctx.ecx=0x598430;MainInput(ctx,memory.data());
    check(kernel::R32(0x598468)==7 && kernel::R32(0x5000+0x2A0)==6,
          "Moving below recompilation settings must select Enhancements without indexing a nonexistent native clip");
    ctx.esp=0x3000;ctx.ecx=0x598430;MainInput(ctx,memory.data());
    check(kernel::R32(0x598468)==8,"Moving below Enhancements must select Exit");
    ctx.esp=0x3000;ctx.ecx=0x598430;MainInput(ctx,memory.data());
    check(kernel::R32(0x598468)==0,"Moving below Exit must wrap to Single Player");
    extra_selected=true; // A native frontend reset may retain the prior custom highlight flag.
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4000,0x1000);MainInput(ctx,memory.data());
    check(kernel::R32(0x598468)==8,"Moving above Single Player must wrap to Exit");
    extra_selected=false;kernel::W32(0x598468,5);
    setup_popup=true;Quad(90,316,256,60,76);
    check(Graphic(ctx,0x5000,0xF063C950,0,2) && kernel::R32(0x63F5E0)==0,
          "The setup popup must hide background main-menu labels so difficulty descriptions stay readable");
    setup_popup=false;
    Quad(90,316,256,60,76);Graphic(ctx,0x5000,0xF063C950,0,2);
    check((kernel::R32(0x63F5E4+0x58)>>24)==254,"Selecting Kontent must not inherit its previous unselected animation opacity");
    extra_selected=true;Quad(90,316,256,60,254);Graphic(ctx,0x5000,0xF063C950,0,2);
    check((kernel::R32(0x63F5E4+0x58)>>24)==76,"Selecting the extra row must not flash the borrowed Kontent highlight");
    LowerCap(ctx,0x5000,20,310);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-348.f)<.01f && 348.f+120.f<=468.f,
          "The complete lower medallion must stay inside the bottom safe area");
    Quad(31,320,64,60);Graphic(ctx,0x5000,0x49B7A657,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-358.f)<.01f,
          "The main ornament joint must follow the taller shaft without stretching its carved texture");
    Quad(31,305.6f,64,56);Graphic(ctx,0x5000,0x71960991,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-303.6f)<.01f &&
          std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x24))-399.6f)<.01f,
          "The shortened ornament shaft must connect its original carved components");
    const float near=LowerCap(ctx,0x5000,20,310)-310,translated=LowerCap(ctx,0x5000,48,285)-285;
    check(std::abs(near-38)<.01f && std::abs(near-translated)<.01f,
          "The carved lower cap must keep its extension while the native animation translates it across the screen");
    check(std::abs(LowerCap(ctx,0x5000,20,310)-310-near)<.01f,
          "The Main ornament keeps its native dimensions across Survival overlay entry");
    Quad(48,305.6f,64,56);Graphic(ctx,0x5000,0x71960991,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x24))-399.6f)<.01f,
          "The translated shaft must remain connected to the extended carved pieces during the native slide");
    check(std::abs(LowerCap(ctx,0x7000,20,330)-330)<.01f,
          "The original Kontent ornament must retain its authored animation and dimensions");
    kernel::W32(0x59847C,0x7100);
    Quad(20,107,128,120);Graphic(ctx,0x5000,0x4599E8C7,0,2);
    const float upper_cap_bottom=std::bit_cast<float>(kernel::R32(0x63F5E4+0x24));
    Quad(31,227,64,78.6f);Graphic(ctx,0x5000,0x71960991,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-upper_cap_bottom)<.01f,
          "The upper cap and attached shaft must retain their native shared edge without a cut or gap");
    Quad(51,18.75f,512,120);Graphic(ctx,0x5000,0x79D0AA6F,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))+1.25f)<.01f,
          "The padded game logo must rise above the taller menu while preserving its original dimensions");
    check(348.f+120.f+12.f<=480.f,
          "The lower medallion must have a safe bottom margin in the original 480-pixel canvas");
    kernel::W32(0x5000+0x2A0,0);kernel::W32(0x5000+0x24,4);kernel::W32(0x5000+0x28,13);
    float previous_extension=0;
    for(unsigned movie_frame=4;movie_frame<=13;++movie_frame) {
        kernel::W32(0x5000+0x33C,movie_frame);
        const float extension=LowerCap(ctx,0x5000,48,285)-283;
        check(extension>=previous_extension-.01f && extension-previous_extension<26,
              "Opening must grow the extra main-menu height over successive native frames without a final jump");
        if(movie_frame==4)check(std::abs(extension)<.01f,"The extra ornament height must begin collapsed with the native entrance");
        previous_extension=extension;
    }
    check(std::abs(previous_extension-40)<.01f,"The completed entrance must fit the ten-row ornament inside the canvas");
    kernel::W32(0x5000+0x2A0,15);kernel::W32(0x5000+0x24+15*16,225);kernel::W32(0x5000+0x28+15*16,240);
    kernel::W32(0x5000+0x33C,225);const float leaving=LowerCap(ctx,0x5000,48,285)-285;
    kernel::W32(0x5000+0x33C,240);
    check(std::abs(leaving-38)<.01f && std::abs(LowerCap(ctx,0x5000,48,285)-285+2)<.01f,
          "Leaving the main menu must collapse the added height with its original exit clip");
    kernel::W32(0x5000+0x2A0,6);
    kernel::W32(0x592D20+0xC,0);
    RestoreMainSelector();
    check(kernel::R32(0x592D20+0xC)==1,
          "The selected sparkle must be restored immediately after the native input callback");
    kernel::W32(0x592D20+0xC,0);kernel::W32(0x5000+0x2A0,19);
    RestoreMainSelector();
    check(kernel::R32(0x592D20+0xC)==0,
          "Leaving Main must not turn the sparkle back on during exit transitions");
    kernel::W32(0x5000+0x2A0,6);
    for(unsigned row=0;row<10;++row) {
        kernel::W32(0x598468,row);extra_selected=row>=6;
        kernel::W32(0x4DF4DC,28);kernel::W32(0x5CE864,0);
        Quad(52,188+28.f*std::min(row,5u),64,64);
        check(!Graphic(ctx,0x5000,0x59FB5186,0,2) && kernel::R32(0x63F5E0)==2,
              "The main-menu gold pointer must remain visible beside the native white selection sparkle");
        const float actual_y=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
        const float actual_x=std::bit_cast<float>(kernel::R32(0x63F5E4+0x10));
        check(std::abs(actual_x-52.f)<.01f,
              "The gold marker keeps its authored x on the emblem, like Konfigure Recompilation");
        const unsigned visual=openshao::menu::MainVisualRow(row);
        const float expected_y=188.f+28.f*std::min(row,5u)+
            (row>=6?28.f*(float(visual)-5):row>=3?28.f:0)-openshao::menu::MainMenuRise+
            openshao::menu::MainListAdjustment(visual);
        check(std::abs(actual_y-expected_y)<.01f,
              "The gold pointer must follow all ten compressed main-menu rows, including Game Mode");
        for(uint32_t key:{0x54811410u,0x00017E4Au}) {
            Quad(52,188+28.f*std::min(row,5u),64,64);
            check(Graphic(ctx,0x5000,key,0,2) && kernel::R32(0x63F5E0)==0,
                  "Main-menu movie glints must stay hidden on every row, including native rows 0-5");
        }
    }
    for(unsigned clip:{0u,7u,8u,15u,18u}) {
        kernel::W32(0x5000+0x2A0,clip);
        Quad(52,188,64,64,127);
        check(!Graphic(ctx,0x5000,0x59FB5186,0,2) && kernel::R32(0x63F5E0)==2 &&
              kernel::R32(0x63F5E4+0x58)>>24==127,
              "The original gold marker must preserve its bitmap and opacity during Main movie clips");
    }
    kernel::W32(0x5000+0x2A0,19);
    Quad(52,188,64,64);
    check(Graphic(ctx,0x5000,0x59FB5186,0,2) && kernel::R32(0x63F5E0)==0,
          "Main's later exit clips must not leave a pointer above the incoming screen");
    kernel::W32(0x5000+0x2A0,6);
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4000,0);kernel::W32(0x4004,0x40);
    kernel::W32(0x598468,7);kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    check(MainInput(ctx,memory.data()) && Active() && model.CurrentPage()==Page::Enhancements &&
          Snapshot().rows.size()==6 && !exit_prompt,
          "The new main-menu Enhancements row must open its standalone list rather than Exit");
    ctx.esp=0x3000;ctx.ecx=0x5985E8;release=false;Process(ctx,0,0x20);
    check(closing,"Leaving unchanged Enhancements must close the native Options scene");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0);MainInput(ctx,memory.data());
    check(!Active() && kernel::R32(0x598468)==7,"Closing Enhancements must restore its main-menu row");
    check(kernel::R32(0x592D20+0xC)==1,
          "Returning from the native Options or Exit dialog must restore the white selection star before the next frame");
    model.Open(settings);active=true;closing=false;release=false;selection=0;
    kernel::W32(0x5985F4,0x6000);kernel::W32(0x6000+0x2E4,3);
    kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x5985E8);kernel::G<uint8_t>(0x6C92FD)=1;
    // Real Options uses clip 24 (284 -> 291) to enter Audio and clip 34 to return.
    kernel::W32(0x6000+0x24+24*16,284);kernel::W32(0x6000+0x28+24*16,291);
    kernel::W32(0x6000+0x24+34*16,291);kernel::W32(0x6000+0x28+34*16,284);
    Process(ctx,0,0x40);
    check(kernel::R32(0x6000+0x2A0)==24,"Entering a custom category must start the original Options-to-Audio movie clip");
    check(sounds.size()>=3 && sounds.back()==0x100200,"Entering a settings category must use the native accept sound");
    const auto sound_count=sounds.size();Process(ctx,0x4000,0);
    check(selection==0 && sounds.size()==sound_count,"Navigation during the native movie must not skip rows or emit duplicate sounds");
    Quad(52,216,64,64);Graphic(ctx,0x6000,0x59FB5186,0,2);
    check(kernel::R32(0x63F5E0)==0,"The ornament pointer must hide with the native sparkle while the submenu movie runs");
    kernel::W32(0x6000+0x33C,288);
    // Incoming title and first row inherit independent native translation and opacity.
    frame={};Quad(72,46,256,60,127);Graphic(ctx,0x6000,0x64E46E7E,0,2);
    Quad(110,226,256,60,127);Graphic(ctx,0x6000,0xF7900E18,0,2);
    Quad(110,240,256,60,76);Graphic(ctx,0x6000,0x3973BDED,0,2);captions.clear();Draw(ctx);
    bool incoming=false;for(const auto& c:captions)if(c.text==openshao::menu::Catalog::Builtin().Get("en","settings.mode") && c.alpha==127 && c.x==113 && c.y==231)incoming=true;
    check(incoming,"Custom rows must inherit the native first-row position and opacity without a host-time slide or fade");
    kernel::W32(0x6000+0x33C,291);kernel::G<uint8_t>(0x6000+0x340)=1;
    frame={};Quad(52,36,256,60);Graphic(ctx,0x6000,0x64E46E7E,0,2);
    Quad(90,206,256,60);Graphic(ctx,0x6000,0xF7900E18,0,2);
    Quad(90,234,256,60,76);Graphic(ctx,0x6000,0x3973BDED,0,2);captions.clear();Draw(ctx);
    incoming=false;for(const auto& c:captions)if(c.text==openshao::menu::Catalog::Builtin().Get("en","settings.mode") && c.alpha==254 && c.x==93 && c.y==211)incoming=true;
    check(incoming,"Completed native clips must restore the canonical text layout");
    release=false;Process(ctx,0,0x20);
    check(model.CurrentPage()==Page::Home && kernel::R32(0x6000+0x2A0)==34,"Returning from a custom category must use the original Audio-to-Options clip");
    animation.running=false;kernel::G<uint8_t>(0x6000+0x340)=1;
    for(const auto [page,extension]:{std::pair{Page::Home,72.f},std::pair{Page::Display,24.f},std::pair{Page::Graphics,48.f},std::pair{Page::Controls,24.f},std::pair{Page::Controller,72.f},std::pair{Page::Keyboard,72.f},std::pair{Page::KeyboardAlternate,72.f},std::pair{Page::Mouse,72.f},std::pair{Page::Audio,-48.f},std::pair{Page::Language,-48.f},std::pair{Page::Enhancements,48.f}}) {
        model.Go(page);selection=0;LowerCap(ctx,0x6000,20,250);
        const float top=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14)),bottom=std::bit_cast<float>(kernel::R32(0x63F5E4+0x24));
        check(std::abs(top-250-extension)<.01f && std::abs(bottom-top-120)<.01f,"Every settings category must size its ornament to the visible rows while preserving the carved cap proportions");
    }
    model.Go(Page::Home);kernel::W32(0x6000+0x2A0,0);kernel::W32(0x6000+0x24,4);kernel::W32(0x6000+0x28,13);
    kernel::W32(0x6000+0x33C,4);
    check(std::abs(LowerCap(ctx,0x6000,48,230)-230)<.01f,"Custom Options must begin its added height collapsed with the retail entrance");
    kernel::W32(0x6000+0x33C,13);
    check(std::abs(LowerCap(ctx,0x6000,48,230)-302)<.01f,"Custom Options must reach the complete seven-row height at the end of its entrance");
    const auto home=Snapshot();model.Go(Page::Audio);animation.Start(ctx,home,false);
    float previous_custom=72;
    for(unsigned movie_frame=284;movie_frame<=291;++movie_frame) {
        kernel::W32(0x6000+0x33C,movie_frame);
        const float extension=LowerCap(ctx,0x6000,48,230)-230;
        check(extension<=previous_custom+.01f && previous_custom-extension<30,
              "Changing from seven rows to two must contract continuously during the native submenu clip");
        if(movie_frame==284)check(std::abs(extension-72)<.01f,"A category transition must start at the previous ornament height");
        previous_custom=extension;
    }
    check(std::abs(previous_custom+48)<.01f,"The completed Audio transition must enclose exactly its two rows");
    animation.running=false;kernel::G<uint8_t>(0x6000+0x340)=1;
    model.Go(Page::Audio);Quad(31,249.3f,64,56);Graphic(ctx,0x6000,0x71960991,0,2);
    const float shaft_top=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14)),shaft_bottom=std::bit_cast<float>(kernel::R32(0x63F5E4+0x24));
    check(std::abs(shaft_top-249.3f)<.01f && std::abs(shaft_bottom-257.3f)<.01f,"The two-row audio ornament must extend around both controls without folding its shaft");
    Quad(80,66,256,60,127);Graphic(ctx,0x6000,0xC7AE190C,0,2);captions.clear();Draw(ctx);
    bool followed=false;for(const auto& c:captions)if(c.text==openshao::menu::Catalog::Builtin().Get("en","settings.master") && c.x==121 && c.y==241 && c.alpha==127)followed=true;
    check(followed,"Custom content must follow the native page entrance and exit position and fade, including outside the stable Options state");
    model.Go(Page::Home);animation.running=true;animation.back=true;kernel::W32(0x6000+0x2A0,23);
    frame={};Quad(52,36,256,60,127);Graphic(ctx,0x6000,0xC7AE190C,0,2);
    Quad(90,206,256,60,127);Graphic(ctx,0x6000,0x2CC0BFA5,0,2);captions.clear();Draw(ctx);
    bool exiting=false;for(const auto& c:captions)if(c.text=="Display" && c.alpha==127)exiting=true;
    check(exiting,"Closing settings after a native return must preserve the outer exit clip's original row opacity");
    animation.running=false;
    for(unsigned category=0;category<5;++category) {
        model.Open(settings);selection=category;release=false;Process(ctx,0,0x40);
        kernel::G<uint8_t>(0x6000+0x340)=1;release=false;Process(ctx,0,0x20);
        check(model.CurrentPage()==Page::Home && selection==category,"Returning from each category must restore its selected home row");
        kernel::G<uint8_t>(0x6000+0x340)=1;
    }
    model.Open(settings);selection=2;release=false;Process(ctx,0,0x40);
    kernel::G<uint8_t>(0x6000+0x340)=1;selection=1;release=false;Process(ctx,0,0x40);
    kernel::G<uint8_t>(0x6000+0x340)=1;selection=24;release=false;Process(ctx,0,0x40);
    kernel::G<uint8_t>(0x6000+0x340)=1;selection=15;release=false;Process(ctx,0,0x20);
    check(model.CurrentPage()==Page::Keyboard && selection==24,"Returning from alternate bindings must restore the paginated alternate menu row");
    kernel::G<uint8_t>(0x6000+0x340)=1;release=false;Process(ctx,0,0x20);
    check(model.CurrentPage()==Page::Controls && selection==1,"Returning from keyboard bindings must restore Keyboard on the Controls page");
    kernel::G<uint8_t>(0x6000+0x340)=1;release=false;Process(ctx,0,0x40);
    check(model.CurrentPage()==Page::Keyboard && selection==24,"Reentering a page must retain its last selected row and pagination");
    kernel::G<uint8_t>(0x6000+0x340)=1;animation.running=false;model.Go(Page::Home);
    for(unsigned row:{0u,1u,3u,6u,8u,24u})for(float authored_y:{188.f,216.f,244.f}) {
        selection=row;Quad(52,authored_y,64,64);Graphic(ctx,0x6000,0x59FB5186,0,2);
        const float y=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14));
        check(std::abs(y-(188+24*(row%8)))<.01f,"The ornament pointer must align with all eight home rows after every native submenu return");
        check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x24))-y-64)<.01f,"Pointer alignment must preserve the original sprite height");
    }
    raw::Active(true);raw::Controller("",0);release=false;selection=0;
    raw::Controller("",0x4000);Process(ctx,0,0);Process(ctx,0,0);Process(ctx,0x4000,0);
    check(selection==1,"A raw controller edge followed by its native menu edge must advance only one row");
    Process(ctx,0,0);Process(ctx,0x4000,0);
    check(selection==2,"Native held-direction repeat events must remain available after the initial duplicate edge is consumed");
    raw::Controller("",0);Process(ctx,0,0);raw::Controller("",0x4000);Process(ctx,0x4000,0);
    check(selection==3,"A second controller press must advance the following row after the neutral frame");
    raw::Controller("",0);Process(ctx,0,0);model.Go(Page::Language);model.Edit().language="en";selection=0;release=false;
    raw::Controller("",0x2000);Process(ctx,0,0);Process(ctx,0x2000,0);
    check(model.Draft().language=="fr","A language change must not skip an autonym when both input routes report the same press");
    raw::Controller("",0);Process(ctx,0,0);
    model.Go(Page::Controller);selection=0;release=false;
    raw::Begin(raw::Device::Controller);raw::Controller("",0);raw::Controller("b",0);
    auto count=sounds.size();Process(ctx,0,0);
    check(model.Draft().bindings[0]=="a" && notice=="settings.shared" && sounds.size()==count+1 && sounds.back()==0x100300,
          "Rejecting an occupied captured controller input must preserve A, show the conflict and play the native back sound");
    raw::Begin(raw::Device::Controller);raw::Controller("",0);raw::Controller("touchpad",0);
    count=sounds.size();Process(ctx,0,0);
    check(model.Draft().bindings[0]=="touchpad" && sounds.size()==count+1 && sounds.back()==0x100200,"Accepting a free captured binding must use the native confirmation sound");
    raw::Begin(raw::Device::Controller);release=false;count=sounds.size();Process(ctx,0,0x20);
    check(sounds.size()==count+1 && sounds.back()==0x100300,"Cancelling input capture must use the native back sound");
    trial=true;trial_until=Clock::now()+std::chrono::seconds(15);release=false;count=sounds.size();Process(ctx,0,0x40);
    check(sounds.size()==count+1 && sounds.back()==0x100200,"Keeping a display trial must use the native confirmation sound");
    // A language restart must be prepared before persistence and committed only after a successful write.
    for(const auto* from:{"en","fr","de","es","it","pt-BR"})for(const auto* to:{"en","fr","de","es","it","pt-BR"}) {
        settings={};settings.language=from;model.Open(settings);model.Edit().language=to;model.Edit().master_volume=35;
        closing=false;notice.clear();prepared=committed=cancelled=persisted=0;save_order.clear();
        check(Save(ctx),"Saving a valid draft must succeed for every original and installed language");
        const bool changed=std::string_view(from)!=to;
        check(prepared==unsigned(changed) && committed==unsigned(changed) && cancelled==0,
              "Every changed language, including Italian and PT-BR, must restart once; unchanged languages must stay in the same process");
        check(persisted==1 && last_saved.language==to && last_saved.master_volume==35 && settings.language==from,
              "The complete draft must be saved while the old session retains its active resource language until shutdown");
        if(changed)check(save_order==std::vector<std::string>{"prepare","persist","commit"},
                         "A restart must never close the game before preflight and configuration persistence finish");
    }
    for(const auto* text:openshao::launcher::languages)for(const auto* from:openshao::launcher::voice_languages)for(const auto* to:openshao::launcher::voice_languages) {
        settings={};settings.language=text;settings.voice_language=from;model.Open(settings);model.Edit().voice_language=to;
        prepared=committed=cancelled=persisted=0;closing=false;save_order.clear();
        const bool changed=std::string_view(from)!=to;
        check(Save(ctx) && prepared==unsigned(changed) && committed==unsigned(changed) && persisted==1 &&
              last_saved.language==text && last_saved.voice_language==to && settings.voice_language==from,
              "All text languages must support every voice choice, retain the active voice until shutdown and avoid redundant restarts");
    }
    settings={};settings.language="es";settings.voice_language="fr";model.Open(settings);model.Edit().voice_language="en";
    prepared=committed=cancelled=persisted=0;closing=false;save_order.clear();
    check(Save(ctx) && prepared==1 && committed==1 && persisted==1 && last_saved.language=="es" && last_saved.voice_language=="en",
          "Changing only voices must preserve Spanish text, persist both selections and restart once");
    settings={};model.Open(settings);model.Edit().language="it";prepare_fails=true;
    prepared=committed=cancelled=persisted=0;closing=false;save_order.clear();
    check(!Save(ctx) && prepared==1 && persisted==0 && committed==0 && model.Dirty() && !closing,
          "A failed child preflight must keep the original session and unsaved draft without writing settings");
    prepare_fails=false;persist_fails=true;prepared=committed=cancelled=persisted=0;
    check(!Save(ctx) && prepared==1 && committed==0 && cancelled==1 && model.Dirty() && !closing,
          "A failed settings write must cancel the prepared child without shutting down the active game");
    persist_fails=false;commit_fails=true;prepared=committed=cancelled=persisted=0;
    check(!Save(ctx) && persisted==1 && committed==1 && cancelled==1 && !closing,
          "A failed shutdown request must cancel the waiting child and leave the active game usable");
    commit_fails=false;prepared=committed=cancelled=persisted=0;
    check(Save(ctx) && prepared==1 && committed==1,
          "Saving again after a failed shutdown must retry the still-pending language change");
    settings={};model.Open(settings);model.Edit().language="de";model.Edit().aspect_ratio="16:9";
    model.Go(Page::Home);selection=5;release=false;closing=false;trial=false;
    prepared=committed=cancelled=persisted=0;Process(ctx,0,0x40);
    check(trial && prepared==0 && persisted==0,"A combined display and language change must wait for the display confirmation before preparing any restart");
    release=false;Process(ctx,0,0x20);
    check(!trial && prepared==0 && committed==0 && persisted==0 && settings.aspect_ratio=="original",
          "Rejecting the display trial must restore the display without saving or restarting for the pending language");
    model.Edit().aspect_ratio="16:9";release=false;Process(ctx,0,0x40);
    check(trial && prepared==0 && persisted==0,"Retrying the combined draft must still wait for confirmation");
    release=false;Process(ctx,0,0x40);
    check(!trial && prepared==1 && persisted==1 && committed==1 && closing &&
          last_saved.language=="de" && last_saved.aspect_ratio=="16:9",
          "Confirming the combined display and language draft must save both changes and commit exactly one restart");
    // Exit is a separate confirmation, never a settings save or language restart.
    settings={};model.Open(settings);model.Go(Page::Enhancements);selection=0;release=false;closing=false;trial=false;animation.running=false;
    Process(ctx,0,0x80);
    check(previews==std::vector<bool>{false} && !model.Dirty(),"X must preview the normal pair without changing or saving the draft");
    release=false;Process(ctx,0x2000,0);release=false;Process(ctx,0,0x80);
    check(previews==std::vector<bool>{false,true},"The preview must follow the unsaved Scorpion toggle");
    preview_succeeds=false;release=false;Process(ctx,0,0x80);
    check(notice=="settings.preview_error" && !closing,"A preview failure must keep the settings screen usable");
    const auto stopped=preview_stops;release=false;Process(ctx,0,0x20);
    check(!closing && selection==4 && notice=="settings.unsaved" && preview_stops==stopped,
          "Unsaved Enhancements must select Save Changes before leaving");
    release=false;Process(ctx,0x4000,0);release=false;Process(ctx,0,0x40);
    check(closing && preview_stops>stopped,
          "Discarding standalone Enhancements must stop its preview and close back to the main menu");
    preview_succeeds=true;
    settings={};model.Open(settings);model.Edit().scorpion_unfiltered=true;native_preferences_applied=0;
    check(Save(ctx) && native_preferences_applied==0,
          "Saving only Scorpion voice frequency must not reconfigure the original guest music or effects volumes");
    settings={};settings.music_volume=90;
    auto native_active=settings;native_active.music_volume=75;native_active.effects_volume=75;
    model.Open(native_active);model.Edit().scorpion_unfiltered=true;native_preferences_applied=0;
    check(Save(ctx) && native_preferences_applied==0,
          "Saving enhancements must leave active guest audio untouched even when host defaults differ from the retail menu values");
    settings={};settings.language="pt-BR";model.Open(settings);model.Go(Page::Audio);selection=1;
    animation.running=false;closing=false;release=false;frame={};
    frame[0]={true,true,true,0,0,93,211,239,254,254};
    captions.clear();Draw(ctx);
    bool output_selected=false,stereo_selected=false;
    for(const auto& caption:captions) {
        output_selected|=caption.text=="Saída de áudio" && caption.y==235 && caption.alpha==254;
        stereo_selected|=caption.text=="Estéreo" && caption.y==235 && caption.alpha==254;
    }
    check(output_selected && stereo_selected && SelectorY()==212,
          "Audio Output and its localized mode must share the selected row's highlight and native selector position");
    model.Go(Page::Language);selection=0;notice="settings.pack_required";
    captions.clear();Draw(ctx);
    bool missing_pack_visible=false;
    for(const auto& caption:captions)
        missing_pack_visible|=caption.text==openshao::menu::Catalog::Builtin().Get("pt-BR","settings.pack_required");
    check(missing_pack_visible,"Language screen must show the missing-pack warning instead of hiding it behind the restart hint");
    notice.clear();captions.clear();Draw(ctx);
    bool restart_hint_visible=false;
    for(const auto& caption:captions)
        restart_hint_visible|=caption.text==openshao::menu::Catalog::Builtin().Get("pt-BR","settings.restart");
    check(restart_hint_visible,"Language screen must retain its restart hint when no error notice exists");
    model.Go(Page::Audio);selection=1;
    Process(ctx,0x2000,0);
    check(model.Draft().audio_output==2 && applied_audio_modes.empty(),"Editing output must remain a draft until Save");
    persist_fails=true;
    check(!Save(ctx) && applied_audio_modes.empty(),"A failed settings write must not apply the new output mode");
    persist_fails=false;
    check(Save(ctx) && applied_audio_modes==std::vector<int>{2} && last_saved.audio_output==2 && native_preferences_applied==0,
          "Saving Audio Output must apply the selected layout without resetting retail audio volumes");
    check(Save(ctx) && applied_audio_modes.size()==1,"An unchanged save must not reapply audio routing");
    model.Go(Page::Audio);model.Change(1,1);model.Discard();
    check(model.Draft().audio_output==2 && applied_audio_modes.size()==1,"Discard must preserve the saved output mode and current routing");
    // Successful Save must close through the retail Options teardown; an
    // unsuccessful write must keep the custom screen active for a retry.
    settings={};model.Open(settings);model.Edit().scorpion_unfiltered=true;
    model.Go(Page::Home);selection=5;release=false;closing=false;trial=false;exit_prompt=false;animation.running=false;
    native_close_calls.clear();const auto first_save=persisted;const auto first_stops=preview_stops;
    Process(ctx,0,0x40);
    check(closing && persisted==first_save+1 && last_saved.scorpion_unfiltered &&
          native_close_calls==std::vector<uint32_t>{0x14CFA,0x166DB,0x11EAB} && preview_stops==first_stops+1,
          "Saving an enhancement must persist it and immediately run native Options teardown");
    model.Open(settings);model.Edit().scorpion_unfiltered=false;selection=5;
    closing=false;release=false;persist_fails=true;native_close_calls.clear();
    const auto rejected_save=persisted;
    Process(ctx,0,0x40);
    check(!closing && persisted==rejected_save && model.Dirty() && native_close_calls.empty(),
          "A failed save must keep the unsaved settings screen open");
    persist_fails=false;
    // Display changes first require user confirmation, and then close only
    // after the confirmed setting has been persisted.
    settings={};model.Open(settings);model.Edit().aspect_ratio="16:9";
    selection=5;closing=false;release=false;trial=false;native_close_calls.clear();
    Process(ctx,0,0x40);
    check(trial && !closing && native_close_calls.empty(),
          "A display change must not close before the confirmation");
    const auto confirmed_save=persisted;
    release=false;Process(ctx,0,0x40);
    check(!trial && closing && persisted==confirmed_save+1 && last_saved.aspect_ratio=="16:9" &&
          native_close_calls==std::vector<uint32_t>{0x14CFA,0x166DB,0x11EAB},
          "Confirming a display change must save and close through native Options");
    native_close_calls.clear();exit_prompt=false;closing=false;
    Close(ctx);
    check(native_close_calls==std::vector<uint32_t>{0x14CFA,0x166DB,0x11EAB},
          "Returning from custom Options must stop its native sound and finalize retail Options state before restarting main menu music");
    settings={};model.Open(settings);active=closing=false;trial=false;extra_selected=true;animation.running=false;
    kernel::W32(0x6C930C,0x598430);
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x598468,8);kernel::W32(0x4000,0);kernel::W32(0x4004,0x40);
    MainInput(ctx,memory.data());
    check(Active() && exit_prompt && Snapshot().title=="main.exit_confirm" && Snapshot().rows.size()==2 && selection==0 &&
          Snapshot().rows[0].key=="system.no" && Snapshot().rows[1].key=="system.yes",
          "Exit must open the menu confirmation with No selected by default");
    release=false;Process(ctx,0,0x40);
    check(closing && exit_requests==0,"Accepting the default No must return without requesting shutdown");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0);MainInput(ctx,memory.data());
    check(!Active() && kernel::R32(0x598468)==8,"Cancelling Exit must restore the Exit main-menu selection");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0x40);MainInput(ctx,memory.data());
    release=false;Process(ctx,0,0x20);
    check(closing && exit_requests==0,"B must cancel Exit without shutting down");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0);MainInput(ctx,memory.data());
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0x40);MainInput(ctx,memory.data());
    release=false;Process(ctx,0x4000,0);exit_succeeds=false;release=false;Process(ctx,0,0x40);
    check(exit_requests==1 && !closing && notice=="main.exit_error","A failed close request must keep the confirmation usable");
    const auto prior_saves=persisted,prior_restarts=prepared;
    exit_succeeds=true;release=false;Process(ctx,0,0x40);
    check(exit_requests==2 && closing && persisted==prior_saves && prepared==prior_restarts,
          "Confirming Yes must request a normal shutdown without saving a draft or restarting the game");
    return failures?1:0;
}

