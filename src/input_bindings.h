#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace openshao::bindings {
inline constexpr std::array<const char*,24> keys{"a","b","x","y","black","white","start","back",
    "left_stick","right_stick","dpad_up","dpad_down","dpad_left","dpad_right","left_trigger","right_trigger",
    "move_up","move_down","move_left","move_right","camera_up","camera_down","camera_left","camera_right"};
inline constexpr std::array<const char*,24> labels{"A / Accept","B / Cancel","X","Y",
    "Black / RB / R1","White / LB / L1","Start / Pause","Back / Select","Left stick click","Right stick click",
    "D-pad up","D-pad down","D-pad left","D-pad right","Left trigger / LT / L2","Right trigger / RT / R2",
    "Move up","Move down","Move left","Move right","Camera up","Camera down","Camera left","Camera right"};
inline const std::array<std::string,24> controller_defaults{"a","b","x","y","rightshoulder","leftshoulder","start","back",
    "leftstick","rightstick","dpup","dpdown","dpleft","dpright","lefttrigger","righttrigger",
    "lefty-","lefty+","leftx-","leftx+","righty-","righty+","rightx-","rightx+"};
struct Keyboard {
    std::array<std::array<int,2>,24> keys{{{0x20,0},{0x1B,0x11},{'J',0},{'K',0},{'C',0},{'Q',0},{0x0D,0},{0x08,0},
        {0,0},{0,0},{0x26,0},{0x28,0},{0x25,0},{0x27,0},{0x10,0},{'E',0},
        {'W',0},{'S',0},{'A',0},{'D',0},{0,0},{0,0},{0,0},{0,0}}};
    bool operator==(const Keyboard&) const = default;
};
inline bool ValidKey(int key) {
    // F9 and F10 remain available for the overlay and pointer release.
    return key==0 || (key>=8 && key<=254 && key!=0x78 && key!=0x79 && key!=0x5B && key!=0x5C);
}
inline std::string CanonicalSource(std::string source) {
    if(source=="south" || source=="cross") return "a";
    if(source=="east" || source=="circle") return "b";
    if(source=="west" || source=="square") return "x";
    if(source=="north" || source=="triangle") return "y";
    // SDL 3.4 names paddles paddle1..4; older mappings use the directional names.
    if(source=="rightpaddle1")return "paddle1";
    if(source=="leftpaddle1")return "paddle2";
    if(source=="rightpaddle2")return "paddle3";
    if(source=="leftpaddle2")return "paddle4";
    return source;
}
inline constexpr std::array<const char*,37> sources{"none","a","b","x","y","back","guide","start","leftstick","rightstick",
    "leftshoulder","rightshoulder","dpup","dpdown","dpleft","dpright","misc1","paddle1","paddle2",
    "paddle3","paddle4","touchpad","misc2","misc3","misc4","misc5","misc6",
    "lefttrigger","righttrigger","leftx-","leftx+","lefty-","lefty+","rightx-","rightx+","righty-","righty+"};
