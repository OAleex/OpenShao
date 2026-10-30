#include "ed_nexus.h"
#include "pause_recomp.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <unordered_map>

struct X86Context { uint32_t esp=0x1000,eax=0,ecx=0; };
namespace openshao::ed_nexus { namespace { bool EnsureMenuStorage(); } }
std::unordered_map<uint32_t,uint32_t> memory;
void Check(bool value,const char* text) { if(!value) {std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);} }
unsigned opens=0,sounds=0,updates=0,builds=0,native_pause_events=0;
unsigned toggles=0,desktop_exits=0;
unsigned original_cheats=0;
namespace mem {uint32_t AllocSystem(uint32_t size){static uint32_t next=0x8000;Check(size<=2048,"pause menus use bounded guest allocations");const auto result=next;next+=0x1000;return result;}}
namespace openshao::pause_recomp {
Page current=Page::Categories;
static bool portuguese_debug=false;
std::string Localized(std::string_view id) {
    if(portuguese_debug) {
        if(id=="debug.menu")return std::string("Menu de Depura\xE7\xE4""o");
        if(id=="debug.unlock_kontent")return std::string("Desbloquear Konte\xFA""do");
        if(id=="debug.kontent_unlocked")return std::string("Konte\xFA""do desbloqueado");
    }
    if(id=="debug.menu")return "Debug Menu";
    if(id=="debug.unlock_kontent")return "Unlock All Kontent";
    if(id=="debug.kontent_unlocked")return "Kontent Unlocked";
    Check(false,"every debug caption uses a known localized catalog key");
    return {};
}
std::string Text(Item item) {
    if(item==Item::ShowFps)return "FPS "+std::to_string(toggles);
    return "Native Menu " +std::to_string(uint32_t(item));
}
bool Toggle(Item item){Check(item>=Item::ShowFps && item<=Item::Bloom,"only safe pause toggles");++toggles;return true;}
bool Exit(){++desktop_exits;return true;}
void Open(){current=Page::Categories;}
void Close(){current=Page::Categories;}
Page CurrentPage(){return current;}
unsigned RowCount(){return current==Page::Categories?5:current==Page::Controls?6:current==Page::Graphics?7:current==Page::Display?4:2;}
std::string RowText(unsigned row){return "Entry "+std::to_string(unsigned(current))+" "+std::to_string(row)+" " +std::to_string(toggles);}
bool Choose(unsigned row){
    if(current==Page::Categories){
        if(row==4)return true;
        current=std::array{Page::Display,Page::Graphics,Page::Controls,Page::Audio}[row];return false;
    }
    if(row==RowCount()-1){current=Page::Categories;return false;}
    ++toggles;return false;
}
bool Back(){if(current==Page::Categories)return true;current=Page::Categories;return false;}
bool Update(){return false;}
bool Capturing(){return false;}
}
void sub_000160AF(X86Context&,uint8_t*);
void sub_00011F64(X86Context&,uint8_t*);
void sub_00013FCB(X86Context&,uint8_t*);
void sub_00013751(X86Context&,uint8_t*);
void sub_00015C6D(X86Context&,uint8_t*);
void sub_00016992(X86Context&,uint8_t*);
static void CheckKontent(bool unlocked) {
    struct Query {void (*fn)(X86Context&,uint8_t*);unsigned count;};
    for(const auto q:{Query{sub_000160AF,64},Query{sub_00011F64,176},Query{sub_00013FCB,8},Query{sub_00013751,16}}) {
        for(unsigned i=0;i<=q.count;++i) {
            X86Context ctx;ctx.ecx=0x4567;memory[ctx.esp+4]=i;q.fn(ctx,nullptr);
            Check(ctx.eax==(0xAABBCC00u|unsigned(unlocked && i<q.count)) && ctx.esp==0x1004 && ctx.ecx==0x4567,
                  "Kontent unlock preserves every valid query ABI and leaves out-of-range requests unchanged");
        }
    }
    X86Context mk2;sub_00015C6D(mk2,nullptr);
    Check(mk2.eax==(0xAABBCC00u|unsigned(unlocked)) && mk2.esp==0x1004,"MKII follows the same session unlock");
}
void sub_000135C6(X86Context&,uint8_t*);
namespace kernel {
void Return(X86Context& ctx,uint32_t value,int args){ctx.eax=value;ctx.esp+=4+args*4;}
uint32_t R32(uint32_t address) { return memory[address]; }
void W32(uint32_t address,uint32_t value) { memory[address]=value; }
uint32_t CallGuest(X86Context& ctx,uint32_t target,std::initializer_list<uint32_t> args) {
    uint32_t result=0;
    if(target==0x135C6) {Check(args.size()==0,"Nexus opener keeps its no-argument ABI");sub_000135C6(ctx,nullptr);}
    else if(target==0x1120d) {
        Check(ctx.ecx==0x4ef3c8 && args.size()==1,"menu lookup retains the original GUI ABI");
        if(*args.begin()==0x49f924) result=0x4000;
        else {Check(*args.begin()==0x49f8fc,"Nexus lookup uses the native native list");result=0x6000;}
    }
    else if(target==0x151560) {
        ++sounds;
        Check(ctx.ecx==0x653bc0 && args.size()==8,"acknowledgement uses the original sound manager and ABI");
        Check(*args.begin()==0x5ce79082,"only the Nexus unlock requests vo_sk_combo_counter_190_1");
    }
    else Check(false,"unexpected guest call");
    ctx.esp=0xdead;ctx.eax=0xbeef;ctx.ecx=0xcafe;return result;
}
}
static std::string GuestText(uint32_t address) {
    std::string result;
    for(unsigned i=0;i<24;++i) {
        const uint32_t value=memory[(address+i)&~uint32_t(3)];
        const char glyph=char(value >> ((i%4)*8));
        if(!glyph)return result;
        result.push_back(glyph);
    }
    Check(false,"localized debug text must have a terminator inside its own storage");
    return {};
}
#define X86_FUNC(name) void name(X86Context& ctx,uint8_t* base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
#define LOGI(...) ((void)0)
X86_FUNC(sub_000142EB);
X86_FUNC(sub_00013B4D);
X86_FUNC(sub_00015687);
X86_FUNC(sub_0001332D);
X86_FUNC(__imp__sub_000142EB) { ++updates;ctx.eax=0x1234;ctx.ecx=0x5678;ctx.esp+=4; }
X86_FUNC(__imp__sub_00013B4D) { ++builds;ctx.esp+=4; }
X86_FUNC(__imp__sub_00015687) { ++native_pause_events;ctx.eax=0xCC00;ctx.esp+=4; }
X86_FUNC(__imp__sub_001B5300) { ++original_cheats;ctx.esp+=4; }
X86_FUNC(__imp__sub_000135C6) {
    ++opens;ctx.esp+=4;
    const uint32_t entries[]={0x18b,0x139c7,0x19d,0x139a4,0x1a0,0x14ed0,0x666,0x12216,
                              0x660,0x16a5a,0x661,0x12c20,0x1a6,0x16677,0xffffffff,0};
    for(unsigned i=0;i<16;++i)memory[0x4f1f00+i*4]=entries[i];
    memory[0x6054]=0x4f1f00;memory[0x6064]=7;memory[0x606c]=0;
}
X86_FUNC(__imp__sub_000160AF){ctx.eax=0xAABBCC00;ctx.esp+=4;}
X86_FUNC(__imp__sub_00011F64){ctx.eax=0xAABBCC00;ctx.esp+=4;}
X86_FUNC(__imp__sub_00013FCB){ctx.eax=0xAABBCC00;ctx.esp+=4;}
X86_FUNC(__imp__sub_00013751){ctx.eax=0xAABBCC00;ctx.esp+=4;}
X86_FUNC(__imp__sub_00015C6D){ctx.eax=0xAABBCC00;ctx.esp+=4;}
X86_FUNC(__imp__sub_00016992){ctx.eax=0x1234;ctx.esp+=4;}
X86_FUNC(__imp__sub_0001332D) {
    const uint32_t source[]={0x1a4,0x15263,0x1a5,0x11bcc,0x1a7,0x16262,0xffffffff,0};
    const uint32_t other[]={0x1a3,0x15506,0x1a4,0x15263,0x1a5,0x11bcc,0x1a7,0x16262,0xffffffff,0};
    for(unsigned i=0;i<8;++i) {
        memory[0x4f1cc0+i*4]=source[i];
        memory[0x5000+i*4]=source[i]; // The real builder copies source into an owned list buffer.
    }
    for(unsigned i=0;i<10;++i)memory[0x4f1ce0+i*4]=other[i];
    memory[0x4054]=0x5000;memory[0x4064]=3;
    ctx.esp+=4;ctx.eax=0x12345678;
}
bool exercise_pause=false,exercise_locked_pause=false;
X86_FUNC(__imp__sub_00012BA8) {
    if(exercise_locked_pause || exercise_pause) {
        X86Context list;sub_0001332D(list,nullptr);
        Check(list.esp==0x1004 && list.eax==0x12345678,"adding a pause entry preserves the original builder ABI");
        Check(memory[0x4f1cc0]==0x1a4 && memory[0x4f1cc4]==0x15263 &&
              memory[0x4f1cc8]==0x1a5 && memory[0x4f1ccc]==0x11bcc &&
              memory[0x4f1cd0]==0x1a7 && memory[0x4f1cd4]==0x16262 &&
              memory[0x4f1cd8]==0xffffffff,"Continue, Options and Exit Game keep their original source definition");
        if(exercise_locked_pause) {
            const auto rows=memory[0x4054];
            Check(memory[0x4064]==5 && rows!=0x5000 && memory[rows+40]==0xffffffff,
                  "locked gameplay pause adds only settings and exit, preserving its native rows");
            Check(memory[rows+16]==0xffffff00 && memory[rows+24]==0x1a7 && memory[rows+28]==0x16262,
                  "pause settings must precede Exit Game while retaining the native return-to-main-menu callback");
        }
    }
    if(exercise_pause) {
        const auto rows=memory[0x4054];
        Check(memory[0x4064]==6 && rows!=0x5000 && memory[rows+40]==0x19f &&
              memory[rows+44]==0x135c6 && memory[rows+48]==0xffffffff,
              "unlocked pause adds settings, desktop exit, Nexus and a terminator in owned storage");
        for(unsigned i=0;i<4;++i)Check(memory[rows+i*4]==memory[0x5000+i*4],
              "root additions preserve original Continue and Options callbacks");
        Check(memory[rows+16]==0xffffff00 && memory[rows+24]==memory[0x5010] && memory[rows+28]==memory[0x5014],
              "Exit Game follows settings with its original label and callback");
        Check(memory[0x4f1cd8]==0xffffffff && memory[0x4f1ce0]==0x1a3 &&
              memory[0x4f1cf8]==0x1a7 && memory[0x4f1d00]==0xffffffff,
              "native three-item source and unrelated four-item definition are never overwritten");
        auto event=[](unsigned selected,unsigned event_id) {
            X86Context result;memory[result.esp+4]=0x4000;memory[result.esp+8]=event_id;
            memory[0x406c]=selected;
            sub_00015687(result,nullptr);
            Check(result.esp==0x1004 && result.eax==0,"custom pause action preserves native event ABI");
        };
        event(2,4);
        Check(memory[0x4064]==5 && memory[0x4054]!=rows && memory[0x406c]==0,
              "recompilation settings opens the shared display, graphics, controls and audio groups");
        X86Context no_nexus;no_nexus.ecx=0x4ef3c8;memory[0x67c66a]=0x80;
        sub_000142EB(no_nexus,nullptr);
        Check(opens==0,"Nexus X shortcut does not interrupt pause recompilation settings");
        auto submenu_rows=memory[0x4054];
        X86Context fps_label;memory[fps_label.esp+4]=memory[submenu_rows];sub_00016992(fps_label,nullptr);
        Check(memory[fps_label.eax]==0x72746e45,
              "native label lookup exposes the first native pause category");
        event(2,4);
        Check(memory[0x4064]==6 && openshao::pause_recomp::CurrentPage()==openshao::pause_recomp::Page::Controls,
              "controls category opens with controller, keyboard, mouse, deadzone and rumble");
        event(3,4);event(4,4);
        Check(toggles==2,"deadzone and rumble options dispatch through the native menu");
        event(5,4);
        Check(memory[0x4064]==5 && openshao::pause_recomp::CurrentPage()==openshao::pause_recomp::Page::Categories,
              "return from controls restores category navigation");
        event(1,4);
        Check(memory[0x4064]==7,"graphics exposes all six shared controls plus Back");
        const auto graphics_rows=memory[0x4054];
        Check(memory[graphics_rows+7*8]==0xffffffff && memory[graphics_rows+7*8+4]==0,
              "the seventh graphics row retains its own sentinel within expanded guest storage");
        X86Context last_label;memory[last_label.esp+4]=memory[graphics_rows+6*8];sub_00016992(last_label,nullptr);
        Check(memory[last_label.eax]==0x72746e45,"the seventh native row has valid owned text storage");
        event(0,4);event(1,4);event(2,4);
        Check(toggles==5,"three live graphics settings dispatch in place");
        event(6,4);
        event(0,4);Check(memory[0x4064]==4,"display category exposes screen controls and Back");
        event(3,4);
        event(3,4);Check(memory[0x4064]==2,"audio category exposes volume and back");
        event(1,4);
        event(4,4);
        Check(memory[0x4054]==rows && memory[0x4064]==6 && memory[0x406c]==2,
              "Return action restores the original pause list and selection");
        event(4,4);
        Check(memory[0x4064]==2 && memory[0x406c]==0 && memory[0x4054]!=rows,
              "desktop exit confirmation defaults to No and displays the unsaved-progress warning");
        event(0,4);
        Check(desktop_exits==0 && memory[0x4054]==rows,"No returns to pause without exiting");
        event(4,4);event(1,4);
        Check(desktop_exits==1,"only explicitly confirming Yes requests desktop exit");
        X86Context cancel;memory[cancel.esp+4]=0x4000;memory[cancel.esp+8]=6;
        sub_00015687(cancel,nullptr);
        Check(cancel.esp==0x1004 && memory[0x4054]==rows && memory[0x406c]==4,
              "cancel leaves desktop confirmation and returns to pause");
        event(2,4);event(2,4);
        Check(openshao::pause_recomp::CurrentPage()==openshao::pause_recomp::Page::Controls,"Back fixture starts inside controls");
        cancel={};memory[cancel.esp+4]=0x4000;memory[cancel.esp+8]=6;
        sub_00015687(cancel,nullptr);
        Check(memory[0x4064]==5 && openshao::pause_recomp::CurrentPage()==openshao::pause_recomp::Page::Categories,
              "custom settings Back event returns to categories before leaving the native menu");
        cancel={};memory[cancel.esp+4]=0x4000;memory[cancel.esp+8]=6;
        sub_00015687(cancel,nullptr);
        Check(memory[0x4054]==rows,"second Back event restores the native pause root");
        event(2,4);event(2,4);
        const auto before_b=native_pause_events;
        X86Context submenu_b;memory[submenu_b.esp+4]=0x4000;memory[submenu_b.esp+8]=8;
        sub_00015687(submenu_b,nullptr);
        Check(submenu_b.eax==1 && submenu_b.esp==0x1004 &&
              openshao::pause_recomp::CurrentPage()==openshao::pause_recomp::Page::Categories &&
              native_pause_events==before_b,
              "B inside controls navigates to categories without closing gameplay pause");
        submenu_b={};memory[submenu_b.esp+4]=0x4000;memory[submenu_b.esp+8]=8;
        sub_00015687(submenu_b,nullptr);
        Check(submenu_b.eax==1 && memory[0x4054]==rows && native_pause_events==before_b,
              "B from categories restores native pause root without leaving gameplay");
        // The pause root uses a distinct retail event handler from the Nexus list.
        const auto pause_events=native_pause_events;
        X86Context original;memory[original.esp+4]=0x4000;memory[original.esp+8]=4;
        memory[0x406c]=0;sub_00015687(original,nullptr);
        Check(native_pause_events==pause_events+1 && original.eax==0xCC00 && original.esp==0x1004,
              "Continue dispatches to the real native Select-list handler");
        original={};memory[original.esp+4]=0x4000;memory[original.esp+8]=4;
        memory[0x406c]=3;sub_00015687(original,nullptr);
        Check(native_pause_events==pause_events+2 && original.eax==0xCC00 && original.esp==0x1004,
              "Exit Game in its new row still dispatches to the native handler");
        original={};memory[original.esp+4]=0x4000;memory[original.esp+8]=6;
        sub_00015687(original,nullptr);
        Check(native_pause_events==pause_events+3 && original.eax==0xCC00,
              "Back from the native pause root retains the game's original close handler");
        original={};memory[original.esp+4]=0x4000;memory[original.esp+8]=8;
        sub_00015687(original,nullptr);
        Check(native_pause_events==pause_events+4 && original.eax==0xCC00,
              "native B event 8 is delegated, preserving original pause-exit behavior");
        X86Context name;memory[name.esp+4]=0x19f;sub_00016992(name,nullptr);
        Check(GuestText(name.eax)=="Debug Menu" && name.esp==0x1004,
              "the pause entry displays localized Debug Menu with a terminated guest string and preserves its lookup ABI");
        openshao::pause_recomp::portuguese_debug=true;
        Check(openshao::ed_nexus::EnsureMenuStorage(),"debug menu allocation remains valid after changing languages");
        Check(GuestText(name.eax)==std::string("Menu de Depura\xE7\xE4""o"),
              "same native label storage refreshes to Portuguese, preserving accents and original lookup pointer");
        openshao::pause_recomp::portuguese_debug=false;
        Check(openshao::ed_nexus::EnsureMenuStorage() && GuestText(name.eax)=="Debug Menu",
              "the localized debug label can switch back to English without rebuilding the native menu");
        X86Context selected;sub_000135C6(selected,nullptr);
        Check(opens==1 && selected.esp==0x1004,"selecting the pause entry opens the Nexus");
        X86Context update;update.ecx=0x4ef3c8;memory[0x67c66a]=0x80;
        sub_000142EB(update,nullptr);
        Check(opens==1,"the X shortcut does not reopen Nexus after selecting its pause entry");
        Check(update.esp==0x1004 && update.eax==0x1234 && update.ecx==0x5678,"extra guest call preserves the original update result and stack");
        update.ecx=0x4ef3c8;sub_000142EB(update,nullptr);
        Check(opens==1,"X in Nexus does not recreate the menu");
        X86Context back;memory[back.esp+8]=6;sub_00013B4D(back,nullptr);
        Check(builds==1 && back.esp==0x1004,"returning from Nexus retains the original cancel handler");
        update.ecx=0x4ef3c8;sub_000142EB(update,nullptr);
        Check(opens==2,"Nexus can be opened again after returning to the normal pause");
        Check(memory[0x6064]==8 && memory[0x6054]!=0x4f1f00,
              "unlocked Nexus appends one Kontent action in an owned list");
        const uint32_t list=memory[0x6054];
        for(unsigned i=0;i<14;++i)Check(memory[list+i*4]==memory[0x4f1f00+i*4],"all seven retail Nexus actions remain intact");
        Check(memory[list+64]==0xffffffff && memory[0x4f1f38]==0xffffffff,"owned list and original definition keep separate terminators");
        X86Context label;memory[label.esp+4]=memory[list+56];sub_00016992(label,nullptr);
        Check(GuestText(label.eax)=="Unlock All Kontent" && label.esp==0x1004,
              "the appended action displays localized text and preserves the native lookup ABI");
        CheckKontent(false);
        for(uint32_t address=0x5a375c;address<0x5a3790;address+=4)memory[address]=0xA5310000u|address;
        // Only accepting the added row activates it, never merely opening Nexus.
        memory[0x606c]=7;X86Context action;memory[action.esp+4]=0x6000;memory[action.esp+8]=5;
        sub_00013B4D(action,nullptr);CheckKontent(false);
        action={};memory[action.esp+4]=0x6000;memory[action.esp+8]=4;
        const auto original_builds=builds;sub_00013B4D(action,nullptr);
        Check(action.eax==1 && action.esp==0x1004 && builds==original_builds,
              "the Kontent action handles accept without invoking a retail Nexus callback");
        CheckKontent(true);
        label={};memory[label.esp+4]=memory[list+56];sub_00016992(label,nullptr);
        Check(GuestText(label.eax)=="Kontent Unlocked" && label.esp==0x1004,
              "the completed debug action displays the localized completion text");
        for(uint32_t address=0x5a375c;address<0x5a3790;address+=4)
            Check(memory[address]==(0xA5310000u|address),"session unlock never writes the native save flags or character unlocks");
        action={};memory[action.esp+4]=0x6000;memory[action.esp+8]=4;sub_00013B4D(action,nullptr);
        CheckKontent(true);
        X86Context ordinary;memory[ordinary.esp+4]=0x1a4;sub_00016992(ordinary,nullptr);
        Check(ordinary.eax==0x1234 && ordinary.esp==0x1004,"ordinary localized strings keep the original lookup");
    }
    ctx.eax=0xabcd;ctx.esp+=4;
}
#include "ed_nexus_hooks.inl"

