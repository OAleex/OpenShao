#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::fighter_selection {
enum class Purpose { Campaign, Survival };
bool CampaignInput(X86Context&, uint8_t* base);
void Begin(X86Context&, uint8_t* base, Purpose, uint32_t player_mode,
           uint32_t return_row);
bool CharacterSelection();
bool SetupPopup();
// Difficulty popup open, or confirmed and closing: the roster is still on screen behind it.
bool RosterBackdrop();
// The game start callbacks: hand them the real Solo/Ko-op mode (the roster keeps Versus until then).
void ApplyStartMode();
uint32_t PlayerMode();
uint32_t MainClip(uint32_t clip);
bool StartingFor(Purpose);
bool PopupInput(X86Context&, uint8_t* base);
bool HintActive();
bool LockedPortrait(uint32_t slot);
bool LockedInput(X86Context&);
bool ReturnToMain(X86Context&, uint8_t* base);
void FinishStartup();
}
