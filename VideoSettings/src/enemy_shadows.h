#pragma once
#include "d8.h"
#include <array>
#include <algorithm>
#include <cmath>

// Animated model silhouettes projected onto the native contact plane. This is
// deliberately not a shadow map: no new render targets or per-frame allocations.
namespace enemy_shadows {
struct Vec {float x,y,z;};
inline bool installed=false,enabled=false,drawing=false;
inline D3DMATRIX projection{};
inline DWORD tint=0x70000000;
inline unsigned submitted=0;
inline D3DMATRIX Multiply(const D3DMATRIX& a,const D3DMATRIX& b){
    D3DMATRIX out{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)out.m[i][j]+=a.m[i][k]*b.m[k][j];return out;
}
inline bool Project(Vec point,Vec degrees,D3DMATRIX& out){
    for(float f:{point.x,point.y,point.z,degrees.x,degrees.y,degrees.z})if(!std::isfinite(f))return false;
    constexpr float radians=3.14159265358979323846f/180.f;
    // Native quad uses Y, X, Z rotations in that order. Its up vector starts
    // on the Y axis, so the first rotation has no effect on the plane normal.
    float x=degrees.x*radians,z=degrees.z*radians;
    Vec n{-std::cos(x)*std::sin(z),std::cos(x)*std::cos(z),std::sin(x)};
    if(n.y<.45f)return false; // Walls / invalid contacts retain native shadows.
    out={};out._11=out._33=out._44=1;
    out._12=-n.x/n.y;out._32=-n.z/n.y;
    out._42=(n.x*point.x+n.y*point.y+n.z*point.z)/n.y+.035f;
    return true;
}
struct Caster {
    void* actor=nullptr;
    std::array<void*,16> clumps{};
    unsigned count=0;
    bool supported=true;
    void Add(void* clump){
        if(!clump||std::find(clumps.begin(),clumps.begin()+count,clump)!=clumps.begin()+count)return;
        if(count==clumps.size()){supported=false;return;}clumps[count++]=clump;
    }
};
inline std::array<Caster,128> casters{};
inline unsigned casterCount=0;
inline Caster* recording=nullptr;
inline Caster* casting=nullptr;
inline bool replaced=false;
inline void BeginFrame(){casterCount=0;recording=casting=nullptr;replaced=false;}
inline Caster* Find(void* actor){for(unsigned i=0;i<casterCount;++i)if(casters[i].actor==actor)return &casters[i];return nullptr;}
inline Caster* Record(void* actor){
    auto c=Find(actor);if(!c){if(casterCount==casters.size())return nullptr;c=&casters[casterCount++];}
    *c={};c->actor=actor;return c;
}
inline bool FixedXYZ(DWORD fvf){return !(fvf&1)&&(fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZ;}
inline void ObserveDraw(void* d){
    if(!recording||drawing||!recording->supported)return;
    DWORD fvf=0;if(FAILED(D8<77>(d,&fvf))||!FixedXYZ(fvf))recording->supported=false;
}
// Scoped device overrides only surround final submissions. RenderWare and the
// FPS mod keep their own caches; never restore a block around a whole clump.
struct State {
    void* d;
    static constexpr D3DRENDERSTATETYPE states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_CULLMODE,
        D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_ALPHATESTENABLE,
        D3DRS_TEXTUREFACTOR,D3DRS_COLORWRITEENABLE,D3DRS_STENCILENABLE,D3DRS_STENCILFUNC,D3DRS_STENCILREF,
        D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,
        D3DRS_FOGENABLE,D3DRS_VERTEXBLEND,D3DRS_INDEXEDVERTEXBLENDENABLE,D3DRS_LIGHTING};
    std::array<DWORD,std::size(states)> values{};
    DWORD colorOp=0,colorArg=0,alphaOp=0,alphaArg=0,nextOp=0,pixelShader=0;
    explicit State(void* device):d(device){
        for(size_t i=0;i<std::size(states);++i)D8<51>(d,states[i],&values[i]);
        D8<62>(d,DWORD(0),DWORD(1),&colorOp);D8<62>(d,DWORD(0),DWORD(2),&colorArg);
        D8<62>(d,DWORD(0),DWORD(4),&alphaOp);D8<62>(d,DWORD(0),DWORD(5),&alphaArg);
        D8<62>(d,DWORD(1),DWORD(1),&nextOp);D8<89>(d,&pixelShader);
    }
    void Set(bool clear){
        D8<88>(d,DWORD(0));
        TS(d,0,1,D3DTOP_SELECTARG1);TS(d,0,2,D3DTA_TFACTOR);
        TS(d,0,4,D3DTOP_SELECTARG1);TS(d,0,5,D3DTA_TFACTOR);TS(d,1,1,D3DTOP_DISABLE);
        RS(d,D3DRS_TEXTUREFACTOR,tint);RS(d,D3DRS_ZENABLE,clear?FALSE:TRUE);RS(d,D3DRS_ZWRITEENABLE,FALSE);
        RS(d,D3DRS_ZFUNC,D3DCMP_LESSEQUAL);RS(d,D3DRS_CULLMODE,D3DCULL_NONE);RS(d,D3DRS_ALPHATESTENABLE,FALSE);
        RS(d,D3DRS_ALPHABLENDENABLE,TRUE);RS(d,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);RS(d,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        RS(d,D3DRS_BLENDOP,D3DBLENDOP_ADD);RS(d,D3DRS_COLORWRITEENABLE,clear?0:7);
        RS(d,D3DRS_STENCILENABLE,TRUE);RS(d,D3DRS_STENCILFUNC,clear?D3DCMP_ALWAYS:D3DCMP_EQUAL);
        RS(d,D3DRS_STENCILREF,0);RS(d,D3DRS_STENCILMASK,0x80);RS(d,D3DRS_STENCILWRITEMASK,0x80);
        RS(d,D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP);RS(d,D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
        RS(d,D3DRS_STENCILPASS,clear?D3DSTENCILOP_ZERO:D3DSTENCILOP_INVERT);
        RS(d,D3DRS_FOGENABLE,FALSE);RS(d,D3DRS_VERTEXBLEND,D3DVBF_DISABLE);RS(d,D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE);RS(d,D3DRS_LIGHTING,FALSE);
    }
    ~State(){
        for(size_t i=0;i<std::size(states);++i)RS(d,states[i],values[i]);
        TS(d,0,1,colorOp);TS(d,0,2,colorArg);TS(d,0,4,alphaOp);TS(d,0,5,alphaArg);TS(d,1,1,nextOp);D8<88>(d,pixelShader);
    }
};
struct DrawScope {
    State saved;
    D3DMATRIX world{};
    explicit DrawScope(void* d):saved(d){
        D8<38>(d,D3DTS_WORLD,&world);auto projected=Multiply(world,projection);
        D8<37>(d,D3DTS_WORLD,&projected);saved.Set(false);++submitted;
    }
    ~DrawScope(){D8<37>(saved.d,D3DTS_WORLD,&world);}
};
// D3D8 surface desc differs from D3D9 (Size, no multisample quality).
struct SurfaceDesc {D3DFORMAT format;DWORD type,usage,pool,size,multisample,width,height;};
inline bool HasStencil(void* d){
    DWORD stencil=1;D3DVIEWPORT9 vp{};
    if(!d||FAILED(D8<51>(d,D3DRS_STENCILENABLE,&stencil))||stencil||FAILED(D8<41>(d,&vp)))return false;
    if(vp.Width!=Present().width||vp.Height!=Present().height)return false;
    void* depth=nullptr;if(FAILED(D8<33>(d,&depth))||!depth)return false;
    SurfaceDesc desc{};HRESULT hr=D8<8>(depth,&desc);D8<2>(depth);
    // Only an 8-bit stencil has our reserved high bit.
    return SUCCEEDED(hr)&&desc.format==D3DFMT_D24S8;
}
inline bool ClearCoverage(void* d){
    State saved(d);DWORD fvf=0;void* stream=nullptr;UINT stride=0;D3DVIEWPORT9 vp{};
    if(FAILED(D8<77>(d,&fvf))||FAILED(D8<84>(d,UINT(0),&stream,&stride))||FAILED(D8<41>(d,&vp))){if(stream)D8<2>(stream);return false;}
    struct Vertex {float x,y,z,rhw;};
    float l=float(vp.X)-.5f,t=float(vp.Y)-.5f,r=l+vp.Width,b=t+vp.Height;
    Vertex vertices[]={{l,t,0,1},{r,t,0,1},{l,b,0,1},{r,b,0,1}};
    saved.Set(true);D8<76>(d,DWORD(D3DFVF_XYZRHW));
    auto hr=D8<72>(d,D3DPT_TRIANGLESTRIP,UINT(2),static_cast<const void*>(vertices),UINT(sizeof(Vertex)));
    D8<76>(d,fvf);D8<83>(d,UINT(0),stream,stride);if(stream)D8<2>(stream);return SUCCEEDED(hr);
}
}
