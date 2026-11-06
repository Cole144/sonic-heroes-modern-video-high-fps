#pragma once
float selfLimbOffset=0;
void* __cdecl SelfShadowPart(void* part){
    struct V {float x,y,z,nx,ny,nz,u,v;};
    V base[]={{-.8f,0,-.8f,0,1,0,0,0},{.8f,0,-.8f,0,1,0,1,0},{-.8f,0,.8f,0,1,0,0,1},{.8f,0,.8f,0,1,0,1,1}};
    V limb[]={{-.5f,.7f,-.4f,0,1,0,0,0},{0,.7f,-.4f,0,1,0,1,0},{-.5f,.7f,.4f,0,1,0,0,1},{0,.7f,.4f,0,1,0,1,1}};
    for(auto& vertex:limb)vertex.z+=selfLimbOffset;
    if(part==reinterpret_cast<void*>(1))Check(SUCCEEDED(DrawUP(testDevice,D3DPT_TRIANGLESTRIP,2,base,sizeof(V))),"self-shadow body submission succeeds");
    else Check(SUCCEEDED(DrawUP(testDevice,D3DPT_TRIANGLESTRIP,2,limb,sizeof(V))),"self-shadow animated limb submission succeeds");return nullptr;
}
void* __cdecl SelfShadowFixture(void*){SelfShadowPart(reinterpret_cast<void*>(1));SelfShadowPart(reinterpret_cast<void*>(2));return nullptr;}
DWORD SelfPixel(void* target,int x,int y){
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(D8<9>(target,&lock,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY))),"read self-shadow GPU result");
    DWORD pixel=*reinterpret_cast<const DWORD*>(static_cast<const char*>(lock.pBits)+y*lock.Pitch+x*4);
    D8<10>(target);return pixel&0xffffff;
}
void* __cdecl SelfSphereFixture(void*){
    struct V {float x,y,z,nx,ny,nz,u,v;};static std::vector<V> vertices;
    if(vertices.empty()){
        auto vertex=[](int ring,int segment){float a=ring*3.14159265f/24,b=segment*6.2831853f/48;float x=sinf(a)*cosf(b),y=cosf(a),z=sinf(a)*sinf(b);return V{x*.7f,y*.7f,z*.7f,x,y,z,0,0};};
        for(int r=0;r<24;r++)for(int j=0;j<48;j++){auto a=vertex(r,j),b=vertex(r+1,j),c=vertex(r,j+1),e=vertex(r+1,j+1);for(auto point:{a,b,c,c,b,e})vertices.push_back(point);}
    }
    Check(SUCCEEDED(DrawUP(testDevice,D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(V))),"curved self-shadow fixture submits animated-mesh vertex format");return nullptr;
}
void VerifySelfShadows(){
    using namespace self_shadows;
    Settings defaults;Check(defaults.selfShadows==0,"self shadows are opt-in");defaults.selfShadows=-2;Sanitize(defaults);Check(defaults.selfShadows==1,"self-shadow setting is sanitized");
    auto d=testDevice;DWORD block=0;D8<57>(d,D3DSBT_ALL,&block);
    Check(Ensure(),"create real D3D8 self-shadow map, packed-depth shaders and lookup textures");
    // Independent color/depth pair keeps all pixel and occlusion checks local to this fixture.
    void* oldRT=nullptr,*oldDS=nullptr,*target=nullptr,*targetDepth=nullptr;D3DVIEWPORT9 oldVP{};
    D8<32>(d,&oldRT);D8<33>(d,&oldDS);D8<41>(d,&oldVP);
    Check(SUCCEEDED(D8<25>(d,UINT(1280),UINT(720),D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,BOOL(TRUE),&target)),"create isolated self-shadow color target");
    Check(SUCCEEDED(D8<26>(d,UINT(1280),UINT(720),D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&targetDepth)),"create independent native-format scene depth");
    Check(SUCCEEDED(D8<31>(d,target,targetDepth)),"bind self-shadow fixture target");
    D3DVIEWPORT9 vp{0,0,1280,720,0,1};D8<40>(d,&vp);
    Matrix world=Identity(),view{},proj=Identity();view._11=1;view._23=-1;view._32=1;view._43=2;view._44=1;proj._11=proj._22=.8f;proj._33=.25f;
    auto inv=InverseView(view);auto roundtrip=enemy_shadows::Multiply(view,inv);
    Check(fabsf(roundtrip._11-1)<1e-5f&&fabsf(roundtrip._43)<1e-5f,"light lookup correctly inverts translated native camera");
    D8<37>(d,D3DTS_WORLD,&world);D8<37>(d,D3DTS_VIEW,&view);D8<37>(d,D3DTS_PROJECTION,&proj);
    D8<76>(d,DWORD(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1));D8<88>(d,DWORD(0));
    for(DWORD i=0;i<3;i++){D8<61>(d,i,static_cast<void*>(nullptr));TS(d,i,24,D3DTTFF_DISABLE);TS(d,i,11,i);}
    TS(d,0,1,D3DTOP_SELECTARG1);TS(d,0,2,D3DTA_TFACTOR);TS(d,0,4,D3DTOP_SELECTARG1);TS(d,0,5,D3DTA_TFACTOR);TS(d,1,1,D3DTOP_DISABLE);
    RS(d,D3DRS_TEXTUREFACTOR,0xffc0c0c0);RS(d,D3DRS_LIGHTING,FALSE);RS(d,D3DRS_FOGENABLE,FALSE);RS(d,D3DRS_CULLMODE,D3DCULL_NONE);RS(d,D3DRS_ALPHABLENDENABLE,FALSE);RS(d,D3DRS_ALPHATESTENABLE,FALSE);
    RS(d,D3DRS_STENCILENABLE,FALSE);RS(d,static_cast<D3DRENDERSTATETYPE>(47),0);RS(d,D3DRS_ZENABLE,TRUE);RS(d,D3DRS_ZWRITEENABLE,TRUE);RS(d,D3DRS_ZFUNC,D3DCMP_LESSEQUAL);RS(d,D3DRS_COLORWRITEENABLE,15);
    auto savedDraw=originalDrawUP;originalDrawUP=reinterpret_cast<DrawUPFn>((*static_cast<void***>(d))[72]);
    Bounds bounds;bounds.Add({0,.35f,0},1.1f);void* fixture=reinterpret_cast<void*>(1);enabled=true;
    auto frame=[&](bool shadows,bool foreground){
        D8<36>(d,DWORD(0),static_cast<const D3DRECT*>(nullptr),DWORD(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER),DWORD(0xff204060),1.f,DWORD(0));
        D8<34>(d);SelfShadowFixture(nullptr);
        if(foreground){
            struct Flat {float x,y,z,rhw;DWORD color;};
            Flat rect[]={{700,340,.1f,1,0xffee8866},{735,340,.1f,1,0xffee8866},{700,380,.1f,1,0xffee8866},{735,380,.1f,1,0xffee8866}};
            D8<76>(d,DWORD(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));TS(d,0,2,D3DTA_DIFFUSE);
            D8<72>(d,D3DPT_TRIANGLESTRIP,UINT(2),static_cast<const void*>(rect),UINT(sizeof(Flat)));
            D8<76>(d,DWORD(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1));TS(d,0,2,D3DTA_TFACTOR);
        }
        if(shadows)RenderGroup(&fixture,1,bounds,&SelfShadowFixture);D8<35>(d);
    };
    selfLimbOffset=0;frame(false,false);auto before=SelfPixel(target,717,360);Check(before==0xc0c0c0,"native fixture material is unchanged with feature off");
    frame(true,false);auto shaded=SelfPixel(target,717,360);
    Check((shaded&255)<150&&(shaded&255)>90,"raised mesh casts an actual shadow onto its own lower mesh");
    Check(SelfPixel(target,1024,360)==before,"exposed skeletal surface remains unshadowed");
    Check(SelfPixel(target,10,10)==0x204060,"self-shadow pass leaves surrounding scene unchanged");
    Capture(L"build/preview-self-shadow.bmp",1280,720,target);
    selfLimbOffset=1.f;frame(true,false);Check(SelfPixel(target,717,360)==before,"moving an animated limb removes its old shadow");
    selfLimbOffset=0;frame(true,true);Check(SelfPixel(target,717,360)==0xee8866,"self-shadow cannot draw through foreground geometry");
    DWORD ps=1;D8<89>(d,&ps);Check(ps==0,"self-shadow restores original pixel shader");Matrix after{};D8<38>(d,D3DTS_VIEW,&after);Check(!memcmp(&after,&view,sizeof(view)),"self-shadow restores native view after light pass");
    D8<41>(d,&vp);Check(vp.Width==1280&&vp.Height==720,"self-shadow restores native viewport");
    void* restored=nullptr;D8<33>(d,&restored);Check(restored==targetDepth,"self-shadow restores the scene's original depth surface");D8<2>(restored);
        frame(false,false);unsigned oldMaps=playerMaps,oldReceivers=playerReceivers;
    D8<34>(d);RenderAtomic(fixture,reinterpret_cast<void*>(1),123,bounds,&SelfShadowFixture,&SelfShadowPart);
    RenderAtomic(fixture,reinterpret_cast<void*>(2),123,bounds,&SelfShadowFixture,&SelfShadowPart);D8<35>(d);
    Check(playerMaps==oldMaps+1&&playerReceivers==oldReceivers+2,"visible atomic callbacks share one whole-character depth map per frame");
    Check(SelfPixel(target,717,360)==shaded,"atomic callback receives shadow from another skeletal mesh in the same character");
    selfLimbOffset=1;frame(false,false);D8<34>(d);RenderAtomic(fixture,reinterpret_cast<void*>(1),124,bounds,&SelfShadowFixture,&SelfShadowPart);D8<35>(d);
    Check(SelfPixel(target,717,360)==before&&playerMaps==oldMaps+2,"cached player shadow refreshes with next animation frame");selfLimbOffset=0;
    Release();Check(Ensure(),"self-shadow resources recreate after release/device-reset preparation");
    frame(true,false);Check(SelfPixel(target,717,360)==shaded,"recreated resources produce the same mesh shadow");
        Bounds sphereBounds;sphereBounds.Add({0,0,0},.7f);
    D8<36>(d,DWORD(0),static_cast<const D3DRECT*>(nullptr),DWORD(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER),DWORD(0xff204060),1.f,DWORD(0));
    D8<34>(d);SelfSphereFixture(nullptr);RenderGroup(&fixture,1,sphereBounds,&SelfSphereFixture);D8<35>(d);
    D3DLOCKED_RECT spherePixels{};Check(SUCCEEDED(D8<9>(target,&spherePixels,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY))),"read curved-surface regression fixture");
    unsigned litCount=0,falseShadows=0,grazingCount=0,darkGrazing=0;
    for(int py=175;py<545;py+=4)for(int px=300;px<980;px+=4){
        float x=(px/640.f-1)/.8f,z=(1-py/360.f)/.8f;float y2=.49f-x*x-z*z;if(y2<.02f)continue;
        float facing=(-.45f*x+sqrtf(y2)-.35f*z)/(.7f*sqrtf(1.325f));
        DWORD color=*reinterpret_cast<const DWORD*>(static_cast<const char*>(spherePixels.pBits)+py*spherePixels.Pitch+px*4);
        if(facing<.3f){++grazingCount;if((color&255)<180)++darkGrazing;}
        if(facing<.5f)continue;++litCount;if((color&255)<182)++falseShadows;
    }
    D8<10>(target);Check(litCount>1000&&falseShadows*100<litCount,"light-facing curved mesh has no false dark bands or shadow acne");
    Check(grazingCount>50&&darkGrazing==0,"grazing and backlit curved surfaces avoid harsh duplicate shadow bands");
    Capture(L"build/preview-self-shadow-curved.bmp",1280,720,target);
    enabled=false;originalDrawUP=savedDraw;Release();D8<31>(d,oldRT,oldDS);D8<40>(d,&oldVP);D8<54>(d,block);D8<56>(d,block);
    for(auto p:{oldRT,oldDS,target,targetDepth})if(p)D8<2>(p);
}
