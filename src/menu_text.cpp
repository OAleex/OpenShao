#include "menu_text.h"
#include "mods/json.h"
#include "menu_text_data.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace openshao::menu {
namespace {
std::string Lower(std::string_view value) {
    std::string result(value);
    for(auto& c:result)c=char(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
}
Catalog Catalog::Parse(std::string_view text) {
    const auto root=mods::Json::Parse(text);
    if(root.At("version").Number()!=1)throw std::runtime_error("Unsupported menu text catalog version");
    Catalog catalog;
    for(const auto& [locale,labels]:std::get<mods::Json::Object>(root.At("languages").value))
        for(const auto& [id,label]:std::get<mods::Json::Object>(labels.value)) {
            // Survival frames (unlock and completion) use the framed multiline format.
            if(id.starts_with("survival.unlock_") || id=="survival.complete")EncodeNativeMultiline(label.Text(),false,127);
            else EncodeNative(label.Text(),false,id.starts_with("system.")?255:95);
            catalog.languages_[Lower(locale)].emplace(id,label.Text());
        }
    if(!catalog.languages_.contains("en"))throw std::runtime_error("Menu text catalog requires English fallback");
    if(root.Has("graphics"))for(const auto& [id,aliases]:std::get<mods::Json::Object>(root.At("graphics").value))
        for(const auto& alias:aliases.Items()) {
            const auto& hex=alias.Text();size_t parsed=0;
            const auto value=std::stoul(hex,&parsed,16);
            if(hex.size()!=8 || parsed!=8 || value>UINT32_MAX || !catalog.Get("en",id).size() ||
               !catalog.graphics_.emplace(uint32_t(value),id).second)
                throw std::runtime_error("Invalid or duplicate menu graphic identifier");
        }
    return catalog;
}
const Catalog& Catalog::Builtin(){static const auto catalog=Parse(MenuTextData);return catalog;}
std::string_view Catalog::Get(std::string_view locale,std::string_view id) const {
    auto lookup=[&](std::string_view language)->std::string_view {
        const auto l=languages_.find(std::string(language));
        if(l==languages_.end())return {};
        const auto text=l->second.find(std::string(id));
        return text==l->second.end()?std::string_view{}:text->second;
    };
    const auto text=lookup(Lower(locale));return text.empty()?lookup("en"):text;
}
std::string_view Catalog::Graphic(uint32_t crc) const {
    const auto found=graphics_.find(crc);return found==graphics_.end()?std::string_view{}:found->second;
}
std::string EncodeNative(std::string_view text,bool translated,size_t limit) {
    std::string result;
    for(size_t i=0;i<text.size();) {
        const auto first=static_cast<unsigned char>(text[i++]);uint32_t code=first;
        if(first>=0x80) {
            unsigned extra=0;uint32_t minimum=0;
            if(first>=0xC2 && first<=0xDF){extra=1;code=first&31;minimum=0x80;}
            else if(first>=0xE0 && first<=0xEF){extra=2;code=first&15;minimum=0x800;}
            else if(first>=0xF0 && first<=0xF4){extra=3;code=first&7;minimum=0x10000;}
            else throw std::runtime_error("Invalid UTF-8 in menu text");
            while(extra--) {
                if(i==text.size() || (static_cast<unsigned char>(text[i])&0xC0)!=0x80)
                    throw std::runtime_error("Truncated UTF-8 in menu text");
                code=(code<<6)|(static_cast<unsigned char>(text[i++])&63);
            }
            if(code<minimum)throw std::runtime_error("Overlong UTF-8 in menu text");
        }
        if(code<32 || code==127 || code>255 || (code>=128 && code<160))
            throw std::runtime_error("Menu text uses an unsupported native glyph");
        if(translated) {
            if(code==0xE3)code=0xE4;else if(code==0xC3)code=0xC4;
            else if(code==0xF5)code=0xF6;else if(code==0xD5)code=0xD6;
        }
        result.push_back(char(code));
        if(result.size()>limit)throw std::runtime_error("Text exceeds native string storage");
    }
    return result;
}
std::string EncodeNativeMultiline(std::string_view text,bool translated,size_t limit) {
    std::string result;
    for(size_t start=0;start<=text.size();) {
        const auto end=text.find('\n',start);
        const auto line=text.substr(start,end==std::string_view::npos?end:end-start);
        result+=EncodeNative(line,translated,limit);
        if(result.size()>limit)throw std::runtime_error("Multiline menu text exceeds native storage");
        if(end==std::string_view::npos)break;
        result.push_back('\n');
        if(result.size()>limit)throw std::runtime_error("Multiline menu text exceeds native storage");
        start=end+1;
    }
    return result;
}
float FitScale(float width,float preferred,float available) {
    if(!std::isfinite(width) || !std::isfinite(preferred) || !std::isfinite(available) ||
       width<0 || preferred<=0 || available<=0)return 0;
    return width>0?std::min(preferred,available/width):preferred;
}
TitleLayout LayoutTitle(float width) {
    TitleLayout result;result.scale=FitScale(width,1.5f,608-result.text_x-12);
    result.arrow_tip=result.text_x+std::max(0.f,width)*result.scale+12;return result;
}
}
