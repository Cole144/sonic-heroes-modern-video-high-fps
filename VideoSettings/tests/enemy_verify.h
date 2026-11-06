#pragma once
void VerifyEnemyMath(){
    using namespace enemy_shadows;
    D3DMATRIX p{};
    Check(Project({10,5,3},{0,0,0},p),"enemy shadow accepts a flat native ground plane");
    Check(p._11==1&&p._33==1&&p._22==0&&fabsf(p._42-5.035f)<.00001f,"vertical projection preserves silhouette X/Z and applies a small ground offset");
    Check(Project({10,5,3},{30,47,-15},p),"enemy shadow accepts sloped native ground");
    float y=10*p._12+3*p._32+p._42;
    Check(fabsf(y-5.035f)<.0001f,"inclined plane passes through native contact position");
    Check(!Project({0,0,0},{90,0,0},p)&&!Project({NAN,0,0},{0,0,0},p),"walls and non-finite contacts retain original shadows");
    BeginFrame();auto c=Record(reinterpret_cast<void*>(1));
    c->Add(reinterpret_cast<void*>(2));c->Add(reinterpret_cast<void*>(2));
    Check(c->count==1&&Find(reinterpret_cast<void*>(1))==c,"caster registry deduplicates multipass clumps by actor");
    for(uintptr_t i=3;i<19;i++)c->Add(reinterpret_cast<void*>(i));
    Check(!c->supported&&c->count==16,"oversized caster uses original shadow without overflowing fixed storage");
    BeginFrame();Check(!Find(reinterpret_cast<void*>(1)),"caster lifetimes expire each native frame");
    Check(FixedXYZ(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)&&!FixedXYZ(D3DFVF_XYZRHW)&&!FixedXYZ(D3DFVF_XYZB1)&&!FixedXYZ(0x80000001),"shader guard rejects transformed, blended and programmable positions");
    object_shadows::Contact contact;contact.actor=reinterpret_cast<void*>(1);contact.origin={1,20,3};contact.checked=100;
    Check(object_shadows::Reusable(contact,contact.actor,{1,24,3},500),"floating sign bob uses cached ground without a raycast per frame");
    Check(!object_shadows::Reusable(contact,contact.actor,{2,20,3},500)&&!object_shadows::Reusable(contact,contact.actor,{1,20,3},1100),"moved objects and aged ground contacts refresh their collision query");
    Settings s;s.objectShadows=-5;Sanitize(s);Check(s.objectShadows==1,"object shadow choice sanitizes independently of enemy preference");
    s.enemyShadows=-1;Sanitize(s);Check(s.enemyShadows==1,"enemy shadow preference is sanitized");
}
void VerifyEnemyGpu(){
    using namespace enemy_shadows;
    void* d=testDevice;DWORD block=0;D8<57>(d,D3DSBT_ALL,&block);
    Check(!HasStencil(d),"D16 devices fall back without altering their depth buffer");
    void* color=nullptr,*oldDepth=nullptr,*depth=nullptr;D8<32>(d,&color);D8<33>(d,&oldDepth);
    Check(SUCCEEDED(D8<26>(d,UINT(1280),UINT(720),D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&depth)),"create real D3D8 stencil surface for enemy silhouettes");
    Check(SUCCEEDED(D8<31>(d,color,depth)),"bind enemy stencil test target");
    RS(d,D3DRS_STENCILENABLE,FALSE);Check(HasStencil(d),"D24S8 main viewport supports model silhouettes");
    RS(d,D3DRS_STENCILENABLE,TRUE);Check(!HasStencil(d),"active external stencil effect retains native shadows");RS(d,D3DRS_STENCILENABLE,FALSE);
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    D3DMATRIX view{};view._11=view._23=view._32=view._44=1;
    D8<37>(d,D3DTS_WORLD,&identity);D8<37>(d,D3DTS_VIEW,&view);D8<37>(d,D3DTS_PROJECTION,&identity);
    D8<76>(d,DWORD(D3DFVF_XYZ));D8<88>(d,DWORD(0));
    RS(d,D3DRS_STENCILENABLE,FALSE);RS(d,D3DRS_LIGHTING,TRUE);RS(d,D3DRS_CULLMODE,D3DCULL_CW);
    Project({0,.5f,0},{0,0,0},projection);tint=0x70000000;
    // Two overlapping pieces at different heights must form ONE silhouette.
    Vec left[]={{-.75f,.2f,-.5f},{.25f,.3f,-.5f},{-.75f,.4f,.5f},{.25f,.1f,.5f}};
    Vec right[]={{-.25f,.1f,-.5f},{.75f,.2f,-.5f},{-.25f,.3f,.5f},{.75f,.4f,.5f}};
    D8<36>(d,DWORD(0),static_cast<const D3DRECT*>(nullptr),DWORD(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL),DWORD(0xffc0c0c0),1.f,DWORD(3));
    D8<34>(d);Check(ClearCoverage(d),"clear only reserved stencil bit through a replayable draw");
    for(auto mesh:{left,right}){DrawScope scope(d);Check(SUCCEEDED(D8<72>(d,D3DPT_TRIANGLESTRIP,UINT(2),static_cast<const void*>(mesh),UINT(sizeof(Vec)))),"submit projected animated-model fixture");}
    Check(ClearCoverage(d),"release enemy coverage bit after silhouette");D8<35>(d);
    DWORD outer=Pixel(260,360),overlap=Pixel(640,360),other=Pixel(1020,360);
    Check(outer==overlap&&overlap==other&&(overlap&255)>100&&(overlap&255)<112,"overlapping limbs shade once instead of creating doubled dark shapes");
    Check(Pixel(10,10)==0xc0c0c0,"enemy silhouette does not darken surrounding floor");
    DWORD state=0;D8<51>(d,D3DRS_LIGHTING,&state);Check(state==TRUE,"draw restores original lighting state");D8<51>(d,D3DRS_CULLMODE,&state);Check(state==D3DCULL_CW,"draw restores original culling state");
    D3DMATRIX restored{};D8<38>(d,D3DTS_WORLD,&restored);Check(!memcmp(&restored,&identity,sizeof(identity)),"projected draw restores native world transform");
    // Low stencil bits survive both private clears and all projected draws.
    struct Flat {float x,y,z,rhw;DWORD color;};Flat rect[]={{0,0,0,1,0xff00ff00},{50,0,0,1,0xff00ff00},{0,50,0,1,0xff00ff00},{50,50,0,1,0xff00ff00}};
    RS(d,D3DRS_STENCILENABLE,TRUE);RS(d,D3DRS_STENCILFUNC,D3DCMP_EQUAL);RS(d,D3DRS_STENCILMASK,0xff);RS(d,D3DRS_STENCILREF,3);RS(d,D3DRS_STENCILWRITEMASK,0);RS(d,D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
    RS(d,D3DRS_LIGHTING,FALSE);RS(d,D3DRS_ZENABLE,FALSE);RS(d,D3DRS_CULLMODE,D3DCULL_NONE);RS(d,D3DRS_ALPHABLENDENABLE,FALSE);RS(d,D3DRS_COLORWRITEENABLE,15);
    TS(d,0,1,D3DTOP_SELECTARG1);TS(d,0,2,D3DTA_DIFFUSE);TS(d,1,1,D3DTOP_DISABLE);D8<76>(d,DWORD(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
    D8<34>(d);D8<72>(d,D3DPT_TRIANGLESTRIP,UINT(2),static_cast<const void*>(rect),UINT(sizeof(Flat)));D8<35>(d);
    Check(Pixel(25,25)==0x00ff00,"existing low stencil bits are preserved and private coverage is cleared");
    D8<31>(d,color,oldDepth);D8<54>(d,block);D8<56>(d,block);D8<2>(color);D8<2>(oldDepth);D8<2>(depth);
    submitted=0;drawing=false;projection={};
}