inline bool ValidSource(const std::string& source) {
    const auto normalized=CanonicalSource(source);
    return std::find(sources.begin(),sources.end(),normalized)!=sources.end();
}
inline const char* SourceLabel(std::string_view source) {
    if(source=="none")return "Unassigned";
    if(source=="a")return "A / Cross";if(source=="b")return "B / Circle";
    if(source=="x")return "X / Square";if(source=="y")return "Y / Triangle";
    if(source=="leftshoulder")return "LB / L1";if(source=="rightshoulder")return "RB / R1";
    if(source=="lefttrigger")return "LT / L2";if(source=="righttrigger")return "RT / R2";
    if(source=="start")return "Start / Options";if(source=="back")return "Back / Select / Share";
    if(source=="leftstick")return "Left stick click";if(source=="rightstick")return "Right stick click";
    if(source=="paddle1" || source=="rightpaddle1")return "Paddle 1";
    if(source=="paddle2" || source=="leftpaddle1")return "Paddle 2";
    if(source=="paddle3" || source=="rightpaddle2")return "Paddle 3";
    if(source=="paddle4" || source=="leftpaddle2")return "Paddle 4";
    if(source=="dpup")return "D-pad up";if(source=="dpdown")return "D-pad down";
    if(source=="dpleft")return "D-pad left";if(source=="dpright")return "D-pad right";
    if(source=="leftx-")return "Left stick left";if(source=="leftx+")return "Left stick right";
    if(source=="lefty-")return "Left stick up";if(source=="lefty+")return "Left stick down";
    if(source=="rightx-")return "Right stick left";if(source=="rightx+")return "Right stick right";
    if(source=="righty-")return "Right stick up";if(source=="righty+")return "Right stick down";
    return nullptr;
}
inline std::string KeyLabel(int key) {
    if(!key)return "Unassigned";
    if((key>='A' && key<='Z') || (key>='0' && key<='9'))return std::string(1,char(key));
    if(key>=0x70 && key<=0x87)return "F"+std::to_string(key-0x6F);
    if(key>=0x60 && key<=0x69)return "Numpad "+std::to_string(key-0x60);
    switch(key) {
    case 8:return "Backspace";case 9:return "Tab";case 13:return "Enter";case 16:return "Shift";
    case 17:return "Ctrl";case 18:return "Alt";case 19:return "Pause";case 20:return "Caps Lock";
    case 27:return "Escape";case 32:return "Space";case 33:return "Page Up";case 34:return "Page Down";
    case 35:return "End";case 36:return "Home";case 37:return "Left arrow";case 38:return "Up arrow";
    case 39:return "Right arrow";case 40:return "Down arrow";case 45:return "Insert";case 46:return "Delete";
    case 160:return "Left Shift";case 161:return "Right Shift";case 162:return "Left Ctrl";case 163:return "Right Ctrl";
    case 164:return "Left Alt";case 165:return "Right Alt";case 106:return "Numpad *";case 107:return "Numpad +";
    case 109:return "Numpad -";case 110:return "Numpad .";case 111:return "Numpad /";
    case 186:return ";";case 187:return "=";case 188:return ",";case 189:return "-";case 190:return ".";
    case 191:return "/";case 192:return "`";case 219:return "[";case 220:return "\\";case 221:return "]";case 222:return "'";
    default:return "Key "+std::to_string(key);
    }
}
using Values=std::array<float,24>;
template<class Pad> void Apply(Pad& pad,const Values& values) {
    constexpr unsigned analog[]={0,1,2,3,4,5,0,0,0,0,0,0,0,0,6,7};
    for(unsigned i=0;i<16;++i) {
        const auto value=std::clamp(values[i],0.f,1.f);
        if(i<6 || i>=14)pad.analog[analog[i]]=std::max(pad.analog[analog[i]],uint8_t(std::lround(value*255)));
        else if(value>=.5f) {
            constexpr unsigned bits[]={4,5,6,7,0,1,2,3};
            pad.buttons|=uint16_t(1u<<bits[i-6]);
        }
    }
    auto axis=[&](unsigned neg,unsigned pos,int16_t& destination) {
        if(values[neg]>0 || values[pos]>0)
            destination=int16_t(std::clamp(std::lround((values[pos]-values[neg])*32768),-32768l,32767l));
    };
    axis(18,19,pad.lx);axis(17,16,pad.ly);axis(22,23,pad.rx);axis(21,20,pad.ry);
}
inline bool KeyOverlap(int a,int b) {
    if(!a || !b)return false;
    if(a==b)return true;
    for(int generic:{16,17,18}) {
        const int left=generic==16?160:generic==17?162:164;
        if((a==generic && (b==left || b==left+1)) || (b==generic && (a==left || a==left+1)))return true;
    }
    return false;
}
inline std::string KeyboardConflicts(const Keyboard& keyboard,unsigned row,int slot,int key) {
    if(!key)return {};
    std::string result;
    for(unsigned i=0;i<keys.size();++i)for(int j=0;j<2;++j)
        if((i!=row || j!=slot) && KeyOverlap(keyboard.keys[i][j],key)) {
            if(!result.empty())result+=", ";result+=labels[i];
        }
    return result;
}
}
