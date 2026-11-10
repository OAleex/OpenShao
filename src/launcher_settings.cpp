#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "platform_compat.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include "launcher_settings.h"
#include "frame_rate.h"
#include "mods/languages.h"

namespace openshao::launcher {
Settings Read(const toml::table& table) {
    Settings s;
#ifdef _WIN32
    if(table["renderer"].value_or(std::string{})=="vulkan")s.renderer="vulkan";
#endif
    s.language = table["language"].value_or(s.language);
    // Convert older preferences to a fixed supported voice language before publishing settings.
    const auto matching=std::find_if(voice_languages.begin(),voice_languages.end(),[&](auto code){return mods::SameLanguage(code,s.language);});
    const std::string previous_voice=matching==voice_languages.end()?s.voice_language:*matching;
    s.voice_language=table["voice_language"].value_or(previous_voice);
    if(s.voice_language=="auto")s.voice_language=previous_voice;
    s.display_mode = table["display_mode"].value_or(s.display_mode);
    s.aspect_ratio = table["aspect_ratio"].value_or(s.aspect_ratio);
    s.width = table["width"].value_or(s.width); s.height = table["height"].value_or(s.height);
    s.render_percent = table["render_percent"].value_or(s.render_percent);
    const auto frame_rate = table["fps_cap"].value_or(FrameRate);
    s.fps_cap = frames::SafeRate(frame_rate);
    s.aniso = table["anisotropic_filtering"].value_or(s.aniso);
    s.master_volume = table["master_volume"].value_or(s.master_volume);
    s.vsync = table["vsync"].value_or(s.vsync);
    // Read prior keys so renaming the preference preserves the user's existing choice.
    s.bloom_effect = table["bloom_effect"].value_or(table["blur"].value_or(table["bloom"].value_or(s.bloom_effect)));
    s.show_fps = table["show_fps"].value_or(s.show_fps);
    s.scorpion_unfiltered = table["enhancements"]["scorpion_unfiltered"].value_or(s.scorpion_unfiltered);
    s.restore_dialogue = table["enhancements"]["restore_dialogue"].value_or(s.restore_dialogue);
    s.cinematic_bars = table["enhancements"]["cinematic_bars"].value_or(s.cinematic_bars);
    s.improved_difficulty = table["enhancements"]["improved_difficulty"].value_or(s.improved_difficulty);
    s.cheats_enabled = table["enhancements"]["cheats_enabled"].value_or(s.cheats_enabled);
    s.cheat_menu_key = table["enhancements"]["cheat_menu_key"].value_or(s.cheat_menu_key);
    s.statue_full_heal = table["enhancements"]["statue_full_heal"].value_or(s.statue_full_heal);
    s.cheat_menu_x = std::clamp(table["enhancements"]["cheat_menu_x"].value_or(s.cheat_menu_x), 0, 100);
    s.cheat_menu_y = std::clamp(table["enhancements"]["cheat_menu_y"].value_or(s.cheat_menu_y), 0, 100);
    const auto g = table["game"];
    s.original_override = g["override_original"].value_or(s.original_override);
    s.camera_reverse = g["camera_reverse"].value_or(s.camera_reverse);
    s.music_volume = g["music_volume"].value_or(s.music_volume);
    s.effects_volume = g["effects_volume"].value_or(s.effects_volume);
    s.speech_volume = g["speech_volume"].value_or(s.speech_volume);
    s.audio_output = g["audio_output"].value_or(s.audio_output);
    s.brightness = g["brightness"].value_or(s.brightness);
    s.versus_rounds = g["versus_rounds"].value_or(s.versus_rounds);
    s.versus_time = g["versus_time"].value_or(s.versus_time);
    const auto c = table["controller"];
    s.deadzone = c["deadzone"].value_or(s.deadzone);
    s.rumble = c["rumble"].value_or(s.rumble);
    for (size_t i = 0; i < s.bindings.size(); ++i) s.bindings[i] = c["bindings"][binding_keys[i]].value_or(s.bindings[i]);
    for(size_t i=0;i<s.keyboard.keys.size();++i)for(unsigned j=0;j<2;++j)
        s.keyboard.keys[i][j]=table["keyboard"]["bindings"][binding_keys[i]][j].value_or(s.keyboard.keys[i][j]);
    const auto m = table["mouse"];
    s.mouse.sensitivity = m["sensitivity"].value_or(s.mouse.sensitivity);
    s.mouse.invert_y = m["invert_y"].value_or(s.mouse.invert_y);
    s.mouse.invert_x = m["invert_x"].value_or(s.mouse.invert_x);
    s.mouse.motion = m["motion"].value_or(s.mouse.motion);
    for (size_t i = 0; i < s.mouse.bindings.size(); ++i)
        s.mouse.bindings[i] = m["bindings"][mouse::button_keys[i]].value_or(s.mouse.bindings[i]);
    return s;
}
toml::table Merge(toml::table table, const Settings& s) {
#ifdef _WIN32
    table.insert_or_assign("renderer",s.renderer);
#else
    table.erase("renderer");
#endif
    table.insert_or_assign("voice_language",s.voice_language);
    table.insert_or_assign("language", s.language); table.insert_or_assign("display_mode", s.display_mode);
    table.insert_or_assign("aspect_ratio",s.aspect_ratio);
    table.insert_or_assign("width", s.width); table.insert_or_assign("height", s.height);
    table.insert_or_assign("render_percent", s.render_percent); table.insert_or_assign("fps_cap", s.fps_cap);
    table.insert_or_assign("anisotropic_filtering", s.aniso); table.insert_or_assign("master_volume", s.master_volume);
    table.insert_or_assign("vsync", s.vsync); table.erase("bloom"); table.erase("blur");
    table.insert_or_assign("bloom_effect", s.bloom_effect);
    table.insert_or_assign("show_fps", s.show_fps);
    auto enhancements = table["enhancements"].as_table() ? *table["enhancements"].as_table() : toml::table{};
    enhancements.insert_or_assign("scorpion_unfiltered", s.scorpion_unfiltered);
    enhancements.insert_or_assign("restore_dialogue", s.restore_dialogue);
    enhancements.insert_or_assign("cinematic_bars", s.cinematic_bars);
    enhancements.insert_or_assign("improved_difficulty", s.improved_difficulty);
    enhancements.erase("vertical_shadows");
    enhancements.insert_or_assign("cheats_enabled", s.cheats_enabled);
    enhancements.insert_or_assign("cheat_menu_key", s.cheat_menu_key);
    enhancements.insert_or_assign("statue_full_heal", s.statue_full_heal);
    enhancements.insert_or_assign("cheat_menu_x", std::clamp(s.cheat_menu_x, 0, 100));
    enhancements.insert_or_assign("cheat_menu_y", std::clamp(s.cheat_menu_y, 0, 100));
    table.insert_or_assign("enhancements", enhancements);
    toml::table c, b;
    for (size_t i = 0; i < s.bindings.size(); ++i) b.insert_or_assign(binding_keys[i], s.bindings[i]);
    c.insert_or_assign("deadzone", s.deadzone); c.insert_or_assign("rumble", s.rumble);
    c.insert_or_assign("bindings", b);
    table.insert_or_assign("controller", c);
    toml::table kb;
    for(size_t i=0;i<s.keyboard.keys.size();++i)
        kb.insert_or_assign(binding_keys[i],toml::array{s.keyboard.keys[i][0],s.keyboard.keys[i][1]});
    table.insert_or_assign("keyboard",toml::table{{"bindings",kb}});
    toml::table m, mb;
    for (size_t i = 0; i < s.mouse.bindings.size(); ++i) mb.insert_or_assign(mouse::button_keys[i], s.mouse.bindings[i]);
    m.insert_or_assign("sensitivity", s.mouse.sensitivity); m.insert_or_assign("invert_y", s.mouse.invert_y);
    m.insert_or_assign("invert_x",s.mouse.invert_x); m.insert_or_assign("motion",s.mouse.motion);
    m.insert_or_assign("bindings", mb); table.insert_or_assign("mouse", m);
    auto g = table["game"].as_table() ? *table["game"].as_table() : toml::table{};
    g.insert_or_assign("override_original", s.original_override); g.insert_or_assign("camera_reverse", s.camera_reverse);
    g.insert_or_assign("music_volume", s.music_volume); g.insert_or_assign("effects_volume", s.effects_volume);
    g.insert_or_assign("speech_volume", s.speech_volume); g.insert_or_assign("brightness", s.brightness);
    g.insert_or_assign("audio_output", s.audio_output);
    g.insert_or_assign("versus_rounds", s.versus_rounds); g.insert_or_assign("versus_time", s.versus_time);
    table.insert_or_assign("game", g);
    return table;
}
bool Validate(const Settings& s, std::string& error) {
    if(std::find(voice_languages.begin(),voice_languages.end(),s.voice_language)==voice_languages.end())error="Select a valid voice language.";
    else if (!mods::ValidLanguageCode(s.language)) error = "Select a valid game language.";
    else if(!aspect::Valid(s.aspect_ratio)) error="Select a supported aspect ratio.";
    else if (s.display_mode != "windowed" && s.display_mode != "borderless" && s.display_mode != "fullscreen") error = "Select a display mode.";
    else if ((s.width == 0) != (s.height == 0) || s.width < 0 || s.height < 0 || s.width > 16384 || s.height > 16384 ||
             (s.width && (s.width < 320 || s.height < 240))) error = "Set both resolution fields to zero, or select a resolution from 320 x 240 to 16384 x 16384.";
    else if (s.render_percent < 25 || s.render_percent > 400) error = "Render scale must be from 25 to 400 percent.";
    else if (!frames::SupportedRate(s.fps_cap)) error = "Select 60, 120, 144, 165 or 240 FPS.";
    else if (s.aniso != 0 && s.aniso != 2 && s.aniso != 4 && s.aniso != 8 && s.aniso != 16) error = "Select an anisotropic filtering level.";
    else if (s.master_volume < 0 || s.master_volume > 100) error = "Master volume must be from 0 to 100 percent.";
    else if (s.mouse.motion!="right_stick" && s.mouse.motion!="left_stick" && s.mouse.motion!="none") error="Select a mouse movement target.";
    else if (std::any_of(s.keyboard.keys.begin(),s.keyboard.keys.end(),[](const auto& keys){return !bindings::ValidKey(keys[0]) || !bindings::ValidKey(keys[1]);})) error="Choose a supported key. F9 and F10 are reserved.";
    else if (s.mouse.sensitivity < 1 || s.mouse.sensitivity > 300) error = "Mouse sensitivity must be from 1 to 300 percent.";
    else if (std::any_of(s.mouse.bindings.begin(), s.mouse.bindings.end(), [](const auto& action) {
        return std::find(mouse::actions.begin(), mouse::actions.end(), action) == mouse::actions.end();
    })) error = "Select an action for each mouse button.";
    else if (s.music_volume < 0 || s.music_volume > 100 || s.effects_volume < 0 || s.effects_volume > 100 ||
             s.speech_volume < 0 || s.speech_volume > 100) error = "Game sound levels must be from 0 to 100 percent.";
    else if (s.audio_output < 0 || s.audio_output > 2) error = "Audio Output must be Mono, Stereo or Surround.";
    else if (s.brightness < 0 || s.brightness > 100) error = "Brightness adjustment must be from 0 to 100 percent.";
    else if (s.versus_rounds != 1 && s.versus_rounds != 3) error = "Versus rounds must be 1 or 3.";
    else if (s.versus_time != 0 && s.versus_time != 120 && s.versus_time != 180) error = "Versus match time must be 120 seconds, 180 seconds, or unlimited.";
    else if (!std::isfinite(s.deadzone) || s.deadzone < 0 || s.deadzone > .5) error = "Stick dead zone must be from 0 to 50 percent.";
    else if (std::any_of(s.bindings.begin(), s.bindings.end(), [](const auto& binding) { return !bindings::ValidSource(binding); })) error = "Each controller button must have a valid SDL button name.";
    else { error.clear(); return true; }
    return false;
}
bool Write(const std::filesystem::path& path, const toml::table& table, std::string& error) {
    auto temporary = path;
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L".tmp";
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << table << '\n'; stream.flush();
        if (!stream) throw std::runtime_error("Cannot write the settings file.");
        stream.close();
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace the settings file. Your previous settings remain unchanged.");
#else
        // rename replaces atomically; a directory destination fails and stays intact.
        std::error_code replaced;
        std::filesystem::rename(temporary, path, replaced);
        if (replaced) throw std::runtime_error("Cannot replace the settings file. Your previous settings remain unchanged.");
#endif
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        std::error_code ignored; std::filesystem::remove(temporary, ignored);
        return false;
    }
}
unsigned GameLanguage(const std::string& language) {
    if (mods::SameLanguage(language,"fr")) return 1;
    if (mods::SameLanguage(language,"de")) return 3;
    if (mods::SameLanguage(language,"es")) return 4;
    if (mods::SameLanguage(language,"it")) return 2;
    return 0;
}
}
