#include "P001Minimal.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
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
    // VST3 does not legally accept legacy CC-OUT events as MIDI CC
    // INPUT. The opt-in P002 source diagnostic therefore exposes
    // a real automatable parameter consumed sample-accurately from
    // IParameterChanges. In normal P001 mode this parameter is absent.
    const char* diag=std::getenv("AWE64_P002_PRIVATE_NOTE_FIXTURE");
    const char* bank=std::getenv("AWE64_P002_PRIVATE_ROM_BANK_PATH");
    if((diag && *diag) || (bank && *bank))
        parameters.addParameter(STR16("P002 private CC64 sustain"),
            nullptr,127,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateDiagnosticCc64Param));
    if(bank && *bank)
        parameters.addParameter(STR16("P002 ROM SF2 program"),
            nullptr,127,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateProgramParam));
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
        p002Started_.fill(false);
        p002KeyDown_.fill(false);
        p002BankEnabled_=false;
        p002RomBank_.clear(); // non-RT activation thread only
        p002DiagnosticEnabled_=false;
        preCardPcm16DiagnosticTap_=false;
        originalGoldCard_.reset(); // exact original CT1745 reset on the non-RT thread
        chip_.close(); // non-RT activation thread only
        return kResultOk;
    }
    if (active_) return kResultOk;
    // The original 86Box AWE64 Gold CT1745 mixer starts at the source
    // reset registers (FM/master 0xF8, bass/treble 0x80). Reverb/chorus,
    // INIT EQ and calibrated -1.1dB are ALREADY in the EMU8000 chip
    // and are deliberately never duplicated.
    originalGoldCard_.reset();
    // Preserve the unchanged pre-card AWE32_WAV/PCM16 reference path
    // only when an independent private native regression host requests
    // it. Ordinary instrument output now follows the original AWE64
    // Gold card mixer and fork FLOAT wavetable sink.
    const char* rawTap=std::getenv("AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC");
    preCardPcm16DiagnosticTap_=(rawTap && rawTap[0]=='1' && rawTap[1]=='\0');
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
    // P002 private ORIGINAL x86 one-note diagnostic opt-in. Never
    // changes approved P001 normal playback or accepts a bankless fake
    // preset when the private fixture is missing/corrupt.
    p002DiagnosticEnabled_=false;
    p002BankEnabled_=false;
    const char* bankPath=std::getenv("AWE64_P002_PRIVATE_ROM_BANK_PATH");
    const char* sourcePath=std::getenv("AWE64_P002_PRIVATE_VXD_RESOURCES_PATH");
    const char* diagnosticPath=std::getenv("AWE64_P002_PRIVATE_NOTE_FIXTURE");
    if((bankPath && *bankPath) != (sourcePath && *sourcePath) ||
       ((bankPath && *bankPath) && (diagnosticPath && *diagnosticPath))) {
        std::fputs("125A P002 private source/ROM bank mode incomplete or conflicting\n",
                   stderr);
        chip_.close();return kResultFalse;
    }
    if(bankPath && *bankPath && sourcePath && *sourcePath) {
        std::string why;
        if(!p002RomBank_.load(std::filesystem::path(sourcePath),
                             std::filesystem::path(bankPath),why)) {
            std::fprintf(stderr,"125A P002 source ROM bank rejected: %s\n",
                         why.c_str());
            chip_.close();return kResultFalse;
        }
        chip_.writePort(0xe22u,(1u<<5u)|31u);
        chip_.writePort(0xa20u,0x0004u);
        p002BankEnabled_=true;
    }
    if(diagnosticPath && *diagnosticPath) {
        std::string why;
        if(!loadP002PrivateNoteFixture(std::filesystem::path(diagnosticPath),p002Fixture_,why)) {
            std::fprintf(stderr,"125A P002 source diagnostic rejected: %s\\n",why.c_str());
            chip_.close();return kResultFalse;
        }
        chip_.writePort(0xe22u,(1u<<5u)|31u); // original HWCF3 unmute
        chip_.writePort(0xa20u,0x0004u);
        p002DiagnosticEnabled_=true;
    }
    active_=true; playingKey_=-1;
    p002Started_.fill(false);
    p002KeyDown_.fill(false);
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
void Minimal::noteOn(int channel,int pitch,float velocity) noexcept {
    if (!active_ || pitch<0 || pitch>127) return;
    if (!(velocity>0.f)) {noteOff(channel,pitch);return;}
    if(p002BankEnabled_) {
        if(channel<0 || channel>15)return;
        const int encoded=static_cast<int>(velocity*127.f+0.5f);
        if(encoded<=0 || encoded>127)return;
        (void)p002RomBank_.noteOn(chip_,static_cast<std::uint8_t>(channel),
                                 static_cast<std::uint8_t>(pitch),
                                 static_cast<std::uint8_t>(encoded));
        return;
    }
    if(p002DiagnosticEnabled_) {
        // Select a unique, activation-verified original source NoteOn
        // by MIDI key + velocity, NOT by a guessed soundbank program,
        // synthetic envelope, physical voice "free list" or bank index.
        // The key-to-source table was preflighted before activation;
        // the audio callback only performs bounded static table scans.
        const auto incoming=static_cast<int>(velocity*127.f+0.5f);
        if(channel!=p002Fixture_.channel)return;
        for(std::size_t group=0;group<p002Fixture_.noteGroupCount();++group) {
            if(p002Started_[group] ||
               pitch!=p002Fixture_.groupKey(group) ||
               incoming!=p002Fixture_.groupVelocity(group))continue;
            const auto notes=p002Fixture_.groupVoices(group);
            if(!awe64::creative::candidateOriginalSf2StoreNoteOnSourceReleaseState(
                   p002Fixture_.original_device_state,true,notes) ||
               !awe64::creative::candidateOriginalSf2WriteInitialIpToVxdState(
                   p002Fixture_.original_device_state,true,notes))
                return;
            if(!awe64::creative::applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
                   chip_,notes,p002Fixture_.sourceState()))return;
            p002Started_[group]=true;
            p002KeyDown_[group]=true;
            return;
        }
        return; // No unauthorized source voice, no invisible retrigger.
    }
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
void Minimal::noteOff(int channel,int pitch) noexcept {
    if(!active_)return;
    if(p002BankEnabled_) {
        if(channel>=0 && channel<16 && pitch>=0 && pitch<=127)
            (void)p002RomBank_.noteOff(
                chip_,static_cast<std::uint8_t>(channel),
                static_cast<std::uint8_t>(pitch));
        return;
    }
    if(p002DiagnosticEnabled_) {
        if(channel!=p002Fixture_.channel || pitch<0 || pitch>127)return;
        for(std::size_t group=0;group<p002Fixture_.noteGroupCount();++group) {
            if(!p002Started_[group] || !p002KeyDown_[group] ||
               pitch!=p002Fixture_.groupKey(group))continue;
            // Original VXD physical marker and generator30/38 release
            // byte preflight. Pedal-down keeps this voice sounding
            // until the same source release is performed by CC64-up.
            if(p002Fixture_.verified_source_note_release &&
               !awe64::creative::applyOriginalSf2MidiNoteOffFromVxdState(
                   chip_,p002Fixture_.original_device_state,
                   p002Fixture_.groupVoices(group),
                   static_cast<std::uint8_t>(channel),
                   static_cast<std::uint8_t>(pitch),true))
                return;
            p002KeyDown_[group]=false;
            return;
        }
        return;
    }
    if(playingKey_==pitch) {
        // Normal P001 fixed ROM test voice stays byte-for-byte the
        // original P001 diagnostic path; production SF2 is not claimed.
        write(5,kVoice,0xA20,0x807Fu);
        playingKey_=-1;
    }
}
bool Minimal::privateSustainCc64(const std::uint8_t value) noexcept {
    if(p002BankEnabled_)return p002RomBank_.sustain(chip_,value);
    if(!p002DiagnosticEnabled_ ||
       !p002Fixture_.verified_source_note_release || value>127u)
        return false;
    // Real original Win95 SBAWE.VXD LCOD+0x656D scans all 30
    // physical channel:FF deferred rows. Assemble every actual
    // NoteOn's source-backed physical record on the stack; exclude
    // unplayed groups to avoid fictional active voice provenance.
    std::array<awe64::creative::OriginalSf2VxdSnapshotVoice,
               awe64::creative::kOriginalCreativeMusicVoices> assigned{};
    std::size_t count=0;
    for(std::size_t group=0;group<p002Fixture_.noteGroupCount();++group) {
        if(!p002Started_[group])continue;
        for(const auto& voice:p002Fixture_.groupVoices(group)) {
            if(count>=assigned.size())return false;
            assigned[count++]=voice;
        }
    }
    return awe64::creative::applyOriginalSf2MidiCc64FromVxdState(
        chip_,p002Fixture_.original_device_state,
        std::span<const awe64::creative::OriginalSf2VxdSnapshotVoice>(
            assigned.data(),count),p002Fixture_.channel,value,true)
        .has_value();
}
void Minimal::render(int32 begin,int32 end,float* left,float* right) noexcept {
    while(begin<end) {
        const auto count=std::min<int32>(980,end-begin);
        chip_.render(scratch_.data(),static_cast<size_t>(count));
        for(int32 i=0;i<count;++i) {
            const auto preL=scratch_[2u*size_t(i)];
            const auto preR=scratch_[2u*size_t(i)+1];
            if(preCardPcm16DiagnosticTap_) {
                // Historical pre-CT1745 chip PCM diagnostic; remains
                // byte-identical to all previous P001/P002 oracle tests.
                left[begin+i]=digitalPcm16(preL);
                right[begin+i]=digitalPcm16(preR);
            }else {
                // Independent ORIGINAL SB AWE64 Gold downstream stage.
                // Do not clip to PCM16: 86Box sound.c uses FLOAT /32768.
                const auto card=originalGoldCard_.process(preL,preR);
                left[begin+i]=
                    awe64::card::OriginalCt1745WavetableMixer::floatSink(
                        card.left);
                right[begin+i]=
                    awe64::card::OriginalCt1745WavetableMixer::floatSink(
                        card.right);
            }
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
    // Merge MIDI note events with the genuine VST3 IParameterChanges
    // CC64 stream in original sample-offset order. P001 receives exactly
    // the previous no-parameter path, while P002 private diagnostic can
    // reproduce source LCOD+0x656D pedal down/up sample accurately.
    IParamValueQueue* sustainQueue=nullptr;
    IParamValueQueue* programQueue=nullptr;
    if(((p002DiagnosticEnabled_ &&
         p002Fixture_.verified_source_note_release) ||
        p002BankEnabled_) && data.inputParameterChanges) {
        const auto count=data.inputParameterChanges->getParameterCount();
        for(int32 q=0;q<count;++q) {
            auto* queue=data.inputParameterChanges->getParameterData(q);
            if(queue && queue->getParameterId()==
                             kP002PrivateDiagnosticCc64Param) {
                if(sustainQueue)return kInvalidArgument; // ambiguous MIDI state
                sustainQueue=queue;
            }else if(queue && p002BankEnabled_ &&
                      queue->getParameterId()==kP002PrivateProgramParam) {
                if(programQueue)return kInvalidArgument;
                programQueue=queue;
            }
        }
    }
    const int32 ccCount=sustainQueue?sustainQueue->getPointCount():0;
    const int32 programCount=programQueue?programQueue->getPointCount():0;
    if(ccCount<0 || programCount<0)return kInvalidArgument;
    int32 ccIndex=0;
    int32 programIndex=0;
    int32 midiIndex=0;
    const int32 events=data.inputEvents?data.inputEvents->getEventCount():0;
    if(events<0)return kInvalidArgument;
    int32 pos=0;
    while(midiIndex<events || ccIndex<ccCount ||
          programIndex<programCount) {
        Event e{};
        bool haveMidi=midiIndex<events;
        if(haveMidi && data.inputEvents->getEvent(midiIndex,e)!=kResultOk) {
            ++midiIndex;
            continue;
        }
        int32 midiOffset=std::numeric_limits<int32>::max();
        if(haveMidi)midiOffset=std::clamp(e.sampleOffset,int32(0),data.numSamples);
        int32 ccOffset=std::numeric_limits<int32>::max();
        ParamValue ccNormalized=0.;
        const bool haveCc=ccIndex<ccCount;
        if(haveCc) {
            if(sustainQueue->getPoint(ccIndex,ccOffset,ccNormalized)!=kResultOk ||
               ccOffset<0 || ccOffset>data.numSamples ||
               !std::isfinite(ccNormalized) ||
               ccNormalized<0. || ccNormalized>1.)
                return kInvalidArgument;
        }
        int32 programOffset=std::numeric_limits<int32>::max();
        ParamValue programNormalized=0.;
        const bool haveProgram=programIndex<programCount;
        if(haveProgram) {
            if(programQueue->getPoint(programIndex,programOffset,
                                     programNormalized)!=kResultOk ||
               programOffset<0 || programOffset>data.numSamples ||
               !std::isfinite(programNormalized) ||
               programNormalized<0. || programNormalized>1.)
                return kInvalidArgument;
        }
        // A simultaneous program change must affect the NEXT NoteOn,
        // but never rewrite active physical voices. Ties are a strict
        // deterministic private diagnostic scheduling rule, not a
        // claim about untraced original WinMM ordering. CC64 precedes
        // NoteOff at the same timestamp, as verified in the prior path.
        const bool chooseProgram=haveProgram &&
            (!haveMidi || programOffset<=midiOffset) &&
            (!haveCc || programOffset<=ccOffset);
        const bool chooseCc=!chooseProgram && haveCc &&
            (!haveMidi || ccOffset<=midiOffset);
        const int32 offset=chooseProgram?programOffset:
            (chooseCc?ccOffset:midiOffset);
        if(offset<pos)return kInvalidArgument;
        render(pos,offset,left,right);
        pos=offset;
        if(chooseProgram) {
            const auto value=static_cast<std::uint8_t>(
                programNormalized*127. + 0.5);
            if(!p002RomBank_.programChange(value))return kInvalidArgument;
            ++programIndex;
            continue;
        }
        if(chooseCc) {
            const auto value=static_cast<std::uint8_t>(
                ccNormalized*127. + 0.5);
            if(!privateSustainCc64(value))return kInvalidArgument;
            ++ccIndex;
            continue;
        }
        if(e.type==Event::kNoteOnEvent &&
           (p002DiagnosticEnabled_ || p002BankEnabled_ ||
            e.noteOn.channel==0))
            noteOn(e.noteOn.channel,e.noteOn.pitch,e.noteOn.velocity);
        else if(e.type==Event::kNoteOffEvent &&
                (p002DiagnosticEnabled_ || p002BankEnabled_ ||
                 e.noteOff.channel==0))
            noteOff(e.noteOff.channel,e.noteOff.pitch);
        ++midiIndex;
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
