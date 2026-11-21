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
// Clears only Survival's Modes/Players overlay after session initialization.
void Started();
bool MainMenu(X86Context& ctx);
bool MedallionMenu();
// Also true while Main animates to and from the fighter screen.
bool OverlayShown();
float OrnamentExtension();
bool Transitioning();
bool MenuInput(X86Context& ctx, uint8_t* base, bool keyboard_enter=false);
// Called on Main's first caption; opacity is Main's page fade.
void DrawMenu(X86Context& ctx, unsigned opacity = 255);
bool HideMainCaption(std::string_view id);
}
