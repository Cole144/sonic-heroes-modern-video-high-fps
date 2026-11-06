#pragma once
#include "enemy_shadows.h"
#include "self_shadow_shaders.h"
namespace self_shadows {
using Vec=enemy_shadows::Vec;
using Matrix=D3DMATRIX;
inline Matrix Identity(){Matrix a{};a._11=a._22=a._33=a._44=1;return a;}
inline Vec Sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec Unit(Vec a){float l=sqrtf(Dot(a,a));return {a.x/l,a.y/l,a.z/l};}
inline Matrix InverseView(const Matrix& a){
    // RenderWare camera frames are orthonormal (no scale/shear).
    Matrix b=Identity();for(int i=0;i<3;i++)for(int j=0;j<3;j++)b.m[i][j]=a.m[j][i];
    for(int j=0;j<3;j++)b.m[3][j]=-(a._41*b.m[0][j]+a._42*b.m[1][j]+a._43*b.m[2][j]);return b;
}
struct Bounds {
    Vec low{1e20f,1e20f,1e20f},high{-1e20f,-1e20f,-1e20f};bool valid=false;
    void Add(Vec p,float r){if(!std::isfinite(r)||r<=0||r>1000)return;for(float x:{p.x,p.y,p.z})if(!std::isfinite(x))return;
        low={std::min(low.x,p.x-r),std::min(low.y,p.y-r),std::min(low.z,p.z-r)};
        high={std::max(high.x,p.x+r),std::max(high.y,p.y+r),std::max(high.z,p.z+r)};valid=true;}
};
inline constexpr UINT MapSize=1024;
inline bool enabled=false,busy=false,failed=false,installed=false;
inline int pass=0; // 1: packed light depth; 2: receiver overlay
inline void* texture=nullptr,*surface=nullptr,*depth=nullptr,*ramps[2]{};
inline DWORD depthShader=0,receiverShader=0;
inline Matrix lightView{},lightProjection{},lightUV{};
inline float depthRange=1;
inline unsigned groups=0,depthDraws=0,receiverDraws=0;
inline unsigned playerMaps=0,playerReceivers=0;
struct CachedMap {
    void* clump=nullptr,*texture=nullptr,*surface=nullptr;
    uint32_t frame=0;bool valid=false;
    Matrix view{},projection{},uv{};float range=1;
};
inline std::array<CachedMap,16> cache{};
inline void Invalidate(){for(auto& entry:cache){entry.clump=nullptr;entry.valid=false;}}
inline std::array<void*,512> players{};inline size_t playerCount=0;
inline void Register(void* clump){if(clump&&playerCount<players.size()&&std::find(players.begin(),players.begin()+playerCount,clump)==players.begin()+playerCount)players[playerCount++]=clump;}
inline bool Player(void* clump){return std::find(players.begin(),players.begin()+playerCount,clump)!=players.begin()+playerCount;}
inline void Release(){
    for(auto& entry:cache){if(entry.surface)D8<2>(entry.surface);if(entry.texture)D8<2>(entry.texture);entry={};}
    auto d=Device();for(void** p:{&surface,&texture,&depth,&ramps[0],&ramps[1]})if(*p){D8<2>(*p);*p=nullptr;}
    if(d){if(depthShader)D8<90>(d,depthShader);if(receiverShader)D8<90>(d,receiverShader);}depthShader=receiverShader=0;failed=false;
}
inline bool Ensure(){
    if(texture)return true;if(failed)return false;auto d=Device();
    bool ok=SUCCEEDED(D8<87>(d,depthCode,&depthShader))&&SUCCEEDED(D8<87>(d,receiveCode,&receiverShader))&&
        SUCCEEDED(D8<20>(d,MapSize,MapSize,UINT(1),DWORD(D3DUSAGE_RENDERTARGET),D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture))&&
        SUCCEEDED(D8<15>(texture,UINT(0),&surface))&&SUCCEEDED(D8<26>(d,MapSize,MapSize,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&depth));
    for(int i=0;i<2&&ok;++i){
        ok=SUCCEEDED(D8<20>(d,UINT(256),UINT(1),UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&ramps[i]));
        if(ok){D3DLOCKED_RECT lock{};ok=SUCCEEDED(D8<16>(ramps[i],UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)));
            if(ok){auto pixels=static_cast<DWORD*>(lock.pBits);for(DWORD j=0;j<256;j++)pixels[j]=0xff000000|(j<<(i?8:16));D8<17>(ramps[i],UINT(0));}}
    }
    if(!ok){Release();failed=true;}return ok;
}
inline bool Setup(const Bounds& b){
    if(!b.valid)return false;Vec center{(b.low.x+b.high.x)*.5f,(b.low.y+b.high.y)*.5f,(b.low.z+b.high.z)*.5f};
    Vec delta=Sub(b.high,b.low);float radius=sqrtf(Dot(delta,delta))*.525f;if(radius<.1f||radius>1500)return false;
    Vec z=Unit({.45f,-1.f,.35f}),x=Unit(Cross({0,1,0},z)),y=Cross(z,x);
    Vec eye{center.x-z.x*radius*2,center.y-z.y*radius*2,center.z-z.z*radius*2};
    lightView=Identity();Vec axes[]={x,y,z};for(int j=0;j<3;j++){lightView.m[0][j]=axes[j].x;lightView.m[1][j]=axes[j].y;lightView.m[2][j]=axes[j].z;lightView.m[3][j]=-Dot(eye,axes[j]);}
    depthRange=radius*4;lightProjection=Identity();lightProjection._11=lightProjection._22=1/radius;lightProjection._33=1/depthRange;
    Matrix uv=Identity();uv._11=.5f/radius;uv._22=-.5f/radius;uv._41=uv._42=.5f+.5f/MapSize;
    lightUV=enemy_shadows::Multiply(lightView,uv);return true;
}
inline bool EligibleDraw(void* d){
    DWORD fvf=0,blend=1,shader=1;D8<77>(d,&fvf);D8<51>(d,D3DRS_ALPHABLENDENABLE,&blend);D8<89>(d,&shader);
    return enemy_shadows::FixedXYZ(fvf)&&!blend&&!shader;
}
struct DrawState {
    void* d;DWORD ps=0;std::array<std::array<float,4>,5> constants{};
    static constexpr D3DRENDERSTATETYPE types[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_COLORWRITEENABLE,D3DRS_STENCILENABLE,D3DRS_CULLMODE,static_cast<D3DRENDERSTATETYPE>(47),D3DRS_NORMALIZENORMALS};
    static constexpr DWORD stages[]={1,2,3,4,5,6,11,13,14,15,16,17,18,24};
    std::array<DWORD,std::size(types)> states{};
    DWORD tss[6][std::size(stages)]{};void* textures[6]{};Matrix view{},projection{},transforms[6]{};
    explicit DrawState(void* device):d(device){
        for(size_t i=0;i<std::size(types);i++)D8<51>(d,types[i],&states[i]);
        D8<89>(d,&ps);D8<92>(d,DWORD(0),constants.data(),DWORD(5));D8<38>(d,D3DTS_VIEW,&view);D8<38>(d,D3DTS_PROJECTION,&projection);
        for(DWORD i=0;i<6;i++){D8<60>(d,i,&textures[i]);D8<38>(d,DWORD(D3DTS_TEXTURE0)+i,&transforms[i]);for(size_t j=0;j<std::size(stages);j++)D8<62>(d,i,stages[j],&tss[i][j]);}
        Matrix toLight=pass==1?Identity():enemy_shadows::Multiply(InverseView(view),lightView);
        for(DWORD i=0;i<6;i++){
            bool map=pass==2&&i<4;bool normal=pass==2&&i==5;bool low=pass==1&&i==1;Matrix matrix{};
            if(map){matrix=enemy_shadows::Multiply(InverseView(view),lightUV);matrix._41+=(i&1?.75f:-.75f)/MapSize;matrix._42+=(i&2?.75f:-.75f)/MapSize;}
            else if(normal){for(int j=0;j<3;j++)matrix.m[j][0]=-toLight.m[j][2];matrix._44=1;}
            else {for(int j=0;j<4;j++)matrix.m[j][0]=toLight.m[j][2]/depthRange*(low?256.f:1.f);matrix._42=.5f;matrix._44=1;}
            D8<37>(d,DWORD(D3DTS_TEXTURE0)+i,&matrix);D8<61>(d,i,map?texture:ramps[low?1:0]);
            TS(d,i,1,D3DTOP_SELECTARG1);TS(d,i,2,D3DTA_TEXTURE);TS(d,i,4,D3DTOP_SELECTARG1);TS(d,i,5,D3DTA_TEXTURE);TS(d,i,11,(normal?D3DTSS_TCI_CAMERASPACENORMAL:D3DTSS_TCI_CAMERASPACEPOSITION)|i);TS(d,i,24,D3DTTFF_COUNT2);
            TS(d,i,13,low?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);TS(d,i,14,D3DTADDRESS_CLAMP);
            TS(d,i,16,D3DTEXF_POINT);TS(d,i,17,D3DTEXF_POINT);TS(d,i,18,D3DTEXF_NONE);
        }
        D8<88>(d,pass==1?depthShader:receiverShader);
        float c[5][4]={{255.f/256.f,255.f/65536.f,0,0},{.0025f,.0025f,.0025f,.0025f},{.07f,.07f,.07f,.07f},{0,0,0,0},{1,1,1,1}};
        D8<91>(d,DWORD(0),c,DWORD(5));
        if(pass==1){D8<37>(d,D3DTS_VIEW,&lightView);D8<37>(d,D3DTS_PROJECTION,&lightProjection);}
        RS(d,D3DRS_NORMALIZENORMALS,TRUE);RS(d,D3DRS_ZENABLE,TRUE);RS(d,D3DRS_ZWRITEENABLE,pass==1);RS(d,D3DRS_ZFUNC,pass==1?D3DCMP_LESSEQUAL:D3DCMP_EQUAL);
        RS(d,D3DRS_ALPHABLENDENABLE,pass==2);RS(d,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);RS(d,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);RS(d,D3DRS_BLENDOP,D3DBLENDOP_ADD);
        RS(d,D3DRS_ALPHATESTENABLE,FALSE);RS(d,D3DRS_FOGENABLE,FALSE);RS(d,D3DRS_LIGHTING,FALSE);RS(d,D3DRS_COLORWRITEENABLE,7);RS(d,D3DRS_STENCILENABLE,FALSE);RS(d,D3DRS_CULLMODE,pass==1?D3DCULL_NONE:states[12]);RS(d,static_cast<D3DRENDERSTATETYPE>(47),pass==1?0:states[13]);
        if(pass==1)++depthDraws;else ++receiverDraws;
    }
    ~DrawState(){
        for(size_t i=0;i<std::size(types);i++)RS(d,types[i],states[i]);
        D8<88>(d,ps);D8<91>(d,DWORD(0),constants.data(),DWORD(5));D8<37>(d,D3DTS_VIEW,&view);D8<37>(d,D3DTS_PROJECTION,&projection);
        for(DWORD i=0;i<6;i++){D8<61>(d,i,textures[i]);if(textures[i])D8<2>(textures[i]);D8<37>(d,DWORD(D3DTS_TEXTURE0)+i,&transforms[i]);for(size_t j=0;j<std::size(stages);j++)TS(d,i,stages[j],tss[i][j]);}
    }
};
inline bool MainTarget(){
    D3DVIEWPORT9 vp{};return SUCCEEDED(D8<41>(Device(),&vp))&&vp.Width==Present().width&&vp.Height==Present().height;
}
using Render=void*(__cdecl*)(void*);
inline bool DepthPass(void* const* clumps,unsigned count,Render render){
    auto d=Device();void* target=nullptr,*oldDepth=nullptr;D3DVIEWPORT9 vp{};
    if(FAILED(D8<32>(d,&target))||FAILED(D8<33>(d,&oldDepth))||FAILED(D8<41>(d,&vp))){if(target)D8<2>(target);if(oldDepth)D8<2>(oldDepth);return false;}
    bool ok=SUCCEEDED(D8<31>(d,surface,depth));
    if(ok){D3DVIEWPORT9 shadowVP{0,0,MapSize,MapSize,0,1};D8<40>(d,&shadowVP);
        ok=SUCCEEDED(D8<36>(d,DWORD(0),static_cast<const D3DRECT*>(nullptr),DWORD(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER),DWORD(0xffffffff),1.f,DWORD(0)));
        if(ok){pass=1;for(unsigned i=0;i<count;i++)render(clumps[i]);pass=0;}}
    bool restored=SUCCEEDED(D8<31>(d,target,oldDepth))&&SUCCEEDED(D8<40>(d,&vp));
    D8<2>(target);if(oldDepth)D8<2>(oldDepth);return ok&&restored;
}
inline void RenderGroup(void* const* clumps,unsigned count,const Bounds& bounds,Render render){
    if(!enabled||busy||!count||!MainTarget()||!Setup(bounds)||!Ensure())return;
    busy=true;
    if(DepthPass(clumps,count,render)){pass=2;for(unsigned i=0;i<count;i++)render(clumps[i]);pass=0;++groups;}
    busy=false;
}
// Player atomics are submitted by the world renderer, not RpClumpRender.
// Generate the whole body's map once, then shade only the atomic just drawn.
inline void RenderAtomic(void* clump,void* atomic,uint32_t frame,const Bounds& bounds,Render renderClump,Render renderAtomic){
    if(!enabled||busy||!clump||!atomic||!MainTarget()||!Ensure())return;
    auto it=std::find_if(cache.begin(),cache.end(),[&](const CachedMap& e){return e.clump==clump;});
    if(it==cache.end())it=std::find_if(cache.begin(),cache.end(),[&](const CachedMap& e){return !e.valid||e.frame!=frame;});
    if(it==cache.end())return;auto& entry=*it;
    if(entry.clump!=clump){entry.clump=clump;entry.valid=false;}
    if(!entry.texture){
        auto d=Device();if(FAILED(D8<20>(d,MapSize,MapSize,UINT(1),DWORD(D3DUSAGE_RENDERTARGET),D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&entry.texture)))return;
        if(FAILED(D8<15>(entry.texture,UINT(0),&entry.surface))){D8<2>(entry.texture);entry.texture=nullptr;return;}
    }
    void* sharedTexture=texture,*sharedSurface=surface;texture=entry.texture;surface=entry.surface;busy=true;
    if(!entry.valid||entry.frame!=frame){
        entry.valid=Setup(bounds)&&DepthPass(&clump,1,renderClump);entry.frame=frame;
        if(entry.valid){entry.view=lightView;entry.projection=lightProjection;entry.uv=lightUV;entry.range=depthRange;++playerMaps;}
    }
    if(entry.valid){
        lightView=entry.view;lightProjection=entry.projection;lightUV=entry.uv;depthRange=entry.range;
        pass=2;renderAtomic(atomic);pass=0;++playerReceivers;
    }
    busy=false;texture=sharedTexture;surface=sharedSurface;
}
}






