#include "pause_recomp.h"
#include "launcher.h"
#include "host.h"
#include "recomp_input.h"
#include "menu_text.h"
#include "mods/languages.h"
#include "save_pages.h"
#include "frame_rate.h"
#include "core/input.h"
#include "core/log.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>

namespace openshao::pause_recomp {
namespace {
Page page=Page::Categories;
Page entry_page=Page::Categories;
unsigned mapping_page=0;
constexpr unsigned BindingsPerPage=3;
constexpr unsigned BindingPages=(bindings::Count+BindingsPerPage-1)/BindingsPerPage;
constexpr unsigned MouseButtonsPerPage=4;
constexpr unsigned MousePages=1+(mouse::button_keys.size()+MouseButtonsPerPage-1)/MouseButtonsPerPage;
save_pages::Triggers triggers;
bool paging_opened=true;
bool KeyboardPage(){return page==Page::Keyboard || page==Page::KeyboardAlternate;}
bool BindingPage(){return page==Page::Controller || KeyboardPage();}
unsigned BindingIndex(unsigned row) {return mapping_page*BindingsPerPage+row;}
unsigned BindingRows() {return std::min(BindingsPerPage,bindings::Count-BindingIndex(0));}
unsigned MouseRows() {
    if(!mapping_page)return 4;
    return std::min(MouseButtonsPerPage,unsigned(mouse::button_keys.size())-(mapping_page-1)*MouseButtonsPerPage);
}
unsigned PageCount() {
    if(KeyboardPage())return 2*BindingPages;
    if(page==Page::Controller)return BindingPages;
    if(page==Page::Mouse)return MousePages;
    return 0;
}
unsigned PageIndex(){return mapping_page+(page==Page::KeyboardAlternate?BindingPages:0);}
bool capturing=false,display_trial=false;
unsigned capture_row=0;
launcher::Settings original_display,proposed_display;
std::chrono::steady_clock::time_point display_deadline;
int last_display_seconds=0;
std::string status;
std::string Local(std::string_view id) {
    const auto& locale=launcher::Current().language;
    return menu::EncodeNative(menu::Caption(locale,id),mods::SameLanguage(locale,"pt-BR"));
}
bool UpdatePaging() {
    const auto count=PageCount();
    if(!count)return false;
    uint8_t left=0,right=0;
    for(int port=0;port<2;++port) {
        const auto pad=::input::Poll(port);
        if(!pad.connected)continue;
        left=std::max(left,pad.analog[6]);
        right=std::max(right,pad.analog[7]);
    }
    if(paging_opened || capturing) {
        triggers.Resume(left,right);
        paging_opened=false;
        return false;
    }
    const int direction=triggers.Update(left,right);
    if(!direction)return false;
    const unsigned step=direction<0?count-1:1;
    const unsigned next=(PageIndex()+step)%count;
    if(KeyboardPage()) {
        page=next<BindingPages?Page::Keyboard:Page::KeyboardAlternate;
        mapping_page=next%BindingPages;
    } else mapping_page=next;
    status.clear();
    return true;
}
template<class T,size_t N> void Cycle(T& value,const std::array<T,N>& values,int direction) {
    const auto it=std::find(values.begin(),values.end(),value);
    value=values[it==values.end()?0:(size_t(it-values.begin())+(direction<0?N-1:1))%N];
}
bool ValueRow(unsigned row) {
    switch(page) {
    case Page::Controls:return row==3 || row==4;
    case Page::Graphics:return row<5;
    case Page::Interface:case Page::Cheats:return row<2;
    case Page::Display:case Page::EnhancedCamera:return row<3;
    case Page::Enhancements:return row<3;
    case Page::Audio:return row==0;
    case Page::Mouse:return row<MouseRows();
    default:return false;
    }
}
bool Adjustable(unsigned row) {
    return ValueRow(row) && !(page==Page::Graphics && row==4 && launcher::Current().vsync);
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
    proposed_display=launcher::Current();
    display_trial=false;page=Page::Display;
}
void BeginCapture(unsigned row) {
    capture_row=BindingIndex(row);
    capturing=true;status.clear();
    recomp::raw::Active(true);
    recomp::raw::Begin(page==Page::Controller?recomp::raw::Device::Controller:recomp::raw::Device::Keyboard);
}
constexpr std::array<std::string_view,uint32_t(Item::Count)> ItemKeys{
    "recomp.title","main.exit_desktop","settings.fps","settings.vsync",
    "settings.bloom_effect","settings.back_hint","pause.unsaved",
    "pause.exit_no","pause.exit_yes","recomp.enhancements","photo.mode","pause.save_game"
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
    case Item::ShowFps: return label+": "+Local(settings.show_fps?"value.on":"value.off");
    case Item::Vsync: return label+": "+Local(settings.vsync?"value.on":"value.off");
    case Item::Bloom: return label+": "+Local(settings.bloom_effect?"value.on":"value.off");
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
void Open(Page initial) {
    if(display_trial)UndoDisplay();
    recomp::raw::Active(false);capturing=false;display_trial=false;status.clear();
    mapping_page=0;paging_opened=true;page=entry_page=initial;
}
void Close() {
    if(display_trial)UndoDisplay();
    recomp::raw::Active(false);capturing=false;status.clear();page=Page::Categories;
}
Page CurrentPage(){return page;}
bool Capturing(){return capturing;}
bool CapturingController(){return capturing && page==Page::Controller;}
bool CanAdjust(unsigned row){return row<RowCount() && !capturing && Adjustable(row);}
bool CanChoose(unsigned row){return row<RowCount() && !capturing && !ValueRow(row);}
std::string PageIndicator() {
    const auto count=PageCount();
    if(count<2 || capturing)return {};
    return save_pages::PageIndicator(Local("save.page"),PageIndex()+1,count,
        page==Page::KeyboardAlternate?Local("settings.alternate"):std::string{});
}
unsigned RowCount() {
    switch(page) {
    case Page::Categories:return 6;
    case Page::Controls:case Page::Display:return 5;
    case Page::Enhancements:return 4;
    case Page::Controller:case Page::Keyboard:case Page::KeyboardAlternate:return BindingRows();
    case Page::Mouse:return MouseRows();
    case Page::Graphics:return 5;
    case Page::Interface:return 2;
    case Page::EnhancedCamera:return 3;
    case Page::Cheats:return 2;
    case Page::Audio:return 1;
    case Page::DisplayConfirm:return 2;
    }
    return 0;
}
std::string RowText(unsigned row) {
    const auto& s=page==Page::Display?proposed_display:launcher::Current();
    const auto yes=Local("value.on");
    const auto no=Local("value.off");
    const auto on=[&](bool value){return value?yes:no;};
    const auto pair=[&](std::string name,std::string value){
        if(Adjustable(row))value="< "+value+" >";
        return std::move(name)+": "+std::move(value);
    };
    if (row>=RowCount())return {};
    if(!status.empty() && BindingPage() && capture_row==BindingIndex(row))return status;
    switch(page) {
    case Page::Categories: {
        static constexpr std::array<std::string_view,6> keys{
            "recomp.display","recomp.graphics","recomp.controls","recomp.audio","recomp.interface","recomp.cheats"};
        return Local(keys[row]);
    }
    case Page::Controls:
        switch(row) {
        case 0:return Local("settings.controller");
        case 1:return Local("settings.keyboard");
        case 2:return Local("settings.mouse");
        case 3:return pair(Local("settings.deadzone"),std::to_string(int(s.deadzone*100+.5))+"%");
        case 4:return pair(Local("settings.rumble"),on(s.rumble));
        default:break;
        }
        break;
    case Page::Graphics:
        switch(row) {
        case 0:return pair(Local("settings.scale"),std::to_string(s.render_percent)+"%");
        case 1:return pair(Local("settings.aniso"),s.aniso?std::to_string(s.aniso)+"x":"0");
        case 2:return pair(Local("settings.vsync"),on(s.vsync));
        case 3:return pair(Local("settings.bloom_effect"),on(s.bloom_effect));
        case 4:return pair(Local("settings.frame_rate"),s.vsync?Local("value.display_refresh"):std::to_string(s.fps_cap)+" FPS");
        default:break;
        }
        break;
    case Page::Interface:
        if(row==0)return pair(Local("settings.fps"),on(s.show_fps));
        if(row==1)return pair(Local("settings.notifications"),on(s.screenshot_notifications));
        break;
    case Page::Display:
        switch(row) {
        case 0:return pair(Local("settings.mode"),
            Local(s.display_mode=="windowed"?"value.windowed":s.display_mode=="fullscreen"?"value.fullscreen":"value.borderless"));
        // Zero size follows the desktop.
        case 1:return pair(Local("pause.resolution"),s.width&&s.height?std::to_string(s.width)+"x"+std::to_string(s.height):Local("value.auto"));
        case 2:return pair(Local("settings.aspect"),s.aspect_ratio=="original"?"4:3":s.aspect_ratio);
        case 3:return Local("settings.apply");
        case 4:return Local("settings.discard_display");
        default:break;
        }
        break;
    case Page::Audio:return pair(Local("settings.master"),std::to_string(s.master_volume)+"%");
    case Page::Enhancements:
        if(row==0)return pair(Local("settings.scorpion_unfiltered"),on(s.scorpion_unfiltered));
        if(row==1)return pair(Local("settings.restore_dialogue"),on(s.restore_dialogue));
        if(row==2)return pair(Local("settings.cinematic_bars"),on(s.cinematic_bars));
        if(row==3)return Local("settings.enhanced_camera")+" >";
        break;
    case Page::EnhancedCamera:
        if(row==0)return pair(Local("settings.camera_enabled"),on(s.hold_camera_angle));
        if(row==1)return pair(Local("settings.camera_speed"),std::to_string(s.camera_speed)+"%");
        if(row==2)return pair(Local("settings.camera_smoothing"),on(s.smooth_camera_framing));
        break;
    case Page::Cheats:
        if(row==0)return pair(Local("settings.cheats"),on(s.cheats_enabled));
        if(row==1)return pair(Local("pause.cheat_key"),bindings::KeyLabel(s.cheat_menu_key));
        break;
    case Page::Controller:case Page::Keyboard:case Page::KeyboardAlternate: {
        const auto action=BindingIndex(row);
        if(capturing && capture_row==action)return Local("pause.capture");
        const auto value=page==Page::Controller?"source."+bindings::CanonicalSource(s.bindings[action]):
            "key."+std::to_string(s.keyboard.keys[action][page==Page::KeyboardAlternate?1:0]);
        return pair(Local("binding."+std::string(bindings::keys[action])),Local(value));
    }
    case Page::Mouse: {
        if(mapping_page==0) {
            switch(row) {
            case 0:return pair(Local("settings.sensitivity"),std::to_string(s.mouse.sensitivity)+"%");
            case 1:return pair(Local("settings.invert_x"),on(s.mouse.invert_x));
            case 2:return pair(Local("settings.invert_y"),on(s.mouse.invert_y));
            default:return pair(Local("settings.motion"),Local("value."+s.mouse.motion));
            }
        }
        const auto index=(mapping_page-1)*MouseButtonsPerPage+row;
        return pair(Local("mouse."+std::string(mouse::button_keys[index])),Local("binding."+s.mouse.bindings[index]));
    }
    case Page::DisplayConfirm:
        if(row==1)return Local("pause.revert_screen");
        return pair(Local("pause.keep_screen"),
            std::to_string(std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(display_deadline-std::chrono::steady_clock::now()).count()+1)))+"s");
    }
    return {};
}
bool Back() {
    if(capturing) {recomp::raw::Cancel();recomp::raw::Active(false);capturing=false;return false;}
    if(page==Page::DisplayConfirm){UndoDisplay();return false;}
    if(page==entry_page)return true;
    if(BindingPage() || page==Page::Mouse)page=Page::Controls;
    else if(page==Page::EnhancedCamera)page=Page::Enhancements;
    else page=Page::Categories;
    mapping_page=0;paging_opened=true;status.clear();return false;
}
void Choose(unsigned row) {
    if(!CanChoose(row))return;
    if(page==Page::Categories) {
        page=std::array{Page::Display,Page::Graphics,Page::Controls,Page::Audio,Page::Interface,Page::Cheats}[row];mapping_page=0;
        if(page==Page::Display)original_display=proposed_display=launcher::Current();
        return;
    }
    if(page==Page::DisplayConfirm) {
        if(row==1){UndoDisplay();return;}
        // The preview is not saved yet: commit, and roll back if writing fails.
        std::string error;
        if(!launcher::ApplyInGame(proposed_display,true,error)) {
            LOGE("Pause display confirmation could not save: %s",error.c_str());
            UndoDisplay();return;
        }
        display_trial=false;page=Page::Display;return;
    }
    if(page==Page::Enhancements && row==3){page=Page::EnhancedCamera;return;}
    if(page==Page::Display && row>=3) {
        if(row==4){proposed_display=launcher::Current();status.clear();return;}
        const auto& current=launcher::Current();
        auto next=current;
        next.display_mode=proposed_display.display_mode;next.width=proposed_display.width;next.height=proposed_display.height;
        next.aspect_ratio=proposed_display.aspect_ratio;
        if(next==current)return;
        original_display=current;proposed_display=next;
        if(Apply(next,false)) {
            display_trial=true;page=Page::DisplayConfirm;display_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            last_display_seconds=15;
        }
        return;
    }
    if(page==Page::Controls && row<3) {
        page=std::array{Page::Controller,Page::Keyboard,Page::Mouse}[row];
        mapping_page=0;paging_opened=true;
        return;
    }
    if(BindingPage())BeginCapture(row);
}
bool Adjust(unsigned row,int direction) {
    if(!direction || !CanAdjust(row))return false;
    const int step=direction<0?-1:1;
    auto next=page==Page::Display?proposed_display:launcher::Current();
    switch(page) {
    case Page::Controls:
        if(row==3)next.deadzone=std::clamp(std::round(next.deadzone*100+step*5)/100.,0.,.5);
        if(row==4)next.rumble=!next.rumble;
        break;
    case Page::Graphics:
        if(row==0)next.render_percent=std::clamp(next.render_percent+step*5,25,400);
        if(row==1)Cycle(next.aniso,std::array{0,2,4,8,16},step);
        if(row==2)next.vsync=!next.vsync;
        if(row==3)next.bloom_effect=!next.bloom_effect;
        if(row==4)Cycle(next.fps_cap,frames::RateOptions,step);
        break;
    case Page::Interface:
        if(row==0)next.show_fps=!next.show_fps;
        if(row==1)next.screenshot_notifications=!next.screenshot_notifications;
        break;
    case Page::Display: {
        if(row==0)Cycle(next.display_mode,std::array<std::string,3>{"fullscreen","borderless","windowed"},step);
        if(row==1) {
            constexpr std::array<std::pair<int,int>,7> sizes{{{0,0},{640,480},{960,540},{1280,720},{1600,900},{1920,1080},{2560,1440}}};
            auto size=std::pair{next.width,next.height};Cycle(size,sizes,step);next.width=size.first;next.height=size.second;
        }
        if(row==2) {
            std::string_view mode=next.aspect_ratio;
            Cycle(mode,aspect::modes,step);next.aspect_ratio=mode;
        }
        proposed_display=next;
        return true;
    }
    case Page::Audio:next.master_volume=std::clamp(next.master_volume+step*10,0,100);break;
    case Page::EnhancedCamera:
        if(row==0)next.hold_camera_angle=!next.hold_camera_angle;
        if(row==1)next.camera_speed=std::clamp(next.camera_speed+step*launcher::CameraSpeedStep,launcher::CameraSpeedMin,launcher::CameraSpeedMax);
        if(row==2)next.smooth_camera_framing=!next.smooth_camera_framing;
        break;
    case Page::Enhancements:
        if(row==0)next.scorpion_unfiltered=!next.scorpion_unfiltered;
        if(row==1)next.restore_dialogue=!next.restore_dialogue;
        if(row==2)next.cinematic_bars=!next.cinematic_bars;
        break;
    case Page::Cheats:
        if(row==0)next.cheats_enabled=!next.cheats_enabled;
        if(row==1)Cycle(next.cheat_menu_key,std::array{0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A},step);
        break;
    case Page::Mouse:
        if(mapping_page==0) {
            if(row==0)next.mouse.sensitivity=std::clamp(next.mouse.sensitivity+step*25,1,300);
            if(row==1)next.mouse.invert_x=!next.mouse.invert_x;
            if(row==2)next.mouse.invert_y=!next.mouse.invert_y;
            if(row==3)Cycle(next.mouse.motion,std::array<std::string,3>{"right_stick","left_stick","none"},step);
        } else {
            const auto index=(mapping_page-1)*MouseButtonsPerPage+row;
            const auto& choices=mouse::actions;
            const auto it=std::find(choices.begin(),choices.end(),next.mouse.bindings[index]);
            const auto count=choices.size();
            next.mouse.bindings[index]=choices[it==choices.end()?0:(size_t(it-choices.begin())+(step<0?count-1:1))%count];
        }
        break;
    default:return false;
    }
    Apply(next,true);return true;
}
bool Update() {
    if(display_trial && std::chrono::steady_clock::now()>=display_deadline){UndoDisplay();return true;}
    if(display_trial) {
        const int seconds=std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(
            display_deadline-std::chrono::steady_clock::now()).count()+1));
        if(seconds!=last_display_seconds){last_display_seconds=seconds;return true;}
    }
    if(UpdatePaging())return true;
    if(!capturing)return false;
    if(recomp::raw::Expired()){
        recomp::raw::Active(false);capturing=false;status="";return true;
    }
    const auto input=recomp::raw::Take();
    if(input.cancelled) {
        recomp::raw::Active(false);capturing=false;status.clear();
        return true;
    }
    if(input.device==recomp::raw::Device::None)return false;
    auto next=launcher::Current();
    bool valid=false;
    if(page==Page::Controller && input.device==recomp::raw::Device::Controller) {
        const auto source=bindings::CanonicalSource(input.source);
        valid=bindings::ValidSource(source) && bindings::ControllerConflicts(next.bindings,capture_row,source).empty();
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
