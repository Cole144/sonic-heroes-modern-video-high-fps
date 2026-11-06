#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace motion {
// Count successful submissions, not the configured interpolation target. The
// half-second window stays responsive; the longest gap is held for two seconds.
struct RateWindow {
    struct Bucket {int64_t id=-1;uint64_t count=0;};
    std::array<Bucket,128> buckets{};uint64_t total=0;double first=0,last=0;
    void Add(double now){
        if(!total)first=now;last=now;++total;
        int64_t id=int64_t(now*200);auto& b=buckets[size_t(id)%buckets.size()];
        if(b.id!=id)b={id,0};++b.count;
    }
    float At(double now)const{
        if(total<2||now-last>=.5)return 0;
        double span=std::min(.5,now-first);if(span<.1)return 0;
        int64_t end=int64_t(now*200),begin=end-100;double n=0;
        for(const auto& b:buckets)if(b.id>=begin&&b.id<=end)n+=double(b.count)*(b.id==begin?1-(now*200-double(end)):1);
        if(now-first<.5&&n)--n;return float(double(n)/span);
    }
};
struct FrameRates {
    RateWindow output,base;double last=-1,peakUntil=0;float lastMs=0,peakMs=0;
    void Presented(double now,bool native){
        if(last>=0){lastMs=float(std::max(0.0,now-last)*1000);if(now>=peakUntil||lastMs>=peakMs){peakMs=lastMs;peakUntil=now+2;}}
        last=now;output.Add(now);if(native)base.Add(now);
    }
    float Peak(double now)const{return std::max(now<peakUntil?peakMs:lastMs,last>=0?float(std::max(0.0,now-last)*1000):0.f);}
};
}
