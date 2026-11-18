// P007W: exercise the ACTUAL Windows VST3 module through Steinberg hosting APIs.
// Public CI: real compiled VST3 getState/setState roundtrip, no private Win95.
// --private-audio: additionally boot the privately bundled original Win95 and
// verify real host->VST3 MIDI NoteOn/Off -> nonzero output PCM. Never upload it.
// This is a QA executable, NOT part of the shipping one-folder VST3.
#define NOMINMAX
#include <windows.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/common/memorystream.h"
#include "P007OriginalAudioProof.hpp"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace std::chrono_literals;

namespace {
constexpr ParamID bankId = 1002, programId = 1000, masterId = 1001;
constexpr int block = 1024;
void require(bool ok, const char* description) {
    if (!ok) {
        std::fprintf(stderr, "P007W_FAIL %s\n", description);
        throw std::runtime_error(description);
    }
}
bool closeTo(double a, double b) { return std::fabs(a-b) < 0.000001; }

struct Instance {
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    IPtr<IAudioProcessor> processor;
    bool initialized = false;
    Instance()=default;
    Instance(const Instance&)=delete;
    Instance& operator=(const Instance&)=delete;
    Instance(Instance&&)=default;
    Instance& operator=(Instance&&)=delete;
    ~Instance() {
        if (component && initialized) (void)component->terminate();
    }
};
Instance instantiate(const VST3::Hosting::PluginFactory& factory,
                     const VST3::UID& id, FUnknown* context) {
    Instance result;
    result.component = factory.createInstance<IComponent>(id);
    require(bool(result.component), "factory could not create IComponent");
    require(result.component->initialize(context)==kResultOk, "IComponent initialize");
    result.initialized=true;
    IEditController* edit=nullptr;
    require(result.component->queryInterface(IEditController::iid,
        reinterpret_cast<void**>(&edit))==kResultOk && edit!=nullptr,
        "combined IEditController missing");
    result.controller=owned(edit);
    IAudioProcessor* proc=nullptr;
    require(result.component->queryInterface(IAudioProcessor::iid,
        reinterpret_cast<void**>(&proc))==kResultOk && proc!=nullptr,
        "IAudioProcessor missing");
    result.processor=owned(proc);
    return result;
}
void rewind(IBStream& s) {
    require(s.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk, "IBStream seek");
}
std::array<char,44> readState(IComponent& component) {
    MemoryStream stream;
    require(component.getState(&stream)==kResultOk, "IComponent getState");
    require(stream.getSize()==44, "state is not canonical v2 44 bytes");
    std::array<char,44> result{};
    std::memcpy(result.data(), stream.getData(), result.size());
    return result;
}
void writeState(IComponent& component, const std::array<char,44>& bytes,
                bool shouldSucceed) {
    MemoryStream s;
    int32 written=0;
    require(s.write(const_cast<char*>(bytes.data()), 44, &written)==kResultOk
        && written==44, "QA IBStream write");
    rewind(s);
    const auto result=component.setState(&s);
    require((result==kResultOk)==shouldSucceed,
        shouldSucceed ? "valid IComponent setState" : "corrupt state accepted");
}
void stateRoundtrip(const VST3::Hosting::PluginFactory& factory,
                    const VST3::UID& id, FUnknown* host) {
    auto first=instantiate(factory,id,host);
    // Distinct values catch bank/program swapped or ignored in the real plugin.
    require(first.controller->setParamNormalized(bankId,71./127.)==kResultOk,
            "set bank");
    require(first.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "set program");
    require(first.controller->setParamNormalized(masterId,.75)==kResultOk,
            "set master");
    for(const auto [param,amount] : std::array<std::pair<ParamID,double>,3>{{
        {1100u,.25},{1108u,.60},{1110u,.80}}})
      require(first.controller->setParamNormalized(param,amount)==kResultOk,
              "P007AA set original Creative NRPN VST3 parameter");
    const auto saved=readState(*first.component);
    require(saved[0]=='P'&&saved[1]=='7'&&saved[2]=='S'&&saved[3]=='T',
            "VST3 stream signature");

    // Completely new instance: real component persistence, not codec-only test.
    auto reopened=instantiate(factory,id,host);
    writeState(*reopened.component,saved,true);
    require(closeTo(reopened.controller->getParamNormalized(bankId),71./127.),
            "bank missing after plugin reopen");
    require(closeTo(reopened.controller->getParamNormalized(programId),40./127.),
            "program missing after plugin reopen");
    require(closeTo(reopened.controller->getParamNormalized(masterId),.75),
            "master missing after plugin reopen");
    for(const auto [param,amount] : std::array<std::pair<ParamID,double>,3>{{
        {1100u,.25},{1108u,.60},{1110u,.80}}})
      require(closeTo(reopened.controller->getParamNormalized(param),amount),
              "P007AA Creative NRPN knob not restored from real VST3 state");
    require(readState(*reopened.component)==saved, "state bytes changed on reopen");

    // Verify the combined controller reload path, too (many hosts call both).
    MemoryStream controllerState;
    int32 written=0;
    require(controllerState.write(const_cast<char*>(saved.data()),44,&written)==kResultOk
            && written==44,"controller state fixture");
    rewind(controllerState);
    require(reopened.controller->setComponentState(&controllerState)==kResultOk,
            "IEditController setComponentState");
    require(readState(*reopened.component)==saved,"controller reload mismatch");

    // Corruption must leave the live plugin state completely untouched.
    auto broken=saved;
    broken[31]^=1; // corrupt NRPN value, original driver state remains unchanged
    writeState(*reopened.component,broken,false);
    require(readState(*reopened.component)==saved,"corrupt state modified plugin");
    std::puts("P007W_REAL_WINDOWS_VST3_TWO_INSTANCE_PROJECT_STATE_REOPEN_PASS");
}


void publicWindowsAudioLifecycle(const VST3::Hosting::PluginFactory& factory,
                                 const VST3::UID& id,FUnknown* host,
                                 const fs::path& bundle) {
    // Public CI intentionally has NO Win95/Creative assets. This verifies
    // the real Windows VST3 event->audio host API + activation, not sound.
    // NEVER call missing-guest silence a Creative MIDI-to-PCM PASS.
    const auto privateDisk=bundle/"Contents"/"Resources"/"PrivateVM"/"vm"/"win95.img";
    require(!fs::exists(privateDisk),
        "public QA must not run if a private licensed Win95 disk is present");
    unsigned calls=0, midiEvents=0;
    for(double sampleRate : {44100.,48000.,96000.}) {
        auto plugin=instantiate(factory,id,host);
        ProcessSetup setup{};
        setup.processMode=kRealtime;
        setup.symbolicSampleSize=kSample32;
        setup.maxSamplesPerBlock=block;
        setup.sampleRate=sampleRate;
        require(plugin.processor->canProcessSampleSize(kSample32)==kResultTrue,
            "actual plugin rejects 32bit float audio");
        require(plugin.processor->setupProcessing(setup)==kResultOk,
            "actual plugin setupProcessing failed");
        require(plugin.component->setActive(true)==kResultOk,
            "actual in-process native core activation failed");
        bool active=true,processing=false;
        try {
            require(plugin.processor->setProcessing(true)==kResultOk,
                "actual plugin setProcessing failed");
            processing=true;
            std::array<float,block> left{},right{};
            float* channels[2]={left.data(),right.data()};
            AudioBusBuffers output{};
            output.numChannels=2;
            output.channelBuffers32=channels;
            EventList events;
            ProcessData data{};
            data.processMode=kRealtime;
            data.symbolicSampleSize=kSample32;
            data.numSamples=block;
            data.numInputs=0;
            data.numOutputs=1;
            data.outputs=&output;
            data.inputEvents=&events;
            for(unsigned iteration=0;iteration<300;++iteration) {
                events.clear();
                if(iteration%20==0) {
                    Event e{};
                    e.type=Event::kNoteOnEvent;
                    e.noteOn.channel=0;
                    e.noteOn.pitch=60;
                    e.noteOn.velocity=.8f;
                    require(events.addEvent(e)==kResultOk,"host NoteOn addEvent");
                    ++midiEvents;
                } else if(iteration%20==10) {
                    Event e{};
                    e.type=Event::kNoteOffEvent;
                    e.noteOff.channel=0;
                    e.noteOff.pitch=60;
                    require(events.addEvent(e)==kResultOk,"host NoteOff addEvent");
                    ++midiEvents;
                }
                left.fill(-123.f);
                right.fill(123.f);
                require(plugin.processor->process(data)==kResultOk,
                    "real VST3 processing failed");
                ++calls;
                for(unsigned i=0;i<block;++i) {
                    require(std::isfinite(left[i])&&std::isfinite(right[i]),
                        "nonfinite VST3 public-host output");
                    // No original licensed guest was booted. The real native
                    // plugin must output silence, never stale buffer contents.
                    require(left[i]==0.f&&right[i]==0.f,
                        "nonzero audio without original Creative guest");
                }
            }
        }catch(...) {
            if(processing)(void)plugin.processor->setProcessing(false);
            if(active)(void)plugin.component->setActive(false);
            throw;
        }
        require(plugin.processor->setProcessing(false)==kResultOk,
            "VST3 stop processing failed");
        require(plugin.component->setActive(false)==kResultOk,
            "VST3 deactivate/cleanup failed");
    }
    require(calls==900&&midiEvents==90,"incomplete public-host callback coverage");
    std::printf("P007W_REAL_WINDOWS_VST3_44K_48K_96K_MIDI_AUDIO_LIFECYCLE_PASS"
                " callbacks=%u midi_events=%u guest=ABSENT_OUTPUT_SILENCE_ONLY\\n",
                calls,midiEvents);
}


void syntheticGuestAudio(const VST3::Hosting::PluginFactory& factory,
                         const VST3::UID& id,FUnknown* host,
                         const fs::path& bundle) {
    // This is intentionally a DIFFERENT acceptance from original Creative.
    // Only a disposable, clearly labeled synthetic bundle may run it.
    const auto root=bundle/"Contents"/"Resources"/"PrivateVM";
    const auto disk=root/"vm"/"win95.img";
    require(fs::is_regular_file(root/"P007_SYNTHETIC_TEST_ONLY.marker"),
            "synthetic-only marker missing: refuse to use owner's real Win95");
    require(fs::is_regular_file(disk)&&fs::file_size(disk)<1024*1024,
            "real/private disk may not run in synthetic mode");
    char profile[4096]{};
    const DWORD len=GetEnvironmentVariableA("LOCALAPPDATA",profile,4096);
    require(len>0&&len<4096&&std::strstr(profile,"p007-synthetic-profile"),
            "synthetic guest requires a disposable LOCALAPPDATA");
    using Counter=unsigned (__cdecl*)();
    unsigned passedRates=0;
    for (double hz : {44100.,48000.,96000.}) {
        auto plugin=instantiate(factory,id,host);
        require(plugin.controller->setParamNormalized(1108u,.60)==kResultOk,
                "original Creative Filter Cutoff NRPN parameter");
        require(plugin.controller->setParamNormalized(1100u,.40)==kResultOk,
                "original Creative AMP Attack NRPN parameter");
        require(plugin.controller->setParamNormalized(bankId,3./127.)==kResultOk,
                "synthetic bank automation");
        require(plugin.controller->setParamNormalized(programId,40./127.)==kResultOk,
                "synthetic program automation");
        require(plugin.controller->setParamNormalized(masterId,.5)==kResultOk,
                "synthetic master automation");
        ProcessSetup setup{};
        setup.processMode=kRealtime;
        setup.symbolicSampleSize=kSample32;
        setup.maxSamplesPerBlock=block;
        setup.sampleRate=hz;
        require(plugin.processor->setupProcessing(setup)==kResultOk,
                "synthetic VST3 setupProcessing");
        require(plugin.component->setActive(true)==kResultOk,
                "synthetic VST3 worker activation");
        bool active=true,processing=false;
        try {
            require(plugin.processor->setProcessing(true)==kResultOk,
                    "synthetic VST3 start processing");
            processing=true;
            Counter noteOnCounter=nullptr,noteOffCounter=nullptr,programCounter=nullptr,
                    bankCounter=nullptr;
            HMODULE core=GetModuleHandleW(L"125A_AWE_Core.dll");
            require(core!=nullptr,"synthetic QA guest module not loaded");
            const auto getCounter=[&](const char* name) {
                const auto ptr=GetProcAddress(core,name);
                require(ptr!=nullptr,"synthetic core missing QA counter export");
                return reinterpret_cast<Counter>(ptr);
            };
            noteOnCounter=getCounter("p007_mock_noteon_count");
            noteOffCounter=getCounter("p007_mock_noteoff_count");
            programCounter=getCounter("p007_mock_program_plus_one");
            bankCounter=getCounter("p007_mock_bank_plus_one");
            Counter rejectsCounter=getCounter("p007_mock_rejected_control_packets");
            Counter filterCount=getCounter("p007_mock_nrpn_filter_count");
            Counter filterRaw=getCounter("p007_mock_nrpn_filter_raw");
            Counter attackCount=getCounter("p007_mock_nrpn_attack_count");
            Counter nrpnRejects=getCounter("p007_mock_nrpn_rejected_selectors");
            Counter nrpnReset=getCounter("p007_mock_nrpn_reset121");
            Counter reverbDry=getCounter("p007_mock_nrpn_dry_reverb");
            Counter chorusDry=getCounter("p007_mock_nrpn_dry_chorus");
            std::array<float,block> left{},right{};
            float* channels[2]={left.data(),right.data()};
            AudioBusBuffers output{};
            output.numChannels=2;
            output.channelBuffers32=channels;
            EventList events;
            ProcessData data{};
            data.processMode=kRealtime;
            data.symbolicSampleSize=kSample32;
            data.numSamples=block;
            data.numInputs=0;
            data.numOutputs=1;
            data.outputs=&output;
            data.inputEvents=&events;
            unsigned audibleHalf=0,audibleFull=0,muted=0;
            auto next=std::chrono::steady_clock::now();
            for(unsigned iteration=0;iteration<100;++iteration) {
                events.clear();
                if(iteration==5 || iteration==40) {
                    Event e{};
                    e.type=iteration==5?Event::kNoteOnEvent:Event::kNoteOffEvent;
                    e.busIndex=0;
                    e.sampleOffset=0;
                    if(iteration==5) {
                        e.noteOn.channel=0;
                        e.noteOn.pitch=60;
                        e.noteOn.velocity=.9f;
                    } else {
                        e.noteOff.channel=0;
                        e.noteOff.pitch=60;
                        e.noteOff.velocity=0.f;
                    }
                    require(events.addEvent(e)==kResultOk,
                            "synthetic host MIDI event submission");
                }
                if(iteration==55) {
                    require(plugin.controller->setParamNormalized(masterId,0.)==kResultOk,
                            "set true zero master");
                }
                if(iteration==70) {
                    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
                            "restore true unity master");
                }
                if(iteration==72) {
                    // Original Creative doc: filter 0% is SoundFont-neutral.
                    // CC121 must restore the patch instead of setting the
                    // ORIGINAL cutoff to ~100 Hz via NRPN=0.
                    require(plugin.controller->setParamNormalized(1108u,0.)==kResultOk,
                            "return original Creative cutoff to SoundFont-neutral 0%");
                }
                if(iteration==80) {
                    require(plugin.processor->setProcessing(false)==kResultOk,
                            "transport stop processing");
                    require(plugin.processor->setProcessing(true)==kResultOk,
                            "transport resume processing");
                }
                left.fill(-333.f);
                right.fill(333.f);
                require(plugin.processor->process(data)==kResultOk,
                        "actual VST3 synthetic guest audio callback");
                for(unsigned i=0;i<block;++i) {
                    const float l=left[i],r=right[i];
                    require(std::isfinite(l)&&std::isfinite(r),
                            "nonfinite VST3 output");
                    if(iteration>=10 && iteration<55 &&
                       std::fabs(l-.125f)<.00001f&&std::fabs(r+.0625f)<.00001f)
                        ++audibleHalf;
                    if(iteration>=56&&iteration<70) {
                        require(l==0.f&&r==0.f,
                            "master 0% did not produce exact digital silence");
                        ++muted;
                    }
                    if(iteration>=74&&iteration<80 &&
                       std::fabs(l-.25f)<.00001f&&std::fabs(r+.125f)<.00001f)
                        ++audibleFull;
                }
                next+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(double(block)/hz));
                std::this_thread::sleep_until(next);
            }
            require(noteOnCounter()>=1u && noteOffCounter()>=1u,
                    "host MIDI did not reach synthetic in-process guest core");
            require(programCounter()==41u && bankCounter()==4u,
                    "actual P165 Bank/Program never arrived at synthetic guest");
            require(rejectsCounter()==2u,
                    "synthetic bank/program retry fault injection was not exercised");
            require(nrpnRejects()==1u && filterCount()>=1u &&
                    filterRaw()==76u && attackCount()>=1u,
                    "real VST3 failed 12-control Creative NRPN UART retry and delivery");
            require(nrpnReset()>=1u && reverbDry()>=2u && chorusDry()>=2u,
                    "P007AB 0%-neutral SoundFont reset / FX digital dry MIDI not delivered");
            require(filterRaw()==76u,
                    "P007AB 0% cutoff sent destructive raw NRPN 0 instead of CC121 reset");
            require(audibleHalf>=block*5u,
                    "actual VST3 did not produce synthetic 50-percent stereo PCM");
            require(muted==block*14u,
                    "actual VST3 did not honor exact-zero Master");
            require(audibleFull>=block*3u,
                    "actual VST3 did not recover original-level synthetic PCM");
            ++passedRates;
            std::printf("P007X_SYNTHETIC_VST3_HOST_AUDIO_MIDI_PASS"
                        " rate=%.0f half=%u muted=%u full=%u"
                        " guest_original=NO\\n",
                        hz,audibleHalf,muted,audibleFull);
        } catch(...) {
            if(processing)(void)plugin.processor->setProcessing(false);
            if(active)(void)plugin.component->setActive(false);
            throw;
        }
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
    }
    require(passedRates==3,"three synthetic host rates not covered");
    std::puts("P007X_REAL_WINDOWS_VST3_SYNTHETIC_GUEST_44K_48K_96K_AUDIO_MIDI_PASS");
}

