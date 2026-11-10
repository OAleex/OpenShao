#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "core/input.h"

// Recording, replay and state hashes: the deterministic base of online play.
// While a session is active, every input::Poll returns the fixed sample of the current frame.
namespace openshao::netplay::session {
struct Settings {
    std::filesystem::path record, replay, hash_out;
    unsigned hash_every = 0;  // 0 keeps the recording's cadence during replay
};
// Exit code when the replay conditions or the recording do not match.
inline constexpr int kMismatch = 60;
void Configure(const Settings& settings);
// The live device poll that recording samples; set once by the controller layer.
void SetLiveSource(input::Pad (*live)(int));
bool Requested();
bool Active();
bool Replaying();
// Opens the files and publishes frame 0. Called once the save folder is known, before boot.
bool Start(const std::filesystem::path& save_directory, std::string& error);
// The game-facing poll: the frame's sample while a session runs, otherwise the live device.
input::Pad Poll(int port);
// End of the guest's Swap: publishes the next frame's sample and hashes on schedule.
void FrameBoundary();
// Completes the recording and report; safe to call more than once.
void Finish();
}  // namespace openshao::netplay::session
