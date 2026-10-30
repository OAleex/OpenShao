#include "kernel/kernel.h"
#include "../src/recomp_settings.cpp"
#include <array>
#include <iostream>

static std::array<uint8_t,8<<20> memory{};
static openshao::launcher::Settings settings;
static std::vector<uint32_t> sounds;
static unsigned starts=0;
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
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t* args,size_t count) {
    if(fn==0x1B1240 && count==1) {
        const auto scene=R32(ctx.ecx+12);W32(scene+0x2A0,R32(args[0]));W32(scene+0x33C,R32(scene+0x24+R32(args[0])*16));G<uint8_t>(scene+0x340)=0;++starts;
    } else if(fn==0x100100 || fn==0x100200 || fn==0x100300)sounds.push_back(fn);
    else if(fn==0x14CFA || fn==0x166DB || fn==0x11EAB)native_close_calls.push_back(fn);
    else if(fn!=0x1B29B0 && fn!=0x1B2390)std::abort();
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
    for(unsigned t=0;t<2;++t)for(unsigned v=0;v<3;++v) {
        const auto a=0x63F5E4+t*0x6C;
        kernel::W32(a+0x10+v*16,std::bit_cast<uint32_t>(x+(v==2?width:0)));
        kernel::W32(a+0x14+v*16,std::bit_cast<uint32_t>(y+(v==1?height:0)));
        kernel::W32(a+0x58+v*4,(alpha<<24)|0xFFFFFF);
    }
    kernel::W32(0x63F5E0,2);
}
int main() {
    using namespace openshao::recomp;
    int failures=0;auto check=[&](bool ok,const char* why){if(!ok){++failures;std::cerr<<why<<'\n';}};
    X86Context ctx{};ctx.esp=0x3000;ctx.ecx=0x598430;
    kernel::W32(0x59843C,0x5000);kernel::W32(0x5000+0x2A0,6);kernel::W32(0x5000+0x33C,77);
    kernel::W32(0x598468,5);kernel::W32(ctx.esp+4,0x4000);kernel::W32(ctx.esp+8,0x4004);
    kernel::W32(0x6CA3C0,0x100100);kernel::W32(0x6CA3B8,0x100200);kernel::W32(0x6CA3BC,0x100300);
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
    Quad(90,316,256,60,76);Graphic(ctx,0x5000,0xF063C950,0,2);
    check((kernel::R32(0x63F5E4+0x58)>>24)==254,"Selecting Kontent must not inherit its previous unselected animation opacity");
    extra_selected=true;Quad(90,316,256,60,254);Graphic(ctx,0x5000,0xF063C950,0,2);
    check((kernel::R32(0x63F5E4+0x58)>>24)==76,"Selecting the extra row must not flash the borrowed Kontent highlight");
    Quad(20,310,128,120);Graphic(ctx,0x5000,0,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-398.f)<.01f,
          "The nine-row main menu must move the carved ornament cap down by three native row steps");
    Quad(31,320,64,60);Graphic(ctx,0x5000,0,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-408.f)<.01f,
          "The main ornament joint must follow the taller shaft without stretching its carved texture");
    Quad(31,305.6f,64,56);Graphic(ctx,0x5000,0,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-305.6f)<.01f &&
          std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x24))-449.6f)<.01f,
          "The ornament shaft must extend to the third added row while keeping its original top fixed");
    kernel::W32(0x598468,8);Quad(52,188,64,64);Graphic(ctx,0x5000,0x59FB5186,0,2);
    check(std::abs(std::bit_cast<float>(kernel::R32(0x63F5E4+0x14))-272.f)<.01f,
          "Exit pointer must track the ninth menu row with native 28-pixel spacing");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4000,0);kernel::W32(0x4004,0x40);
    kernel::W32(0x598468,7);kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x598430);
    check(MainInput(ctx,memory.data()) && Active() && model.CurrentPage()==Page::Enhancements &&
          Snapshot().rows.size()==5 && !exit_prompt,
          "The new main-menu Enhancements row must open its standalone list rather than Exit");
    ctx.esp=0x3000;ctx.ecx=0x5985E8;release=false;Process(ctx,0,0x20);
    check(closing,"Leaving unchanged Enhancements must close the native Options scene");
    ctx.esp=0x3000;ctx.ecx=0x598430;kernel::W32(0x4004,0);MainInput(ctx,memory.data());
    check(!Active() && kernel::R32(0x598468)==7,"Closing Enhancements must restore its main-menu row");
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
    for(const auto [page,extension]:{std::pair{Page::Home,72.f},std::pair{Page::Display,24.f},std::pair{Page::Graphics,48.f},std::pair{Page::Controls,24.f},std::pair{Page::Controller,72.f},std::pair{Page::Keyboard,72.f},std::pair{Page::KeyboardAlternate,72.f},std::pair{Page::Mouse,72.f},std::pair{Page::Audio,-72.f},std::pair{Page::Language,-48.f},std::pair{Page::Enhancements,24.f}}) {
        model.Go(page);selection=0;Quad(20,250,128,120);Graphic(ctx,0x6000,0,0,2);
        const float top=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14)),bottom=std::bit_cast<float>(kernel::R32(0x63F5E4+0x24));
        check(std::abs(top-250-extension)<.01f && std::abs(bottom-top-120)<.01f,"Every settings category must size its ornament to the visible rows while preserving the carved cap proportions");
    }
    model.Go(Page::Audio);Quad(31,249.3f,64,56);Graphic(ctx,0x6000,0,0,2);
    const float shaft_top=std::bit_cast<float>(kernel::R32(0x63F5E4+0x14)),shaft_bottom=std::bit_cast<float>(kernel::R32(0x63F5E4+0x24));
    check(std::abs(shaft_top-225.3f)<.01f && std::abs(shaft_bottom-233.3f)<.01f,"The one-row audio ornament must retain an upright shaft instead of folding its geometry");
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
    check(!closing && selection==3 && notice=="settings.unsaved" && preview_stops==stopped,
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

