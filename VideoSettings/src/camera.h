#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>

namespace video {
struct CameraView {float x,y;};
// Graphics Essentials' adapter declares this callback void, so do not depend
// on the retail implementation's EAX return surviving that adapter.
using SetCameraViewFn=void(__cdecl*)(void*,const CameraView*);

// Called only after TObjCamera::setCameraViewWindow has authored a fresh view.
// Feed a copy through the existing RenderWare/Graphics Essentials hook chain:
// both projection and clipping use the expanded view, without accumulating
// scale into the game's persistent scripted-camera parameters.
inline bool ApplyCameraView(void* camera,int percent,CameraView* sharedView,SetCameraViewFn setter){
    if(!camera||!sharedView||!setter||percent==100)return false;
    auto bytes=static_cast<unsigned char*>(camera);
    int projection=0;memcpy(&projection,bytes+0x14,4);
    if(projection!=1)return false; // rwPERSPECTIVE only
    CameraView base{};memcpy(&base,bytes+0x68,sizeof(base));
    if(!std::isfinite(base.x)||!std::isfinite(base.y)||base.x<=0||base.y<=0)return false;
    float scale=std::clamp(percent,50,150)*.01f;
    CameraView expanded{base.x*scale,base.y*scale};
    setter(camera,&expanded);
    // The retail function also publishes this view for game projection helpers.
    *sharedView=expanded;
    return true;
}
}
