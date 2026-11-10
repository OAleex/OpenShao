#include "discord_presence.h"
#include "discord_rpc.h"
#include "host.h"
#include <ctime>
#include <cstdlib>
#include <mutex>
#include <string_view>

namespace openshao::discord {
namespace {
constexpr const char* application_id = "1566670201318023228";
// The registered icon also works before any Rich Presence art assets are uploaded.
constexpr const char* icon_url = "https://cdn.discordapp.com/app-icons/1566670201318023228/731f0725bc313e4df003bd9ca82726c6.png";

class Connection {
public:
    ~Connection() { Stop(); }
    void Set(State state) {
        std::lock_guard lock(mutex_);
        if (active_ && state_ == state) return;
        if (!active_) {
            // The library owns the reconnecting IPC worker. Do not register or start Discord.
            try { Discord_Initialize(application_id, nullptr, 0, nullptr); }
            catch (...) { Discord_Shutdown(); return; }
            active_ = true;
        }
        DiscordRichPresence activity{};
        activity.details = state == State::Launcher ? "In launcher" : "Playing OpenShao";
        activity.startTimestamp = std::time(nullptr);
        activity.largeImageKey = icon_url;
        activity.largeImageText = "OpenShao";
        Discord_UpdatePresence(&activity);
        state_ = state;
    }
    void Stop() {
        std::lock_guard lock(mutex_);
        if (!active_) return;
        Discord_ClearPresence();
        Discord_Shutdown();
        active_ = false;
    }
private:
    std::mutex mutex_;
    bool active_ = false;
    State state_ = State::Launcher;
};

Connection& Current() {
    static Connection connection;
    return connection;
}
}

void SetState(State state, const Options& options, bool enabled) {
    if (!enabled) { Shutdown(); return; }
    if (const char* disabled = std::getenv("OPENSHAO_DISABLE_DISCORD"); disabled && std::string_view(disabled) == "1") return;
    if (options.background || options.host_test || options.info || options.paths || options.support_report ||
        options.validate_disc || options.register_disc || options.validate_language_pack ||
        options.restart_preflight || !options.launcher_test.empty()) return;
    Current().Set(state);
}

void Shutdown() { Current().Stop(); }
}
