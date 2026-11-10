#include "catalog.h"
#include "languages.h"
#include "version.h"
#include "../file_ops.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <set>
#include <stdexcept>
namespace openshao::mods {
namespace {
std::string Text(const Json& j, const char* key, bool optional=false) { return optional&&!j.Has(key)?"":j.At(key).Text(); }
uint64_t Number(const Json& j,const char* key) { auto n=j.At(key).Number(); if(n<0) throw std::runtime_error("Negative mod size or offset"); return uint64_t(n); }
void Hash(const std::string& hash) {
    if(hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=hash.npos) throw std::runtime_error("Invalid SHA-256 in mod metadata");
}
void Path(const std::string& p) { if(!files::SafeRelative(p)) throw std::runtime_error("Unsafe path in mod metadata: "+p); }
void Id(const std::string& id) {
    if(id.empty()||id.size()>80||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=id.npos)
        throw std::runtime_error("Invalid mod id");
}
// Release asset of the maintainer's repositories, matched by owner so renamed repos still work.
bool OfficialRelease(std::string_view url) {
    constexpr std::string_view owner = "https://github.com/OAleex/";
    if (!url.starts_with(owner)) return false;
    const auto rest = url.substr(owner.size());
    const auto slash = rest.find('/');
    if (slash == 0 || slash == std::string_view::npos) return false;
    for (const char ch : rest.substr(0, slash))
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_' && ch != '.') return false;
    return rest.substr(slash).starts_with("/releases/download/");
}
bool KnownType(std::string_view type) {
    return type=="language"||type=="texture"||type=="gameplay"||type=="data"||type=="mixed";
}
Mod ReadMod(const Json& j) {
    Mod m; m.id=Text(j,"id"); m.name=Text(j,"name"); m.summary=Text(j,"summary"); m.type=Text(j,"type");
    m.language=Text(j,"language",true); m.install_dir=Text(j,"install_dir");
    Id(m.id);
    Path(m.install_dir);
    if(m.install_dir!="packages/"+m.id && !(m.type=="language" && m.install_dir=="languages/"+m.id))
        throw std::runtime_error("Install each mod in packages/<id>, or each language pack in languages/<id>");
    if(!KnownType(m.type))m.unsupported_reason="Update OpenShao to use this mod type";
    if(m.type=="language" ? !ValidLanguageCode(m.language) : !m.language.empty())
        throw std::runtime_error("Only language mods can declare a valid language code");
    m.language_name=Text(j,"language_name",true);
    if(m.language_name.size()>80||m.language_name.find_first_of("\r\n\t")!=m.language_name.npos)
        throw std::runtime_error("Invalid language display name");
    for(const auto& a:j.At("authors").Items()) m.authors.push_back(a.Text());
    if(j.Has("dependencies")) for(const auto& d:j.At("dependencies").Items()) {
        Dependency dependency{Text(d,"id"),Text(d,"version")};Id(dependency.id);
        if(dependency.id==m.id)throw std::runtime_error("A mod cannot depend on itself");
        CompareVersions(dependency.version,dependency.version);
        if(std::any_of(m.dependencies.begin(),m.dependencies.end(),[&](auto& x){return x.id==dependency.id;}))
            throw std::runtime_error("Duplicate mod dependency");
        m.dependencies.push_back(std::move(dependency));
    }
    if(j.Has("conflicts"))for(const auto& c:j.At("conflicts").Items()) {
        auto id=c.Text();Id(id);if(id==m.id)throw std::runtime_error("A mod cannot conflict with itself");m.conflicts.push_back(id);
    }
    if(j.Has("features"))for(const auto& feature:j.At("features").Items()) {
        const auto& name=feature.Text();m.features.push_back(name);
        if(name!="files"&&name!="gamedata"&&name!="voices"&&name!="fonts"&&name!="movies"&&name!="lua"&&name!="hdtextures")
            m.unsupported_reason="Update OpenShao to use this mod feature: "+name;
    }
    return m;
}
std::vector<std::string_view> Split(std::string_view text,char sep) {
    std::vector<std::string_view> parts;
    do { const auto pos=text.find(sep); parts.push_back(text.substr(0,pos)); if(pos==text.npos) break; text.remove_prefix(pos+1); } while(true);
    return parts;
}
uint64_t Digits(std::string_view s) {
    uint64_t n=0; auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(s.empty()||r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||(s.size()>1&&s.front()=='0')) throw std::runtime_error("Invalid semantic version"); return n;
}
struct Version { std::vector<std::string_view> core,pre; };
Version ReadVersion(std::string_view text) {
    const auto plus=text.find('+'); if(plus!=text.npos) text=text.substr(0,plus);
    const auto dash=text.find('-'); Version v{Split(text.substr(0,dash),'.'),{}};
    if(v.core.size()!=3) throw std::runtime_error("Expected a major.minor.patch version");
    for(auto p:v.core) Digits(p);
    if(dash!=text.npos) { v.pre=Split(text.substr(dash+1),'.'); for(auto p:v.pre) {
        if(p.empty()||p.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")!=p.npos) throw std::runtime_error("Invalid prerelease version");
        if(p.find_first_not_of("0123456789")==p.npos) Digits(p);
    } }
    return v;
}
}
int CompareVersions(std::string_view a,std::string_view b) {
    const auto x=ReadVersion(a),y=ReadVersion(b);
    for(size_t i=0;i<3;++i) { auto p=Digits(x.core[i]),q=Digits(y.core[i]); if(p!=q) return p<q?-1:1; }
    if(x.pre.empty()!=y.pre.empty()) return x.pre.empty()?1:-1;
    for(size_t i=0;i<std::min(x.pre.size(),y.pre.size());++i) {
        auto p=x.pre[i],q=y.pre[i]; if(p==q) continue;
        const bool pn=p.find_first_not_of("0123456789")==p.npos, qn=q.find_first_not_of("0123456789")==q.npos;
        if(pn!=qn) return pn?-1:1;
        if(pn) return Digits(p)<Digits(q)?-1:1;
        return p<q?-1:1;
    }
    return x.pre.size()==y.pre.size()?0:x.pre.size()<y.pre.size()?-1:1;
}
std::vector<Mod> ParseCatalog(std::string_view json) {
    const auto j=Json::Parse(json);
    const auto schema=j.At("schema").Number();
    if((schema!=1&&schema!=2)||Text(j.At("game"),"xbe_sha256")!=GameSha256) throw std::runtime_error("Unsupported mods catalog or game version");
    std::vector<Mod> mods; std::set<std::string> ids;
    for(const auto& row:j.At("mods").Items()) {
        auto m=ReadMod(row); if(!ids.insert(m.id).second) throw std::runtime_error("Duplicate mod id");
        for(const auto& v:row.At("versions").Items()) {
            Release r{Text(v,"version"),Text(v,"recomp"),Text(v,"url"),Text(v,"sha256"),Number(v,"size")};
            CompareVersions(r.version,r.version); CompareVersions(r.recomp,r.recomp); Hash(r.sha256);
            const bool official=OfficialRelease(r.url);
            if(!official||!r.size||r.size>2ull*1024*1024*1024) throw std::runtime_error("Invalid mod release URL or size");
            m.versions.push_back(r);
        }
        std::sort(m.versions.begin(),m.versions.end(),[](auto& a,auto& b){return CompareVersions(a.version,b.version)>0;}); mods.push_back(std::move(m));
    } return mods;
}
Manifest ParseManifest(std::string_view json) {
    const auto j=Json::Parse(json); Manifest m; static_cast<Mod&>(m)=ReadMod(j);
    const auto schema=j.At("schema").Number();m.schema=unsigned(schema);
    if((schema<1||schema>3)||Text(j.At("game"),"xbe_sha256")!=GameSha256) throw std::runtime_error("This mod requires a different game version");
    if(!m.unsupported_reason.empty())throw std::runtime_error(m.unsupported_reason);
    m.version=Text(j,"version"); m.recomp=Text(j,"recomp"); m.raw=json;
    CompareVersions(m.version,m.version); if(CompareVersions(m.recomp,RecompVersion)>0) throw std::runtime_error("Update OpenShao before installing this mod");
    std::set<std::string> paths;
    auto unique=[&](const std::string& p) { Path(p); std::string key=p; for(auto& c:key) if(c>='A'&&c<='Z') c+=32;
        if(key=="mod.json"||key=="manifest.json"||key.ends_with(".xdelta")||!paths.insert(key).second) throw std::runtime_error("Reserved or duplicate payload path"); };
    if(j.Has("ostp")) {
        // A texture pack in one file holds nothing else. It is larger than an ordinary file can be.
        const auto& o=j.At("ostp");
        if(schema<3)throw std::runtime_error("A texture pack file needs a schema 3 manifest");
        Payload p{Text(o,"path"),Text(o,"sha256"),Number(o,"size")};Hash(p.sha256);
        if(p.path!="pack.ostp"||!p.size||p.size>16ull*1024*1024*1024)throw std::runtime_error("Invalid texture pack file in mod metadata");
        if(m.type!="texture"||std::find(m.features.begin(),m.features.end(),"hdtextures")==m.features.end())
            throw std::runtime_error("A texture pack file belongs to a texture mod with the hdtextures feature");
        const auto empty=[&](const char* key){return !j.Has(key)||j.At(key).Items().empty();};
        if(!empty("files")||!empty("patches")||!empty("routines"))throw std::runtime_error("A mod with a texture pack file holds no other files, patches or routines");
        m.ostp=p;
        return m;
    }
    for(const auto& f:j.At("files").Items()) {
        Payload p{Text(f,"path"),Text(f,"sha256"),Number(f,"size")}; unique(p.path); Hash(p.sha256);
        if(p.size>512*1024*1024) throw std::runtime_error("Mod file exceeds 512 MiB"); m.files.push_back(p);
    }
    for(const auto& p:j.At("patches").Items()) {
        Patch patch; patch.path=Text(p,"path"); unique(patch.path); patch.source_hash=Text(p,"source_sha256"); patch.target_hash=Text(p,"target_sha256");
        Hash(patch.source_hash); Hash(patch.target_hash); const auto& s=p.At("source");
        if(s.Has("wad")) { patch.wad=Text(s,"wad"); if(patch.wad!="GAMEDATA") throw std::runtime_error("Unsupported patch archive"); patch.record=Number(s,"record"); }
        else { patch.file=Text(s,"file"); Path(patch.file); patch.range=s.Has("offset");
            if(patch.range) { patch.offset=Number(s,"offset"); patch.size=Number(s,"size"); } }
        m.patches.push_back(patch);
    }
    if(m.files.size()+m.patches.size()>20000 || m.files.empty()&&m.patches.empty()) throw std::runtime_error("Invalid mod file count");
    if(j.Has("routines"))for(const auto& r:j.At("routines").Items()) {
        const auto& path=r.Text();Path(path);
        if(!path.starts_with("scripts/")||!path.ends_with(".lua")||std::find(m.routines.begin(),m.routines.end(),path)!=m.routines.end())
            throw std::runtime_error("Each routine must name a unique scripts/*.lua file");
        if(std::none_of(m.files.begin(),m.files.end(),[&](auto& f){return f.path==path;}))
            throw std::runtime_error("Routine scripts must be declared ordinary package files");
        m.routines.push_back(path);
    }
    return m;
}
}
