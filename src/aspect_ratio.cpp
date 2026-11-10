#include "aspect_ratio.h"
#include "launcher.h"
#include "app/app.h"
#include "core/log.h"
#include <atomic>
#include <cstring>

namespace openshao::aspect {
namespace { std::atomic<uint64_t> output_size{0}; }
float Current() {
    const auto size=output_size.load(std::memory_order_relaxed);
    if(size)return Resolve(launcher::Current().aspect_ratio,uint32_t(size>>32),uint32_t(size));
    const auto mode=app::Output();
    return Resolve(launcher::Current().aspect_ratio,mode.width,mode.height);
}
float Output(uint32_t width,uint32_t height) {
    const uint64_t size=(uint64_t(width)<<32)|height;
    if(output_size.exchange(size,std::memory_order_relaxed)!=size)
        LOGI("openshao graphics: aspect=%s output=%ux%u ratio=%.7g",launcher::Current().aspect_ratio.c_str(),width,height,
             Resolve(launcher::Current().aspect_ratio,width,height));
    return Resolve(launcher::Current().aspect_ratio,width,height);
}
void CorrectUiVertices(void* vertices,uint32_t count,uint32_t stride,bool cover) {
    if(!canvas_depth || !vertices || stride<8 || uint64_t(count)*stride>(2u<<20))return;
    auto scale=CanvasScale(Current());
    if(cover)scale={1/scale.y,1/scale.x};
    if(scale.x==1 && scale.y==1)return;
    auto* bytes=static_cast<uint8_t*>(vertices);
    for(uint32_t i=0;i<count;++i) {
        float xy[2];std::memcpy(xy,bytes+size_t(i)*stride,8);
        if(!std::isfinite(xy[0]) || !std::isfinite(xy[1]))continue;
        xy[0]=(xy[0]+.5f-320.f)*scale.x+320.f-.5f;
        xy[1]=(xy[1]+.5f-240.f)*scale.y+240.f-.5f;
        std::memcpy(bytes+size_t(i)*stride,xy,8);
    }
}
bool ExpandViewportTint(void* vertices,uint32_t count,uint32_t stride) {
    if((!screen_sprite_depth && font_anchor==UiAnchor::None) || !vertices || count!=4 || stride!=20)return false;
    auto* bytes=static_cast<uint8_t*>(vertices);
    float xy[4][2];uint32_t color=0;
    if(cinematic_sprite) {
        const auto scale=CanvasScale(Current());
        for(unsigned i=0;i<4;++i) {
            std::memcpy(xy[i],bytes+i*stride,8);
            if(!std::isfinite(xy[i][0]) || !std::isfinite(xy[i][1]))return false;
        }
        // The retail masks own their fade/slide: undo only the fitted projection so bands span the output.
        for(unsigned i=0;i<4;++i) {
            xy[i][0]=(xy[i][0]+.5f)/scale.x-.5f;
            xy[i][1]=(xy[i][1]+.5f)/scale.y-.5f;
            std::memcpy(bytes+i*stride,xy[i],8);
        }
        return true;
    }
    const auto anchor=screen_sprite_depth?sprite_anchor:font_anchor;
    if(anchor!=UiAnchor::None) {
        const float ratio=Current();
        auto offset=AnchorOffset(anchor,ratio);
        if(anchor==UiAnchor::TutorialPrompt)offset.x-=tutorial_portrait_inset;
        for(unsigned i=0;i<4;++i) {
            std::memcpy(xy[i],bytes+i*stride,8);
            if(!std::isfinite(xy[i][0]) || !std::isfinite(xy[i][1]))return false;
        }
        for(unsigned i=0;i<4;++i) {
            xy[i][0]+=offset.x+SpreadX(anchor,xy[i][0],ratio);xy[i][1]+=offset.y;
            std::memcpy(bytes+i*stride,xy[i],8);
        }
        return true;
    }
    for(unsigned i=0;i<4;++i) {
        std::memcpy(xy[i],bytes+i*stride,8);
        uint32_t c;std::memcpy(&c,bytes+i*stride+12,4);
        if(i==0)color=c;
        if(c!=color || ((c&0x00FEFEFE) && !solid_fill_depth && !artwork_sprite && tutorial_sprite==TutorialSprite::None))return false;
    }
    // Full-screen masks must cover the authored viewport. Keep widgets, rotated art and textured images fitted.
    for(const auto& vertex:xy)for(float value:vertex)if(!std::isfinite(value))return false;
    if(xy[0][0]!=xy[1][0] || xy[2][0]!=xy[3][0] ||
       xy[0][1]!=xy[2][1] || xy[1][1]!=xy[3][1])return false;
    const auto scale=CanvasScale(Current());
    if(artwork_sprite) {
        // Art and parchment share one bitmap; fit its authored bounds, not padded texture dimensions.
        const float width=xy[2][0]-xy[0][0],height=xy[1][1]-xy[0][1];
        if(width<=0 || height<=0)return false;
        const auto rect=FitArtwork(width,height,scale);
        for(unsigned i=0;i<4;++i) {
            xy[i][0]=rect.x+(i<2?0:rect.w);
            xy[i][1]=rect.y+(i%2?rect.h:0);
            std::memcpy(bytes+i*stride,xy[i],8);
        }
        return true;
    }
    if(tutorial_sprite!=TutorialSprite::None) {
        // Close the native half-pixel band/portrait gap before projection to avoid seams at fractional scales.
        const float extra=320.f*(1/scale.x-1);
        const float bottom=240.f*(1/scale.y-1);
        for(unsigned i=0;i<4;++i) {
            xy[i][0]+=tutorial_sprite==TutorialSprite::Panel && i<2?-extra:
                extra-tutorial_portrait_inset+(tutorial_sprite==TutorialSprite::Panel?.5f:0);
            xy[i][1]+=bottom;
            std::memcpy(bytes+i*stride,xy[i],8);
        }
        return true;
    }
    if(xy[0][0]>-320.5f || xy[2][0]<319.5f || xy[0][1]>-240.5f || xy[1][1]<239.5f)return false;
    for(unsigned i=0;i<4;++i) {
        xy[i][0]=(xy[i][0]+.5f)/scale.x-.5f;
        xy[i][1]=(xy[i][1]+.5f)/scale.y-.5f;
        std::memcpy(bytes+i*stride,xy[i],8);
    }
    return true;
}
}
