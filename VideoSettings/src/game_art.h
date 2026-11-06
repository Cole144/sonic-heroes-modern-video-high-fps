#pragma once
#include "draw.h"
#include <fstream>
#include <stdexcept>
namespace gameart {
using Bytes=std::vector<unsigned char>;
inline uint32_t U32(const Bytes& b,size_t p){if(p+4>b.size())throw std::runtime_error("Asset bounds");uint32_t v;memcpy(&v,b.data()+p,4);return v;}
inline Bytes Prs(const Bytes& b,size_t start,size_t end){
    Bytes out;size_t p=start;unsigned bits=0,left=0;
    auto byte=[&](){if(p>=end||p>=b.size())throw std::runtime_error("PRS bounds");return unsigned(b[p++]);};
    auto bit=[&](){if(!left){bits=byte();left=8;}unsigned v=bits&1;bits>>=1;--left;return v;};
    for(;;){
        if(out.size()>16*1024*1024)throw std::runtime_error("PRS size");
        if(bit()){out.push_back(static_cast<unsigned char>(byte()));continue;}
        int offset;unsigned length;
        if(bit()){unsigned a=byte(),word=a|(byte()<<8);if(!word)break;offset=int(word>>3)-8192;length=word&7;length=length?length+2:byte()+1;}
        else{length=bit()<<1;length=(length|bit())+2;offset=int(byte())-256;}
        if(offset>=0||size_t(-offset)>out.size())throw std::runtime_error("PRS offset");
        for(unsigned i=0;i<length;i++)out.push_back(out[out.size()+offset]);
    }return out;
}
inline void* FindTexture(const Bytes& b,size_t start,size_t end,const char* name,int depth=0){
    if(depth>4||end>b.size())return nullptr;
    for(size_t p=start;p+12<=end;){
        uint32_t type=U32(b,p),size=U32(b,p+4);size_t next=p+12+size;if(next>end)return nullptr;
        if(type==22){if(auto result=FindTexture(b,p+12,next,name,depth+1))return result;}
        if(type==21&&size>=104){
            size_t d=p+24;
            if(!strncmp(reinterpret_cast<const char*>(b.data()+d+8),name,32)){
                uint32_t dims=U32(b,d+80),w=dims&65535,h=dims>>16,length=U32(b,d+88);
                if(!w||!h||w>4096||h>4096||length!=w*h*4||d+92+length>next)return nullptr;
                void* texture=nullptr;D3DLOCKED_RECT lock{};
                if(FAILED(D8<20>(ui::device,w,h,UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture)))return nullptr;
                if(FAILED(D8<16>(texture,UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)))){D8<2>(texture);return nullptr;}
                for(UINT y=0;y<h;y++)memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch,b.data()+d+92+y*w*4,w*4);
                D8<17>(texture,UINT(0));return texture;
            }
        }p=next;
    }return nullptr;
}
inline void* FromOne(const std::wstring& path,const char* texture){
    try{
        std::ifstream in(path,std::ios::binary|std::ios::ate);auto size=in.tellg();if(size<=0||size>32*1024*1024)return nullptr;
        Bytes b(static_cast<size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),size);if(!in)return nullptr;
        for(size_t p=24+U32(b,16);p+12<=b.size();){
            uint32_t index=U32(b,p),sizeData=U32(b,p+4);size_t end=p+12+sizeData;if(end>b.size()||24+size_t(index)*64+64>b.size())return nullptr;
            std::string name(reinterpret_cast<const char*>(b.data()+24+size_t(index)*64),64);
            if(name.find(".TXD")!=std::string::npos||name.find(".txd")!=std::string::npos){auto txd=Prs(b,p+12,end);if(auto result=FindTexture(txd,0,txd.size(),texture))return result;}
            p=end;
        }
    }catch(const std::exception&){}return nullptr;
}
}
