#include "host.h"
#include "app/app.h"
#include "core/log.h"
#include "kernel/disc.h"
#include "game_setup.h"
#include "runtime_paths.h"
#include "launcher.h"
#include "restart.h"
#include "language_pack.h"
#include "texture_pack.h"
#include <cstdlib>
#include "mods/manager.h"
#include "mods/mod_settings.h"
#include "mods/languages.h"
#include "mods/runtime.h"
#include "mods/sha256.h"
#include "file_ops.h"
#include <algorithm>
#include <mutex>
#include <fstream>
#include <map>

namespace {
std::unique_ptr<openshao::storage::DirectoryLock> mod_lock;
std::filesystem::path OriginalPath(const std::filesystem::path& root,std::string name) {
    std::replace(name.begin(),name.end(),'\\','/');
    if(!openshao::files::SafeRelative(name))return {};
    auto path=root;
    for(const auto& part:openshao::files::FromUtf8(name)) {
        auto next=path/part;
        if(!std::filesystem::exists(next)) {
            auto key=openshao::files::Utf8(part);
            const auto lower=[](std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+='a'-'A';return value;};
            key=lower(key);
            if(!std::filesystem::is_directory(path))return {};
            for(const auto& item:std::filesystem::directory_iterator(path))
                if(lower(openshao::files::Utf8(item.path().filename()))==key){next=item.path();break;}
        }
        if(std::filesystem::is_symlink(next))return {};
        path=next;
    }
    return path;
}
// The texture sources that need no installed mod: the developer mode's folder (the launcher's Developer page), and before it the
// folder or pack file that OPENSHAO_HD_PACK_DIR names, so a pack can be tried without packaging and installing it
void AddDevelopmentTexturePacks(std::vector<openshao::texpack::PackSpec>& ordered) {
    using namespace openshao;
    if(const auto developer=launcher::DeveloperTextureFolder();!developer.empty())ordered.insert(ordered.begin(),texpack::PackSpec{developer});
    if(const char* folder=std::getenv("OPENSHAO_HD_PACK_DIR");folder&&*folder)ordered.insert(ordered.begin(),texpack::PackSpec{std::filesystem::path(folder)});
}
bool MountLanguage(const openshao::setup::Source& source,const app::GameFiles& game,std::string& error) {
    using namespace openshao;
    language::Disable();mods::StageRoutines({});texpack::SetPacks({});
    try {
        const auto selected=options.restart_preflight?options.restart_language:options.validate_language_pack?std::string("pt-BR"):launcher::Current().language;
        auto settings=launcher::Current();settings.language=selected;
        if(options.restart_preflight)settings.voice_language=options.restart_voice_language;
        if(options.validate_language_pack)settings.voice_language=selected;
        const auto voices=settings.voice_language;
        std::vector<language::PackSelection> packs;
        std::vector<mods::RoutineScript> scripts;
        std::vector<std::pair<std::string,texpack::PackSpec>> hd_packs;  // with the id of the mod, to put them in the user's order
        bool credits=false;
        if(!options.language_pack.empty()){packs.push_back({options.language_pack,selected=="pt-BR",voices=="pt-BR"});credits=selected=="pt-BR";}
        else {
            if(!options.restart_preflight) {
                mods::Recover(paths::Mods());
                mod_lock=std::make_unique<storage::DirectoryLock>();
                if(!mod_lock->Acquire(paths::Mods(),error))return false;
            }
            const auto active=mods::ActiveMods(paths::Mods(),selected);
            bool language_ready=mods::IsOriginalLanguage(selected);
            for(const auto& row:active) {
                const auto pack=paths::Mods()/files::FromUtf8(row.mod.install_dir);
                if(!files::Within(pack,paths::Mods())||!std::filesystem::is_directory(pack))
                    throw std::runtime_error("Installed mod folder is missing: "+row.mod.name);
                packs.push_back({pack,true,row.mod.type!="language" || mods::SameLanguage(selected,voices)});
                if(row.mod.type=="language") {language_ready=true;credits=selected=="pt-BR";}
                if(row.mod.ostp) {
                    // checked as it was installed; here only its size, and the reader checks its structure when it opens it
                    const auto file=pack/"pack.ostp";std::error_code ec;
                    if(!std::filesystem::is_regular_file(file,ec)||std::filesystem::file_size(file,ec)!=row.mod.ostp->size)
                        throw std::runtime_error("Installed texture pack file changed: "+row.mod.name);
                    hd_packs.push_back({row.mod.id,{file,mods::ReadTextureQuality(paths::Mods(),row.mod.id)}});
                } else if(std::find(row.mod.features.begin(),row.mod.features.end(),"hdtextures")!=row.mod.features.end()) {
                    const auto index=std::find_if(row.mod.files.begin(),row.mod.files.end(),[](auto& f){return f.path=="hd/index.txt";});
                    if(index==row.mod.files.end()||mods::Sha256File(pack/"hd/index.txt")!=index->sha256)
                        throw std::runtime_error("Installed texture pack index changed: "+row.mod.name);
                    hd_packs.push_back({row.mod.id,pack/"hd"});
                }
                for(const auto& path:row.mod.routines) {
                    const auto script=pack/files::FromUtf8(path);
                    const auto file=std::find_if(row.mod.files.begin(),row.mod.files.end(),[&](auto& f){return f.path==path;});
                    if(file==row.mod.files.end()||mods::Sha256File(script)!=file->sha256)
                        throw std::runtime_error("Installed routine script changed: "+row.mod.name);
                    scripts.push_back({row.mod.id,script});
                }
            }
            if(!language_ready)throw std::runtime_error("Install and enable a pack for the selected language in Mods, or select another language.");
            if(!mods::SameLanguage(selected,voices) && !mods::IsOriginalLanguage(voices)) {
                bool voice_ready=false;
                for(const auto& row:mods::ActiveMods(paths::Mods(),voices))if(row.mod.type=="language") {
                    const auto pack=paths::Mods()/files::FromUtf8(row.mod.install_dir);
                    if(!files::Within(pack,paths::Mods()) || !std::filesystem::is_directory(pack))
                        throw std::runtime_error("Installed voice language pack is missing");
                    packs.push_back({pack,false,true});voice_ready=true;
                }
                if(!voice_ready)throw std::runtime_error("Install and enable a pack for the selected voice language in Mods.");
            }
        }
        if(packs.empty()) {
            // no mod is active, but the texture sources that need none still apply
            std::vector<texpack::PackSpec> development;
            AddDevelopmentTexturePacks(development);
            texpack::SetPacks(development);
            return true;
        }
        const auto* disc=kernel::MountedDisc();
        struct Readers {std::mutex mutex;std::map<std::string,language::ReadAt> files;};
        const auto readers=std::make_shared<Readers>();
        const language::SourceSize size=[disc,path=source.path](std::string_view name)->uint64_t {
            if(disc){const auto* entry=disc->Find(name);return entry&&!entry->dir?entry->size:0;}
            const auto file=OriginalPath(path,std::string(name));std::error_code ec;
            const auto length=std::filesystem::file_size(file,ec);return ec?0:length;
        };
        const language::SourceRead read=[disc,readers,path=source.path](std::string_view name,uint64_t at,std::span<uint8_t> bytes) {
            if(disc) {
                const auto* entry=disc->Find(name);
                return entry&&!entry->dir&&bytes.size()<=UINT32_MAX&&disc->Read(*entry,at,bytes.data(),uint32_t(bytes.size()))==bytes.size();
            }
            language::ReadAt reader;
            {
                std::lock_guard lock(readers->mutex);
                auto found=readers->files.find(std::string(name));
                if(found==readers->files.end()) {
                    const auto file=OriginalPath(path,std::string(name));
                    if(file.empty())return false;
                    found=readers->files.emplace(name,language::PackFileReader(file)).first;
                }
                reader=found->second;
            }
            return reader(at,bytes);
        };
        const bool mounted=language::MountPacks(packs,game.xbe,read,size,credits,error);
        if(!mounted)return false;
        if(options.validate_language_pack&&!language::ValidateMovies(error))return false;
        mods::StageRoutines(std::move(scripts));
        // The first pack wins when several have a texture: the order is the user's, from the Mods page
        std::vector<texpack::PackSpec> ordered;
        if(!hd_packs.empty()) {
            std::vector<std::string> installed;
            for(const auto& row:mods::ReadInstalled(paths::Mods()))if(row.mod.type=="texture")installed.push_back(row.mod.id);
            const auto order=mods::TextureOrder(paths::Mods(),installed);
            const auto rank=[&](const std::string& id){return std::find(order.begin(),order.end(),id)-order.begin();};
            std::stable_sort(hd_packs.begin(),hd_packs.end(),[&](const auto& a,const auto& b){return rank(a.first)<rank(b.first);});
            for(auto& pack:hd_packs)ordered.push_back(std::move(pack.second));
        }
        // The development sources come before every texture mod
        AddDevelopmentTexturePacks(ordered);
        texpack::SetPacks(ordered);
        const auto& stats=language::Statistics();
        LOGR("Language resources: text=%s voices=%s",selected.c_str(),voices.c_str());
        LOGR("Mods ready: packs=%zu resources=%u voices=%u occurrences=%u movies=%u",packs.size(),stats.archive.resources,
             stats.archive.voice_names,stats.archive.voice_occurrences,stats.movies);
        return true;
    }catch(const std::exception& e){error=e.what();language::Disable();mods::StageRoutines({});texpack::SetPacks({});return false;}
}
}

