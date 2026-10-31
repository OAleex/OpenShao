#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include "mouse_state.h"
#include "input_bindings.h"
#include "aspect_ratio.h"
#include "brightness.h"

namespace openshao::launcher {
inline constexpr int FrameRate=60;
inline constexpr std::array<const char*, 6> languages{"en", "fr", "de", "es", "it", "pt-BR"};
inline constexpr std::array<const char*,6> voice_languages{"en","fr","de","es","it","pt-BR"};
inline constexpr auto binding_keys = bindings::keys;
struct Settings {
#ifdef _WIN32
    std::string renderer = "d3d12";
#else
    std::string renderer = "vulkan";
#endif
    std::string voice_language = "en";
    std::string language = "en", display_mode = "borderless", aspect_ratio = "original";
    int width = 0, height = 0, render_percent = 100, aniso = 0;
    int fps_cap = FrameRate;
    int master_volume = 100;
    double deadzone = .15;
    bool vsync = true, bloom_effect = false, show_fps = false, rumble = true;
    bool scorpion_unfiltered = false;
    bool restore_dialogue = true;
    bool cinematic_bars = false;
    bool improved_difficulty = true;
    bool cheats_enabled = false;
    int cheat_menu_key = 0x77; // VK_F8
    bool statue_full_heal = true;
    int cheat_menu_x = 100, cheat_menu_y = 5; // percent of the free screen space
    mouse::Settings mouse;
    bool original_override = false, camera_reverse = false;
    int music_volume = 90, effects_volume = 90, speech_volume = 90;
    int audio_output = 1; // Mono / Stereo / Surround in Konfigure Recompilation.
    int brightness = brightness::DefaultPercent, versus_rounds = 3, versus_time = 120;
    std::array<std::string,24> bindings = bindings::controller_defaults;
    bindings::Keyboard keyboard;
    bool operator==(const Settings&) const = default;
};
Settings Read(const toml::table& table);
toml::table Merge(toml::table table, const Settings& settings);
bool Validate(const Settings& settings, std::string& error);
bool Write(const std::filesystem::path& file, const toml::table& table, std::string& error);
unsigned GameLanguage(const std::string& language);
}
