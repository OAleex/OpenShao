#pragma once
#include <SDL3/SDL_scancode.h>
#include "input_bindings.h"
namespace openshao::bindings {
inline unsigned VirtualKey(SDL_Scancode scan) {
    if(scan>=SDL_SCANCODE_A && scan<=SDL_SCANCODE_Z)return 'A'+scan-SDL_SCANCODE_A;
    if(scan>=SDL_SCANCODE_1 && scan<=SDL_SCANCODE_9)return '1'+scan-SDL_SCANCODE_1;
    if(scan>=SDL_SCANCODE_F1 && scan<=SDL_SCANCODE_F12)return 0x70+scan-SDL_SCANCODE_F1;
    if(scan>=SDL_SCANCODE_F13 && scan<=SDL_SCANCODE_F24)return 0x7C+scan-SDL_SCANCODE_F13;
    if(scan>=SDL_SCANCODE_KP_1 && scan<=SDL_SCANCODE_KP_9)return 0x61+scan-SDL_SCANCODE_KP_1;
    switch(scan) {
    case SDL_SCANCODE_0:return '0';case SDL_SCANCODE_KP_0:return 0x60;
    case SDL_SCANCODE_BACKSPACE:return 8;case SDL_SCANCODE_TAB:return 9;
    case SDL_SCANCODE_RETURN:case SDL_SCANCODE_KP_ENTER:return 13;
    case SDL_SCANCODE_ESCAPE:return 27;case SDL_SCANCODE_SPACE:return 32;
    case SDL_SCANCODE_LSHIFT:return 160;case SDL_SCANCODE_RSHIFT:return 161;
    case SDL_SCANCODE_LCTRL:return 162;case SDL_SCANCODE_RCTRL:return 163;
    case SDL_SCANCODE_LALT:return 164;case SDL_SCANCODE_RALT:return 165;
    case SDL_SCANCODE_CAPSLOCK:return 20;case SDL_SCANCODE_PAUSE:return 19;
    case SDL_SCANCODE_PAGEUP:return 33;case SDL_SCANCODE_PAGEDOWN:return 34;case SDL_SCANCODE_END:return 35;
    case SDL_SCANCODE_HOME:return 36;case SDL_SCANCODE_LEFT:return 37;case SDL_SCANCODE_UP:return 38;
    case SDL_SCANCODE_RIGHT:return 39;case SDL_SCANCODE_DOWN:return 40;case SDL_SCANCODE_INSERT:return 45;
    case SDL_SCANCODE_DELETE:return 46;case SDL_SCANCODE_KP_MULTIPLY:return 106;case SDL_SCANCODE_KP_PLUS:return 107;
    case SDL_SCANCODE_KP_MINUS:return 109;case SDL_SCANCODE_KP_PERIOD:return 110;case SDL_SCANCODE_KP_DIVIDE:return 111;
    case SDL_SCANCODE_NUMLOCKCLEAR:return 144;case SDL_SCANCODE_SCROLLLOCK:return 145;
    case SDL_SCANCODE_SEMICOLON:return 186;case SDL_SCANCODE_EQUALS:return 187;case SDL_SCANCODE_COMMA:return 188;
    case SDL_SCANCODE_MINUS:return 189;case SDL_SCANCODE_PERIOD:return 190;case SDL_SCANCODE_SLASH:return 191;
    case SDL_SCANCODE_GRAVE:return KeyBelowEscape;case SDL_SCANCODE_LEFTBRACKET:return 219;case SDL_SCANCODE_BACKSLASH:return 220;
    case SDL_SCANCODE_RIGHTBRACKET:return 221;case SDL_SCANCODE_APOSTROPHE:return 222;case SDL_SCANCODE_NONUSBACKSLASH:return 226;
    default:return 0;
    }
}
}
