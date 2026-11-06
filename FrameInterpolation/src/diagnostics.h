#pragma once
#include <windows.h>
#include <atomic>
#include <array>
#include <filesystem>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
namespace timing {
inline double Clock(){static const double inverse=[](){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return 1.0/double(f.QuadPart);}();LARGE_INTEGER n;QueryPerformanceCounter(&n);return double(n.QuadPart)*inverse;}
enum Phase {Capture,State,Match,Replay,Restore,Overlay,Present,Wait,Cleanup,BufferMirror,PhaseCount};
enum Reason:uint32_t {Inactive=1,NoHistory=2,Warming=4,Unmatched=8,EarlySlotMiss=16,LateReplay=32,Synchronized=64,Unsupported=128,Failed=256,PacingReduced=512,TextureResync=1024,StateRejected=2048,BufferRejected=4096,GeometryRejected=8192,TextureMutation=16384,FeedbackRejected=32768,CapacityRejected=65536,NativeDeferred=131072,DeferredBarrier=262144,BarrierTarget=524288,BarrierClear=1048576,BarrierUncaptured=2097152,BarrierResource=4194304,BarrierReadback=8388608,StreamingBuffer=33554432,Governed=67108864};
struct Record {
    uint64_t sequence=0,context=0;
    double time=0,tickMs=0,preDrawMs=0,renderMs=0,phase[PhaseCount]{};
    uint32_t target=60,menu=0,action=0,status=0,draws=0,bytes=0,submissions=0,generated=0,skipped=0,reasons=0,warmup=0;
    double estimateMs=0,maxPresentGapMs=0;uint32_t dropped=0,plannedImages=0,cached=0,imageLimit=0;
};
#pragma warning(push)
#pragma warning(disable:4324) // Intentional cache-line separation for producer/consumer.
template<size_t N> struct Queue {
    std::array<Record,N> entries{};
    alignas(64) std::atomic<uint32_t> write{0};
    alignas(64) std::atomic<uint32_t> read{0};
    std::atomic<uint32_t> dropped{0};
    bool Push(const Record& item){auto w=write.load(std::memory_order_relaxed);if(w-read.load(std::memory_order_acquire)==N){dropped.fetch_add(1,std::memory_order_relaxed);return false;}entries[w%N]=item;write.store(w+1,std::memory_order_release);return true;}
    bool Pop(Record& item){auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;item=entries[r%N];read.store(r+1,std::memory_order_release);return true;}
};
#pragma warning(pop)
static_assert(std::atomic<uint32_t>::is_always_lock_free,"Render logging requires lock-free queue indices");
inline Queue<8192> queue;
inline std::atomic<bool> enabled{false},stop{false};
inline std::atomic<uint32_t> ioError{0};
inline HANDLE worker=nullptr;
inline std::wstring session;
inline Record frame;
inline double previousSync=0,drawStart=0;
inline uint64_t sequence=0;
struct Scope {
    Phase phase;double start;
    explicit Scope(Phase p):phase(p),start(enabled.load(std::memory_order_relaxed)?Clock():0){}
    ~Scope(){if(start)frame.phase[phase]+=(Clock()-start)*1000;}
};
inline void BeginDraw(bool menu,uint64_t context,uint32_t action){
    if(!enabled.load(std::memory_order_relaxed))return;
    drawStart=Clock();frame.menu=menu;frame.context=context;frame.action=action;
    frame.preDrawMs=previousSync?(drawStart-previousSync)*1000:0;
}
inline void BeforePresent(){if(enabled.load(std::memory_order_relaxed)&&drawStart)frame.renderMs=(Clock()-drawStart)*1000;}
inline void FinishTick(uint32_t target,uint32_t status){
    double now=Clock();if(enabled.load(std::memory_order_relaxed)){
        frame.sequence=++sequence;frame.time=now;frame.tickMs=previousSync?(now-previousSync)*1000:0;
        frame.target=target;frame.status=status;frame.dropped=queue.dropped.load(std::memory_order_relaxed);queue.Push(frame);
    }
    frame={};drawStart=0;previousSync=now;
}
inline const char* Header="sequence,time_s,context,menu,action,target,status,tick_ms,pre_draw_ms,render_including_capture_ms,capture_ms,state_ms,match_ms,replay_ms,restore_ms,overlay_ms,present_ms,wait_ms,cleanup_ms,draws,bytes,submissions,generated,skipped,reason_bits,warmup,replay_estimate_ms,max_present_gap_ms,dropped,planned_images,buffer_mirror_ms,cached_draws,image_limit\n";
inline int Format(const Record& r,char* line,size_t capacity){return _snprintf_s(line,capacity,_TRUNCATE,
    "%llu,%.6f,%llu,%u,%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%u,%u,%u,%u,%.4f,%.4f,%u,%u,%.4f,%u,%u\n",
    r.sequence,r.time,r.context,r.menu,r.action,r.target,r.status,r.tickMs,r.preDrawMs,r.renderMs,
    r.phase[Capture],r.phase[State],r.phase[Match],r.phase[Replay],r.phase[Restore],r.phase[Overlay],r.phase[Present],r.phase[Wait],r.phase[Cleanup],
    r.draws,r.bytes,r.submissions,r.generated,r.skipped,r.reasons,r.warmup,r.estimateMs,r.maxPresentGapMs,r.dropped,r.plannedImages,r.phase[BufferMirror],r.cached,r.imageLimit);}
inline bool WriteAll(HANDLE file,const char* data,DWORD size){while(size){DWORD wrote=0;if(!WriteFile(file,data,size,&wrote,nullptr)||!wrote){ioError.store(GetLastError()?GetLastError():ERROR_WRITE_FAULT);return false;}data+=wrote;size-=wrote;}return true;}
inline DWORD WINAPI Writer(void*){
    // Four rotating 16 MiB segments bound a session's disk usage. Only this
    // session's own numbered files are replaced. Render thread never waits here.
    unsigned segment=0;uint64_t bytes=0;HANDLE file=INVALID_HANDLE_VALUE;
    auto open=[&](){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);auto path=session+L"-"+std::to_wstring(segment%4)+L".csv";file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE){ioError.store(GetLastError());return false;}bytes=strlen(Header);return WriteAll(file,Header,DWORD(bytes));};
    if(!open())return 1;
    std::array<char,65536> batch{};char line[1024];
    for(;;){size_t used=0;Record record;while(used+sizeof(line)<=batch.size()&&queue.Pop(record)){int n=Format(record,line,sizeof(line));if(n>0){memcpy(batch.data()+used,line,size_t(n));used+=size_t(n);}}
        if(used){if(bytes+used>16*1024*1024){++segment;if(!open())break;}if(!WriteAll(file,batch.data(),DWORD(used)))break;bytes+=used;}
        if(stop.load(std::memory_order_acquire)&&queue.read.load()==queue.write.load())break;
        if(!used)Sleep(100);
    }
    CloseHandle(file);return 0;
}
inline bool Start(const std::filesystem::path& folder){
    if(worker)return true;std::error_code ec;std::filesystem::create_directories(folder,ec);if(ec){ioError.store(ERROR_PATH_NOT_FOUND);return false;}
    SYSTEMTIME t;GetLocalTime(&t);wchar_t name[128];swprintf_s(name,L"fps-%04u%02u%02u-%02u%02u%02u-%lu",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId());session=(folder/name).wstring();stop=false;
    worker=CreateThread(nullptr,0,Writer,nullptr,0,nullptr);if(!worker){ioError.store(GetLastError());return false;}SetThreadPriority(worker,THREAD_PRIORITY_BELOW_NORMAL);return true;
}
inline void Stop(){stop.store(true,std::memory_order_release);if(worker){WaitForSingleObject(worker,5000);CloseHandle(worker);worker=nullptr;}}
}