bool app::FindGame(GameFiles& game) {
    bool remember = openshao::options.register_disc;
    auto input = openshao::options.disc.empty() ? openshao::options.game : openshao::options.disc;
    if (input.empty() && !openshao::options.background) {
        if (!openshao::options.setup) input = openshao::setup::RememberedDisc();
        if (openshao::options.setup || input.empty() || !std::filesystem::exists(input)) {
            if (!openshao::setup::ChooseDisc(input)) return false;
            remember = true;
        }
    }
    const auto failure = [&](const std::string& message) {
        LOGE("Game setup: %s", message.c_str());
        if (!openshao::options.validate_disc && !openshao::options.validate_language_pack && !openshao::options.restart_preflight)
            openshao::paths::ReportError(std::filesystem::path(std::u8string(message.begin(), message.end())).wstring());
        return false;
    };
    if (input.empty()) return failure("Select your Xbox game with --setup.");
    openshao::setup::Source source;
    std::vector<uint8_t> bytes;
    std::string error;
    if (!openshao::setup::PrepareGame(input, source, bytes, error,
                                    !openshao::options.background && !openshao::options.validate_disc && !openshao::options.validate_language_pack && !openshao::options.restart_preflight))
        return failure(error);
    if (remember && openshao::launcher::ShouldPersist() && !openshao::setup::RememberDisc(source.path, error)) return failure(error);
    if (source.folder) {
        kernel::MountDisc(nullptr);
        game = {source.path, std::move(bytes)};
    } else {
        auto disc = std::make_unique<kernel::Disc>();
        if (!disc->Open(source.path, error)) return failure(error);
        kernel::MountDisc(std::move(disc));
        game = {openshao::paths::Cache() / "dvd-scratch", std::move(bytes)};
    }
    if (!MountLanguage(source, game, error)) return failure(error);
    if(!openshao::options.restart_preflight)openshao::restart::GameSource(source.path,!source.folder);
    LOGR("Verified Shaolin Monks game files");
    return true;
}
