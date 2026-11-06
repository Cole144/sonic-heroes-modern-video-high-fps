#pragma once
#include "enemy_shadows.h"
namespace object_shadows {
using enemy_shadows::Vec;
struct Contact {void* actor=nullptr;Vec origin{},ground{},degrees{};ULONGLONG checked=0;bool valid=false;};
inline std::array<Contact,64> contacts{};
inline size_t next=0;
inline bool installed=false,suppressBlob=false;
inline unsigned ringDraws=0,signalDraws=0;
inline void Clear(){contacts={};next=0;}
inline bool Reusable(const Contact& c,void* actor,Vec origin,ULONGLONG now){
    return c.actor==actor&&now>=c.checked&&now-c.checked<1000&&fabsf(c.origin.x-origin.x)<.01f&&fabsf(c.origin.z-origin.z)<.01f&&fabsf(c.origin.y-origin.y)<8.f;
}
inline float Height(Vec* origin,int* angles){
#ifndef VIDEO_TEST
    float result;
    __asm {
        push edi
        mov edi,angles
        mov ecx,origin
        xor edx,edx
        mov eax,4440f0h
        call eax
        fstp result
        pop edi
    }
    return result;
#else
    (void)origin;(void)angles;return -1000000.f;
#endif
}
inline Contact& Ground(void* actor,Vec origin){
    ULONGLONG now=GetTickCount64();Contact* slot=nullptr;
    for(auto& c:contacts)if(c.actor==actor){if(Reusable(c,actor,origin,now))return c;slot=&c;break;}
    if(!slot)slot=&contacts[next++%contacts.size()];
    *slot={};slot->actor=actor;slot->origin=origin;slot->checked=now;
    int angle[3]{};float height=Height(&origin,angle);
    slot->valid=std::isfinite(height)&&height>-999999.f&&height<=origin.y+1.f&&origin.y-height<249.f;
    slot->ground={origin.x,height,origin.z};
    slot->degrees={float(angle[0]&65535)*360.f/65536,0,float(angle[2]&65535)*360.f/65536};
    return *slot;
}
}
