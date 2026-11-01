#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace openshao::pause_recomp {
enum class Item : uint32_t {
    OpenSettings, OpenExit, ShowFps, Vsync, Bloom, Return, Warning, No, Yes, SaveGame, Count
};
enum class Page : uint8_t {
    Categories, Controls, Graphics, Display, Audio,
    Controller, Keyboard, KeyboardAlternate, Mouse, DisplayConfirm, Cheats
};
inline constexpr unsigned MaxRows=7;
std::string Text(Item item);
// Game-font encoded label from the shared localized JSON catalog.
std::string Localized(std::string_view id);
bool Toggle(Item item);
bool Exit();
void Open();
void Close();
Page CurrentPage();
unsigned RowCount();
std::string RowText(unsigned row);
// true means return to the native Select pause root; otherwise draw the new page.
bool Choose(unsigned row);
bool Back();
// Poll capture/timeouts from the guest UI frame, not the OS input thread.
bool Update();
bool Capturing();
}
