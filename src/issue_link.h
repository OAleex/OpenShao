#pragma once
#include <string>
#include <string_view>
#include "launcher_settings.h"

// GitHub issue-form links with display/gameplay settings only (no paths, accounts or hardware IDs).
namespace openshao::support {
inline constexpr std::string_view IssuesUrl = "https://github.com/OAleex/OpenShao/issues/new";
enum class IssueKind { Bug, Crash };

inline std::string PercentEncode(std::string_view text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') out.push_back(char(c));
        else { out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
    }
    return out;
}

inline std::string IssueEnvironment(const launcher::Settings& s, std::string_view version, std::string_view platform) {
    const auto on = [](bool value) { return value ? "on" : "off"; };
    const auto size = [](int value) { return value > 0 ? std::to_string(value) : std::string("desktop"); };
    std::string text;
    text += "OpenShao " + std::string(version) + " (" + std::string(platform) + ")\n";
    text += "Renderer: " + std::string(s.renderer == "d3d12" ? "Direct3D 12" : s.renderer == "vulkan" ? "Vulkan" : s.renderer) + "\n";
    text += "Display: " + s.display_mode + ", " + size(s.width) + " x " + size(s.height) +
            ", aspect " + s.aspect_ratio + ", render scale " + std::to_string(s.render_percent) + "%\n";
    text += "Frame rate limit: " + std::to_string(s.fps_cap) + ", VSync " + on(s.vsync) + "\n";
    text += "Menu language: " + s.language + ", voices: " + s.voice_language + "\n";
    text += "Improved difficulty " + std::string(on(s.improved_difficulty)) + ", restored dialogue " +
            on(s.restore_dialogue) + ", bloom " + on(s.bloom_effect) + ", cinematic bars " + on(s.cinematic_bars) + "\n";
    return text;
}

inline std::string IssueLink(IssueKind kind, std::string_view environment) {
    const std::string_view form = kind == IssueKind::Crash ? "crash.yml" : "bug_report.yml";
    return std::string(IssuesUrl) + "?template=" + std::string(form) + "&environment=" + PercentEncode(environment);
}
}
