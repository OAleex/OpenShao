#include "recomp_settings.h"
#include "audio_options.h"
#include "restart.h"
#include "recomp_settings_model.h"
#include "recomp_input.h"
#include "menu_renderer.h"
#include "menu_text.h"
#include "game_preferences.h"
#include "launcher.h"
#include "performance_overlay.h"
#include "host.h"
#include "scorpion_voice.h"
#include "survival_mode.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include <bit>
#include <chrono>
#include <charconv>

X86_EXTERN_FUNC(__imp__sub_00015C86);
namespace openshao::recomp {
namespace {
Model model;
bool extra_selected=false,active=false,closing=false,release=true,exit_prompt=false;
constexpr unsigned MainRecompRow=6,MainEnhancementsRow=7,MainExitRow=8,MainRows=9;
unsigned return_row=6;
unsigned selection=0;
std::array<unsigned,size_t(Page::Cheats)+1> selections{};
uint32_t pending_directions=0;
std::string notice;
launcher::Settings trial_previous;
bool trial=false;
bool capture_pending=false;
std::chrono::steady_clock::time_point trial_until;
using Clock=std::chrono::steady_clock;
struct View {Page page;std::string title;std::vector<Row> rows;unsigned selected;};
float ClipProgress(uint32_t scene) {
    const uint32_t clip=kernel::R32(scene+0x2A0);
    if(clip>=64)return 1;
    const float first=float(kernel::R32(scene+0x24+clip*16)),last=float(kernel::R32(scene+0x28+clip*16));
    return first==last?1:std::clamp((float(kernel::R32(scene+0x33C))-first)/(last-first),0.f,1.f);
}
float EaseExpansion(float phase){return phase*phase*(3-2*phase);}
View Snapshot(){
    if(exit_prompt)return {Page::Home,"main.exit_confirm",{{"system.no",{},true},{"system.yes",{},true}},selection};
    return {model.CurrentPage(),model.Title(),model.Rows(),selection};
}
struct Animation {
    View previous;
    uint32_t scene=0;
    bool running=false,back=false;
    bool Busy()const{return running && scene && !kernel::G<uint8_t>(scene+0x340);}
    float Progress()const {
        if(!Busy())return 1;
        const auto clip=kernel::R32(scene+0x2A0);
        if(clip!=24 && clip!=34)return 1;
        return ClipProgress(scene);
    }
    void Start(X86Context& ctx,View view,bool returning) {
        previous=std::move(view);back=returning;scene=kernel::R32(0x5985F4);running=scene!=0;
        if(!running)return;
        // These are the retail Options <-> Audio clips, including their original timing and transforms.
        const auto saved=ctx;const auto pointer=ctx.esp-256,old=kernel::R32(pointer);
        kernel::W32(pointer,back?34:24);ctx.ecx=0x5985E8;
        kernel::CallGuest(ctx,0x1B1240,{pointer});kernel::W32(pointer,old);
        ctx=saved;kernel::CallGuest(ctx,0x1B29B0,{0x592D20});ctx=saved;
    }
} animation;
struct Frame {
    bool valid=false,first=false,second=false;
    float x=0,y=0,row_x=93,row_y=211,next_y=239;
    unsigned alpha=0,row_alpha=0;
};
std::array<Frame,2> frame{};
View Displayed(bool child) {
    if(!animation.running)return Snapshot();
    return child!=animation.back?Snapshot():animation.previous;
}
unsigned RowsPerPage(const View& view){return view.page==Page::Home?8:7;}
unsigned FirstRow(const View& view){return view.selected/RowsPerPage(view)*RowsPerPage(view);}
unsigned VisibleRows(const View& view){const auto first=FirstRow(view);return first<view.rows.size()?std::min(RowsPerPage(view),unsigned(view.rows.size())-first):0;}
float OrnamentExtension(){
    float rows=float(VisibleRows(Snapshot()));
    if(animation.Busy()){const float old=float(VisibleRows(animation.previous));rows=old+(rows-old)*EaseExpansion(animation.Progress());}
    // The retail Options ornament encloses four rows at the custom menu's spacing.
    return (rows-4)*24;
}
float OrnamentEnvelope(uint32_t scene,bool main) {
    const uint32_t clip=kernel::R32(scene+0x2A0);
    if(clip==0)return EaseExpansion(ClipProgress(scene));
    if(main && clip>=7 && clip<=19) {
        const float phase=EaseExpansion(ClipProgress(scene));
        return kernel::R32(scene+0x2C+clip*16)&2 ? phase : 1-phase;
    }
    if(!main && clip==23)return 1-EaseExpansion(ClipProgress(scene));
    return 1;
}
void Sound(X86Context& ctx,uint32_t slot) {
    const auto function=kernel::R32(slot);if(!function)return;
    const auto saved=ctx;kernel::CallGuest(ctx,function,{});ctx=saved;
}
uint32_t Top(){const auto depth=kernel::R32(0x6C9308);return depth<32?kernel::R32(0x6C9308+depth*4):0;}
std::string Translate(std::string_view key) {
    const auto text=menu::Catalog::Builtin().Get(launcher::Current().language,key);
    if(text.empty() && key.starts_with("key.")) {
        int code=0;const auto digits=key.substr(4);
        const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),code);
        if(parsed.ec==std::errc{} && parsed.ptr==digits.data()+digits.size()) {
            auto label=bindings::KeyLabel(code);
            if(label.starts_with("Numpad "))return Translate("key.numpad")+" "+label.substr(7);
            if(label.starts_with("Key "))return Translate("key.generic")+" "+std::to_string(code);
            return label;
        }
    }
    return text.empty()?std::string(key):std::string(text);
}
void State(X86Context& ctx,unsigned selected) {
    kernel::W32(0x598468,selected);
    const auto panel=kernel::R32(0x59843C),state=selected>=6?6:selected+1;
    if(panel && kernel::R32(panel+0x2A0)==state)return;
    const auto saved=ctx;ctx.ecx=0x598430;
    // Use bounded native selection states; state seven is a retail accept animation.
    const auto pointer=ctx.esp-256;const auto previous=kernel::R32(pointer);
    kernel::W32(pointer,state);
    kernel::CallGuest(ctx,0x1B1240,{pointer});kernel::W32(pointer,previous);ctx=saved;
}
void Close(X86Context& ctx) {
    scorpion::StopPreview(ctx);
    if(exit_prompt)LOGR("Exit to desktop confirmation dismissed");
    if(trial){std::string error;launcher::ApplyInGame(trial_previous,false,error);trial=false;}
    closing=true;raw::Active(false);
    const auto saved=ctx;
    // Retail Options Back: stop its music, commit state, clear selection, restart the main track (0x11EAB alone orphaned the loop).
    kernel::CallGuest(ctx,0x14CFA,{});ctx=saved;
    kernel::CallGuest(ctx,0x166DB,{});ctx=saved;
    kernel::W32(0x5985E8+0x38,0);
    ctx.ecx=0x5985E8;kernel::CallGuest(ctx,0x11EAB,{});ctx=saved;
}
void RevertTrial() {
    std::string error;launcher::ApplyInGame(trial_previous,false,error);
    auto& draft=model.Edit();draft.display_mode=trial_previous.display_mode;draft.width=trial_previous.width;draft.height=trial_previous.height;
    draft.aspect_ratio=trial_previous.aspect_ratio;draft.render_percent=trial_previous.render_percent;
    trial=false;notice="settings.reverted";
}
bool Save(X86Context& ctx) {
    std::string error;const auto& desired=model.Draft();
    const bool audio_changed=desired.audio_output!=launcher::Current().audio_output;
    const bool restart=desired.language!=launcher::Current().language || desired.voice_language!=launcher::Current().voice_language;
    if(restart && !restart::Prepare(desired,error)) {
        LOGE("Language restart preparation: %s",error.c_str());notice="settings.error";return false;
    }
    if(!launcher::ApplyInGame(desired,true,error)){
        if(restart)restart::Cancel();
        LOGE("Recomp settings: %s",error.c_str());notice="settings.error";return false;
    }
    if(audio_changed)audio::options::Apply(ctx,desired.audio_output);
    // Reapplying retail music/effects/rules on an unrelated save restarts native audio state.
    model.Open(desired);
    if(restart) {
        if(!restart::Commit(error)) {
            restart::Cancel();LOGE("Language restart: %s",error.c_str());notice="settings.restart_error";return false;
        }
        notice="settings.restarting";closing=true;raw::Active(false);
    } else notice="settings.saved";
    return true;
}
void Process(X86Context& ctx,uint32_t directions,uint32_t pressed) {
    if(closing)return;
    if(trial && std::chrono::steady_clock::now()>=trial_until)RevertTrial();
    if(raw::Expired()){notice="settings.capture_timeout";capture_pending=false;}
    const auto fixed=raw::Navigation(),held=raw::ControllerButtons();
    // Raw controller edges can precede the guest's by frames: consume that one duplicate.
    pending_directions&=held;
    const auto duplicates=directions&pending_directions;
    directions&=~duplicates;pending_directions&=~duplicates;
    pending_directions|=(fixed&0xF000)&held&~directions;
    directions|=fixed&0xF000;pressed|=fixed&0xFF;
    if(animation.Busy()){release=true;return;}
    const auto captured=raw::Take();
    if(captured.device!=raw::Device::None) {
        capture_pending=false;
        const bool accepted=captured.device==raw::Device::Controller?model.ControllerBinding(selection,captured.source):
            model.KeyboardBinding(selection,model.CurrentPage()==Page::KeyboardAlternate?1:0,captured.key);
        notice=accepted?"settings.bound":"settings.shared";Sound(ctx,accepted?0x6CA3B8:0x6CA3BC);release=true;return;
    }
    if(raw::Capturing()!=raw::Device::None) {
        if(pressed&0x20){raw::Cancel();capture_pending=false;notice="settings.capture_cancelled";Sound(ctx,0x6CA3BC);release=true;}
        return;
    }
    if(capture_pending){capture_pending=false;notice="settings.capture_cancelled";release=true;}
    if(release){if(!directions && !pressed)release=false;return;}
    if(exit_prompt) {
        if(directions&0xF000){selection^=1;Sound(ctx,0x6CA3C0);}
        else if(pressed&0x20){Sound(ctx,0x6CA3BC);Close(ctx);}
        else if(pressed&0x40) {
            Sound(ctx,0x6CA3B8);
            if(!selection)Close(ctx);
            else if(openshao::RequestExit()) {LOGR("Exit to desktop requested from main menu");closing=true;raw::Active(false);}
            else {LOGE("Exit to desktop: the window close request failed");notice="main.exit_error";}
        }
        if(directions || pressed)release=true;
        return;
    }
    if(trial) {
        if(pressed&0x40){
            Sound(ctx,0x6CA3B8);trial=false;
            if(!Save(ctx)){std::string error;launcher::ApplyInGame(trial_previous,false,error);}
            else if(!closing)Close(ctx);
            release=true;
        }
        else if(pressed&0x20){Sound(ctx,0x6CA3BC);RevertTrial();release=true;}
        return;
    }
    const auto rows=model.Rows();if(rows.empty())return;
    const auto previous_view=Snapshot();
    selection=std::min(selection,unsigned(rows.size()-1));
    if(directions&0x1000){selection=(selection+unsigned(rows.size())-1)%unsigned(rows.size());if(selection!=previous_view.selected)Sound(ctx,0x6CA3C0);}
    else if(directions&0x4000){selection=(selection+1)%unsigned(rows.size());if(selection!=previous_view.selected)Sound(ctx,0x6CA3C0);}
    else if(directions&0xA000){
        const int direction=directions&0x8000?-1:1;
        const auto previous=model.Draft();
        model.Change(selection,direction);notice.clear();
        if(model.CurrentPage()==Page::Language && !launcher::LanguageAvailable(selection==0?model.Draft().language:model.Draft().voice_language)) {
            if(selection==0)model.Edit().language=previous.language;
            else model.Edit().voice_language=previous.voice_language;
            notice="settings.pack_required";
        }
        Sound(ctx,0x6CA3C0);
    }
    else if(pressed&0x20) {
        Sound(ctx,0x6CA3BC);
        if(model.CurrentPage()==Page::Home || model.CurrentPage()==Page::Enhancements) {
            if(model.Dirty()){notice="settings.unsaved";selection=unsigned(rows.size())-2;}
            else Close(ctx);
        } else {
            if(model.CurrentPage()==Page::Enhancements)scorpion::StopPreview(ctx);
            model.Back();notice.clear();
        }
        release=true;
    } else if(pressed&0x80) {
        if(model.CurrentPage()==Page::Enhancements && selection==0) {
            if(!scorpion::Preview(ctx,model.Draft().scorpion_unfiltered))notice="settings.preview_error";
            else notice.clear();
            release=true;
        } else if(selection<24 && (model.CurrentPage()==Page::Controller || model.CurrentPage()==Page::Keyboard || model.CurrentPage()==Page::KeyboardAlternate)) {
            if(model.CurrentPage()==Page::Controller)model.ControllerBinding(selection,"none");
            else model.KeyboardBinding(selection,model.CurrentPage()==Page::KeyboardAlternate?1:0,0);
            Sound(ctx,0x6CA3C0);
        }
    } else if(pressed&0x40) {
        Sound(ctx,0x6CA3B8);
        if((model.CurrentPage()==Page::Home || model.CurrentPage()==Page::Enhancements) && rows[selection].key=="settings.save") {
            const auto& current=launcher::Current();const auto& desired=model.Draft();
            const bool display=desired.display_mode!=current.display_mode || desired.width!=current.width || desired.height!=current.height || desired.aspect_ratio!=current.aspect_ratio || desired.render_percent!=current.render_percent;
            if(display) {
                trial_previous=current;std::string error;
                if(launcher::ApplyInGame(desired,false,error)){trial=true;trial_until=std::chrono::steady_clock::now()+std::chrono::seconds(15);notice="settings.keep_display";}
                else {LOGE("Recomp display: %s",error.c_str());notice="settings.error";}
            } else if(Save(ctx) && !closing)Close(ctx);
        } else if((model.CurrentPage()==Page::Home || model.CurrentPage()==Page::Enhancements) && rows[selection].key=="settings.discard"){model.Discard();Close(ctx);}
        else if(selection<24 && (model.CurrentPage()==Page::Controller || model.CurrentPage()==Page::Keyboard || model.CurrentPage()==Page::KeyboardAlternate)) {
            raw::Begin(model.CurrentPage()==Page::Controller?raw::Device::Controller:raw::Device::Keyboard);capture_pending=true;notice="settings.capture";
        } else {
            const auto previous=model.CurrentPage();model.Enter(selection);
            if(previous==model.CurrentPage() && !rows[selection].action)model.Change(selection,1);
        }
        release=true;
    }
    if(!closing && previous_view.page!=model.CurrentPage()) {
        selections[size_t(previous_view.page)]=previous_view.selected;
        selection=std::min(selections[size_t(model.CurrentPage())],unsigned(model.Rows().size()-1));
        animation.Start(ctx,previous_view,(pressed&0x20)!=0);
    }
}
}
bool Active(){return active;}
bool ExtraSelected(){return extra_selected;}
bool CustomScene(uint32_t scene){return active && scene && scene==kernel::R32(0x5985F4);}
unsigned Selection(){return selection;}
unsigned SelectorY(){const auto view=Snapshot();return 188+24*(view.selected-FirstRow(view));}
std::string Title(){return Snapshot().title;}
void RestoreMainSelector(){
    if(Top()!=0x598430 || active || survival::Transitioning() || survival::SetupPopup())return;
    const auto scene=kernel::R32(0x59843C);
    const auto clip=scene?kernel::R32(scene+0x2A0):0;
    if(clip>=1 && clip<=6)kernel::W32(0x592D20+0xC,1);
}
bool MainInput(X86Context& ctx,uint8_t* base) {
    if(ctx.ecx!=0x598430)return false;
    const unsigned highlighted=kernel::R32(0x598468);
    const bool game_mode=Top()==0x598430 &&
        (survival::MedallionMenu() || highlighted==menu::MainGameModeRow);
    raw::NativeEnterAccept(game_mode);
    // Consume Enter on this update so it cannot activate another page later.
    const bool enter_pressed=raw::TakeEnter() && game_mode;
    if(closing && Top()==0x598430){
        active=closing=exit_prompt=false;extra_selected=true;
        State(ctx,return_row);
        RestoreMainSelector();
    }
    const auto directions=kernel::R32(kernel::R32(ctx.esp+4)),pressed=kernel::R32(kernel::R32(ctx.esp+8));
    const auto panel=kernel::R32(0x59843C),status=panel?kernel::R32(panel+0x2E4):UINT32_MAX;
    // Consume input during Main's row-focus animation, before Main accepts another mode.
    if(survival::MenuInput(ctx,base,enter_pressed))return true;
    if(status!=0 && status!=2)return false;
    if(survival::MainMenu(ctx)){kernel::Return(ctx,0,2);return true;}
    const bool accept=!!(pressed&0x40) || enter_pressed;
    const auto selected=kernel::R32(0x598468);
    extra_selected=selected>=MainRecompRow;
    const bool up=(directions&0x1000)!=0,down=!up && (directions&0x4000)!=0;
    if(selected==menu::MainGameModeRow) {
        if(up || down) {
            Sound(ctx,0x6CA3C0);extra_selected=false;State(ctx,up?2:3);
        } else if(accept) {
            Sound(ctx,0x6CA3B8);survival::Open(ctx,base);
        }
        kernel::Return(ctx,0,2);return true;
    }
    if((selected==2 && down) || (selected==3 && up)) {
        Sound(ctx,0x6CA3C0);extra_selected=true;State(ctx,menu::MainGameModeRow);kernel::Return(ctx,0,2);return true;
    }
    if(extra_selected) {
        if(directions&0x1000){Sound(ctx,0x6CA3C0);extra_selected=selected>MainRecompRow;State(ctx,selected-1);}
        else if(directions&0x4000){Sound(ctx,0x6CA3C0);const auto next=(selected+1)%MainRows;extra_selected=next>=MainRecompRow;State(ctx,next);}
        else if(pressed&0x40) {
            return_row=selected;exit_prompt=selected==MainExitRow;
            if(exit_prompt)LOGR("Exit to desktop confirmation opened");
            auto settings=launcher::Current();preferences::ReadActive(settings);settings.show_fps=performance::Visible();
            settings.language=launcher::Saved().language;settings.voice_language=launcher::Saved().voice_language;model.Open(settings);
            if(selected==MainEnhancementsRow)model.Go(Page::Enhancements);
            selection=0;selections={};pending_directions=0;notice.clear();release=true;active=true;closing=false;animation.running=false;frame={};raw::Active(true);
            kernel::W32(0x598468,4);__imp__sub_00015C86(ctx,base);kernel::W32(0x598468,selected);return true;
        }
        kernel::Return(ctx,0,2);return true;
    }
    if((selected==0 && (directions&0x1000)) || (selected==5 && !(directions&0x1000) && (directions&0x4000))) {
        Sound(ctx,0x6CA3C0);extra_selected=true;State(ctx,selected==0?MainExitRow:MainRecompRow);kernel::Return(ctx,0,2);return true;
    }
    return false;
}
void Draw(X86Context& ctx) {
    if(!kernel::G<uint8_t>(0x6C92FD) || kernel::R32(0x6CA2EC))return;
    if(!active)return;
    if(trial && std::chrono::steady_clock::now()>=trial_until)RevertTrial();
    unsigned visible=0;
    for(unsigned group=0;group<frame.size();++group) {
        const auto& f=frame[group];if(!f.valid || (group && !animation.running))continue;
        const auto view=Displayed(group!=0);const auto& rows=view.rows;const auto first=FirstRow(view),limit=RowsPerPage(view);
        const unsigned alpha=f.first?f.row_alpha:f.alpha;
        visible=std::max(visible,alpha);if(!alpha)continue;
        const float x=f.first?f.row_x:93+f.x,y=f.first?f.row_y:211+f.y;
        const float step=f.first && f.second?24.f*(f.next_y-f.row_y)/28.f:24.f;
        const auto opacity=[&](unsigned value){return (value*alpha+127)/254;};
        for(unsigned index=first;index<rows.size() && index<first+limit;++index) {
            const auto& row=rows[index];const float row_y=y+(index-first)*step;
            // Same caption style as Main: the halo label for every row, dimmed when not focused.
            menu::NativeLabel(ctx,x,row_y,Translate(row.key),1.05f,
                opacity(index==view.selected?menu::MainSelectedAlpha:menu::MainIdleAlpha),false,false,row.value.empty()?500:267);
            if(!row.value.empty())menu::NativeLabel(ctx,375+x-93,row_y,Translate(row.value),.85f,opacity(index==view.selected?254:130),false,true,225);
        }
        if(rows.size()>limit)menu::NativeLabel(ctx,556+f.x,183+f.y,std::to_string(view.selected+1)+" / "+std::to_string(rows.size()),.7f,opacity(180),false,true,70);
    }
    if(visible<127 || animation.Busy())return;
    const int footer_y=!exit_prompt && model.CurrentPage()==Page::Home?430:410;
    menu::NativeButton(ctx,70,footer_y,menu::Button::Back);
    menu::NativeLabel(ctx,92,float(footer_y),Translate(trial?"settings.revert":"settings.back_hint"),.9f,254,false,true,145);
    const auto rows=Snapshot().rows;const auto page=model.CurrentPage();
    const bool binding=(page==Page::Controller || page==Page::Keyboard || page==Page::KeyboardAlternate) && selection<24;
    const bool preview=page==Page::Enhancements && selection==0 && !trial;
    const bool adjustable=selection<rows.size() && (!rows[selection].action || binding) && !preview &&
        !(page==Page::Graphics && selection==5 && model.Draft().vsync);
    if(preview) {
        menu::NativeButton(ctx,250,footer_y,menu::Button::Preview);
        menu::NativeLabel(ctx,272,float(footer_y),Translate("settings.listen_hint"),.9f,254,false,true,160);
    } else if(!trial && adjustable) {
        menu::NativeButton(ctx,250,footer_y,menu::Button::Adjust);
        menu::NativeLabel(ctx,300,float(footer_y),Translate("settings.adjust_hint"),.7f,254,false,true,132);
    }
    menu::NativeButton(ctx,450,footer_y,menu::Button::Accept);
    menu::NativeLabel(ctx,472,float(footer_y),Translate(trial?"settings.keep":"settings.edit_hint"),.9f,254,false,true,136);
    auto message=notice;
    if(trial)message=Translate("settings.keep_display")+" "+std::to_string(std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(trial_until-std::chrono::steady_clock::now()).count())));
    else if(!exit_prompt && page==Page::Language && message.empty())message="settings.restart";
    else if(!exit_prompt && page==Page::Enhancements && message.empty()) {
        if(selection==0)message="settings.scorpion_hint";
        else if(selection==1)message="settings.restore_dialogue_hint";
        else if(selection==2)message="settings.cinematic_bars_hint";
        else if(selection==3)message="settings.improved_difficulty_hint";
        else if(selection==4)message="settings.statue_heal_hint";
    }
    else if(!exit_prompt && page==Page::Cheats && message.empty()) {
        if(selection==0)message="settings.cheats_hint";
        else if(selection==1)message="settings.cheat_key_hint";
    }
    if(!message.empty())menu::NativeLabel(ctx,55,453,Translate(message),.72f,254,false,true,550);
}
bool Graphic(X86Context& ctx,uint32_t scene,uint32_t key,uint32_t first,uint32_t end) {
    if(first>=end || end>512)return false;
    const bool main=scene && scene==kernel::R32(0x59843C),custom=CustomScene(scene);
    const bool compact_survival=main && survival::OverlayShown() && Top()==0x598430;
    const auto id=menu::Catalog::Builtin().Graphic(key);
    if(main && survival::HideMainCaption(id)){kernel::W32(0x63F5E0,first);return true;}
    if(main && id=="main.single_player" && survival::OverlayShown() && Top()==0x598430) {
        // The locked hint follows Main's page fade (clips 1..6 only dim by focus).
        unsigned alpha=0;
        for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v)
            alpha=std::max(alpha,kernel::R32(0x63F5E4+i*0x6C+0x58+v*4)>>24);
        const auto clip=kernel::R32(scene+0x2A0);
        survival::DrawMenu(ctx,clip>=1 && clip<=6?255:alpha);
    }
    if(!main && !custom)return false;
    if(main && survival::SetupPopup() && (id.starts_with("main.") || key==0x59FB5186 ||
       key==0x4599E8C7 || key==0x49B7A657 || key==0x71960991)) {
        kernel::W32(0x63F5E0,first);return true;
    }
    if(compact_survival && key==0x59FB5186) {
        // Same joined medallion as Konfigure Recompilation; drop its extra bronze marker.
        kernel::W32(0x63F5E0,first);return true;
    }
    if(custom && (id.starts_with("options.") || id.starts_with("audio.") || id.starts_with("gameplay."))) {
        const bool child=id.starts_with("audio.");auto& f=frame[child?1:0];
        float left=INFINITY,top=INFINITY;unsigned alpha=0;
        for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v) {
            const auto a=0x63F5E4+i*0x6C;
            left=std::min(left,std::bit_cast<float>(kernel::R32(a+0x10+v*16)));
            top=std::min(top,std::bit_cast<float>(kernel::R32(a+0x14+v*16)));
            alpha=std::max(alpha,kernel::R32(a+0x58+v*4)>>24);
        }
        if(std::isfinite(left+top)) {
            if(id=="options.title" || (id=="audio.title" && animation.running)) {
                f.valid=true;f.x=left+3-55;f.y=top-3-33;f.alpha=alpha;
                if(alpha) {
                    const float width=menu::NativeLabel(ctx,left+3,top-3,Translate(Displayed(child).title),1.5f,alpha,true,false,541);
                    if(alpha>=frame[child?0:1].alpha)menu::TitleWidth(left+3+width);
                }
            } else if(id=="options.brightness" || id=="audio.music") {
                f.first=true;f.row_x=left+3;f.row_y=top+5;
                const auto clip=kernel::R32(scene+0x2A0);
                const unsigned source=!child && (clip==24 || clip==34)?76:254;
                f.row_alpha=std::min(254u,(alpha*254+source/2)/source);
            } else if(id=="options.audio" || id=="audio.effects") {
                f.second=true;f.next_y=top+5;
            }
        }
        kernel::W32(0x63F5E0,first);return true;
    }
    if((key==0x54811410 || key==0x17E4A) && (custom || main)) {
        kernel::W32(0x63F5E0,first);return true;
    }
    if(main && id.starts_with("main.")) {
        constexpr std::string_view labels[]={"main.single_player","main.koop","main.versus","main.load_game","main.options","main.kontent"};
        const auto highlight=menu::MainHighlight(kernel::R32(scene+0x2A0),kernel::R32(0x598468));
        const auto clip=kernel::R32(scene+0x2A0);
        // Inherit native opacity only from entrance/exit clips (page fades).
        if(clip>=1 && clip<=6)for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v){const auto a=0x63F5E4+i*0x6C+0x58+v*4;kernel::W32(a,(kernel::R32(a)&0xFFFFFF)|((id==labels[highlight]?menu::MainSelectedAlpha:menu::MainIdleAlpha)<<24));}
        if(id=="main.options")menu::MainRowAnchor(first,end);
        if(id=="main.versus")menu::MainExtraLabel(ctx,first,end,kernel::R32(0x598468)==menu::MainGameModeRow,highlight==2,menu::MainGameModeRow);
        if(id=="main.kontent")for(unsigned row=MainRecompRow;row<MainRows;++row)menu::MainExtraLabel(ctx,first,end,extra_selected && kernel::R32(0x598468)==row,highlight==5,row);
        if(extra_selected && !compact_survival)menu::DimMenuLabel(first,end,id==labels[highlight]);
    }
    if(key==0x59FB5186 && (custom || main)) {
        // Gold marker beside the sparkle: keep its authored alpha during Main's clips 0..18.
        if(main && (Top()!=0x598430 || (survival::Transitioning() && !survival::MedallionMenu()) || survival::SetupPopup() ||
            kernel::R32(scene+0x2A0)>18)) {
            kernel::W32(0x63F5E0,first);return true;
        }
        const auto status=kernel::R32(scene+0x2E4);
        if(custom && (animation.Busy() || closing || (status!=3 && status!=6))) {
            kernel::W32(0x63F5E0,first);return true;
        }
        float top=INFINITY;
        for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v)
            top=std::min(top,std::bit_cast<float>(kernel::R32(0x63F5E4+i*0x6C+0x14+v*16)));
        // Return clips leave the pointer on their authored Audio row.
        const auto selected=kernel::R32(0x598468);
        const auto visual=menu::MainVisualRow(selected);
        const float pitch=float(kernel::R32(0x4DF4DC)+(kernel::R32(0x5CE864)==1?2:0));
        const float offset=custom?float(SelectorY())-top:
            (extra_selected?pitch*(float(visual)-5):selected>=3?pitch:0)-
            menu::MainMenuRise+menu::MainListAdjustment(visual);
        for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v) {
            const auto address=0x63F5E4+i*0x6C+0x14+v*16;
            const float y=std::bit_cast<float>(kernel::R32(address));
            // Keep the authored x: marker on the emblem shaft, sparkle on its tip.
            kernel::W32(address,std::bit_cast<uint32_t>(y+offset));
        }
    }
    // Extend the existing ornament shaft; translate its carved bottom pieces without stretching them.
    float top=INFINITY,bottom=-INFINITY;
    for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v) {
        const auto a=0x63F5E4+i*0x6C+0x10+v*16;
        const float y=std::bit_cast<float>(kernel::R32(a+4));
        top=std::min(top,y);bottom=std::max(bottom,y);
    }
    const float extension=(compact_survival?survival::OrnamentExtension():
        main?menu::MainOrnamentExtension:OrnamentExtension())*OrnamentEnvelope(scene,main);
    const float shaft_start=main?305.6f:249.3f;
    if(compact_survival && key==0x71960991 && std::abs(top-shaft_start)>=1.f) {
        // Main's long shaft sections would hang below the short medallion's lower cap.
        kernel::W32(0x63F5E0,first);return true;
    }
    // Identify pieces by artwork (positions change while opening); the flipped lower cap shares the upper texture.
    const uint32_t record=0x63F5E4+first*0x6C;
    const bool cap=key==0x4599E8C7 && std::bit_cast<float>(kernel::R32(record+0x4C))>std::bit_cast<float>(kernel::R32(record+0x50));
    const bool joint=key==0x49B7A657;
    const bool shaft=key==0x71960991 && std::abs(top-shaft_start)<1;
    if(cap || joint || shaft)for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v) {
        const auto a=0x63F5E4+i*0x6C+0x14+v*16;
        const float y=std::bit_cast<float>(kernel::R32(a)),shaft_top=std::min(top,bottom+extension-8);
        const float factor=std::clamp((y-top)/std::max(1.f,bottom-top),0.f,1.f);
        const float offset=shaft?shaft_top-top+(bottom+extension-shaft_top-(bottom-top))*factor:extension;
        kernel::W32(a,std::bit_cast<uint32_t>(y+offset));
    }
    if(main && (key==0x79D0AA6F || key==0x4599E8C7 || key==0x49B7A657 || key==0x71960991))
        for(auto i=first;i<end;++i)for(unsigned v=0;v<3;++v) {
            const auto a=0x63F5E4+i*0x6C+0x14+v*16;
            // Upper carving and shaft segments move together to avoid a gap.
            const float shift=key==0x79D0AA6F?-20.f:menu::MainOrnamentOffset;
            kernel::W32(a,std::bit_cast<uint32_t>(std::bit_cast<float>(kernel::R32(a))+shift));
        }
    return false;
}
}
X86_EXTERN_FUNC(__imp__sub_00014679);
X86_FUNC(sub_00014679) {
    if(!openshao::recomp::Active() || ctx.ecx!=0x5985E8){__imp__sub_00014679(ctx,base);return;}
    const auto scene=kernel::R32(0x5985F4);
    if(scene && (kernel::R32(scene+0x2E4)==3 || kernel::R32(scene+0x2E4)==6))
        openshao::recomp::Process(ctx,kernel::R32(kernel::R32(ctx.esp+4)),kernel::R32(kernel::R32(ctx.esp+8)));
    kernel::Return(ctx,0,2);
}
X86_EXTERN_FUNC(__imp__sub_00011D84);
X86_FUNC(sub_00011D84) {
    if(openshao::recomp::Active() && ctx.ecx==0x5985E8){kernel::Return(ctx,0,0);return;}
    __imp__sub_00011D84(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_00011EAB);
X86_FUNC(sub_00011EAB) {
    if(!openshao::recomp::Active() && ctx.ecx==0x5985E8) {
        auto settings=openshao::launcher::Current();const auto previous=settings;
        openshao::preferences::ReadActive(settings);
        if(settings!=previous) {
            settings.language=openshao::launcher::Saved().language;settings.voice_language=openshao::launcher::Saved().voice_language;
            std::string error;
            if(!openshao::launcher::ApplyInGame(settings,true,error))LOGE("Native options persistence: %s",error.c_str());
        }
    }
    __imp__sub_00011EAB(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_000131CF);
X86_FUNC(sub_000131CF){
    if(openshao::recomp::Active() && !openshao::recomp::closing && openshao::recomp::Top()==0x5985E8) {
        const auto panel=kernel::R32(0x5985F4);
        if(panel && (kernel::R32(panel+0x2E4)==3 || kernel::R32(panel+0x2E4)==6) && !openshao::recomp::animation.Busy()) {
            const auto saved=ctx;kernel::CallGuest(ctx,0x1B2390,{0x592D20});ctx=saved;
        }
    }
    openshao::recomp::frame={};
    // Main's attract countdown keeps running under Survival's submenus: refresh it there only.
    if(openshao::survival::MedallionMenu() && openshao::recomp::Top()==0x598430)
        kernel::W32(0x4DF4D8,0xA8C);
    // Keep Main's lake backdrop (0x43D00, thunk 0x13A08) active every frontend frame.
    if(openshao::recomp::Top()==0x598430) {
        const auto scene=kernel::R32(0x59843C);
        if(scene && openshao::menu::MainBackdropClip(kernel::R32(scene+0x2A0))) {
            const auto saved=ctx;kernel::CallGuest(ctx,0x13A08,{});ctx=saved;
        }
    }
    __imp__sub_000131CF(ctx,base);openshao::recomp::Draw(ctx);
}
X86_EXTERN_FUNC(__imp__sub_001B2200);
X86_FUNC(sub_001B2200) {
    const auto pointer=kernel::R32(ctx.esp+4),value=pointer?kernel::R32(pointer):0;
    // Survival keeps Game Mode selected; its roster still returns through Main's clips 8 and 10.
    const bool bounded=openshao::recomp::ExtraSelected() && !openshao::survival::OverlayShown() &&
        ctx.ecx==kernel::R32(0x59843C) && value>=7 && value<=10;
    if(bounded)kernel::W32(pointer,6);
    __imp__sub_001B2200(ctx,base);
    if(bounded)kernel::W32(pointer,value);
}
