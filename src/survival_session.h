#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::survival {
inline constexpr uint32_t RulesLabel = 0xFFFFFD20;
void AnnounceMode(X86Context& ctx);
void AnnounceRules(X86Context& ctx);
void StopAnnouncement(X86Context& ctx);
void VoicePlayed(uint32_t sound, uint32_t handle);
void UpdateVoice(X86Context& ctx);
bool ActiveSession();
// Returns true when the Foundry portal requested Survival 1 (the load is skipped).
bool InterceptFoundryPortal(X86Context& ctx, uint32_t destination);
uint8_t RulesBackdropOpacity();
void DrawRulesBackdrop(X86Context& ctx);
bool HoldRulesWorld();
bool DeferMusic(uint32_t track);
inline uint16_t PauseButtons(uint16_t buttons,bool active) {
    constexpr uint16_t Start=0x10,Select=0x20;
    return active && (buttons&Start)?uint16_t((buttons&~Start)|Select):buttons;
}
void ResetSession();
void RoomReady(X86Context& ctx);
void RoomLoading();
bool FilterLevelMessage(uint32_t info);
void TriggerActivated(X86Context& ctx,uint32_t target,bool enable);
}
