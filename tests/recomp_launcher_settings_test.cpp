#include "platform_compat.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include "launcher_settings.h"
#include "bloom_effect_compat.h"

int main() {
    using namespace openshao::launcher;
    int failures = 0;
    const auto check = [&](bool good, const char* message) { if (!good) { ++failures; std::cerr << message << '\n'; } };
#ifdef _WIN32
    check(Merge({},Read({}))["renderer"].value_or(std::string{})=="d3d12",
          "New and legacy Windows settings must start with D3D12");
    check(Merge({},Read(toml::parse("renderer='vulkan'\n")))["renderer"].value_or(std::string{})=="vulkan",
          "A Vulkan launcher selection must survive a complete settings load and save");
    check(Merge({},Read(toml::parse("renderer='unknown'\n")))["renderer"].value_or(std::string{})=="d3d12",
          "An unsupported renderer preference must recover to the Windows default");
#else
    check(!Merge(toml::parse("renderer='d3d12'\n"),Read(toml::parse("renderer='d3d12'\n"))).contains("renderer"),
          "Linux must remove a copied renderer preference and retain its fixed Vulkan renderer");
#endif
    auto table = toml::parse("custom = 42\n[controller]\ndeadzone = 0.23\ncustom = true\n[controller.bindings]\na = 'cross'\ncustom = 'keep'\n");
    auto config = Read(table);
    check(!Merge({}, Read({}))["enhancements"]["scorpion_unfiltered"].value_or(true),
          "New and legacy configurations must preserve the original Scorpion voice distribution by default");
    const auto enhanced = Read(toml::parse("[enhancements]\nscorpion_unfiltered = true\n"));
    check(!Merge({},Read({}))["enhancements"]["cinematic_bars"].value_or(true),
          "Cinematic bars must be an optional enhancement, disabled for existing and new settings");
    for(bool enabled:{false,true})
        check(Merge({},Read(toml::table{{"enhancements",toml::table{{"cinematic_bars",enabled}}}}))["enhancements"]["cinematic_bars"].value_or(!enabled)==enabled,
              "Both cinematic bar choices must survive a complete configuration round trip");
    const auto old_shadow_setting=toml::parse("[enhancements]\nvertical_shadows = true\nscorpion_unfiltered = true\n");
    const auto without_shadow_setting=Merge(old_shadow_setting,Read(old_shadow_setting));
    check(!without_shadow_setting["enhancements"].as_table()->contains("vertical_shadows") &&
          without_shadow_setting["enhancements"]["scorpion_unfiltered"].value_or(false),
          "Saving preferences must remove the retired Vertical Shadows option while keeping other enhancements");
    check(Read({}).improved_difficulty && Merge({},Read({}))["enhancements"]["improved_difficulty"].value_or(false) &&
          !Merge({},Read(toml::parse("[enhancements]\nimproved_difficulty = false\n")))["enhancements"]["improved_difficulty"].value_or(true),
          "Improved difficulty must be enabled by default and survive a configuration round trip");
    check(Merge({}, enhanced)["enhancements"]["scorpion_unfiltered"].value_or(false),
          "The Scorpion enhancement must survive configuration load and save");
    check(Read({}).restore_dialogue && Merge({}, Read({}))["enhancements"]["restore_dialogue"].value_or(false),
          "New and legacy configurations must restore dropped script dialogue by default");
    check(!Merge({}, Read(toml::parse("[enhancements]\nrestore_dialogue = false\n")))["enhancements"]["restore_dialogue"].value_or(true),
          "Choosing the original dialogue behavior must survive configuration load and save");
    check(Read({}).brightness == 50, "A new configuration must default to 50 percent brightness");
    for (const int brightness : {0, 23, 50, 100}) {
        const auto saved_brightness = Read(toml::table{{"game", toml::table{{"brightness", brightness}}}});
        check(saved_brightness.brightness == brightness && Read(Merge({}, saved_brightness)).brightness == brightness,
              "Existing brightness preferences, including zero, must survive loading and saving");
    }
    check(!config.bloom_effect, "Settings without a Bloom preference must load with the effect disabled");
    const auto new_settings = Read({});
    check(!new_settings.bloom_effect && !Read(Merge({}, new_settings)).bloom_effect,
          "A new configuration must keep Bloom disabled after loading and saving");
    const auto enabled_effect = Read(toml::parse("bloom_effect = true\n"));
    check(enabled_effect.bloom_effect && Read(Merge({}, enabled_effect)).bloom_effect,
          "An explicitly enabled Bloom preference must survive loading and saving");
    const auto renamed_effect = Read(toml::parse("bloom_effect = false\n"));
    check(!renamed_effect.bloom_effect, "The canonical Bloom Effect preference must load a disabled effect");
    const auto canonical_effect = Merge(toml::parse("blur = true\nbloom = true\ncustom = 42\n"), renamed_effect);
    check(canonical_effect["bloom_effect"].value_or(true) == false &&
          !canonical_effect.contains("blur") && !canonical_effect.contains("bloom") &&
          canonical_effect["custom"].value_or(0) == 42,
          "Saving Bloom Effect must replace both legacy keys without changing unrelated preferences");
    check(!Read(toml::parse("bloom_effect = false\nblur = true\nbloom = true\n")).bloom_effect,
          "The canonical Bloom Effect preference must take priority over both legacy keys");
    config.language = "fr";
    const auto merged = Merge(table, config);
    check(merged["custom"].value_or(0) == 42, "Saving must preserve unrelated application settings");
    check(merged["controller"].as_table()->size() == 3 && merged["controller"]["bindings"].as_table()->size() == 24,
          "Saving must keep only supported controller settings and button mappings");
    check(Read(merged) == config && std::abs(config.deadzone - .23) < .0001, "The settings must survive a complete round trip");
    for(const char* mode:{"original","auto","4:3","16:9","16:10","21:9","32:9"}) {
        auto aspect=Read(toml::table{{"aspect_ratio",mode}});
        check(Merge({},aspect)["aspect_ratio"].value_or(std::string{})==mode,
              "Every supported aspect selection must survive launcher settings persistence");
    }
    const auto voices=Read(toml::parse("language='es'\nvoice_language='en'\n"));
    check(Merge({},voices)["voice_language"].value_or(std::string{})=="en" && voices.language=="es",
          "Spanish text and English voices must survive independent settings persistence");
    check(Merge({},Read({}))["voice_language"].value_or(std::string{})=="en",
          "Voice settings must always contain an explicit language");
    for(const auto* locale:languages) {
        const auto legacy=Read(toml::table{{"language",locale},{"voice_language","auto"}});
        const std::string expected=locale;
        check(legacy.voice_language==expected && Read(Merge({},legacy))==legacy,
              "An older voice preference must migrate once to an explicit supported language and remain stable when saved");
        auto explicit_voice=legacy;explicit_voice.language="fr";
        check(Read(Merge({},explicit_voice)).voice_language==expected,"Changing text must not alter the explicit migrated voice choice");
        check(Read(toml::table{{"language",locale}}).voice_language==expected,
              "A legacy configuration without a voice field must preserve its text language for voices");
    }
    for (const char* voice : {"de", "es"}) {
        auto independent = Settings{};independent.voice_language=voice;
        std::string error;
        check(Validate(independent,error) && Read(Merge({},independent))==independent,
              "German and Spanish voices must validate and persist independently of text");
    }
    std::string voice_error;
    check(!Validate(Read(toml::parse("voice_language='invalid'\n")),voice_error),"Unknown voice languages must be rejected");
    auto no_bloom_effect = Read(toml::parse("blur = false\n"));
    check(!no_bloom_effect.bloom_effect && !Read(Merge({}, no_bloom_effect)).bloom_effect, "Disabling Bloom Effect must survive a complete round trip");
    const auto from_old_setting = Read(toml::parse("bloom = false\n"));
    check(!from_old_setting.bloom_effect, "An existing disabled bloom setting must migrate to disabled Bloom Effect");
    const auto migrated = Merge(toml::parse("bloom = false\ncustom = 42\n"), from_old_setting);
    check(!Read(migrated).bloom_effect && !migrated.contains("bloom") &&
          migrated["bloom_effect"].value_or(true) == false && !migrated.contains("blur") && migrated["custom"].value_or(0) == 42,
          "Migration must remove the old key and preserve unrelated settings");
    check(Read(toml::parse("blur = true\nbloom = false\n")).bloom_effect,
          "The previous effect preference must take priority over an older setting");
    openshao::BloomEffectDraw bloom_effect{9, 4, 0x104, 640, 480, 160, 120, 1, 1, 0x8006,
                            false, false, true, true};
    check(openshao::IsBloomEffectComposite(bloom_effect), "The captured final bloom effect composite must be recognized");
    bloom_effect.source_is_target = false;
    check(!openshao::IsBloomEffectComposite(bloom_effect), "Ordinary textured draws must not be suppressed");
    bloom_effect.source_is_target = true;
    bloom_effect.blend_destination = 0x303;
    check(!openshao::IsBloomEffectComposite(bloom_effect), "Regular alpha-blended effects must remain visible");
    bloom_effect.blend_destination = 1;
    bloom_effect.source_width = 320;
    check(!openshao::IsBloomEffectComposite(bloom_effect), "Intermediate bloom effect passes must remain visible");
    bloom_effect.source_width = 160;
    bloom_effect.vertex_shader = 0;
    check(!openshao::IsBloomEffectComposite(bloom_effect), "Other full-screen effects must remain visible");
    const auto mouse = Read(toml::parse("[mouse]\nsensitivity = 173\ninvert_y = true\n[mouse.bindings]\nleft = 'right_trigger'\n"));
    check(Read(Merge({}, mouse)) == mouse && mouse.mouse.sensitivity == 173 && mouse.mouse.invert_y &&
          mouse.mouse.bindings[0] == "right_trigger", "Mouse sensitivity, inversion, and button mappings must survive saving");
    const auto remapped = Read(toml::parse("[keyboard.bindings]\na = [90, 88]\n[controller.bindings]\nleft_trigger = 'a'\nmove_left = 'rightx-'\n[mouse]\ninvert_x = true\nmotion = 'left_stick'\n[mouse.bindings]\nwheel_up = 'dpad_up'\n"));
    const auto saved_bindings = Merge({}, remapped);
    check(saved_bindings["keyboard"]["bindings"]["a"][0].value_or(0) == 90 &&
          saved_bindings["keyboard"]["bindings"]["a"][1].value_or(0) == 88,
          "Both keyboard bindings must survive loading and saving");
    check(saved_bindings["controller"]["bindings"]["left_trigger"].value_or(std::string{}) == "a" &&
          saved_bindings["controller"]["bindings"]["move_left"].value_or(std::string{}) == "rightx-",
          "Controller triggers and stick directions must be remappable");
    check(saved_bindings["mouse"]["bindings"]["wheel_up"].value_or(std::string{}) == "dpad_up" &&
          saved_bindings["mouse"]["invert_x"].value_or(false) &&
          saved_bindings["mouse"]["motion"].value_or(std::string{}) == "left_stick",
          "Mouse wheel, motion target and horizontal inversion must survive saving");
    std::string error;
    const auto surround=Read(toml::parse("[game]\naudio_output=2\n"));
    check(Merge({},surround)["game"]["audio_output"].value_or(-1)==2,"The original Audio Output selection must survive saving without a launcher control");
    check(!Validate(Read(toml::parse("[game]\naudio_output=3\n")),error),"The hidden console digital-encoding mode must not be selectable on the host");
    check(!Validate(Read(toml::parse("[controller.bindings]\na='missing_axis'\n")),error),"Unknown controller sources must be rejected before launch");
    check(!Validate(Read(toml::parse("[keyboard.bindings]\na=[120,0]\n")),error),"Reserved overlay keys cannot be assigned to gameplay");
    check(!Validate(Read(toml::parse("[keyboard.bindings]\na=[-1,0]\n")),error),"Invalid keyboard codes must be rejected");
    check(!Validate(Read(toml::parse("[mouse]\nmotion='invalid'\n")),error),"Invalid mouse motion targets must be rejected");
    auto overlap=openshao::bindings::Keyboard{};overlap.keys[0]={163,0};
    check(!openshao::bindings::KeyboardConflicts(overlap,0,0,163).empty(),"A side-specific modifier conflicts with a generic modifier binding");
    check(Merge({},Settings{})["fps_cap"].value_or(0)==60,"New settings must default to 60 FPS");
    for (const int fps : {60,120,144,165,240}) {
        for (const bool vsync : {false,true}) {
            const auto loaded=Read(toml::table{{"fps_cap",fps},{"vsync",vsync}});
            const auto saved=Merge({},loaded);
            check(saved["fps_cap"].value_or(0)==fps && saved["vsync"].value_or(!vsync)==vsync,
                  "Supported frame rates must persist independently of VSync");
            check(Read(saved)==loaded,"A supported frame rate must survive a complete round trip");
            check(Validate(loaded,error),"Each supported frame rate must pass launcher validation");
        }
    }
    for(const int fps:{-1,0,29,30,45,61,121,166,241,1000}) {
        const toml::table legacy{{"fps_cap",fps}};
        const auto loaded=Read(legacy);
        check(loaded==Settings{},"Legacy frame-rate preferences must not change launcher settings");
        check(Merge(legacy,loaded)["fps_cap"].value_or(0)==60,"Saving legacy settings must use 60 FPS");
        auto unsupported=Settings{};unsupported.fps_cap=fps;
        check(!Validate(unsupported,error),"Unsupported frame-rate limits must fail direct validation");
    }
    check(Validate(config, error), "Valid settings must be accepted");
    config.width = 1920; config.height = 0;
    check(!Validate(config, error), "An incomplete resolution must be rejected");
    config.width = 0; config.deadzone = NAN;
    check(!Validate(config, error), "A nonfinite dead zone must be rejected");
    config = Settings{}; config.language = "bad code";
    check(!Validate(config, error), "An unavailable game language must be rejected");
    config.language = "pt-BR";
    check(Validate(config, error) && GameLanguage(config.language) == 0,
          "Brazilian Portuguese must use its language pack over the English game slot");
    config.language="ja";
    check(Validate(config,error)&&GameLanguage(config.language)==0,"New language codes must be saved and use the translated English slot");
    check(!Validate(Read(toml::parse("[game]\nmusic_volume = 101\n")), error), "Original music volume must reject values above 100 percent");
    check(!Validate(Read(toml::parse("[game]\nversus_rounds = 2\n")), error), "Versus rounds must match an original game option");
    check(!Validate(Read(toml::parse("[mouse]\nsensitivity = 0\n")), error), "Mouse sensitivity must reject zero");
    check(!Validate(Read(toml::parse("[mouse.bindings]\nleft = 'missing'\n")), error), "Mouse buttons must reject unknown actions");
    check(GameLanguage("en") == 0 && GameLanguage("fr") == 1 && GameLanguage("de") == 3 &&
          GameLanguage("es") == 4 && GameLanguage("it") == 2, "Language selection must match the original text tables");
    const auto root = std::filesystem::temp_directory_path() / ("openshao-settings-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directory(root);
    const auto file = root / "settings.toml";
    check(Write(file, merged, error) && Read(toml::parse_file(file.string())).language == "fr", "The atomic settings writer must write the selected language");
    std::filesystem::create_directory(root / "blocked");
    std::ofstream(root / "blocked/keep") << "preserve";
    check(!Write(root / "blocked", merged, error) && std::filesystem::exists(root / "blocked/keep"), "A failed replacement must preserve its destination");
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
