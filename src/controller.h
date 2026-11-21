#pragma once
#include <cstdint>
namespace openshao::launcher {struct Settings;}
namespace openshao::controller {
void Update(); // window/main thread only; game threads read cached snapshots
void Shutdown();
void SetFocused(bool focused);
bool HasRumble(int port);
bool RumbleEnabled();
bool CameraRecenterDown();
int KeyboardPort();
void SetMultiplayer(bool enabled); // game callbacks; no SDL calls on this thread
void Window(void* hwnd);
bool Message(void*,uint32_t,uintptr_t,intptr_t,intptr_t*);
void Load(const void* table);
void Configure(const launcher::Settings& settings);
void Save(void* table);
}
