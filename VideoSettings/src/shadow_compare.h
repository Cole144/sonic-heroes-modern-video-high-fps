#pragma once
#include <unordered_map>
#include <vector>
#include <utility>

namespace shadow_compare {
struct RasterApi {
    void*(__cdecl* create)(int,int,int,int)=reinterpret_cast<void*(__cdecl*)(int,int,int,int)>(0x6520e0);
    void*(__cdecl* sub)(void*,void*,const RECT*)=reinterpret_cast<void*(__cdecl*)(void*,void*,const RECT*)>(0x652080);
    void*(__cdecl* destroy)(void*)=reinterpret_cast<void*(__cdecl*)(void*)>(0x651ed0);
};
inline RasterApi api;
struct Rasters {void* root=nullptr;void* sub=nullptr;};
struct Caster {
    Rasters inactive;
    std::vector<std::pair<void*,void*>> models;
    void* saved[2]{};
};
inline std::unordered_map<void*,Caster> casters;
inline Rasters sharedInactive;
inline bool installed=false,original=false,keyDown=false,drawing=false;
inline ULONGLONG noticeUntil=0;
inline const char* notice="";
inline void Notify(const char* text){notice=text;noticeUntil=GetTickCount64()+1800;}
inline void Destroy(Rasters& rasters){
    if(rasters.sub)api.destroy(rasters.sub);if(rasters.root)api.destroy(rasters.root);rasters={};
}
inline bool Create(Rasters& rasters,int size){
    if(rasters.root&&rasters.sub)return true;
    rasters.root=api.create(size,size,32,0x505);if(!rasters.root)return false;
    rasters.sub=api.create(0,0,0,5);
    // RwRect is x,y,width,height, unlike a Win32 RECT's right/bottom.
    RECT rect{size/128,size/128,size-size/64,size-size/64};
    if(!rasters.sub||!api.sub(rasters.sub,rasters.root,&rect)){Destroy(rasters);return false;}
    return true;
}
inline void Swap(Rasters& inactive,void*& root,void*& sub){std::swap(inactive.root,root);std::swap(inactive.sub,sub);}
inline void __stdcall Remove(void* effect){
    auto it=casters.find(effect);if(it==casters.end())return;
    Destroy(it->second.inactive);casters.erase(it);
}
inline void End(){Destroy(sharedInactive);original=false;noticeUntil=0;}
inline void Register(void* player,const shadow_models::Plan& plan,int count,void* list){
    void* effect=static_cast<unsigned char*>(player)+0x928;
    Remove(effect);Caster caster;
    for(int i=0;i<count;++i)if(plan.originalIndex[i]){
        auto full=shadow_models::Field<void*>(list,size_t(i)*8+4);
        auto low=shadow_models::Field<void*>(list,size_t(plan.originalIndex[i])*8+4);
        if(full&&low)caster.models.emplace_back(full,low);
    }
    if(!caster.models.empty())casters.emplace(effect,std::move(caster));
}
inline void Bind(void* effect){
    using shadow_models::Field;
    auto set=reinterpret_cast<void*(__cdecl*)(void*,void*)>(0x64df90);
    int slot=Field<signed char>(effect,1);
    if(slot>=0&&slot<3){void* texture=Mem<void*>(0xa7532c+slot*4);if(texture)set(texture,Field<void*>(effect,0x20));}
    if(void* texture=Field<void*>(effect,0x28))set(texture,Field<void*>(effect,0x20));
}
// The alternate low-poly clump borrows the currently animated skin hierarchy
// only during the mask draw. The game continues updating its primary clump.
inline void PrepareModels(){
    using shadow_models::Field;
    for(auto& entry:casters)for(int part=0;part<2;++part){
        void* effect=entry.first;auto& c=entry.second;
        void*& current=Field<void*>(effect,0x30+part*4);c.saved[part]=current;
        if(!original||!current)continue;
        for(auto pair:c.models)if(pair.first==current){
            void* hierarchy=Field<void*>(effect,0x38+part*4);if(!hierarchy)break;
            reinterpret_cast<void*(__cdecl*)(void*,void*,void*)>(0x66b540)(pair.second,reinterpret_cast<void*>(0x5a22a0),hierarchy);
            void* frame=Field<void*>(pair.second,4);void* sourceFrame=Field<void*>(current,4);
            auto matrix=reinterpret_cast<void*(__cdecl*)(void*)>(0x64c300)(sourceFrame);
            reinterpret_cast<void*(__cdecl*)(void*,void*,int)>(0x64c760)(frame,matrix,0);
            current=pair.second;break;
        }
    }
}
inline void RestoreModels(){for(auto& entry:casters)for(int part=0;part<2;++part)shadow_models::Field<void*>(entry.first,0x30+part*4)=entry.second.saved[part];}
}
