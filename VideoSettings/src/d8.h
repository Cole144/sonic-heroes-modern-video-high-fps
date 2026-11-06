#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

// D3D8 COM slots and ABI verified against IDirect3DDevice8, not D3D9.
// D3D9 is used only for the shared enumerations and POD types below.
struct Present8 {
    UINT width,height; D3DFORMAT format; UINT count;
    D3DMULTISAMPLE_TYPE samples; D3DSWAPEFFECT swap;
    HWND window; BOOL windowed,depth; D3DFORMAT depthFormat;
    DWORD flags,refresh,interval;
};
static_assert(sizeof(Present8)==52,"Build Win32 only");
template<int I,class... A> HRESULT D8(void* object,A... args) {
    return reinterpret_cast<HRESULT(__stdcall*)(void*,A...)>((*reinterpret_cast<void***>(object))[I])(object,args...);
}
template<class T> T& Mem(uintptr_t p){return *reinterpret_cast<T*>(p);}
#ifdef VIDEO_TEST
inline void* testDevice=nullptr;
inline HWND testWindow=nullptr;
inline Present8 testPresent{};
inline void* Device(){return testDevice;}
inline Present8& Present(){return testPresent;}
inline HWND Window(){return testWindow;}
#else
inline void* Device(){return Mem<void*>(0x8e0be8);}
inline Present8& Present(){return Mem<Present8>(0xaa5140);}
inline HWND Window(){return Mem<HWND>(0x8e0be4);}
#endif
inline HRESULT RS(void* d,D3DRENDERSTATETYPE t,DWORD v){return D8<50>(d,t,v);}
inline HRESULT TS(void* d,DWORD stage,DWORD t,DWORD v){return D8<63>(d,stage,t,v);}
