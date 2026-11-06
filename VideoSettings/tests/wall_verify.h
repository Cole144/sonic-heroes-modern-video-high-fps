#pragma once
// Character shadows on vertical receivers (native straight-down projection).
void VerifyWallShadowMath(){
    using namespace wall_shadows;
    D3DMATRIX down{};down._11=.5f;down._41=.5f;down._32=.5f;down._42=.5f;down._43=1;down._44=1;
    Vec d{};Check(Direction(down,d)&&fabsf(fabsf(d.y)-1)<1e-5f&&fabsf(d.x)<1e-5f&&fabsf(d.z)<1e-5f,"projection direction comes from the shadow texture matrix (straight down)");
    D3DMATRIX flat{};Check(!Direction(flat,d),"degenerate texture matrix leaves the native pass unchanged");
    Check(Strength(1)==1&&Strength(-1)==1&&Strength(.9f)==1,"floors keep full shadow strength");
    Check(Strength(0)==0&&Strength(.3f)==0,"walls receive no shadow");
    float slope=Strength(.7071f);Check(slope>.5f&&slope<.9f&&Level(slope)==3,"45-degree slope keeps a faded shadow");
    Check(Level(Strength(.5f))==1,"steep slope keeps only a faint shadow");
    std::vector<uint32_t> tris;unsigned short strip[]={0,1,2,3,3,4,4,5,6};
    Check(Triangles(D3DPT_TRIANGLESTRIP,7,[&](UINT i){return uint32_t(strip[i]);},tris),"strip receivers are expanded");
    Check(tris==std::vector<uint32_t>({0,1,2, 2,1,3, 4,5,6}),"strip expansion keeps winding and drops joins");
    Check(!Triangles(D3DPT_LINELIST,2,[](UINT i){return uint32_t(i);},tris),"non-triangle draws are left native");
    // floor, wall, 45-degree slope, degenerate
    std::vector<uint32_t> list={0,1,2, 3,4,5, 6,7,8, 9,10,11};
    std::vector<Vec> normals={{0,1,0},{0,0,1},{0,.7071f,-.7071f},{0,0,0}};
    Plan plan;Group(list,normals,{0,-1,0},plan);
    Check(plan.drawn==2&&plan.count[0]==2&&plan.count[Levels]==1&&plan.count[3]==1,"wall and zero-area triangles are skipped, floor and slope kept");
    Check(plan.first[Levels]==0&&plan.first[3]==1&&plan.indices==std::vector<uint32_t>({0,1,2,6,7,8}),"strongest group is drawn first");
    Check(plan.low==0&&plan.high==8,"plan records the drawn vertex range");
}
namespace wall_test {
struct V {float x,y,z;DWORD color;float u,v;};
// Floor (left), 45-degree slope (middle), wall (right).
inline const V quads[12]={
    {-.95f,0,-.6f,~0u,0,0},{-.35f,0,-.6f,~0u,0,0},{-.95f,0,.6f,~0u,0,0},{-.35f,0,.6f,~0u,0,0},
    {-.3f,-.5f,-.5f,~0u,0,0},{.3f,-.5f,-.5f,~0u,0,0},{-.3f,.5f,.5f,~0u,0,0},{.3f,.5f,.5f,~0u,0,0},
    {.35f,-.8f,0,~0u,0,0},{.95f,-.8f,0,~0u,0,0},{.35f,.8f,0,~0u,0,0},{.95f,.8f,0,~0u,0,0}};
inline const unsigned short indices[18]={0,1,2,2,1,3, 4,5,6,6,5,7, 8,9,10,10,9,11};
}
void VerifyWallShadowGpu(){
    using namespace wall_test;
    void* d=testDevice;DWORD block=0;D8<57>(d,D3DSBT_ALL,&block);
    bool savedShadows=current.shadows,savedOriginal=shadow_compare::original;current.shadows=true;shadow_compare::original=false;
    auto savedIndexed=originalIndexed;auto savedDraw=originalDraw;
    originalIndexed=reinterpret_cast<IndexedFn>((*reinterpret_cast<void***>(d))[71]);originalDraw=reinterpret_cast<DrawFn>((*reinterpret_cast<void***>(d))[70]);
    void* oldRT=nullptr,*oldDS=nullptr,*target=nullptr;D3DVIEWPORT9 oldVP{};D8<32>(d,&oldRT);D8<33>(d,&oldDS);D8<41>(d,&oldVP);
    Check(SUCCEEDED(D8<25>(d,UINT(256),UINT(256),D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,BOOL(TRUE),&target)),"create wall-shadow color target");
    D8<31>(d,target,static_cast<void*>(nullptr));D3DVIEWPORT9 vp{0,0,256,256,0,1};D8<40>(d,&vp);
    // Screen x = x, screen y = (y+z)/2: floor, slope and wall are all visible.
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    D3DMATRIX projection{};projection._11=1;projection._22=.5f;projection._32=.5f;projection._43=.5f;projection._44=1;
    D3DMATRIX down{};down._11=.5f;down._41=.5f;down._32=.5f;down._42=.5f;down._43=1;down._44=1;
    D8<37>(d,D3DTS_WORLD,&identity);D8<37>(d,D3DTS_VIEW,&identity);D8<37>(d,D3DTS_PROJECTION,&projection);D8<37>(d,D3DTS_TEXTURE0,&down);
    void* vb=nullptr,*ib=nullptr,*flat=nullptr,*texture=nullptr;const DWORD fvf=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
    Check(SUCCEEDED(D8<23>(d,UINT(sizeof(quads)+sizeof(V)*4),DWORD(D3DUSAGE_WRITEONLY),fvf,D3DPOOL_MANAGED,&vb)),"create managed level vertex buffer");
    BYTE* p=nullptr;D8<11>(vb,UINT(0),UINT(0),&p,DWORD(0));memset(p,0,sizeof(V)*4);memcpy(p+sizeof(V)*4,quads,sizeof(quads));D8<12>(vb); // 4 leading pad vertices
    Check(SUCCEEDED(D8<24>(d,UINT(sizeof(indices)),DWORD(D3DUSAGE_WRITEONLY),DWORD(D3DFMT_INDEX16),D3DPOOL_MANAGED,&ib)),"create managed level index buffer");
    D8<11>(ib,UINT(0),UINT(0),&p,DWORD(0));memcpy(p,indices,sizeof(indices));D8<12>(ib);
    V expanded[18];for(int i=0;i<18;++i)expanded[i]=quads[indices[i]];
    Check(SUCCEEDED(D8<23>(d,UINT(sizeof(expanded)),DWORD(D3DUSAGE_WRITEONLY),fvf,D3DPOOL_MANAGED,&flat)),"create non-indexed receiver buffer");
    D8<11>(flat,UINT(0),UINT(0),&p,DWORD(0));memcpy(p,expanded,sizeof(expanded));D8<12>(flat);
    Check(SUCCEEDED(D8<20>(d,UINT(4),UINT(4),UINT(1),DWORD(0),D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture)),"create shadow texture");
    D3DLOCKED_RECT rect{};D8<16>(texture,UINT(0),&rect,static_cast<const RECT*>(nullptr),DWORD(0));
    for(int y=0;y<4;++y)for(int x=0;x<4;++x)reinterpret_cast<DWORD*>(static_cast<char*>(rect.pBits)+y*rect.Pitch)[x]=0xff404040;D8<17>(texture,UINT(0));
    // Native receiver pass state (ShadowRenderFast): multiply the scene by the shadow texel.
    D8<76>(d,fvf);D8<61>(d,DWORD(0),texture);
    RS(d,D3DRS_LIGHTING,FALSE);RS(d,D3DRS_CULLMODE,D3DCULL_NONE);RS(d,D3DRS_ZENABLE,FALSE);RS(d,D3DRS_ALPHATESTENABLE,FALSE);
    RS(d,D3DRS_ALPHABLENDENABLE,TRUE);RS(d,D3DRS_SRCBLEND,D3DBLEND_ZERO);RS(d,D3DRS_DESTBLEND,D3DBLEND_SRCCOLOR);RS(d,D3DRS_TEXTUREFACTOR,0x12345678);
    TS(d,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);TS(d,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);TS(d,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    auto pass=[&](bool receiver){TS(d,0,D3DTSS_TEXCOORDINDEX,receiver?D3DTSS_TCI_CAMERASPACEPOSITION:0);TS(d,0,D3DTSS_TEXTURETRANSFORMFLAGS,receiver?D3DTTFF_COUNT3|D3DTTFF_PROJECTED:D3DTTFF_DISABLE);};
    auto clear=[&](){D8<36>(d,DWORD(0),static_cast<const D3DRECT*>(nullptr),DWORD(D3DCLEAR_TARGET),DWORD(0xffc0c0c0),1.f,DWORD(0));};
    auto red=[&](int x,int y){return int(SelfPixel(target,x,y)>>16&255);};
    // Native pass, unfiltered: everything darkens, including the wall.
    pass(true);wall_shadows::enabled=false;clear();D8<34>(d);D8<83>(d,UINT(0),vb,UINT(sizeof(V)));D8<85>(d,ib,UINT(4));
    Check(SUCCEEDED(Indexed(d,D3DPT_TRIANGLELIST,0,12,0,6)),"native receiver draw");D8<35>(d);
    Check(red(45,128)<60&&red(211,128)<60,"fixture reproduces the native shadow on the wall");
    wall_shadows::enabled=true;wall_shadows::rebuilt=0;
    clear();D8<34>(d);Check(SUCCEEDED(Indexed(d,D3DPT_TRIANGLELIST,0,12,0,6)),"filtered indexed receiver draw");D8<35>(d);
    int floor=red(45,128),slope=red(128,128),wall=red(211,128);
    Check(floor<60,"floor keeps the full character shadow");
    Check(wall==192,"wall no longer receives the shadow");
    Check(slope>floor+15&&slope<180,"45-degree slope receives a faded shadow");
    Check(red(5,5)==192,"empty background untouched");
    void* boundIb=nullptr;UINT boundBase=0;D8<86>(d,&boundIb,&boundBase);Check(boundIb==ib&&boundBase==4,"original indices and base vertex are restored");if(boundIb)D8<2>(boundIb);
    DWORD state=0;D8<62>(d,DWORD(1),DWORD(D3DTSS_COLOROP),&state);Check(state==D3DTOP_DISABLE,"stage 1 restored after the faded group");
    D8<51>(d,D3DRS_TEXTUREFACTOR,&state);Check(state==0x12345678,"texture factor restored");
    clear();D8<34>(d);Indexed(d,D3DPT_TRIANGLELIST,0,12,0,6);D8<35>(d);
    Check(wall_shadows::rebuilt==1&&red(211,128)==192,"cached receiver plan is reused on the next frame");
    // Non-receiver draws are never filtered.
    pass(false);clear();D8<34>(d);Indexed(d,D3DPT_TRIANGLELIST,0,12,0,6);D8<35>(d);
    Check(red(211,128)<60,"ordinary textured draws are unchanged");
    // Former DrawPrimitive receivers.
    pass(true);clear();D8<34>(d);D8<83>(d,UINT(0),flat,UINT(sizeof(V)));D8<85>(d,static_cast<void*>(nullptr),UINT(0));
    Check(SUCCEEDED(Draw(d,D3DPT_TRIANGLELIST,0,6)),"filtered non-indexed receiver draw");D8<35>(d);
    Check(red(45,128)<60&&red(211,128)==192,"non-indexed receivers skip walls too");
    D8<86>(d,&boundIb,&boundBase);Check(boundIb==nullptr,"non-indexed draw leaves no index buffer bound");if(boundIb)D8<2>(boundIb);
    // Released level buffers drop their cached plans (addresses get reused).
    size_t before=wall_shadows::cache.size();D8<83>(d,UINT(0),static_cast<void*>(nullptr),UINT(0));D8<2>(flat);flat=nullptr;
    Check(wall_shadows::cache.size()==before-1,"releasing a level buffer forgets its plan");
    D8<85>(d,static_cast<void*>(nullptr),UINT(0));D8<2>(vb);D8<2>(ib);
    Check(wall_shadows::cache.empty()&&wall_shadows::watched.empty(),"all plans are dropped with their buffers");
    D8<61>(d,DWORD(0),static_cast<void*>(nullptr));D8<2>(texture);
    D8<31>(d,oldRT,oldDS);D8<40>(d,&oldVP);D8<54>(d,block);D8<56>(d,block);D8<2>(target);D8<2>(oldRT);if(oldDS)D8<2>(oldDS);
    originalIndexed=savedIndexed;originalDraw=savedDraw;current.shadows=savedShadows;shadow_compare::original=savedOriginal;wall_shadows::Clear();
}
