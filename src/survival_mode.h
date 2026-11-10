#pragma once
#include <cstdint>
#include <string_view>
struct X86Context;
namespace openshao::survival {
// Host text IDs (outside the retail range), resolved by Label().
inline constexpr uint32_t LabelBase = 0xFFFFFD00;
inline constexpr uint32_t CompleteLabel = LabelBase;
inline constexpr uint32_t PortalNewLabel = LabelBase + 1;
inline constexpr uint32_t PortalPendingLabel = LabelBase + 2;
inline constexpr uint32_t PortalSavedLabel = LabelBase + 3;
void Open(X86Context& ctx, uint8_t* base);
uint32_t Label(uint32_t id);
bool PopupInput(X86Context& ctx, uint8_t* base);
bool Starting();
void Started();
bool CharacterSelection();
uint32_t PlayerMode();
bool MainMenu(X86Context& ctx);
bool SetupPopup();
bool MedallionMenu();
// Also true while Main animates to and from the fighter screen.
bool OverlayShown();
float OrnamentExtension();
bool Transitioning();
bool MenuInput(X86Context& ctx, uint8_t* base, bool keyboard_enter=false);
// Called on Main's first caption; opacity is Main's page fade.
void DrawMenu(X86Context& ctx, unsigned opacity = 255);
bool HideMainCaption(std::string_view id);
// Hides the Versus arena name behind Survival's difficulty popup.
inline bool HideVersusArenaCaption(uint32_t caller, bool setup_popup, uint32_t top, uint32_t parent) {
    constexpr uint32_t Characters = 0x598DF0, Popup = 0x596810;
    return (caller == 0x45F0D || caller == 0x4F97B) && setup_popup &&
        (top == Characters || (top == Popup && parent == Characters));
}
}
