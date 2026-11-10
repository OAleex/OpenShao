#include "restart.h"
#include "restart_process.h"
#include "host.h"
#include "launcher.h"
#include "runtime_paths.h"
#include "app/app.h"
#include "core/log.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif

namespace openshao::restart {
namespace {
std::filesystem::path source,save_directory;
bool disc_source=false;
std::mutex lock;
std::unique_ptr<Child> prepared;
std::string Utf8(const std::filesystem::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
}
void GameSource(const std::filesystem::path& value,bool disc){source=std::filesystem::absolute(value);disc_source=disc;}
std::filesystem::path CurrentGameSource(){return source;}
void SaveDirectory(const std::filesystem::path& value){save_directory=std::filesystem::absolute(value);}
const std::filesystem::path& ActiveSaveDirectory(){return save_directory;}
bool Prepare(const launcher::Settings& settings,std::string& error) {
    const auto& language=settings.language;
    std::lock_guard guard(lock);
    if(prepared){error="A language restart is already pending";return false;}
    if(!launcher::Validate(settings,error))return false;
    if(std::find(launcher::languages.begin(),launcher::languages.end(),language)==launcher::languages.end()) {
        error="Unknown game language";return false;
    }
    if(source.empty() || save_directory.empty()){error="The active game and save directories are unavailable";return false;}
    std::vector<std::string> args{"--no-launcher","--resume-main-menu","--restart-language",std::string(language),"--restart-voice-language",settings.voice_language,
        disc_source?"--disc":"--game",Utf8(source),"--data-dir",Utf8(paths::Root()),"--save-dir",Utf8(save_directory)};
    if((language=="pt-BR" || settings.voice_language=="pt-BR") && !options.language_pack.empty())args.insert(args.end(),{"--language-pack",Utf8(options.language_pack)});
    if(options.background) {
        args.insert(args.end(),{"--background","--timeout",std::to_string(options.timeout)});
        if(options.virtual_gamepads)args.insert(args.end(),{"--virtual-gamepads",std::to_string(options.virtual_gamepads)});
    }
#ifdef _WIN32
    const auto executable=app::ExeDir()/L"OpenShao.exe";
#else
    const auto executable=app::ExeDir()/"OpenShao";
#endif
    auto child=std::make_unique<Child>();
    if(!child->Prepare(executable,std::move(args),std::chrono::seconds(30),error))return false;
    prepared=std::move(child);LOGR("Language restart prepared: %.*s",int(language.size()),language.data());return true;
}
bool Commit(std::string& error) {
    std::lock_guard guard(lock);
    if(!prepared){error="No language restart has been prepared";return false;}
    if(!prepared->Running()){error="The prepared replacement process is no longer running";return false;}
#ifdef _WIN32
    const bool sent=PostMessageW(static_cast<HWND>(app::Window()),WM_CLOSE,0,0)!=FALSE;
#else
    SDL_Event event{};event.type=SDL_EVENT_QUIT;const bool sent=SDL_PushEvent(&event);
#endif
    if(!sent){error="The active game window could not be closed";return false;}
    prepared->Release();prepared.reset();LOGR("Language restart committed");return true;
}
void Cancel(){std::lock_guard guard(lock);prepared.reset();}
bool ContinueStartup(std::string& error) {
    if(options.restart_parent) {
        const bool ready=AwaitParent(options.restart_parent,options.restart_ready,[&](std::string& message) {
            options.restart_preflight=true;app::GameFiles game;
            const bool valid=app::FindGame(game);options.restart_preflight=false;
            if(!valid)message="The selected language resources could not be prepared; the current game remains open";
            return valid;
        },std::chrono::seconds(45),error);
        options.restart_ready=0;
        if(!ready)return false;
        options.restart_parent=0;options.restart_language.clear();
        // The old process has exited: acquire the data lock and read the newly committed complete draft.
        if(!paths::Initialize(app::ExeDir(),options.background,options.data_dir,error))return false;
        app::Load();LOGR("Language restart: previous session exited; settings reloaded");
    }
    BeginResume(options.resume_main_menu);return true;
}
}
