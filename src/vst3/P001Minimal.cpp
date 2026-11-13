#include "P001Minimal.h"
#include "pluginterfaces/base/ibstream.h"
#include "P002PrivateOriginalRamCapture.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
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
// Write-only sample-clock evidence. All I/O at setActive(false), NOT process().
bool savePrivateNativeRegisterOuts(
    const std::filesystem::path& output,
    const std::span<const awe64::ChipPortWrite> events,
    const bool overflow) noexcept {
    if(output.empty() || !output.is_absolute() || overflow || events.empty())
        return false;
    HANDLE h=CreateFileW(output.c_str(),GENERIC_WRITE,0,nullptr,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    constexpr char header[]=
        "# INDEPENDENT_NATIVE_P002_PORT_WRITES_ONLY\n"
        "# CLOCK=HOST_44100_SAMPLE_FRAMES; ORIGIN=ACTIVATION\n"
        "# SOURCE_ORIGINAL_WIN95_PROVENANCE=SEPARATELY_REQUIRED\n";
    DWORD written=0;
    bool ok=WriteFile(h,header,static_cast<DWORD>(sizeof(header)-1u),
                      &written,nullptr)!=0 &&
            written==static_cast<DWORD>(sizeof(header)-1u);
    for(const auto& item:events) {
        if(!ok)break;
        char line[96]{};
        const int n=std::snprintf(line,sizeof(line),"%llu %X %X\n",
            static_cast<unsigned long long>(item.frame),
            static_cast<unsigned>(item.port),
            static_cast<unsigned>(item.value));
        if(n<=0 || n>=static_cast<int>(sizeof(line))) {
            ok=false;break;
        }
        written=0;
        ok=WriteFile(h,line,static_cast<DWORD>(n),&written,nullptr)!=0 &&
           written==static_cast<DWORD>(n);
    }
    if(!CloseHandle(h))ok=false;
    if(!ok)DeleteFileW(output.c_str());
    return ok;
}
// PRIVATE native RAM SoundFont pre-CT1745 source proof. The actual
// unchanged EMU8000 core renders 32-bit interleaved signed samples;
// this emits the already-authorized P001 diagnostic 16-bit saturation.
// Never run the file writer from process(); writes only at deactivation.
bool savePrivateNativeDryWav(
    const std::filesystem::path& destination,
    const std::span<const std::int16_t> samples,
    const bool overflow) noexcept {
    if(destination.empty() || !destination.is_absolute() ||
       overflow || samples.size()<512u || (samples.size()%2u)!=0u ||
       samples.size()>44100u*2u*30u)
        return false;
    const auto bytes=static_cast<std::uint32_t>(
        samples.size()*sizeof(std::int16_t));
    // Standard 44-byte RIFF/WAVE PCM16 stereo 44100 Hz. Windows
    // x64 is little-endian; write explicit byte order for the header.
    std::array<std::uint8_t,44> header{};
    const auto bytes4=[&](const unsigned at,const std::uint32_t v) noexcept {
        for(unsigned n=0;n<4;++n)
            header[at+n]=static_cast<std::uint8_t>(v>>(8u*n));
    };
    const auto bytes2=[&](const unsigned at,const std::uint16_t v) noexcept {
        header[at]=static_cast<std::uint8_t>(v);
        header[at+1u]=static_cast<std::uint8_t>(v>>8u);
    };
    header[0]='R';header[1]='I';header[2]='F';header[3]='F';
    bytes4(4u,36u+bytes);
    header[8]='W';header[9]='A';header[10]='V';header[11]='E';
    header[12]='f';header[13]='m';header[14]='t';header[15]=' ';
    bytes4(16u,16u);bytes2(20u,1u);bytes2(22u,2u);
    bytes4(24u,44100u);bytes4(28u,44100u*4u);
    bytes2(32u,4u);bytes2(34u,16u);
    header[36]='d';header[37]='a';header[38]='t';header[39]='a';
    bytes4(40u,bytes);
    HANDLE h=CreateFileW(destination.c_str(),GENERIC_WRITE,0u,nullptr,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD written=0u;
    bool ok=WriteFile(h,header.data(),static_cast<DWORD>(header.size()),
                      &written,nullptr)!=0 &&
            written==header.size();
    if(ok) {
        written=0u;
        ok=WriteFile(h,samples.data(),bytes,&written,nullptr)!=0 &&
           written==bytes;
    }
    if(!CloseHandle(h))ok=false;
    if(!ok)DeleteFileW(destination.c_str());
    return ok;
}
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
    const char* nativeRam=std::getenv("AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES");
    const bool nativeRamRequested=(nativeRam && nativeRam[0]=='1' &&
                                   nativeRam[1]=='\0');
    if((diag && *diag) || (bank && *bank) || nativeRamRequested)
        parameters.addParameter(STR16("P002 private CC64 sustain"),
            nullptr,127,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateDiagnosticCc64Param));
    if((bank && *bank) || nativeRamRequested)
        parameters.addParameter(STR16("P002 source SF2 program"),
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
        const bool traceSaved=privateNativeRegisterTracePath_.empty() ||
            savePrivateNativeRegisterOuts(
                privateNativeRegisterTracePath_,chip_.portWriteTrace(),
                chip_.portWriteTraceOverflow());
        const bool pcmSaved=privateNativeDryWavPath_.empty() ||
            savePrivateNativeDryWav(
                privateNativeDryWavPath_,
                std::span<const std::int16_t>(
                    privateNativeDryPcm_.data(),privateNativeDryFrames_*2u),
                privateNativeDryOverflow_);
        privateNativeDryWavPath_.clear();
        privateNativeDryPcm_.clear(); // non-RT deactivation only
        privateNativeDryFrames_=0u;
        privateNativeDryOverflow_=false;
        privateNativeRegisterTracePath_.clear();
        chip_.disarmPortWriteTrace();
        processing_=false; active_=false; playingKey_=-1;
        p002Started_.fill(false);
        p002KeyDown_.fill(false);
        p002BankEnabled_=false;
        p002PrivateRamPreloaded_=false;
        p002OriginalRamNotes_.clear();
        p002RomBank_.clear(); // non-RT activation thread only
        p002DiagnosticEnabled_=false;
        preCardPcm16DiagnosticTap_=false;
        originalGoldCard_.reset(); // exact original CT1745 reset on the non-RT thread
        chip_.close(); // non-RT activation thread only
        return traceSaved && pcmSaved ? kResultOk : kResultFalse;
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
    p002PrivateRamPreloaded_=false;
    p002OriginalRamNotes_.clear();
    const char* bankPath=std::getenv("AWE64_P002_PRIVATE_ROM_BANK_PATH");
    const char* sourcePath=std::getenv("AWE64_P002_PRIVATE_VXD_RESOURCES_PATH");
    const char* diagnosticPath=std::getenv("AWE64_P002_PRIVATE_NOTE_FIXTURE");
    // P002 private original SOURCE RAM capture from the actual Win95
    // Creative driver. THREE independently SHA-pinned private inputs
    // are mandatory. Nothing is embedded in or distributed with VST3.
    const char* originalRamSf2=
        std::getenv("AWE64_P002_PRIVATE_ORIGINAL_RAM_SF2");
    const char* originalRamPorts=
        std::getenv("AWE64_P002_PRIVATE_ORIGINAL_RAM_PORTS");
    const char* originalRamFinal=
        std::getenv("AWE64_P002_PRIVATE_ORIGINAL_RAM_FINAL_PCM16");
    // These are actual 1996 original driver NoteOn/NoteOff physical
    // port writes from THE SAME independently verified Win95 RAM bank.
    // Optional: no captured note file means authenticated RAM-only
    // mode remains silent, never substitutes an inaccurate ROM note.
    const char* originalRamNotes=
        std::getenv("AWE64_P002_PRIVATE_ORIGINAL_RAM_NOTE_PORTS");
    const bool sourceRamNoteRequested=
        originalRamNotes && *originalRamNotes;
    const bool anyCapturedRam=
        (originalRamSf2 && *originalRamSf2) ||
        (originalRamPorts && *originalRamPorts) ||
        (originalRamFinal && *originalRamFinal);
    // Default preserved: original ROM bank, source RAM port capture
    // and P001 are mutually exclusive. Native RAM source implementation
    // is an explicit, fail-closed opt-in which REQUIRES the three
    // SHA-pinned original Win95 source RAM inputs + authentic x86 VXD
    // resource/state pack. It does not enable captured note-clip replay.
    const char* nativeRam=std::getenv("AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES");
    const bool nativeRamRequested=(nativeRam && nativeRam[0]=='1' &&
                                   nativeRam[1]=='\0');
    const char* tracePath=std::getenv("AWE64_P002_NATIVE_REGISTER_OUT_PATH");
    const bool traceRequested=tracePath && *tracePath;
    const char* dryWavPath=std::getenv("AWE64_P002_NATIVE_DRY_PCM_WAV_PATH");
    const bool dryWavRequested=dryWavPath && *dryWavPath;
    const bool resourcePresent=sourcePath && *sourcePath;
    const bool bankPresent=bankPath && *bankPath;
    const bool diagnosticPresent=diagnosticPath && *diagnosticPath;
    const bool completeRamOriginal=(originalRamSf2 && *originalRamSf2) &&
        (originalRamPorts && *originalRamPorts) &&
        (originalRamFinal && *originalRamFinal);
    const bool modeConflict=nativeRamRequested
        ? (!completeRamOriginal || !resourcePresent || bankPresent ||
           diagnosticPresent || sourceRamNoteRequested)
        : (bankPresent!=resourcePresent ||
           (bankPresent && diagnosticPresent) ||
           (sourceRamNoteRequested && !anyCapturedRam) ||
           (anyCapturedRam &&
            (!completeRamOriginal || bankPresent || diagnosticPresent)));

    if((nativeRam && *nativeRam && !nativeRamRequested) ||
       ((traceRequested || dryWavRequested) && !nativeRamRequested) ||
       modeConflict) {
        std::fputs("125A P002 private source/ROM bank mode incomplete or conflicting\n",
                   stderr);
        chip_.close();return kResultFalse;
    }
    if(sourceRamNoteRequested) {
        // Validate the *complete* SHA256-pinned original source note
        // fixture BEFORE any original EMU8000 RAM upload takes place.
        // No dynamic allocations/files are permitted in process().
        std::string why;
        if(!p002OriginalRamNotes_.load(
               std::filesystem::path(originalRamNotes),why)) {
            std::fprintf(stderr,
                "125A P002 original Creative RAM NoteOn source rejected: %s\n",
                why.c_str());
            chip_.close();return kResultFalse;
        }
    }
    if(anyCapturedRam) {
        std::string why;
        if(!preloadOriginalCreativeCapturedRamSource(
                chip_,std::filesystem::path(originalRamSf2),
                std::filesystem::path(originalRamPorts),
                std::filesystem::path(originalRamFinal),why)) {
            std::fprintf(stderr,
                "125A P002 ORIGINAL RAM source replay rejected: %s\n",
                why.c_str());
            chip_.close();return kResultFalse;
        }
        p002PrivateRamPreloaded_=true;
        // This explicitly does NOT route NoteOn through the P001 ROM
        // voice or guess the Creative source VXD RAM voice descriptors.
        // Authentic source RAM is physically loaded; actual RAM
        // NoteOn/Off and native PCM comparison remain separate gates.
        std::fputs("P002_REAL_ORIGINAL_CREATIVE_83_RAM_SAMPLES_PRELOADED"
                   " full_source_port_replay=TRUE"
                   " source_final_260921_PCM16_WORDS_EXACT=TRUE"
                   " source_NOTEON_RECORDING_SHA_VERIFIED_OPTIONAL=TRUE"
                   " NATIVE_PCM_PARITY=NOT_PROVEN\n",stderr);
    }
    if(nativeRamRequested) {
        // Actual original Creative 83-sample RAM state has ALREADY
        // been independently authenticated and loaded above. Load
        // the same owner SoundFont's source-original VXD resources
        // for host-MIDI-driven NoteOn/Off. The original chip's RAM
        // and DSP are never altered by this adapter's code.
        std::string why;
        if(!p002PrivateRamPreloaded_ ||
           !p002RomBank_.load(std::filesystem::path(sourcePath),
                              std::filesystem::path(originalRamSf2),
                              why,true)) {
            std::fprintf(stderr,
                "125A P002 original Creative native RAM source rejected: %s\n",
                why.c_str());
            chip_.close();return kResultFalse;
        }
        chip_.writePort(0xe22u,(1u<<5u)|31u);
        chip_.writePort(0xa20u,0x0004u);
        p002BankEnabled_=true;
        // Arm only AFTER the authentic Win95 Creative source RAM upload
        // and the private VXD table/state SHA gates have succeeded.
        if(traceRequested) {
            const std::filesystem::path output(tracePath);
            if(!output.is_absolute() ||
               !chip_.armPortWriteTrace(262144u)) {
                std::fputs("P002 native port trace cannot be armed\n",stderr);
                chip_.close();return kResultFalse;
            }
            privateNativeRegisterTracePath_=output;
        }
        // Capture the actual chip PCM for the same requested private
        // native RAM note test. Storage is allocated *before* any
        // audio processing, and cannot exceed 30 seconds at 44.1kHz.
        if(dryWavRequested) {
            const std::filesystem::path output(dryWavPath);
            if(!output.is_absolute()) {
                std::fputs("P002 native dry PCM requires absolute path\n",
                           stderr);
                chip_.close();return kResultFalse;
            }
            try {
                privateNativeDryPcm_.resize(44100u*2u*30u);
            }catch (...) {
                std::fputs("P002 native dry PCM allocation failed\n",
                           stderr);
                chip_.close();return kResultFalse;
            }
            privateNativeDryFrames_=0u;
            privateNativeDryOverflow_=false;
            privateNativeDryWavPath_=output;
        }
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
    if(p002PrivateRamPreloaded_ && !p002BankEnabled_) {
        // EXACT two original Win95 RAM NoteOn capture groups only;
        // no home-made patch data, filter, ADSR or fallback ROM voice.
        const auto v=static_cast<int>(velocity*127.f+0.5f);
        if(p002OriginalRamNotes_.ready())
            (void)p002OriginalRamNotes_.noteOn(chip_,channel,pitch,v);
        return;
    }
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
    if(p002PrivateRamPreloaded_ && !p002BankEnabled_) {
        if(p002OriginalRamNotes_.ready())
            (void)p002OriginalRamNotes_.noteOff(chip_,channel,pitch);
        return;
    }
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
        if(p002PrivateRamPreloaded_ && p002OriginalRamNotes_.ready())
            p002OriginalRamNotes_.render(
                chip_,scratch_.data(),static_cast<std::size_t>(count));
        else
            chip_.render(scratch_.data(),static_cast<std::size_t>(count));
        for(int32 i=0;i<count;++i) {
            const auto preL=scratch_[2u*size_t(i)];
            const auto preR=scratch_[2u*size_t(i)+1];
            if(!privateNativeDryWavPath_.empty()) {
                if(privateNativeDryFrames_*2u+1u<
                   privateNativeDryPcm_.size()) {
                    const auto at=privateNativeDryFrames_*2u;
                    privateNativeDryPcm_[at]=static_cast<std::int16_t>(
                        std::clamp(preL,int32_t(-32768),int32_t(32767)));
                    privateNativeDryPcm_[at+1u]=static_cast<std::int16_t>(
                        std::clamp(preR,int32_t(-32768),int32_t(32767)));
                    ++privateNativeDryFrames_;
                }else privateNativeDryOverflow_=true;
            }
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
