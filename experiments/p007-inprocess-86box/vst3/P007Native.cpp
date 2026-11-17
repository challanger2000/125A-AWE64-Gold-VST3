#include "P007Native.hpp"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
namespace p007 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
// Address within THIS plugin module, not Studio Pro executable.
static unsigned char anchor=0;
std::uint8_t midiVelocity(float x) noexcept {
    if(!std::isfinite(x))return 0u;
    return static_cast<std::uint8_t>(
        std::clamp(x,0.f,1.f)*127.f+0.5f);
}
} // namespace
bool Core::openNonRealtime() noexcept {
    if(module_)return true;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&anchor),&own))return false;
    wchar_t path[2048]{};
    const DWORD n=GetModuleFileNameW(own,path,2048u);
    if(!n||n>=2048u)return false;
    wchar_t* slash=wcsrchr(path,L'\\');
    if(!slash)return false;
    slash[1]=L'\0';
    if(wcscat_s(path,2048u,L"125A_AWE_Core.dll")!=0)return false;
    // Explicit local module path; NEVER search or spawn 86Box.exe.
    module_=LoadLibraryExW(path,nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module_)return false;
    pull=reinterpret_cast<Pull>(GetProcAddress(module_,"p007_audio_pull"));
    midi=reinterpret_cast<Midi>(GetProcAddress(module_,"p007_midi_submit"));
    midiEnabled=reinterpret_cast<Enabled>(GetProcAddress(module_,"p007_midi_uart_enable"));
    if(!pull||!midi||!midiEnabled) {
        closeNonRealtime();
        return false;
    }
    // The guest has NOT booted; no MIDI is accepted until Win95/AWE is ready.
    midiEnabled(0);
    return true;
}
void Core::closeNonRealtime() noexcept {
    if(module_) {
        if(midiEnabled)midiEnabled(0);
        pull=nullptr;midi=nullptr;midiEnabled=nullptr;
        FreeLibrary(module_);module_=nullptr;
    }
}
tresult PLUGIN_API Plugin::initialize(FUnknown* context) {
    if(SingleComponentEffect::initialize(context)!=kResultOk)
        return kResultFalse;
    addEventInput(STR16("Original Creative WinMM MIDI"),16);
    addAudioOutput(STR16("Original EMU8000 Stereo"),SpeakerArr::kStereo);
    return kResultOk;
}
tresult PLUGIN_API Plugin::terminate() {
    if(active_)(void)setActive(false);
    return SingleComponentEffect::terminate();
}
tresult PLUGIN_API Plugin::setActive(TBool on) {
    if(on) {
        if(active_)return kResultOk;
        // SDK activation is non-RT. Host cannot activate if internal core
        // is missing from the SAME .vst3 folder.
        if(!core_.openNonRealtime())return kResultFalse;
        active_=true;
    } else if(active_) {
        core_.closeNonRealtime();active_=false;
    }
    return SingleComponentEffect::setActive(on);
}
tresult PLUGIN_API Plugin::canProcessSampleSize(int32 size) {
    return size==kSample32?kResultTrue:kResultFalse;
}
tresult PLUGIN_API Plugin::process(ProcessData& data) {
    // Only constant-time calls to bounded, heapless native core FIFOs;
    // the original PC CPU/timers/Win95 MUST NOT run on this DAW thread.
    if(active_&&core_.midi&&data.inputEvents) {
        const auto n=data.inputEvents->getEventCount();
        for(int32 i=0;i<n;++i){
            Event event{};
            if(data.inputEvents->getEvent(i,event)!=kResultOk)continue;
            if(event.type==Event::kNoteOnEvent){
                auto const& e=event.noteOn;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)core_.midi(static_cast<std::uint8_t>(0x90|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity));
            }else if(event.type==Event::kNoteOffEvent){
                auto const& e=event.noteOff;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)core_.midi(static_cast<std::uint8_t>(0x80|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity));
            }
        }
    }
    if(data.numOutputs<1||!data.outputs||data.numSamples<=0)return kResultOk;
    auto& output=data.outputs[0];
    if(output.numChannels<2||!output.channelBuffers32)return kResultOk;
    float* l=output.channelBuffers32[0],*r=output.channelBuffers32[1];
    if(!l||!r)return kResultOk;
    constexpr std::uint32_t kChunk=512u;
    std::array<float,kChunk*2u> temp{};
    const auto count=static_cast<std::uint32_t>(data.numSamples);
    for(std::uint32_t pos=0;pos<count;) {
        const std::uint32_t n=std::min(kChunk,count-pos);
        const std::uint32_t got=active_&&core_.pull?
            core_.pull(temp.data(),n):0u;
        const std::uint32_t valid=std::min(got,n);
        for(std::uint32_t i=0;i<valid;++i){
            l[pos+i]=temp[2u*i];r[pos+i]=temp[2u*i+1u];
        }
        for(std::uint32_t i=valid;i<n;++i){
            l[pos+i]=0.f;r[pos+i]=0.f;
        }
        pos+=n;
    }
    output.silenceFlags=0;
    return kResultOk;
}
} // namespace p007
