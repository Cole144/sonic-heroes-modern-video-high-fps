#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace shadow_quality {
struct Preset {uint32_t size,border,interior;float blur;};
inline Preset Quality(int quality){
    uint32_t scale=1u<<std::clamp(quality,0,2);
    // The native blur is in normalized UV space, not texels. Preserve its
    // footprint: shrinking it exposes the deliberately coarse shadow meshes.
    return {256*scale,2*scale,252*scale,.025f};
}
struct Site {uintptr_t address;uint32_t original;};
// Native shadow-camera depth size; raster dimensions; normalized subraster
// border; four blur-quad coordinates; private blur constant operand.
inline constexpr std::array<Site,9> sites{{
    {0x7476cc,256},{0x63bf6c,256},{0x63bf81,2},{0x63bf90,252},
    {0x63b07b,0x43800000},{0x63b083,0x43800000},
    {0x63b093,0x43800000},{0x63b09b,0x43800000},{0x63b138,0x745a78}
}};
inline float blurStep=.025f;
inline int requested=0,active=0,maxQuality=2;
inline bool compatible=false;
// The retail passes inset both ends of UVs, producing scaled silhouettes.
// Enhanced models instead use four centered, subpixel-offset copies at the
// same scale. Keep native blend weights and quad positions unchanged.
template<class Vertex> bool FilterQuad(const Vertex* source,Vertex* copy,int count,int quality){
    if(!source||!copy||count!=4||quality<=0)return false;
    const float marker=source[0].u;
    const int tap=marker>.09375f?0:marker>.03125f?1:marker>-.03125f?2:3;
    float radius=.75f/float(Quality(quality).size);
    float du=(tap&1)?radius:-radius,dv=(tap&2)?radius:-radius;
    for(int i=0;i<4;++i){copy[i]=source[i];copy[i].u=((i&2)?1.f:0.f)+du;copy[i].v=((i&1)?1.f:0.f)+dv;}
    return true;
}
inline std::array<uint32_t,9> expected=[](){std::array<uint32_t,9> values{};for(size_t i=0;i<sites.size();++i)values[i]=sites[i].original;return values;}();
inline std::array<uint32_t,9> Values(int quality,uint32_t blurAddress){
    auto p=Quality(quality);float extent=float(p.size);uint32_t bits;memcpy(&bits,&extent,4);
    return {p.size,p.size,p.border,p.interior,bits,bits,bits,bits,blurAddress};
}
// Validate all owned locations before any change. Called only at the beginning
// of native shadow initialization, never while a level's rasters are in use.
template<class Read,class Write> bool Apply(int quality,uint32_t blurAddress,Read read,Write write){
    for(size_t i=0;i<sites.size();++i)if(read(sites[i].address)!=expected[i])return false;
    auto values=Values(quality,blurAddress);size_t changed=0;
    for(;changed<sites.size();++changed)if(!write(sites[changed].address,values[changed]))break;
    if(changed!=sites.size()){++changed;while(changed){--changed;write(sites[changed].address,expected[changed]);}return false;}
    expected=values;blurStep=Quality(quality).blur;active=quality;return true;
}
}
