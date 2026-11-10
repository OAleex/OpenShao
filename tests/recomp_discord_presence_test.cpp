// Capture the Discord boundary without connecting to a user's desktop client.
#include "discord_presence.h"
#include "discord_rpc.h"
#include "host.h"
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Published {
    std::string details, image, image_text;
    int64_t start;
};
std::vector<Published> published;
std::vector<std::string> calls;
std::string application;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

extern "C" void Discord_Initialize(const char* id, DiscordEventHandlers*, int registration, const char* steam) {
    application = id;
    Require(registration == 0 && steam == nullptr, "Presence must not register protocols or launch Discord");
    calls.push_back("start");
}
extern "C" void Discord_UpdatePresence(const DiscordRichPresence* activity) {
    Require(activity && activity->details && activity->largeImageKey && activity->largeImageText,
            "Published activity must include a status and the app icon");
    Require(!activity->partyId && !activity->joinSecret && !activity->spectateSecret,
            "Local play must not advertise online join or spectate support");
    published.push_back({activity->details, activity->largeImageKey, activity->largeImageText, activity->startTimestamp});
    calls.push_back("publish");
}
extern "C" void Discord_ClearPresence() { calls.push_back("clear"); }
extern "C" void Discord_Shutdown() { calls.push_back("stop"); }

int main() {
    using namespace openshao;
    using discord::State;
    try {
        const auto before = std::time(nullptr);
        discord::SetState(State::Launcher, {});
        Require(calls == std::vector<std::string>{"start", "publish"},
                "Opening the launcher must initialize and publish its activity");
        Require(application == "1566670201318023228", "The connection must use OpenShao's application");
        Require(published.back().details == "In launcher", "The launcher status must be in English");
        Require(published.back().image.starts_with("https://cdn.discordapp.com/app-icons/1566670201318023228/"),
                "Presence must use the icon already registered for this application");
        Require(published.back().image_text == "OpenShao" && published.back().start >= before &&
                published.back().start <= std::time(nullptr), "The activity must identify the app and start its timer");
        discord::SetState(State::Launcher, {});
        Require(published.size() == 1, "Repeated state updates must not reset the activity timer");
        discord::SetState(State::Playing, {});
        Require(published.size() == 2 && published.back().details == "Playing OpenShao",
                "Starting the game must replace the launcher activity");
        discord::Shutdown();
        discord::Shutdown();
        Require(calls == std::vector<std::string>{"start", "publish", "publish", "clear", "stop"},
                "Closing must clear and stop the connection exactly once");

        calls.clear();
        discord::SetState(State::Launcher, {}, false);
        Require(calls.empty(), "A saved disabled preference must prevent the initial connection");
        discord::SetState(State::Launcher, {}, true);
        discord::SetState(State::Launcher, {}, false);
        Require(calls == std::vector<std::string>{"start", "publish", "clear", "stop"},
                "Disabling presence in the launcher must clear the activity immediately");
        discord::SetState(State::Playing, {}, false);
        Require(calls.size() == 4, "Starting the game must respect the disabled preference");
        discord::SetState(State::Launcher, {}, true);
        Require(published.back().details == "In launcher" && calls.size() == 6,
                "Enabling presence again must reconnect and publish the current state");
        discord::Shutdown();

        calls.clear();
        for (bool Options::* flag : {&Options::background, &Options::host_test, &Options::info,
                &Options::paths, &Options::support_report, &Options::validate_disc, &Options::register_disc,
                &Options::validate_language_pack, &Options::restart_preflight}) {
            Options diagnostic;
            diagnostic.*flag = true;
            discord::SetState(State::Launcher, diagnostic);
            discord::SetState(State::Playing, diagnostic);
        }
        Options automation;
        automation.launcher_test = "isolated-launcher";
        discord::SetState(State::Launcher, automation);
        discord::Shutdown();
        Require(calls.empty(), "Diagnostics and launcher automation must never connect to Discord");

        Options configure;
        configure.configure_only = true;
        discord::SetState(State::Launcher, configure);
        Require(calls == std::vector<std::string>{"start", "publish"},
                "A settings-only launcher must still publish the launcher activity");
        discord::Shutdown();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: Discord states, icon, timer, shutdown and diagnostic isolation\n";
}
