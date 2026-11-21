#include "manager.h"
#include "languages.h"
#include "routines.h"
#include "../file_ops.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <stdexcept>
namespace openshao::mods {
namespace {
std::string Key(std::string name) {
    for(auto& c:name){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+='a'-'A';}
    return name;
}
struct Claims {
    std::set<std::string> resources;
    bool archive=false, archive_parts=false, voices=false, voice_bank=false;
};
Claims Resources(const Manifest& mod) {
    Claims claims;
    const auto add=[&](std::string path) {
        path=Key(path);
        if(path.starts_with("files/")) {
            const auto target=path.substr(6);
            if(target=="default.xbe")throw std::runtime_error("Executable byte replacements cannot change recompiled routines. Use Lua routine hooks.");
            claims.resources.insert("file:"+target);
            if(target.size()==12&&target.starts_with("gamedata.wa")&&target.back()>='d'&&target.back()<='z')claims.archive=true;
        } else if(path.starts_with("gamedata/record")) {
            const auto dot=path.find('.');
            const auto number=path.substr(15,dot-15);
            unsigned record=0;const auto parsed=std::from_chars(number.data(),number.data()+number.size(),record);
            if(number.empty()||parsed.ec!=std::errc{}||parsed.ptr!=number.data()+number.size())
                throw std::runtime_error("Invalid archive record filename");
            if(!claims.resources.insert("record:"+std::to_string(record)).second)
                throw std::runtime_error("One mod replaces the same archive record more than once");
            claims.archive_parts=true;
            constexpr std::array<unsigned,23> banks{7027,7041,7042,7043,7048,7080,7081,7108,7109,7132,7133,7154,7155,7175,7176,7187,7213,7214,7232,7233,7244,7271,7272};
            claims.voice_bank=claims.voice_bank||std::find(banks.begin(),banks.end(),record)!=banks.end();
        } else if(path.starts_with("audio/voices/")) {
            claims.resources.insert(path);claims.archive_parts=true;claims.voices=true;
        } else if(path.starts_with("audio/movies/")) {
            const auto dot=path.find_last_of('.');
            claims.resources.insert("file:front/movies/"+path.substr(13,dot-13)+".sfd");
        } else if(path.starts_with("fonts/")) claims.resources.insert(path);
    };
    for(const auto& f:mod.files)add(f.path);
    for(const auto& p:mod.patches)add(p.path);
    if(claims.archive&&claims.archive_parts)throw std::runtime_error("One mod replaces a whole archive and its individual resources");
    if(claims.voice_bank&&claims.voices)throw std::runtime_error("One mod replaces a whole voice bank and its individual recordings");
    return claims;
}
}
void ValidateEnabled(const std::vector<Installed>& rows) {
    std::map<std::string,const Installed*> enabled;
    for(const auto& row:rows)if(row.enabled)enabled.emplace(row.mod.id,&row);
    std::map<std::string,int> visits;
    const auto visit=[&](auto&& self,const Installed& row)->void {
        auto& state=visits[row.mod.id];
        if(state==1)throw std::runtime_error("Mod dependency cycle includes "+row.mod.name);
        if(state==2)return;state=1;
        for(const auto& dependency:row.mod.dependencies) {
            const auto found=enabled.find(dependency.id);
            if(found==enabled.end()||CompareVersions(found->second->mod.version,dependency.version)<0)
                throw std::runtime_error(row.mod.name+" requires enabled mod "+dependency.id+" version "+dependency.version+" or later");
            self(self,*found->second);
        }
        state=2;
    };
    for(const auto& [id,row]:enabled)visit(visit,*row);
    std::map<std::string,Claims> claims;
    for(const auto& [id,row]:enabled)claims.emplace(id,Resources(row->mod));
    for(auto a=enabled.begin();a!=enabled.end();++a)for(auto b=std::next(a);b!=enabled.end();++b) {
        const auto& left=a->second->mod;const auto& right=b->second->mod;
        const auto conflicts=[&](std::string_view resource) {
            throw std::runtime_error(left.name+" conflicts with "+right.name+" ("+std::string(resource)+"). Disable one mod first.");
        };
        if(std::find(left.conflicts.begin(),left.conflicts.end(),right.id)!=left.conflicts.end()||
           std::find(right.conflicts.begin(),right.conflicts.end(),left.id)!=right.conflicts.end())conflicts("declared incompatibility");
        if(left.type=="language"&&right.type=="language") {
            if(Key(left.language)==Key(right.language))conflicts("same language");
            continue; // Only the selected language is mounted.
        }
        const auto& x=claims.at(left.id);const auto& y=claims.at(right.id);
        if((x.archive&&y.archive_parts)||(y.archive&&x.archive_parts))conflicts("whole archive and archive resources");
        if((x.voice_bank&&y.voices)||(y.voice_bank&&x.voices))conflicts("whole voice bank and voice recordings");
        for(const auto& resource:x.resources)if(y.resources.contains(resource))conflicts(resource);
    }
}
std::vector<Installed> ActiveMods(const std::filesystem::path& root,std::string_view language) {
    auto rows=ReadInstalled(root);ValidateEnabled(rows);
    std::erase_if(rows,[&](const auto& row){return !row.enabled||(row.mod.type=="language"&&Key(row.mod.language)!=Key(std::string(language)));});
    for(const auto& row:rows)for(const auto& dependency:row.mod.dependencies)
        if(std::none_of(rows.begin(),rows.end(),[&](auto& active){return active.mod.id==dependency.id;}))
            throw std::runtime_error(row.mod.name+" requires selecting the language of "+dependency.id);
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.mod.id<b.mod.id;});
    return rows;
}
void ValidateRoutines(const std::filesystem::path& root,const std::vector<Installed>& rows,
                      std::string_view staged_id,const std::filesystem::path& staged_folder) {
    if(std::none_of(rows.begin(),rows.end(),[](auto& row){return row.enabled&&!row.mod.routines.empty();}))return;
    std::set<std::string> languages{"en"};
    for(const auto& row:rows)if(row.enabled&&row.mod.type=="language")languages.insert(Key(row.mod.language));
    for(const auto& language:languages) {
        std::vector<RoutineScript> scripts;
        for(const auto& row:rows)if(row.enabled&&(row.mod.type!="language"||Key(row.mod.language)==language)) {
            const auto folder=row.mod.id==staged_id?staged_folder:root/files::FromUtf8(row.mod.install_dir);
            for(const auto& path:row.mod.routines)scripts.push_back({row.mod.id,folder/files::FromUtf8(path)});
        }
        RoutineEngine engine;std::string error;
        if(!engine.Load(scripts,CompiledRoutineTargets(),{},error))throw std::runtime_error(error);
    }
}
}
