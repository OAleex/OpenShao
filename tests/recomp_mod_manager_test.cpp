#include "mods/manager.h"
#include "mods/sha256.h"
#include "mods/mod_settings.h"
#include "file_ops.h"
#include "texture_pack_builder.h"
#include "texture_pack_file.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <toml++/toml.hpp>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace fs=std::filesystem;
using namespace openshao;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){try{
    Require(argc==2,"Fixture folder required");const auto base=files::FromUtf8(argv[1]),root=base/"mods";
    const auto layout_rejected=[](std::initializer_list<const char*> paths) {
        mods::Installed row;row.mod.id="Layout";row.mod.name="Layout";row.mod.type="mixed";
        for(const auto* path:paths)row.mod.files.push_back({path,std::string(64,'0'),0});
        try {mods::ValidateEnabled({row});return false;}catch(...){return true;}
    };
    Require(layout_rejected({"files/GAMEDATA.WAD","gamedata/record00001.bin"}),"One mod must not replace a whole archive and its individual resources");
    Require(layout_rejected({"gamedata/record00001.bin","gamedata/record1.ewdf"}),"Different spellings must not replace the same numbered resource");
    Require(layout_rejected({"gamedata/record07027.bin","audio/voices/voice.adx"}),"One mod must not replace both a voice bank and its recordings");
    const auto original=[](const mods::Patch&){std::string text;for(int i=0;i<40;++i)text+="The original game data.\n";return std::vector<uint8_t>(text.begin(),text.end());};
    uint64_t last_progress=0;
    const mods::Progress progress=[&](auto,uint64_t done,uint64_t total){
        Require(total>0&&done<=total,"Mod operations must report a bounded overall progress value");
        const auto value=done*10000/total;
        Require(value>=last_progress,"Mod progress must not restart between stages");
        last_progress=value;return true;
    };
    auto good=mods::InspectPackage(base/"good.tar.xz",root,progress);
    const auto legacy=mods::InspectPackage(base/"legacy.tar.xz",root);
    Require(legacy->mod.raw==good->mod.raw,"Existing manifest.json packages must retain their metadata");
    mods::InstallPackage(root,*legacy,original);
    const auto metadata=root/"languages/LanguagePack_PT-BR/mod.json";
    Require(Read(metadata)==legacy->mod.raw&&!fs::exists(metadata.parent_path()/"manifest.json"),
            "Both metadata names must install as mod.json");
    fs::rename(metadata,metadata.parent_path()/"manifest.json");
    mods::SetEnabled(root,legacy->mod.id,false);
    mods::InstallPackage(root,*good,original);
    Require(Read(metadata)==good->mod.raw&&!fs::exists(metadata.parent_path()/"manifest.json")&&!mods::ReadInstalled(root)[0].enabled,
            "Updating an old installation must adopt mod.json and preserve its disabled state");
    mods::Uninstall(root,legacy->mod.id);
    // Catch silent last-writer-wins behavior and broken dependency state.
    const auto install = [&](const char* id) {
        const auto package = mods::InspectPackage(base/(std::string(id)+".tar.xz"),root);
        mods::InstallPackage(root,*package,original);
    };
    install("TextureA");
    const auto initial_registry = Read(root/"installed.toml");
    bool conflict = false;
    try { install("TextureB"); } catch (...) { conflict = true; }
    Require(conflict && Read(root/"installed.toml")==initial_registry && !fs::exists(root/"packages/TextureB"),
            "Conflicting enabled replacements must preserve the installed state");
    mods::SetEnabled(root,"TextureA",false);
    install("TextureB");
    conflict = false;
    try { mods::SetEnabled(root,"TextureA",true); } catch (...) { conflict = true; }
    Require(conflict,"Enabling conflicting installed mods must fail");
    mods::Uninstall(root,"TextureB");
    mods::SetEnabled(root,"TextureA",true);
    install("Accessory");
    for (const bool remove : {false,true}) {
        bool blocked = false;
        try { if(remove)mods::Uninstall(root,"TextureA");else mods::SetEnabled(root,"TextureA",false); } catch (...) { blocked = true; }
        Require(blocked,"Enabled dependencies must not be disabled or uninstalled");
    }
    mods::SetEnabled(root,"Accessory",false);
    mods::Uninstall(root,"TextureA");
    mods::Uninstall(root,"Accessory");
    install("Japanese");
    Require(mods::EnabledLanguagePack(root,"ja")==root/"packages/Japanese","A new language must load by metadata, without a fixed ID");
    conflict = false;
    try { install("OtherJapanese"); } catch (...) { conflict = true; }
    Require(conflict,"Only one provider of a language can be enabled");
    install("Korean");
    Require(mods::EnabledLanguagePack(root,"ko")==root/"packages/Korean","Different installed languages must coexist");
    mods::Uninstall(root,"Japanese");mods::Uninstall(root,"Korean");
    Require(good->mod.version=="1.0.0"&&good->mod.authors[0]=="Tester","Package inspection must read mod.json");
    Require(mods::ReadInstalled(root).empty(),"Inspection must not install a mod");
    last_progress=0;
    mods::InstallPackage(root,*good,original,progress);
    const auto live=root/"languages/LanguagePack_PT-BR";
    Require(Read(live/"notes/new.txt")=="new content","Plain files must install");
    std::string target;for(int i=0;i<40;++i)target+="The translated game data.\n";target+=std::string(100,'!');
    Require(Read(live/"fonts/test.xpr")==target,"Original data must be patched before publication");
    Require(mods::EnabledLanguagePack(root,"pt-BR")==live,"Installed pack must be enabled");
    mods::SetEnabled(root,"LanguagePack_PT-BR",false);
    Require(mods::EnabledLanguagePack(root,"pt-BR").empty()&&fs::exists(live),"Disabling must keep files and prevent mounting");
    mods::SetEnabled(root,"LanguagePack_PT-BR",true);
    auto bad=mods::InspectPackage(base/"bad-hash.tar.xz",root);bool rejected=false;
    try{mods::InstallPackage(root,*bad,original);}catch(...){rejected=true;}
    Require(rejected&&mods::ReadInstalled(root)[0].mod.version=="1.0.0"&&Read(live/"fonts/test.xpr")==target,"Failed update must preserve previous registry and files");
    for(const auto* name:{"traversal.tar.xz","extra.tar.xz","bad-xz.tar.xz","unfinished-tar.tar.xz","both-metadata.tar.xz","large-metadata.tar.xz"}){
        rejected=false;try{mods::InspectPackage(base/name,root);}catch(...){rejected=true;}
        Require(rejected,"Unsafe or corrupt archives must be rejected");
    }
    std::string metadata_error;
    try{mods::InspectPackage(base/"large-metadata.tar.xz",root);}catch(const std::exception& e){metadata_error=e.what();}
    Require(metadata_error.find("extraction limits")!=std::string::npos,"mod.json must retain the metadata extraction size limit");
    std::string incomplete_error;
    try{mods::InspectPackage(base/"unfinished-tar.tar.xz",root);}catch(const std::exception& e){incomplete_error=e.what();}
    Require(incomplete_error.find("incomplete")!=std::string::npos&&incomplete_error.find("Download")!=std::string::npos,
            "An unfinished TAR must explain how to obtain a complete package");
    rejected=false;try{mods::InstallPackage(root,*good,original,[](auto,uint64_t,uint64_t){return false;});}catch(...){rejected=true;}
    Require(rejected&&Read(live/"notes/new.txt")=="new content","Cancellation must preserve the live mod");
    const auto registry_before_cancel=Read(root/"installed.toml");
    rejected=false;
    try{mods::InstallPackage(root,*good,original,[](auto stage,uint64_t,uint64_t){return stage!="Finishing installation";});}catch(...){rejected=true;}
    Require(rejected&&Read(root/"installed.toml")==registry_before_cancel&&Read(live/"fonts/test.xpr")==target,
            "Cancellation after preparing all files must preserve the live mod and registry");
#ifdef _WIN32
    const auto held=CreateFileW((live/"fonts/test.xpr").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Require(held!=INVALID_HANDLE_VALUE,"Fixture must hold the installed file open");
    rejected=false;try{mods::InstallPackage(root,*good,original);}catch(...){rejected=true;}
    CloseHandle(held);
    Require(rejected&&Read(live/"fonts/test.xpr")==target,"A file in use must prevent replacement and preserve the old mod");
#endif
    // Simulate a process dying after the old directory was parked.
    const auto previous=Read(root/"installed.toml");fs::rename(live,root/".backup-test");
    std::ostringstream journal;journal<<toml::table{{"destination","languages/LanguagePack_PT-BR"},{"backup",".backup-test"},{"existed",true},{"previous",previous},{"next","uncommitted state"}};
    files::AtomicWrite(root/"transaction.toml",journal.str());mods::Recover(root);
    Require(Read(live/"fonts/test.xpr")==target&&!fs::exists(root/"transaction.toml"),"Interrupted update must restore old mod");
    mods::Uninstall(root,"LanguagePack_PT-BR");
    Require(mods::ReadInstalled(root).empty()&&!fs::exists(live),"Uninstall must remove only the selected mod");
    Require(mods::CompareVersions("1.0.0-beta.2","1.0.0-beta.11")<0&&mods::CompareVersions("1.0.0","1.0.0-beta.11")>0,"Version comparison must follow numeric prerelease order");
    // A texture pack in one file: checked where it is, copied once and kept as pack.ostp
    {
        uint64_t last=0;
        const mods::Progress steady=[&](auto,uint64_t done,uint64_t total){
            Require(total>0&&done<=total&&done*10000/total>=last,"A texture pack's progress must stay bounded and never go back");
            last=done*10000/total;return true;
        };
        const auto textures=std::vector<Tex>{{0x10,64,64,16,16,7,2,0,0x20},{0x20,32,16,32,16,6,1,0,0x40}};
        const auto info=[](const std::string& version,const std::string& id="AiTextures",const std::string& recomp="0.1.0"){
            return R"({"id":")"+id+R"(","name":"AI \"textures\"","version":")"+version+R"(","authors":["Someone","Another"],)"
                   R"("summary":"Textures for the tests.","min_recomp":")"+recomp+R"(","features":["hdtextures"],"type":"texture","texture_count":2})";};
        const auto write=[&](const std::string& name,const Bytes& bytes){
            std::ofstream out(base/name,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));return base/name;};
        const auto file=write("ai-1.ostp",Build(textures,info("1.0.0")));
        const auto pack=mods::InspectPackage(file,root,mods::ProgressSlice(steady,0,5000));
        Require(pack->mod.id=="AiTextures"&&pack->mod.name=="AI \"textures\""&&pack->mod.schema==3&&pack->mod.type=="texture"&&pack->mod.authors.size()==2&&
                pack->mod.install_dir=="packages/AiTextures"&&pack->mod.version=="1.0.0"&&pack->mod.features==std::vector<std::string>{"hdtextures"}&&pack->folder.empty(),
                "A texture pack file gives the mod system its metadata without unpacking anything");
        Require(pack->mod.ostp&&pack->mod.ostp->path=="pack.ostp"&&pack->mod.ostp->size==fs::file_size(file)&&pack->mod.ostp->sha256==mods::Sha256File(file),
                "The whole file is hashed once, at inspection");
        mods::InstallPackage(root,*pack,original,mods::ProgressSlice(steady,5000,mods::ProgressTotal));
        const auto live_pack=root/"packages/AiTextures";
        Require(Read(live_pack/"pack.ostp")==Read(file)&&Read(live_pack/"mod.json")==pack->mod.raw,"The file is copied as pack.ostp, with its metadata beside it");
        std::string error;
        const auto opened=texpack::PackFile::Open(live_pack/"pack.ostp",error);
        Require(opened&&opened->entries().size()==2,"The installed copy opens as a texture pack");
        const auto rows=mods::ReadInstalled(root);
        Require(rows.size()==1&&rows[0].enabled&&rows[0].mod.ostp&&rows[0].mod.ostp->sha256==pack->mod.ostp->sha256,"The registry keeps the texture pack file of the mod");
        // an update, then going back
        mods::InstallPackage(root,*mods::InspectPackage(write("ai-2.ostp",Build(textures,info("1.1.0"))),root),original);
        Require(mods::ReadInstalled(root)[0].mod.version=="1.1.0"&&mods::ReadInstalled(root).size()==1,"A newer version replaces the installed one");
        mods::InstallPackage(root,*mods::InspectPackage(file,root),original);
        Require(mods::ReadInstalled(root)[0].mod.version=="1.0.0"&&Read(live_pack/"pack.ostp")==Read(file),"An older version replaces it too");
        // what is refused
        const auto refused=[&](const fs::path& f,const char* word){
            try{mods::InspectPackage(f,root);}catch(const std::exception& e){return std::string(e.what()).find(word)!=std::string::npos;}return false;};
        Bytes cut=Build(textures,info("1.2.0"));cut.resize(cut.size()-64);
        Require(refused(write("cut.ostp",cut),"texture pack"),"A texture pack file cut short is refused");
        Bytes junk={'O','S','T','P','A','C','K',0,1,2,3};
        Require(refused(write("junk.ostp",junk),"texture pack"),"A file that only starts like a texture pack is refused");
        Require(refused(write("future.ostp",Build(textures,info("1.2.0","Future","9.9.9"))),"Update OpenShao"),"A pack that needs a newer OpenShao is refused");
        Require(refused(write("dotted.ostp",Build(textures,info("1.2.0","has.dot"))),"texture pack"),"An id the mod system does not accept is refused");
        Require(mods::ReadInstalled(root).size()==1&&mods::ReadInstalled(root)[0].mod.version=="1.0.0","Nothing refused changes what is installed");
        // the manifest says what a texture pack file may be
        const std::string base_manifest=R"({"schema":3,"game":{"xbe_sha256":")"+std::string(mods::GameSha256)+R"("},"id":"P","name":"P","summary":"s","authors":["a"],"type":"texture","install_dir":"packages/P",)"
            R"("features":["hdtextures"],"version":"1.0.0","recomp":"0.1.0","ostp":{"path":"pack.ostp","size":10,"sha256":")"+std::string(64,'a')+R"("})";
        const auto parses=[&](const std::string& tail,const std::string& swap_from={},const std::string& swap_to={}){
            std::string text=base_manifest+tail+"}";if(!swap_from.empty())text.replace(text.find(swap_from),swap_from.size(),swap_to);
            try{mods::ParseManifest(text);return true;}catch(...){return false;}};
        Require(parses(",\"files\":[],\"patches\":[]"),"A manifest with a texture pack file parses");
        Require(!parses(",\"files\":[],\"patches\":[]","\"schema\":3","\"schema\":2"),"A texture pack file needs schema 3");
        Require(!parses(",\"files\":[],\"patches\":[]","\"path\":\"pack.ostp\"","\"path\":\"other.ostp\""),"The texture pack file has one name");
        Require(!parses(",\"files\":[],\"patches\":[]","\"features\":[\"hdtextures\"]","\"features\":[]"),"A texture pack file needs the hdtextures feature");
        Require(!parses(",\"files\":[{\"path\":\"a.txt\",\"size\":1,\"sha256\":\""+std::string(64,'b')+"\"}],\"patches\":[]"),"A texture pack file holds nothing else");
        Require(!parses(",\"files\":[],\"patches\":[]","\"size\":10","\"size\":0"),"A texture pack file has a size");
        mods::Uninstall(root,"AiTextures");
        Require(mods::ReadInstalled(root).empty()&&!fs::exists(live_pack),"Uninstalling removes the texture pack file");
    }
    {   // What the user chose is kept by the mod's id, apart from the mod's own files
        const auto folder=fs::temp_directory_path()/"openshao-mod-settings-test";fs::remove_all(folder);
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::High,"Nothing chosen means High, even without the folder");
        mods::WriteTextureQuality(folder,"A",texpack::Quality::Low);
        mods::WriteTextureQuality(folder,"B.x-1",texpack::Quality::High);
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::Low&&mods::ReadTextureQuality(folder,"B.x-1")==texpack::Quality::High,
                "Each mod keeps its own choice");
        mods::WriteTextureQuality(folder,"B.x-1",texpack::Quality::Low);
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::Low&&mods::ReadTextureQuality(folder,"B.x-1")==texpack::Quality::Low,
                "Changing one choice keeps the others");
        mods::WriteTextureQuality(folder,"A",texpack::Quality::High);
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::High,"A choice can change back");
        std::ofstream(folder/"settings.toml",std::ios::trunc)<<"[mods.A]\ntexture_quality = \"medium\"\n[mods.B]\ntexture_quality = \"low\"\n[mods.C]\ntexture_quality = \"high\"\n";
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::Low&&mods::ReadTextureQuality(folder,"B")==texpack::Quality::Low&&
                mods::ReadTextureQuality(folder,"C")==texpack::Quality::High,"The earlier name of the reduced quality, medium, still reads as Low");
        std::ofstream(folder/"settings.toml",std::ios::trunc)<<"this is [not toml";
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::High,"A file that cannot be read means High");
        // Writing must not start from nothing: a damaged file would be replaced by one key and every other choice lost
        const auto damaged=[&]{std::ifstream f(folder/"settings.toml");return std::string(std::istreambuf_iterator<char>(f),{});};
        bool refused=false;
        try{mods::WriteTextureQuality(folder,"A",texpack::Quality::Low);}catch(const std::exception&){refused=true;}
        Require(refused&&damaged()=="this is [not toml","A file that cannot be read is not written over");
        refused=false;
        try{mods::WriteTextureOrder(folder,{"A","B"});}catch(const std::exception&){refused=true;}
        Require(refused&&damaged()=="this is [not toml","The order is not written over it either");
        fs::remove(folder/"settings.toml");
        mods::WriteTextureQuality(folder,"A",texpack::Quality::Low);
        Require(mods::ReadTextureQuality(folder,"A")==texpack::Quality::Low,"With the damaged file gone, a choice is written");
        fs::remove_all(folder);
        // The order of the texture mods: the user's first, then the others as installed, and none that is gone
        using Ids=std::vector<std::string>;
        Require(mods::TextureOrder(folder,{"A","B","C"})==Ids({"A","B","C"}),"Without a choice the order is the installation order");
        mods::WriteTextureOrder(folder,{"C","A"});
        Require(mods::TextureOrder(folder,{"A","B","C"})==Ids({"C","A","B"}),"The user's order comes first and the others follow");
        Require(mods::TextureOrder(folder,{"A","D"})==Ids({"A","D"}),"A mod that is no longer installed leaves the order");
        Require(mods::TextureOrder(folder,{})==Ids{},"No mods, no order");
        mods::WriteTextureQuality(folder,"A",texpack::Quality::Low);
        Require(mods::TextureOrder(folder,{"A","B","C"})==Ids({"C","A","B"})&&mods::ReadTextureQuality(folder,"A")==texpack::Quality::Low,
                "The order and the quality share the file without touching each other");
        fs::remove_all(folder);
    }
    std::cout<<"Mod manager checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
