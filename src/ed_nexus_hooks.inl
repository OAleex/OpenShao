#include "ed_nexus_sequence.h"
#include "pause_recomp.h"
#include "recomp_input.h"
#include "difficulty.h"
#include "campaign_moves.h"
#include "photo_mode.h"
#include <algorithm>
#include <bit>
#include <unordered_map>

namespace openshao::ed_nexus {
namespace {
Sequence sequence;
bool kontent_unlocked = false;
uint32_t kontent_menu = 0;
constexpr uint32_t MenuLabel = 0x19F;
constexpr uint32_t KontentLabel = 0xFFFFFFFE;
constexpr uint32_t KontentRow = 7;
// Debug row after Kontent: player damage multiplier, cycled x1 -> x2 -> x4 -> x8 for this session.
constexpr uint32_t DamageLabel = 0xFFFFFFFD;
constexpr uint32_t DamageRow = 8;
// Debug row after player damage: refills both heroes' health (also bypasses the Monk healing cut).
constexpr uint32_t HealthLabel = 0xFFFFFFFC;
constexpr uint32_t HealthRow = 9;
// Debug row after health: Player 2 on/off. The change waits for the next map (ed_nexus.cpp).
constexpr uint32_t Player2Label = 0xFFFFFFFB;
constexpr uint32_t Player2Row = 10;
constexpr uint32_t Player2TextBytes = 64;
bool player2_pending = false, player2_target = false;
constexpr uint32_t DebugRows = 11;
constexpr std::array<uint32_t,4> DamageSteps{1,2,4,8};
unsigned damage_step = 0;
constexpr uint32_t KontentListBytes = (DebugRows+1)*8;
constexpr uint32_t DebugTextBytes = 24;
constexpr uint32_t PauseLabelBase = 0xFFFFFF00;
// Retail Continue and Options, Save Game, Settings, Enhancements, Photo Mode, retail Quit, Exit, optional Nexus, and sentinel.
constexpr uint32_t PauseRootBytes = 10*8;
// Reserve room for the settings rows and the native list terminator.
constexpr uint32_t PauseSettingsBytes = (pause_recomp::MaxRows+1)*8;
constexpr uint32_t PauseExitBytes = 3*8;
constexpr uint32_t PauseListBytes = PauseRootBytes + PauseSettingsBytes + PauseExitBytes;
constexpr uint32_t PauseTextCount = uint32_t(pause_recomp::Item::Count)+pause_recomp::MaxRows;
constexpr uint32_t PauseSaveRow = 2, PauseSettingsRow = 3, PauseEnhancementsRow = 4, PausePhotoRow = 5;
constexpr uint32_t SaveMenuOpener = 0x13E0E, SaveMenuPlayerIndex = 0x4EFA60;
uint32_t pause_storage = 0;
bool save_after_pause = false;
uint32_t pause_text_storage = 0, pause_text_stride = 0;
bool EnsureMenuStorage();
enum class PausePage { Root, Settings, Exit };
struct PauseMenuState {
    uint32_t list = 0, original_rows = 0, original_count = 0;
    uint32_t root_count = 0;
    PausePage page = PausePage::Root;
    uint32_t return_row = PauseSettingsRow;
    uint32_t normal_scale = 0, selected_scale = 0;
    bool back_release = false;
    uint32_t initial_selection = 0;
    uint32_t panel_padding = 0, footer_text = 0;
    // Photo Mode follows Enhancements where the mode offers it; the rows after it move down.
    uint32_t photo_row = ~0u, exit_row = 6;
};
PauseMenuState pause_menu;
void WriteGuestText(uint32_t address, const char* text, unsigned bytes) {
    for (unsigned word=0;word<bytes/4;++word) {
        uint32_t value=0;
        for (unsigned byte=0;byte<4 && *text;++byte) value|=uint32_t(uint8_t(*text++))<<(byte*8);
        kernel::W32(address+word*4,value);
    }
}
void WriteMenuText(uint32_t address,const std::string& text,uint32_t bytes=DebugTextBytes) {
    // The owned guest allocation keeps native text pointers valid across draws.
    WriteGuestText(address,text.c_str(),bytes);
}
bool EnsurePauseStorage() {
    if (pause_storage) return true;
    pause_storage=mem::AllocSystem(PauseListBytes);
    if (!pause_storage) LOGI("Cannot allocate gameplay pause menu storage");
    return pause_storage!=0;
}
void PauseText() {
    std::array<std::string,PauseTextCount> labels;
    uint32_t stride=4;
    for(uint32_t i=0;i<PauseTextCount;++i) {
        labels[i]=i<uint32_t(pause_recomp::Item::Count)?
            pause_recomp::Text(pause_recomp::Item(i)):
            pause_recomp::RowText(i-uint32_t(pause_recomp::Item::Count));
        stride=std::max(stride,(uint32_t(labels[i].size())+4)&~3u);
    }
    if(stride>pause_text_stride) {
        const auto storage=mem::AllocSystem(PauseTextCount*stride);
        if(!storage){LOGI("Cannot allocate gameplay pause text storage");return;}
        if(pause_text_storage)mem::FreeSystem(pause_text_storage);
        pause_text_storage=storage;pause_text_stride=stride;
    }
    for(uint32_t i=0;i<PauseTextCount;++i)
        WriteGuestText(pause_text_storage+i*pause_text_stride,labels[i].c_str(),pause_text_stride);
}
void FitPauseList(X86Context& ctx) {
    const auto list=pause_menu.list;
    if(!list)return;
    const auto saved=ctx;
    const auto rows=kernel::R32(list+0x54),count=kernel::R32(list+0x64);
    // Leave enough height for the ornate corners even when a page has one row.
    const unsigned extra_padding=count==1?8:0;
    kernel::W32(list+0x40,pause_menu.panel_padding+extra_padding);
    uint32_t width=0;
    for(unsigned row=0;row<count;++row) {
        const auto text=kernel::CallGuest(ctx,0x16992,{kernel::R32(rows+row*8)});ctx=saved;
        const auto measured=kernel::CallGuest(ctx,0x152DC0,{text,kernel::R32(list+0x78)});ctx=saved;
        width=std::max(width,measured);
    }
    // Shrink both font scales together only when Select's ornament would reach the canvas edges.
    const float normal=std::bit_cast<float>(pause_menu.normal_scale);
    const float selected=std::bit_cast<float>(pause_menu.selected_scale);
    const float largest=std::max(normal,selected);
    const float available=544.f-4*extra_padding;
    const float factor=width && width*largest>available?available/(width*largest):1.f;
    kernel::W32(list+0x84,std::bit_cast<uint32_t>(normal*factor));
    kernel::W32(list+0x88,std::bit_cast<uint32_t>(selected*factor));
}
void PausePair(uint32_t address,uint32_t label,uint32_t callback=0x135C1) {
    kernel::W32(address,label);
    kernel::W32(address+4,callback);
}
void PauseTerminator(uint32_t address) {PausePair(address,0xFFFFFFFF,0);}
void RefreshSettingsPage(X86Context& ctx,uint32_t row=0) {
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
    FitPauseList(ctx);
}
void ShowPausePage(X86Context& ctx,PausePage page,pause_recomp::Page initial=pause_recomp::Page::Categories) {
    if (!pause_menu.list) return;
    const auto list=pause_menu.list;
    const auto previous=pause_menu.page;
    if(previous==PausePage::Settings && page!=PausePage::Settings)pause_recomp::Close();
    pause_menu.page=page;
    if (page==PausePage::Root) {
        kernel::W32(list+0x54,pause_storage);
        kernel::W32(list+0x64,pause_menu.root_count);
        kernel::W32(list+0x6C,previous==PausePage::Exit?pause_menu.exit_row:pause_menu.return_row);
        FitPauseList(ctx);
    } else if (page==PausePage::Settings) {
        pause_menu.return_row=initial==pause_recomp::Page::Enhancements?PauseEnhancementsRow:PauseSettingsRow;
        pause_recomp::Open(initial);
        RefreshSettingsPage(ctx);
    } else {
        PauseText();
        kernel::W32(list+0x54,pause_storage+PauseRootBytes+PauseSettingsBytes);
        kernel::W32(list+0x64,2);
        kernel::W32(list+0x6C,0); // No is the default; Yes explicitly warns about unsaved progress.
        FitPauseList(ctx);
    }
}
void OpenSaveMenu(X86Context& ctx) {
    const auto saved=ctx;
    // The save statue selects the acting player before opening the same menu.
    kernel::W32(SaveMenuPlayerIndex,0);
    kernel::CallGuest(ctx,SaveMenuOpener,{});
    ctx=saved;
    LOGI("Save menu opened from gameplay pause");
}
bool HandlePauseAction(X86Context& ctx,uint32_t list,uint32_t event) {
    if (!pause_menu.list || !pause_storage || list!=pause_menu.list) return false;
    const auto rows=kernel::R32(list+0x54);
    const auto expected=pause_menu.page==PausePage::Root?pause_storage:
        pause_menu.page==PausePage::Settings?pause_storage+PauseRootBytes:
        pause_storage+PauseRootBytes+PauseSettingsBytes;
    if(rows!=expected)return false;
    if(pause_menu.page==PausePage::Settings &&
       (pause_recomp::CapturingController() || recomp::raw::ControllerCancelHeld())) {
        // Physical B may also be mapped to a native menu action. Let capture decide what it means.
        if(event==6 || event==8)pause_menu.back_release=true;
        kernel::Return(ctx,1,0);
        return true;
    }
    if((event==6 || event==8) && pause_menu.back_release) {
        kernel::Return(ctx,1,0);
        return true;
    }
    // B (6) and Select (8) close pause natively; settings return one level instead.
    if ((event==6 || (event==8 && pause_menu.page==PausePage::Settings)) &&
        pause_menu.page!=PausePage::Root) {
        pause_menu.back_release=true;
        if(pause_menu.page==PausePage::Settings && !pause_recomp::Back())RefreshSettingsPage(ctx);
        else ShowPausePage(ctx,PausePage::Root);
        // A zero result would let the same B close the Select menu.
        kernel::Return(ctx,1,0);
        return true;
    }
    // Zero lets the GUI run its fallback accept/back; consume captured input without closing Select.
    if(pause_menu.page==PausePage::Settings && pause_recomp::Capturing()) {
        kernel::Return(ctx,1,0);
        return true;
    }
    const auto row=kernel::R32(list+0x6C);
    if(pause_menu.page==PausePage::Settings && (event==2 || event==3)) {
        const auto previous=pause_recomp::CurrentPage();
        if(!pause_recomp::Adjust(row,event==2?-1:1))return false;
        RefreshSettingsPage(ctx,previous==pause_recomp::CurrentPage()?row:0);
        kernel::Return(ctx,1,0);
        return true;
    }
    if (event!=4) return false;
    if (pause_menu.page==PausePage::Root) {
        if (row==PauseSettingsRow) ShowPausePage(ctx,PausePage::Settings);
        else if (row==PauseEnhancementsRow) ShowPausePage(ctx,PausePage::Settings,pause_recomp::Page::Enhancements);
        else if (row==pause_menu.photo_row) openshao::photo_mode::OpenFromMenu();
        else if (row==pause_menu.exit_row) ShowPausePage(ctx,PausePage::Exit);
        else if (row==PauseSaveRow) {
            // Close the pause exactly like Continue; the save menu opens once the pause has ended.
            save_after_pause=true;
            kernel::W32(list+0x6C,0);
            return false;
        }
        else return false;
    } else if (pause_menu.page==PausePage::Settings) {
        if(!pause_recomp::CanChoose(row)) {
            kernel::Return(ctx,1,0);
            return true;
        }
        const auto previous=pause_recomp::CurrentPage();
        pause_recomp::Choose(row);
        RefreshSettingsPage(ctx,previous==pause_recomp::CurrentPage()?row:0);
    } else if (pause_menu.page==PausePage::Exit) {
        if (row==0) ShowPausePage(ctx,PausePage::Root);
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
    // Builder 0xA3D40 copies 0x4F1CC0 into the list's own buffer; 0x4F1CE0 is another table.
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
    const auto initial_selection=pause_menu.initial_selection;
    const auto back_release=pause_menu.back_release;
    const bool photo=openshao::photo_mode::Available();
    pause_menu={list,original_rows,original_count,7+unsigned(photo)+unsigned(Unlocked()),PausePage::Root,
                PauseSettingsRow,kernel::R32(list+0x84),kernel::R32(list+0x88),back_release};
    pause_menu.photo_row=photo?PausePhotoRow:~0u;
    const uint32_t exit_game_row=photo?PausePhotoRow+1:PausePhotoRow;
    pause_menu.exit_row=exit_game_row+1;
    pause_menu.panel_padding=kernel::R32(list+0x40);
    for (unsigned i=0;i<4;++i) kernel::W32(pause_storage+i*4,kernel::R32(original_rows+i*4));
    PausePair(pause_storage+PauseSaveRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::SaveGame));
    PausePair(pause_storage+PauseSettingsRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenSettings));
    PausePair(pause_storage+PauseEnhancementsRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenEnhancements));
    if (photo) PausePair(pause_storage+PausePhotoRow*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenPhotoMode));
    PausePair(pause_storage+exit_game_row*8,kernel::R32(original_rows+16),kernel::R32(original_rows+20));
    PausePair(pause_storage+pause_menu.exit_row*8,PauseLabelBase+uint32_t(pause_recomp::Item::OpenExit));
    if (Unlocked()) PausePair(pause_storage+(pause_menu.exit_row+1)*8,MenuLabel,0x135C6);
    PauseTerminator(pause_storage+pause_menu.root_count*8);
    PausePair(pause_storage+PauseRootBytes+PauseSettingsBytes,PauseLabelBase+uint32_t(pause_recomp::Item::No));
    PausePair(pause_storage+PauseRootBytes+PauseSettingsBytes+8,PauseLabelBase+uint32_t(pause_recomp::Item::Yes));
    PauseTerminator(pause_storage+PauseRootBytes+PauseSettingsBytes+16);
    PauseText();
    kernel::W32(list+0x54,pause_storage);
    kernel::W32(list+0x64,pause_menu.root_count);
    kernel::W32(list+0x6C,initial_selection<pause_menu.root_count?initial_selection:0);
    FitPauseList(ctx);
    LOGI("Gameplay pause expanded: list=%08X rows=%u",list,pause_menu.root_count);
    return true;
}
void WriteDamageText() {
    WriteMenuText(kontent_menu+KontentListBytes+3*DebugTextBytes,
                  pause_recomp::Localized("debug.player_damage")+": x"+std::to_string(DamageSteps[damage_step]));
}
void WritePlayer2Text() {
    const bool on=player2_pending?player2_target:Player2Active();
    std::string text=pause_recomp::Localized("debug.player2")+": "+pause_recomp::Localized(on?"value.on":"value.off");
    if(player2_pending)text+=" ("+pause_recomp::Localized("debug.next_map")+")";
    WriteMenuText(kontent_menu+KontentListBytes+5*DebugTextBytes,text,Player2TextBytes);
}
bool EnsureMenuStorage() {
    if(!kontent_menu) {
        kontent_menu=mem::AllocSystem(KontentListBytes+5*DebugTextBytes+Player2TextBytes);
        if(!kontent_menu){LOGI("Cannot allocate debug menu storage");return false;}
    }
    // Each caption has its own terminated storage (translations exceed "Nexus").
    WriteMenuText(kontent_menu+KontentListBytes,pause_recomp::Localized("debug.unlock_kontent"));
    WriteMenuText(kontent_menu+KontentListBytes+DebugTextBytes,pause_recomp::Localized("debug.kontent_unlocked"));
    WriteMenuText(kontent_menu+KontentListBytes+2*DebugTextBytes,pause_recomp::Localized("debug.menu"));
    WriteDamageText();
    WriteMenuText(kontent_menu+KontentListBytes+4*DebugTextBytes,pause_recomp::Localized("debug.restore_health"));
    WritePlayer2Text();
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
    kernel::W32(kontent_menu+64,DamageLabel);
    kernel::W32(kontent_menu+68,0x135C1);
    kernel::W32(kontent_menu+72,HealthLabel);
    kernel::W32(kontent_menu+76,0x135C1);
    kernel::W32(kontent_menu+80,Player2Label);
    kernel::W32(kontent_menu+84,0x135C1);
    kernel::W32(kontent_menu+88,0xFFFFFFFF);kernel::W32(kontent_menu+92,0);
    kernel::W32(list+0x54,kontent_menu);kernel::W32(list+0x64,DebugRows);
}
bool in_pause = false, in_nexus = false, deferred_hud = false;
uint32_t CachedFooter(std::string text) {
    // Native widgets retain caption pointers; cache each complete footer in owned guest storage.
    static std::unordered_map<std::string,uint32_t> footers;
    if(const auto found=footers.find(text);found!=footers.end())return found->second;
    const auto bytes=(uint32_t(text.size())+4)&~3u;
    const auto storage=mem::AllocSystem(bytes);
    if(!storage)return 0;
    WriteGuestText(storage,text.c_str(),bytes);
    footers.emplace(std::move(text),storage);
    return storage;
}
uint32_t BackFooter(uint32_t address,bool adjust,bool accept,bool navigate) {
    std::string text;
    for(unsigned i=0;i<256;++i) {
        const auto at=address+i;
        const char glyph=char(kernel::R32(at&~3u)>>((at%4)*8));
        if(!glyph)break;
        text.push_back(glyph);
    }
    if(text.size()==256)return address;
    auto first=text.find('|');
    if(first==std::string::npos)return address;
    first=text.find_first_not_of(' ',first+1);
    if(first==std::string::npos)return address;
    auto last=text.find_first_of("=<>|{}",first);
    if(last==std::string::npos)last=text.size();
    while(last>first && text[last-1]==' ')--last;
    text.replace(first,last-first,pause_recomp::Localized("footer.back"));
    if(!accept) {
        const auto button=text.find('=');
        if(button!=std::string::npos && button<first)text.erase(button,text.find('|')-button);
    }
    if(!navigate) {
        const auto arrows=text.find("{/}");
        if(arrows!=std::string::npos) {
            auto first=arrows;
            while(first>0 && text[first-1]==' ')--first;
            const auto next=text.find_first_of("=<>|{}",arrows+3);
            text.erase(first,next==std::string::npos?next:next-first);
            if(next!=std::string::npos)text.insert(first,"  ");
        }
    }
    if(adjust)text+="  </> "+pause_recomp::Localized("settings.adjust_hint");
    const auto storage=CachedFooter(std::move(text));
    return storage?storage:address;
}
struct PauseScope {
    bool previous_pause = in_pause, previous_nexus = in_nexus;
    PauseMenuState previous_menu = pause_menu;
    PauseScope() {
        static bool first_entry = true;
        if (first_entry) {
            LOGI("Gameplay pause scope reached (sub_00012BA8)");
            first_entry = false;
        }
        in_pause = true; in_nexus = false; pause_menu={}; save_after_pause = false;
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
                kernel::W32(pause_menu.list+0x40,pause_menu.panel_padding);
                kernel::W32(pause_menu.list+0x84,pause_menu.normal_scale);
                kernel::W32(pause_menu.list+0x88,pause_menu.selected_scale);
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
bool PauseRootShown() {
    return in_pause && !in_nexus && pause_menu.page==PausePage::Root && !pause_recomp::Capturing();
}
bool KontentUnlocked() { return kontent_unlocked; }
float PlayerDamage() { return Unlocked() ? float(DamageSteps[damage_step]) : 1.f; }
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
    const auto entry=ctx;
    const auto previous_hud=openshao::ed_nexus::deferred_hud;
    openshao::ed_nexus::deferred_hud=false;
    bool return_from_options=false;
    do {
        ctx=entry;
        openshao::ed_nexus::PauseScope pause;
        openshao::ed_nexus::pause_menu.initial_selection=return_from_options?1:0;
        openshao::ed_nexus::pause_menu.back_release=return_from_options;
        __imp__sub_00012BA8(ctx,base);
        // Retail leaves Select to run Options; rebuild Select in the same call so B returns without a gameplay frame.
        return_from_options=(kernel::R32(0x4EFA5C)&0xFF)!=0;
    } while(return_from_options);
    if(openshao::ed_nexus::deferred_hud) {
        const auto result=ctx;
        kernel::CallGuest(ctx,0x15060,{});
        ctx=result;
    }
    openshao::ed_nexus::deferred_hud=previous_hud;
    if (openshao::ed_nexus::save_after_pause) {
        openshao::ed_nexus::save_after_pause = false;
        openshao::ed_nexus::OpenSaveMenu(ctx);
    }
}
X86_EXTERN_FUNC(__imp__sub_00015060);
X86_FUNC(sub_00015060) {
    // Options restores the gameplay HUD on exit. Keep it dormant until Select also closes.
    if(openshao::ed_nexus::in_pause && (kernel::R32(0x4EFA5C)&0xFF)) {
        openshao::ed_nexus::deferred_hud=true;
        kernel::Return(ctx,0,0);
        return;
    }
    __imp__sub_00015060(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_000142EB);
X86_FUNC(sub_000142EB) {
    const bool gameplay_gui = ctx.ecx == 0x4EF3C8;
    __imp__sub_000142EB(ctx,base);
    // A held or repeated Back event must not traverse the newly displayed parent page.
    if(gameplay_gui && !(kernel::R32(0x67C668)&0x120))openshao::ed_nexus::pause_menu.back_release=false;
    if(gameplay_gui && openshao::ed_nexus::in_pause && openshao::ed_nexus::pause_menu.page==openshao::ed_nexus::PausePage::Settings &&
       openshao::pause_recomp::Update()) {
        openshao::ed_nexus::RefreshSettingsPage(ctx,kernel::R32(openshao::ed_nexus::pause_menu.list+0x6C));
    }
    if (!gameplay_gui || openshao::ed_nexus::pause_menu.page!=openshao::ed_nexus::PausePage::Root ||
        !openshao::ed_nexus::OpenShortcut(openshao::ed_nexus::Unlocked(),
            openshao::ed_nexus::in_pause,openshao::ed_nexus::in_nexus,uint16_t(kernel::R32(0x67C66A)))) return;
    const auto saved = ctx;
    kernel::CallGuest(ctx,0x135C6,{});
    ctx = saved;
}
void openshao::ed_nexus::DrawPausePageIndicator(X86Context& ctx) {
    if(!in_pause || pause_menu.page!=PausePage::Settings || !pause_menu.footer_text ||
       kernel::R32(ctx.esp+20)!=pause_menu.footer_text || kernel::R32(ctx.esp+4)!=320)return;
    auto indicator=openshao::pause_recomp::PageIndicator();
    if(indicator.empty())return;
    const auto text=CachedFooter(std::move(indicator));
    if(!text)return;
    // Put the page indicator immediately above Select's native button legend.
    const uint32_t footer_y=kernel::R32(ctx.esp+8);
    constexpr auto scale=std::bit_cast<uint32_t>(.8f);
    const auto saved=ctx;
    kernel::CallGuest(ctx,0x152CE0,{320,footer_y-28,kernel::R32(ctx.esp+12),2,text,scale,scale});
    ctx=saved;
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
       kernel::R32(list+0x64)==openshao::ed_nexus::DebugRows &&
       kernel::R32(list+0x6C)==openshao::ed_nexus::DamageRow) {
        using namespace openshao::ed_nexus;
        damage_step=(damage_step+1)%DamageSteps.size();
        WriteDamageText();
        LOGI("Player damage x%u for this session",DamageSteps[damage_step]);
        kernel::Return(ctx,1,0);
        return;
    }
    if(openshao::ed_nexus::Unlocked() && openshao::ed_nexus::in_nexus && event==4 && list &&
       openshao::ed_nexus::kontent_menu && kernel::R32(list+0x54)==openshao::ed_nexus::kontent_menu &&
       kernel::R32(list+0x64)==openshao::ed_nexus::DebugRows && kernel::R32(list+0x6C)==openshao::ed_nexus::Player2Row) {
        using namespace openshao::ed_nexus;
        // Toggle the wanted state; it is applied when the next map loads, like the native spawn.
        const bool active=Player2Active();
        player2_target=player2_pending?!player2_target:!active;
        player2_pending=player2_target!=active;
        WritePlayer2Text();
        LOGI("Debug player 2 %s at the next map",player2_pending?(player2_target?"on":"off"):"unchanged");
        kernel::Return(ctx,1,0);
        return;
    }
    if(openshao::ed_nexus::Unlocked() && openshao::ed_nexus::in_nexus && event==4 && list &&
       openshao::ed_nexus::kontent_menu && kernel::R32(list+0x54)==openshao::ed_nexus::kontent_menu &&
       kernel::R32(list+0x64)==openshao::ed_nexus::DebugRows && kernel::R32(list+0x6C)==openshao::ed_nexus::HealthRow) {
        const unsigned refilled=openshao::difficulty::RefillHeroes(ctx);
        LOGI("Debug health restore: %u hero(es) refilled",refilled);
        kernel::Return(ctx,1,0);
        return;
    }
    if(openshao::ed_nexus::Unlocked() && openshao::ed_nexus::in_nexus && event==4 && list &&
       openshao::ed_nexus::kontent_menu && kernel::R32(list+0x54)==openshao::ed_nexus::kontent_menu &&
       kernel::R32(list+0x64)==openshao::ed_nexus::DebugRows && kernel::R32(list+0x6C)==openshao::ed_nexus::KontentRow) {
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
        kernel::Return(ctx,openshao::ed_nexus::pause_text_storage+
                       (label-openshao::ed_nexus::PauseLabelBase)*openshao::ed_nexus::pause_text_stride,0);
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
    if(openshao::ed_nexus::kontent_menu && label==openshao::ed_nexus::DamageLabel) {
        kernel::Return(ctx,openshao::ed_nexus::kontent_menu+openshao::ed_nexus::KontentListBytes+
                       3*openshao::ed_nexus::DebugTextBytes,0);
        return;
    }
    if(openshao::ed_nexus::kontent_menu && label==openshao::ed_nexus::HealthLabel) {
        kernel::Return(ctx,openshao::ed_nexus::kontent_menu+openshao::ed_nexus::KontentListBytes+
                       4*openshao::ed_nexus::DebugTextBytes,0);
        return;
    }
    if(openshao::ed_nexus::kontent_menu && label==openshao::ed_nexus::Player2Label) {
        kernel::Return(ctx,openshao::ed_nexus::kontent_menu+openshao::ed_nexus::KontentListBytes+
                       5*openshao::ed_nexus::DebugTextBytes,0);
        return;
    }
    if(const uint32_t text=openshao::survival::Label(label)) {kernel::Return(ctx,text,0);return;}
    if(const uint32_t text=openshao::difficulty::Label(label,kernel::R32(ctx.esp))) { kernel::Return(ctx,text,0); return; }
    std::string corrected;
    if(openshao::campaign_moves::ReferenceCaption(label,corrected)) {
        std::string title;
        if(const auto key=openshao::campaign_moves::ReferenceTranslationKey(label);!key.empty())
            title=openshao::pause_recomp::Localized(key);
        if(const auto source=openshao::campaign_moves::ReferenceTitleSource(label)) {
            const auto saved=ctx;
            const auto native=kernel::CallGuest(ctx,0x16992,{source});
            ctx=saved;
            openshao::campaign_moves::LocalizedReferenceTitle(native,title);
        }
        if(!title.empty())corrected.replace(0,corrected.find('\n'),title);
        if(const auto text=openshao::ed_nexus::CachedFooter(std::move(corrected))) {
            kernel::Return(ctx,text,0);
            return;
        }
        const auto argument=ctx.esp+4;
        kernel::W32(argument,511);
        __imp__sub_00016992(ctx,base);
        kernel::W32(argument,label);
        return;
    }
    __imp__sub_00016992(ctx,base);
    if(openshao::campaign_moves::CorrectCaption(label,ctx.eax,corrected))
        if(const auto text=openshao::ed_nexus::CachedFooter(std::move(corrected)))ctx.eax=text;
    const bool footer=(label>=0x58A && label<=0x58F) || label==0x591 ||
                      (label==0x590 && openshao::ed_nexus::in_nexus);
    const bool nested=openshao::ed_nexus::pause_menu.page!=openshao::ed_nexus::PausePage::Root ||
                      openshao::ed_nexus::in_nexus || (kernel::R32(0x4EFA5C)&0xFF)!=0;
    if(footer && openshao::ed_nexus::in_pause && nested && ctx.eax) {
        const bool settings=openshao::ed_nexus::pause_menu.page==openshao::ed_nexus::PausePage::Settings;
        const auto row=settings?kernel::R32(openshao::ed_nexus::pause_menu.list+0x6C):0;
        if(settings && openshao::pause_recomp::Capturing()) {
            const auto caption=openshao::pause_recomp::CapturingController()?
                openshao::pause_recomp::Localized("settings.hold_cancel"):
                "| "+openshao::pause_recomp::Localized("footer.cancel");
            if(const auto text=openshao::ed_nexus::CachedFooter(caption))ctx.eax=text;
        } else {
            const bool adjust=settings && openshao::pause_recomp::CanAdjust(row);
            const bool accept=!settings || openshao::pause_recomp::CanChoose(row);
            const bool navigate=!settings || openshao::pause_recomp::RowCount()>1;
            ctx.eax=openshao::ed_nexus::BackFooter(ctx.eax,adjust,accept,navigate);
        }
        if(settings)openshao::ed_nexus::pause_menu.footer_text=ctx.eax;
    }
}