// The original 512-MiB Win95 template and the owner's persistent Studio Pro
// guest must NEVER be altered by a research test run. Override LOCALAPPDATA
// for THIS isolated QA process before the real plugin is activated. The VST3
// then uses its unchanged private guest-copy implementation under TEMP.
class OwnerGuestSandbox final {
    std::wstring old_;
    bool had_=false;
    fs::path root_;
public:
    OwnerGuestSandbox() {
        const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
        if(n>0) {
            std::wstring buffer(n,L'\0');
            const DWORD got=GetEnvironmentVariableW(L"LOCALAPPDATA",buffer.data(),n);
            require(got>0&&got<n,"read old LOCALAPPDATA");
            buffer.resize(got);
            old_=std::move(buffer);
            had_=true;
        }
        root_=fs::temp_directory_path()/
              (L"P007-OwnerPrivateQA-"+std::to_wstring(GetCurrentProcessId()));
        require(!fs::exists(root_),
                "isolated private QA path already exists: no overwrite");
        require(fs::create_directories(root_),
                "create isolated private QA profile");
        require(SetEnvironmentVariableW(L"LOCALAPPDATA",root_.c_str())!=0,
                "set isolated private QA profile");
    }
    OwnerGuestSandbox(const OwnerGuestSandbox&)=delete;
    OwnerGuestSandbox& operator=(const OwnerGuestSandbox&)=delete;
    ~OwnerGuestSandbox() {
        if(had_) (void)SetEnvironmentVariableW(L"LOCALAPPDATA",old_.c_str());
        else (void)SetEnvironmentVariableW(L"LOCALAPPDATA",nullptr);
        std::error_code ignored;
        fs::remove_all(root_,ignored);
    }
};

