// Exercise the real browser worker and installer with a controlled download source.
// The separate HTTPS check covers the production transport.
#include "mods/browser.h"
#include "mods/http.h"
#include "mods/sha256.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
using namespace openshao::mods;
namespace {
fs::path fixtures;
std::atomic<bool> corrupt{false},offline{false};
std::atomic<unsigned> progress_calls{0};
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
BrowserState Ready(Browser& browser){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    uint64_t previous=0;
    for(;;){auto state=browser.View();
        Check(state.total>0&&state.done<=state.total,"Each browser operation must have a bounded progress range");
        const auto value=state.done*10000/state.total;
        Check(value>=previous,"Browser progress must not restart between stages");previous=value;
        if(!state.busy){
            Check(state.error.empty()?state.done==state.total:state.done<state.total,"Only completed operations may reach full progress");
            return state;
        }
        Check(std::chrono::steady_clock::now()<deadline,"Browser worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));}
}
void Catalog(unsigned version){fs::copy_file(fixtures/("catalog-"+std::to_string(version)+".json"),fixtures/"catalog.json",fs::copy_options::overwrite_existing);}
}
namespace openshao::mods {
void Download(std::string_view url,const fs::path& file,uint64_t maximum,const Progress& progress){
    if(offline)throw std::runtime_error("Controlled network failure");
    // master advances with each release; the HEAD alias can retain an older catalog.
    const std::string_view repository="https://raw.githubusercontent.com/OAleex/OpenShao-Mods/";
    const bool catalog_request=url.starts_with(repository);
    if(catalog_request&&url.substr(repository.size())!="HEAD/mods.json"&&url.substr(repository.size())!="master/mods.json")
        throw std::runtime_error("Mod server returned HTTP 404");
    const auto source=catalog_request?fixtures/(url.substr(repository.size())=="HEAD/mods.json"?"catalog-1.json":"catalog.json"):fixtures/fs::path(std::string(url.substr(url.find_last_of('/')+1)));
    const auto size=fs::file_size(source);Check(size<=maximum,"Download exceeded the advertised limit");
    for(unsigned i=0;i<3;++i){++progress_calls;Report(progress,"Downloading mod",size*i/3,size);}
    fs::copy_file(source,file,fs::copy_options::overwrite_existing);
    if(corrupt&&!catalog_request){std::ofstream stream(file,std::ios::binary|std::ios::app);stream.put('!');}
}
}
int main(int argc,char** argv){
    try{
        Check(argc==2,"Expected fixture folder");fixtures=fs::absolute(argv[1]);Catalog(1);
        Browser browser(fixtures/"data/mods");auto state=Ready(browser);
        Check(state.error.empty()&&state.catalog.size()==2,"Opening the browser must discover all catalog mods");
        browser.Install("Texture",fixtures/"game");state=Ready(browser);
        Check(state.error.empty()&&state.installed.size()==1&&state.installed[0].mod.version=="1.0.0","Catalog installation must use the inspected archive");
        const auto installed=fixtures/"data/mods/packages/Texture/files/texture.bin";
        const auto old_hash=Sha256File(installed);
        browser.Enable("Texture",false);Check(Ready(browser).error.empty(),"Disabling an installed mod must succeed");
        Catalog(2);browser.Refresh();state=Ready(browser);
        Check(state.error.empty()&&state.updates&&state.catalog.size()==3&&DisplayMods(state).size()==3,"Refresh must discover both updates and new mods");
        corrupt=true;browser.Install("Texture",fixtures/"game");state=Ready(browser);corrupt=false;
        Check(!state.error.empty()&&state.installed[0].mod.version=="1.0.0"&&!state.installed[0].enabled&&Sha256File(installed)==old_hash,"A failed download must preserve the installed version and enabled state");
        browser.Install("Texture",fixtures/"game");state=Ready(browser);
        Check(state.error.empty()&&!state.updates&&state.installed[0].mod.version=="2.0.0"&&!state.installed[0].enabled&&Sha256File(installed)!=old_hash,"Updating must replace verified files and preserve disabled state");
        browser.Install("Japanese",fixtures/"game");state=Ready(browser);
        Check(state.error.empty()&&state.installed.size()==2&&!EnabledLanguagePack(fixtures/"data/mods","ja").empty(),"A catalog language must install enabled and resolve by locale");
        browser.InstallFile(fixtures/"NewMod-1.0.0.tar.xz",fixtures/"game");state=Ready(browser);
        Check(state.error.empty()&&state.installed.size()==3&&state.installed.back().mod.id=="NewMod"&&state.installed.back().enabled,
              "Selecting a local archive must inspect and install it in one operation");
        browser.Enable("NewMod",false);Check(Ready(browser).error.empty(),"A local mod must support disabling");
        browser.InstallFile(fixtures/"NewMod-2.0.0.tar.xz",fixtures/"game");state=Ready(browser);
        Check(state.error.empty()&&state.installed.back().mod.version=="2.0.0"&&!state.installed.back().enabled,
              "Local updates must install directly and preserve the enabled state");
        const auto local_file=fixtures/"data/mods/packages/NewMod/files/newmod.bin";
        const auto local_hash=Sha256File(local_file);
        browser.InstallFile(fixtures/"bad-local.tar.xz",fixtures/"game");state=Ready(browser);
        Check(!state.error.empty()&&state.installed.size()==3&&Sha256File(local_file)==local_hash&&!state.installed.back().enabled,
              "A corrupt local update must preserve the installed files and enabled state");
        // asking before a mod that is already installed is replaced
        const auto asked=[&]{
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            for(;;){auto view=browser.View();if(view.asking)return view;
                Check(view.busy&&std::chrono::steady_clock::now()<deadline,"An installation over an installed mod must wait for an answer");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));}};
        browser.Answer(true);Check(!browser.View().asking,"Answering when nothing asks changes nothing");
        browser.InstallFile(fixtures/"NewMod-1.0.0.tar.xz",fixtures/"game",true);
        auto waiting=asked();
        Check(waiting.asking->installed=="2.0.0"&&waiting.asking->incoming=="1.0.0"&&!waiting.asking->name.empty()&&waiting.busy,
              "The question names the mod and the installed and the incoming versions");
        browser.Answer(false);state=Ready(browser);
        Check(state.error.empty()&&!state.asking&&state.status=="Installation cancelled"&&state.installed.back().mod.version=="2.0.0"&&Sha256File(local_file)==local_hash,
              "Saying no leaves what is installed as it was");
        browser.InstallFile(fixtures/"NewMod-1.0.0.tar.xz",fixtures/"game",true);asked();browser.Answer(true);state=Ready(browser);
        Check(state.error.empty()&&state.installed.back().mod.version=="1.0.0"&&!state.installed.back().enabled,
              "Saying yes replaces it, with an older version too, and keeps it disabled");
        browser.InstallFile(fixtures/"NewMod-1.0.0.tar.xz",fixtures/"game",true);
        waiting=asked();
        Check(waiting.asking->installed==waiting.asking->incoming,"The same version asks too, to install it again");
        browser.Cancel();state=Ready(browser);
        Check(state.error.empty()&&!state.asking&&state.installed.back().mod.version=="1.0.0","Cancelling while it asks ends the installation without changes");
        offline=true;browser.Refresh();state=Ready(browser);offline=false;
        Check(!state.error.empty()&&state.catalog.size()==3&&state.installed.size()==3,"Network failure must preserve cached catalog and installations");
        Check(progress_calls>0,"Downloads must report progress");
        std::cout<<"Catalog discovery, direct local and online installation, updates, languages, rollback and offline checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
