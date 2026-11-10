#pragma once
#include <cstdint>
#include <string>
struct X86Context;
namespace openshao::recomp {
bool MainInput(X86Context& ctx,uint8_t* base);
void RestoreMainSelector();
bool Active();
bool ExtraSelected();
bool CustomScene(uint32_t scene);
unsigned Selection();
unsigned SelectorY();
std::string Title();
void Draw(X86Context& ctx);
bool Graphic(X86Context& ctx,uint32_t scene,uint32_t key,uint32_t first,uint32_t end);
}
