#include "ed_nexus_sequence.h"
#include "pause_recomp.h"

namespace openshao::ed_nexus {
namespace {
Sequence sequence;
bool kontent_unlocked = false;
uint32_t kontent_menu = 0;
constexpr uint32_t MenuLabel = 0x19F;
constexpr uint32_t KontentLabel = 0xFFFFFFFE;
constexpr uint32_t KontentRow = 7;
constexpr uint32_t KontentListBytes = 9*8;
constexpr uint32_t DebugTextBytes = 24;
constexpr uint32_t PauseLabelBase = 0xFFFFFF00;
constexpr uint32_t PauseRootBytes = 7*8;
// Six graphics controls plus Back need the largest native Select list.
constexpr uint32_t PauseSettingsBytes = (pause_recomp::MaxRows+1)*8;
constexpr uint32_t PauseExitBytes = 3*8;
constexpr uint32_t PauseListBytes = PauseRootBytes + PauseSettingsBytes + PauseExitBytes;
constexpr uint32_t PauseTextBytes = 64;
constexpr uint32_t PauseTextCount = uint32_t(pause_recomp::Item::Count)+pause_recomp::MaxRows;
constexpr uint32_t PauseSettingsRow = 2, PauseDesktopExitRow = 4;
uint32_t pause_storage = 0;
bool EnsureMenuStorage();
enum class PausePage { Root, Settings, Exit };
struct PauseMenuState {
    uint32_t list = 0, original_rows = 0, original_count = 0;
    uint32_t root_count = 0;
    PausePage page = PausePage::Root;
};
PauseMenuState pause_menu;
void WriteGuestText(uint32_t address, const char* text, unsigned bytes) {
    for (unsigned word=0;word<bytes/4;++word) {
        uint32_t value=0;
        for (unsigned byte=0;byte<4 && *text;++byte) value|=uint32_t(uint8_t(*text++))<<(byte*8);
        kernel::W32(address+word*4,value);
    }
}
void WriteMenuText(uint32_t address,const std::string& text) {
    // The owned guest allocation keeps native text pointers valid across draws.
    WriteGuestText(address,text.c_str(),DebugTextBytes);
}
bool EnsurePauseStorage() {
    if (pause_storage) return true;
    pause_storage=mem::AllocSystem(PauseListBytes+PauseTextCount*PauseTextBytes);
    if (!pause_storage) LOGI("Cannot allocate gameplay pause menu storage");
    return pause_storage!=0;
}
void PauseText() {
    for(uint32_t i=0;i<PauseTextCount;++i) {
        const auto label=i<uint32_t(pause_recomp::Item::Count)?
            pause_recomp::Text(pause_recomp::Item(i)):
            pause_recomp::RowText(i-uint32_t(pause_recomp::Item::Count));
        WriteGuestText(pause_storage+PauseListBytes+i*PauseTextBytes,label.c_str(),PauseTextBytes);
    }
}
void PausePair(uint32_t address,uint32_t label,uint32_t callback=0x135C1) {
    kernel::W32(address,label);
    kernel::W32(address+4,callback);
}
void PauseTerminator(uint32_t address) {PausePair(address,0xFFFFFFFF,0);}
void RefreshSettingsPage(uint32_t row=0) {
    if (!pause_menu.list || !pause_storage)return;
    const unsigned count=pause_recomp::RowCount();
    if (!count || count>pause_recomp::MaxRows)return;
    const uint32_t list=pause_menu.list,rows=pause_storage+PauseRootBytes;
    for(unsigned i=0;i<count;++i)
        PausePair(rows+i*8,PauseLabelBase+uint32_t(pause_recomp::Item::Count)+i);
    PauseTerminator(rows+count*8);
    PauseText();
    kernel::W32(list+0x54,rows);
    kernel::W32(list+0x64,count);
    kernel::W32(list+0x6C,row<count?row:0);
}
void ShowPausePage(PausePage page) {
    if (!pause_menu.list) return;
    const auto list=pause_menu.list;
    const auto previous=pause_menu.page;
    if(previous==PausePage::Settings && page!=PausePage::Settings)pause_recomp::Close();
    pause_menu.page=page;
    if (page==PausePage::Root) {
        kernel::W32(list+0x54,pause_storage);
        kernel::W32(list+0x64,pause_menu.root_count);
        kernel::W32(list+0x6C,previous==PausePage::Exit?PauseDesktopExitRow:PauseSettingsRow);
    } else if (page==PausePage::Settings) {
        pause_recomp::Open();
        RefreshSettingsPage();
    } else {
        PauseText();
        kernel::W32(list+0x54,pause_storage+PauseRootBytes+PauseSettingsBytes);
        kernel::W32(list+0x64,2);
        kernel::W32(list+0x6C,0); // No is the default; Yes explicitly warns about unsaved progress.
    }
}
bool HandlePauseAction(X86Context& ctx,uint32_t list,uint32_t event) {
    if (!pause_menu.list || !pause_storage || list!=pause_menu.list) return false;
    const auto rows=kernel::R32(list+0x54);
    const auto expected=pause_menu.page==PausePage::Root?pause_storage:
        pause_menu.page==PausePage::Settings?pause_storage+PauseRootBytes:
        pause_storage+PauseRootBytes+PauseSettingsBytes;
    if(rows!=expected)return false;
    // Remapping listens to raw SDL/keyboard input independently of the guest.
    // Don't let the same captured A/B/D-pad event navigate or close the native
    // pause menu, especially when capturing its own Cancel or Accept binding.
    if(pause_menu.page==PausePage::Settings && pause_recomp::Capturing()) {
        kernel::Return(ctx,0,0);
        return true;
    }
    // B (event 8) and Back (event 6) are native pause-close events. While the
    // custom SETTINGS pages are open, consume them to navigate one level back;
    // at the native pause root and the separate Exit prompt, keep retail B.
    if ((event==6 || (event==8 && pause_menu.page==PausePage::Settings)) &&
        pause_menu.page!=PausePage::Root) {
        if(pause_menu.page==PausePage::Settings && !pause_recomp::Back())RefreshSettingsPage();
        else ShowPausePage(PausePage::Root);
        // Navigation consumed this input. A zero result lets the native GUI
        // propagate the same B press and close the surrounding Select menu.
        kernel::Return(ctx,1,0);
        return true;
    }
    if (event!=4) return false;
    const auto row=kernel::R32(list+0x6C);
    if (pause_menu.page==PausePage::Root) {
        if (row==PauseSettingsRow) ShowPausePage(PausePage::Settings);
        else if (row==PauseDesktopExitRow) ShowPausePage(PausePage::Exit);
        else return false;
    } else if (pause_menu.page==PausePage::Settings) {
        const auto previous=pause_recomp::CurrentPage();
        if(pause_recomp::Choose(row))ShowPausePage(PausePage::Root);
        else RefreshSettingsPage(previous==pause_recomp::CurrentPage()?row:0);
    } else if (pause_menu.page==PausePage::Exit) {
        if (row==0) ShowPausePage(PausePage::Root);
        else if (row==1) pause_recomp::Exit();
    }
    // The retail Select-list handler returns zero for accept and back events.
    kernel::Return(ctx,0,0);
    return true;
}
bool InstallPauseList(X86Context& ctx) {
    const auto saved=ctx;
    ctx.ecx=0x4EF3C8;
    const uint32_t list=kernel::CallGuest(ctx,0x1120D,{0x49F924});
    ctx=saved;
    if (!list) {
        LOGI("Gameplay pause expansion skipped: Select List 49F924 not found");
        return false;
    }
    const uint32_t original_rows=kernel::R32(list+0x54);
    const uint32_t original_count=kernel::R32(list+0x64);
    // Retail builder 0xA3D40 copies source 0x4F1CC0 into the list's OWN
    // destination buffer. 0x4F1CE0 is a different retail four-row table and
    // must never be used as the original Select List's storage.
    bool original_valid=original_rows!=0 && original_count==3;
    if (original_valid) {
        for (unsigned i=0;i<6;++i) {
            if (kernel::R32(original_rows+i*4)!=kernel::R32(0x4F1CC0+i*4)) original_valid=false;
        }
        if (kernel::R32(original_rows+24)!=0xFFFFFFFF || kernel::R32(original_rows+28)!=0)
            original_valid=false;
    }
    if (!original_valid) {
        LOGI("Gameplay pause expansion skipped: list=%08X buffer=%08X count=%u first=%08X",
             list,original_rows,original_count,original_rows?kernel::R32(original_rows):0);
        return false;
    }
    if (!EnsurePauseStorage()) return false;
    if (Unlocked() && !EnsureMenuStorage()) return false;
    pause_menu={list,original_rows,original_count,5+unsigned(Unlocked()),PausePage::Root};
    for (unsigned i=0;i<4;++i) kernel::W32(pause_storage+i*4,kernel::R32(original_rows+i*4));
    PausePair(pause_storage+PauseSettingsRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenSettings));
    PausePair(pause_storage+24,kernel::R32(original_rows+16),kernel::R32(original_rows+20));
    PausePair(pause_storage+PauseDesktopExitRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenExit));
    if (Unlocked()) PausePair(pause_storage+40,MenuLabel,0x135C6);
    PauseTerminator(pause_storage+pause_menu.root_count*8);
    PausePair(pause_storage+PauseRootBytes+PauseSettingsBytes,PauseLabelBase+uint32_t(pause_recomp::Item::No));
    PausePair(pause_storage+PauseRootBytes+PauseSettingsBytes+8,PauseLabelBase+uint32_t(pause_recomp::Item::Yes));
    PauseTerminator(pause_storage+PauseRootBytes+PauseSettingsBytes+16);
    PauseText();
    kernel::W32(list+0x54,pause_storage);
    kernel::W32(list+0x64,pause_menu.root_count);
    LOGI("Gameplay pause expanded: list=%08X rows=%u",list,pause_menu.root_count);
    return true;
}
bool EnsureMenuStorage() {
    if(!kontent_menu) {
        kontent_menu=mem::AllocSystem(KontentListBytes+3*DebugTextBytes);
        if(!kontent_menu){LOGI("Cannot allocate debug menu storage");return false;}
    }
    // All three captions have their own terminated guest storage, including
    // translated captions longer than the original eight-byte "Nexus" string.
    WriteMenuText(kontent_menu+KontentListBytes,pause_recomp::Localized("debug.unlock_kontent"));
    WriteMenuText(kontent_menu+KontentListBytes+DebugTextBytes,pause_recomp::Localized("debug.kontent_unlocked"));
    WriteMenuText(kontent_menu+KontentListBytes+2*DebugTextBytes,pause_recomp::Localized("debug.menu"));
    return true;
}
void AddKontentAction(X86Context& ctx) {
    const auto saved=ctx;
    ctx.ecx=0x4EF3C8;
    const uint32_t list=kernel::CallGuest(ctx,0x1120D,{0x49F8FC});
    ctx=saved;
    // Clone the seven native rows; extending their definition would overwrite the adjacent object.
    if(!list || kernel::R32(list+0x54)!=0x4F1F00 || kernel::R32(list+0x64)!=7 ||
       kernel::R32(0x4F1F38)!=0xFFFFFFFF)return;
    if(!EnsureMenuStorage())return;
    for(unsigned i=0;i<14;++i)kernel::W32(kontent_menu+i*4,kernel::R32(0x4F1F00+i*4));
    kernel::W32(kontent_menu+56,KontentLabel);
    kernel::W32(kontent_menu+60,0x135C1); // Retail no-op; accept is handled below.
    kernel::W32(kontent_menu+64,0xFFFFFFFF);kernel::W32(kontent_menu+68,0);
    kernel::W32(list+0x54,kontent_menu);kernel::W32(list+0x64,8);
}
bool in_pause = false, in_nexus = false;
struct PauseScope {
    bool previous_pause = in_pause, previous_nexus = in_nexus;
    PauseMenuState previous_menu = pause_menu;
    PauseScope() {
        static bool first_entry = true;
        if (first_entry) {
            LOGI("Gameplay pause scope reached (sub_00012BA8)");
            first_entry = false;
        }
        in_pause = true; in_nexus = false; pause_menu={};
    }
    ~PauseScope() {
        pause_recomp::Close();
        if (pause_menu.list && pause_storage) {
            const auto rows=kernel::R32(pause_menu.list+0x54);
            if (rows==pause_storage || rows==pause_storage+PauseRootBytes ||
                rows==pause_storage+PauseRootBytes+PauseSettingsBytes) {
                kernel::W32(pause_menu.list+0x54,pause_menu.original_rows);
                kernel::W32(pause_menu.list+0x64,pause_menu.original_count);
                kernel::W32(pause_menu.list+0x6C,0);
            }
        }
        pause_menu=previous_menu;
        in_pause = previous_pause; in_nexus = previous_nexus;
    }
};
void UpdateHeldInput(X86Context& ctx) {
    const uint32_t depth = kernel::R32(0x6C9308);
    bool main = depth && depth <= 16 &&
        kernel::R32(0x6C9308 + depth * 4) == 0x598430;
    if (main) {
        const uint32_t panel = kernel::R32(0x59843C);
        const uint32_t state = panel ? kernel::R32(panel + 0x2E4) : ~0u;
        main = state == 0 || state == 2;
    }
    // White/LB is a held condition, not an entry in the ten-button sequence.
    const uint16_t pressed = uint16_t(kernel::R32(0x67C66A)) & ~uint16_t{1};
    const bool modifier = main && (kernel::R32(0x67C668) & 1);
    if (!sequence.Feed(pressed,main,modifier)) return;
    const auto saved = ctx;
    ctx.ecx = 0x653BC0;
    // Loaded shared voice bank: vo_sk_combo_counter_190_1.adx.
    const uint32_t voice = kernel::CallGuest(ctx,0x151560,{0x5CE79082,0x3F800000,0,0,0,0xBF800000,0,0});
    ctx = saved;
    LOGI("Nexus unlocked for this session (voice=%08X)",voice);
}
}
bool Unlocked() { return sequence.Unlocked(); }
bool GameplayPaused() { return in_pause; }
bool KontentUnlocked() { return kontent_unlocked; }
void FrontendInput(X86Context& ctx,uint8_t*) {
    // The frontend still ticks with empty input while the held-modifier path runs.
    if (ctx.ecx != 0x598430 || !(kernel::R32(0x67C668) & 1)) sequence.ResetInput();
}
}
X86_EXTERN_FUNC(__imp__sub_00012BA8);
X86_EXTERN_FUNC(__imp__sub_0001332D);
X86_FUNC(sub_0001332D) {
    __imp__sub_0001332D(ctx,base);
    if (openshao::ed_nexus::in_pause) {
        static bool first_builder = true;
        if (first_builder) {
            LOGI("Gameplay pause builder reached (sub_0001332D)");
            first_builder = false;
        }
        openshao::ed_nexus::InstallPauseList(ctx);
    }
}
X86_EXTERN_FUNC(__imp__sub_000135C6);
X86_FUNC(sub_000135C6) {
    const bool gameplay_nexus = openshao::ed_nexus::in_pause && openshao::ed_nexus::Unlocked();
    if (gameplay_nexus) openshao::ed_nexus::in_nexus = true;
    __imp__sub_000135C6(ctx,base);
    if (gameplay_nexus) {
        openshao::ed_nexus::AddKontentAction(ctx);
        LOGI("Nexus opened from gameplay pause");
    }
}
X86_EXTERN_FUNC(__imp__sub_001B5300);
X86_FUNC(sub_001B5300) {
    // Holding the original modifier bypasses the normal frontend callback.
    if (ctx.ecx == 0x6CA3A4) openshao::ed_nexus::UpdateHeldInput(ctx);
    __imp__sub_001B5300(ctx,base);
}
X86_FUNC(sub_00012BA8) {
    openshao::ed_nexus::PauseScope pause;
    __imp__sub_00012BA8(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_000142EB);
X86_FUNC(sub_000142EB) {
    const bool gameplay_gui = ctx.ecx == 0x4EF3C8;
    __imp__sub_000142EB(ctx,base);
    if(openshao::ed_nexus::in_pause && openshao::ed_nexus::pause_menu.page==openshao::ed_nexus::PausePage::Settings &&
       openshao::pause_recomp::Update()) {
        openshao::ed_nexus::RefreshSettingsPage(kernel::R32(openshao::ed_nexus::pause_menu.list+0x6C));
    }
    if (!gameplay_gui || openshao::ed_nexus::pause_menu.page!=openshao::ed_nexus::PausePage::Root ||
        !openshao::ed_nexus::OpenShortcut(openshao::ed_nexus::Unlocked(),
            openshao::ed_nexus::in_pause,openshao::ed_nexus::in_nexus,uint16_t(kernel::R32(0x67C66A)))) return;
    const auto saved = ctx;
    kernel::CallGuest(ctx,0x135C6,{});
    ctx = saved;
}
X86_EXTERN_FUNC(__imp__sub_00013B4D);
X86_EXTERN_FUNC(__imp__sub_00015687);
X86_FUNC(sub_00015687) {
    const uint32_t list=kernel::R32(ctx.esp+4),event=kernel::R32(ctx.esp+8);
    if(openshao::ed_nexus::in_pause && openshao::ed_nexus::HandlePauseAction(ctx,list,event)) return;
    __imp__sub_00015687(ctx,base);
}
X86_FUNC(sub_00013B4D) {
    const uint32_t list=kernel::R32(ctx.esp+4),event=kernel::R32(ctx.esp+8);
    if(openshao::ed_nexus::Unlocked() && openshao::ed_nexus::in_nexus && event==4 && list &&
       openshao::ed_nexus::kontent_menu && kernel::R32(list+0x54)==openshao::ed_nexus::kontent_menu &&
       kernel::R32(list+0x64)==8 && kernel::R32(list+0x6C)==openshao::ed_nexus::KontentRow) {
        // Override session availability only; never write flags serialized by the native save system.
        openshao::ed_nexus::kontent_unlocked=true;
        LOGI("All Kontent unlocked for this session; native save flags unchanged");
        kernel::Return(ctx,1,0);
        return;
    }
    // Event 6 in the original Nexus list handler restores the normal pause.
    const bool cancel = event == 6;
    __imp__sub_00013B4D(ctx,base);
    if (cancel) openshao::ed_nexus::in_nexus = false;
}

// Gallery predicates return AL with caller-cleaned arguments; preserve other registers and invalid indices.
#define OPENSHAO_KONTENT_QUERY(name,count) \
X86_EXTERN_FUNC(__imp__##name); \
X86_FUNC(name) { \
    const uint32_t index=kernel::R32(ctx.esp+4); \
    __imp__##name(ctx,base); \
    if(openshao::ed_nexus::kontent_unlocked && index<count)ctx.eax=(ctx.eax&0xFFFFFF00u)|1; \
}
OPENSHAO_KONTENT_QUERY(sub_000160AF,64)  // Arenas.
OPENSHAO_KONTENT_QUERY(sub_00011F64,176) // Concept art.
OPENSHAO_KONTENT_QUERY(sub_00013FCB,8)   // Movies.
OPENSHAO_KONTENT_QUERY(sub_00013751,16)  // Photos.
#undef OPENSHAO_KONTENT_QUERY
X86_EXTERN_FUNC(__imp__sub_00015C6D);
X86_FUNC(sub_00015C6D) {
    __imp__sub_00015C6D(ctx,base);
    if(openshao::ed_nexus::kontent_unlocked)ctx.eax=(ctx.eax&0xFFFFFF00u)|1;
}
X86_EXTERN_FUNC(__imp__sub_00016992);
X86_FUNC(sub_00016992) {
    const uint32_t label=kernel::R32(ctx.esp+4);
    if(openshao::ed_nexus::pause_storage && label>=openshao::ed_nexus::PauseLabelBase &&
       label<openshao::ed_nexus::PauseLabelBase+openshao::ed_nexus::PauseTextCount) {
        kernel::Return(ctx,openshao::ed_nexus::pause_storage+openshao::ed_nexus::PauseListBytes+
                       (label-openshao::ed_nexus::PauseLabelBase)*openshao::ed_nexus::PauseTextBytes,0);
        return;
    }
    if(openshao::ed_nexus::kontent_menu && kernel::R32(ctx.esp+4)==openshao::ed_nexus::MenuLabel) {
        kernel::Return(ctx,openshao::ed_nexus::kontent_menu+openshao::ed_nexus::KontentListBytes+48,0);
        return;
    }
    if(openshao::ed_nexus::kontent_menu && kernel::R32(ctx.esp+4)==openshao::ed_nexus::KontentLabel) {
        kernel::Return(ctx,openshao::ed_nexus::kontent_menu+openshao::ed_nexus::KontentListBytes+
                       (openshao::ed_nexus::kontent_unlocked?24:0),0);
        return;
    }
    __imp__sub_00016992(ctx,base);
}