void privateMidiAudio(const VST3::Hosting::PluginFactory& factory,
                      const VST3::UID& id,FUnknown* host,
                      const fs::path& bundle) {
    const auto privateDisk=bundle/"Contents"/"Resources"/"PrivateVM"/"vm"/"win95.img";
    require(fs::is_regular_file(privateDisk),
            "private Win95 HDD absent; refusing synthetic Creative audio PASS");
    require(!fs::exists(bundle/"Contents"/"Resources"/"PrivateVM"/
                        "P007_SYNTHETIC_TEST_ONLY.marker"),
            "synthetic guest rejected in ORIGINAL Creative acceptance");
    require(fs::file_size(privateDisk) >= 32ull*1024ull*1024ull,
            "private guest HDD unexpectedly small; reject synthetic fixture");
    const auto privateResources=bundle/"Contents"/"Resources"/"PrivateVM";
    require(fs::is_directory(privateResources/"roms") &&
            fs::is_directory(privateResources/"assets") &&
            fs::is_regular_file(privateResources/"vm"/"P007_AWE64.cfg"),
            "private original Creative ROM/assets/guest profile incomplete");
    OwnerGuestSandbox sandbox;
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,"bank 0");
    require(plugin.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "program 40");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "master unity");

    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;
    setup.sampleRate=44100.0;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private sample setup");
    require(plugin.component->setActive(true)==kResultOk,"private VST3 activation");
    bool processing=false,active=true;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private setProcessing");
        processing=true;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers output{};
        output.numChannels=2;
        output.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=block;
        data.numInputs=0;
        data.numOutputs=1;
        data.outputs=&output;
        data.inputEvents=&events;

        using ReadyFn=int (__cdecl*)();
        using CoreSymbolFn=std::uintptr_t (__cdecl*)();
        ReadyFn ready=nullptr;
        // A CI synthetic DLL is not the ORIGINAL 86Box core. Demand exact
        // upstream CPU entrypoint exports before any originality claim.
        const HMODULE guestDll=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(guestDll!=nullptr,"actual private 86Box core not loaded");
        const auto corePCInit=reinterpret_cast<CoreSymbolFn>(
            GetProcAddress(guestDll,"p007_core_symbol_pc_init"));
        const auto corePCRun=reinterpret_cast<CoreSymbolFn>(
            GetProcAddress(guestDll,"p007_core_symbol_pc_run"));
        require(corePCInit && corePCRun &&
                corePCInit()!=0u && corePCRun()!=0u,
                "original 86Box CPU symbol exports absent: no authenticity proof");
        ready=reinterpret_cast<ReadyFn>(
            GetProcAddress(guestDll,"p007_midi_guest_ready"));
        require(ready!=nullptr,"original guest WinMM READY export absent");

        using Clock=std::chrono::steady_clock;
        Clock::time_point readyAt{},noteAt{};
        p007::original_audio_proof::Window before,after;
        bool sentOn=false,sentOff=false;
        const auto start=Clock::now();
        auto next=start;
        auto lastReport=start;
        while(Clock::now()-start < 110s) {
            events.clear();
            const auto now=Clock::now();
            if(readyAt==Clock::time_point{} && ready()) {
                readyAt=now;
                std::puts("P007Y_ORIGINAL_CREATIVE_WINMM_READY_BASELINE_START");
            }
            // Baseline is measured for 1.5 seconds AFTER genuine Creative
            // WinMM READY, before any host NoteOn was submitted.
            if(!sentOn && readyAt!=Clock::time_point{} &&
               now-readyAt>=1500ms && before.frames>=44100u) {
                Event e{};
                e.type=Event::kNoteOnEvent;
                e.busIndex=0;
                e.sampleOffset=0;
                e.noteOn.channel=0;
                e.noteOn.pitch=60;
                e.noteOn.velocity=.86f;
                require(events.addEvent(e)==kResultOk,"host MIDI NoteOn");
                sentOn=true;
                noteAt=now;
                std::printf("P007Y_PRIVATE_ORIGINAL_NOTEON_SENT"
                            " baseline_frames=%llu baseline_rms=%.8f\n",
                            static_cast<unsigned long long>(before.frames),
                            before.rms());
            }else if(sentOn && !sentOff && now-noteAt>=4s){
                Event e{};
                e.type=Event::kNoteOffEvent;
                e.busIndex=0;
                e.sampleOffset=0;
                e.noteOff.channel=0;
                e.noteOff.pitch=60;
                e.noteOff.velocity=0.f;
                require(events.addEvent(e)==kResultOk,"host MIDI NoteOff");
                sentOff=true;
            }
            require(plugin.processor->process(data)==kResultOk,"real VST3 process");
            for(int i=0;i<block;++i) {
                const auto l=left[static_cast<std::size_t>(i)];
                const auto r=right[static_cast<std::size_t>(i)];
                require(std::isfinite(l)&&std::isfinite(r),
                        "NaN/Inf in original EMU8000 PCM");
                if(readyAt!=Clock::time_point{} && !sentOn)
                    before.append(l,r);
                if(sentOn && now-noteAt>=250ms && now-noteAt<3s)
                    after.append(l,r);
            }
            if(sentOff && now-noteAt>=5s)break;
            if(now-lastReport >= 10s){
                std::printf("P007Y_PRIVATE_HOST_PROGRESS"
                            " ready=%d baseline_frames=%llu"
                            " note=%d noteoff=%d post_frames=%llu"
                            " post_peak=%.8f\n",
                    ready(),
                    static_cast<unsigned long long>(before.frames),
                    sentOn?1:0,sentOff?1:0,
                    static_cast<unsigned long long>(after.frames),after.peak);
                std::fflush(stdout);
                lastReport=now;
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(sentOn&&sentOff,
                "original Win95/Creative WinMM never completed NoteOn and NoteOff");
        require(p007::original_audio_proof::accept(before,after),
                "original MIDI-dependent PCM NOT proven; possible Win95 "
                "background sound, synthetic constant or guest silence");
        std::printf("P007Y_PRIVATE_REAL_VST3_ORIGINAL_CREATIVE_NOTEON_PCM_PASS"
                    " before_rms=%.9f after_rms=%.9f"
                    " before_frames=%llu after_frames=%llu"
                    " post_peak=%.9f post_nonzero=%llu\n",
                    before.rms(),after.rms(),
                    static_cast<unsigned long long>(before.frames),
                    static_cast<unsigned long long>(after.frames),
                    after.peak,
                    static_cast<unsigned long long>(after.nonzero));
    }catch(...){
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}
} // namespace

