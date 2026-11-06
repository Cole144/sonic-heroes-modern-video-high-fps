#pragma once
#include "d8.h"
#include <vector>
#include <string>
#include <cmath>
#include <gdiplus.h>

namespace ui {
struct Vertex{float x,y,z,rhw;DWORD color;float u,v;};
inline void* device=nullptr;
inline void* atlas=nullptr;
inline float glyphWidth[96]{};
inline float glyphU[96]{},glyphV[96]{},fontHeight=60;
inline bool nativeFont=false;
inline std::wstring fontPath;
inline float sx=1,sy=1,ox=0,oy=0;
inline float opacity=1,translateX=0;
struct NativeTransform {bool enabled=false;float ax=1,ay=0,bx=0,by=1,x=0,y=0,pivotX=0,pivotY=0;DWORD tint=0xffffffff;};
inline NativeTransform nativeTransform;
inline std::vector<Vertex> vertices;
inline void* batchTexture=nullptr;
inline DWORD block=0;
inline bool drawing=false;
struct Sprite {
    void* texture=nullptr;
    float u=0,v=0,uw=1,vh=1,aspect=1;
};
inline Sprite LoadSprite(const wchar_t* path,bool trim=true){
    static ULONG_PTR token=0;
    if(!token){Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return {};}
    Gdiplus::Bitmap bitmap(path);if(bitmap.GetLastStatus()!=Gdiplus::Ok)return {};
    const int w=int(bitmap.GetWidth()),h=int(bitmap.GetHeight());if(w<=0||h<=0||w>8192||h>8192)return {};
    Gdiplus::Rect rect(0,0,w,h);Gdiplus::BitmapData data{};
    if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok)return {};
    int left=w,top=h,right=0,bottom=0;
    for(int y=0;y<h;y++){auto src=reinterpret_cast<const DWORD*>(static_cast<const char*>(data.Scan0)+y*data.Stride);for(int x=0;x<w;x++)if((src[x]>>24)>32){left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1);}}
    if(!trim){left=top=0;right=w;bottom=h;}
    Sprite result;
    if(right>left&&bottom>top){
        UINT tw=1,th=1;while(tw<UINT(right-left))tw*=2;while(th<UINT(bottom-top))th*=2;
        if(SUCCEEDED(D8<20>(device,tw,th,UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&result.texture))){
            D3DLOCKED_RECT lock{};
            if(SUCCEEDED(D8<16>(result.texture,UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)))){
                for(UINT y=0;y<th;y++){
                    auto dst=static_cast<char*>(lock.pBits)+y*lock.Pitch;memset(dst,0,tw*4);
                    if(y<UINT(bottom-top))memcpy(dst,static_cast<const char*>(data.Scan0)+(top+int(y))*data.Stride+left*4,(right-left)*4);
                }
                D8<17>(result.texture,UINT(0));result.uw=float(right-left)/tw;result.vh=float(bottom-top)/th;result.aspect=float(right-left)/(bottom-top);
            }else{D8<2>(result.texture);result.texture=nullptr;}
        }
    }
    bitmap.UnlockBits(&data);return result;
}
inline void Flush(){
    if(vertices.empty())return;
    D8<61>(device,DWORD(0),batchTexture);
    TS(device,0,1,batchTexture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
    TS(device,0,4,batchTexture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
    D8<72>(device,D3DPT_TRIANGLELIST,UINT(vertices.size()/3),static_cast<const void*>(vertices.data()),UINT(sizeof(Vertex)));
    vertices.clear();
}
inline void Texture(void* t){if(batchTexture!=t){Flush();batchTexture=t;}}
inline Vertex V(float x,float y,DWORD color,float u=0,float v=0){
    color=(color&0xffffff)|(DWORD(float(color>>24)*opacity)<<24);
    if(nativeTransform.enabled){
        auto& t=nativeTransform;DWORD result=0;for(int shift=0;shift<32;shift+=8)result|=((((color>>shift)&255)*((t.tint>>shift)&255)/255)<<shift);
        x-=t.pivotX;y-=t.pivotY;return {t.x+x*t.ax+y*t.bx,t.y+x*t.ay+y*t.by,0,1,result,u,v};
    }
    return {(x+translateX)*sx+ox-.5f,y*sy+oy-.5f,0,1,color,u,v};
}
inline void Quad(float x,float y,float w,float h,DWORD color,void* tex=nullptr,float u=0,float v=0,float uw=1,float vh=1){
    Texture(tex);Vertex a=V(x,y,color,u,v),b=V(x+w,y,color,u+uw,v),c=V(x+w,y+h,color,u+uw,v+vh),d=V(x,y+h,color,u,v+vh);
    vertices.insert(vertices.end(),{a,b,c,a,c,d});
}
inline void Image(const Sprite& image,float x,float y,float w,float h,DWORD tint=0xffffffff){if(image.texture)Quad(x,y,w,h,tint,image.texture,image.u,image.v,image.uw,image.vh);}
inline void Triangle(float ax,float ay,float bx,float by,float cx,float cy,DWORD c){Texture(nullptr);vertices.insert(vertices.end(),{V(ax,ay,c),V(bx,by,c),V(cx,cy,c)});}
inline void Hex(float x,float y,float r,DWORD c){
    for(int i=0;i<6;i++) {float a=(float(i)*60-90)*.0174532925f,b=(float(i+1)*60-90)*.0174532925f;
        Triangle(x,y,x+std::cos(a)*r,y+std::sin(a)*r,x+std::cos(b)*r,y+std::sin(b)*r,c);}
}
inline void Circle(float x,float y,float r,DWORD c){for(int i=0;i<64;i++){float a=float(i)*.0981747704f,b=float(i+1)*.0981747704f;Triangle(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r,y+sinf(b)*r,c);}}
inline bool MakeAtlas(){
    if(atlas)return true;
    if(!fontPath.empty()){
        Sprite source=LoadSprite(fontPath.c_str(),false);
        if(source.texture){
            Gdiplus::Bitmap bitmap(fontPath.c_str());int character=0;
            for(UINT y=0;y<bitmap.GetHeight()&&character<91;y++){
                Gdiplus::Color color;bitmap.GetPixel(0,y,&color);
                if(color.GetR()<200||color.GetG()>100)continue;
                int left=0;
                for(UINT x=1;x<bitmap.GetWidth()&&character<91;x++){
                    bitmap.GetPixel(x,y,&color);if(color.GetR()<200||color.GetG()>100)continue;
                    int width=int(x)-left;if(width<=1)break;
                    glyphU[character]=float(left+1)/256;glyphV[character]=float(y+1)/256;glyphWidth[character]=float(width-1);
                    ++character;left=int(x);
                }
            }
            if(character==91){
                D3DLOCKED_RECT lock{};
                if(SUCCEEDED(D8<16>(source.texture,UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)))){
                    for(int y=0;y<256;y++){auto pixels=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);for(int x=0;x<256;x++){DWORD p=pixels[x];DWORD alpha=std::min((p>>16)&255,std::min((p>>8)&255,p&255));pixels[x]=(alpha<<24)|0xffffff;}}
                    D8<17>(source.texture,UINT(0));atlas=source.texture;fontHeight=25;nativeFont=true;return true;
                }
            }
            D8<2>(source.texture);
        }
    }
    constexpr int W=2048,H=512;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=W;info.bmiHeader.biHeight=-H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap){DeleteDC(dc);return false;}
    HGDIOBJ oldBitmap=SelectObject(dc,bitmap);
    memset(pixels,0,W*H*4);
    HFONT font=CreateFontW(-60,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");
    HGDIOBJ oldFont=SelectObject(dc,font);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,255));
    for(int i=0;i<96;i++){char c=char(i+32);SIZE size{};GetTextExtentPoint32A(dc,&c,1,&size);glyphWidth[i]=float(size.cx);TextOutA(dc,(i%16)*128,(i/16)*80,&c,1);}
    GdiFlush();
    HRESULT hr=D8<20>(device,UINT(W),UINT(H),UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&atlas);
    D3DLOCKED_RECT lock{};
    if(SUCCEEDED(hr)&&SUCCEEDED(D8<16>(atlas,UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)))) {
        for(int y=0;y<H;y++){auto dst=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);auto src=static_cast<DWORD*>(pixels)+y*W;for(int x=0;x<W;x++)dst[x]=((src[x]&255)<<24)|0xffffff;}
        D8<17>(atlas,UINT(0));
    }else{if(atlas)D8<2>(atlas);atlas=nullptr;}
    SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);return atlas!=nullptr;
}
inline bool Begin(void* d,int width,int height,int scale=100){
    device=d;drawing=true;opacity=1;translateX=0;nativeTransform={};
    if(!MakeAtlas()||FAILED(D8<57>(d,D3DSBT_ALL,&block))){drawing=false;return false;}
    D3DVIEWPORT9 viewport{0,0,UINT(width),UINT(height),0,1};D8<40>(d,&viewport);
    float f=std::min(width/960.0f,height/540.0f)*scale/100.0f;sx=sy=f;ox=(width-960*f)*.5f;oy=(height-540*f)*.5f;
    D8<88>(d,DWORD(0));D8<76>(d,DWORD(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));
    RS(d,D3DRS_ZENABLE,FALSE);RS(d,D3DRS_ZWRITEENABLE,FALSE);RS(d,D3DRS_ALPHATESTENABLE,FALSE);RS(d,D3DRS_CULLMODE,D3DCULL_NONE);
    RS(d,D3DRS_LIGHTING,FALSE);RS(d,D3DRS_FOGENABLE,FALSE);RS(d,D3DRS_STENCILENABLE,FALSE);RS(d,D3DRS_CLIPPING,FALSE);
    RS(d,D3DRS_ALPHABLENDENABLE,TRUE);RS(d,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);RS(d,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);RS(d,D3DRS_BLENDOP,D3DBLENDOP_ADD);RS(d,D3DRS_COLORWRITEENABLE,15);
    TS(d,0,2,D3DTA_TEXTURE);TS(d,0,3,D3DTA_DIFFUSE);TS(d,0,5,D3DTA_TEXTURE);TS(d,0,6,D3DTA_DIFFUSE);
    TS(d,0,11,0);TS(d,0,24,0);TS(d,0,16,D3DTEXF_LINEAR);TS(d,0,17,D3DTEXF_LINEAR);TS(d,0,18,D3DTEXF_NONE);TS(d,0,19,0);
    TS(d,0,13,D3DTADDRESS_CLAMP);TS(d,0,14,D3DTADDRESS_CLAMP);TS(d,1,1,D3DTOP_DISABLE);TS(d,1,4,D3DTOP_DISABLE);
    batchTexture=nullptr;return true;
}
inline void End(){Flush();D8<54>(device,block);D8<56>(device,block);block=0;drawing=false;}
inline float TextWidth(const std::string& text,float height){float w=0;for(unsigned char c:text)if(c>=32&&c<128)w+=(glyphWidth[c-32]-(nativeFont?3:0))*height/fontHeight;return w;}
inline void Plain(const std::string& text,float x,float y,float height,DWORD color){
    if(nativeFont){float f=height/fontHeight;for(unsigned char c:text){if(c<32||c>122)continue;int i=c-32;float w=glyphWidth[i];Quad(x,y,w*f,25*f,color,atlas,glyphU[i],glyphV[i],w/256,25.f/256);x+=(w-3)*f;}return;}
    float f=height/60.0f;for(unsigned char c:text){if(c<32||c>=128)continue;int i=c-32;float w=glyphWidth[i]+2;Quad(x,y,w*f,74*f,color,atlas,float(i%16)/16.0f,float(i/16)*80/512,w/2048,74.0f/512);x+=glyphWidth[i]*f;}
}
inline void Text(const std::string& text,float x,float y,float h,DWORD c=0xffffffff,bool outline=true){
    if(outline){float r=h*.07f;for(int a=0;a<8;a++){float t=float(a)*.785398f;Plain(text,x+cosf(t)*r,y+sinf(t)*r,h,0xff111cba);}}
    Plain(text,x,y,h,c);
}
inline void Right(const std::string& t,float x,float y,float h,DWORD c=0xffffffff,bool outline=true){Text(t,x-TextWidth(t,h),y,h,c,outline);}
inline void Monitor(float x,float y,float size,float phase=-1,float strength=1){
    static void* monitor=nullptr;
    if(!monitor){
        if(FAILED(D8<20>(device,UINT(64),UINT(64),UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&monitor)))return;
        D3DLOCKED_RECT lock{};if(FAILED(D8<16>(monitor,UINT(0),&lock,static_cast<const RECT*>(nullptr),DWORD(0)))){D8<2>(monitor);monitor=nullptr;return;}
        for(int py=0;py<64;py++){auto pixels=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+py*lock.Pitch);for(int px=0;px<64;px++)pixels[px]=0;}
        auto box=[&](int l,int t,int r,int b,DWORD color){for(int py=t;py<b;py++){auto pixels=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+py*lock.Pitch);for(int px=l;px<r;px++)pixels[px]=color;}};
        box(28,39,36,54,0xffb98330);box(29,39,35,53,0xfffff1b0);box(30,39,34,53,0xffff791c);
        box(18,51,46,57,0xffb98330);box(19,51,45,56,0xfffff1b0);box(20,52,44,55,0xffff791c);
        box(7,8,57,43,0xffb98330);box(8,9,56,42,0xfffff1b0);box(10,10,54,40,0xffff791c);
        box(14,14,50,36,0xfffff1b0);box(15,15,49,35,0xffffedb0);
        D8<17>(monitor,UINT(0));
    }
    float unit=size*.02f;
    float angle=phase<0?0:sinf(phase*6.2831853f)*.065f*strength;
    auto rotated=[&](float l,float t,float r,float b,DWORD color,void* texture,float u,float v,float uw,float vh){
        Texture(texture);float sn=sinf(angle),cs=cosf(angle);
        auto vertex=[&](float px,float py,float tu,float tv){px=(px-32)*unit;py=(py-26)*unit;return V(x+px*cs-py*sn,y+px*sn+py*cs,color,tu,tv);};
        Vertex a=vertex(l,t,u,v),bb=vertex(r,t,u+uw,v),c=vertex(r,b,u+uw,v+vh),d=vertex(l,b,u,v+vh);
        vertices.insert(vertices.end(),{a,bb,c,a,c,d});
    };
    rotated(0,0,64,64,0xffffffff,monitor,0,0,1,1);
    if(phase>=0){float scan=15+fmodf(phase,1.f)*17;rotated(15,scan,49,scan+3,(DWORD(160*strength)<<24)|0xfffff1,nullptr,0,0,1,1);}
}
}
