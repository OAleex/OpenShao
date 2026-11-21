#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace openshao::pause_recomp {
enum class Item : uint32_t {
    OpenSettings, OpenExit, ShowFps, Vsync, Bloom, Return, Warning, No, Yes, OpenEnhancements, OpenPhotoMode, SaveGame, Count
};
enum class Page : uint8_t {
    Categories, Controls, Graphics, Display, Audio,
    Controller, Keyboard, KeyboardAlternate, Mouse, DisplayConfirm, EnhancedCamera, Enhancements, Interface, Cheats
};
inline constexpr unsigned MaxRows=6;
std::string Text(Item item);
// Game-font encoded label from the shared localized JSON catalog.
std::string Localized(std::string_view id);
bool Toggle(Item item);
bool Exit();
void Open(Page initial=Page::Categories);
void Close();
Page CurrentPage();
unsigned RowCount();
std::string RowText(unsigned row);
std::string PageIndicator();
bool CanAdjust(unsigned row);
bool CanChoose(unsigned row);
void Choose(unsigned row);
bool Adjust(unsigned row,int direction);
// true means return to the native Select pause root.
bool Back();
// Poll capture/timeouts from the guest UI frame, not the OS input thread.
bool Update();
bool Capturing();
bool CapturingController();
}
