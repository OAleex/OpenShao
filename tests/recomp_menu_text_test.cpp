#include "menu_text.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace openshao::menu;
    unsigned failures=0;
    auto check=[&](bool value,const char* message){if(!value){++failures;std::cerr<<message<<'\n';}};
    const auto catalog=Catalog::Parse(R"({"version":1,"languages":{"en":{"title":"Options"},"pt-BR":{"title":"Opções"}}})");
    check(catalog.Get("pt-br","title")=="Opções","Locale lookup must preserve UTF-8 accents and ignore locale case");
    check(catalog.Get("unknown","title")=="Options","Unknown locales must fall back to English");
    check(catalog.Get("en","absent").empty(),"Missing menu IDs must be detectable before hiding native artwork");
    check(EncodeNative("Opções",false)==std::string("Op\xE7\xF5" "es"),"Retail glyphs use Latin-1 character codes");
    check(EncodeNative("Opções",true)==std::string("Op\xE7\xF6" "es"),"Translated nasal glyph slots must be mapped only at the renderer boundary");
    check(EncodeNative("Padrão",true)==std::string("Padr\xE4" "o"),"Portuguese nasal A must use the translated font slot");
    check(EncodeNative("Zurück",false)==std::string("Zur\xFC" "ck"),"German umlauts must retain their original slots");
    bool invalid=false;try{EncodeNative(std::string("\xC0\xAF",2),false);}catch(const std::exception&){invalid=true;}
    check(invalid,"Invalid UTF-8 must not produce native string overreads");
    const auto long_message=std::string(180,'a');
    bool system_ok=false;try{system_ok=Catalog::Parse("{\"version\":1,\"languages\":{\"en\":{\"system.test\":\""+long_message+"\"}}}").Get("en","system.test")==long_message;}catch(const std::exception&){}
    check(system_ok,"System dialogs must support native message buffers longer than a menu caption");
    invalid=false;try{EncodeNative(long_message,false);}catch(const std::exception&){invalid=true;}
    check(invalid,"Ordinary menu captions must retain their original 95-byte storage limit");
    invalid=false;try{EncodeNative(std::string(256,'a'),false,255);}catch(const std::exception&){invalid=true;}
    check(invalid,"System messages must reject text beyond their native message buffer limit");
    for(float width:{40.f,180.f,360.f,800.f}) {
        const auto title=LayoutTitle(width);
        check(title.scale>0 && title.scale<=1.5f,"Title size must stay bounded");
        check(std::abs(title.arrow_tip-(title.text_x+width*title.scale+12.f))<.01f,
              "The arrow tip must follow the measured translated title with a fixed gap");
        check(title.arrow_tip<=608.f && title.arrow_tip>=title.text_x,"Long translated titles must fit the original canvas");
    }
    check(std::abs(FitScale(500,1.1f,220)-.44f)<.001f,"Long labels must fit their available column");
    check(std::abs(FitScale(100,1.1f,220)-1.1f)<.001f,"Short labels must retain the requested font size");
    const auto embedded=Catalog::Builtin();
    check(embedded.Graphic(0x36C56E9D)=="character_select.title","Character Select artwork must use the shared heading style");
    check(embedded.Graphic(0x86ABCE30)=="level_select.title","Level Select artwork must use the shared heading style");
    check(embedded.Graphic(0x0D41E9C5)=="load.title","Load Game artwork must use the shared heading style");
    check(embedded.Get("de","main.kontent")=="Inhalt","German labels must use the official graphical term");
    check(embedded.Get("fr","main.load_game")=="Charger","French labels must use the official graphical term");
    for(const auto language:{"en","fr","de","es","it","pt-BR"})
        check(!embedded.Get(language,"recomp.title").empty(),"Every supported locale must include the new menu title");
    check(embedded.Get("pt-BR","value.on")=="Sim" && embedded.Get("pt-BR","value.off")=="Não","Portuguese boolean settings must use yes and no");
    check(embedded.Get("en","debug.menu")=="Debug Menu","English debug entry must keep the native menu name");
    check(embedded.Get("pt-BR","debug.menu")=="Menu de Depuração","Portuguese debug entry must use the shared translated caption");
    check(EncodeNative(embedded.Get("pt-BR","debug.menu"),true)==std::string("Menu de Depura\xE7\xE4o"),
          "Portuguese debug text must use the translated font's nasal glyph slots");
    for(const auto locale:{"en","fr","de","es","it","pt-BR"}) {
        for(const auto key:{"debug.menu","debug.unlock_kontent","debug.kontent_unlocked"})
            check(!embedded.Get(locale,key).empty() && EncodeNative(embedded.Get(locale,key),locale==std::string_view("pt-BR")).size()<24,
                  "Every translated debug caption must fit a terminated native string slot");
    }
    for(const auto locale:{"en","fr","de","es","it","pt-BR"}) {
        const bool portuguese=std::string_view(locale)=="pt-BR";
        for(const auto key:{"survival.unlock_new","survival.unlock_pending","survival.unlock_saved","survival.complete"}) {
            const auto message=embedded.Get(locale,key);
            const auto encoded=EncodeNativeMultiline(message,portuguese,127);
            check(!message.empty() && encoded.find('\n')!=std::string::npos && encoded.size()<128,
                  "All Survival frames (unlock and completion) need localized, line-broken native text within 128 bytes");
        }
        const auto hint=embedded.Get(locale,"survival.locked_hint");
        check(!hint.empty() && EncodeNative(hint,portuguese).size()<95,
              "Each locale needs a compact locked footer description");
    }
    check(embedded.Get("pt-BR","survival.unlock_new").find("Salve o jogo")!=std::string_view::npos,
          "The first Portuguese unlock notification must explicitly tell players to save");
    check(embedded.Get("pt-BR","survival.unlock_pending").find("Salve o jogo")!=std::string_view::npos &&
          embedded.Get("pt-BR","survival.unlock_saved").find("Salve o jogo")==std::string_view::npos,
          "Pending unlock explains saving, while an already-saved unlock does not ask to save again");
    for(const auto locale:{"en","fr","de","es","it","pt-BR"}) {
        check(embedded.Get(locale,"language.en")=="English" && embedded.Get(locale,"language.fr")=="Français" &&
              embedded.Get(locale,"language.de")=="Deutsch" && embedded.Get(locale,"language.es")=="Español" &&
              embedded.Get(locale,"language.it")=="Italiano" && embedded.Get(locale,"language.pt-BR")=="Português Brasileiro",
              "Every menu locale must display each language using its own name and native accents");
    }
    return failures?1:0;
}
