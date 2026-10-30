#pragma once
#include <bitset>
#include <cstdint>
#include <string_view>
struct X86Context;
namespace openshao::menu {
enum class Button {Accept,Back,Adjust,Preview};
void NativeButton(X86Context& ctx,int x,int y,Button button);
void MainControllerNotice(X86Context& ctx,uint32_t text,unsigned alpha,float scale);
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
