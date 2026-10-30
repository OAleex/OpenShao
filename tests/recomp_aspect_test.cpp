#include "aspect_ratio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void Check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static bool Near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
    using namespace openshao::aspect;
    Check(Near(Resolve("original",3440,1440),4.f/3),"original mode remains 4:3 on an ultrawide display");
    Check(Near(Resolve("16:9",640,480),16.f/9),"a forced ratio is independent of window dimensions");
    Check(Near(Resolve("16:10",1920,1080),1.6f),"16:10 is supported");
    Check(Near(Resolve("21:9",640,480),21.f/9) && Near(Resolve("32:9",640,480),32.f/9),"ultrawide presets are exact");
    Check(Near(Resolve("auto",3440,1440),3440.f/1440),"automatic mode uses the actual pixel ratio, not a rounded 21:9 label");
    Check(Near(Resolve("auto",1500,1000),1.5f) && Near(Resolve("auto",1000,1500),2.f/3),"automatic mode supports 3:2 and portrait windows");
    Check(Near(Resolve("auto",0,0),4.f/3),"zero-sized outputs have a safe startup ratio");
    Check(Near(CameraScale(4.f/3),4.f/3) && Near(CameraScale(16.f/9),16.f/9) && Near(CameraScale(32.f/9),32.f/9),"camera aspect matches the output ratio (retail 640/480 at 4:3)");
    auto rect=Fit(1920,1080,4.f/3);
    Check(Near(rect.x,240) && Near(rect.y,0) && Near(rect.w,1440) && Near(rect.h,1080),"4:3 presentation is pillarboxed inside 16:9");
    rect=Fit(640,480,32.f/9);
    Check(Near(rect.x,0) && Near(rect.y,150) && Near(rect.w,640) && Near(rect.h,180),"32:9 presentation fits a smaller 4:3 window without stretching");
    auto hud=CanvasScale(32.f/9);
    Check(Near(hud.x,.375f) && Near(hud.y,1),"HUD keeps its 4:3 proportions inside super ultrawide");
    hud=CanvasScale(2.f/3);Check(Near(hud.x,1) && Near(hud.y,.5f),"portrait HUD fits vertically");
    auto movie=MovieRect(640,480,16.f/9,640,480);
    Check(Near(movie.x,80) && Near(movie.y,0) && Near(movie.w,480) && Near(movie.h,480),"4:3 movies remain uncropped and unstretched in widescreen");
    movie=MovieRect(640,480,4.f/3,1920,1080);
    Check(Near(movie.x,0) && Near(movie.y,60) && Near(movie.w,640) && Near(movie.h,360),"16:9 movies remain letterboxed in the original canvas");
    Check(!Valid("invalid") && Valid("auto") && Valid("4:3"),"only supported aspect modes are accepted");
    std::puts("PASS: ratio modes, exact automatic dimensions, horizontal FOV, presentation, HUD and movie fitting");
}
