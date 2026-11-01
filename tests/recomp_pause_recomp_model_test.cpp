#include "pause_recomp.h"
#include "launcher.h"
#include "recomp_input.h"
#include "core/log.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace {
openshao::launcher::Settings settings;
unsigned saves=0,live=0,exits=0;
}
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
    for(const auto& alias:std::array<std::pair<const char*,const char*>,4>{{
        {"rightpaddle1","paddle1"},{"leftpaddle1","paddle2"},
        {"rightpaddle2","paddle3"},{"leftpaddle2","paddle4"}}}) {
        assert(openshao::bindings::ValidSource(alias.first));
        assert(openshao::bindings::CanonicalSource(alias.first)==alias.second);
        assert(openshao::bindings::ValidSource(alias.second));
    }
    settings.language="pt-BR";settings.voice_language="pt-BR";
    Open();
    assert(RowCount()==6 && RowText(0)=="Tela" && RowText(4)=="Cheats" && RowText(5)=="Voltar");
    assert(!Choose(2) && CurrentPage()==Page::Controls && RowCount()==6);
    const auto deadzone=settings.deadzone;
    Choose(3);assert(settings.deadzone!=deadzone && saves==1);
    Choose(4);assert(settings.rumble==false && saves==2);
    Choose(0);assert(CurrentPage()==Page::Controller && RowCount()==6);
    Choose(0);assert(Capturing() && RowText(0).find("Pressione")!=std::string::npos);
    openshao::recomp::raw::Controller("",0);
    openshao::recomp::raw::Controller("paddle1",0);
    assert(Update() && !Capturing() && settings.bindings[0]=="paddle1" && saves==3);
    Choose(1);openshao::recomp::raw::Controller("",0);openshao::recomp::raw::Controller("paddle1",0);
    assert(Update() && settings.bindings[1]=="b" && saves==3); // Conflict cannot steal other binding.
    Choose(4);assert(RowCount()==6 && RowText(0).find("y:")!=std::string::npos);
    assert(!Back() && CurrentPage()==Page::Controls);
    Choose(1);assert(CurrentPage()==Page::Keyboard);
    Choose(0);assert(Capturing());
    openshao::recomp::raw::Key('Z',true);
    assert(Update() && !Capturing() && settings.keyboard.keys[0][0]=='Z' && saves==4);
    openshao::recomp::raw::Key('Z',false);
    for(unsigned i=0;i<8;++i)Choose(4);
    assert(CurrentPage()==Page::KeyboardAlternate && RowCount()==6);
    Choose(0);openshao::recomp::raw::Key('P',true);
    assert(Update() && settings.keyboard.keys[0][1]=='P' && saves==5);
    openshao::recomp::raw::Key('P',false);
    Back();Choose(2);
    assert(CurrentPage()==Page::Mouse && RowCount()==6);
    Choose(0);assert(settings.mouse.sensitivity==125 && saves==6);
    Choose(1);assert(settings.mouse.invert_x && saves==7);
    Choose(4);assert(RowText(0).find("Esquerdo")!=std::string::npos);
    const auto mouse_before=settings.mouse.bindings[0];Choose(0);
    assert(settings.mouse.bindings[0]!=mouse_before && saves==8);
    assert(!Back() && CurrentPage()==Page::Controls);
    assert(!Back() && CurrentPage()==Page::Categories);
    Choose(1);assert(CurrentPage()==Page::Graphics && RowCount()==7);
    assert(RowText(0).find("Escala")!=std::string::npos && RowText(1).find("Aniso")!=std::string::npos);
    assert(RowText(2).find("V-Sync")!=std::string::npos && RowText(3).find("Bloom")!=std::string::npos);
    assert(RowText(4).find("FPS")!=std::string::npos && RowText(5).find("quadros")!=std::string::npos);
    Choose(3);assert(settings.bloom_effect && saves==9);
    Choose(1);assert(settings.aniso==2 && saves==10);
    Choose(0);assert(settings.render_percent==105 && saves==11);
    Back();Choose(0);assert(CurrentPage()==Page::Display && RowCount()==4);
    assert(RowText(1).find("Auto")!=std::string::npos);
    const auto before_mode=settings.display_mode;
    Choose(0);assert(CurrentPage()==Page::DisplayConfirm && RowCount()==2 && live==1 && saves==11);
    Back();assert(CurrentPage()==Page::Display && settings.display_mode==before_mode && live==2);
    Choose(1);assert(CurrentPage()==Page::DisplayConfirm && settings.width==640 && settings.height==480);
    Choose(0);assert(CurrentPage()==Page::Display && saves==12);
    Choose(2);assert(CurrentPage()==Page::DisplayConfirm && settings.aspect_ratio!="original");
    Close();assert(settings.aspect_ratio=="original" && !Capturing());
    Open();Choose(1);Choose(2);assert(settings.vsync==false && saves==13);
    Back();Choose(3);assert(RowCount()==2);
    Choose(0);assert(settings.master_volume==0 && saves==14);
    Back();Choose(1);Choose(4);assert(settings.show_fps && saves==15);
    Back();assert(Back());
    Open();Choose(1);
    assert(RowText(5).find("60 FPS")!=std::string::npos);
    const auto vsync=settings.vsync;
    unsigned expected_saves=saves;
    for(const int fps:{120,144,165,240,60}) {
        Choose(5);
        assert(settings.fps_cap==fps && RowText(5).find(std::to_string(fps)+" FPS")!=std::string::npos);
        assert(settings.vsync==vsync && saves==++expected_saves);
    }
    Choose(5);assert(settings.fps_cap==120 && RowText(5).find("120 FPS")!=std::string::npos);
    Choose(5);assert(settings.fps_cap==144 && RowText(5).find("144 FPS")!=std::string::npos);
    Choose(5);assert(settings.fps_cap==165 && RowText(5).find("165 FPS")!=std::string::npos);
    Choose(5);assert(settings.fps_cap==240 && RowText(5).find("240 FPS")!=std::string::npos);
    Choose(5);assert(settings.fps_cap==60 && RowText(5).find("60 FPS")!=std::string::npos);
    Choose(6);assert(CurrentPage()==Page::Categories);
    settings.vsync=true;Choose(1);
    assert(RowText(5).find("Monitor")!=std::string::npos);
    const auto saved_before=saves;Choose(5);
    assert(settings.fps_cap==60 && saves==saved_before);
    Choose(2);assert(!settings.vsync);Choose(5);assert(settings.fps_cap==120);Back();
    for(const auto* locale:{"en","fr","de","es","it","pt-BR"}){
        settings.language=locale;
        Open();
        for(unsigned category=0;category<5;++category){
            Choose(category);
            for(unsigned row=0;row<RowCount();++row)assert(RowText(row).size()<=24);
            Back();
        }
    }
    settings.language="pt-BR";Open();Choose(4);
    assert(CurrentPage()==Page::Cheats && RowCount()==3 && RowText(1).find("F8")!=std::string::npos);
    const auto cheat_saves=saves;
    Choose(0);assert(settings.cheats_enabled && saves==cheat_saves+1);
    Choose(1);assert(settings.cheat_menu_key==0x78 && RowText(1).find("F9")!=std::string::npos);
    Choose(2);assert(CurrentPage()==Page::Categories);
    Back();
    assert(exits==0);
    std::puts("PASS: pause categories, immediate settings, remapping/conflicts, screen rollback/confirm, six locales");
}
