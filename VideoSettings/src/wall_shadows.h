#pragma once
#include "d8.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The native receiver pass projects each character shadow texture along one
// fixed direction (straight down) with no regard for the surface it lands on.
// A wall parallel to that direction receives the texture smeared into tall
// stripes. Receiver triangles are grouped by how squarely they face the
// projection: floors keep the full shadow, slopes fade, walls are skipped.
namespace wall_shadows {
struct Vec {float x,y,z;};
inline bool enabled=true;
inline unsigned filtered=0,culled=0,rebuilt=0; // diagnostics for tests

// Full strength up to ~35 degrees from the projection, gone past ~65 degrees.
constexpr float FullCosine=.82f,NoneCosine=.42f;
constexpr int Levels=4; // drawn strength levels: 1/4 .. 4/4

inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Normalize(Vec& v){
    float l=std::sqrt(Dot(v,v));if(!(l>1e-12f)||!std::isfinite(l))return false;
    v={v.x/l,v.y/l,v.z/l};return true;
}
// m maps model-space positions to shadow texture coordinates (row vectors:
// world * view * texture). The texture is constant along the direction that
// changes neither u nor v: the cross product of their position gradients.
inline bool Direction(const D3DMATRIX& m,Vec& d){
    d=Cross({m._11,m._21,m._31},{m._12,m._22,m._32});return Normalize(d);
}
inline float Strength(float cosine){
    float c=std::fabs(cosine);
    if(!(c>NoneCosine))return 0;if(c>=FullCosine)return 1;
    return (c-NoneCosine)/(FullCosine-NoneCosine);
}
// 0: skipped, Levels: full strength.
inline int Level(float strength){int l=int(strength*Levels+.5f);return l<0?0:l>Levels?Levels:l;}
inline Vec FaceNormal(Vec a,Vec b,Vec c){
    Vec n=Cross({b.x-a.x,b.y-a.y,b.z-a.z},{c.x-a.x,c.y-a.y,c.z-a.z});
    if(!Normalize(n))n={0,0,0};return n;
}
// Expands list/strip/fan topology into a triangle list with each triangle's
// front-face winding preserved, so the pass's culling state still applies.
template<class Get> bool Triangles(D3DPRIMITIVETYPE type,UINT count,Get index,std::vector<uint32_t>& out){
    out.clear();if(!count||count>2000000)return false;out.reserve(size_t(count)*3);
    for(UINT i=0;i<count;++i){
        uint32_t a,b,c;
        switch(type){
        case D3DPT_TRIANGLELIST:a=index(i*3);b=index(i*3+1);c=index(i*3+2);break;
        case D3DPT_TRIANGLESTRIP:a=index(i);b=index(i+1);c=index(i+2);if(i&1)std::swap(a,b);break;
        case D3DPT_TRIANGLEFAN:a=index(0);b=index(i+1);c=index(i+2);break;
        default:return false;
        }
        if(a==b||b==c||a==c)continue; // strip joins draw nothing
        out.push_back(a);out.push_back(b);out.push_back(c);
    }
    return true;
}
struct Plan {
    std::vector<uint32_t> indices;       // triangles ordered by strength, strongest first
    std::array<UINT,Levels+1> first{},count{}; // per level, in triangles
    UINT drawn=0;
    uint32_t low=0,high=0; // vertex range of the drawn triangles
};
inline void Group(const std::vector<uint32_t>& tris,const std::vector<Vec>& normals,Vec d,Plan& plan){
    plan.indices.clear();plan.first={};plan.count={};plan.drawn=0;
    std::vector<unsigned char> level(normals.size());
    for(size_t t=0;t<normals.size();++t){level[t]=static_cast<unsigned char>(Level(Strength(Dot(normals[t],d))));++plan.count[level[t]];}
    UINT at=0;for(int l=Levels;l>=1;--l){plan.first[l]=at;at+=plan.count[l];}
    plan.drawn=at;plan.indices.resize(size_t(at)*3);
    auto next=plan.first;
    for(size_t t=0;t<normals.size();++t)if(level[t]){
        UINT slot=next[level[t]]++;memcpy(&plan.indices[size_t(slot)*3],&tris[t*3],12);
    }
    plan.low=UINT_MAX;plan.high=0;for(uint32_t v:plan.indices){plan.low=std::min(plan.low,v);plan.high=std::max(plan.high,v);}
    if(plan.indices.empty())plan.low=0;
}

// ---- Device side -----------------------------------------------------------
struct BufferDesc {D3DFORMAT format;D3DRESOURCETYPE type;DWORD usage;D3DPOOL pool;UINT size;DWORD fvf;};
struct Key {
    void* vb;void* ib;UINT base,first,count,min,vertices,stride;D3DPRIMITIVETYPE type;
    bool operator==(const Key& o)const{return !memcmp(this,&o,sizeof(Key));}
};
struct KeyHash {size_t operator()(const Key& k)const{
    const unsigned char* p=reinterpret_cast<const unsigned char*>(&k);size_t h=2166136261u;
    for(size_t i=0;i<sizeof(Key);++i)h=(h^p[i])*16777619u;return h;}};
struct Entry {
    bool usable=false,planned=false;
    std::vector<uint32_t> tris;std::vector<Vec> normals;
    Vec d{};Plan plan;
    void* ib=nullptr;D3DFORMAT format=D3DFMT_INDEX16;UINT capacity=0;
};
inline std::unordered_map<Key,Entry,KeyHash> cache;
inline std::unordered_set<void*> watched; // source buffers referenced by cache keys
inline size_t cachedBytes=0;
constexpr size_t MaxEntries=16384,MaxBytes=96u<<20;

inline void ReleaseEntry(Entry& e){if(e.ib){void* ib=e.ib;e.ib=nullptr;D8<2>(ib);}}
inline void Clear(){
    std::vector<void*> owned;for(auto& [key,e]:cache)if(e.ib){owned.push_back(e.ib);e.ib=nullptr;}
    cache.clear();watched.clear();cachedBytes=0;
    for(void* ib:owned)D8<2>(ib);
}
// A freed level buffer's address can be reused by a different mesh.
inline void Forget(void* buffer){
    if(!watched.erase(buffer))return;
    std::vector<void*> owned;
    for(auto it=cache.begin();it!=cache.end();){
        if(it->first.vb==buffer||it->first.ib==buffer){if(it->second.ib)owned.push_back(it->second.ib);it=cache.erase(it);}else ++it;
    }
    for(void* ib:owned)D8<2>(ib);
}
using ReleaseFn=ULONG(__stdcall*)(void*);
inline std::unordered_map<void**,ReleaseFn> releases;
inline ULONG __stdcall BufferRelease(void* b){
    auto fn=releases.at(*static_cast<void***>(b));ULONG left=fn(b);
    if(!left&&!watched.empty())Forget(b);
    return left;
}
inline bool WatchTable(void* buffer){
    auto table=*static_cast<void***>(buffer);if(releases.count(table))return true;
    if(table[2]==reinterpret_cast<void*>(&BufferRelease))return false;
    DWORD old;if(!VirtualProtect(table+2,4,PAGE_READWRITE,&old))return false;
    releases.emplace(table,reinterpret_cast<ReleaseFn>(table[2]));table[2]=reinterpret_cast<void*>(&BufferRelease);
    DWORD ignored;VirtualProtect(table+2,4,old,&ignored);return true;
}
inline bool Readable(const BufferDesc& desc){
    return (desc.pool==D3DPOOL_MANAGED||desc.pool==D3DPOOL_SYSTEMMEM)&&!(desc.usage&D3DUSAGE_DYNAMIC);
}
struct ReadLock {
    void* buffer;const unsigned char* data=nullptr;
    explicit ReadLock(void* b):buffer(b){BYTE* p=nullptr;if(SUCCEEDED(D8<11>(b,UINT(0),UINT(0),&p,DWORD(D3DLOCK_READONLY))))data=p;}
    ~ReadLock(){if(data)D8<12>(buffer);}
};
// Reads the triangles and positions a draw uses. Any unsupported layout leaves
// the entry unusable, which draws the native pass unchanged.
inline bool Load(const Key& k,Entry& e){
    BufferDesc vd{};if(FAILED(D8<13>(k.vb,&vd))||!Readable(vd)||k.stride<12)return false;
    UINT vertexCount=vd.size/k.stride;
    {
        std::vector<uint32_t> tris;bool ok;
        if(k.ib){
            BufferDesc id{};if(FAILED(D8<13>(k.ib,&id))||!Readable(id))return false;
            UINT width=id.format==D3DFMT_INDEX16?2:id.format==D3DFMT_INDEX32?4:0;if(!width)return false;
            ReadLock lock(k.ib);if(!lock.data)return false;
            UINT total=id.size/width;bool inside=true;
            auto get=[&](UINT i)->uint32_t{UINT at=k.first+i;if(at>=total){inside=false;return 0;}
                if(width==2){uint16_t v;memcpy(&v,lock.data+size_t(at)*2,2);return v;}uint32_t v;memcpy(&v,lock.data+size_t(at)*4,4);return v;};
            ok=Triangles(k.type,k.count,get,tris)&&inside;e.format=id.format;
        }else{
            ok=Triangles(k.type,k.count,[&](UINT i){return uint32_t(k.first+i);},tris);
            uint32_t top=0;for(uint32_t v:tris)top=std::max(top,v);e.format=top<65536?D3DFMT_INDEX16:D3DFMT_INDEX32;
        }
        if(!ok)return false;e.tris=std::move(tris);
    }
    ReadLock lock(k.vb);if(!lock.data)return false;
    e.normals.resize(e.tris.size()/3);
    for(size_t t=0;t<e.normals.size();++t){
        Vec p[3];
        for(int j=0;j<3;++j){
            uint64_t v=uint64_t(e.tris[t*3+j])+(k.ib?k.base:0);if(v>=vertexCount)return false;
            memcpy(&p[j],lock.data+size_t(v)*k.stride,12);
        }
        e.normals[t]=FaceNormal(p[0],p[1],p[2]);
    }
    return true;
}
inline bool Upload(void* d,Entry& e){
    UINT width=e.format==D3DFMT_INDEX16?2:4;UINT bytes=UINT(e.plan.indices.size())*width;if(!bytes)return true;
    if(!e.ib||e.capacity<bytes){
        ReleaseEntry(e);e.capacity=0;
        if(FAILED(D8<24>(d,bytes,DWORD(D3DUSAGE_WRITEONLY),DWORD(e.format),D3DPOOL_MANAGED,&e.ib))||!e.ib){e.ib=nullptr;return false;}
        e.capacity=bytes;
    }
    BYTE* p=nullptr;if(FAILED(D8<11>(e.ib,UINT(0),bytes,&p,DWORD(0)))||!p)return false;
    if(width==4)memcpy(p,e.plan.indices.data(),bytes);
    else for(size_t i=0;i<e.plan.indices.size();++i){uint16_t v=uint16_t(e.plan.indices[i]);memcpy(p+i*2,&v,2);}
    D8<12>(e.ib);return true;
}
// Stage 0 of the native receiver pass: camera-space positions through the
// projected shadow texture matrix (ShadowRenderFast/Fallback, 0x6b76f0/0x6b7c70).
inline bool ReceiverPass(void* d){
    DWORD index=0,flags=0;
    return SUCCEEDED(D8<62>(d,DWORD(0),DWORD(D3DTSS_TEXCOORDINDEX),&index))&&(index&0xffff0000)==D3DTSS_TCI_CAMERASPACEPOSITION&&
           SUCCEEDED(D8<62>(d,DWORD(0),DWORD(D3DTSS_TEXTURETRANSFORMFLAGS),&flags))&&flags==(D3DTTFF_COUNT3|D3DTTFF_PROJECTED);
}
inline D3DMATRIX Multiply(const D3DMATRIX& a,const D3DMATRIX& b){
    D3DMATRIX out{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)out.m[i][j]+=a.m[i][k]*b.m[k][j];return out;
}
// Returns the grouped plan for this receiver draw, or nullptr to draw natively.
inline const Entry* Prepare(void* d,D3DPRIMITIVETYPE type,bool indexed,UINT min,UINT vertices,UINT first,UINT count){
    if(!enabled||!ReceiverPass(d))return nullptr;
    DWORD fvf=0;if(FAILED(D8<77>(d,&fvf))||(fvf&1)||(fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZ)return nullptr;
    D3DMATRIX world{},view{},texture{};
    if(FAILED(D8<38>(d,D3DTS_WORLD,&world))||FAILED(D8<38>(d,D3DTS_VIEW,&view))||FAILED(D8<38>(d,D3DTS_TEXTURE0,&texture)))return nullptr;
    Vec dir;if(!Direction(Multiply(Multiply(world,view),texture),dir))return nullptr;
    Key k{};k.type=type;k.first=first;k.count=count;
    void* vb=nullptr;if(FAILED(D8<84>(d,UINT(0),&vb,&k.stride))||!vb)return nullptr;k.vb=vb;D8<2>(vb);
    if(indexed){void* ib=nullptr;if(FAILED(D8<86>(d,&ib,&k.base))||!ib)return nullptr;k.ib=ib;D8<2>(ib);k.min=min;k.vertices=vertices;}
    auto it=cache.find(k);
    if(it==cache.end()){
        if(cache.size()>=MaxEntries||cachedBytes>=MaxBytes)Clear();
        if(!WatchTable(k.vb)||(k.ib&&!WatchTable(k.ib)))return nullptr;
        it=cache.emplace(k,Entry{}).first;watched.insert(k.vb);if(k.ib)watched.insert(k.ib);
        it->second.usable=Load(k,it->second);
        cachedBytes+=it->second.tris.size()*4+it->second.normals.size()*sizeof(Vec)+sizeof(Entry);
    }
    Entry& e=it->second;if(!e.usable)return nullptr;
    // Static level geometry keeps one projection; rebuild only when it turns.
    if(!e.planned||Dot(e.d,dir)<.9998f){
        Group(e.tris,e.normals,dir,e.plan);e.d=dir;
        if(!Upload(d,e)){e.usable=false;ReleaseEntry(e);return nullptr;}
        e.planned=true;++rebuilt;cachedBytes+=e.plan.indices.size()*4;
    }
    return &e;
}
using IndexedFn=HRESULT(__stdcall*)(void*,D3DPRIMITIVETYPE,UINT,UINT,UINT,UINT);
struct StageState {
    void* d;DWORD op,arg1,arg2,alphaOp,alphaArg,nextOp,factor;
    explicit StageState(void* device):d(device){
        D8<62>(d,DWORD(1),DWORD(D3DTSS_COLOROP),&op);D8<62>(d,DWORD(1),DWORD(D3DTSS_COLORARG1),&arg1);D8<62>(d,DWORD(1),DWORD(D3DTSS_COLORARG2),&arg2);
        D8<62>(d,DWORD(1),DWORD(D3DTSS_ALPHAOP),&alphaOp);D8<62>(d,DWORD(1),DWORD(D3DTSS_ALPHAARG1),&alphaArg);
        D8<62>(d,DWORD(2),DWORD(D3DTSS_COLOROP),&nextOp);D8<51>(d,D3DRS_TEXTUREFACTOR,&factor);
    }
    void Restore(){
        TS(d,1,D3DTSS_COLOROP,op);TS(d,1,D3DTSS_COLORARG1,arg1);TS(d,1,D3DTSS_COLORARG2,arg2);
        TS(d,1,D3DTSS_ALPHAOP,alphaOp);TS(d,1,D3DTSS_ALPHAARG1,alphaArg);TS(d,2,D3DTSS_COLOROP,nextOp);RS(d,D3DRS_TEXTUREFACTOR,factor);
    }
};
// Draws each strength group with the native pass state. Weaker groups blend
// the shadow texel toward white, which the pass's blend treats as "no shadow".
inline HRESULT Submit(void* d,const Entry& e,bool indexed,UINT min,UINT vertices,IndexedFn draw){
    const Plan& plan=e.plan;++filtered;culled+=plan.count[0];
    if(!plan.drawn)return D3D_OK;
    void* oldIb=nullptr;UINT oldBase=0;if(FAILED(D8<86>(d,&oldIb,&oldBase)))oldIb=nullptr;
    if(!indexed){
        // Former DrawPrimitive: the indices are absolute vertex numbers.
        min=plan.low;vertices=plan.high-plan.low+1;
    }
    D8<85>(d,e.ib,indexed?oldBase:UINT(0));
    HRESULT hr=D3D_OK;StageState saved(d);bool changed=false;
    for(int l=Levels;l>=1;--l){
        UINT tris=plan.count[l];if(!tris)continue;
        if(l<Levels){
            // Only fade when stage 1 is free; otherwise keep the stronger half.
            if(saved.op!=D3DTOP_DISABLE){if(l*2<Levels)continue;}
            else{
                if(!changed){TS(d,1,D3DTSS_COLOROP,D3DTOP_BLENDFACTORALPHA);TS(d,1,D3DTSS_COLORARG1,D3DTA_CURRENT);TS(d,1,D3DTSS_COLORARG2,D3DTA_TFACTOR);
                    TS(d,1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);TS(d,1,D3DTSS_ALPHAARG1,D3DTA_CURRENT);TS(d,2,D3DTSS_COLOROP,D3DTOP_DISABLE);changed=true;}
                RS(d,D3DRS_TEXTUREFACTOR,(DWORD(l*255/Levels)<<24)|0xffffff);
            }
        }
        HRESULT r=draw(d,D3DPT_TRIANGLELIST,min,vertices,plan.first[l]*3,tris);if(FAILED(r))hr=r;
    }
    if(changed)saved.Restore();
    D8<85>(d,oldIb,oldBase);if(oldIb)D8<2>(oldIb);
    return hr;
}
}
