#include "browser.h"
#include "game_reader.h"
#include "http.h"
#include "sha256.h"
#include "version.h"
#include "../file_ops.h"
#include <algorithm>
#include <fstream>
namespace fs=std::filesystem;
namespace openshao::mods {
namespace {
std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
struct TempFile {fs::path path;~TempFile(){std::error_code ec;fs::remove(path,ec);}};
}
Browser::Browser(const fs::path& root):root_(root) { Refresh(); }
Browser::~Browser(){Cancel();if(worker_.joinable())worker_.join();}
BrowserState Browser::View()const{std::lock_guard lock(mutex_);return state_;}
void Browser::Cancel(){cancel_=true;answered_.notify_all();}
void Browser::Answer(bool install){
    {std::lock_guard lock(mutex_);if(!state_.asking)return;answer_=install;state_.asking.reset();++state_.revision;}
    answered_.notify_all();
}
bool Browser::Confirm(const Replacement& what){
    std::unique_lock lock(mutex_);
    answer_.reset();state_.asking=what;++state_.revision;
    answered_.wait(lock,[&]{return answer_.has_value()||cancel_.load();});
    state_.asking.reset();
    const bool yes=answer_.value_or(false)&&!cancel_;answer_.reset();return yes;
}
Progress Browser::ProgressCallback(){return [this](std::string_view text,uint64_t done,uint64_t total){
    if(cancel_)return false;std::lock_guard lock(mutex_);state_.status=text;
    const auto value=total?uint64_t(ProgressTotal*(static_cast<long double>(std::min(done,total))/total)):0;
    state_.done=std::max(state_.done,std::min(value,ProgressTotal-1));return true;};}
void Browser::Start(const Job& job,bool checking){
    {std::lock_guard lock(mutex_);if(state_.busy)return;}
    if(worker_.joinable())worker_.join();cancel_=false;
    {std::lock_guard lock(mutex_);state_.busy=true;state_.checking=checking;state_.error.clear();state_.status="Working...";state_.done=0;state_.total=ProgressTotal;++state_.revision;}
    worker_=std::jthread([this,job]{
        try{job();}catch(const std::exception& e){std::lock_guard lock(mutex_);state_.error=e.what();state_.status="Operation could not be completed";}
        try{Reload();}catch(const std::exception& e){std::lock_guard lock(mutex_);state_.error=e.what();}
        std::lock_guard lock(mutex_);state_.busy=false;
        if(state_.error.empty())state_.done=state_.total;
        if(state_.checking&&state_.error.empty())state_.status=state_.updates?"Mod updates are available":"No mod updates are available";
        ++state_.revision;
    });
}
void Browser::Reload(){
    auto rows=ReadInstalled(root_);std::lock_guard lock(mutex_);state_.installed=std::move(rows);state_.updates=false;
    for(const auto& m:state_.catalog)for(const auto& i:state_.installed)if(i.mod.id==m.id)if(const auto* v=LatestCompatible(m))if(CompareVersions(v->version,i.mod.version)>0)state_.updates=true;
}
void Browser::Refresh(){Start([this]{
    Recover(root_);Reload();const auto cache=root_.parent_path()/"cache/mods.json";fs::create_directories(cache.parent_path());
    if(fs::is_regular_file(cache))try{auto mods=ParseCatalog(Read(cache));std::lock_guard lock(mutex_);state_.catalog=std::move(mods);}catch(...){}
    TempFile download{files::UniqueSibling(cache,".download-")};
    const auto progress=ProgressCallback();
    Download(CatalogUrl,download.path,8*1024*1024,ProgressSlice(progress,0,9000));
    Report(progress,"Checking catalog",9000,ProgressTotal);
    const auto text=Read(download.path);auto mods=ParseCatalog(text);files::AtomicWrite(cache,text);
    std::lock_guard lock(mutex_);state_.catalog=std::move(mods);state_.status="Mods are up to date";
},true);}
void Browser::InstallFile(const fs::path& file,const fs::path& game,bool ask){Start([this,file,game,ask]{
    const auto progress=ProgressCallback();GameReader source(game);
    const auto package=InspectPackage(file,root_,ProgressSlice(progress,0,5000));
    if(ask) {
        const auto rows=ReadInstalled(root_);
        const auto found=std::find_if(rows.begin(),rows.end(),[&](auto& r){return r.mod.id==package->mod.id;});
        if(found!=rows.end()&&!Confirm({package->mod.name,found->mod.version,package->mod.version})) {
            std::lock_guard lock(mutex_);state_.status="Installation cancelled";return;
        }
    }
    InstallPackage(root_,*package,[&](const Patch& patch){return source.Original(patch);},ProgressSlice(progress,5000,ProgressTotal));
    std::lock_guard lock(mutex_);state_.status="Mod installed";
});}
void Browser::Install(std::string id,const fs::path& game){Start([this,id,game]{
    const auto view=View();const auto it=std::find_if(view.catalog.begin(),view.catalog.end(),[&](auto& m){return m.id==id;});
    if(it==view.catalog.end())throw std::runtime_error("Refresh the mod catalog before installing");const auto* version=LatestCompatible(*it);
    if(!version)throw std::runtime_error("No release is available for this version of OpenShao");
    const auto progress=ProgressCallback();GameReader source(game);
    const auto cache=root_.parent_path()/"cache";fs::create_directories(cache);
    TempFile archive{files::UniqueSibling(cache/"mod.tar.xz",".download-")};Download(version->url,archive.path,version->size,ProgressSlice(progress,0,3000));
    Report(progress,"Verifying download",3000,ProgressTotal);
    if(fs::file_size(archive.path)!=version->size||Sha256File(archive.path)!=version->sha256)throw std::runtime_error("Downloaded mod failed SHA-256 verification. Try downloading again.");
    auto p=InspectPackage(archive.path,root_,ProgressSlice(progress,3500,6500));
    if(p->mod.id!=id||p->mod.version!=version->version||p->mod.install_dir!=it->install_dir||p->mod.type!=it->type||p->mod.language!=it->language)throw std::runtime_error("Downloaded mod information does not match the catalog");
    InstallPackage(root_,*p,[&](const Patch& patch){return source.Original(patch);},ProgressSlice(progress,6500,ProgressTotal));
    std::lock_guard lock(mutex_);state_.status="Mod installed";
});}
void Browser::Enable(std::string id,bool enabled){Start([this,id,enabled]{SetEnabled(root_,id,enabled);std::lock_guard lock(mutex_);state_.status=enabled?"Mod enabled":"Mod disabled";});}
void Browser::Remove(std::string id){Start([this,id]{Uninstall(root_,id);std::lock_guard lock(mutex_);state_.status="Mod uninstalled";});}
std::vector<Mod> DisplayMods(const BrowserState& view){auto list=view.catalog;for(const auto& i:view.installed)if(std::none_of(list.begin(),list.end(),[&](auto& m){return m.id==i.mod.id;}))list.push_back(i.mod);return list;}
}
