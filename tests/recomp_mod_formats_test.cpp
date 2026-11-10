#include "mods/sha256.h"
#include "mods/json.h"
#include "mods/vcdiff.h"
#include "mods/catalog.h"
#include "mods/browser.h"
#include <iostream>
#include <stdexcept>
#include <string>
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
std::string Manifest(const std::string& id, const std::string& type, const std::string& language = "") {
    return R"({"schema":2,"id":")" + id + R"(","name":"Synthetic mod","summary":"Test","authors":["Tester"],"type":")" + type +
        R"(","language":")" + language + R"(","install_dir":"packages/)" + id +
        R"(","version":"1.0.0","recomp":"0.1.0","game":{"xbe_sha256":"48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a"},"files":[{"path":"files/test.bin","size":3,"sha256":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"}],"patches":[]})";
}
int main() { try {
    // Rejecting these packages would leave new catalog entries unusable.
    for (const auto* type : {"texture", "data", "gameplay", "mixed"}) {
        const auto mod = openshao::mods::ParseManifest(Manifest("NewMod", type));
        Require(mod.type == type && mod.install_dir == "packages/NewMod", "General mod types must be installable");
    }
    {
        const auto with_feature=[](const std::string& feature) {
            auto json=Manifest("HdPack","texture");
            const auto at=json.find(R"("version":"1.0.0")");
            Require(at!=std::string::npos,"Feature fixture must contain a version");
            json.insert(at,R"("features":[")"+feature+R"("],)");
            return openshao::mods::ParseManifest(json);
        };
        Require(with_feature("hdtextures").unsupported_reason.empty(),"High resolution texture packs must be supported");
        bool rejected=false;
        try{with_feature("future-feature");}catch(...){rejected=true;}
        Require(rejected,"Unknown mod features must stay unavailable");
    }
    const auto translated = openshao::mods::ParseManifest(Manifest("Japanese", "language", "ja"));
    for(const auto* reserved:{"mod.json","MOD.JSON","manifest.json"}) {
        auto json=Manifest("Reserved","data");
        const auto offset=json.find("files/test.bin");
        Require(offset!=std::string::npos,"Reserved-path fixture must contain a payload");
        json.replace(offset,std::string("files/test.bin").size(),reserved);
        bool rejected=false;
        try{openshao::mods::ParseManifest(json);}catch(...){rejected=true;}
        Require(rejected,"Package metadata names must not be accepted as payload paths");
    }
    Require(translated.language == "ja", "New language packs must not require the PT-BR ID or locale");
    const auto catalog=openshao::mods::ParseCatalog(R"({"schema":1,"game":{"xbe_sha256":"48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a"},"mods":[
        {"id":"Future","name":"Future","summary":"Test","type":"future-type","authors":["Tester"],"install_dir":"packages/Future","versions":[]},
        {"id":"Japanese","name":"Japanese","language_name":"Japanese","summary":"Test","type":"language","language":"ja","authors":["Tester"],"install_dir":"packages/Japanese","versions":[
        {"version":"3.0.0","recomp":"9.0.0","url":"https://github.com/OAleex/OpenShao-Mods/releases/download/test/future.tar.xz","size":1,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"},
        {"version":"2.0.0","recomp":"0.2.0","url":"https://github.com/OAleex/OpenShao-Mods/releases/download/test/good.tar.xz","size":1,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]}
    ]})");
    Require(catalog.size()==2&&!catalog[0].unsupported_reason.empty(),"Unsupported future mod types must not hide compatible catalog entries");
    Require(!openshao::mods::LatestCompatible(catalog[0])&&openshao::mods::LatestCompatible(catalog[1])->version=="2.0.0",
            "Updates must select the newest release compatible with this runtime");
    // Release assets are accepted from the maintainer's repositories under any name, and from nowhere else.
    auto release_catalog=[](const std::string& url){
        return R"({"schema":1,"game":{"xbe_sha256":"48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a"},"mods":[
            {"id":"Pack","name":"Pack","summary":"Test","type":"data","authors":["Tester"],"install_dir":"packages/Pack","versions":[
            {"version":"1.0.0","recomp":"0.2.0","url":")"+url+R"(","size":1,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]}]})";
    };
    for(const auto* url:{"https://github.com/OAleex/OpenShao-Mods/releases/download/v1/pack.tar.xz",
                         "https://github.com/OAleex/Earlier-Name_2.0/releases/download/v1/pack.tar.xz"})
        Require(openshao::mods::ParseCatalog(release_catalog(url)).front().versions.size()==1,"Releases of the maintainer's repositories must be accepted");
    for(const auto* url:{"https://github.com/Other/OpenShao-Mods/releases/download/v1/pack.tar.xz",
                         "https://github.com/OAleexx/OpenShao-Mods/releases/download/v1/pack.tar.xz",
                         "https://github.com/OAleex//releases/download/v1/pack.tar.xz",
                         "https://github.com/OAleex/OpenShao-Mods/archive/v1.tar.gz",
                         "http://github.com/OAleex/OpenShao-Mods/releases/download/v1/pack.tar.xz",
                         "https://github.com/OAleex/a/b/releases/download/v1/pack.tar.xz"}) {
        bool rejected=false;
        try{openshao::mods::ParseCatalog(release_catalog(url));}catch(...){rejected=true;}
        Require(rejected,"Release assets outside the maintainer's repositories must be rejected");
    }
    for (const auto& invalid : {Manifest("../escape", "texture"), Manifest("Bad", "unknown"), Manifest("Bad", "language", "../ja")}) {
        bool rejected = false;
        try { openshao::mods::ParseManifest(invalid); } catch (...) { rejected = true; }
        Require(rejected, "Invalid or unsupported mod metadata must remain rejected");
    }
    Require(openshao::mods::Sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 empty vector");
    const std::string abc = "abc";
    Require(openshao::mods::Sha256({reinterpret_cast<const uint8_t*>(abc.data()), abc.size()}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc vector");
    const auto parsed = openshao::mods::Json::Parse(R"({"name":"Tradu\u00e7\u00f5es \ud83d\udc09","n":42,"a":[true,null]})");
    Require(parsed.At("name").Text() == "Tradu\xc3\xa7\xc3\xb5" "es \xf0\x9f\x90\x89", "JSON Unicode and surrogate pairs");
    Require(parsed.At("n").Number() == 42 && parsed.At("a").Items().size() == 2, "JSON nested values");
    for (const auto* bad : {"{\"x\":1,\"x\":2}", "[1,]", "\"\\ud800\"", "{} junk", "01"}) {
        bool rejected=false; try { openshao::mods::Json::Parse(bad); } catch(...) { rejected=true; }
        Require(rejected, "Malformed JSON must be rejected");
    }
    const std::string hex="d6c3c40004177461726765742e62696e2f2f736f757263652e62696e2f051c00208874000b080326ae79cc7472616e736c6174656421140b201387720064000c20";
    std::vector<uint8_t> patch;
    for(size_t i=0;i<hex.size();i+=2) patch.push_back(uint8_t(std::stoul(hex.substr(i,2),nullptr,16)));
    std::string original,target;
    for(int i=0;i<40;++i) { original+="The original game data.\n"; target+="The translated game data.\n"; }
    target+=std::string(100,'!');
    const auto decoded=openshao::mods::ApplyDelta({reinterpret_cast<const uint8_t*>(original.data()),original.size()},patch);
    Require(std::string(decoded.begin(),decoded.end())==target,"Real xdelta3 -9 -S none fixture must decode with app header and Adler-32");
    patch.back()^=1; bool rejected=false;
    try { openshao::mods::ApplyDelta({reinterpret_cast<const uint8_t*>(original.data()),original.size()},patch); } catch(...) { rejected=true; }
    Require(rejected,"Corrupt patches must be rejected");
    std::cout << "Mod format checks passed\n"; return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
