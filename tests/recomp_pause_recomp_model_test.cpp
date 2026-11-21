#include "pause_recomp.h"
#include "launcher.h"
#include "recomp_input.h"
#include "core/input.h"
#include "core/log.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace {
openshao::launcher::Settings settings;
unsigned saves=0,live=0,exits=0;
input::Pad pad;
}
namespace input {Pad Poll(int port){return port==0?::pad:Pad{};}}
namespace openshao::launcher {
const Settings& Current(){return ::settings;}
bool ApplyInGame(const Settings& next,bool save,std::string& error){
    assert(next.language==::settings.language && next.voice_language==::settings.voice_language);
    ::settings=next;
    if(save)++saves;else ++live;
    error.clear();return true;
}
}
namespace openshao {bool RequestExit(){++exits;return true;}}
namespace logx {void Write(Level,const char*,...){}}
int main(){
    using namespace openshao::pause_recomp;
    const auto change=[](unsigned row){assert(Adjust(row,1));};
    const auto turn=[](unsigned trigger){
        assert(!Update());
        pad.analog[trigger]=255;
        assert(Update());
        assert(!Update()); // Holding a trigger must not scroll through the remaining pages.
        pad.analog[trigger]=0;
        assert(!Update());
    };
    for(const auto& alias:std::array<std::pair<const char*,const char*>,4>{{
        {"rightpaddle1","paddle1"},{"leftpaddle1","paddle2"},
        {"rightpaddle2","paddle3"},{"leftpaddle2","paddle4"}}}) {
        assert(openshao::bindings::ValidSource(alias.first));
        assert(openshao::bindings::CanonicalSource(alias.first)==alias.second);
        assert(openshao::bindings::ValidSource(alias.second));
    }
    settings.language="pt-BR";settings.voice_language="pt-BR";
    pad.connected=true;
    assert(Localized("binding.camera_recenter")=="Recentralizar C\xE2mera");
    Open(Page::Controller);
    assert(RowText(0)==Localized("binding.a")+": "+Localized("source.a"));
    assert(RowCount()==3 && RowText(3).empty());
    assert(PageIndicator()==std::string("#   P\xE1gina 1/9   $"));
    pad.analog[7]=255;
    assert(!Update() && !Update()); // A trigger held before opening the list waits for release.
    pad.analog[7]=0;
    assert(!Update());
    turn(6);
    assert(RowCount()==1 && RowText(0)==Localized("binding.camera_recenter")+": "+Localized("source.leftstick"));
    assert(PageIndicator().find("9/9")!=std::string::npos);
    turn(7);
    assert(RowCount()==3 && PageIndicator().find("1/9")!=std::string::npos);
    Open();
    assert(RowCount()==6 && RowText(0)=="Tela" && RowText(5)=="Cheats" && RowText(6).empty() && PageIndicator().empty());
    Choose(2);assert(CurrentPage()==Page::Controls && RowCount()==5);
    const auto deadzone=settings.deadzone;
    change(3);assert(settings.deadzone!=deadzone && saves==1);
    change(4);assert(settings.rumble==false && saves==2);
    Choose(0);assert(CurrentPage()==Page::Controller && RowCount()==3);
    Choose(0);assert(Capturing() && RowText(0).find("Pressione")!=std::string::npos);
    openshao::recomp::raw::Controller("",0);
    openshao::recomp::raw::Controller("paddle1",0);
    assert(Update() && !Capturing() && settings.bindings[0]=="paddle1" && saves==3);
    assert(RowText(0).ends_with("Bot\xE4o Traseiro Direito 1"));
    Choose(1);openshao::recomp::raw::Controller("",0);openshao::recomp::raw::Controller("paddle1",0);
    pad.analog[7]=255;
    assert(PageIndicator().empty());
    assert(Update() && settings.bindings[1]=="b" && saves==3); // Conflict cannot steal other binding.
    assert(!Update() && PageIndicator().find("1/9")!=std::string::npos);
    pad.analog[7]=0;assert(!Update());
    turn(7);assert(RowCount()==3 && RowText(0)==Localized("binding.y")+": "+Localized("source.y"));
    assert(!Back() && CurrentPage()==Page::Controls);
    Choose(1);assert(CurrentPage()==Page::Keyboard);
    Choose(0);assert(Capturing());
    openshao::recomp::raw::Key('Z',true);
    assert(Update() && !Capturing() && settings.keyboard.keys[0][0]=='Z' && saves==4);
    openshao::recomp::raw::Key('Z',false);
    for(unsigned i=0;i<8;++i)turn(7);
    assert(CurrentPage()==Page::Keyboard && RowCount()==1 && RowText(0)==Localized("binding.camera_recenter")+": "+Localized("key.192"));
    turn(7);
    assert(CurrentPage()==Page::KeyboardAlternate && RowCount()==3 && PageIndicator().find("10/18 - Teclas Alternativas")!=std::string::npos);
    turn(6);assert(CurrentPage()==Page::Keyboard && RowCount()==1);
    turn(7);
    Choose(0);openshao::recomp::raw::Key('P',true);
    assert(Update() && settings.keyboard.keys[0][1]=='P' && saves==5);
    openshao::recomp::raw::Key('P',false);
    Back();Choose(2);
    assert(CurrentPage()==Page::Mouse && RowCount()==4);
    change(0);assert(settings.mouse.sensitivity==125 && saves==6);
    change(1);assert(settings.mouse.invert_x && saves==7);
    turn(7);assert(RowText(0)==Localized("mouse.left")+": < "+Localized("binding."+settings.mouse.bindings[0])+" >");
    const auto mouse_before=settings.mouse.bindings[0];change(0);
    assert(settings.mouse.bindings[0]!=mouse_before && saves==8);
    turn(7);assert(RowCount()==3 && !Adjust(3,1) && PageIndicator().find("3/3")!=std::string::npos);
    turn(6);assert(RowCount()==4 && PageIndicator().find("2/3")!=std::string::npos);
    assert(!Back() && CurrentPage()==Page::Controls);
    assert(!Back() && CurrentPage()==Page::Categories);
    Choose(1);assert(CurrentPage()==Page::Graphics && RowCount()==5);
    const auto before_accept=settings;const auto saves_before_accept=saves;
    for(unsigned row=0;row<5;++row)Choose(row);
    assert(settings==before_accept && saves==saves_before_accept);
    assert(RowText(0).find("Escala")!=std::string::npos && RowText(1).find("Filtro")!=std::string::npos);
    assert(RowText(2).find("V-Sync")!=std::string::npos && RowText(3).find("Bloom")!=std::string::npos);
    assert(RowText(4).find("quadros")!=std::string::npos && RowText(5).empty());
    change(3);assert(settings.bloom_effect && saves==9);
    change(1);assert(settings.aniso==2 && saves==10);
    change(0);assert(settings.render_percent==105 && saves==11);
    Back();Choose(0);assert(CurrentPage()==Page::Display && RowCount()==5);
    assert(RowText(1).find("Auto")!=std::string::npos);
    const auto before_mode=settings.display_mode;
    change(0);assert(CurrentPage()==Page::Display && settings.display_mode==before_mode && live==0 && saves==11);
    Choose(4);assert(settings.display_mode==before_mode && live==0);
    change(0);Choose(3);assert(CurrentPage()==Page::DisplayConfirm && RowCount()==2 && live==1 && saves==11);
    Back();assert(CurrentPage()==Page::Display && settings.display_mode==before_mode && live==2);
    change(1);assert(CurrentPage()==Page::Display && settings.width==0 && settings.height==0);
    Choose(3);assert(CurrentPage()==Page::DisplayConfirm && settings.width==640 && settings.height==480);
    Choose(0);assert(CurrentPage()==Page::Display && saves==12);
    change(2);Choose(3);assert(CurrentPage()==Page::DisplayConfirm && settings.aspect_ratio!="original");
    Close();assert(settings.aspect_ratio=="original" && !Capturing());
    Open();Choose(1);change(2);assert(settings.vsync==false && saves==13);
    Back();Choose(3);assert(RowCount()==1 && RowText(1).empty());
    assert(Adjust(0,-1) && settings.master_volume==90 && saves==14);
    Back();Choose(4);assert(CurrentPage()==Page::Interface && RowCount()==2 && RowText(0).find("FPS")!=std::string::npos);
    change(0);assert(settings.show_fps && saves==15);
    change(1);assert(!settings.screenshot_notifications && saves==16 && RowText(1).find("< ")!=std::string::npos);
    assert(!CanChoose(0) && !CanChoose(1) && RowText(2).empty());
    Back();assert(Back());
    Open();Choose(1);
    assert(RowText(4).find("60 FPS")!=std::string::npos);
    const auto vsync=settings.vsync;
    unsigned expected_saves=saves;
    for(const int fps:{120,144,165,240,60}) {
        change(4);
        assert(settings.fps_cap==fps && RowText(4).find(std::to_string(fps)+" FPS")!=std::string::npos);
        assert(settings.vsync==vsync && saves==++expected_saves);
    }
    change(4);assert(settings.fps_cap==120 && RowText(4).find("120 FPS")!=std::string::npos);
    change(4);assert(settings.fps_cap==144 && RowText(4).find("144 FPS")!=std::string::npos);
    change(4);assert(settings.fps_cap==165 && RowText(4).find("165 FPS")!=std::string::npos);
    change(4);assert(settings.fps_cap==240 && RowText(4).find("240 FPS")!=std::string::npos);
    change(4);assert(settings.fps_cap==60 && RowText(4).find("60 FPS")!=std::string::npos);
    assert(!CanChoose(5) && !Back() && CurrentPage()==Page::Categories);
    settings.vsync=true;Choose(1);
    assert(RowText(4).find("Frequ")!=std::string::npos);
    const auto saved_before=saves;Choose(4);
    assert(!CanAdjust(4));
    assert(settings.fps_cap==60 && saves==saved_before);
    change(2);assert(!settings.vsync && CanAdjust(4));change(4);assert(settings.fps_cap==120);Back();
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"}){
        settings.language=locale;
        Open();
        for(unsigned category=0;category<5;++category){
            Choose(category);
            for(unsigned row=0;row<RowCount();++row)assert(!RowText(row).empty());
            Back();
        }
    }
    Open();Choose(2);Choose(0);turn(6);
    const auto before_camera=saves;
    assert(RowCount()==1 && RowText(0).ends_with(Localized("source.leftstick")));
    Choose(0);
    openshao::recomp::raw::Controller("",0);
    openshao::recomp::raw::Controller("rightstick",0);
    assert(Update() && settings.bindings[openshao::bindings::RecenterCamera]=="rightstick" && saves==before_camera+1);
    assert(RowText(0).ends_with(Localized("source.rightstick")));
    Open(Page::Enhancements);
    assert(RowText(0).find("Scorpion")!=std::string::npos && RowCount()==4);
    for(unsigned row=0;row<6;++row)assert(CanAdjust(row)==(row<3));
    const auto before_enhancement_accept=settings;const auto before_enhancement_saves=saves;
    for(unsigned row=0;row<3;++row)Choose(row);
    assert(settings==before_enhancement_accept && saves==before_enhancement_saves);
    for(unsigned row=0;row<6;++row)assert(CanChoose(row)==(row==3));
    // The pause menu must retain the complete captions used by the main settings.
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"}) {
        settings.language=locale;
        const auto suffix=": < "+Localized("value.off")+" >";
        for(const auto [row,key]:std::array<std::pair<unsigned,const char*>,3>{{
            {0,"settings.scorpion_unfiltered"},{1,"settings.restore_dialogue"},
            {2,"settings.cinematic_bars"}}})
            assert(RowText(row)==Localized(key)+": < "+Localized(row==1?"value.on":"value.off")+" >");
        Choose(3);
        assert(RowText(2)==Localized("settings.camera_smoothing")+suffix);
        Back();
    }
    const auto old_scorpion=settings.scorpion_unfiltered,old_dialogue=settings.restore_dialogue;
    const auto old_bars=settings.cinematic_bars;
    const auto enhancement_saves=saves;
    for(unsigned row=0;row<3;++row)assert(Adjust(row,1));
    assert(settings.scorpion_unfiltered!=old_scorpion && settings.restore_dialogue!=old_dialogue &&
           settings.cinematic_bars!=old_bars && saves==enhancement_saves+3);
    assert(!Adjust(3,1));Choose(3);
    assert(CurrentPage()==Page::EnhancedCamera && RowCount()==3 && RowText(1).find("< 100% >")!=std::string::npos);
    const auto camera_saves=saves;
    assert(Adjust(0,1) && Adjust(1,1) && Adjust(1,-1) && Adjust(2,1));
    assert(settings.hold_camera_angle && settings.smooth_camera_framing && settings.camera_speed==100 && saves==camera_saves+4);
    Back();assert(RowCount()==4 && RowText(0).find("Scorpion")!=std::string::npos);
    assert(Back());
    settings.language="pt-BR";Open();Choose(5);
    assert(CurrentPage()==Page::Cheats && RowCount()==2 && RowText(1).find("F8")!=std::string::npos);
    const auto cheat_saves=saves;
    assert(Adjust(0,1) && settings.cheats_enabled && saves==cheat_saves+1);
    assert(Adjust(1,1) && settings.cheat_menu_key==0x78 && RowText(1).find("F9")!=std::string::npos);
    Back();
    assert(exits==0);
    Open(Page::Controller);
    settings.bindings[1]="none";
    const auto previous=settings.bindings[2];
    const auto before_b=saves;
    const auto now=openshao::recomp::raw::Clock::now();
    Choose(2);
    openshao::recomp::raw::Controller("",0,now);
    openshao::recomp::raw::Controller("b",0x20,now);
    assert(!Update() && CapturingController());
    openshao::recomp::raw::Controller("b",0x20,now+std::chrono::seconds(1));
    assert(Update() && !Capturing() && CurrentPage()==Page::Controller && settings.bindings[2]==previous && saves==before_b);
    openshao::recomp::raw::Controller("",0);
    Choose(2);
    openshao::recomp::raw::Controller("",0,now);
    openshao::recomp::raw::Controller("b",0x20,now);
    openshao::recomp::raw::Controller("",0,now+std::chrono::milliseconds(130));
    assert(Update() && !Capturing() && settings.bindings[2]=="b" && saves==before_b+1);
    Choose(2);openshao::recomp::raw::FocusLost();
    assert(Update() && !Capturing() && CurrentPage()==Page::Controller && settings.bindings[2]=="b" && saves==before_b+1);
    std::puts("PASS: pause categories, immediate settings, remapping/conflicts, screen rollback/confirm, six locales");
}