int main(int argc,char** argv) {
    try{
        require(argc==2 || (argc==3 && (std::strcmp(argv[2],"--private-audio")==0 || std::strcmp(argv[2],"--synthetic")==0)),
                "usage: P007ActualHostQA.exe <AWE64_Gold_P007.vst3 folder> [--private-audio]");
        const fs::path bundle=fs::absolute(argv[1]);
        require(fs::is_directory(bundle),"actual VST3 bundle not found");
        std::string error;
        auto module=VST3::Hosting::Module::create(bundle.string(),error);
        if(!module)std::fprintf(stderr,"P007W_MODULE_ERROR %s\n",error.c_str());
        require(bool(module),"Steinberg Module cannot load actual Windows VST3");
        auto const& factory=module->getFactory();
        auto classes=factory.classInfos();
        const auto choice=std::find_if(classes.begin(),classes.end(),
            [](auto const& c){return c.category()==kVstAudioEffectClass;});
        require(choice!=classes.end(),"no instrument IComponent in real VST3");
        auto host=owned(new HostApplication());
        stateRoundtrip(factory,choice->ID(),host.get());
        if(argc==2) publicWindowsAudioLifecycle(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--synthetic")==0)
            syntheticGuestAudio(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--private-audio")==0)
            privateMidiAudio(factory,choice->ID(),host.get(),bundle);
        std::puts("P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS");
        return 0;
    }catch(const std::exception& ex){
        std::fprintf(stderr,"P007W_ACTUAL_WINDOWS_VST3_HOST_QA_FAIL %s\n",ex.what());
        return 1;
    }
}
