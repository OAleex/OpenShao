#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace openshao::menu {
class Catalog {
    std::map<std::string,std::map<std::string,std::string>> languages_;
    std::map<uint32_t,std::string> graphics_;
public:
    static Catalog Parse(std::string_view json);
    static const Catalog& Builtin();
    std::string_view Get(std::string_view locale,std::string_view id) const;
    std::string_view Graphic(uint32_t crc) const;
};
std::string EncodeNative(std::string_view utf8,bool translated_font,size_t limit=95);
std::string EncodeNativeMultiline(std::string_view utf8,bool translated_font,size_t limit=127);
float FitScale(float width,float preferred,float available);
struct TitleLayout {float text_x=55,scale=1.5f,arrow_tip=55;};
TitleLayout LayoutTitle(float measured_width);
}
