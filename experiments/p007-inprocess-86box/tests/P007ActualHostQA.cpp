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
#include "P007OriginalControlDifferential.hpp"
#include "P007TenControlMatrix.hpp"
#include "P007OriginalEnvFilterDepth.hpp"
#include "P007OriginalSpectralProof.hpp"
#include "P007ALCorrectedSpectralProof.hpp"
#include "P007ALOriginalChipRegisterProof.hpp"
#include "../include/P007RegisterProbe.h"
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
#include <memory>
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

// Private, authenticated Creative-original PCM A/B/A probe. Unlike synthetic
// UART coverage, THIS only passes if real Win95-driven EMU8000 PCM changes
// with cutoff and amp NRPN and returns to original SoundFont-neutral sound.
// Six repeated notes, one ORIGINAL guest boot. Proprietary images stay local.
void privateOriginalNrpnAudio(const VST3::Hosting::PluginFactory& factory,
                              const VST3::UID& id,FUnknown* host,
                              const fs::path& bundle) {
    const auto root=bundle/"Contents"/"Resources"/"PrivateVM";
    const auto disk=root/"vm"/"win95.img";
    require(fs::is_regular_file(disk)&&fs::file_size(disk)>=32ull*1024ull*1024ull,
            "P007AD requires authentic owner-private Win95 HDD");
    require(!fs::exists(root/"P007_SYNTHETIC_TEST_ONLY.marker")&&
            fs::is_directory(root/"roms")&&fs::is_directory(root/"assets")&&
            fs::is_regular_file(root/"vm"/"P007_AWE64.cfg"),
            "P007AD rejects synthetic or incomplete licensed original guest");
    OwnerGuestSandbox profile;
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,
            "private original ROM bank 0");
    require(plugin.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "private original GM program 40");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "original output unity");
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;setup.sampleRate=44100.;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private original native 44.1k");
    require(plugin.component->setActive(true)==kResultOk,
            "private original 86Box/Creative activate");
    bool active=true,processing=false;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private original guest process start");
        processing=true;
        const HMODULE dll=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(dll!=nullptr,"authentic original core missing");
        using Symbol=std::uintptr_t (__cdecl*)();
        using Ready=int (__cdecl*)();
        const auto originalInit=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_init"));
        const auto originalRun=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_run"));
        const auto ready=reinterpret_cast<Ready>(
            GetProcAddress(dll,"p007_midi_guest_ready"));
        require(originalInit&&originalRun&&ready&&
                originalInit()!=0u&&originalRun()!=0u,
                "original 86Box CPU symbols required, synthetic core forbidden");
        // CC7 positive control separates generic Win95 WinMM controller
        // delivery from NRPN-specific driver support. If CC7=0 cannot
        // mute the ORIGINAL sound, do not blame the NRPN mapping yet.
        // Unlike VST3 GUI parameter events, this direct test-only send
        // still uses the exact same p007_midi_submit UART packet FIFO,
        // and is invoked on the same host/audio driver thread.
        using Midi= int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
        const auto sendMidi=reinterpret_cast<Midi>(
                         GetProcAddress(dll,"p007_midi_submit"));
        require(sendMidi!=nullptr,
                "authentic 86Box UART MIDI producer export unavailable");
        struct Scenario {
            const char* name;
            ParamID param;
            double value;
            int ccNumber=-1;
            int ccValue=0;
        };
        constexpr std::array<Scenario,8> scenarios{{
          {"A1_original_dry",0u,0.},
          {"A2_original_repeat",0u,0.},
          {"POSITIVE_B_CC7_mute_original",0u,0.,7,0},
          {"POSITIVE_A_CC7_restore100",0u,0.,7,100},
          {"B_filter_cutoff_10percent",1108u,.10},
          {"A3_filter_neutral_0percent",1108u,0.},
          {"B_amp_attack_65percent",1100u,.65},
          {"A4_amp_neutral_0percent",1100u,0.}
        }};
        namespace proof=p007::original_control_proof;
        std::array<proof::Take,scenarios.size()> takes{};
        p007::original_audio_proof::Window before,originalFirst;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=block;data.numOutputs=1;data.outputs=&out;
        data.inputEvents=&events;
        using Clock=std::chrono::steady_clock;
        enum class Phase {Boot,Prepare,Note,Tail};
        Phase phase=Phase::Boot;
        std::size_t index=0;
        Clock::time_point readyAt{},phaseAt{},noteAt{},offAt{};
        const auto start=Clock::now();
        auto next=start,lastReport=start;
        while(Clock::now()-start<180s&&index<scenarios.size()) {
            events.clear();
            const auto now=Clock::now();
            if(phase==Phase::Boot) {
                if(readyAt==Clock::time_point{}&&ready()) {
                    readyAt=now;std::puts("P007AD_ORIGINAL_WIN95_WINMM_READY");
                }
                if(readyAt!=Clock::time_point{}&&
                   now-readyAt>=1500ms&&before.frames>=44100u) {
                    phase=Phase::Prepare;phaseAt=now;
                }
            }else if(phase==Phase::Prepare && now-phaseAt>=750ms) {
                require(ready()!=0,"original Win95 Creative READY lost during sweep");
                Event e{};e.type=Event::kNoteOnEvent;e.busIndex=0;
                e.noteOn.channel=0;e.noteOn.pitch=60;e.noteOn.velocity=.86f;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOn event not queued");
                noteAt=now;phase=Phase::Note;
            }else if(phase==Phase::Note&&now-noteAt>=1850ms) {
                Event e{};e.type=Event::kNoteOffEvent;e.busIndex=0;
                e.noteOff.channel=0;e.noteOff.pitch=60;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOff event not queued");
                offAt=now;phase=Phase::Tail;
            }else if(phase==Phase::Tail&&now-offAt>=2000ms) {
                const auto& note=takes[index];
                std::printf("P007AD_PRIVATE_ORIGINAL_NOTE name=%s"
                            " onset_rms=%.9f roughness=%.9f"
                            " onset_frames=%llu sustain_rms=%.9f"
                            " tail_rms=%.9f\n",scenarios[index].name,
                            note.onset.rms(),note.onset.roughness(),
                            static_cast<unsigned long long>(note.onset.stereoFrames),
                            note.sustain.rms(),note.tail.rms());
                ++index;
                if(index<scenarios.size()) {
                    const auto& chosen=scenarios[index];
                    if(chosen.param!=0u)
                        require(plugin.controller->setParamNormalized(
                            chosen.param,chosen.value)==kResultOk,
                            "original Creative NRPN host automation failed");
                    if(chosen.ccNumber>=0)
                        require(sendMidi(0xb0u,
                             static_cast<std::uint8_t>(chosen.ccNumber),
                             static_cast<std::uint8_t>(chosen.ccValue))==1,
                            "positive-control MIDI CC7 was not queued");
                    phase=Phase::Prepare;phaseAt=now;
                }
            }
            require(plugin.processor->process(data)==kResultOk,
                    "authentic original VST3 audio process failed");
            for(unsigned i=0;i<block;++i) {
                const auto l=left[i],r=right[i];
                require(std::isfinite(l)&&std::isfinite(r),
                        "nonfinite original EMU8000 PCM");
                if(index>=scenarios.size())continue;
                if(phase==Phase::Boot&&readyAt!=Clock::time_point{})
                    before.append(l,r);
                if(phase==Phase::Note) {
                    const auto delta=now-noteAt;
                    if(delta>=180ms && delta<1150ms) {
                        takes[index].onset.append(l,r);
                        if(index==0u)originalFirst.append(l,r);
                    }
                    if(delta>=1150ms && delta<1750ms)
                        takes[index].sustain.append(l,r);
                }else if(phase==Phase::Tail) {
                    const auto delta=now-offAt;
                    if(delta>=250ms&&delta<1750ms)
                        takes[index].tail.append(l,r);
                }
            }
            if(now-lastReport>=15s) {
                std::printf("P007AD_PRIVATE_PROGRESS original_ready=%d take=%zu/%zu\n",
                            ready(),index,scenarios.size());
                std::fflush(stdout);lastReport=now;
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(index==scenarios.size(),
                "original Windows-95 guest did not complete six A/B/A notes");
        require(p007::original_audio_proof::accept(before,originalFirst),
                "original Creative PCM not proven above Win95 background");
        const bool stable=proof::stable(takes[0],takes[1]);
        const bool cc7Mute=takes[2].onset.rms()<takes[0].onset.rms()*.28;
        const bool cc7Restore=proof::stable(takes[0],takes[3]);
        const bool cutoff=proof::cutoffEffect(takes[3],takes[4],takes[5]);
        const bool attack=proof::attackEffect(takes[3],takes[6],takes[7]);
        std::printf("P007AE_PRIVATE_ORIGINAL_PCM_DIAGNOSTIC"
                    " stable_original=%d cc7_mute=%d cc7_restored=%d"
                    " filter_nrpn=%d amp_attack_nrpn=%d"
                    " original_win95_creative=1 synthetic=0\n",
                    int(stable),int(cc7Mute),int(cc7Restore),
                    int(cutoff),int(attack));
        std::fflush(stdout);
        require(stable&&cc7Mute&&cc7Restore&&cutoff&&attack,
                "P007AE original Win95 control differential NOT proven."
                " Read cc7_mute and cc7_restored FIRST: if false, original"
                " generic MIDI CC also fails. If true but NRPN=0, focus"
                " on original Creative driver NRPN rather than host MIDI.");
        std::puts("P007AD_PRIVATE_REAL_WIN95_CREATIVE_NRPN_ABA_PCM_PASS");
    }catch(...) {
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}

void privateTenControlMatrix(const VST3::Hosting::PluginFactory& factory,
                              const VST3::UID& id,FUnknown* host,
                              const fs::path& bundle) {
    const auto root=bundle/"Contents"/"Resources"/"PrivateVM";
    const auto disk=root/"vm"/"win95.img";
    require(fs::is_regular_file(disk)&&fs::file_size(disk)>=32ull*1024ull*1024ull,
            "P007AG requires real owner-private Win95 HDD");
    require(!fs::exists(root/"P007_SYNTHETIC_TEST_ONLY.marker")&&
            fs::is_directory(root/"roms")&&fs::is_directory(root/"assets")&&
            fs::is_regular_file(root/"vm"/"P007_AWE64.cfg"),
            "P007AD rejects synthetic or incomplete licensed original guest");
    OwnerGuestSandbox profile;
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,
            "private original ROM bank 0");
    require(plugin.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "private original GM program 40");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "original output unity");
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;setup.sampleRate=44100.;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private original native 44.1k");
    require(plugin.component->setActive(true)==kResultOk,
            "private original 86Box/Creative activate");
    bool active=true,processing=false;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private original guest process start");
        processing=true;
        const HMODULE dll=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(dll!=nullptr,"authentic original core missing");
        using Symbol=std::uintptr_t (__cdecl*)();
        using Ready=int (__cdecl*)();
        const auto originalInit=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_init"));
        const auto originalRun=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_run"));
        const auto ready=reinterpret_cast<Ready>(
            GetProcAddress(dll,"p007_midi_guest_ready"));
        require(originalInit&&originalRun&&ready&&
                originalInit()!=0u&&originalRun()!=0u,
                "original 86Box CPU symbols required, synthetic core forbidden");
        // CC7 positive control separates generic Win95 WinMM controller
        // delivery from NRPN-specific driver support. If CC7=0 cannot
        // mute the ORIGINAL sound, do not blame the NRPN mapping yet.
        // Unlike VST3 GUI parameter events, this direct test-only send
        // still uses the exact same p007_midi_submit UART packet FIFO,
        // and is invoked on the same host/audio driver thread.
        using Midi= int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
        const auto sendMidi=reinterpret_cast<Midi>(
                         GetProcAddress(dll,"p007_midi_submit"));
        require(sendMidi!=nullptr,
                "authentic 86Box UART MIDI producer export unavailable");
        struct Scenario {
            const char* name;
            ParamID param;
            double value;
            int ccNumber=-1;
            int ccValue=0;
        };
        // Real original Win95 driver, unchanged original MIDI ROM Program40.
        // A1/A2 validate the baseline. Then EACH of ten remaining P165
        // controllers gets its own B modified note and A restored note.
        // Control 1100 (AMP Attack) and 1108 (Cutoff) have ALREADY passed
        // the owner's P007AE acoustic test; they are not retested here.
        std::array<Scenario,22> scenarios{};
        scenarios[0]={"A1_original_dry",0u,0.};
        scenarios[1]={"A2_original_repeat",0u,0.};
        for(unsigned k=0;k<p007::ten_matrix::knobs.size();++k) {
            const auto& knob=p007::ten_matrix::knobs[k];
            scenarios[2u+2u*k]={knob.title,knob.vstId,knob.normalized};
            scenarios[3u+2u*k]={"RESTORE_ORIGINAL_SOUNDFONT",
                                knob.vstId,0.};
        }
        namespace proof=p007::original_control_proof;
        std::array<proof::Take,22> takes{};
        p007::original_audio_proof::Window before,originalFirst;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=block;data.numOutputs=1;data.outputs=&out;
        data.inputEvents=&events;
        using Clock=std::chrono::steady_clock;
        enum class Phase {Boot,Prepare,Note,Tail};
        Phase phase=Phase::Boot;
        std::size_t index=0;
        Clock::time_point readyAt{},phaseAt{},noteAt{},offAt{};
        const auto start=Clock::now();
        auto next=start,lastReport=start;
        while(Clock::now()-start<260s&&index<scenarios.size()) {
            events.clear();
            const auto now=Clock::now();
            if(phase==Phase::Boot) {
                if(readyAt==Clock::time_point{}&&ready()) {
                    readyAt=now;std::puts("P007AG_ORIGINAL_WIN95_WINMM_READY");
                }
                if(readyAt!=Clock::time_point{}&&
                   now-readyAt>=1500ms&&before.frames>=44100u) {
                    phase=Phase::Prepare;phaseAt=now;
                }
            }else if(phase==Phase::Prepare && now-phaseAt>=750ms) {
                require(ready()!=0,"original Win95 Creative READY lost during sweep");
                Event e{};e.type=Event::kNoteOnEvent;e.busIndex=0;
                e.noteOn.channel=0;e.noteOn.pitch=60;e.noteOn.velocity=.86f;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOn event not queued");
                noteAt=now;phase=Phase::Note;
            }else if(phase==Phase::Note&&now-noteAt>=1850ms) {
                Event e{};e.type=Event::kNoteOffEvent;e.busIndex=0;
                e.noteOff.channel=0;e.noteOff.pitch=60;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOff event not queued");
                offAt=now;phase=Phase::Tail;
            }else if(phase==Phase::Tail&&now-offAt>=2000ms) {
                const auto& note=takes[index];
                std::printf("P007AG_PRIVATE_ORIGINAL_MATRIX_NOTE name=%s"
                            " onset_rms=%.9f roughness=%.9f"
                            " onset_frames=%llu sustain_rms=%.9f"
                            " tail_rms=%.9f\n",scenarios[index].name,
                            note.onset.rms(),note.onset.roughness(),
                            static_cast<unsigned long long>(note.onset.stereoFrames),
                            note.sustain.rms(),note.tail.rms());
                ++index;
                if(index<scenarios.size()) {
                    const auto& chosen=scenarios[index];
                    if(chosen.param!=0u)
                        require(plugin.controller->setParamNormalized(
                            chosen.param,chosen.value)==kResultOk,
                            "original Creative NRPN host automation failed");
                    if(chosen.ccNumber>=0)
                        require(sendMidi(0xb0u,
                             static_cast<std::uint8_t>(chosen.ccNumber),
                             static_cast<std::uint8_t>(chosen.ccValue))==1,
                            "positive-control MIDI CC7 was not queued");
                    phase=Phase::Prepare;phaseAt=now;
                }
            }
            require(plugin.processor->process(data)==kResultOk,
                    "authentic original VST3 audio process failed");
            for(unsigned i=0;i<block;++i) {
                const auto l=left[i],r=right[i];
                require(std::isfinite(l)&&std::isfinite(r),
                        "nonfinite original EMU8000 PCM");
                if(index>=scenarios.size())continue;
                if(phase==Phase::Boot&&readyAt!=Clock::time_point{})
                    before.append(l,r);
                if(phase==Phase::Note) {
                    const auto delta=now-noteAt;
                    if(delta>=180ms && delta<1150ms) {
                        takes[index].onset.append(l,r);
                        if(index==0u)originalFirst.append(l,r);
                    }
                    if(delta>=1150ms && delta<1750ms)
                        takes[index].sustain.append(l,r);
                }else if(phase==Phase::Tail) {
                    const auto delta=now-offAt;
                    if(delta>=250ms&&delta<1750ms)
                        takes[index].tail.append(l,r);
                }
            }
            if(now-lastReport>=15s) {
                std::printf("P007AG_PRIVATE_PROGRESS original_ready=%d take=%zu/%zu\n",
                            ready(),index,scenarios.size());
                std::fflush(stdout);lastReport=now;
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(index==scenarios.size(),
                "original Windows-95 guest did not complete all twenty-two A/B/A matrix notes");
        require(p007::original_audio_proof::accept(before,originalFirst),
                "original Creative PCM not proven above Win95 background");
        using namespace p007::ten_matrix;
        require(proof::stable(takes[0],takes[1]),
                "P007AG independent original SoundFont A1/A2 baseline is unstable");
        unsigned measurable=0u,inconclusive=0u,unstable=0u;
        for(unsigned k=0;k<knobs.size();++k) {
            const unsigned a=1u+k*2u,b=a+1u,back=b+1u;
            const auto e=evaluate(takes[0],takes[a],takes[b],
                                  takes[back],knobs[k].type);
            if(e.verdict==Verdict::Proven)++measurable;
            else if(e.verdict==Verdict::Inconclusive)++inconclusive;
            else ++unstable;
            std::printf("P007AG_ORIGINAL_WIN95_CREATIVE_NRPN_RESULT"
                " control=%s vst3=%u verdict=%s"
                " A_onset_rms=%.9f B_onset_rms=%.9f"
                " A_return_rms=%.9f"
                " A_roughness=%.7f B_roughness=%.7f"
                " A_tail_rms=%.9f B_tail_rms=%.9f"
                " onset_effect=%d sustain_effect=%d tail_effect=%d"
                " reference_before_stable=%d reference_after_stable=%d"
                " original_win95=1 synthetic=0\n",
                knobs[k].title,knobs[k].vstId,verdictName(e.verdict),
                e.aOnset,e.bOnset,e.restoredOnset,
                e.aRoughness,e.bRoughness,e.aTail,e.bTail,
                int(e.onsetChanged),int(e.sustainChanged),int(e.tailChanged),
                int(e.stableBefore),int(e.stableAfter));
            std::fflush(stdout);
        }
        std::printf("P007AG_ORIGINAL_WIN95_CREATIVE_TEN_CONTROL_SUMMARY"
            " measured=%u inconclusive=%u unstable=%u total=%zu"
            " original_guest=1 synthetic=0\n",
            measurable,inconclusive,unstable,knobs.size());
        std::fflush(stdout);
        require(unstable==0u,
            "P007AG original A/B/A restoration failed for one or more controls."
            " Read individual original-PCM RESULT rows.");
        require(measurable==knobs.size(),
            "P007AG some Creative controls INCONCLUSIVE on original ROM"
            " Program40; inspect per-control PCM numbers."
            " Not measured != proven defective: try suitable SF2 preset.");
        std::puts("P007AG_PRIVATE_REAL_WIN95_ALL_TEN_CREATIVE_NRPN_PCM_PASS");
    }catch(...) {
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}

void privateFiveControlPadSweep(const VST3::Hosting::PluginFactory& factory,
                              const VST3::UID& id,FUnknown* host,
                              const fs::path& bundle) {
    const auto root=bundle/"Contents"/"Resources"/"PrivateVM";
    const auto disk=root/"vm"/"win95.img";
    require(fs::is_regular_file(disk)&&fs::file_size(disk)>=32ull*1024ull*1024ull,
            "P007AG requires real owner-private Win95 HDD");
    require(!fs::exists(root/"P007_SYNTHETIC_TEST_ONLY.marker")&&
            fs::is_directory(root/"roms")&&fs::is_directory(root/"assets")&&
            fs::is_regular_file(root/"vm"/"P007_AWE64.cfg"),
            "P007AD rejects synthetic or incomplete licensed original guest");
    OwnerGuestSandbox profile;
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,
            "private original ROM bank 0");
    require(plugin.controller->setParamNormalized(programId,95./127.)==kResultOk,
            "private original GM Pad 8 Sweep program 95");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "original output unity");
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;setup.sampleRate=44100.;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private original native 44.1k");
    require(plugin.component->setActive(true)==kResultOk,
            "private original 86Box/Creative activate");
    bool active=true,processing=false;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private original guest process start");
        processing=true;
        const HMODULE dll=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(dll!=nullptr,"authentic original core missing");
        using Symbol=std::uintptr_t (__cdecl*)();
        using Ready=int (__cdecl*)();
        const auto originalInit=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_init"));
        const auto originalRun=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_run"));
        const auto ready=reinterpret_cast<Ready>(
            GetProcAddress(dll,"p007_midi_guest_ready"));
        require(originalInit&&originalRun&&ready&&
                originalInit()!=0u&&originalRun()!=0u,
                "original 86Box CPU symbols required, synthetic core forbidden");
        // CC7 positive control separates generic Win95 WinMM controller
        // delivery from NRPN-specific driver support. If CC7=0 cannot
        // mute the ORIGINAL sound, do not blame the NRPN mapping yet.
        // Unlike VST3 GUI parameter events, this direct test-only send
        // still uses the exact same p007_midi_submit UART packet FIFO,
        // and is invoked on the same host/audio driver thread.
        using Midi= int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
        const auto sendMidi=reinterpret_cast<Midi>(
                         GetProcAddress(dll,"p007_midi_submit"));
        require(sendMidi!=nullptr,
                "authentic 86Box UART MIDI producer export unavailable");
        struct Scenario {
            const char* name;
            ParamID param;
            double value;
            int ccNumber=-1;
            int ccValue=0;
        };
        // P007AH: original Creative FAQ explicitly recommends Pad 8
        // (Sweep) as a filter-envelope-dependent GM sound. Its zero-based
        // MIDI Program Change is 95, unlike the P007AG orchestral preset 40.
        // AMP Decay and four Filter Env controls are the ONLY unresolved
        // cases from the user's 2026-11-18 authentic ten-control log.
        constexpr std::array<unsigned,5> selected{0u,3u,4u,5u,6u};
        std::array<Scenario,12> scenarios{};
        scenarios[0]={"PAD8_ORIGINAL_A1",0u,0.};
        scenarios[1]={"PAD8_ORIGINAL_A2",0u,0.};
        for(unsigned k=0;k<selected.size();++k) {
            const auto& knob=p007::ten_matrix::knobs[selected[k]];
            scenarios[2u+2u*k]={knob.title,knob.vstId,knob.normalized};
            scenarios[3u+2u*k]={"PAD8_RESTORE_PATCH",
                                knob.vstId,0.};
        }
        namespace proof=p007::original_control_proof;
        std::array<proof::Take,12> takes{};
        p007::original_audio_proof::Window before,originalFirst;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=block;data.numOutputs=1;data.outputs=&out;
        data.inputEvents=&events;
        using Clock=std::chrono::steady_clock;
        enum class Phase {Boot,Prepare,Note,Tail};
        Phase phase=Phase::Boot;
        std::size_t index=0;
        Clock::time_point readyAt{},phaseAt{},noteAt{},offAt{};
        const auto start=Clock::now();
        auto next=start,lastReport=start;
        while(Clock::now()-start<235s&&index<scenarios.size()) {
            events.clear();
            const auto now=Clock::now();
            if(phase==Phase::Boot) {
                if(readyAt==Clock::time_point{}&&ready()) {
                    readyAt=now;std::puts("P007AH_ORIGINAL_WIN95_WINMM_READY");
                }
                if(readyAt!=Clock::time_point{}&&
                   now-readyAt>=1500ms&&before.frames>=44100u) {
                    phase=Phase::Prepare;phaseAt=now;
                }
            }else if(phase==Phase::Prepare && now-phaseAt>=750ms) {
                require(ready()!=0,"original Win95 Creative READY lost during sweep");
                Event e{};e.type=Event::kNoteOnEvent;e.busIndex=0;
                e.noteOn.channel=0;e.noteOn.pitch=60;e.noteOn.velocity=.86f;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOn event not queued");
                noteAt=now;phase=Phase::Note;
            }else if(phase==Phase::Note&&now-noteAt>=3150ms) {
                Event e{};e.type=Event::kNoteOffEvent;e.busIndex=0;
                e.noteOff.channel=0;e.noteOff.pitch=60;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOff event not queued");
                offAt=now;phase=Phase::Tail;
            }else if(phase==Phase::Tail&&now-offAt>=2750ms) {
                const auto& note=takes[index];
                std::printf("P007AH_PRIVATE_ORIGINAL_MATRIX_NOTE name=%s"
                            " onset_rms=%.9f roughness=%.9f"
                            " onset_frames=%llu sustain_rms=%.9f"
                            " tail_rms=%.9f\n",scenarios[index].name,
                            note.onset.rms(),note.onset.roughness(),
                            static_cast<unsigned long long>(note.onset.stereoFrames),
                            note.sustain.rms(),note.tail.rms());
                ++index;
                if(index<scenarios.size()) {
                    const auto& chosen=scenarios[index];
                    if(chosen.param!=0u)
                        require(plugin.controller->setParamNormalized(
                            chosen.param,chosen.value)==kResultOk,
                            "original Creative NRPN host automation failed");
                    if(chosen.ccNumber>=0)
                        require(sendMidi(0xb0u,
                             static_cast<std::uint8_t>(chosen.ccNumber),
                             static_cast<std::uint8_t>(chosen.ccValue))==1,
                            "positive-control MIDI CC7 was not queued");
                    phase=Phase::Prepare;phaseAt=now;
                }
            }
            require(plugin.processor->process(data)==kResultOk,
                    "authentic original VST3 audio process failed");
            for(unsigned i=0;i<block;++i) {
                const auto l=left[i],r=right[i];
                require(std::isfinite(l)&&std::isfinite(r),
                        "nonfinite original EMU8000 PCM");
                if(index>=scenarios.size())continue;
                if(phase==Phase::Boot&&readyAt!=Clock::time_point{})
                    before.append(l,r);
                if(phase==Phase::Note) {
                    const auto delta=now-noteAt;
                    if(delta>=500ms && delta<2250ms) {
                        takes[index].onset.append(l,r);
                        if(index==0u)originalFirst.append(l,r);
                    }
                    if(delta>=2250ms && delta<3000ms)
                        takes[index].sustain.append(l,r);
                }else if(phase==Phase::Tail) {
                    const auto delta=now-offAt;
                    if(delta>=250ms&&delta<2500ms)
                        takes[index].tail.append(l,r);
                }
            }
            if(now-lastReport>=15s) {
                std::printf("P007AH_PRIVATE_PROGRESS original_ready=%d take=%zu/%zu\n",
                            ready(),index,scenarios.size());
                std::fflush(stdout);lastReport=now;
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(index==scenarios.size(),
                "original Windows-95 guest did not complete twelve Pad Sweep A/B/A matrix notes");
        require(p007::original_audio_proof::accept(before,originalFirst),
                "original Creative PCM not proven above Win95 background");
        using namespace p007::ten_matrix;
        require(proof::stable(takes[0],takes[1]),
                "P007AG independent original SoundFont A1/A2 baseline is unstable");
        unsigned measurable=0u,inconclusive=0u,unstable=0u;
        for(unsigned k=0;k<selected.size();++k) {
            const auto& knob=knobs[selected[k]];
            const unsigned a=1u+k*2u,b=a+1u,back=b+1u;
            const auto e=evaluate(takes[0],takes[a],takes[b],
                                  takes[back],knob.type);
            if(e.verdict==Verdict::Proven)++measurable;
            else if(e.verdict==Verdict::Inconclusive)++inconclusive;
            else ++unstable;
            std::printf("P007AH_ORIGINAL_WIN95_CREATIVE_NRPN_RESULT"
                " control=%s vst3=%u verdict=%s"
                " A_onset_rms=%.9f B_onset_rms=%.9f"
                " A_return_rms=%.9f"
                " A_roughness=%.7f B_roughness=%.7f"
                " A_tail_rms=%.9f B_tail_rms=%.9f"
                " onset_effect=%d sustain_effect=%d tail_effect=%d"
                " reference_before_stable=%d reference_after_stable=%d"
                " original_win95=1 synthetic=0\n",
                knob.title,knob.vstId,verdictName(e.verdict),
                e.aOnset,e.bOnset,e.restoredOnset,
                e.aRoughness,e.bRoughness,e.aTail,e.bTail,
                int(e.onsetChanged),int(e.sustainChanged),int(e.tailChanged),
                int(e.stableBefore),int(e.stableAfter));
            std::fflush(stdout);
        }
        std::printf("P007AH_ORIGINAL_WIN95_CREATIVE_FIVE_CONTROL_SUMMARY"
            " measured=%u inconclusive=%u unstable=%u total=%zu"
            " original_guest=1 synthetic=0\n",
            measurable,inconclusive,unstable,selected.size());
        std::fflush(stdout);
        require(unstable==0u,
            "P007AG original A/B/A restoration failed for one or more controls."
            " Read individual original-PCM RESULT rows.");
        // This is an honest DIAGNOSTIC, not a fake 5/5 acoustic PASS.
        // Pad8 may prove 0..5 additional generators. If it proves fewer,
        // keep inconclusive cases open rather than calling them defective.
        std::printf("P007AH_PAD8_SWEEP_FIVE_CONTROL_DIAGNOSTIC"
            " measured=%u inconclusive=%u unstable=%u"
            " original_rom_program_midi=95 genuine_creative=1 synthetic=0\n",
            measurable,inconclusive,unstable);
        std::puts("P007AH_PRIVATE_PAD8_SWEEP_DIAGNOSTIC_COMPLETED");
    }catch(...) {
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}

void privateEnvFilterDepthProof(const VST3::Hosting::PluginFactory& factory,
                              const VST3::UID& id,FUnknown* host,
                              const fs::path& bundle,
                              bool registerProbe=false,bool spectralProbe=false,
                              bool correctedFilterProbe=false) {
    const auto root=bundle/"Contents"/"Resources"/"PrivateVM";
    const auto disk=root/"vm"/"win95.img";
    require(fs::is_regular_file(disk)&&fs::file_size(disk)>=32ull*1024ull*1024ull,
            "P007AG requires real owner-private Win95 HDD");
    require(!fs::exists(root/"P007_SYNTHETIC_TEST_ONLY.marker")&&
            fs::is_directory(root/"roms")&&fs::is_directory(root/"assets")&&
            fs::is_regular_file(root/"vm"/"P007_AWE64.cfg"),
            "P007AD rejects synthetic or incomplete licensed original guest");
    OwnerGuestSandbox profile;
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,
            "private original ROM bank 0");
    require(plugin.controller->setParamNormalized(programId,95./127.)==kResultOk,
            "private original GM Pad 8 Sweep program 95");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "original output unity");
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;setup.sampleRate=44100.;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private original native 44.1k");
    require(plugin.component->setActive(true)==kResultOk,
            "private original 86Box/Creative activate");
    bool active=true,processing=false;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private original guest process start");
        processing=true;
        const HMODULE dll=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(dll!=nullptr,"authentic original core missing");
        using Symbol=std::uintptr_t (__cdecl*)();
        using Ready=int (__cdecl*)();
        const auto originalInit=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_init"));
        const auto originalRun=reinterpret_cast<Symbol>(
            GetProcAddress(dll,"p007_core_symbol_pc_run"));
        const auto ready=reinterpret_cast<Ready>(
            GetProcAddress(dll,"p007_midi_guest_ready"));
        require(originalInit&&originalRun&&ready&&
                originalInit()!=0u&&originalRun()!=0u,
                "original 86Box CPU symbols required, synthetic core forbidden");
        // CC7 positive control separates generic Win95 WinMM controller
        // delivery from NRPN-specific driver support. If CC7=0 cannot
        // mute the ORIGINAL sound, do not blame the NRPN mapping yet.
        // Unlike VST3 GUI parameter events, this direct test-only send
        // still uses the exact same p007_midi_submit UART packet FIFO,
        // and is invoked on the same host/audio driver thread.
        using Midi= int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
        const auto sendMidi=reinterpret_cast<Midi>(
                         GetProcAddress(dll,"p007_midi_submit"));
        require(sendMidi!=nullptr,
                "authentic 86Box UART MIDI producer export unavailable");
        // P007AJ diagnostics never run in shipped plugin. The original
        // Core DLL must include an opt-in PASSIVE register capture shim;
        // it is built from pinned original 86Box with snd_emu8k.c SHA
        // verified unchanged (only the old trace wrapper is instrumented).
        using ProbeVersion=unsigned(__cdecl*)();
        using ProbeBegin=int(__cdecl*)(unsigned);
        using ProbeStop=unsigned(__cdecl*)();
        using ProbeCount=unsigned(__cdecl*)();
        using ProbeDropped=unsigned(__cdecl*)();
        using ProbeCopy=unsigned(__cdecl*)(p007aj_register_event*,unsigned,unsigned);
        ProbeVersion probeVersion=nullptr;
        ProbeBegin probeBegin=nullptr;
        ProbeStop probeStop=nullptr;
        ProbeCount probeCount=nullptr;
        ProbeDropped probeDropped=nullptr;
        ProbeCopy probeCopy=nullptr;
        struct FileCloser {void operator()(FILE* f)const noexcept{if(f)std::fclose(f);}};
        std::unique_ptr<FILE,FileCloser> traceFile;
        bool probeCapturing=false;
        if(registerProbe) {
            probeVersion=reinterpret_cast<ProbeVersion>(
                GetProcAddress(dll,"p007aj_probe_version"));
            probeBegin=reinterpret_cast<ProbeBegin>(
                GetProcAddress(dll,"p007aj_probe_begin"));
            probeStop=reinterpret_cast<ProbeStop>(
                GetProcAddress(dll,"p007aj_probe_stop"));
            probeCount=reinterpret_cast<ProbeCount>(
                GetProcAddress(dll,"p007aj_probe_count"));
            probeDropped=reinterpret_cast<ProbeDropped>(
                GetProcAddress(dll,"p007aj_probe_dropped"));
            probeCopy=reinterpret_cast<ProbeCopy>(
                GetProcAddress(dll,"p007aj_probe_copy"));
            require(probeVersion&&probeBegin&&probeStop&&probeCount&&
                    probeDropped&&probeCopy&&probeVersion()==P007AJ_API_VERSION,
                    "P007AJ bounded original EMU8000 register observer unavailable; "
                    "reject normal uninstrumented production core");
            const auto tracePath=bundle.parent_path()/
                (correctedFilterProbe?
                    L"P007AL-ORIGINAL-EMU8000-REGISTER-EVENTS.csv":
                    L"P007AJ-EMU8000-REGISTER-EVENTS.csv");
            traceFile.reset(_wfopen(tracePath.c_str(),L"wb"));
            require(bool(traceFile),"cannot create bounded ORIGINAL register CSV");
            std::fprintf(traceFile.get(),"scenario,scenario_name,serial,"
                 "chip_frame_44100,io_port_hex,register_word_hex\n");
            std::puts("P007AJ_REAL_ORIGINAL_86BOX_REGISTER_SHIM_PRESENT"
                 " cap_per_take=8192 no_unbounded_file=1"
                 " original_emu8000_dsp_source_unmodified=1");
        }
        struct Scenario {
            const char* name;
            ParamID param;
            double value;
            int ccNumber=-1;
            int ccValue=0;
            // QA-only NRPN24 signed depth: 999=unchanged, 1000=CC121.
            int depth=999;
        };
        // P007AI: four remaining Filter ENV ADSR controls are inaudible
        // on Program40/95 until the ORIGINAL Creative NRPN24 filter depth
        // route is activated. Verify both depth signs before fixing one.
        // IMPORTANT: each VST3 0%-return emits CC121 and resets NRPN24.
        // Reapply depth before A-back notes for a truthful A/B/A contrast.
        constexpr std::array<unsigned,4> selected{3u,4u,5u,6u};
        // P007AO separates real Sustain A/B/A and Release A/B/A.
        std::array<Scenario,15> scenarios{};
        scenarios[0]={"ORIGINAL_PAD8_A1",0u,0.};
        scenarios[1]={"ORIGINAL_PAD8_A2",0u,0.};
        scenarios[2]={"SIGNED_DEPTH_NEGATIVE96",0u,0.,-1,0,-96};
        scenarios[3]={"SIGNED_DEPTH_POSITIVE96",0u,0.,-1,0,96};
        scenarios[4]={"MOD_ENVELOPE_DEPTH_BASELINE",0u,0.};
        for(unsigned k=0;k<selected.size();++k) {
            const auto& knob=p007::ten_matrix::knobs[selected[k]];
            scenarios[5u+2u*k]={knob.title,knob.vstId,knob.normalized};
            scenarios[6u+2u*k]={"RESTORE_ENV_ADSR_AND_REAPPLY_DEPTH",
                                knob.vstId,0.};
        }
        scenarios[13]={"RESTORE_ORIGINAL_SOUNDFONT_CC121",0u,0.,-1,0,1000};
        const unsigned scenarioCount=correctedFilterProbe?15u:14u;
        if(correctedFilterProbe) {
            // Sustain A/B/A: 8 normal, 9 changed, 10 restored (not held).
            // Release A/B/A: 11 held, 12 changed, 13 restored (held).
            scenarios[14]=scenarios[13];
            scenarios[11]={"RELEASE_A_BASELINE_HELD_MOD_SUSTAIN",0u,0.};
            const auto& releaseKnob=p007::ten_matrix::knobs[selected[3]];
            scenarios[12]={releaseKnob.title,releaseKnob.vstId,
                           releaseKnob.normalized};
            scenarios[13]={"RELEASE_A_RETURN_HELD_MOD_SUSTAIN",
                           releaseKnob.vstId,0.};
        }
        namespace proof=p007::original_control_proof;
        std::array<proof::Take,15> takes{};
        std::array<p007::original_spectral_proof::Capture,15> spectra{};
        std::array<p007::original_chip_register_proof::Capture,15> chipRegisters{};
        p007::original_audio_proof::Window before,originalFirst;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=block;data.numOutputs=1;data.outputs=&out;
        data.inputEvents=&events;
        using Clock=std::chrono::steady_clock;
        enum class Phase {Boot,Prepare,Note,Tail};
        Phase phase=Phase::Boot;
        std::size_t index=0;
        Clock::time_point readyAt{},phaseAt{},noteAt{},offAt{};
        const auto start=Clock::now();
        auto next=start,lastReport=start;
        // QA-only signed NRPN24 sender, exactly ONE original Creative
        // six-byte UART packet per >=40ms; failures retry same packet.
        p007::original_filter_env_depth::Transmitter depthSender{};
        p007::original_filter_env_depth::Transmitter ampReleaseSender{};
        p007::original_filter_env_depth::Transmitter modSustainSender{};
        // Original Creative NRPN8 (mod/filter ENV sustain), offset-encoded
        // value +8192. Level 127 holds the filter ENV above zero until
        // NoteOff; otherwise 007F decays to zero long before filter Release.
        constexpr unsigned kFilterSustainForRelease=8192u+127u;
        constexpr unsigned kSlowAmpReleaseUnits=1800u; // Creative NRPN15: 7.2 seconds
        Clock::time_point lastDepthPacket{};
        bool cc121Pending=false;
        int selectedDepth=correctedFilterProbe?96:-96;
        const auto queueDepth=[&](int requested) {
            depthSender=p007::original_filter_env_depth::Transmitter{};
            cc121Pending=(requested==1000);
            if(requested>=-127&&requested<=127)
                depthSender.arm(requested);
            lastDepthPacket=Clock::time_point{};
        };
        while(Clock::now()-start<310s&&index<scenarioCount) {
            events.clear();
            const auto now=Clock::now();
            if(phase==Phase::Boot) {
                if(readyAt==Clock::time_point{}&&ready()) {
                    readyAt=now;std::puts("P007AI_ORIGINAL_WIN95_WINMM_READY");
                    if(registerProbe) {
                        require(probeBegin(0u)!=0,
                                "P007AJ original chip note0 capture start");
                        probeCapturing=true;
                    }
                }
                if(readyAt!=Clock::time_point{}&&
                   now-readyAt>=1500ms&&before.frames>=44100u) {
                    phase=Phase::Prepare;phaseAt=now;
                }
            }else if(phase==Phase::Prepare) {
                // Owner's VST3 resets tone parameters via Creative CC121,
                // then replays other original controls. Do not interleave
                // independent NRPN24 while this happens.
                if(now-phaseAt>=350ms) {
                    if(cc121Pending) {
                        if(sendMidi(0xb0u,121u,0u)==1) {
                            cc121Pending=false;lastDepthPacket=now;
                        }
                    }else if(depthSender.active() &&
                        (lastDepthPacket==Clock::time_point{} ||
                         now-lastDepthPacket>=40ms)) {
                        (void)depthSender.tick(
                            [&](std::uint8_t controller,
                               std::uint8_t value)noexcept{
                                const bool accepted=
                                     sendMidi(0xb0u,controller,value)==1;
                                if(accepted)lastDepthPacket=now;
                                return accepted;
                            });
                    }else if(ampReleaseSender.active() &&
                        (lastDepthPacket==Clock::time_point{} ||
                         now-lastDepthPacket>=40ms)) {
                        (void)ampReleaseSender.tick(
                            [&](std::uint8_t controller,
                               std::uint8_t value)noexcept{
                                const bool accepted=
                                     sendMidi(0xb0u,controller,value)==1;
                                if(accepted)lastDepthPacket=now;
                                return accepted;
                            });
                    }else if(modSustainSender.active() &&
                        (lastDepthPacket==Clock::time_point{} ||
                         now-lastDepthPacket>=40ms)) {
                        (void)modSustainSender.tick(
                            [&](std::uint8_t controller,
                               std::uint8_t value)noexcept{
                                const bool accepted=
                                     sendMidi(0xb0u,controller,value)==1;
                                if(accepted)lastDepthPacket=now;
                                return accepted;
                            });
                    }
                }
                if(now-phaseAt>=1100ms && !cc121Pending &&
                   !depthSender.active() && !ampReleaseSender.active() &&
                   !modSustainSender.active()) {
                    require(ready()!=0,"original Win95 Creative READY lost");
                    Event e{};e.type=Event::kNoteOnEvent;e.busIndex=0;
                    e.noteOn.channel=0;e.noteOn.pitch=60;
                    e.noteOn.velocity=.86f;
                    require(events.addEvent(e)==kResultOk,
                            "P007AI original NoteOn event not queued");
                    noteAt=now;phase=Phase::Note;
                }
            }else if(phase==Phase::Note&&now-noteAt>=3150ms) {
                Event e{};e.type=Event::kNoteOffEvent;e.busIndex=0;
                e.noteOff.channel=0;e.noteOff.pitch=60;
                require(events.addEvent(e)==kResultOk,
                        "original NoteOff event not queued");
                offAt=now;phase=Phase::Tail;
            }else if(phase==Phase::Tail&&now-offAt>=2750ms) {
                if(registerProbe&&probeCapturing){
                    const unsigned attempted=probeStop();
                    const unsigned captured=probeCount();
                    const unsigned missed=probeDropped();
                    probeCapturing=false;
                    require(captured<=P007AJ_CAP && missed+captured==attempted,
                            "P007AJ original register observer count overflow");
                    std::array<p007aj_register_event,256u> chunk{};
                    // The original Creative allocator does NOT stick to voice 0.
                    // Decode pointer low five bits for every genuine voice.
                    for(unsigned offset=0u;offset<captured;){
                        const auto n=probeCopy(chunk.data(),offset,
                                               static_cast<unsigned>(chunk.size()));
                        require(n!=0u,"P007AJ register copy missing event");
                        for(unsigned j=0;j<n;++j){
                            const auto& e=chunk[j];
                            chipRegisters[index].observe(e.port,e.value);
                            std::fprintf(traceFile.get(),
                              "%zu,%s,%u,%llu,%04X,%04X\n",
                              index,scenarios[index].name,e.serial,
                              static_cast<unsigned long long>(e.frame),
                              unsigned(e.port),unsigned(e.value));
                        }
                        offset+=n;
                    }
                    std::fflush(traceFile.get());
                    std::printf("P007AJ_ACTUAL_ORIGINAL_EMU8000_PORT_PROBE"
                       " scenario=%zu name=%s events=%u overflow=%u"
                       " csv_limit=8192 original_guest=1 synthetic=0\n",
                       index,scenarios[index].name,captured,missed);
                    std::fflush(stdout);
                }
                const auto& note=takes[index];
                std::printf("P007AI_PRIVATE_ORIGINAL_MATRIX_NOTE name=%s"
                            " onset_rms=%.9f roughness=%.9f"
                            " onset_frames=%llu sustain_rms=%.9f"
                            " tail_rms=%.9f\n",scenarios[index].name,
                            note.onset.rms(),note.onset.roughness(),
                            static_cast<unsigned long long>(note.onset.stereoFrames),
                            note.sustain.rms(),note.tail.rms());
                ++index;
                if(index==4u) {
                    // Determine which signed original envelope->filter
                    // modulation depth gives a stronger audible signal
                    // without mistaking a dropped NoteOn for a filter effect.
                    const double orig=takes[1].onset.rms();
                    const auto score=[&](unsigned n)noexcept{
                        if(!proof::nonSilent(takes[n]))return 0.;
                        const double signal=std::min(1.0,
                            takes[n].onset.rms()/std::max(.000001,orig));
                        return signal*std::max(
                           proof::relativeChange(orig,takes[n].onset.rms()),
                           proof::relativeChange(
                                takes[1].onset.roughness(),
                                takes[n].onset.roughness()));
                    };
                    const double negative=score(2u),positive=score(3u);
                    selectedDepth=correctedFilterProbe?96:
                        ((negative>=positive)?-96:96);
                    scenarios[4].depth=selectedDepth;
                    for(unsigned k=0;k<selected.size();++k) {
                        // Corrected protocol re-applies filter depth AFTER
                        // every changed GUI parameter, even for B scenarios.
                        scenarios[5u+2u*k].depth=correctedFilterProbe?selectedDepth:999;
                        scenarios[6u+2u*k].depth=selectedDepth;
                    }
                    if(correctedFilterProbe) {
                        scenarios[11].depth=selectedDepth;
                        scenarios[12].depth=selectedDepth;
                        scenarios[13].depth=selectedDepth;
                    }
                    if(correctedFilterProbe)
                        std::puts("P007AL_FORCED_CHIP_FILTER_CLOSING_DIRECTION"
                                  " nrpn24=+96 expected_original_PEFE=00C0");
                    std::printf("P007AI_ORIGINAL_CREATIVE_SIGNED_DEPTH_SELECTED"
                                " depth=%d negative_score=%.8f"
                                " positive_score=%.8f"
                                " reference_rms=%.9f negative_rms=%.9f"
                                " positive_rms=%.9f\n",
                                selectedDepth,negative,positive,orig,
                                takes[2].onset.rms(),takes[3].onset.rms());
                    std::fflush(stdout);
                }
                if(index<scenarioCount) {
                    if(registerProbe){
                        require(probeBegin(static_cast<unsigned>(index))!=0,
                                "P007AJ original chip note capture not started");
                        probeCapturing=true;
                    }
                    const auto& chosen=scenarios[index];
                    if(chosen.param!=0u)
                        require(plugin.controller->setParamNormalized(
                            chosen.param,chosen.value)==kResultOk,
                            "original Creative NRPN host automation failed");
                    if(chosen.ccNumber>=0)
                        require(sendMidi(0xb0u,
                             static_cast<std::uint8_t>(chosen.ccNumber),
                             static_cast<std::uint8_t>(chosen.ccValue))==1,
                            "positive-control MIDI CC7 was not queued");
                    if(chosen.depth!=999)
                        queueDepth(chosen.depth);
                    if(correctedFilterProbe && index>=4u && index<=13u) {
                        // Volume envelope is independent of filter envelope.
                        // Re-apply original Creative NRPN15 after any CC121;
                        // keep it IDENTICAL across A/B/A notes.
                        ampReleaseSender.armUnsigned(15u,kSlowAmpReleaseUnits);
                        // Release A/B/A takes 11..13: do not modify
                        // Sustain return at take10.
                        // on NoteOn: zero modulation envelope by NoteOff.
                        // Sustain the original ENV before comparing rates.
                        if(index>=11u)
                            modSustainSender.armUnsigned(8u,kFilterSustainForRelease);
                    }
                    phase=Phase::Prepare;phaseAt=now;
                }
            }
            require(plugin.processor->process(data)==kResultOk,
                    "authentic original VST3 audio process failed");
            for(unsigned i=0;i<block;++i) {
                const auto l=left[i],r=right[i];
                require(std::isfinite(l)&&std::isfinite(r),
                        "nonfinite original EMU8000 PCM");
                if(index>=scenarioCount)continue;
                if(spectralProbe && (phase==Phase::Note || phase==Phase::Tail))
                    spectra[index].append(l,r,phase==Phase::Tail);
                if(phase==Phase::Boot&&readyAt!=Clock::time_point{})
                    before.append(l,r);
                if(phase==Phase::Note) {
                    const auto delta=now-noteAt;
                    if(delta>=500ms && delta<2250ms) {
                        takes[index].onset.append(l,r);
                        if(index==0u)originalFirst.append(l,r);
                    }
                    if(delta>=2250ms && delta<3000ms)
                        takes[index].sustain.append(l,r);
                }else if(phase==Phase::Tail) {
                    const auto delta=now-offAt;
                    if(delta>=250ms&&delta<2500ms)
                        takes[index].tail.append(l,r);
                }
            }
            if(now-lastReport>=15s) {
                std::printf("P007AI_PRIVATE_PROGRESS original_ready=%d take=%zu/%zu\n",
                            ready(),index,scenarioCount);
                std::fflush(stdout);lastReport=now;
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(index==scenarioCount,
                "original Windows-95 guest did not finish fourteen original NRPN24 depth scenarios");
        if(spectralProbe) {
            // All analysis bins are collected in preallocated memory while
            // VST3 process() runs. Write AFTER playback, never in its callback.
            const auto csvPath=bundle.parent_path()/
                (correctedFilterProbe?
                    L"P007AL-ORIGINAL-PCM-SPECTRAL-BANDS.csv":
                    L"P007AK-ORIGINAL-PCM-SPECTRAL-BANDS.csv");
            std::unique_ptr<FILE,FileCloser> csv(_wfopen(csvPath.c_str(),L"wb"));
            require(bool(csv),"cannot create P007AK original spectral-bin report");
            std::fprintf(csv.get(),
                "scenario,scenario_name,phase,quarter_second_bin,"
                "band,frames,total_energy,band_energy\n");
            for(unsigned takeIndex=0;takeIndex<scenarioCount;++takeIndex) {
                const auto& cap=spectra[takeIndex];
                for(unsigned i=0;i<p007::original_spectral_proof::bins;++i) {
                    for(unsigned k=0;k<p007::original_spectral_proof::bands;++k) {
                        std::fprintf(csv.get(),"%u,%s,%s,%u,%u,%u,%.17g,%.17g\n",
                          takeIndex,scenarios[takeIndex].name,
                          i<p007::original_spectral_proof::noteBins?"NOTE":"RELEASE",
                          i<p007::original_spectral_proof::noteBins?i:
                             i-p007::original_spectral_proof::noteBins,
                          k,cap.count[i],cap.totalPower[i],
                          cap.bandPower[i][k]);
                    }
                }
            }
            require(std::fflush(csv.get())==0,
                "P007AK original spectral-bin report flush failed");
            std::puts("P007AK_ORIGINAL_PCM_SPECTRAL_RAW_BINS_SAVED_OFFLINE");
        }
        if(correctedFilterProbe) {
            for(unsigned t=4u;t<=13u;++t) {
                const auto& cr=chipRegisters[t];
                std::printf("P007AL_ORIGINAL_CHIP_CONTROL take=%u voice=%u"
                    " PEFE=%04X VOLUME_RELEASE_DCYSUSV=%04X"
                    " FILTER_NOTE_DCYSUS=%04X"
                    " FILTER_RELEASE_DCYSUS=%04X complete=%d\n",
                    t,cr.voice,unsigned(cr.pefe),unsigned(cr.volumeRelease),
                    unsigned(cr.filterNoteSustain),unsigned(cr.filterRelease),
                    int(cr.complete()));
            }
            std::fflush(stdout);
            require(chipRegisters[4].complete() &&
                    chipRegisters[4].volumeRelease!=0x8029u,
                "P007AL original Creative NRPN15 did not extend amplitude release");
            for(unsigned t=4u;t<=13u;++t) {
                require(chipRegisters[t].complete(),
                    "P007AL original chip evidence incomplete or ambiguous for allocated voice");
                require(chipRegisters[t].pefe==0x00c0u,
                    "P007AL unexpected original chip filter PEFE; requested closing polarity not applied");
                require(chipRegisters[t].volumeRelease==chipRegisters[4].volumeRelease,
                    "P007AL amplifier release differs across A/B/A - invalid filter comparison");
            }
            // P007AO: do not allow another false Sustain A/B/A caused
            // by reusing an already prepared Release reference. The
            // independent sustain A must return to original after B.
            require(chipRegisters[8].hasFilterNoteSustain &&
                    chipRegisters[9].hasFilterNoteSustain &&
                    chipRegisters[10].hasFilterNoteSustain &&
                    chipRegisters[8].filterNoteSustain==0x007fu &&
                    chipRegisters[9].filterNoteSustain==0x7f7fu &&
                    chipRegisters[10].filterNoteSustain==0x007fu,
                "P007AO original Sustain A/B/A not restored at chip; invalid QA baseline");
            // Release rate has no musical meaning if the mod envelope
            // already decayed to zero during the 3.15-second held note.
            // Verify a genuinely high and IDENTICAL original chip sustain
            // for A/B/A and that Creative clears the sustain at NoteOff.
            for(unsigned t=11u;t<=13u;++t) {
                require(chipRegisters[t].hasFilterNoteSustain &&
                        chipRegisters[t].filterNoteSustain==0x7f7fu,
                    "P007AN original modulation sustain not held in release A/B/A");
                require((chipRegisters[t].filterRelease&0x7f00u)==0u,
                    "P007AN Creative did not clear filter sustain on NoteOff");
            }
            require(chipRegisters[12].filterRelease!=chipRegisters[11].filterRelease &&
                    chipRegisters[13].filterRelease==chipRegisters[11].filterRelease,
                "P007AL original filter release generator did not change and restore");
            std::puts("P007AL_CORRECTED_ORIGINAL_CREATIVE_REGISTER_SETUP_PASS");
        }
        require(p007::original_audio_proof::accept(before,originalFirst),
                "original Creative PCM not proven above Win95 background");
        // AUDIO-ONLY authenticity check: original Win95/Creative PCM
        // must be nonzero after NoteOn, source A1/A2 stable, and the four
        // original filter-generator controls must each change the SAME
        // original signed-depth Pad8 patch and return to that baseline.
        using namespace p007::ten_matrix;
        require(proof::stable(takes[0],takes[1]),
                "P007AI original Pad8 A1/A2 baseline unstable");
        const bool sourceReverted=proof::stable(
            takes[0],takes[correctedFilterProbe?14u:13u]);
        const bool depthNoteAudible=proof::nonSilent(takes[4]);
        std::printf("P007AI_ORIGINAL_CREATIVE_FILTER_DEPTH"
                    " selected=%d original_rms=%.9f"
                    " negative96_rms=%.9f positive96_rms=%.9f"
                    " chosen_rms=%.9f source_return_rms=%.9f"
                    " source_reverted=%d original_note_audible=%d"
                    " genuine_original_win95=1 synthetic=0\n",
                    selectedDepth,takes[1].onset.rms(),
                    takes[2].onset.rms(),takes[3].onset.rms(),
                    takes[4].onset.rms(),
                    takes[correctedFilterProbe?14u:13u].onset.rms(),
                    int(sourceReverted),int(depthNoteAudible));
        unsigned measured=0u,inconclusive=0u,unstable=0u;
        unsigned spectralMeasured=0u;
        for(unsigned k=0;k<selected.size();++k) {
            const auto& knob=knobs[selected[k]];
            const unsigned a=(correctedFilterProbe && k==3u)?11u:
                                   4u+2u*k;
            const unsigned b=a+1u,back=b+1u;
            const auto e=evaluate(
                takes[(correctedFilterProbe && k==3u)?a:4u],
                takes[a],takes[b],takes[back],knob.type);
            if(e.verdict==Verdict::Proven)++measured;
            else if(e.verdict==Verdict::Inconclusive)++inconclusive;
            else ++unstable;
            std::printf("P007AI_ORIGINAL_CREATIVE_FILTER_ENV_RESULT"
                " control=%s vst3=%u nrpn24_depth=%d verdict=%s"
                " A_onset_rms=%.9f B_onset_rms=%.9f"
                " A_return_rms=%.9f A_roughness=%.7f"
                " B_roughness=%.7f"
                " A_sustain_rms=%.9f B_sustain_rms=%.9f"
                " A_tail_rms=%.9f B_tail_rms=%.9f"
                " baseline_stable=%d restored_stable=%d"
                " genuine_original_win95=1 synthetic=0\n",
                knob.title,knob.vstId,selectedDepth,verdictName(e.verdict),
                e.aOnset,e.bOnset,e.restoredOnset,
                e.aRoughness,e.bRoughness,
                takes[a].sustain.rms(),takes[b].sustain.rms(),
                e.aTail,e.bTail,int(e.stableBefore),int(e.stableAfter));
            std::fflush(stdout);
            if(spectralProbe) {
                namespace sp=p007::original_spectral_proof;
                const auto target=static_cast<sp::Target>(k);
                const auto measure=sp::evaluate(spectra[a],spectra[b],spectra[back],target);
                if(correctedFilterProbe) {
                    const auto cm=p007::corrected_filter_proof::evaluate(
                        spectra[a],spectra[b],spectra[back],target);
                    if(cm.proven)++spectralMeasured;
                    std::printf("P007AL_CORRECTED_ORIGINAL_PCM_RESULT control=%s"
                        " verdict=%s effect_db=%.4f drift_db=%.4f"
                        " band=%u timebin=%u consistent_bins=%u release_usable=%u"
                        " filter_NRPN24=96 amp_NRPN15=%u"
                        " release_NRPN8=127 guest=ORIGINAL_WIN95_CREATIVE\n",
                        knob.title,p007::corrected_filter_proof::verdict(cm),
                        cm.peakDb,cm.maxReferenceDriftDb,
                        cm.peakBand,cm.peakBin,cm.longestRun,
                        cm.usableReleaseBins,kSlowAmpReleaseUnits);
                    std::fflush(stdout);
                    continue;
                }
                if(measure.proven)++spectralMeasured;
                std::printf("P007AK_ORIGINAL_PCM_SPECTRAL_RESULT control=%s"
                  " verdict=%s effect_db=%.3f drift_db=%.3f"
                  " band=%u timebin=%u comparable=%u restored=%u"
                  " pairs=%u guest=ORIGINAL_WIN95_CREATIVE\n",
                  knob.title,sp::verdict(measure),measure.strongestDb,
                  measure.maxReferenceDriftDb,measure.strongestBand,
                  measure.strongestBin,measure.comparable,
                  measure.restored,measure.consecutive);
                std::fflush(stdout);
            }
        }
        if(correctedFilterProbe) {
           std::printf("P007AL_FOUR_CORRECTED_FILTER_ENVELOPES_PCM_SPECTRAL"
             " measured=%u unresolved=%u total=4 original_guest=1\n",
             spectralMeasured,4u-spectralMeasured);
           if(spectralMeasured==4u)
             std::puts("P007AL_FOUR_ORIGINAL_FILTER_ENVELOPES_PCM_PROVEN");
        } else if(spectralProbe){
           std::printf("P007AK_FOUR_ORIGINAL_FILTER_ENVELOPES_PCM_SPECTRAL"
             " measured=%u unresolved=%u total=4 original_guest=1\n",
             spectralMeasured,4u-spectralMeasured);
           if(spectralMeasured==4u)
             std::puts("P007AK_FOUR_ORIGINAL_CREATIVE_FILTER_ENVELOPES_PCM_SPECTRAL_PROVEN");
        }
        std::printf("P007AI_ORIGINAL_CREATIVE_FOUR_FILTER_ENV_SUMMARY"
                    " measured=%u inconclusive=%u unstable=%u total=4"
                    " original_return_stable=%d chosen_depth=%d"
                    " original_guest=1 synthetic=0\n",
                    measured,inconclusive,unstable,
                    int(sourceReverted),selectedDepth);
        std::fflush(stdout);
        if(measured==4u&&unstable==0u&&sourceReverted)
            std::puts("P007AI_FOUR_ORIGINAL_CREATIVE_FILTER_ENVELOPES_PCM_PROVEN");
        // Completion is a diagnostic verdict, NOT a claim all four work.
        std::puts("P007AI_PRIVATE_ORIGINAL_ENV_DEPTH_DIAGNOSTIC_COMPLETED");
    }catch(...) {
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
        require(argc==2 || (argc==3 && (std::strcmp(argv[2],"--private-audio")==0 || std::strcmp(argv[2],"--private-nrpn")==0 || std::strcmp(argv[2],"--private-matrix")==0 || std::strcmp(argv[2],"--private-pad-sweep")==0 || std::strcmp(argv[2],"--private-env-depth")==0 || std::strcmp(argv[2],"--private-regtrace")==0 || std::strcmp(argv[2],"--private-spectrum")==0 || std::strcmp(argv[2],"--private-filter-corrected")==0 || std::strcmp(argv[2],"--synthetic")==0)),
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
        if(argc==3 && std::strcmp(argv[2],"--private-nrpn")==0)
            privateOriginalNrpnAudio(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--private-matrix")==0)
            privateTenControlMatrix(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--private-pad-sweep")==0)
            privateFiveControlPadSweep(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--private-env-depth")==0)
            privateEnvFilterDepthProof(factory,choice->ID(),host.get(),bundle);
        if(argc==3 && std::strcmp(argv[2],"--private-regtrace")==0)
            privateEnvFilterDepthProof(factory,choice->ID(),host.get(),bundle,true);
        if(argc==3 && std::strcmp(argv[2],"--private-spectrum")==0)
            privateEnvFilterDepthProof(factory,choice->ID(),host.get(),bundle,false,true);
        if(argc==3 && std::strcmp(argv[2],"--private-filter-corrected")==0)
            privateEnvFilterDepthProof(factory,choice->ID(),host.get(),bundle,true,true,true);
        std::puts("P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS");
        return 0;
    }catch(const std::exception& ex){
        std::fprintf(stderr,"P007W_ACTUAL_WINDOWS_VST3_HOST_QA_FAIL %s\n",ex.what());
        return 1;
    }
}
