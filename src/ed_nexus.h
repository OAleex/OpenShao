#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::ed_nexus {
void FrontendInput(X86Context& ctx, uint8_t* base);
bool Unlocked();
bool GameplayPaused();
bool KontentUnlocked();
float PlayerDamage();  // debug damage multiplier for the players' hits, 1 when off
// Debug Player 2: the live game mode is Ko-op (state + 0x88 == 1).
bool Player2Active();
// Keeps a debug Player 2 out of saves: the slot is written with the campaign's own mode.
void Player2BeforeSave();
void Player2AfterSave();
// A loaded save or a new campaign drops the debug override.
void ResetPlayer2();
}
