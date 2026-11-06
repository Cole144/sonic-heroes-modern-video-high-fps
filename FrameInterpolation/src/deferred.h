#pragma once
// Included inside fi, after Replay. GPU-visible reads or resource mutation are
// barriers: restore the retained native prefix before allowing the operation.
using SurfaceLockFn=HRESULT(__stdcall*)(void*,D3DLOCKED_RECT*,const RECT*,DWORD);
std::unordered_map<void**,SurfaceLockFn> surfaceLocks;
HRESULT __stdcall SurfaceLock(void* surface,D3DLOCKED_RECT* data,const RECT* rect,DWORD flags){
    if(!replaying&&!FlushDeferred(timing::BarrierReadback))return D3DERR_INVALIDCALL;
    return surfaceLocks.at(*static_cast<void***>(surface))(surface,data,rect,flags);
}
bool ObserveSurface(void* surface){
    if(!surface)return false;auto table=*static_cast<void***>(surface);
    if(surfaceLocks.count(table))return table[9]==reinterpret_cast<void*>(&SurfaceLock);
    auto old=reinterpret_cast<SurfaceLockFn>(table[9]);void* next=reinterpret_cast<void*>(&SurfaceLock);
    if(!Write(table+9,&next,4))return false;surfaceLocks.emplace(table,old);return true;
}
bool ArmDeferred(const Command& c){
    if(c.kind!=Command::Clear||!c.clearRects.empty()||!(c.flags&D3DCLEAR_TARGET)||(c.depth&&!(c.flags&D3DCLEAR_ZBUFFER)))return false;
    SurfaceDesc8 desc{};if(FAILED(D8<8>(c.target,&desc))||c.viewport.X||c.viewport.Y||c.viewport.Width!=desc.width||c.viewport.Height!=desc.height)return false;
    void* back=nullptr;bool ok=SUCCEEDED(D8<16>(device,UINT(0),D3DBACKBUFFER_TYPE_MONO,&back))&&back==c.target&&ObserveSurface(back);
    Release(back);if(!ok)return false;
    // Gameplay renders shadow textures before clearing the backbuffer. Those
    // native prepasses may remain submitted, while later main-scene draws are
    // deferred. Replaying the prefix must be independent of its previous GPU
    // contents, including depth/stencil shared by multiple shadow targets.
    if(!Replayable(current))return false;
    std::unordered_set<void*> depthReady,stencilReady;
    for(const auto& p:current.commands){const auto& prefix=*p;
        if(prefix.kind==Command::Copy)return false;
        if(prefix.kind==Command::Clear&&prefix.depth&&prefix.clearRects.empty()){
            SurfaceDesc8 depthDesc{};
            if(SUCCEEDED(D8<8>(prefix.depth,&depthDesc))&&!prefix.viewport.X&&!prefix.viewport.Y&&prefix.viewport.Width==depthDesc.width&&prefix.viewport.Height==depthDesc.height){
                if(prefix.flags&D3DCLEAR_ZBUFFER)depthReady.insert(prefix.depth);
                if(prefix.flags&D3DCLEAR_STENCIL)stencilReady.insert(prefix.depth);
            }
        }else if(prefix.kind==Command::Draw&&prefix.depth){
            if(!depthReady.count(prefix.depth))return false;
            if(prefix.gpu.rs[D3DRS_STENCILENABLE]&&!stencilReady.count(prefix.depth))return false;
        }
    }
    return true;
}
bool FlushDeferred(uint32_t reason){
    if(replaying)return true;
    if(deferArmed||deferredPending)deferCandidate=false;deferArmed=false;
    if(!deferredPending)return true;
    deferredPending=false;timing::frame.reasons|=timing::DeferredBarrier|reason;
    const bool resume=nativeScene;replaying=true;
    bool ok=!resume||SUCCEEDED(driverEnd(device));
    if(ok)ok=Replay(current,nullptr,1);
    if(resume)ok=SUCCEEDED(driverBegin(device))&&ok;
    replaying=false;
    if(!ok){capturing=false;current.valid=false;timing::frame.reasons|=timing::Failed;counters.status=DeviceLost;}
    return ok;
}
SceneFn nativeBegin=nullptr,nativeEnd=nullptr;
HRESULT __stdcall BeginScene(void* d){auto hr=nativeBegin(d);if(SUCCEEDED(hr)&&!replaying)nativeScene=true;return hr;}
HRESULT __stdcall EndScene(void* d){auto hr=nativeEnd(d);if(SUCCEEDED(hr)&&!replaying)nativeScene=false;return hr;}
using TargetFn=HRESULT(__stdcall*)(void*,void*,void*);TargetFn nativeTarget=nullptr;
HRESULT __stdcall SetTarget(void* d,void* target,void* depth){
    // RenderWare redundantly binds the existing framebuffer. That changes no
    // image dependency; retain its normal driver/state effects without flushing.
    bool same=deferredPending&&!current.commands.empty()&&current.commands.back()->target==target&&current.commands.back()->depth==depth;
    if(!same&&!replaying&&!FlushDeferred(timing::BarrierTarget))return D3DERR_INVALIDCALL;return nativeTarget(d,target,depth);
}
using FrontFn=HRESULT(__stdcall*)(void*,void*);FrontFn nativeFront=nullptr;
HRESULT __stdcall FrontBuffer(void* d,void* target){if(!replaying&&!FlushDeferred(timing::BarrierReadback))return D3DERR_INVALIDCALL;return nativeFront(d,target);}
bool InstallDeferred(void** table){
    return Slot(table,34,&BeginScene,nativeBegin)&&Slot(table,35,&EndScene,nativeEnd)&&Slot(table,31,&SetTarget,nativeTarget)&&Slot(table,30,&FrontBuffer,nativeFront); // 27 is CreateImageSurface
}
