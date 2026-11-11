// P001 actual exported Windows VST3 real-time allocation audit.
// Test-only, not linked into the product DLL: MinHook intercepts ntdll's
// RtlAllocateHeap, including allocations promptly freed within process().
// Positive control verifies the hook *actually catches* HeapAlloc.
// The original pinned EMU8000 C kernel is not changed in any way.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "MinHook.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <stdexcept>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(bool result, const char* why) {
    if (!result) throw std::runtime_error(why);
}
using AllocateHeap = PVOID (NTAPI*)(PVOID, ULONG, SIZE_T);
AllocateHeap original_allocate = nullptr;
thread_local bool audio_scope = false;
thread_local std::uint64_t heap_calls = 0;
thread_local std::uint64_t heap_bytes = 0;
PVOID NTAPI intercept_allocate(PVOID heap, ULONG flags, SIZE_T bytes) {
    if (audio_scope) {
        ++heap_calls;
        heap_bytes += bytes;
    }
    return original_allocate(heap, flags, bytes);
}
class HeapMonitor {
public:
    HeapMonitor() {
        require(MH_Initialize()==MH_OK, "MinHook initialization failed");
        auto rc = MH_CreateHookApi(
            L"ntdll.dll", "RtlAllocateHeap",
            reinterpret_cast<LPVOID>(&intercept_allocate),
            reinterpret_cast<LPVOID*>(&original_allocate));
        require(rc==MH_OK && original_allocate, "cannot intercept ntdll RtlAllocateHeap");
        require(MH_EnableHook(MH_ALL_HOOKS)==MH_OK, "cannot enable heap interception");
        // Adversarial positive control: if interception is inert this test FAILS.
        const auto before = heap_calls;
        audio_scope = true;
        void* sample = HeapAlloc(GetProcessHeap(), 0, 64);
        audio_scope = false;
        require(sample!=nullptr, "test-hook HeapAlloc failed");
        HeapFree(GetProcessHeap(),0,sample);
        require(heap_calls>before && heap_bytes>=64, "allocation-hook positive control failed");
        heap_calls=0; heap_bytes=0;
    }
    ~HeapMonitor() {
        audio_scope=false;
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    HeapMonitor(const HeapMonitor&) = delete;
    HeapMonitor& operator=(const HeapMonitor&) = delete;
};
struct Result { tresult code; uint64_t calls; uint64_t bytes; };
Result guardedProcess(IAudioProcessor* processor, ProcessData& packet) {
    // No host EventList creation, logging, buffer resizing or allocations
    // occur within this scope: only the actual exported VST3 process().
    const auto before = heap_calls;
    const auto previousBytes = heap_bytes;
    audio_scope = true;
    const tresult result = processor->process(packet);
    audio_scope = false;
    return {result, heap_calls-before, heap_bytes-previousBytes};
}
struct Instance {
    IComponent* component = nullptr;
    IAudioProcessor* audio = nullptr;
    explicit Instance(IPluginFactory* factory) {
        PClassInfo info{};
        bool found=false;
        for(int32 i=0;i<factory->countClasses();++i) {
            require(factory->getClassInfo(i,&info)==kResultOk, "VST3 class enumeration");
            if(std::strcmp(info.category,kVstAudioEffectClass)==0) {
                found=true; break;
            }
        }
        require(found, "No real exported instrument class");
        require(factory->createInstance(info.cid, IComponent::iid,
            reinterpret_cast<void**>(&component))==kResultOk && component,
            "Cannot instantiate real exported VST3");
        require(component->initialize(nullptr)==kResultOk, "VST3 component initialize");
        require(component->queryInterface(IAudioProcessor::iid,
            reinterpret_cast<void**>(&audio))==kResultOk && audio,
            "VST3 IAudioProcessor missing");
        SpeakerArrangement stereo=SpeakerArr::kStereo;
        require(audio->setBusArrangements(nullptr,0,&stereo,1)==kResultTrue,
                "Stereo instrument negotiation failed");
        ProcessSetup good{kRealtime,kSample32,1024,44100.0};
        require(audio->setupProcessing(good)==kResultOk, "44.1 kHz float host negotiation failed");
        require(component->setActive(true)==kResultOk, "Real owner-identical Creative ROM failed activation");
        require(audio->setProcessing(true)==kResultOk, "VST3 setProcessing true failed");
    }
    ~Instance() {
        if (audio) { audio->setProcessing(false); audio->release(); }
        if (component) { component->setActive(false); component->terminate(); component->release(); }
    }
};
void midiEvent(EventList& list, bool on, int offset) {
    Event event{};
    event.type=on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
    event.busIndex=0;
    event.sampleOffset=offset;
    if (on) {
        event.noteOn.channel=0;
        event.noteOn.pitch=60;
        event.noteOn.velocity=0.87f;
        event.noteOn.noteId=77;
    } else {
        event.noteOff.channel=0;
        event.noteOff.pitch=60;
        event.noteOff.noteId=77;
    }
    require(list.addEvent(event)==kResultOk, "Cannot enqueue original-ROM MIDI in host");
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    HMODULE module=nullptr;
    IPluginFactory* factory=nullptr;
    try {
        require(SetEnvironmentVariableW(L"AWE64_P001_ROM_PATH",argv[2])!=0,
                "Cannot configure original private ROM");
        // Assert original fork's EMU8K_STATE_DUMP cannot fprintf from DAW thread.
        wchar_t temp[MAX_PATH]={};
        require(GetTempPathW(MAX_PATH,temp)>0,"Cannot get private temporary folder");
        wchar_t debugFile[MAX_PATH]={};
        require(wcslen(temp)+40<MAX_PATH,"Temporary folder path too long");
        wcscpy_s(debugFile, temp);
        wcscat_s(debugFile,L"125a-p001-render-must-not-write.txt");
        DeleteFileW(debugFile);
        // getenv() in the original C kernel reads the UCRT environment,
        // which SetEnvironmentVariableW() alone does NOT necessarily refresh.
        // _wputenv_s updates the C runtime AND the process environment.
        require(_wputenv_s(L"EMU8K_STATE_DUMP",debugFile)==0,
                "Could not arm hostile fork debug environment in C runtime");
        require(_wputenv_s(L"EMU8K_STATE_STEP",L"64")==0,
                "Could not arm hostile fork debug frame step in C runtime");
        require(std::getenv("EMU8K_STATE_DUMP") != nullptr,
                "Hostile C getenv state dump not activated; test would be vacuous");
        module=LoadLibraryW(argv[1]);
        require(module!=nullptr, "Cannot load actual Windows VST3 DLL");
        auto getter=reinterpret_cast<IPluginFactory*(*)()>(
            GetProcAddress(module,"GetPluginFactory"));
        require(getter!=nullptr,"VST3 GetPluginFactory export missing");
        factory=getter();
        require(factory!=nullptr,"Null VST3 factory");
        {
            Instance plugin(factory);
            HeapMonitor trace;  // installs instrumentation OUTSIDE audio callback
            constexpr int kBlocks=1200;
            constexpr std::array<int32,8> sizes{1,7,32,64,128,255,512,1024};
            std::array<float,1024> left{},right{};
            float* channels[2]={left.data(),right.data()};
            AudioBusBuffers out{};
            out.numChannels=2;
            out.channelBuffers32=channels;
            std::uint64_t totalFrames=0,totalNonzero=0,totalCalls=0,totalBytes=0;
            LARGE_INTEGER freq{}, begin{}, end{};
            require(QueryPerformanceFrequency(&freq)!=0,"No monotonic high precision clock");
            long long maximumTick=0;
            for (int i=0;i<kBlocks;++i) {
                const auto frames=sizes[size_t(i)%sizes.size()];
                std::fill_n(left.data(),frames,0.f);
                std::fill_n(right.data(),frames,0.f);
                EventList events; // created OUTSIDE the guarded processing callback
                if(i%40==0) midiEvent(events,true,std::min(17,frames-1));
                if(i%40==25) midiEvent(events,false,std::min(11,frames-1));
                ProcessData data{};
                data.processMode=kRealtime;
                data.symbolicSampleSize=kSample32;
                data.numSamples=frames;
                data.numOutputs=1;
                data.outputs=&out;
                data.inputEvents=&events;
                require(QueryPerformanceCounter(&begin)!=0,"Start clock failed");
                const auto rc=guardedProcess(plugin.audio,data);
                require(QueryPerformanceCounter(&end)!=0,"Stop clock failed");
                require(rc.code==kResultOk, "Actual VST3 callback returned failure");
                if (rc.calls || rc.bytes) {
                    std::cerr<<"P001_RT_HEAP_ALLOCATION_DETECTED block="<<i
                             <<" frames="<<frames<<" calls="<<rc.calls
                             <<" bytes="<<rc.bytes<<'\n';
                    throw std::runtime_error("ACTUAL VST3 process() allocated on render thread");
                }
                totalCalls+=rc.calls;
                totalBytes+=rc.bytes;
                totalFrames+=frames;
                maximumTick=std::max(maximumTick,end.QuadPart-begin.QuadPart);
                for(int n=0;n<frames;++n) {
                    require(left[size_t(n)]==left[size_t(n)] &&
                            right[size_t(n)]==right[size_t(n)],
                            "NaN in original ROM callback PCM");
                    totalNonzero += left[size_t(n)]!=0.f;
                    totalNonzero += right[size_t(n)]!=0.f;
                }
            }
            require(totalNonzero>5000,"Original ROM render stress produced no audible notes");
            // If original fork debug FILE* was not blocked at activation, a
            // 64-frame state-step would write a substantial file in 1200 blocks.
            WIN32_FILE_ATTRIBUTE_DATA stat{};
            require(GetFileAttributesExW(debugFile,GetFileExInfoStandard,&stat)!=0,
                    "No debug-path file created during original fork init");
            require(stat.nFileSizeHigh==0 && stat.nFileSizeLow==0,
                    "Original fork fprintf wrote from VST3 audio thread");
            const double worstUs=double(maximumTick)*1e6/double(freq.QuadPart);
            std::cout<<"P001_ACTUAL_VST3_RT_ZERO_HEAP_ALLOC_PASS"
                     <<" calls="<<kBlocks<<" frames="<<totalFrames
                     <<" nonzero_pcm="<<totalNonzero
                     <<" rtl_allocate_heap_calls="<<totalCalls
                     <<" allocation_bytes="<<totalBytes
                     <<" worst_process_us_on_shared_CI="<<worstUs
                     <<" fork_state_dump_bytes=0"
                     <<"\n";
            // Shared CI is not a real ASIO device: observed wall time is NOT
            // a scheduler deadline guarantee and this is NOT DAW certification.
        }
        factory->release(); factory=nullptr;
        FreeLibrary(module); module=nullptr;
        _wputenv_s(L"EMU8K_STATE_DUMP",L"");
        _wputenv_s(L"EMU8K_STATE_STEP",L"");
        DeleteFileW(debugFile);
        return 0;
    } catch(const std::exception& e) {
        if(factory)factory->release();
        if(module)FreeLibrary(module);
        std::cerr<<"P001_ACTUAL_VST3_RT_AUDIT_FAIL "<<e.what()<<'\n';
        return 1;
    }
}
