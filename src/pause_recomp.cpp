#include "pause_recomp.h"
#include "launcher.h"
#include "host.h"
#include "recomp_input.h"
#include "menu_text.h"
#include "frame_rate.h"
#include "core/log.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>

namespace openshao::pause_recomp {
namespace {
Page page=Page::Categories;
unsigned mapping_page=0;
bool capturing=false,display_trial=false;
unsigned capture_row=0;
launcher::Settings original_display,proposed_display;
std::chrono::steady_clock::time_point display_deadline;
int last_display_seconds=0;
std::string status;
std::string Local(std::string_view id) {
    const auto& locale=launcher::Current().language;
    // Same catalog and Latin-1/PT-BR glyph encoding as the other menus.
    return menu::EncodeNative(menu::Catalog::Builtin().Get(locale,id),locale=="pt-BR");
}
std::string Clip(std::string text) {
    // Clip after encoding (one byte per glyph), never inside a UTF-8 sequence.
    if (text.size()>24) text.resize(24);
    return text;
}
template<class T,size_t N> void Cycle(T& value,const std::array<T,N>& values) {
    const auto it=std::find(values.begin(),values.end(),value);
    value=values[(it==values.end()?0:size_t(it-values.begin()+1)%N)];
}
bool Apply(const launcher::Settings& next,bool persist=true) {
    std::string error;
    if (next==launcher::Current())return true;
    if (!launcher::ApplyInGame(next,persist,error)) {
        status=Local("pause.error");
        LOGE("In-game pause settings: %s",error.c_str());return false;
    }
    status.clear();return true;
}
void UndoDisplay() {
    if (display_trial)Apply(original_display,false);
    display_trial=false;page=Page::Display;
}
void BeginCapture(unsigned row) {
    capture_row=mapping_page*3+row;
    capturing=true;status.clear();
    recomp::raw::Active(true);
    recomp::raw::Begin(page==Page::Controller?recomp::raw::Device::Controller:recomp::raw::Device::Keyboard);
}
constexpr std::array<std::string_view,uint32_t(Item::Count)> ItemKeys{
    "recomp.title","main.exit_desktop","settings.fps","settings.vsync",
    "settings.bloom_effect","settings.back_hint","pause.unsaved",
    "pause.exit_no","pause.exit_yes"
};
}
std::string Localized(std::string_view id) {
    return Local(id);
}
std::string Text(Item item) {
    const auto& settings = launcher::Current();
    if(item==Item::Count)return {};
    const auto label=Local(ItemKeys[uint32_t(item)]);
    switch (item) {
    case Item::ShowFps: return Clip(label+": "+Local(settings.show_fps?"value.on":"value.off"));
    case Item::Vsync: return Clip(label+": "+Local(settings.vsync?"value.on":"value.off"));
    case Item::Bloom: return Clip(label+": "+Local(settings.bloom_effect?"value.on":"value.off"));
    default: return label;
    }
    return {};
}
bool Toggle(Item item) {
    auto next = launcher::Current();
    switch (item) {
    case Item::ShowFps: next.show_fps = !next.show_fps; break;
    case Item::Vsync: next.vsync = !next.vsync; break;
    case Item::Bloom: next.bloom_effect = !next.bloom_effect; break;
    default: return false;
    }
    std::string error;
    if (!launcher::ApplyInGame(next, true, error)) {
        LOGE("Pause recompilation settings: %s", error.c_str());
        return false;
    }
    LOGI("Pause recompilation setting updated: %u", unsigned(item));
    return true;
}
bool Exit() {
    const bool requested = openshao::RequestExit();
    if (requested) LOGI("Exit to desktop requested from gameplay pause");
    else LOGE("Exit to desktop: pause close request failed");
    return requested;
}
void Open() {
    if(display_trial)UndoDisplay();
    recomp::raw::Active(false);capturing=false;display_trial=false;status.clear();
    mapping_page=0;page=Page::Categories;
}
void Close() {
    if(display_trial)UndoDisplay();
    recomp::raw::Active(false);capturing=false;status.clear();page=Page::Categories;
}
Page CurrentPage(){return page;}
bool Capturing(){return capturing;}
unsigned RowCount() {
    switch(page) {
    case Page::Controls:case Page::Controller:
    case Page::Keyboard:case Page::KeyboardAlternate:return 6;
    case Page::Categories:return 5;
    case Page::Mouse:return mapping_page==2?5:6;
    case Page::Graphics:return MaxRows;
    case Page::Display:return 4;
    case Page::Audio:case Page::DisplayConfirm:return 2;
    }
    return 0;
}
std::string RowText(unsigned row) {
    const auto& s=launcher::Current();
    const auto back=Local("settings.back_hint");
    const auto yes=Local("value.on");
    const auto no=Local("value.off");
    const auto on=[&](bool value){return value?yes:no;};
    const auto pair=[&](std::string name,std::string value){return Clip(std::move(name)+": "+std::move(value));};
    if (row>=RowCount())return {};
    if(!status.empty() && (page==Page::Controller || page==Page::Keyboard || page==Page::KeyboardAlternate) &&
       row<3 && capture_row==mapping_page*3+row)return Clip(status);
    switch(page) {
    case Page::Categories: {
        static constexpr std::array<std::string_view,5> keys{
            "recomp.display","recomp.graphics","recomp.controls","recomp.audio","settings.back_hint"};
        return Clip(Local(keys[row]));
    }
    case Page::Controls:
        switch(row) {
        case 0:return Clip(Local("pause.map_controller"));
        case 1:return Clip(Local("pause.map_keyboard"));
        case 2:return Clip(Local("pause.mouse_settings"));
        case 3:return pair(Local("settings.deadzone"),std::to_string(int(s.deadzone*100+.5))+"%");
        case 4:return pair(Local("settings.rumble"),on(s.rumble));
        default:return back;
        }
    case Page::Graphics:
        switch(row) {
        case 0:return pair(Local("pause.scale"),std::to_string(s.render_percent)+"%");
        case 1:return pair(Local("pause.aniso"),s.aniso?std::to_string(s.aniso)+"x":"0");
        case 2:return pair(Local("settings.vsync"),on(s.vsync));
        case 3:return pair(Local("settings.bloom_effect"),on(s.bloom_effect));
        case 4:return pair(Local("settings.fps"),on(s.show_fps));
        case 5:return pair(Local("settings.frame_rate"),s.vsync?Local("pause.display_refresh"):std::to_string(s.fps_cap)+" FPS");
        default:return back;
        }
    case Page::Display:
        switch(row) {
        case 0:return pair(Local("settings.mode"),
            Local(s.display_mode=="windowed"?"value.windowed":s.display_mode=="fullscreen"?"value.fullscreen":"value.borderless"));
        // Zero size follows the desktop; long captions may clip in the SELECT frame.
        case 1:return pair(Local("pause.resolution"),s.width&&s.height?std::to_string(s.width)+"x"+std::to_string(s.height):Local("value.auto"));
        case 2:return pair(Local("pause.aspect"),s.aspect_ratio=="original"?"4:3":s.aspect_ratio);
        default:return back;
        }
    case Page::Audio:return row==0?pair(Local("settings.master"),std::to_string(s.master_volume)+"%"):back;
    case Page::Controller:case Page::Keyboard:case Page::KeyboardAlternate: {
        if(row<3) {
            const auto action=mapping_page*3+row;
            const std::string action_name=bindings::keys[action];
            if(capturing && capture_row==action)return Clip(Local("pause.capture"));
            const auto value=page==Page::Controller?s.bindings[action]:bindings::KeyLabel(s.keyboard.keys[action][page==Page::KeyboardAlternate?1:0]);
            return pair(action_name,value);
        }
        if(row==3)return Clip(Local("pause.previous"));
        if(row==4)return pair(Local("pause.next"),std::to_string(mapping_page+1)+"/8");
        return back;
    }
    case Page::Mouse: {
        if(row<RowCount()-2) {
            if(mapping_page==0) {
                switch(row) {
                case 0:return pair(Local("settings.sensitivity"),std::to_string(s.mouse.sensitivity)+"%");
                case 1:return pair(Local("settings.invert_x"),on(s.mouse.invert_x));
                case 2:return pair(Local("settings.invert_y"),on(s.mouse.invert_y));
                default:return pair(Local("settings.motion"),Local(s.mouse.motion=="right_stick"?"value.right_stick":s.mouse.motion=="left_stick"?"value.left_stick":"value.none"));
                }
            }
            const auto index=(mapping_page-1)*4+row;
            if(index>=mouse::button_keys.size())return {};
            return pair(Local("mouse."+std::string(mouse::button_keys[index])),s.mouse.bindings[index]);
        }
        return row==RowCount()-2?pair(Local("pause.next"),std::to_string(mapping_page+1)+"/3"):back;
    }
    case Page::DisplayConfirm:
        if(row==1)return Clip(Local("pause.revert_screen"));
        return pair(Local("pause.keep_screen"),
            std::to_string(std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(display_deadline-std::chrono::steady_clock::now()).count()+1)))+"s");
    }
    return {};
}
bool Back() {
    if(capturing) {recomp::raw::Cancel();recomp::raw::Active(false);capturing=false;return false;}
    if(page==Page::DisplayConfirm){UndoDisplay();return false;}
    if(page==Page::Categories)return true;
    if(page==Page::Controller || page==Page::Keyboard || page==Page::KeyboardAlternate || page==Page::Mouse)page=Page::Controls;
    else page=Page::Categories;
    mapping_page=0;return false;
}
bool Choose(unsigned row) {
    if(row>=RowCount() || capturing)return false;
    if(page==Page::Categories) {
        if(row==4)return true;
        page=std::array{Page::Display,Page::Graphics,Page::Controls,Page::Audio}[row];mapping_page=0;return false;
    }
    if(page==Page::DisplayConfirm) {
        if(row==1){UndoDisplay();return false;}
        // The preview is not saved yet: commit, and roll back if writing fails.
        std::string error;
        if(!launcher::ApplyInGame(proposed_display,true,error)) {
            LOGE("Pause display confirmation could not save: %s",error.c_str());
            UndoDisplay();return false;
        }
        display_trial=false;page=Page::Display;return false;
    }
    if(row==RowCount()-1)return Back();
    auto next=launcher::Current();
    if(page==Page::Controls) {
        if(row<3){page=std::array{Page::Controller,Page::Keyboard,Page::Mouse}[row];mapping_page=0;return false;}
        if(row==3)next.deadzone=next.deadzone>=.5?0.:std::round((next.deadzone*100+5))/100.;
        if(row==4)next.rumble=!next.rumble;
    } else if(page==Page::Graphics) {
        if(row==0)next.render_percent=next.render_percent>=400?25:std::min(400,next.render_percent+5);
        if(row==1)Cycle(next.aniso,std::array{0,2,4,8,16});
        if(row==2)next.vsync=!next.vsync;
        if(row==3)next.bloom_effect=!next.bloom_effect;
        if(row==4)next.show_fps=!next.show_fps;
        if(row==5 && !next.vsync)Cycle(next.fps_cap,frames::RateOptions);
    } else if(page==Page::Display) {
        if(row==0)Cycle(next.display_mode,std::array<std::string,3>{"fullscreen","borderless","windowed"});
        if(row==1) {
            constexpr std::array<std::pair<int,int>,7> sizes{{{0,0},{640,480},{960,540},{1280,720},{1600,900},{1920,1080},{2560,1440}}};
            const auto it=std::find(sizes.begin(),sizes.end(),std::pair{next.width,next.height});
            const auto size=sizes[it==sizes.end()?0:size_t(it-sizes.begin()+1)%sizes.size()];next.width=size.first;next.height=size.second;
        }
        if(row==2) {
            const auto& modes=aspect::modes;
            auto it=std::find(modes.begin(),modes.end(),next.aspect_ratio);
            next.aspect_ratio=modes[it==modes.end()?0:size_t(it-modes.begin()+1)%modes.size()];
        }
        if(row<=2 && next!=launcher::Current()) {
            original_display=launcher::Current();proposed_display=next;
            if(Apply(next,false)) {
                display_trial=true;page=Page::DisplayConfirm;display_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
                last_display_seconds=15;
            }
            return false;
        }
    } else if(page==Page::Audio) {
        if(row==0)next.master_volume=next.master_volume>=100?0:next.master_volume+10;
    } else if(page==Page::Controller || page==Page::Keyboard || page==Page::KeyboardAlternate) {
        if(row<3){BeginCapture(row);return false;}
        if(row==3){mapping_page=(mapping_page+7)%8;return false;}
        if(row==4){
            mapping_page=(mapping_page+1)%8;
            if(mapping_page==0 && page!=Page::Controller)page=page==Page::Keyboard?Page::KeyboardAlternate:Page::Keyboard;
            return false;
        }
    } else if(page==Page::Mouse) {
        if(row==RowCount()-2){mapping_page=(mapping_page+1)%3;return false;}
        if(mapping_page==0) {
            if(row==0)next.mouse.sensitivity=next.mouse.sensitivity>=300?25:std::min(300,next.mouse.sensitivity+25);
            if(row==1)next.mouse.invert_x=!next.mouse.invert_x;
            if(row==2)next.mouse.invert_y=!next.mouse.invert_y;
            if(row==3)Cycle(next.mouse.motion,std::array<std::string,3>{"right_stick","left_stick","none"});
        } else {
            const auto index=(mapping_page-1)*4+row;
            if(index>=mouse::button_keys.size())return false;
            const auto& choices=mouse::actions;
            auto it=std::find_if(choices.begin(),choices.end(),[&](const char* value){return next.mouse.bindings[index]==value;});
            next.mouse.bindings[index]=choices[it==choices.end()?0:size_t(it-choices.begin()+1)%choices.size()];
        }
    }
    Apply(next,true);return false;
}
bool Update() {
    if(display_trial && std::chrono::steady_clock::now()>=display_deadline){UndoDisplay();return true;}
    if(display_trial) {
        const int seconds=std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(
            display_deadline-std::chrono::steady_clock::now()).count()+1));
        if(seconds!=last_display_seconds){last_display_seconds=seconds;return true;}
    }
    if(!capturing)return false;
    if(recomp::raw::Expired()){
        recomp::raw::Active(false);capturing=false;status="";return true;
    }
    const auto input=recomp::raw::Take();
    if(input.device==recomp::raw::Device::None)return false;
    auto next=launcher::Current();
    bool valid=false;
    if(page==Page::Controller && input.device==recomp::raw::Device::Controller) {
        const auto source=bindings::CanonicalSource(input.source);
        valid=bindings::ValidSource(source);
        if(valid && source!="none")for(unsigned i=0;i<next.bindings.size();++i)
            if(i!=capture_row && bindings::CanonicalSource(next.bindings[i])==source)valid=false;
        if(valid)next.bindings[capture_row]=source;
    } else if((page==Page::Keyboard || page==Page::KeyboardAlternate) && input.device==recomp::raw::Device::Keyboard) {
        const unsigned slot=page==Page::KeyboardAlternate?1:0;
        valid=bindings::ValidKey(input.key) && bindings::KeyboardConflicts(next.keyboard,capture_row,slot,input.key).empty();
        if(valid)next.keyboard.keys[capture_row][slot]=input.key;
    }
    recomp::raw::Active(false);capturing=false;
    if(valid)Apply(next,true);
    else {status=Local("pause.conflict");
          LOGI("Gameplay pause mapping rejected: conflict or invalid device");}
    return true;
}
}
