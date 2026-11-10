#include "recomp_settings_model.h"
#include <iostream>
int main() {
    using namespace openshao::recomp;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    openshao::launcher::Settings original;original.master_volume=42;original.mouse.sensitivity=165;
    Model model;model.Open(original);
    check(model.Rows().size()==7 && model.CurrentPage()==Page::Home,"Recompilation Home must include five categories and explicit save/discard");
    check(model.Rows()[5].key=="settings.save" && model.Rows()[6].key=="settings.discard","Enhancements must have a separate main-menu entry");
    model.Enter(5);check(model.CurrentPage()==Page::Home,"Saving must not enter a configuration category");
    model.Go(Page::Enhancements);
    check(model.Title()=="recomp.enhancements" && model.Rows().size()==6 && model.Rows()[0].key=="settings.scorpion_unfiltered" &&
          model.Rows()[1].key=="settings.restore_dialogue" && model.Rows()[1].value=="value.on" &&
          model.Rows()[2].key=="settings.cinematic_bars" && model.Rows()[2].value=="value.off" &&
          model.Rows()[3].key=="settings.improved_difficulty" && model.Rows()[3].value=="value.on" &&
          model.Rows()[4].key=="settings.save" && model.Rows()[5].key=="settings.discard",
          "The main-menu Enhancements page must expose its toggles and explicit Save/Discard actions");
    model.Change(2,1);
    check(openshao::launcher::Merge({},model.Draft())["enhancements"]["cinematic_bars"].value_or(false) && model.Dirty(),
          "The cinematic bar row must enable and persist the enhancement");
    model.Discard();model.Go(Page::Enhancements);
    check(model.Rows()[2].value=="value.off","Discard must restore the cinematic bar preference");
    model.Change(3,1);
    check(!openshao::launcher::Merge({},model.Draft())["enhancements"]["improved_difficulty"].value_or(true) && model.Rows()[3].value=="value.off" && model.Dirty(),
          "Players must be able to choose the original difficulty selector");
    model.Discard();model.Go(Page::Enhancements);
    check(model.Rows()[3].value=="value.on" && model.Draft().improved_difficulty,"Discard must restore the improved difficulty preference");
    model.Change(4,1);
    check(model.Rows()[4].key=="settings.save" && !model.Dirty(),
          "Save must remain an action after removing the retired shadow setting");
    model.Change(1,1);
    check(!model.Draft().restore_dialogue && model.Rows()[1].value=="value.off" && model.Dirty(),
          "Players must be able to choose the original dialogue behavior");
    model.Discard();model.Go(Page::Enhancements);
    model.Change(0,1);
    check(openshao::launcher::Merge({},model.Draft())["enhancements"]["scorpion_unfiltered"].value_or(false) && model.Dirty(),
          "Enabling the Scorpion voice preference must edit the settings draft");
    model.Discard();
    check(!openshao::launcher::Merge({},model.Draft())["enhancements"]["scorpion_unfiltered"].value_or(true),
          "Discarding the enhancement must restore the original voice distribution");
    check(model.Rows()[4].key=="settings.language","The fifth category must contain language selection without duplicating original game settings");
    model.Enter(4);
    check(model.Rows().size()==2 && model.Rows()[0].key=="settings.text_language" && model.Rows()[0].value=="language.en","Language must display an autonym from the text catalog");
    check(model.Rows().size()>1 && model.Rows()[1].key=="settings.voice_language" && model.Rows()[1].value=="language.en",
          "Language must expose an explicit English voice selection by default");
    model.Change(1,1);
    check(openshao::launcher::Merge({},model.Draft())["voice_language"].value_or(std::string{})=="fr" && model.Draft().language=="en",
          "Editing voices must preserve text language");
    model.Change(0,1);model.Change(0,1);
    check(model.Draft().language=="de" && openshao::launcher::Merge({},model.Draft())["voice_language"].value_or(std::string{})=="fr",
          "Editing text must preserve an explicit voice choice");
    const std::array<const char*,6> voices{"en","fr","de","es","it","pt-BR"};
    for (const auto* text : openshao::launcher::languages) {
        auto settings=original;settings.language=text;model.Open(settings);model.Go(Page::Language);
        for (size_t index=1;index<=voices.size();++index) {
            model.Change(1,1);
            const std::string expected=voices[index%voices.size()];
            check(model.Draft().voice_language==expected && model.Rows()[1].value=="language."+expected && model.Draft().language==text,
                  "Every text language must offer all six explicit voice autonyms and wrap without changing text");
        }
        model.Change(1,-1);
        check(model.Draft().voice_language=="pt-BR","Voice selection must wrap backward from English to Brazilian Portuguese");
    }
    model.Open(original);
    model.Discard();model.Go(Page::Language);
    model.Change(2,1);model.Change(3,1);model.Change(4,1);model.Change(5,1);
    check(model.Draft()==original,"Removed native settings rows must not modify brightness, versus rules, camera control or native persistence");
    model.Back();
    model.Enter(0);check(model.CurrentPage()==Page::Display,"Display must open from Home");
    model.Change(1,1);check(model.Draft().width>=320 && model.Draft().height>=240,"A custom width must initialize both resolution dimensions");
    model.Back();model.Enter(3);model.Change(0,1);
    check(model.Draft().master_volume>42 && original.master_volume==42,"Edits must remain in an isolated draft until saved");
    const auto audio_rows=model.Rows();
    check(audio_rows.size()==2 && audio_rows[0].key=="settings.master" && audio_rows[1].key=="audio.output" && audio_rows[1].value=="audio.stereo",
          "Recompilation Audio must show Master Volume and Audio Output immediately, with Stereo as the default");
    model.Change(1,1);
    check(model.Draft().audio_output==2 && model.Rows()[1].value=="audio.surround" && original.audio_output==1,
          "Changing Audio Output must select Surround in the draft without changing the opening settings");
    model.Change(1,1);
    check(model.Draft().audio_output==0 && model.Rows()[1].value=="audio.mono","Audio Output must wrap forward to Mono");
    model.Change(1,-1);
    check(model.Draft().audio_output==2,"Audio Output must wrap backward to Surround");
    model.Change(1,-1);model.Change(2,-1);model.Change(3,1);
    check(model.Draft().music_volume==original.music_volume && model.Draft().effects_volume==original.effects_volume && model.Draft().speech_volume==original.speech_volume && model.Draft().original_override==original.original_override,
        "Global audio edits must preserve the native music, effects and speech controls and persistence policy");
    model.Back();model.Enter(2);model.Enter(0);
    check(model.CurrentPage()==Page::Controller && model.Rows().size()==25,"Every controller direction and device reset must be available");
    model.ControllerBinding(0,"rightx-");
    check(model.Draft().bindings[0]=="a" && model.Draft().bindings[22]=="rightx-","An occupied controller input must be refused without changing either action");
    model.ControllerBinding(22,"none");model.ControllerBinding(0,"rightx-");check(model.Draft().bindings[0]=="rightx-","A released analog direction must be assignable");
    check(model.Rows()[0].value=="source.rightx-","Controller values must use localized catalog keys");
    check(model.Conflict(0).empty(),"A refused duplicate must not leave a shared controller binding");
    model.Enter(24);check(model.Draft().bindings==openshao::bindings::controller_defaults,"Device defaults must restore controller bindings");
    check(model.Draft().master_volume>42 && model.Draft().mouse.sensitivity==165,"Controller reset must preserve unrelated devices and audio");
    model.Back();model.Enter(1);model.KeyboardBinding(0,0,'Z');
    check(model.Rows()[0].value=="key.90","Captured keyboard keys must retain their numeric identity for localized display");
    model.KeyboardBinding(1,0,'Z');
    check(model.Draft().keyboard.keys[1][0]==27,"A captured keyboard input already used by another action must be refused");
    model.KeyboardBinding(1,0,73);model.Change(1,1);
    check(model.Draft().keyboard.keys[1][0]==76 && model.Conflict(1).empty(),"Cycling keyboard inputs must skip J and K, which belong to other actions");
    model.Enter(24);check(model.CurrentPage()==Page::KeyboardAlternate,"Alternate keyboard bindings must have their own page");
    model.KeyboardBinding(0,1,'V');
    check(model.Draft().keyboard.keys[0][0]=='Z' && model.Draft().keyboard.keys[0][1]=='V',"Primary and alternate keys must remain independent");
    model.KeyboardBinding(2,1,'Z');
    check(model.Draft().keyboard.keys[2][1]==0,"Alternate keyboard inputs must also reject conflicts with a primary binding");
    model.Go(Page::Controller);model.Change(0,1);
    check(model.Draft().bindings[0]!="b" && model.Conflict(0).empty(),"Controller cycling must skip inputs owned by other actions");
    model.Discard();check(model.Draft()==original,"Discard must restore the complete opening settings snapshot");
    model.Go(Page::Graphics);
    check(model.Rows()[5].value=="value.display_refresh","VSync must identify the display refresh as its output target");
    model.Change(5,1);
    check(model.Draft().fps_cap==original.fps_cap,"The inactive software limit must not change while VSync follows the monitor");
    model.Change(2,1);
    const auto graphics=model.Rows();
    const auto rate=std::find_if(graphics.begin(),graphics.end(),[](const auto& row){return row.key=="settings.frame_rate";});
    check(rate!=graphics.end(),"Graphics must expose the frame rate in Konfigure Recompilation");
    if(rate!=graphics.end()) {
        const auto row=size_t(rate-graphics.begin());
        check(rate->value=="60 FPS","The frame rate row must start at 60 FPS");
        for(const int fps:{120,144,165,240,60}) {
            model.Change(row,1);
            const auto persisted=openshao::launcher::Merge({},model.Draft());
            check(model.Rows()[row].value==std::to_string(fps)+" FPS" &&
                  persisted["fps_cap"].value_or(0)==fps &&
                  openshao::launcher::Read(persisted).fps_cap==fps,
                  "Each selectable FPS limit must display and round-trip correctly");
            check(!model.Draft().vsync,"Changing frame rate must preserve disabled VSync");
        }
        model.Change(row,-1);check(model.Rows()[row].value=="240 FPS","The frame rate must wrap backward to 240 FPS");
        for(const int fps:{165,144,120,60}) {
            model.Change(row,-1);
            check(model.Rows()[row].value==std::to_string(fps)+" FPS",
                  "Reverse navigation must offer every supported FPS limit in order");
        }
        model.Change(2,1);
        const auto vsync_limit=model.Draft().fps_cap;
        model.Change(row,1);
        check(model.Rows()[row].value=="value.display_refresh" &&
              model.Draft().fps_cap==vsync_limit,
              "VSync must follow display refresh without changing the stored software limit");
        model.Discard();check(model.Draft()==original,"Discard must restore the opening frame rate");
    }
    for(const auto page:{Page::Display,Page::Graphics,Page::Controls,Page::Controller,Page::Keyboard,Page::KeyboardAlternate,Page::Mouse,Page::Audio,Page::Language}) {
        model.Go(page);
        for(size_t row=0;row<model.Rows().size();++row)for(int repeat=0;repeat<60;++repeat)model.Change(row,repeat%2?1:-1);
        std::string error;check(openshao::launcher::Validate(model.Draft(),error),"Every field edit must preserve settings validation");
        check(openshao::launcher::Read(openshao::launcher::Merge({},model.Draft()))==model.Draft(),"Every settings page must round-trip all device and game fields without loss");
    }
    return failures?1:0;
}
