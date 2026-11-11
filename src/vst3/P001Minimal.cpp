#include "P001Minimal.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace awe64::vst3 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr int kVoice = 29;      // single test voice, not the Creative P002 allocator
constexpr uint32_t kStart = 458995u, kLoopBegin = 459001u, kLoopEnd = 467282u;
// P001 test-only ROM region from the original AWE32 AWETEST tool, NOT GM/SBK.
constexpr uint8_t kState[8] = {'1','2','5','A','P','0','0','1'};
inline float digitalPcm16(int32_t s) noexcept {
    return static_cast<float>(std::clamp(s, int32_t(-32768), int32_t(32767))) / 32768.0f;
}
}
tresult PLUGIN_API Minimal::initialize(FUnknown* context) {
    const auto result = SingleComponentEffect::initialize(context);
    if (result != kResultOk) return result;
    addAudioOutput(STR16("EMU8000 Digital Stereo"), SpeakerArr::kStereo);
    addEventInput(STR16("MIDI Notes"), 16);
    return kResultOk;
}
tresult PLUGIN_API Minimal::terminate() {
    chip_.close();
    active_=false;
    return SingleComponentEffect::terminate();
}
tresult PLUGIN_API Minimal::setBusArrangements(
    SpeakerArrangement* inputs, int32 numInputs,
    SpeakerArrangement* outputs, int32 numOutputs) {
    // The SDK SingleComponentEffect default silently accepts mono, 5.1, etc.
    // The chip instrument is strictly stereo out with NO audio inputs.
    if (numInputs!=0 || numOutputs!=1 || !outputs || outputs[0]!=SpeakerArr::kStereo)
        return kResultFalse;
    return SingleComponentEffect::setBusArrangements(inputs,numInputs,outputs,numOutputs);
}
tresult PLUGIN_API Minimal::setupProcessing(ProcessSetup& setup) {
    // No quiet, inaccurate resampler. Multi-rate adapter is separately P004.
    if (setup.sampleRate != 44100.0 || setup.maxSamplesPerBlock<=0
        || setup.symbolicSampleSize!=kSample32) return kResultFalse;
    return SingleComponentEffect::setupProcessing(setup);
}
tresult PLUGIN_API Minimal::canProcessSampleSize(int32 size) {
    return size==kSample32 ? kResultTrue : kResultFalse;
}
tresult PLUGIN_API Minimal::setActive(TBool state) {
    if (!state) {
        processing_=false; active_=false; playingKey_=-1;
        chip_.close(); // non-RT activation thread only
        return kResultOk;
    }
    if (active_) return kResultOk;
    // P001 temporary private ROM location. Deliberately no ROM embedded in VST3.
    const char* path = std::getenv("AWE64_P001_ROM_PATH");
    if (!path || !*path) {
        std::fputs("125A P001 ROM missing (AWE64_P001_ROM_PATH)\n",stderr);
        return kResultFalse;
    }
    std::string error;
    if (!chip_.open(std::filesystem::path(path),error)) {
        std::fprintf(stderr,"125A P001 ROM init failed: %s\n",error.c_str());
        return kResultFalse;
    }
    active_=true; playingKey_=-1;
    return kResultOk;
}
tresult PLUGIN_API Minimal::setProcessing(TBool state) {
    processing_ = state!=0;
    return kResultOk;
}
void Minimal::write(uint16_t reg, uint16_t voice, uint16_t port, uint16_t value) noexcept {
    chip_.writePort(0xE22, uint16_t((reg<<5)|voice));
    chip_.writePort(port,value);
}
void Minimal::write32(uint16_t reg, uint16_t voice, uint16_t port, uint32_t value) noexcept {
    chip_.writePort(0xE22,uint16_t((reg<<5)|voice));
    chip_.writePort(port,uint16_t(value));
    chip_.writePort(uint16_t(port+2),uint16_t(value>>16));
}
void Minimal::noteOn(int pitch,float velocity) noexcept {
    if (!active_ || pitch<0 || pitch>127) return;
    if (!(velocity>0.f)) {noteOff(pitch);return;}
    // Fixed ROM voice with pitch-tracked IP. Creative velocity and bank
    // translation are explicitly reserved for P002/P003.
    write(1,31,0xA20,0x0004); // P001 audible stereo unmute
    write(5,kVoice,0xA20,0x0080); // stop previous test voice
    write32(3,kVoice,0x620,0x0000FFFFu);
    write32(2,kVoice,0x620,0x0000FFFFu);
    write32(6,kVoice,0x620,(128u<<24)|kLoopBegin);
    write32(7,kVoice,0x620,kLoopEnd);
    write32(0,kVoice,0xA20,kStart);
    const int ip=std::clamp(0xE000 + ((pitch-60)*4096)/12,0,65535);
    write(0,kVoice,0xE20,static_cast<uint16_t>(ip));
    write(1,kVoice,0xE20,0xFF00); // no hand-tuned gain/attenuation
    write(2,kVoice,0xE20,0);
    write(3,kVoice,0xE20,0);
    write(4,kVoice,0xE20,0);
    write(5,kVoice,0xE20,0);
    write(6,kVoice,0xA20,0x8000);
    write(6,kVoice,0xA22,0x7F7F);
    write(7,kVoice,0xA20,0x7F00);
    write(4,kVoice,0xA20,0x8000);
    write(4,kVoice,0xA22,0x7F7F);
    write(5,kVoice,0xA22,0x8000);
    write(7,kVoice,0xA22,0x8000);
    write32(1,kVoice,0x620,0x40000000u);
    write(5,kVoice,0xA20,0x7F00); // original chip trigger
    playingKey_=pitch;
}
void Minimal::noteOff(int pitch) noexcept {
    if (active_ && playingKey_==pitch) {
        // On this original chip, bit 0x0080 DISABLES the envelope
        // generator. 0x8080 therefore freezes its current level and leaves
        // the PCM audible: it is NOT a working NoteOff. Keep the engine on
        // and set the genuine release flag 0x8000 with fast test decay 0x7F.
        // P002 must derive program-specific release from Creative driver traces.
        write(5,kVoice,0xA20,0x807Fu);
        playingKey_=-1;
    }
}
void Minimal::render(int32 begin,int32 end,float* left,float* right) noexcept {
    while(begin<end) {
        const auto count=std::min<int32>(980,end-begin);
        chip_.render(scratch_.data(),static_cast<size_t>(count));
        for(int32 i=0;i<count;++i) {
            left[begin+i]=digitalPcm16(scratch_[2u*size_t(i)]);
            right[begin+i]=digitalPcm16(scratch_[2u*size_t(i)+1]);
        }
        begin+=count;
    }
}
tresult PLUGIN_API Minimal::process(ProcessData& data) {
    if (data.numSamples<0 || data.symbolicSampleSize!=kSample32)
        return kInvalidArgument;
    // A host may legitimately flush MIDI/parameter changes without an audio
    // buffer. Do not advance the EMU8000 clock on a zero-frame flush.
    if (data.numSamples==0 && (!data.outputs || data.numOutputs==0))
        return kResultOk;
    if (data.numOutputs!=1 || !data.outputs ||
        data.outputs[0].numChannels!=2 || !data.outputs[0].channelBuffers32)
        return kInvalidArgument;
    auto* left=data.outputs[0].channelBuffers32[0];
    auto* right=data.outputs[0].channelBuffers32[1];
    if (!left || !right) return kInvalidArgument;
    if (!active_ || !processing_ || !chip_.ready()) {
        std::fill_n(left,data.numSamples,0.f);
        std::fill_n(right,data.numSamples,0.f);
        data.outputs[0].silenceFlags=0x3;
        return kResultFalse; // no silent fake synth when ROM or activation is invalid
    }
    int32 pos=0;
    const int32 events=data.inputEvents?data.inputEvents->getEventCount():0;
    for(int32 i=0;i<events;++i) {
        Event e{};
        if (data.inputEvents->getEvent(i,e)!=kResultOk) continue;
        const auto offset=std::clamp(e.sampleOffset,int32(0),data.numSamples);
        if (offset<pos) return kInvalidArgument; // sorted real host events required
        render(pos,offset,left,right);
        pos=offset;
        if (e.type==Event::kNoteOnEvent && e.noteOn.channel==0)
            noteOn(e.noteOn.pitch,e.noteOn.velocity);
        else if(e.type==Event::kNoteOffEvent && e.noteOff.channel==0)
            noteOff(e.noteOff.pitch);
    }
    render(pos,data.numSamples,left,right);
    data.outputs[0].silenceFlags=0; // conservative if held note just ended
    return kResultOk;
}
tresult PLUGIN_API Minimal::getState(IBStream* stream) {
    if(!stream) return kInvalidArgument;
    int32 wrote=0;
    return stream->write(const_cast<uint8_t*>(kState),sizeof(kState),&wrote)==kResultOk &&
           wrote==sizeof(kState) ? kResultOk : kResultFalse;
}
tresult PLUGIN_API Minimal::setState(IBStream* stream) {
    if(!stream) return kInvalidArgument;
    std::array<uint8_t,8> received{};
    int32 count=0;
    if(stream->read(received.data(),received.size(),&count)!=kResultOk ||
       count!=static_cast<int32>(received.size()) ||
       !std::equal(received.begin(),received.end(),std::begin(kState)))
        return kResultFalse;
    return kResultOk; // no bank state promised before P002/P003
}
}
