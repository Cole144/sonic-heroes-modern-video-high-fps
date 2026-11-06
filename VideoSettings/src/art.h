#pragma once
#include "draw.h"
#include "game_art.h"
#include <array>

namespace art {
inline ui::Sprite title,hex,litHex,header,videoWord;
inline void* idle=nullptr;
inline void* selected=nullptr;
inline void* audioTitle=nullptr;
inline void* vibrationTitle=nullptr;
inline void* banner=nullptr;
inline void* selectionGlow=nullptr;
inline std::array<void*,2> unsupportedAudio{};
inline ui::Sprite sonic,heroes;
inline std::vector<void*> nativeTitles;
inline std::vector<void*> foreground;
inline std::vector<void*> retained;
inline std::wstring titlePath;
inline bool triedTitle=false;
inline bool fixedOptions=false;
inline std::wstring promptPath;
inline std::array<void*,4> prompts{},helpTextures{};
inline int promptStyle=0;
inline bool triedPrompts=false;
inline void ClearNative(){for(void* t:retained)D8<2>(t);retained.clear();nativeTitles.clear();foreground.clear();hex={};litHex={};header={};sonic={};heroes={};idle=selected=audioTitle=vibrationTitle=banner=nullptr;helpTextures.fill(nullptr);selectionGlow=nullptr;unsupportedAudio.fill(nullptr);if(videoWord.texture)D8<2>(videoWord.texture);videoWord={};}
inline bool IsTitle(const char* name,bool fixed){return fixed?(!strcmp(name,"option_034")||!strcmp(name,"option_037")||!strcmp(name,"option_040")||!strcmp(name,"option_043")):(!strcmp(name,"option_028")||!strcmp(name,"option_034")||!strcmp(name,"option_037"));}
#ifndef VIDEO_TEST
inline void* __cdecl Texture(void* texture,void* data){
    const char* name=static_cast<const char*>(texture)+16;
    bool fixed=*static_cast<bool*>(data);
    bool isHex=!strcmp(name,"option_007"),isLitHex=!strcmp(name,"option_031"),isHeader=!strcmp(name,"option_001");
    bool isForeground=!strncmp(name,"option_",7)&&!isHeader;
    bool isIdle=!strcmp(name,fixed?"option_064":"option_046"),isSelected=!strcmp(name,fixed?"option_065":"option_047");
    bool isTitle=IsTitle(name,fixed);
    bool isBanner=!strcmp(name,fixed?"option_078":"option_040");
    bool isSonic=!strcmp(name,"bg_013"),isHeroes=!strcmp(name,"bg_015");
    const char* helpNames[]={"help_pc_002","help_xb_002","help_gc_002","help_ps_002"};int help=-1;for(int i=0;i<4;i++)if(!strcmp(name,helpNames[i]))help=i;
    if(!isForeground&&!isHeader&&!isSonic&&!isHeroes&&help<0)return texture;
    auto raster=*static_cast<char**>(texture);if(!raster)return texture;
    int offset=Mem<int>(0xaa503c);if(offset<0||offset>4096)return texture;
    void* native=*reinterpret_cast<void**>(raster+offset);if(!native)return texture;
    if(std::find(retained.begin(),retained.end(),native)==retained.end()){D8<1>(native);retained.push_back(native);}
    if(isHex){hex={native,15.f/256,11.f/256,125.f/256,138.f/256,125.f/138};fixedOptions=fixed;}
    if(isLitHex)litHex={native,15.f/256,11.f/256,125.f/256,138.f/256,125.f/138};
    if(isForeground&&std::find(foreground.begin(),foreground.end(),native)==foreground.end())foreground.push_back(native);
    if(isHeader)header={native,0,0,483.f/512,48.f/64,483.f/48};
    if(isIdle)idle=native;if(isSelected)selected=native;
    if(isBanner)banner=native;
    if(!strcmp(name,fixed?"option_046":"option_040"))selectionGlow=native;
    if(fixed&&!strcmp(name,"option_053"))unsupportedAudio[0]=native;
    if(fixed&&!strcmp(name,"option_062"))unsupportedAudio[1]=native;
    auto config=Mem<uintptr_t>(0xa777e4);bool english=config&&Mem<unsigned char>(config+0x13)==1;
    if(fixed&&english&&!strcmp(name,"option_034"))audioTitle=native;
    if(fixed&&english&&!strcmp(name,"option_037"))vibrationTitle=native;
    if(isSonic)sonic={native,0,0,268.f/512,58.f/64,268.f/58};if(isHeroes)heroes={native,25.f/512,0,345.f/512,58.f/64,345.f/58};
    if(help>=0)helpTextures[help]=native;
    if(isTitle&&std::find(nativeTitles.begin(),nativeTitles.end(),native)==nativeTitles.end())nativeTitles.push_back(native);
    return texture;
}
inline void* __cdecl Dictionary(void* dictionary,void*){
    using Visit=void*(__cdecl*)(void*,void*(__cdecl*)(void*,void*),void*);
    using Find=void*(__cdecl*)(void*,const char*);
    bool fixed=reinterpret_cast<Find>(0x64e3b0)(dictionary,"option_093")!=nullptr;
    reinterpret_cast<Visit>(0x64e0f0)(dictionary,&Texture,&fixed);return dictionary;
}
#endif
inline void Discover(){
#ifndef VIDEO_TEST
    using Visit=int(__cdecl*)(void*(__cdecl*)(void*,void*),void*);
    reinterpret_cast<Visit>(0x64e130)(&Dictionary,nullptr);
#endif
}
inline void LoadTitle(){if(!triedTitle){triedTitle=true;title=ui::LoadSprite(titlePath.c_str());}}
inline void LoadPrompts(){
    if(triedPrompts)return;triedPrompts=true;
    const char* names[]={"bar_pc_003","bar_xb_003","bar_gc_003","bar_ps_003"};
    for(int i=0;i<4;i++)prompts[i]=gameart::FromOne(promptPath,names[i]);
    if(!prompts[0])prompts[0]=gameart::FromOne(promptPath,"bar_003");
}
inline void ObservePrompt(void* texture){for(int i=0;i<4;i++)if(texture&&texture==helpTextures[i])promptStyle=i;}
inline void Prompts(float x,float y,bool keyboard=false){
    if(keyboard){
        ui::Text("Enter",x,y+4,17,0xff655019,false);ui::Text("Select",x+53,y+4,17,0xff655019,false);
        ui::Text("Esc",x+135,y+4,17,0xff655019,false);ui::Text("Back",x+172,y+4,17,0xff655019,false);return;
    }
    LoadPrompts();void* texture=prompts[promptStyle]?prompts[promptStyle]:prompts[0];
    if(texture){ui::Quad(x,y,229.5f,25.5f,0xffffffff,texture,0,0,306.f/512,34.f/64);ui::Quad(x+229.5f,y,9,25.5f,0xffffffff,texture,12.f/512,0,-12.f/512,34.f/64);}
}
inline bool MakeVideoWord(){
    if(videoWord.texture)return true;if(!audioTitle||!vibrationTitle)return false;
    D3DLOCKED_RECT audio{},vibration{};
    if(FAILED(D8<16>(audioTitle,UINT(0),&audio,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY))))return false;
    if(FAILED(D8<16>(vibrationTitle,UINT(0),&vibration,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY)))){D8<17>(audioTitle,UINT(0));return false;}
    // Isolate the white interiors, then outline the assembled word once. Cropping
    // rectangles directly from a connected title steals adjacent letter strokes.
    std::array<unsigned char,128*32> mask{};int cursor=3;
    auto glyph=[&](const D3DLOCKED_RECT& source,int left,int right){
        for(int y=0;y<32;y++){const auto pixels=reinterpret_cast<const DWORD*>(static_cast<const char*>(source.pBits)+y*source.Pitch);
            for(int x=left;x<=right;x++){DWORD p=pixels[x];mask[y*128+cursor+x-left]=static_cast<unsigned char>(std::min((p>>16)&255,(p>>8)&255));}}
        cursor+=right-left+3;
    };
    glyph(vibration,5,25);glyph(audio,67,72);glyph(audio,48,65);glyph(audio,128,144);glyph(audio,76,93);
    D8<17>(audioTitle,UINT(0));D8<17>(vibrationTitle,UINT(0));
    void* texture=nullptr;if(FAILED(D8<20>(ui::device,UINT(128),UINT(32),UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture)))return false;
    D3DLOCKED_RECT output{};if(FAILED(D8<16>(texture,UINT(0),&output,static_cast<const RECT*>(nullptr),DWORD(0)))){D8<2>(texture);return false;}
    for(int y=0;y<32;y++){auto pixels=reinterpret_cast<DWORD*>(static_cast<char*>(output.pBits)+y*output.Pitch);
        for(int x=0;x<128;x++){unsigned alpha=0;for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++)if(dx*dx+dy*dy<=11&&x+dx>=0&&x+dx<128&&y+dy>=0&&y+dy<32)alpha=std::max(alpha,unsigned(mask[(y+dy)*128+x+dx]));
            unsigned white=mask[y*128+x];unsigned intensity=alpha?std::min(255u,white*255/alpha):0;
            pixels[x]=(alpha<<24)|(intensity<<16)|(intensity<<8)|255;
        }
    }
    D8<17>(texture,UINT(0));videoWord={texture,0,0,float(cursor+1)/128,1,float(cursor+1)/32};return true;
}
inline void Title(float x,float y,float height){
    if(MakeVideoWord()){
        float f=height/32;ui::Image(videoWord,x,y,height*videoWord.aspect,height);
        x+=height*videoWord.aspect+6*f;
        ui::Quad(x,y,135*f,height,0xffffffff,audioTitle,101.f/512,0,135.f/512,1);return;
    }
    if(title.texture)ui::Image(title,x,y,height*title.aspect,height);
    else ui::Text("Video Settings",x,y,height);
}
inline void TV(float x,float y,float scale=1,float hover=0,float phase=0){
    if(hex.texture)ui::Image(hex,x-55*scale,y-62*scale,125*scale,138*scale);
    else {ui::Hex(x+3*scale,y+5*scale,64*scale,0x70000000);ui::Hex(x,y,64*scale,0xffff9a38);ui::Hex(x,y,56*scale,0xffffedb0);}
    if(hover>0&&litHex.texture){DWORD alpha=DWORD((215+40*sinf(phase*6.2831853f))*hover);ui::Image(litHex,x-55*scale,y-62*scale,125*scale,138*scale,(alpha<<24)|0xffffff);}
    ui::Monitor(x,y-4*scale,61*scale,hover>0?phase:-1,hover);
}
inline void Bar(float x,float y,float w,float h,bool active){
    void* texture=active?selected:idle;
    // Reuse blank edge strips of the game's ON/OFF bars; their authored words
    // are never drawn. Native textures remain owned by the game and are not shipped.
    if(texture){
        ui::Quad(x,y,5,h,0xffffffff,texture,2.f/256,1.f/64,5.f/256,33.f/64);
        ui::Quad(x+5,y,w-10,h,0xffffffff,texture,12.f/256,1.f/64,4.f/256,33.f/64);
        ui::Quad(x+w-5,y,5,h,0xffffffff,texture,150.f/256,1.f/64,5.f/256,33.f/64);
    }else{
        ui::Quad(x,y,w,h,0xff0c11cb);ui::Quad(x+3,y+3,w-6,h-6,0xff030c22);
        if(active)ui::Quad(x+4,y+h-9,w-8,5,0xff02d6ed);
    }
}
inline void Banner(float x,float y,float w,float h,const std::string& label,bool active=true){
    if(banner){
        DWORD tint=active?0xffffffff:0xffd5d0b0;float end=h*1.5f;
        ui::Quad(x,y,end,h,tint,banner,0,0,48.f/256,1);
        ui::Quad(x+end,y,w-end*2,h,tint,banner,40.f/256,0,5.f/256,1);
        ui::Quad(x+w-end,y,end,h,tint,banner,145.f/256,0,49.f/256,1);
        ui::Text(label,x+(w-ui::TextWidth(label,h*.8f))*.5f,y+2,h*.8f,0xffffffff,false);return;
    }
    ui::Quad(x+18,y,w-18,h,active?0xffef3b00:0xffbd620d);
    ui::Triangle(x,y+h,x+18,y,x+18,y+h,active?0xffef3b00:0xffbd620d);
    ui::Triangle(x+w,y,x+w+19,y,x+w,y+h,active?0xffef3b00:0xffbd620d);
    ui::Quad(x+22,y+3,w-26,9,active?0xffffb600:0xffdda937);
    ui::Text(label,x+(w-ui::TextWidth(label,h*.71f))*.5f+8,y+3,h*.71f,active?0xffffffff:0xffffe9b7,false);
}
}
