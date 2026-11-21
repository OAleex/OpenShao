#pragma once
#include <cstdint>
#include <string>
namespace openshao::performance {
bool Visible();
void SetVisible(bool visible);
// Called after F9 changes the overlay, so the choice can be saved like the menu option.
void OnToggle(void (*changed)(bool visible));
bool Message(uint32_t message, uintptr_t key, intptr_t flags);
std::string Format(double fps);
void Draw(double fps);
}
