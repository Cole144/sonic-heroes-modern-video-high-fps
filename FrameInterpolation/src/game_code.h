#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

// Supported executable = the PC Tsonic_win.exe whose .text section (all game
// code) matches the build these patches were written for. Copies that differ
// only in headers or data (4GB patch, recoloured menus) pass; anything with
// different code is rejected. Each patch site is still byte-checked before use.
namespace game_code {
inline constexpr uint32_t TextSize=0x31c000;
inline constexpr unsigned char TextSha256[32]={0x4a,0xba,0xe8,0x44,0xb9,0xc1,0x3d,0xe5,0xe7,0xb0,0x4e,0xe7,0x33,0x39,0x8b,0x7f,0xbc,0xf3,0xa9,0x34,0x74,0xad,0x42,0xdc,0xce,0x35,0xce,0xb9,0x0b,0x44,0x04,0x4a};

inline bool SupportedFile(std::vector<unsigned char>& file){
    if(file.size()<0x400||file[0]!='M'||file[1]!='Z')return false;
    uint32_t pe=0;memcpy(&pe,&file[0x3c],4);
    if(pe>file.size()-24||memcmp(&file[pe],"PE\0\0",4))return false;
    uint16_t count=0,optional=0;memcpy(&count,&file[pe+6],2);memcpy(&optional,&file[pe+20],2);
    size_t table=size_t(pe)+24+optional;
    for(size_t i=0;i<count;i++){
        size_t entry=table+i*40;if(entry+40>file.size())return false;
        if(memcmp(&file[entry],".text\0\0\0",8))continue;
        uint32_t size=0,offset=0;memcpy(&size,&file[entry+16],4);memcpy(&offset,&file[entry+20],4);
        if(size!=TextSize||offset>file.size()||size>file.size()-offset)return false;
        unsigned char digest[32];
        if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,&file[offset],size,digest,32)<0)return false;
        return !memcmp(digest,TextSha256,32);
    }
    return false;
}

inline bool Supported(){
    wchar_t path[MAX_PATH];if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return false;
    std::ifstream in(path,std::ios::binary);std::vector<unsigned char> file((std::istreambuf_iterator<char>(in)),{});
    return SupportedFile(file);
}
}
