#pragma once
#include <bitset>
#include <cstdint>
#include <string_view>
struct X86Context;
namespace openshao::menu {
inline constexpr unsigned MainGameModeRow=9;
// Caption opacity of the focused and the other rows on Main and its overlays.
inline constexpr unsigned MainSelectedAlpha=254,MainIdleAlpha=76;
// Main clips: 0 intro, 1..6 focus, leave/return pairs 7/8 Single Player .. 17/18 Kontent; the lake backdrop shows in all but leave clips.
inline constexpr bool MainBackdropClip(unsigned clip){return clip<=6 || (clip>=8 && clip<=18 && clip%2==0);}
inline constexpr float MainMenuRise=36;
inline constexpr float MainOrnamentOffset=-2;
inline constexpr float MainOrnamentExtension=40;
inline constexpr unsigned MainVisualRow(unsigned row){return row==MainGameModeRow?3:row>=3?row+1:row;}
inline constexpr float MainListAdjustment(unsigned visual_row){return 14.f-6.f*visual_row;}
enum class Button {Accept,Back,Adjust,Preview};
void NativeButton(X86Context& ctx,int x,int y,Button button);
void MainControllerNotice(X86Context& ctx,uint32_t text,unsigned alpha,float scale);
// Replaces one of Main's first rows for this scene, keeping its native motion and fade.
void OverrideMainRow(unsigned row,std::string_view text,unsigned max_alpha=MainSelectedAlpha);
unsigned MainHighlight(uint32_t state,unsigned selection);
void MainExtraLabel(X86Context& ctx,uint32_t first,uint32_t end,bool selected,bool anchor_selected,unsigned row=6);
void MainRowAnchor(uint32_t first,uint32_t end);
void DimMenuLabel(uint32_t first,uint32_t end,bool selected);
void BeginFrame();
std::bitset<512> FrameArrows();
void SetFrameArrows(const std::bitset<512>& arrows);
bool ViewportArrow(uint32_t index);
void BeginScene();
void EndScene(X86Context& ctx);
bool ReplaceGraphic(X86Context& ctx,uint32_t crc,uint32_t first,uint32_t end);
float NativeLabel(X86Context& ctx,float x,float y,std::string_view text,float scale,unsigned alpha=254,bool title=false,bool compact=false,float available=550);
void TitleWidth(float right);
}