int main() {
    CheckKontent(false);
    exercise_locked_pause=true;X86Context locked_pause;sub_00012BA8(locked_pause,nullptr);exercise_locked_pause=false;
    memory[0x6c9308]=1;memory[0x6c930c]=0x598430;
    memory[0x59843c]=0x2000;memory[0x22e4]=0;
    X86Context input;input.ecx=0x598430;memory[0x1008]=0x3000;
    const auto enter_button = [](uint16_t button) {
        X86Context original;original.ecx=0x6ca3a4;
        memory[0x67c66a]=button;
        const auto before=original_cheats;
        sub_001B5300(original,nullptr);
        Check(original_cheats==before+1 && original.esp==0x1004 && original.ecx==0x6ca3a4,
              "the held-modifier path preserves the original cheat processor and ABI");
        Check(memory[0x67c66a]==button,"Nexus matching does not consume original cheat input");
        X86Context frontend;frontend.ecx=0x598430;
        openshao::ed_nexus::FrontendInput(frontend,nullptr);
    };
    for(auto button:{0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000,0x80,0x20}) {
        memory[0x3000]=button;openshao::ed_nexus::FrontendInput(input,nullptr);
    }
    Check(!openshao::ed_nexus::Unlocked() && sounds==0,"the full code without the modifier stays locked and silent");
    memory[0x67c668]=1;
    for(auto button:{0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000}) enter_button(button);
    memory[0x6c930c]=0x598df0;enter_button(0x80);
    memory[0x6c930c]=0x598430;enter_button(0x20);
    Check(!openshao::ed_nexus::Unlocked(),"another frontend panel interrupts held-code matching");
    for(auto button:{0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000}) enter_button(button);
    memory[0x67c668]=0;memory[0x3000]=0;openshao::ed_nexus::FrontendInput(input,nullptr);
    memory[0x67c668]=1;enter_button(0x80);enter_button(0x20);
    Check(!openshao::ed_nexus::Unlocked() && sounds==0,"releasing the modifier discards a partial code without playing its voice");
    for(auto button:{0x1001,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000,0x80,0x20}) enter_button(button);
    Check(openshao::ed_nexus::Unlocked() && sounds==1,"holding the modifier through all ten entries unlocks once with its dedicated voice");
    Check(input.esp==0x1000 && input.ecx==0x598430 && input.eax==0,"frontend ABI is preserved");
    for(auto button:{0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000,0x80,0x20}) enter_button(button);
    Check(sounds==1,"repeating an already unlocked code does not replay its voice");
    memory[0x67c668]=0;openshao::ed_nexus::FrontendInput(input,nullptr);
    Check(openshao::ed_nexus::Unlocked(),"releasing the modifier after completion preserves the unlock");
    X86Context outside;outside.ecx=0x4ef3c8;memory[0x67c66a]=0x80;
    sub_000142EB(outside,nullptr);
    Check(opens==0 && updates==1,"unlocked gameplay X remains unchanged outside pause");
    exercise_pause=true;X86Context pause;sub_00012BA8(pause,nullptr);
    Check(pause.esp==0x1004 && pause.eax==0xabcd,"pause wrapper preserves original result and stack");
    Check(memory[0x4f1cd8]==0xffffffff,"leaving pause preserves the native source terminator");
    Check(memory[0x4054]==0x5000 && memory[0x4064]==3,
          "leaving pause returns native menu pointers and counts to their original values");
    outside.ecx=0x4ef3c8;sub_000142EB(outside,nullptr);
    Check(opens==2,"leaving pause clears its access context");
    Check(sounds==1,"opening and returning from Nexus do not replay the unlock voice");
    std::puts("PASS: frontend isolation, original pause hooks, repeated access and preserved guest ABI");
}
