#pragma once
#include <cstdint>
#include "input_bindings.h"
namespace input { struct Pad; }
namespace openshao::keyboard {
bool Message(void*, uint32_t, uintptr_t, intptr_t, intptr_t*);
// Platform-neutral key state, indexed by Windows virtual-key code (Linux maps SDL keys to these).
void Key(unsigned virtual_key, bool down);
void ReleaseAll();
void Configure(const bindings::Keyboard& settings);
bool Mapped(unsigned virtual_key);
bool CameraRecenterDown();
void Poll(int port, input::Pad& pad);
}
