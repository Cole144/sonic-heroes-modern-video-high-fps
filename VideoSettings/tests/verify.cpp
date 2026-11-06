#define VIDEO_TEST
#include "../src/mod.cpp"
#include <cassert>
#include <iostream>
#include <map>

void Check(bool condition,const char* label){if(!condition){std::cerr<<"FAIL: "<<label<<"\n";exit(1);}std::cout<<"PASS: "<<label<<"\n";}
float evaluatedFrame=-1;
void RecordFrame(void*,float frame){evaluatedFrame=frame;}
int nativeExecCalls=0;
int cameraSetterCalls=0;
motion::Settings fixtureMotion;
void __cdecl MotionGet(motion::Settings* s){*s=fixtureMotion;}
int __cdecl MotionSet(const motion::Settings* s){fixtureMotion=*s;return 1;}
uint32_t fixtureStatus=motion::Waiting;
void __cdecl MotionStats(motion::Stats* s){s->status=fixtureStatus;}
void __cdecl MotionOverlay(motion::Overlay){}
const motion::Api fixtureMotionApi{sizeof(motion::Api),motion::Abi,&MotionGet,&MotionSet,&MotionStats,&MotionOverlay};
void __cdecl WidescreenCameraFixture(void* camera,const CameraView* view){
    ++cameraSetterCalls;auto bytes=static_cast<unsigned char*>(camera);
    memcpy(bytes+0x68,view,sizeof(*view));
    CameraView reciprocal{.75f/view->x,1.f/view->y}; // 16:9 correction over 4:3
    memcpy(bytes+0x70,&reciprocal,sizeof(reciprocal));
}
void __fastcall FakeExec(void* object,void*){
    nativeExecCalls++;
    if(Field<int>(object,0x2c)==5){Field<int>(object,0x28)=5;Field<int>(object,0x2c)=1;Field<int>(object,0x60)=6;}
}
void* __cdecl TestLabel(void*,int type,const char* name,int*){
    // Read the installed FE source ANM labels, rather than mirroring production
    // constants in a fake animation table. These assets never ship in the mod.
    static auto animationLabels=[](){
        std::map<std::string,std::array<int,4>> result;
        std::ifstream file("research/assets-fe/OPTION.ANM",std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
        auto word=[&](size_t offset){if(offset+4>data.size())throw std::runtime_error("Truncated ANM");int value;memcpy(&value,data.data()+offset,4);return value;};
        if(word(0)!=0x1b1)throw std::runtime_error("Wrong ANM");size_t pos=36;
        int count=word(32);if(count<0||count>1024)throw std::runtime_error("Invalid ANM labels");
        for(int i=0;i<count;i++){
            int n=word(pos);pos+=4;if(n<=0||pos+size_t(n)+8>data.size()||data[pos+n-1])throw std::runtime_error("Invalid ANM label");
            std::string label(reinterpret_cast<const char*>(data.data()+pos));pos+=n;
            int kind=word(pos),index=word(pos+4);pos+=8;
            if(kind==2)result[label]={kind,0,index,0};
        }
        return result;
    }();
    if(type!=2)return nullptr;auto item=animationLabels.find(name);return item==animationLabels.end()?nullptr:item->second.data();
}
void* __cdecl CachedShapeFixture(void* shape,void*,void*){
    Check(SUCCEEDED(Indexed(testDevice,D3DPT_TRIANGLELIST,0,4,0,2)),"retained shape submits through actual D3D8 indexed draw");return shape;
}
DWORD Pixel(int x,int y){
    void* back=nullptr;D8<16>(testDevice,UINT(0),D3DBACKBUFFER_TYPE_MONO,&back);Check(back!=nullptr,"obtain backbuffer for pixel invariant");
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(D8<9>(back,&lock,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY))),"lock backbuffer for pixel invariant");
    DWORD pixel=*reinterpret_cast<const DWORD*>(static_cast<const char*>(lock.pBits)+y*lock.Pitch+x*4);
    D8<10>(back);D8<2>(back);return pixel&0xffffff;
}
// Preview fixture only: the shipped menu leaves the actual game background intact.
void Background(int width=1280,int height=720,float offset=0){
    ui::Begin(testDevice,width,height);float left=-ui::ox/ui::sx,top=-ui::oy/ui::sy;
    ui::Quad(left,top,width/ui::sx,height/ui::sy,0xfff2cf2b);ui::Quad(left,top,width/ui::sx,122-top,0xffd8d8d8);
    for(float x=left;x<left+width/ui::sx;x+=28)ui::Quad(x,top,1,122-top,0x70ffffff);
    for(float y=top;y<122;y+=28)ui::Quad(left,y,width/ui::sx,1,0x70ffffff);
    ui::Quad(left,122,width/ui::sx,294,0xffc3a612);for(int y=124;y<416;y+=6)ui::Quad(left,float(y),width/ui::sx,2,0x20745e00);
    ui::Circle(31,45,240,0x70647f26);ui::Circle(27,43,197,0xff82c21b);
    ui::Image(art::header,116,29,550,56);
    ui::Image(art::sonic,-15+offset,438,350,76,0x48fff6cb);ui::Image(art::heroes,350+offset,438,450,76,0x48fff6cb);ui::End();
}
void Capture(const wchar_t* name,int width,int height,void* target=nullptr){
    void* back=target;if(back)D8<1>(back);else Check(SUCCEEDED(D8<16>(testDevice,UINT(0),D3DBACKBUFFER_TYPE_MONO,&back)),"obtain real D3D8 backbuffer");
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(D8<9>(back,&lock,static_cast<const RECT*>(nullptr),DWORD(D3DLOCK_READONLY))),"read rendered backbuffer");
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+width*height*4;
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));
    for(int y=0;y<height;y++)out.write(static_cast<char*>(lock.pBits)+y*lock.Pitch,width*4);
    D8<10>(back);D8<2>(back);
}
void ShadowTests(){
    for(int quality:{1,2}){
        float sumU=0,sumV=0;
        for(int tap=0;tap<4;++tap){
            ui::Vertex source[4]{},copy[4]{};float inset=(5.f-2.5f*tap)*.025f;
            for(int i=0;i<4;++i)source[i]={float(i),float(i+1),.1f,1.f,0x41ffffff,(i&2)?1.f-inset:inset,(i&1)?1.f-inset:inset};
            ui::Vertex before[4];memcpy(before,source,sizeof(source));
            Check(shadow_quality::FilterQuad(source,copy,4,quality),"enhanced shadow filter accepts native blur pass");
            bool preserved=!memcmp(source,before,sizeof(source));
            for(int i=0;i<4;++i)preserved=preserved&&!memcmp(&source[i],&copy[i],offsetof(ui::Vertex,u));
            Check(preserved&&fabsf(copy[2].u-copy[0].u-1.f)<.000001f&&fabsf(copy[1].v-copy[0].v-1.f)<.000001f,"filter preserves silhouette scale, draw geometry, opacity and caller-owned input");
            Check(fabsf(copy[0].u)*float(shadow_quality::Quality(quality).size)==.75f&&fabsf(copy[0].v)*float(shadow_quality::Quality(quality).size)==.75f,"filter samples stay within one source texel");
            sumU+=copy[0].u;sumV+=copy[0].v;
        }
        Check(fabsf(sumU)<.000001f&&fabsf(sumV)<.000001f,"four shadow filter taps stay centered without positional bias");
    }
    ui::Vertex source[4]{},copy[4]{};
    Check(!shadow_quality::FilterQuad(source,copy,4,0)&&!shadow_quality::FilterQuad(source,copy,3,2),"Original shadows and unexpected primitives retain native filter");
    auto saved=shadow_quality::expected;auto savedActive=shadow_quality::active;auto savedBlur=shadow_quality::blurStep;
    std::map<uintptr_t,uint32_t> memory;for(const auto& site:shadow_quality::sites)memory[site.address]=site.original;
    auto read=[&](uintptr_t at){return memory.at(at);};auto write=[&](uintptr_t at,uint32_t value){memory.at(at)=value;return true;};
    for(int q:{0,1,2,0}){
        Check(shadow_quality::Apply(q,0x12340000,read,write),"guarded shadow preset applies and switches back to original");
        auto p=shadow_quality::Quality(q);Check(memory[0x7476cc]==p.size&&memory[0x63bf6c]==p.size&&memory[0x63bf81]*128==p.size&&memory[0x63bf90]+2*memory[0x63bf81]==p.size,"depth, color and subraster dimensions stay consistent");
        float extent;uint32_t bits=memory[0x63b07b];memcpy(&extent,&bits,4);Check(extent==float(p.size)&&shadow_quality::blurStep==.025f,"higher-resolution blur quad preserves the native normalized softness");
    }
    memory[0x63bf6c]=999;auto altered=memory;
    Check(!shadow_quality::Apply(2,0x12340000,read,write)&&memory==altered,"incompatible shadow patch is rejected before any writes");memory[0x63bf6c]=256;
    auto before=memory;int calls=0;
    Check(!shadow_quality::Apply(2,0x12340000,read,[&](uintptr_t at,uint32_t value){write(at,value);return ++calls!=4;})&&memory==before,"partial patch failure including a failed protection restore recovers all owned values");
    shadow_quality::expected=saved;shadow_quality::active=savedActive;shadow_quality::blurStep=savedBlur;
    auto savedSettings=current;auto savedPath=configPath;auto savedCompatibility=shadow_quality::compatible;
    shadow_quality::compatible=true;shadow_quality::maxQuality=2;current=Settings{};
    Check(Value(1,3)=="Original","legacy/default detailed shadows retain original quality");
    AdjustShadowChoice(1);Check(Value(1,3)=="High (512)","shadow row selects enhanced high quality");
    AdjustShadowChoice(1);Check(Value(1,3)=="Ultra (1024)","shadow row selects enhanced ultra quality");
    configPath=L"build/verify-shadows.ini";Save();current=Settings{};Load();Check(current.shadowQuality==2&&current.shadows,"shadow preset survives config round trip");
    AdjustShadowChoice(1);Check(!current.shadows&&Value(1,3)=="Simple","shadow quality retains the simple-shadow option");
    {std::ofstream legacy(configPath);legacy<<"[Video]\nDetailedShadows=0\n";}current=Settings{};Load();Check(!current.shadows&&current.shadowQuality==0,"legacy simple-shadow preference migrates without enabling detailed shadows");
    current=Settings{};shadow_quality::maxQuality=0;AdjustShadowChoice(1);Check(!current.shadows,"unsupported high-resolution presets are excluded");
    std::filesystem::remove(configPath);configPath=savedPath;current=savedSettings;shadow_quality::compatible=savedCompatibility;shadow_quality::maxQuality=2;saveExists=false;
}
int shadowTextureCalls=0,shadowEffectsCalls=0;
void* __cdecl ShadowTextureFixture(void* material,void* texture){++shadowTextureCalls;shadow_models::Field<void*>(material,0)=texture;return material;}
void* __cdecl ShadowEffectsFixture(void* material,int effects){++shadowEffectsCalls;Check(effects==0,"shadow disables material effects");return material;}
void ShadowModelTests(){
    const char* names[]={"sonic_locator.dff","K_SONIC_LOCATOR.DFF","so_mabuta01.dff","k_missing.dff","ta_sippo.dff","k_ta_sippo.dff"};
    auto native=shadow_models::Select(names,6,false);auto full=shadow_models::Select(names,6,true);
    Check(!native.replaced[1]&&native.names[1]==names[1],"Original retains native shadow model");
    Check(full.replaced[1]&&full.names[1]==names[0]&&full.replaced[5]&&full.names[5]==names[4],"enhanced body and tail models use full counterpart");
    Check(!full.replaced[0]&&!full.replaced[2]&&!full.replaced[3]&&full.names[3]==names[3],"visible models, eyelids and unmatched attachments remain native");
    shadow_models::KeepOriginals(full,names,6);
    Check(full.count==8&&full.originalIndex[1]==6&&full.names[6]==names[1]&&full.originalIndex[5]==7&&full.names[7]==names[5],"comparison loads original body and tail copies into native-owned appended slots");
    Check(shadow_models::Select(names,65,true).names[0]==nullptr,"oversized native model lists are rejected");
    alignas(4) unsigned char clump[0x30]{},atomic[0x80]{},geometry[0x70]{},material[0x30]{},visible[0x30]{};
    memset(visible,0x91,sizeof(visible));unsigned char before[sizeof(visible)];memcpy(before,visible,sizeof(before));
    void* mats[]={material};using shadow_models::Field;
    Field<void*>(clump,8)=atomic+0x40;Field<void*>(atomic,0x40)=clump+8;Field<void*>(atomic,0x18)=geometry;
    Field<unsigned>(geometry,8)=0x100ff;Field<void**>(geometry,0x20)=mats;Field<int>(geometry,0x24)=1;
    Field<void*>(material,0)=visible;Field<unsigned>(material,4)=0xffffffff;Field<void*>(geometry,0x54)=visible;
    shadow_models::Silhouette(clump,&ShadowTextureFixture,&ShadowEffectsFixture);
    Check(Field<unsigned>(material,4)==0xff000000&&Field<void*>(material,0)==nullptr,"full shadow copy becomes opaque untextured black");
    Check(Field<unsigned>(geometry,8)==0x10073&&Field<void*>(geometry,0x54)==visible,"shadow keeps geometry, normals and allocated UV metadata");
    Check(!memcmp(before,visible,sizeof(before))&&shadowTextureCalls==1&&shadowEffectsCalls==1,"shadow conversion leaves visible model memory untouched");
    shadow_models::Silhouette(nullptr,&ShadowTextureFixture,&ShadowEffectsFixture);
}
std::vector<uintptr_t> destroyedRasters;
int rasterCreates=0;bool failRasterSub=false;
void* __cdecl CreateRasterFixture(int w,int h,int depth,int flags){
    ++rasterCreates;Check((w==256&&h==256&&depth==32&&flags==0x505)||(w==0&&h==0&&depth==0&&flags==5),"comparison raster follows native allocation flags");
    return reinterpret_cast<void*>(uintptr_t(0x1000+rasterCreates*0x100));
}
void* __cdecl SubRasterFixture(void* child,void*,const RECT* rect){Check(rect->left==2&&rect->top==2&&rect->right==252&&rect->bottom==252,"original comparison uses native 256px subraster bounds");return failRasterSub?nullptr:child;}
void* __cdecl DestroyRasterFixture(void* p){destroyedRasters.push_back(reinterpret_cast<uintptr_t>(p));return p;}
void ShadowCompareTests(){
    auto api=shadow_compare::api;shadow_compare::api={&CreateRasterFixture,&SubRasterFixture,&DestroyRasterFixture};
    shadow_compare::Rasters pair;Check(shadow_compare::Create(pair,256),"comparison allocation succeeds");
    auto root=pair.root,sub=pair.sub;void* currentRoot=reinterpret_cast<void*>(0x3000),*currentSub=reinterpret_cast<void*>(0x3100);
    shadow_compare::Swap(pair,currentRoot,currentSub);shadow_compare::Swap(pair,currentRoot,currentSub);
    Check(pair.root==root&&pair.sub==sub&&currentRoot==reinterpret_cast<void*>(0x3000),"round-trip toggle retains exact original raster handles");
    Check(shadow_compare::Create(pair,256)&&rasterCreates==2,"repeated comparisons reuse existing resources");
    shadow_compare::Destroy(pair);Check(!pair.root&&!pair.sub&&destroyedRasters==std::vector<uintptr_t>({0x1200,0x1100}),"inactive comparison resources release subraster before parent");
    failRasterSub=true;Check(!shadow_compare::Create(pair,256)&&!pair.root&&!pair.sub&&destroyedRasters.size()==4,"failed subraster allocation rolls back both resources");
    failRasterSub=false;shadow_compare::api=api;
}
#include "enemy_verify.h"
#include "self_verify.h"
int main(){
    Check(StartupProblem(true,0x4060a0,true,true,true)==nullptr,"supported executable with free hook sites starts");
    Check(std::wstring(StartupProblem(false,0,false,false,false)).find(L"different game code")!=std::wstring::npos,"unsupported executable is named as the problem");
    Check(std::wstring(StartupProblem(true,0x123456,true,true,true)).find(L"Options menu")!=std::wstring::npos,"replaced Options menu is named");
    Check(std::wstring(StartupProblem(true,0x4060a0,false,true,true)).find(L"0x658D40")!=std::wstring::npos,"taken display hook is named");
    Check(std::wstring(StartupProblem(true,0x4060a0,true,false,true)).find(L"0x451D65")!=std::wstring::npos,"changed save step is named");
    Check(std::wstring(StartupProblem(true,0x4060a0,true,true,false)).find(L"camera")!=std::wstring::npos,"taken camera hook is named");
    VerifyEnemyMath();
    ShadowCompareTests();
    ShadowModelTests();
    ShadowTests();
    Check(art::IsTitle("option_043",true)&&art::IsTitle("option_040",true)&&!art::IsTitle("option_028",true),"Fixed Edition title suppression includes Language and Game Data, preserves speech bubble");
    Check(art::IsTitle("option_028",false)&&!art::IsTitle("option_043",false),"retail uses a distinct title map");
    std::array<char,0x410> object{};auto set=[&](int offset,int value){memcpy(object.data()+offset,&value,4);};
    set(0x28,5);set(0x2c,1);set(0x60,6);Check(RootShown(object.data()),"TV is visible before the opening animation finishes");
    set(0x28,1);Check(RootShown(object.data()),"TV remains visible during the opening transition");
    set(0x60,10);Check(!RootShown(object.data()),"TV does not appear during Language Settings transition");
    std::array<unsigned char,0x410> saveObject{};saveObject.fill(0x5a);auto saveBefore=saveObject;
    void* savePointer=saveObject.data();uintptr_t stackBefore=0,stackAfter=0;
    __asm {
        push esi
        mov esi,savePointer
        mov stackBefore,esp
        push 0
        push 0
        push 2
        push 0
        push 0
        push 0
        call SkipSaveSuccess
        mov stackAfter,esp
        pop esi
    }
    Check(stackBefore==stackAfter,"save-success replacement preserves native callee stack cleanup");
    *reinterpret_cast<int*>(saveBefore.data()+0x2c)=2;
    Check(saveObject==saveBefore,"success acknowledgement only selects the normal Options exit state");
    std::array<unsigned char,0x400> fovCamera{};
    *reinterpret_cast<int*>(fovCamera.data()+0x14)=1;
    const CameraView scripted{.6145975f,.5f};CameraView shared=scripted;
    WidescreenCameraFixture(fovCamera.data(),&scripted);
    auto defaultCamera=fovCamera;int callsBefore=cameraSetterCalls;
    Check(!ApplyCameraView(fovCamera.data(),100,&shared,&WidescreenCameraFixture)&&fovCamera==defaultCamera&&cameraSetterCalls==callsBefore,"default FOV is an exact no-op on native camera and hook chain");
    Check(ApplyCameraView(fovCamera.data(),150,&shared,&WidescreenCameraFixture),"wider FOV is submitted through the existing camera setter");
    auto expanded=*reinterpret_cast<CameraView*>(fovCamera.data()+0x68);
    auto reciprocal=*reinterpret_cast<CameraView*>(fovCamera.data()+0x70);
    Check(fabsf(expanded.x-scripted.x*1.5f)<.00001f&&expanded.y==.75f&&shared.x==expanded.x&&shared.y==expanded.y,"projection helpers and clipping receive the same expanded FOV");
    Check(fabsf(reciprocal.x*expanded.x-.75f)<.00001f&&reciprocal.y*expanded.y==1.f,"FOV preserves the widescreen adapter's aspect correction");
    for(int i=0;i<60;i++){WidescreenCameraFixture(fovCamera.data(),&scripted);ApplyCameraView(fovCamera.data(),150,&shared,&WidescreenCameraFixture);}
    Check(shared.x==expanded.x&&shared.y==expanded.y&&scripted.y==.5f,"repeated camera updates do not compound FOV or modify authored camera values");
    CameraView zoomed{scripted.x*.5f,scripted.y*.5f};WidescreenCameraFixture(fovCamera.data(),&zoomed);ApplyCameraView(fovCamera.data(),150,&shared,&WidescreenCameraFixture);
    Check(shared.x==expanded.x*.5f&&shared.y==expanded.y*.5f,"scripted camera zoom remains proportional at custom FOV");
    WidescreenCameraFixture(fovCamera.data(),&scripted);ApplyCameraView(fovCamera.data(),50,&shared,&WidescreenCameraFixture);
    Check(shared.y==.25f,"lower FOV narrows the camera view");
    *reinterpret_cast<int*>(fovCamera.data()+0x14)=2;callsBefore=cameraSetterCalls;
    Check(!ApplyCameraView(fovCamera.data(),150,&shared,&WidescreenCameraFixture)&&cameraSetterCalls==callsBefore,"orthographic cameras are unchanged");
    *reinterpret_cast<int*>(fovCamera.data()+0x14)=1;*reinterpret_cast<float*>(fovCamera.data()+0x68)=NAN;
    Check(!ApplyCameraView(fovCamera.data(),150,&shared,&WidescreenCameraFixture),"invalid camera data cannot become an invalid FOV write");
    Settings s;s.mode=999;s.width=-1;s.height=99999;s.aa=3;s.filter=999;s.mip=-999;s.brightness=-99;Sanitize(s);
    Check(s.mode==2&&s.width==640&&s.height==4320&&s.aa==0&&s.filter==6&&s.mip==-4&&s.brightness==-10,"invalid persisted values are bounded");
    s.fov=-1;Sanitize(s);Check(s.fov==50,"FOV lower bound is 50 percent");s.fov=999;Sanitize(s);Check(s.fov==150,"FOV upper bound is 150 percent");
    auto oldConfig=configPath;auto oldSettings=current;
    configPath=L"build/verify-fov.ini";current.fov=125;Save();current.fov=100;Load();
    Check(!saveError&&current.fov==125,"FOV preference persists through the actual save and load path");
    {std::ofstream legacy(configPath);legacy<<"[Video]\nBrightness=2\n";}current=Settings{};Load();
    Check(current.fov==100&&current.brightness==2,"existing configurations without FOV retain the native default");
    std::filesystem::remove(configPath);configPath=oldConfig;current=oldSettings;saveExists=false;
    MenuTransition motion;motion.Begin(1000,true);Check(motion.At(1000)==0&&motion.At(1125)==.5f&&motion.At(1250)==1&&!motion.Active(1250),"opening reaches both endpoints and midpoint smoothly");
    motion.Begin(1300,false);Check(motion.At(1300)==1&&motion.At(1425)==.5f&&motion.At(1550)==0,"closing reverses the same transition");
    Check(RootOpacity(0)==1&&RootOpacity(.5f)==0&&PageOpacity(.45f)==0&&PageOpacity(1)==1&&IconProgress(.6f)==1,"staged transition moves the icon before selectors become opaque");
    std::array<char,16> animation{};std::array<char,80> keys{};
    *reinterpret_cast<uintptr_t*>(animation.data()+8)=reinterpret_cast<uintptr_t>(keys.data());
    *reinterpret_cast<int*>(animation.data()+4)=10;*reinterpret_cast<uintptr_t*>(animation.data()+12)=1;
    *reinterpret_cast<uintptr_t*>(object.data()+0x38)=reinterpret_cast<uintptr_t>(animation.data());
    *reinterpret_cast<float*>(keys.data()+9*8+4)=113;set(0x70,9);set(0x94,3);
    auto realFind=findLabel;auto realEvaluate=evaluateNative;findLabel=&TestLabel;evaluateNative=&RecordFrame;
    nativeChoice=3;rootChoice=3;*reinterpret_cast<float*>(object.data()+0x34)=127;SelectRoot(object.data(),4);
    Check(evaluatedFrame==113&&Field<float>(object.data(),0x34)==113,"TV uses the settled Language loop start, never entrance frame 19");
    StepRoot(object.data());Check(evaluatedFrame==113&&Field<int>(object.data(),0x94)==3,"TV holds the previous icon at rest without moving native selection");
    SelectRoot(object.data(),0);StepRoot(object.data());Check(evaluatedFrame==21&&Field<int>(object.data(),0x94)==0,"Audio owns its real loop instead of being forced to Vibration");
    set(0x2cc,1);set(0x2d0,3);set(0x2d4,2);set(0x34c,3);
    SelectRoot(object.data(),3);Check(Field<int>(object.data(),0x350)==1,"native return transitions retain Language selection");
    Check(native_menu::Navigate(2,8,true)==3&&native_menu::Navigate(3,4,true)==2&&native_menu::Navigate(3,8,true)==4&&native_menu::Navigate(4,4,true)==3,"all three bottom buttons navigate left/right consistently");
    Check(native_menu::Navigate(0,8,true)==1&&native_menu::Navigate(1,4,true)==0&&native_menu::Navigate(0,2,true)==2&&native_menu::Navigate(1,2,true)==3&&native_menu::Navigate(4,1,true)==1,"top row includes selectable Audio and vertical neighbors follow geometry");
    Check(native_menu::Navigate(1,4,false)==1&&native_menu::Navigate(2,1,false)==1,"retail layout never selects missing Audio artwork");
    auto realExec=originalExec;originalExec=&FakeExec;ready=true;set(0x28,0);set(0x2c,5);set(0x94,0);
    Options(object.data(),nullptr);
    Check(entrancePlaying&&animationFrame==1&&Field<int>(object.data(),0x28)==6,"entrance takes ownership after native lifecycle initialization");
    for(int i=0;i<19;i++)Options(object.data(),nullptr);
    Check(!entrancePlaying&&rootChoice==0&&evaluatedFrame==20&&nativeExecCalls==1,"entrance ends at Audio after 19 steps rather than traversing its 30-frame loop to Vibration");
    Options(object.data(),nullptr);Check(rootChoice==0&&nativeExecCalls==1&&evaluatedFrame==21,"stable root does not call the three-item PC selector");
    audioOpen=true;audioMotion=1;animationFrame=143;audioChoice=0;
    for(int i=0;i<12;i++)AudioFrame(object.data(),0);
    Check(audioOpen&&audioMotion==0&&evaluatedFrame==155,"Audio opens through its native authored transition");
    AudioFrame(object.data(),2);Check(audioChoice==1&&evaluatedFrame==156,"Audio down selects the native Mono frame");
    AudioFrame(object.data(),2);Check(audioChoice==0&&evaluatedFrame==155,"Audio wraps only the two supported PC modes");
    AudioFrame(object.data(),32);Check(rootVisible&&rootEntering,"TV follows native root fade during Audio closing rather than popping in after it");for(int i=0;i<12;i++)AudioFrame(object.data(),0);
    Check(!audioOpen&&rootChoice==0&&evaluatedFrame==20,"Audio cancel returns through its native closing transition to Audio root");
    originalExec=realExec;ready=false;focused=false;menuObject=nullptr;
    findLabel=realFind;evaluateNative=realEvaluate;
    std::array<char,64> maestro{};std::array<char,0x100> scene{};std::array<char,16> sceneList{};
    std::array<char,43*0x100> sceneChildren{};std::array<std::array<char,16>,5> nestedLists{};std::array<std::array<char,0x100>,5> shapes{};
    Field<void*>(object.data(),0x38)=animation.data();Field<void*>(animation.data(),12)=maestro.data();Field<void*>(maestro.data(),0x24)=scene.data();
    Field<int>(scene.data(),0)=1;Field<void*>(scene.data(),0xac)=sceneList.data();Field<void*>(sceneList.data(),0)=sceneChildren.data();Field<int>(sceneList.data(),4)=43;Field<int>(sceneList.data(),12)=0x100;
    constexpr int sceneIds[]={2,4,7,10,13};
    for(int i=0;i<5;i++){
        auto child=sceneChildren.data()+sceneIds[i]*0x100;Field<int>(child,0)=1;Field<void*>(child,0xac)=nestedLists[i].data();
        Field<void*>(nestedLists[i].data(),0)=shapes[i].data();Field<int>(nestedLists[i].data(),4)=1;Field<int>(nestedLists[i].data(),12)=0x100;
        Field<int>(shapes[i].data(),0)=2;Field<uintptr_t>(shapes[i].data(),0xac)=i<4?0x1234:0x5678;
    }
    art::fixedOptions=true;BindButtons(object.data());Check(buttonShapes[3]==shapes[3].data()&&buttonShapes[4]==shapes[4].data(),"verified FE scene topology binds stable Language and glow identities");
    Field<int>(sceneList.data(),4)=42;BindButtons(object.data());Check(!buttonShapes[3],"unknown scene topology is rejected before child access");art::fixedOptions=false;
    native_menu::Hexes poses;
    auto observe=[&](float cx,float cy,bool indexed){
        std::array<ui::Vertex,4> v{};float us[]={0,1,1,0},vs[]={0,0,1,1};
        for(int i=0;i<4;i++){float x=us[i]*256-70,y=vs[i]*256-73;v[i]={cx+2*x+.3f*y,cy-.2f*x+1.8f*y,0,1,0x80ffffff,us[i],vs[i]};}
        auto retained=v;unsigned short indices[]={0,1,2,0,2,3};
        if(indexed)poses.ObserveIndexed(3,v.data(),4,indices,6);else poses.Observe(5,v.data(),4);
        Check(!memcmp(v.data(),retained.data(),sizeof(v)),"observing native animation leaves caller vertices untouched");
    };
    observe(610,270,false);observe(820,270,true);observe(510,450,false);observe(715,450,true);
    ui::NativeTransform transform;
    Check(poses.TVTransform(4.f/3,690,337.5f,4,transform)&&fabsf(transform.x-(715+205))<.001f&&fabsf(transform.y-450)<.001f,"TV follows adjacent button position and column spacing after widescreen transforms");
    Check(fabsf(transform.ax-2/1.125f)<.001f&&fabsf(transform.ay+.2f/1.125f)<.001f&&transform.tint==0x80ffffff,"TV inherits native entrance scale rotation and fade without a separate curve");
    ui::nativeTransform=transform;auto center=ui::V(690,337.5f,0xffffffff);Check(fabsf(center.x-transform.x)<.001f&&fabsf(center.y-transform.y)<.001f&&(center.color>>24)==128,"rendered TV vertices use native transform and alpha");ui::nativeTransform={};
    Check(native_menu::Hit(transform,transform.x,transform.y)&&!native_menu::Hit(transform,transform.x+500,transform.y),"TV mouse hit area follows native geometry");
    auto startPose=native_menu::ToPage(transform,0,4.f/3,0,0),endPose=native_menu::ToPage(transform,1,4.f/3,0,0);
    Check(startPose.x==transform.x&&startPose.ax==transform.ax&&fabsf(endPose.x-(248*4.f/3-.5f))<.001f,"page transition begins from actual native pose without snapping to fixed root coordinates");
    buttonShapes[3]=reinterpret_cast<void*>(4);buttonShapes[4]=reinterpret_cast<void*>(5);
    native_menu::Hexes one;one.items[0]=poses.items[0];one.count=1;renderingShape=buttonShapes[3];CaptureButton(one);
    Check(buttonPoses[3].enabled&&buttonPoses[3].x==poses.items[0].x,"Language reference is captured even when it is the only visible hex");
    nativeChoice=3;renderingShape=buttonShapes[4];one.items[0].x=999;CaptureButton(one);
    Check(buttonPoses[3].x==999,"selected Language uses the shared glow geometry when its idle hex is hidden");
    renderingShape=reinterpret_cast<void*>(9);one.items[0].x=500;CaptureButton(one);Check(buttonPoses[3].x==999,"coincident unrelated geometry cannot replace the Language reference");
    buttonShapes.fill(nullptr);buttonPoses.fill({});renderingShape=nullptr;
    poses.Clear();Check(!poses.TVTransform(1,690,337.5f,4,transform),"incomplete native geometry cannot fabricate an entrance animation");
    Check(SelectableRows(2)==1&&TabCount()==3&&DisplayActionRow()==5,"optional FPS absence keeps native three categories");
    motionApi=&fixtureMotionApi;Check(TabCount()==3&&SelectableRows(0)==7&&DisplayActionRow()==6,"FPS appears on normal Display page");
    Check(Value(0,5)=="60"&&std::string(Label(0,5))=="FPS","FPS defaults to 60 with clear label");
    tab=0;row=5;
    for(auto rate:{120u,180u,240u,360u,480u,0u,60u}){Adjust(1);Check(fixtureMotion.targetHz==rate&&fixtureMotion.enabled==(rate!=60?1u:0u),"FPS cycles through higher, unlocked and native targets");}
    Adjust(-1);Check(Value(0,5)=="Unlocked","reverse FPS selection reaches unlocked");
    auto target=fixtureMotion.targetHz;row=DisplayActionRow();Adjust(1);Check(fixtureMotion.targetHz==target,"apply-display row cannot change FPS");
    tab=2;row=1;Adjust(1);Check(fixtureMotion.showFps&&Value(2,1)=="On","FPS display moved to Interface");
    {auto logging=fixtureMotion.logging;Adjust(1);Adjust(-1);Check(fixtureMotion.logging==logging&&Value(2,1)=="On","Interface rows never change performance logging");}
    Check(SelectableRows(2)==2&&std::string(Label(2,1))=="FPS display","Interface has only Brightness and FPS display");
    {auto savedCurrent=current,savedEdit=edit;fixtureMotion.targetHz=180;fixtureMotion.enabled=1;
        current.mode=0;current.aa=0;current.vsync=1;edit=current;Check(FpsNote().find("V-Sync is on")==0,"windowed V-Sync explains why a higher FPS stays at 60");
        current.aa=4;edit=current;Check(FpsNote().empty(),"windowed MSAA uses desktop composition and needs no V-Sync note");
        current.mode=2;edit=current;Check(FpsNote().find("V-Sync is on")==0,"fullscreen V-Sync with MSAA still explains the 60 FPS limit");
        edit.vsync=0;Check(FpsNote().empty(),"note follows the staged display choice before it is applied");edit=current;
        fixtureMotion.targetHz=60;fixtureMotion.enabled=0;Check(FpsNote().empty(),"native 60 FPS needs no note");
        fixtureMotion.targetHz=180;fixtureMotion.enabled=1;current.vsync=0;edit=current;fixtureStatus=motion::Incompatible;Check(FpsNote().find("could not start")!=std::string::npos,"failed FPS hooks are reported instead of silently showing 60");
        fixtureStatus=motion::Waiting;Check(FpsNote().empty(),"working unsynchronized FPS target shows no note");
        current=savedCurrent;edit=savedEdit;}
    fixtureMotion=motion::Settings{};motionApi=nullptr;tab=0;row=0;
    tab=0;row=0;Check(TabCount()==3,"menu has only Display, Graphics and Interface categories");
    Settings a,b;b.brightness=5;b.filter=4;b.fov=150;Check(!DisplayChanged(a,b),"visual and FOV changes do not request display reset");b.mode=2;Check(DisplayChanged(a,b),"fullscreen changes request reset");
    Confirmation c;c.Begin(100);Check(!c.Expired(15099)&&c.Expired(15100),"preview expires after exactly 15 seconds");c.End();Check(!c.Expired(20000),"accepted preview cannot expire");
    Check(Cycle(std::vector<int>{0,2,4},0,-1)==4&&Cycle(std::vector<int>{0,2,4},4,1)==0,"capability selection wraps only through supported values");
    HMODULE lib=LoadLibraryW(L"d3d8.dll");Check(lib!=nullptr,"load system Direct3D 8");
    using CreateFn=void*(__stdcall*)(UINT);auto create=reinterpret_cast<CreateFn>(GetProcAddress(lib,"Direct3DCreate8"));void* d3d=create(220);Check(d3d!=nullptr,"create Direct3D 8 interface");
    testWindow=CreateWindowExW(0,L"STATIC",L"Video Settings renderer test",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    testPresent={1280,720,D3DFMT_X8R8G8B8,1,D3DMULTISAMPLE_NONE,D3DSWAPEFFECT_DISCARD,testWindow,TRUE,TRUE,D3DFMT_D16,D3DPRESENTFLAG_LOCKABLE_BACKBUFFER,0,0};
    Check(SUCCEEDED(D8<15>(d3d,UINT(0),D3DDEVTYPE_HAL,testWindow,DWORD(D3DCREATE_SOFTWARE_VERTEXPROCESSING),&testPresent,&testDevice)),"create hidden hardware rendering device");
    VerifyEnemyGpu();VerifySelfShadows();
    {
        void* originalColor=nullptr,*originalDepth=nullptr,*smallColor=nullptr,*largeDepth=nullptr;D3DVIEWPORT9 viewport{};
        D8<32>(testDevice,&originalColor);D8<33>(testDevice,&originalDepth);D8<41>(testDevice,&viewport);
        Check(SUCCEEDED(D8<25>(testDevice,UINT(256),UINT(256),D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,BOOL(FALSE),&smallColor)),"create actual D3D8 Original-size color target");
        Check(SUCCEEDED(D8<26>(testDevice,UINT(1024),UINT(1024),D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&largeDepth)),"create actual D3D8 shared Ultra depth buffer");
        Check(SUCCEEDED(D8<31>(testDevice,smallColor,largeDepth)),"D3D8 accepts 256px color with retained 1024px depth");
        D3DVIEWPORT9 originalViewport{2,2,252,252,0,1};Check(SUCCEEDED(D8<40>(testDevice,&originalViewport)),"Original comparison viewport fits retained depth");
        Check(SUCCEEDED(D8<31>(testDevice,originalColor,originalDepth)),"restore normal renderer targets after comparison");D8<40>(testDevice,&viewport);
        D8<2>(smallColor);D8<2>(largeDepth);D8<2>(originalColor);if(originalDepth)D8<2>(originalDepth);
    }
    ui::device=testDevice;ui::fontPath=L"C:/Program Files (x86)/SEGA/SONICHEROES/dvdroot/font/a_euascii_0.png";Check(ui::MakeAtlas()&&ui::nativeFont,"native bitmap font parses 91 game glyphs");art::titlePath=L"assets/video-title.png";art::LoadTitle();Check(art::title.texture!=nullptr,"generated transparent title loads into a managed texture");
    Check(art::title.aspect>8&&art::title.aspect<10,"title UVs exclude nearly transparent generation margins");
    auto load=[](const wchar_t* path){auto sprite=ui::LoadSprite(path,false);Check(sprite.texture!=nullptr,"load research-only native art for isolated preview");art::retained.push_back(sprite.texture);return sprite.texture;};
    art::hex={load(L"research/assets-fe/option_007.png"),15.f/256,11.f/256,125.f/256,138.f/256,125.f/138};
    art::litHex={load(L"research/assets-fe/option_031.png"),15.f/256,11.f/256,125.f/256,138.f/256,125.f/138};
    art::selectionGlow=load(L"research/assets-fe/option_046.png");
    art::header={load(L"research/assets-fe/option_001.png"),0,0,483.f/512,48.f/64,483.f/48};
    art::idle=load(L"research/assets-fe/option_064.png");art::selected=load(L"research/assets-fe/option_065.png");
    art::audioTitle=load(L"research/assets-fe/option_034.png");art::vibrationTitle=load(L"research/assets-fe/option_037.png");art::banner=load(L"research/assets-fe/option_078.png");
    Check(art::MakeVideoWord(),"assemble native Video glyphs without neighboring letter outlines");
    art::sonic={load(L"research/assets-bg/bg_013.png"),0,0,268.f/512,58.f/64,268.f/58};art::heroes={load(L"research/assets-bg/bg_015.png"),25.f/512,0,345.f/512,58.f/64,345.f/58};
    art::promptPath=L"../Reloaded-II/Mods/sonicheroes.guis.widescreenrevamp/Redirector/dvdroot/advertise/E/adv_bar.one";
    art::LoadPrompts();Check(art::prompts[0]&&art::prompts[1]&&art::prompts[2]&&art::prompts[3],"read original prompt icons from installed ONE archive without redistributing them");
    // Reproduce the missing branch: a cached native shape submits an indexed
    // 3D mesh and NEVER calls either RenderWare immediate-2D callback.
    {
        struct CachedVertex {float x,y,z;DWORD color;float u,v;};
        std::array<CachedVertex,4> cachedVertices{};float positions[12]{},texcoords[8]{};
        const float us[]={15.f/256,140.f/256,140.f/256,15.f/256},vs[]={11.f/256,11.f/256,149.f/256,149.f/256};
        for(int i=0;i<4;i++){
            positions[i*3]=620+(us[i]*256-70)*1.5f;positions[i*3+1]=410+(vs[i]*256-73)*1.5f;positions[i*3+2]=.5f;
            texcoords[i*2]=us[i];texcoords[i*2+1]=vs[i];cachedVertices[i]={positions[i*3],positions[i*3+1],.5f,0xffffffff,us[i],vs[i]};
        }
        std::array<char,16> representation{};std::array<char,0x60> geometry{};std::array<char,0x20> morph{};
        auto shape=shapes[3].data();Field<void*>(shape,0xac)=representation.data();Field<uintptr_t>(shape,0xb4)=1;
        Field<void*>(representation.data(),8)=geometry.data();Field<int>(geometry.data(),0x14)=4;Field<int>(geometry.data(),0x18)=1;Field<int>(geometry.data(),0x1c)=1;
        Field<void*>(geometry.data(),0x34)=texcoords;Field<void*>(geometry.data(),0x5c)=morph.data();Field<void*>(morph.data(),0x14)=positions;
        float parentMult[]={1,1,1,.5f},parentAdd[]={0,0,0,0};for(int i=0;i<4;i++)Field<float>(shape,0x8c+i*4)=1;
        D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
        D3DMATRIX projection=identity;projection._11=2.f/1280;projection._22=-2.f/720;projection._41=-1;projection._42=1;
        D3DMATRIX oldWorld{},oldView{},oldProjection{};D8<38>(testDevice,D3DTS_WORLD,&oldWorld);D8<38>(testDevice,D3DTS_VIEW,&oldView);D8<38>(testDevice,D3DTS_PROJECTION,&oldProjection);
        D8<37>(testDevice,D3DTS_WORLD,&identity);D8<37>(testDevice,D3DTS_VIEW,&identity);D8<37>(testDevice,D3DTS_PROJECTION,&projection);
        void* vb=nullptr;void* ib=nullptr;DWORD fvf=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
        Check(SUCCEEDED(D8<23>(testDevice,UINT(sizeof(cachedVertices)),DWORD(0),fvf,D3DPOOL_MANAGED,&vb)),"create retained vertex buffer");
        unsigned short indices[]={0,1,2,0,2,3};Check(SUCCEEDED(D8<24>(testDevice,UINT(sizeof(indices)),DWORD(0),D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib)),"create retained index buffer");
        void* locked=nullptr;Check(SUCCEEDED(D8<11>(vb,UINT(0),UINT(sizeof(cachedVertices)),&locked,DWORD(0))),"fill retained vertex buffer");memcpy(locked,cachedVertices.data(),sizeof(cachedVertices));D8<12>(vb);
        Check(SUCCEEDED(D8<11>(ib,UINT(0),UINT(sizeof(indices)),&locked,DWORD(0))),"fill retained index buffer");memcpy(locked,indices,sizeof(indices));D8<12>(ib);
        D8<83>(testDevice,UINT(0),vb,UINT(sizeof(CachedVertex)));D8<85>(testDevice,ib,UINT(0));D8<76>(testDevice,fvf);D8<61>(testDevice,DWORD(0),art::hex.texture);
        RS(testDevice,D3DRS_LIGHTING,FALSE);RS(testDevice,D3DRS_CULLMODE,D3DCULL_NONE);
        auto oldShape=originalShape;auto oldIndexed=originalIndexed;originalShape=&CachedShapeFixture;originalIndexed=reinterpret_cast<IndexedFn>((*reinterpret_cast<void***>(testDevice))[71]);
        nativeHexes.Clear();buttonShapes.fill(nullptr);buttonShapes[3]=shape;buttonPoses.fill({});rootPose={};rootVisible=true;rootEntering=false;ready=true;focused=true;nativeChoice=3;pageMotion.Snap(false);lastMenu=GetTickCount64();
        D8<34>(testDevice);ObserveShape(shape,parentMult,parentAdd);D8<35>(testDevice);
        Check(nativeHexes.count==0&&buttonPoses[3].enabled,"cached draw acquires Language pose without any immediate sprite callback");
        Check(fabsf(buttonPoses[3].x-620)<.01f&&fabsf(buttonPoses[3].y-410)<.01f&&fabsf(buttonPoses[3].ax-4.f/3)<.001f&&(buttonPoses[3].tint>>24)==127,"retained mesh projection preserves native center, scale and entrance alpha");
        Check(renderingShape==nullptr&&renderingTint==0xffffffff,"cached shape context restores after rendering");
        D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Check(rootPose.enabled&&fabsf(rootPose.x-822.5f)<.01f,"TV rendering uses retained Language pose at its adjacent column");
        Capture(L"build/preview-retained.bmp",1280,720);
        // With only the TV pose absent this same region is background. Actual
        // retained capture must visibly change the TV center in the backbuffer.
        DWORD tvPixel=Pixel(823,410);rootPose={};buttonPoses.fill({});D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);
        Check(Pixel(823,410)!=tvPixel,"retained-only native path produces visible TV pixels");
        originalShape=oldShape;originalIndexed=oldIndexed;buttonShapes.fill(nullptr);buttonPoses.fill({});rootPose={};
        D8<83>(testDevice,UINT(0),static_cast<void*>(nullptr),UINT(0));D8<85>(testDevice,static_cast<void*>(nullptr),UINT(0));D8<2>(vb);D8<2>(ib);
        D8<37>(testDevice,D3DTS_WORLD,&oldWorld);D8<37>(testDevice,D3DTS_VIEW,&oldView);D8<37>(testDevice,D3DTS_PROJECTION,&oldProjection);
    }
    current=Settings{};current.width=1920;current.height=1080;current.filter=6;edit=current;ready=true;rootVisible=true;open=true;lastMenu=GetTickCount64();tab=0;row=0;
    focused=true;open=false;art::nativeTitles.push_back(art::idle);D8<61>(testDevice,DWORD(0),art::idle);
    Check(NativeOpacity(testDevice)==0,"focused TV suppresses only the known native title texture");
    D8<61>(testDevice,DWORD(0),art::selected);Check(NativeOpacity(testDevice)==1,"unrelated native artwork is preserved");
    D8<61>(testDevice,DWORD(0),art::selectionGlow);Check(NativeOpacity(testDevice)==0,"TV focus suppresses the previous center glow");
    D8<61>(testDevice,DWORD(0),art::litHex.texture);Check(NativeOpacity(testDevice)==1,"previous selected hex geometry remains visible");
    {NativeReplacement replacement(testDevice);void* bound=nullptr;D8<60>(testDevice,DWORD(0),&bound);Check(bound==art::hex.texture,"previous selected frame uses the original unlit hex");if(bound)D8<2>(bound);}
    void* restored=nullptr;D8<60>(testDevice,DWORD(0),&restored);Check(restored==art::litHex.texture,"native texture binding is restored after replacement");if(restored)D8<2>(restored);
    focused=false;Check(NativeOpacity(testDevice)==1,"returning native focus restores native highlight rendering");focused=true;
    D8<61>(testDevice,DWORD(0),art::idle);ui::drawing=true;Check(NativeOpacity(testDevice)==1,"custom UI bypasses native title suppression");ui::drawing=false;
    pageMotion.Snap(true);art::foreground.push_back(art::selected);D8<61>(testDevice,DWORD(0),art::selected);Check(NativeOpacity(testDevice)==0,"open video page suppresses native option foreground");
    D8<61>(testDevice,DWORD(0),art::sonic.texture);Check(NativeOpacity(testDevice)==1,"native scrolling watermark is never suppressed");
    D8<61>(testDevice,DWORD(0),art::header.texture);Check(NativeOpacity(testDevice)==1,"native header is preserved at its original scale");
    DWORD stageBefore=0,stageAfter=0,factorBefore=0,factorAfter=0;
    D8<62>(testDevice,DWORD(1),DWORD(4),&stageBefore);D8<51>(testDevice,D3DRS_TEXTUREFACTOR,&factorBefore);
    {NativeFade fade(testDevice,.5f);D8<51>(testDevice,D3DRS_TEXTUREFACTOR,&factorAfter);Check((factorAfter>>24)==127,"transition modulates native sprite alpha");}
    D8<62>(testDevice,DWORD(1),DWORD(4),&stageAfter);D8<51>(testDevice,D3DRS_TEXTUREFACTOR,&factorAfter);
    Check(stageBefore==stageAfter&&factorBefore==factorAfter,"native fade restores caller alpha state");
    D8<34>(testDevice);ui::Begin(testDevice,1280,720);ui::Quad(0,0,960,540,0xff204060);ui::Flush();
    {NativeFade fade(testDevice,.5f);ui::Quad(10,10,100,100,0xffffffff);ui::Flush();}
    ui::End();D8<35>(testDevice);
    DWORD faded=Pixel(40,40);Check(abs(int((faded>>16)&255)-143)<=2&&abs(int((faded>>8)&255)-159)<=2&&abs(int(faded&255)-175)<=2,"real D3D8 draw crossfades over existing background");
    D8<61>(testDevice,DWORD(0),static_cast<void*>(nullptr));art::nativeTitles.clear();art::foreground.clear();focused=false;open=true;
    RS(testDevice,D3DRS_FOGENABLE,TRUE);DWORD before=0,after=0;D8<51>(testDevice,D3DRS_FOGENABLE,&before);
    Check(SUCCEEDED(D8<34>(testDevice)),"begin scene");Background();Render(1280,720);Check(SUCCEEDED(D8<35>(testDevice)),"end scene");D8<51>(testDevice,D3DRS_FOGENABLE,&after);Check(before==after,"UI restores caller render state");
    Capture(L"build/preview-display.bmp",1280,720);
    DWORD nativeBackground=Pixel(10,610);
    D8<34>(testDevice);Render(1280,720);D8<35>(testDevice);Check(Pixel(10,610)==nativeBackground,"video foreground leaves native background pixels unchanged");
    D8<34>(testDevice);ui::Begin(testDevice,1280,720);ui::Quad(0,440,70,60,0xff572985);ui::End();Render(1280,720);D8<35>(testDevice);
    Check(Pixel(10,610)==0x572985,"updated background remains visible while video page is open");
    Check(PadDirection(0x1000000)==1&&PadDirection(0x2000000)==2&&PadDirection(0x4000000)==4&&PadDirection(0x8000000)==8,"left stick bits map to menu up/down/left/right");
    Check(PadDirection(0x10)==1&&PadDirection(0x20)==2&&PadDirection(0x40)==4&&PadDirection(0x80)==8&&PadDirection(0)==0&&PadDirection(0x4001)==0,"D-pad still navigates and buttons are not directions");
    Check(PadDirection(0x1000000|0x8000000)==1&&PadDirection(0x2000000|0x4000000)==2,"diagonal stick prefers vertical, like the D-pad");
    Check(RowDisabled(1,6)&&RowDisabled(1,7)&&!RowDisabled(1,5)&&!RowDisabled(0,6)&&!RowDisabled(2,1)&&Value(1,6)=="Disabled"&&Value(1,7)=="Disabled","object and self shadow rows are disabled");
    Check(StepRow(1,5,1)==0&&StepRow(1,0,-1)==5&&StepRow(1,4,1)==5,"Graphics navigation skips the disabled shadow rows");
    {auto savedTab=tab,savedRow=row;auto saved=current;tab=1;row=6;Adjust(1);row=7;Adjust(-1);Check(current.objectShadows==saved.objectShadows&&current.selfShadows==saved.selfShadows,"disabled rows cannot change object or self shadows");tab=savedTab;row=savedRow;}
    enemy_shadows::installed=true;current.enemyShadows=1;Check(SelectableRows(1)==8&&std::string(Label(1,5))=="Enemy shadows"&&Value(1,5)=="Animated model","enemy model option is reachable on Graphics");
    lastMenu=GetTickCount64();tab=1;row=5;D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-graphics.bmp",1280,720);
    motionApi=&fixtureMotionApi;lastMenu=GetTickCount64();tab=0;row=5;D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-fps-display.bmp",1280,720);
    tab=2;row=2;D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-fps-interface.bmp",1280,720);
    motion::Stats fps;fps.renderFps=180;fps.baseFps=60;fps.peakMs=5.6f;fixtureMotion.enabled=1;fixtureMotion.targetHz=180;
    D3DVIEWPORT9 beforeFps{},afterFps{};D8<41>(testDevice,&beforeFps);DrawFps(testDevice,&fps);D8<41>(testDevice,&afterFps);Check(!memcmp(&beforeFps,&afterFps,sizeof(beforeFps)),"FPS overlay restores the native viewport");Capture(L"build/preview-fps.bmp",1280,720);
    {auto savedCurrent=current,savedEdit=edit;current.mode=0;current.aa=0;current.vsync=1;edit=current;tab=0;row=5;lastMenu=GetTickCount64();
        D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-fps-vsync-note.bmp",1280,720);
        fps.renderFps=60;fps.status=motion::NeedVsyncOff;DrawFps(testDevice,&fps);Capture(L"build/preview-fps-vsync.bmp",1280,720);current=savedCurrent;edit=savedEdit;}
    motionApi=nullptr;
    lastMenu=GetTickCount64();tab=0;confirmation.Begin(GetTickCount64());D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-confirm.bmp",1280,720);confirmation.End();
    keyboardHints=true;lastMenu=GetTickCount64();D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-keyboard.bmp",1280,720);keyboardHints=false;
    // A source-asset root composition, not a screenshot of the running game.
    // Native hexes and the mod TV use the same 1.125 logical scale.
    std::vector<ui::Sprite> icons;
    for(const wchar_t* name:{L"option_004",L"option_010",L"option_016",L"option_013",L"option_019",L"option_022",L"option_025",L"option_028"}){
        std::wstring path=L"research/assets-fe/";path+=name;path+=L".png";auto sprite=ui::LoadSprite(path.c_str());Check(sprite.texture!=nullptr,"load native root artwork");icons.push_back(sprite);
    }
    D8<34>(testDevice);ui::Begin(testDevice,1280,720);
    ui::Quad(0,0,960,540,0xfff2cf2b);ui::Quad(0,0,960,122,0xffd8d8d8);
    for(int x=0;x<960;x+=28)ui::Quad(float(x),0,1,122,0x70ffffff);
    for(int y=0;y<122;y+=28)ui::Quad(0,float(y),960,1,0x70ffffff);
    ui::Quad(0,122,960,294,0xffc3a612);for(int y=124;y<416;y+=6)ui::Quad(0,float(y),960,2,0x20745e00);
    ui::Circle(31,45,240,0x70647f26);ui::Circle(27,43,197,0xff82c21b);
    ui::Image(art::header,116,29,550,56);ui::Image(icons[0],176,42,icons[0].aspect*32,32);
    auto nativeHex=[](float x,float y){ui::Image(art::hex,x-55*1.125f,y-62*1.125f,125*1.125f,138*1.125f);};
    nativeHex(461,203);nativeHex(613,203);nativeHex(385,337.5f);nativeHex(537.5f,337.5f);
    auto icon=[&](int index,float x,float y,float h){ui::Image(icons[index],x-icons[index].aspect*h*.5f,y-h*.5f,icons[index].aspect*h,h);};
    icon(1,461,201,100);icon(2,613,210,87);icon(3,613,168,24);icon(4,385,353,67);icon(5,385,322,64);icon(6,537.5f,351,74);icon(7,537.5f,302,27);
    ui::Image(art::sonic,-15,438,350,76,0x48fff6cb);ui::Image(art::heroes,350,438,450,76,0x48fff6cb);ui::End();
    open=false;pageMotion.Snap(false);focused=true;lastMenu=GetTickCount64();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-root.bmp",1280,720);
    D8<34>(testDevice);Background();ui::Begin(testDevice,1280,720);art::TV(220,265,1.4f,false);art::TV(440,265,1.4f,true,.25f);art::TV(660,265,1.4f,true,.75f);art::Title(176,42,32);ui::End();D8<35>(testDevice);Capture(L"build/preview-hover.bmp",1280,720);
    open=true;pageMotion.from=pageMotion.to=.5f;lastMenu=GetTickCount64();D8<34>(testDevice);Background();Render(1280,720);D8<35>(testDevice);Capture(L"build/preview-transition.bmp",1280,720);
    for(auto sprite:icons)D8<2>(sprite.texture);open=true;pageMotion.Snap(true);focused=false;
    Present8 resized=testPresent;resized.width=1024;resized.height=768;Check(SUCCEEDED(D8<14>(testDevice,&resized)),"managed font atlas survives device reset");
    lastMenu=GetTickCount64();tab=2;D8<34>(testDevice);Background(1024,768);Render(1024,768);D8<35>(testDevice);Capture(L"build/preview-4x3.bmp",1024,768);
    art::ClearNative();D8<2>(art::title.texture);art::title={};
    for(void* p:art::prompts)if(p)D8<2>(p);
    if(ui::atlas){D8<2>(ui::atlas);ui::atlas=nullptr;}D8<2>(testDevice);D8<2>(d3d);DestroyWindow(testWindow);
    std::cout<<"All isolated checks passed. This does not test RenderWare reset or in-game navigation.\n";
    return 0;
}



