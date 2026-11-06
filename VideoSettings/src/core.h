#pragma once
#include <algorithm>
#include <vector>

namespace video {
struct Settings {
    int mode=0, width=1920, height=1080, refresh=0, vsync=1, aa=0;
    int filter=0, mip=0, fog=1, shadows=1, brightness=0, fov=100, shadowQuality=0, enemyShadows=0, objectShadows=0, selfShadows=0;
};
inline void Sanitize(Settings& s) {
    s.mode=std::clamp(s.mode,0,2); s.width=std::clamp(s.width,640,7680);
    s.height=std::clamp(s.height,480,4320); s.refresh=std::clamp(s.refresh,0,360);
    s.vsync=!!s.vsync; if(s.aa!=0&&s.aa!=2&&s.aa!=4&&s.aa!=8)s.aa=0;
    s.filter=std::clamp(s.filter,0,6); s.mip=std::clamp(s.mip,-4,4);
    s.fog=!!s.fog; s.shadows=!!s.shadows;
    s.enemyShadows=!!s.enemyShadows;s.objectShadows=!!s.objectShadows;s.selfShadows=!!s.selfShadows;
    s.shadowQuality=std::clamp(s.shadowQuality,0,2);
    s.brightness=std::clamp(s.brightness,-10,10);
    s.fov=std::clamp(s.fov,50,150);
}
inline bool DisplayChanged(const Settings& a,const Settings& b) {
    return a.mode!=b.mode||a.width!=b.width||a.height!=b.height||a.refresh!=b.refresh||a.vsync!=b.vsync||a.aa!=b.aa;
}
template<class T> T Cycle(const std::vector<T>& values,T value,int direction) {
    if(values.empty())return value;
    auto it=std::find(values.begin(),values.end(),value);
    int i=it==values.end()?0:int(it-values.begin());
    return values[(i+direction+int(values.size()))%int(values.size())];
}
struct Confirmation {
    unsigned long long deadline=0;
    bool active=false;
    void Begin(unsigned long long now){active=true;deadline=now+15000;}
    bool Expired(unsigned long long now)const{return active&&now>=deadline;}
    void End(){active=false;deadline=0;}
};
struct MenuTransition {
    unsigned long long started=0;
    float from=0,to=0;
    static constexpr unsigned duration=250;
    float At(unsigned long long now)const{
        float t=std::clamp(float(now>=started?now-started:0)/duration,0.f,1.f);
        t=t*t*(3-2*t);return from+(to-from)*t;
    }
    bool Active(unsigned long long now)const{return from!=to&&now-started<duration;}
    void Begin(unsigned long long now,bool opening){from=At(now);to=opening?1.f:0.f;started=now;}
    void Snap(bool opening){from=to=opening?1.f:0.f;started=0;}
};
inline float Smooth(float value){value=std::clamp(value,0.f,1.f);return value*value*(3-2*value);}
inline float RootOpacity(float amount){return 1-Smooth(amount*2);}
inline float PageOpacity(float amount){return Smooth((amount-.45f)/.55f);}
inline float IconProgress(float amount){return Smooth(amount/.6f);}
}
