#pragma once
#include <array>
#include <cstring>

namespace shadow_models {
constexpr int MaxFiles=64;
struct Plan {
    std::array<const char*,MaxFiles> names{};
    std::array<bool,MaxFiles> replaced{};
    std::array<int,MaxFiles> originalIndex{};
    int count=0;
};
inline Plan Select(const char* const* names,int count,bool enabled){
    Plan plan;
    if(!names||count<1||count>MaxFiles)return plan;
    for(int i=0;i<count;++i){
        plan.names[i]=names[i];
        if(!enabled||!names[i]||_strnicmp(names[i],"k_",2))continue;
        // Only substitute a model explicitly requested in this same native
        // character load. The loader reads it again, creating an independent
        // clump/geometry/material set with native lifetime and animation.
        for(int j=0;j<count;++j)if(names[j]&&!_stricmp(names[i]+2,names[j])){
            plan.names[i]=names[j];plan.replaced[i]=true;break;
        }
    }
    return plan;
}
inline void KeepOriginals(Plan& plan,const char* const* names,int count){
    plan.count=count;int extras=0;for(int i=0;i<count;++i)if(plan.replaced[i])++extras;
    if(count+extras>MaxFiles)return;
    for(int i=0;i<count;++i)if(plan.replaced[i]){
        plan.originalIndex[i]=plan.count;plan.names[plan.count++]=names[i];
    }
}
template<class T> T& Field(void* p,size_t offset){return *reinterpret_cast<T*>(static_cast<unsigned char*>(p)+offset);}
using SetTexture=void*(__cdecl*)(void*,void*);
using SetEffects=void*(__cdecl*)(void*,int);
inline void Silhouette(void* clump,SetTexture texture,SetEffects effects){
    if(!clump)return;
    auto end=static_cast<unsigned char*>(clump)+8;
    for(void* link=Field<void*>(clump,8);link!=end;link=Field<void*>(link,0)){
        void* atomic=static_cast<unsigned char*>(link)-0x40;
        void* geometry=Field<void*>(atomic,0x18);
        if(!geometry)continue;
        // Preserve skin, normals, topology and allocated UV storage. Match
        // the native shadow material: opaque black, lit/modulated, no texture
        // or vertex colour. This runs before first instancing/drawing.
        Field<unsigned>(geometry,8)=(Field<unsigned>(geometry,8)&~(4u|8u|128u))|0x60u;
        void** materials=Field<void**>(geometry,0x20);
        int count=Field<int>(geometry,0x24);
        for(int i=0;i<count;++i){
            void* material=materials[i];
            effects(material,0);texture(material,nullptr);
            Field<unsigned>(material,4)=0xff000000;
        }
    }
}
}
