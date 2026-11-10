#include "recomp_settings_model.h"
#include "frame_rate.h"
#include <algorithm>
#include <cmath>
namespace openshao::recomp {
namespace {
template<class T,class Range>void Cycle(T& value,const Range& values,int delta) {
    auto it=std::find(values.begin(),values.end(),value);
    int index=it==values.end()?0:int(it-values.begin());
    value=values[size_t((index+int(values.size())+(delta<0?-1:1))%int(values.size()))];
}
std::string Toggle(bool value){return value?"value.on":"value.off";}
std::string Source(std::string_view value){return "source."+std::string(value);}
}
void Model::Open(const launcher::Settings& settings){initial_=draft_=settings;page_=Page::Home;}
std::string Model::Title()const {
    switch(page_) {
    case Page::Home:return "recomp.title";case Page::Display:return "recomp.display";case Page::Graphics:return "recomp.graphics";
    case Page::Controls:return "recomp.controls";case Page::Controller:return "settings.controller";
    case Page::Keyboard:return "settings.keyboard";case Page::KeyboardAlternate:return "settings.alternate";
    case Page::Mouse:return "settings.mouse";case Page::Audio:return "recomp.audio";case Page::Language:return "settings.language";
    case Page::Enhancements:return "recomp.enhancements";
    case Page::Cheats:return "recomp.cheats";
    }
    return "recomp.title";
}
void Model::Back() {
    if(page_==Page::KeyboardAlternate)page_=Page::Keyboard;
    else if(page_==Page::Controller || page_==Page::Keyboard || page_==Page::Mouse)page_=Page::Controls;
    else page_=Page::Home;
}
std::vector<Row> Model::Rows()const {
    std::vector<Row> rows;
    auto add=[&](std::string key,std::string value={},bool action=false){rows.push_back({std::move(key),std::move(value),action});};
    const auto& s=draft_;
    switch(page_) {
    case Page::Home:
        for(const auto* id:{"recomp.display","recomp.graphics","recomp.controls","recomp.audio","settings.language","recomp.cheats","settings.save","settings.discard"})add(id,{},true);
        break;
    case Page::Display:
        add("settings.mode","value."+s.display_mode);add("settings.width",s.width?std::to_string(s.width):"value.desktop");
        add("settings.height",s.height?std::to_string(s.height):"value.desktop");add("settings.aspect",s.aspect_ratio=="original"?"4:3":s.aspect_ratio=="auto"?"value.auto":s.aspect_ratio);
        add("settings.desktop",{},true);break;
    case Page::Graphics:
        add("settings.scale",std::to_string(s.render_percent)+"%");add("settings.aniso",s.aniso?std::to_string(s.aniso)+"x":"value.game_default");
        add("settings.vsync",Toggle(s.vsync));add("settings.bloom_effect",Toggle(s.bloom_effect));add("settings.fps",Toggle(s.show_fps));
        add("settings.frame_rate",s.vsync?"value.display_refresh":std::to_string(s.fps_cap)+" FPS");break;
    case Page::Controls:
        add("settings.controller",{},true);add("settings.keyboard",{},true);add("settings.mouse",{},true);
        add("settings.deadzone",std::to_string(int(std::lround(s.deadzone*100)))+"%");add("settings.rumble",Toggle(s.rumble));break;
    case Page::Controller:
    case Page::Keyboard:
    case Page::KeyboardAlternate:
        for(unsigned i=0;i<24;++i) {
            add("binding."+std::string(bindings::keys[i]),page_==Page::Controller?Source(s.bindings[i]):"key."+std::to_string(s.keyboard.keys[i][page_==Page::KeyboardAlternate?1:0]),true);
        }
        if(page_==Page::Keyboard)add("settings.alternate",{},true);
        add("settings.defaults",{},true);break;
    case Page::Mouse:
        add("settings.sensitivity",std::to_string(s.mouse.sensitivity)+"%");add("settings.invert_x",Toggle(s.mouse.invert_x));add("settings.invert_y",Toggle(s.mouse.invert_y));
        add("settings.motion","value."+s.mouse.motion);
        for(unsigned i=0;i<mouse::button_keys.size();++i)add("mouse."+std::string(mouse::button_keys[i]),"binding."+s.mouse.bindings[i]);
        add("settings.defaults",{},true);break;
    case Page::Audio:
        add("settings.master",std::to_string(s.master_volume)+"%");
        add("audio.output",s.audio_output==0 ? "audio.mono" : s.audio_output==2 ? "audio.surround" : "audio.stereo");break;
    case Page::Language:
        add("settings.text_language","language."+s.language);add("settings.voice_language","language."+s.voice_language);break;
    case Page::Enhancements:
        add("settings.scorpion_unfiltered",Toggle(s.scorpion_unfiltered));
        add("settings.restore_dialogue",Toggle(s.restore_dialogue));
        add("settings.cinematic_bars",Toggle(s.cinematic_bars));
        add("settings.improved_difficulty",Toggle(s.improved_difficulty));
        add("settings.statue_heal",Toggle(s.statue_full_heal));
        add("settings.save",{},true);add("settings.discard",{},true);break;
    case Page::Cheats:
        add("settings.cheats",Toggle(s.cheats_enabled));
        add("settings.cheat_key","key."+std::to_string(s.cheat_menu_key));break;
    }
    return rows;
}
void Model::Enter(size_t row) {
    if(page_==Page::Home) {
        constexpr enum Page pages[]={Page::Display,Page::Graphics,Page::Controls,Page::Audio,Page::Language,Page::Cheats};
        if(row<std::size(pages))page_=pages[row];return;
    }
    if(page_==Page::Controls) {
        if(row<3){constexpr enum Page pages[]={Page::Controller,Page::Keyboard,Page::Mouse};page_=pages[row];}
        return;
    }
    if(page_==Page::Keyboard && row==24){page_=Page::KeyboardAlternate;return;}
    if((page_==Page::Controller && row==24) || (page_==Page::Keyboard && row==25) || (page_==Page::KeyboardAlternate && row==24)) {
        if(page_==Page::Controller){draft_.bindings=bindings::controller_defaults;draft_.deadzone=.15;draft_.rumble=true;}
        else draft_.keyboard={};
    }
    if(page_==Page::Mouse && row==11)draft_.mouse={};
    if(page_==Page::Display && row==4)draft_.width=draft_.height=0;
}
bool Model::ControllerBinding(size_t row,std::string source) {
    if(row>=24 || !bindings::ValidSource(source))return false;
    source=bindings::CanonicalSource(std::move(source));
    if(source!="none")for(size_t i=0;i<draft_.bindings.size();++i)
        if(i!=row && bindings::CanonicalSource(draft_.bindings[i])==source)return false;
    draft_.bindings[row]=std::move(source);return true;
}
bool Model::KeyboardBinding(size_t row,int slot,int key) {
    if(row>=24 || slot<0 || slot>=2 || !bindings::ValidKey(key))return false;
    if(!bindings::KeyboardConflicts(draft_.keyboard,unsigned(row),slot,key).empty())return false;
    draft_.keyboard.keys[row][slot]=key;return true;
}
std::string Model::Conflict(size_t row)const {
    if(row>=24)return {};
    if(page_==Page::Keyboard || page_==Page::KeyboardAlternate) {
        const int slot=page_==Page::KeyboardAlternate?1:0;
        return bindings::KeyboardConflicts(draft_.keyboard,unsigned(row),slot,draft_.keyboard.keys[row][slot]);
    }
    std::string result;
    if(page_==Page::Controller && draft_.bindings[row]!="none")for(unsigned i=0;i<24;++i)
        if(i!=row && draft_.bindings[i]==draft_.bindings[row]) {
            if(!result.empty())result+=", ";result+=bindings::labels[i];
        }
    return result;
}
void Model::Change(size_t row,int direction) {
    if(row>=Rows().size() || !direction)return;
    auto& s=draft_;const int delta=direction<0?-1:1;
    auto number=[&](int& value,int step,int lo,int hi){value=std::clamp(value+delta*step,lo,hi);};
    switch(page_) {
    case Page::Display:
        if(row==0){const std::array<std::string,3> choices{"fullscreen","borderless","windowed"};Cycle(s.display_mode,choices,delta);}
        if(row==1 || row==2){if(!s.width || !s.height){s.width=640;s.height=480;}else if(row==1)number(s.width,16,320,16384);else number(s.height,8,240,16384);}
        if(row==3){std::array<std::string,6> modes;for(unsigned i=0;i<6;++i)modes[i]=aspect::modes[i];Cycle(s.aspect_ratio,modes,delta);}break;
    case Page::Graphics:
        if(row==0)number(s.render_percent,5,25,400);if(row==1)Cycle(s.aniso,std::array{0,2,4,8,16},delta);
        if(row==2)s.vsync=!s.vsync;if(row==3)s.bloom_effect=!s.bloom_effect;if(row==4)s.show_fps=!s.show_fps;
        if(row==5 && !s.vsync)Cycle(s.fps_cap,frames::RateOptions,delta);break;
    case Page::Controls:
        if(row==3)s.deadzone=std::clamp(std::round(s.deadzone*100+delta)/100,0.,.5);if(row==4)s.rumble=!s.rumble;break;
    case Page::Controller:
        if(row<24){
            std::array<std::string,bindings::sources.size()> choices;std::copy(bindings::sources.begin(),bindings::sources.end(),choices.begin());
            auto candidate=s.bindings[row];
            for(size_t attempt=0;attempt<choices.size();++attempt){Cycle(candidate,choices,delta);if(ControllerBinding(row,candidate))break;}
        }break;
    case Page::Keyboard:
    case Page::KeyboardAlternate:
        if(row<24){
            const int slot=page_==Page::KeyboardAlternate?1:0;int candidate=s.keyboard.keys[row][slot];
            for(unsigned attempt=0;attempt<256;++attempt){candidate=(candidate+(delta<0?255:1))%256;if(KeyboardBinding(row,slot,candidate))break;}
        }break;
    case Page::Mouse:
        if(row==0)number(s.mouse.sensitivity,5,1,300);if(row==1)s.mouse.invert_x=!s.mouse.invert_x;if(row==2)s.mouse.invert_y=!s.mouse.invert_y;
        if(row==3){const std::array<std::string,3> modes{"right_stick","left_stick","none"};Cycle(s.mouse.motion,modes,delta);}
        if(row>=4 && row<11){std::array<std::string,mouse::actions.size()> actions;std::copy(mouse::actions.begin(),mouse::actions.end(),actions.begin());Cycle(s.mouse.bindings[row-4],actions,delta);}break;
    case Page::Audio:
        if(row==0)number(s.master_volume,5,0,100);
        if(row==1)Cycle(s.audio_output,std::array{0,1,2},delta);break;
    case Page::Language:
        if(row==0){std::array<std::string,6> languages;std::copy(launcher::languages.begin(),launcher::languages.end(),languages.begin());Cycle(s.language,languages,delta);}
        else if(row==1)Cycle(s.voice_language,launcher::voice_languages,delta);
        break;
    case Page::Enhancements:
        if(row==0)s.scorpion_unfiltered=!s.scorpion_unfiltered;
        if(row==1)s.restore_dialogue=!s.restore_dialogue;
        if(row==2)s.cinematic_bars=!s.cinematic_bars;
        if(row==3)s.improved_difficulty=!s.improved_difficulty;
        if(row==4)s.statue_full_heal=!s.statue_full_heal;
        break;
    case Page::Cheats:
        if(row==0)s.cheats_enabled=!s.cheats_enabled;
        if(row==1){constexpr std::array keys{0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B};Cycle(s.cheat_menu_key,keys,delta);}
        break;
    default:break;
    }
}
}
