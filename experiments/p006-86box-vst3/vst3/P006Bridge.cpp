#include "P006Bridge.hpp"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace p006 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr char kAudioName[]="Local\\125A_P006_AUDIO_DEMO";
constexpr char kMidiPipe[]="\\\\.\\pipe\\125A_P006_MIDI_DEMO";
[[nodiscard]] std::uint8_t midiVelocity(float x) noexcept {
    if(!std::isfinite(x))return 0;
    const auto clipped=std::clamp(x,0.f,1.f);
    return static_cast<std::uint8_t>(clipped*127.f+0.5f);
}
}
tresult PLUGIN_API Plugin::initialize(FUnknown* context) {
    if(SingleComponentEffect::initialize(context)!=kResultOk)
        return kResultFalse;
    addEventInput(STR16("Creative MIDI In"),16);
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
        if(!audio_.create(kAudioName))return kResultFalse;
        if(!midi_.startNonRealtime(kMidiPipe)){
            audio_.close();return kResultFalse;
        }
        // Optional quick private proof with the UNMODIFIED 86Box.exe.
        // Caller launches DAW with AWE64_P006_WAV_PATH set to the
        // SAME absolute file as 86Box's AWE32_WAV environment setting.
        // Do NOT enable when using the patched direct fork PCM hook:
        // that would create two audio writers in one SPSC ring.
        char wavPath[512]{};
        const DWORD pathLen=GetEnvironmentVariableA(
            "AWE64_P006_WAV_PATH",wavPath,
            static_cast<DWORD>(sizeof(wavPath)));
        if(pathLen>0u){
            if(pathLen>=sizeof(wavPath)||
               !wavTail_.startNonRealtime(audio_.data(),wavPath)){
                midi_.stopNonRealtime();audio_.close();return kResultFalse;
            }
        }
        active_=true;
    }else if(active_) {
        wavTail_.stopNonRealtime();
        midi_.stopNonRealtime();
        audio_.close();active_=false;
    }
    return SingleComponentEffect::setActive(on);
}
tresult PLUGIN_API Plugin::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize==kSample32?kResultTrue:kResultFalse;
}
tresult PLUGIN_API Plugin::process(ProcessData& data) {
    // NOTE ON/OFF only. COM latency not sample-accurate.
    if(active_&&data.inputEvents) {
        const auto count=data.inputEvents->getEventCount();
        for(int32 i=0;i<count;++i) {
            Event e{};
            if(data.inputEvents->getEvent(i,e)!=kResultOk)continue;
            if(e.type==Event::kNoteOnEvent) {
                const auto& n=e.noteOn;
                if(n.channel<0||n.channel>15||n.pitch<0||n.pitch>127)continue;
                (void)midi_.submit(static_cast<std::uint8_t>(0x90|n.channel),
                    static_cast<std::uint8_t>(n.pitch),midiVelocity(n.velocity));
            }else if(e.type==Event::kNoteOffEvent) {
                const auto& n=e.noteOff;
                if(n.channel<0||n.channel>15||n.pitch<0||n.pitch>127)continue;
                (void)midi_.submit(static_cast<std::uint8_t>(0x80|n.channel),
                    static_cast<std::uint8_t>(n.pitch),midiVelocity(n.velocity));
            }
        }
    }
    if(data.numOutputs<1||!data.outputs||data.numSamples<0)
        return kResultOk;
    auto& b=data.outputs[0];
    if(!b.channelBuffers32||b.numChannels<2)return kResultOk;
    float* left=b.channelBuffers32[0];
    float* right=b.channelBuffers32[1];
    if(!left||!right)return kResultOk;
    const auto n=static_cast<std::uint32_t>(data.numSamples);
    if(active_&&audio_.data())
        (void)p006::read(*audio_.data(),left,right,n);
    else for(std::uint32_t i=0;i<n;++i){left[i]=0;right[i]=0;}
    b.silenceFlags=0;
    return kResultOk;
}
}
