#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace openshao::aspect {
inline constexpr float Original=4.f/3;
inline constexpr std::array<std::string_view,6> modes{"original","auto","16:9","16:10","21:9","32:9"};
inline constexpr std::array<const char*,6> labels{"Original (4:3)","Automatic (window)","Widescreen (16:9)","Widescreen (16:10)","Ultrawide (21:9)","Super ultrawide (32:9)"};
inline bool Valid(std::string_view mode){return mode=="4:3" || std::find(modes.begin(),modes.end(),mode)!=modes.end();}
inline float Resolve(std::string_view mode,uint32_t width,uint32_t height) {
    if(mode=="auto")return width && height?float(width)/height:Original;
    if(mode=="16:9")return 16.f/9;
    if(mode=="16:10")return 16.f/10;
    if(mode=="21:9")return 21.f/9;
    if(mode=="32:9")return 32.f/9;
    return Original;
}
// The camera aspect field (camera+0x20) is width/height: retail sets 640/480 in its constructor (0x124450).
inline float CameraScale(float ratio){return ratio;}
struct Rect {float x=0,y=0,w=0,h=0;};
inline Rect Fit(float width,float height,float ratio) {
    if(width<=0 || height<=0 || !std::isfinite(ratio) || ratio<=0)return {};
    const float w=std::min(width,height*ratio),h=w/ratio;
    return {(width-w)*.5f,(height-h)*.5f,w,h};
}
struct Scale {float x=1,y=1;};
inline Scale CanvasScale(float ratio) {
    if(!std::isfinite(ratio) || ratio<=0)return {};
    return {std::min(1.f,Original/ratio),std::min(1.f,ratio/Original)};
}
inline Rect MovieRect(float canvas_width,float canvas_height,float ratio,float movie_width,float movie_height) {
    if(canvas_width<=0 || canvas_height<=0 || ratio<=0 || movie_width<=0 || movie_height<=0)return {};
    const float movie_ratio=movie_width/movie_height;
    const float w=canvas_width*std::min(1.f,movie_ratio/ratio);
    const float h=canvas_height*std::min(1.f,ratio/movie_ratio);
    return {(canvas_width-w)*.5f,(canvas_height-h)*.5f,w,h};
}
float Current();
float Output(uint32_t width,uint32_t height);
void UpdateCamera();
void CorrectUiVertices(void* vertices,uint32_t count,uint32_t stride,bool cover=false);
bool ExpandViewportTint(void* vertices,uint32_t count,uint32_t stride);
inline thread_local unsigned canvas_depth=0;
inline thread_local unsigned screen_sprite_depth=0;
inline thread_local unsigned solid_fill_depth=0;
inline thread_local unsigned backdrop_depth=0;
inline thread_local bool artwork_sprite=false;
inline thread_local bool cinematic_sprite=false;
inline constexpr float ArtworkFooter=64;
enum class TutorialSprite {None,Panel,Portrait};
// TopSplit moves each vertex toward its own side of the viewport, so a shared
// composition keeps its pieces whole and only quads crossing the center widen.
// TopStretch maps the shared Ko-op bar linearly, keeping its fill proportional.
// KoopComboRight is player two's combo block: the retail layout runs past the
// right edge (also at 4:3), so it mirrors player one's inset from its medallion.
enum class UiAnchor {None,TopLeft,TopRight,BottomLeft,BottomRight,TutorialPrompt,TopSplit,TopStretch,KoopComboRight,EnemyLeft,EnemyRight};
inline constexpr float KoopComboInset=78;
inline thread_local UiAnchor sprite_anchor=UiAnchor::None;
inline thread_local UiAnchor font_anchor=UiAnchor::None;
inline Scale AnchorOffset(UiAnchor anchor,float ratio) {
    if(anchor==UiAnchor::None)return {0,0};
    const auto scale=CanvasScale(ratio);
    // Keep common enemy bars on their authored vertical baseline. Anchor only
    // their horizontal position, including in portrait Automatic viewports.
    if(anchor==UiAnchor::EnemyLeft || anchor==UiAnchor::EnemyRight)
        return {(anchor==UiAnchor::EnemyLeft?-320.f:320.f)*(1/scale.x-1),0};
    if(anchor==UiAnchor::TopSplit || anchor==UiAnchor::TopStretch)return {0,-240.f*(1/scale.y-1)};
    const bool right=anchor==UiAnchor::TopRight || anchor==UiAnchor::BottomRight || anchor==UiAnchor::TutorialPrompt ||
                     anchor==UiAnchor::KoopComboRight;
    const bool bottom=anchor==UiAnchor::BottomLeft || anchor==UiAnchor::BottomRight || anchor==UiAnchor::TutorialPrompt;
    return {(right?320.f:-320.f)*(1/scale.x-1)-(anchor==UiAnchor::KoopComboRight?KoopComboInset:0),
            (bottom?240.f:-240.f)*(1/scale.y-1)};
}
// Half width of the retail Ko-op health bar (x 107.5 to 533.5 on the 640x480 canvas).
inline constexpr float KoopBarHalfWidth=213;
inline float SpreadX(UiAnchor anchor,float x,float ratio) {
    if(anchor!=UiAnchor::TopSplit && anchor!=UiAnchor::TopStretch)return 0;
    const float spread=320.f*(1/CanvasScale(ratio).x-1);
    if(anchor==UiAnchor::TopSplit)return x<0?-spread:spread;
    return spread*std::clamp(x/KoopBarHalfWidth,-1.f,1.f);
}
inline thread_local TutorialSprite tutorial_sprite=TutorialSprite::None;
inline thread_local float tutorial_portrait_inset=0;
inline thread_local unsigned viewport_text_depth=0;
class SpriteScope {
    bool solid_fill_;
    TutorialSprite previous_tutorial_=tutorial_sprite;
    bool previous_artwork_=artwork_sprite;
    bool previous_cinematic_=cinematic_sprite;
    UiAnchor previous_anchor_=sprite_anchor;
public:
    explicit SpriteScope(bool solid_fill=false,TutorialSprite tutorial=TutorialSprite::None,bool artwork=false,UiAnchor anchor=UiAnchor::None,bool cinematic=false):solid_fill_(solid_fill){
        ++screen_sprite_depth;if(solid_fill_)++solid_fill_depth;tutorial_sprite=tutorial;artwork_sprite=artwork;sprite_anchor=anchor;
        cinematic_sprite=cinematic;
    }
    ~SpriteScope(){--screen_sprite_depth;if(solid_fill_)--solid_fill_depth;tutorial_sprite=previous_tutorial_;artwork_sprite=previous_artwork_;sprite_anchor=previous_anchor_;cinematic_sprite=previous_cinematic_;}
    SpriteScope(const SpriteScope&)=delete;
    SpriteScope& operator=(const SpriteScope&)=delete;
};
class CanvasScope {
public:
    CanvasScope(){++canvas_depth;}
    ~CanvasScope(){--canvas_depth;}
    CanvasScope(const CanvasScope&)=delete;
    CanvasScope& operator=(const CanvasScope&)=delete;
};
class FontScope {
    UiAnchor previous_=font_anchor;
public:
    explicit FontScope(UiAnchor anchor){font_anchor=anchor;}
    ~FontScope(){font_anchor=previous_;}
    FontScope(const FontScope&)=delete;
    FontScope& operator=(const FontScope&)=delete;
};
// Restore guest state after the backend copies the transformed 640x480 screen-space passes.
template<class State,class Draw> void WithCanvasClip(State& state,Draw draw,bool viewport=false) {
    const auto scale=CanvasScale(Current());
    if(viewport || artwork_sprite || viewport_text_depth || !canvas_depth || (scale.x==1 && scale.y==1)){draw();return;}
    const auto count=state.scissor_count;
    const auto scissors=state.scissors;
    const float left=320.f*(1-scale.x),top=240.f*(1-scale.y);
    float x0=left,y0=top,x1=640-left,y1=480-top;
    if(count) {
        x0=std::max(x0,left+float(int32_t(scissors[0]))*scale.x);
        y0=std::max(y0,top+float(int32_t(scissors[1]))*scale.y);
        x1=std::min(x1,left+float(int32_t(scissors[2]))*scale.x);
        y1=std::min(y1,top+float(int32_t(scissors[3]))*scale.y);
    }
    state.scissor_count=1;
    state.scissors[0]=uint32_t(std::ceil(x0));state.scissors[1]=uint32_t(std::ceil(y0));
    state.scissors[2]=uint32_t(std::max(std::ceil(x0),std::floor(x1)));
    state.scissors[3]=uint32_t(std::max(std::ceil(y0),std::floor(y1)));
    draw();
    state.scissor_count=count;state.scissors=scissors;
}
}
