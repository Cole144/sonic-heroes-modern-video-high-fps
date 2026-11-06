#pragma once
#include "draw.h"
#include <array>

namespace native_menu {
// Rows are Audio/Vibration and Game Data/Language/Video. No wrapping between
// unrelated rows; every direction is owned here instead of mixing PC up/down
// traversal with a separate TV shortcut.
inline int Navigate(int choice,DWORD input,bool audio){
    constexpr int up[]={0,1,0,1,1},down[]={2,3,2,3,4},left[]={0,0,2,2,3},right[]={1,1,3,4,4};
    choice=std::clamp(choice,0,4);
    int next=input&1?up[choice]:input&2?down[choice]:input&4?left[choice]:input&8?right[choice]:choice;
    return !audio&&next==0?1:next;
}
// The final RenderWare Im2D vertices already include the authored animation and
// the installed widescreen/scaling hooks. Recover a sprite's UV-to-screen basis.
struct HexPose {float x=0,y=0,ax=0,ay=0,bx=0,by=0;DWORD tint=0xffffffff;};
inline bool Pose(const ui::Vertex& a,const ui::Vertex& b,const ui::Vertex& c,HexPose& out){
    float u=b.u-a.u,v=b.v-a.v,s=c.u-a.u,t=c.v-a.v,det=u*t-s*v;
    if(!std::isfinite(det)||fabsf(det)<.000001f)return false;
    out.ax=((b.x-a.x)*t-(c.x-a.x)*v)/det/256;
    out.ay=((b.y-a.y)*t-(c.y-a.y)*v)/det/256;
    out.bx=((c.x-a.x)*u-(b.x-a.x)*s)/det/256;
    out.by=((c.y-a.y)*u-(b.y-a.y)*s)/det/256;
    out.x=a.x+(70-a.u*256)*out.ax+(73-a.v*256)*out.bx;
    out.y=a.y+(70-a.u*256)*out.ay+(73-a.v*256)*out.by;
    out.tint=a.color;
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.ax)&&std::isfinite(out.ay)&&std::isfinite(out.bx)&&std::isfinite(out.by);
}
// Project retained RenderWare geometry with the exact D3D8 fixed-function
// transforms/viewport used by its draw, including the widescreen transform.
inline bool Project(const float* position,const float* uv,const D3DMATRIX& world,const D3DMATRIX& view,const D3DMATRIX& projection,const D3DVIEWPORT9& viewport,DWORD tint,ui::Vertex& out){
    float p[4]={position[0],position[1],position[2],1};
    for(const D3DMATRIX* matrix:{&world,&view,&projection}){
        float q[4]{};for(int col=0;col<4;col++)for(int row=0;row<4;row++)q[col]+=p[row]*matrix->m[row][col];
        std::copy(q,q+4,p);
    }
    if(!std::isfinite(p[3])||fabsf(p[3])<.000001f)return false;
    out={viewport.X+(p[0]/p[3]+1)*viewport.Width*.5f,viewport.Y+(1-p[1]/p[3])*viewport.Height*.5f,p[2]/p[3],1/p[3],tint,uv[0],uv[1]};
    return std::isfinite(out.x)&&std::isfinite(out.y);
}
inline DWORD ShapeTint(const float* localMult,const float* localAdd,const float* parentMult,const float* parentAdd){
    constexpr int shifts[]={16,8,0,24};DWORD tint=0;
    for(int i=0;i<4;i++){
        float channel=255*localMult[i]*parentMult[i]+localAdd[i]*parentMult[i]+255*parentAdd[i];
        tint|=DWORD(std::clamp(channel,0.f,255.f))<<shifts[i];
    }
    return tint;
}
struct Hexes {
    std::array<HexPose,8> items{};unsigned count=0;
    void Clear(){count=0;}
    void Add(const ui::Vertex& a,const ui::Vertex& b,const ui::Vertex& c,unsigned first){
        HexPose pose;if(!Pose(a,b,c,pose))return;
        for(unsigned i=first;i<count;i++)if(fabsf(items[i].x-pose.x)<.5f&&fabsf(items[i].y-pose.y)<.5f)return;
        if(count<items.size())items[count++]=pose;
    }
    void Observe(int primitive,const ui::Vertex* vertices,int n){
        if(!vertices||n<3||n>4096)return;
        unsigned first=count;
        if(primitive==3){for(int i=0;i+2<n;i+=3)Add(vertices[i],vertices[i+1],vertices[i+2],first);}
        else if(primitive==4){for(int i=0;i+2<n;i++)Add(vertices[i],vertices[i+1],vertices[i+2],first);}
        else if(primitive==5){for(int i=1;i+1<n;i++)Add(vertices[0],vertices[i],vertices[i+1],first);}
    }
    void ObserveIndexed(int primitive,const ui::Vertex* vertices,int n,const unsigned short* indices,int total){
        if(!vertices||!indices||n<3||n>4096||total<3||total>12288)return;
        for(int i=0;i<total;i++)if(indices[i]>=n)return;
        unsigned first=count;
        if(primitive==3){for(int i=0;i+2<total;i+=3)Add(vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]],first);}
        else if(primitive==4){for(int i=0;i+2<total;i++)Add(vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]],first);}
        else if(primitive==5){for(int i=1;i+1<total;i++)Add(vertices[indices[0]],vertices[indices[i]],vertices[indices[i+1]],first);}
    }
    bool TVTransform(float screenScale,float rootX,float rootY,unsigned expected,ui::NativeTransform& out)const{
        if(count!=expected||count<3||count>4)return false;
        // The rightmost member of the lower row is the adjacent Language button.
        // Resolve the row using projected centers, not the selected animation.
        const HexPose* reference=&items[0];
        for(unsigned i=1;i<count;i++)if(items[i].y>reference->y+20*screenScale||(fabsf(items[i].y-reference->y)<=20*screenScale&&items[i].x>reference->x))reference=&items[i];
        const auto& p=*reference;
        const HexPose* previous=nullptr;
        for(unsigned i=0;i<count;i++)if(items[i].x<p.x&&fabsf(items[i].y-p.y)<=20*screenScale&&(!previous||items[i].x>previous->x))previous=&items[i];
        float step=previous?p.x-previous->x:152.5f*screenScale;
        out={true,p.ax/1.125f,p.ay/1.125f,p.bx/1.125f,p.by/1.125f,p.x+step,p.y,rootX,rootY,p.tint};return true;
    }
};
inline bool Hit(const ui::NativeTransform& pose,float x,float y){
    if(!pose.enabled)return false;
    float det=pose.ax*pose.by-pose.bx*pose.ay;if(fabsf(det)<.00001f)return false;
    x-=pose.x;y-=pose.y;
    return fabsf((x*pose.by-y*pose.bx)/det)<56&&fabsf((y*pose.ax-x*pose.ay)/det)<63;
}
inline ui::NativeTransform ToPage(ui::NativeTransform pose,float amount,float scale,float ox,float oy){
    auto lerp=[&](float a,float b){return a+(b-a)*amount;};
    pose.ax=lerp(pose.ax,scale);pose.ay=lerp(pose.ay,0);pose.bx=lerp(pose.bx,0);pose.by=lerp(pose.by,scale);
    pose.x=lerp(pose.x,248*scale+ox-.5f);pose.y=lerp(pose.y,205*scale+oy-.5f);
    DWORD tint=0;for(int shift=0;shift<32;shift+=8)tint|=DWORD(lerp(float((pose.tint>>shift)&255),255))<<shift;pose.tint=tint;
    return pose;
}
}
