#pragma once
#include <cstdint>
struct ImFont;

namespace openshao::cheat {
bool Enabled();
void SetEnabled(bool enabled);
bool Visible();
void Toggle();
bool Message(uint32_t message, uintptr_t key, intptr_t flags);
bool HandleKey(unsigned virtual_key, bool down);
void Draw(double fps);
void SetHotkey(int virtual_key);
int Hotkey();

// The menu falls back to the default ImGui font when these are not set.
void SetFonts(ImFont* regular, ImFont* bold);
// Bakes the menu typeface at several sizes. Call once the ImGui context exists and before
// the renderer builds the font atlas; does nothing afterwards.
void InitFonts();
// Menu position as a percentage (0-100) of the free screen space.
void SetMenuPosition(int x_percent, int y_percent);

void SetStatueFullHeal(bool enabled);
bool StatueFullHeal();
// Called from guest hooks on the game thread.
void TrackPlayerHealth();
// Ko-op paths. Called from guest hooks on the game thread with the stack pointer at the hooked call.
struct CoopEvent { bool pending = false; uint32_t id = 0, value = 0; };
bool CoopForceTwoPlayer(uint32_t esp);
bool CoopForceOnePlayer(uint32_t esp);
bool CoopForceStone(uint32_t esp);
bool CoopForceStonePower(uint32_t esp, uint32_t condition);
bool CoopForceEventFlag(uint32_t condition);
CoopEvent CoopEventBefore(uint32_t esp);
void CoopEventAfter(const CoopEvent& event);
// Starts a one second burst of the Infinite Health routine when the save statue is used.
void OnStatueUsed();

struct CheatState {
    bool infinite_health = false;
    bool infinite_fatality_p1 = false;
    bool infinite_fatality_p2 = false;
    bool infinite_xp_p1 = false;
    bool infinite_xp_p2 = false;
    bool infinite_energy_p1 = false;
    bool infinite_energy_p2 = false;
    bool coop_paths = false;
    bool unlock_all = false;
};
const CheatState& State();
}
