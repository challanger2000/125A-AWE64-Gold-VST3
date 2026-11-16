#include "P001Minimal.h"
#include "P002OriginalWin95ChipPrelude.h"
#include "P002PrivateDryPcmCapture.h"
#include "P003Sf2HostProgramNames.h"
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
    // This explicitly private diagnostic uses an original source trace
    // for CLOCKS and strict comparison only, NEVER for the native OUT values.
    const char* clockEnv=std::getenv("AWE64_P002_CLOCKED_NATIVE_ORIGINAL_RAM");
    const bool clockedRamRequested=clockEnv && clockEnv[0]=='1' &&
                                   clockEnv[1]=='\0';
    const char* paceEnv=std::getenv("AWE64_P003_AUTONOMOUS_OUT_PACING");
    const bool autonomousPacing=paceEnv && paceEnv[0]=='1' &&
                                paceEnv[1]=='\0';

    const char* privateResetEnv=
        std::getenv("AWE64_P002_PRIVATE_WIN95_SOURCE_RESET");
    const bool privateResetRequested=privateResetEnv &&
        privateResetEnv[0]=='1' && privateResetEnv[1]=='\0';
    if(nativeRamRequested && privateResetRequested) {
        parameters.addParameter(STR16("P002 private Win95 midiOutReset"),
            nullptr,1,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateWin95ResetParam));
    }
    if(!clockedRamRequested && !autonomousPacing &&
       ((diag && *diag) || (bank && *bank) || nativeRamRequested))
        parameters.addParameter(STR16("P002 private CC64 sustain"),
            nullptr,127,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateDiagnosticCc64Param));
    if(!clockedRamRequested && !autonomousPacing &&
       ((bank && *bank) || nativeRamRequested)) {
        parameters.addParameter(STR16("P002 source SF2 program"),
            nullptr,127,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateProgramParam));
        parameters.addParameter(STR16("P003 exact SF2 bank"),
            nullptr,128,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP003ExactSf2BankParam));
        // The SoundFont inventory has an unknown number of REAL PHDR
        // entries until activation. A continuous normalized selector
        // is mapped only over those existing rows; no 128-slot padding.
        parameters.addParameter(STR16("P003 actual SF2 preset row"),
            nullptr,0,0.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP003Sf2PresetOrdinalParam));
        // These are original MIDI controller STATE (not a hand-tuned
        // filter, amp or FX send). Actual original VXD source mapping
        // from CC7/CC11/CC10 happens when the next voice is compiled.
        parameters.addParameter(STR16("P002 source CC7 volume"),
            nullptr,127,100./127.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateCc7VolumeParam));
        parameters.addParameter(STR16("P002 source CC10 pan"),
            nullptr,127,64./127.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateCc10PanParam));
        parameters.addParameter(STR16("P002 source CC11 expression"),
            nullptr,127,1.,ParameterInfo::kCanAutomate,
            static_cast<int32>(kP002PrivateCc11ExpressionParam));
    }
    // The combined-component VST3 implements IUnitInfo already.
    // Expose a single stable root ProgramList; only its real PHDR
    // contents are populated after the source-authorized SF2 loads
    // on non-realtime activation. The list is *empty*, not 128 fake
    // programs, before an authentic SoundFont has been accepted.
    if((bank && *bank) || nativeRamRequested) {
        if(!addUnit(new Unit(STR16("AWE64 Gold SoundFont"),
                             kRootUnitId,kNoParentUnitId,
                             static_cast<ProgramListID>(
                                 sf2host::kProgramListId))) ||
           !addProgramList(new ProgramList(
                             STR16("Original SF2 PHDR Presets"),
                             static_cast<ProgramListID>(
                                 sf2host::kProgramListId),
                             kRootUnitId)))
            return kResultFalse;
    }
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
        // Preserve logical bank/program/CC for the next activation, but
        // never save sounding voices, source timers or VXD heap pointers.
        // A newer, unconsumed host setState always takes precedence.
        if(p002BankEnabled_ &&
           pendingBankState_.load(std::memory_order_acquire)==
               state::kNoSelection) {
            const auto latest=publishedBankState_.load(std::memory_order_acquire);
            if(latest!=state::kNoSelection)
                pendingBankState_.store(latest,std::memory_order_release);
        }
        const bool nativeClockVerified=!clockedNativeRamFailed_;
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
        nativeRamClock_.clear();
        clockedNativeRam_=false;
        autonomousNativeRam_=false;
        clockedNativeRamFailed_=false;
        p002RomBank_.clear(); // non-RT activation thread only
        if(auto* list=getProgramList(static_cast<ProgramListID>(
                             sf2host::kProgramListId))) {
            list->clearPrograms(); // never from process(), no fake rows
            (void)notifyProgramListChange(
                static_cast<ProgramListID>(sf2host::kProgramListId));
        }
        p002DiagnosticEnabled_=false;
        preCardPcm16DiagnosticTap_=false;
        originalGoldCard_.reset(); // exact original CT1745 reset on the non-RT thread
        chip_.close(); // non-RT activation thread only
        return traceSaved && pcmSaved && nativeClockVerified
            ? kResultOk : kResultFalse;
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
    nativeRamClock_.clear();
    clockedNativeRam_=false;
    autonomousNativeRam_=false;
    clockedNativeRamFailed_=false;
    privateWin95SourceReset_=false;
    privateWin95SourceResetDone_=false;
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
    // Exact SHA-pinned original Win95 physical chip-prelude experiment.
    // ONLY the initialization and original RAM transfer are source-driven;
    // the actual C4/D4 note register compiler remains independent MIDI.
    const char* originalPreludeTrace=
        std::getenv("AWE64_P002_PRIVATE_ORIGINAL_CHIP_PRELUDE_TRACE");
    const bool originalPreludeRequested=
        originalPreludeTrace && *originalPreludeTrace;
    const char* originalReset=
        std::getenv("AWE64_P002_PRIVATE_WIN95_SOURCE_RESET");
    const bool sourceResetRequested=originalReset &&
        originalReset[0]=='1' && originalReset[1]=='\0';
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
    // Clocked native RAM captures require authentic private Creative
    // register frame provenance; do not permit unverified timing.
    const char* clockEnv=std::getenv("AWE64_P002_CLOCKED_NATIVE_ORIGINAL_RAM");
    const bool clockedRamRequested=clockEnv && clockEnv[0]=='1' &&
                                   clockEnv[1]=='\0';
    const char* paceEnv=std::getenv("AWE64_P003_AUTONOMOUS_OUT_PACING");
    const bool autonomousPacing=paceEnv && paceEnv[0]=='1' &&
                                paceEnv[1]=='\0';
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
           diagnosticPresent ||
           sourceRamNoteRequested!=clockedRamRequested)
        : (bankPresent!=resourcePresent ||
           (bankPresent && diagnosticPresent) ||
           (sourceRamNoteRequested && !anyCapturedRam) ||
           (anyCapturedRam &&
            (!completeRamOriginal || bankPresent || diagnosticPresent)));

    if((nativeRam && *nativeRam && !nativeRamRequested) ||
       (clockEnv && *clockEnv && !clockedRamRequested) ||
       (paceEnv && *paceEnv && !autonomousPacing) ||
       (autonomousPacing &&
        (!nativeRamRequested || clockedRamRequested ||
         sourceRamNoteRequested)) ||
       (clockedRamRequested && !nativeRamRequested) ||
       ((traceRequested || dryWavRequested) && !nativeRamRequested) ||
       modeConflict || (originalPreludeRequested && !nativeRamRequested) ||
       (originalReset && *originalReset && !sourceResetRequested) ||
       (sourceResetRequested && (!originalPreludeRequested ||
                                 !nativeRamRequested))) {
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
    awe64::creative::P003RamUploadResult p003CapturedOriginalRam{};
    if(anyCapturedRam) {
        std::string why;
        if(!preloadOriginalCreativeCapturedRamSource(
                chip_,std::filesystem::path(originalRamSf2),
                std::filesystem::path(originalRamPorts),
                std::filesystem::path(originalRamFinal),why,
                &p003CapturedOriginalRam)) {
            std::fprintf(stderr,
                "125A P002 ORIGINAL RAM source replay rejected: %s\n",
                why.c_str());
            chip_.close();return kResultFalse;
        }
        p002PrivateRamPreloaded_=true;
        if(originalPreludeRequested) {
            // First original RAM attestation has already passed. Recreate
            // the ORIGINAL clean 86Box chip, then execute its complete
            // authenticated Win95 pre-note history with physical time.
            // No note-clip or source values are used for native MIDI notes.
            chip_.close();
            std::string reopen;
            if(!chip_.open(std::filesystem::path(path),reopen)) {
                std::fprintf(stderr,"P002 original chip reinitialize failed: %s\n",
                             reopen.c_str());return kResultFalse;
            }
            OriginalChipPreludeResult result;
            std::string preludeError;
            if(!replayPrivateOriginalWin95ChipPrelude(
                   chip_,std::filesystem::path(originalPreludeTrace),
                   std::filesystem::path(originalRamFinal),137u,
                   result,preludeError)) {
                std::fprintf(stderr,"P002 original chip prelude refused: %s\n",
                             preludeError.c_str());
                chip_.close();return kResultFalse;
            }
            // P003 must attest the FINAL original chip after the
            // optional prelude reopened it. The earlier P003 upload
            // belonged to the discarded pre-prelude device instance.
            if(!p003ReattestOwnerRamAfterOriginalPrelude(
                   chip_,std::filesystem::path(originalRamSf2),
                   std::filesystem::path(originalRamFinal),preludeError)) {
                std::fprintf(stderr,
                    "P003 final original prelude RAM rejected: %s\n",
                    preludeError.c_str());
                chip_.close();return kResultFalse;
            }
            std::fprintf(stderr,
                "P002_WIN95_ORIGINAL_PRELUDE_REPLAYED "
                "writes=%u reads=%u INIT=%u HWCF=%u "
                "read_differences=%u source_clock_at_native_activation=%llu "
                "first_read_frame=%u first_read_port=0x%04x "
                "first_read_selector=0x%04x original_value=0x%04x "
                "native_value=0x%04x "
                "real_original_vs_native_audio=NOT_YET_COMPARED\n",
                result.physical_writes,result.physical_reads,
                result.source_init_writes,
                result.source_hardware_config_writes,
                result.read_disagreements,
                static_cast<unsigned long long>(
                    result.source_chip_clock_at_activation),
                result.first_read_disagreement_frame,
                static_cast<unsigned>(result.first_read_disagreement_port),
                static_cast<unsigned>(result.first_read_disagreement_selector),
                static_cast<unsigned>(result.first_read_disagreement_source),
                static_cast<unsigned>(result.first_read_disagreement_native));
        }
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
                              why,true,&p003CapturedOriginalRam)) {
            std::fprintf(stderr,
                "125A P002 original Creative native RAM source rejected: %s\n",
                why.c_str());
            chip_.close();return kResultFalse;
        }
        if(!originalPreludeRequested) {
            chip_.writePort(0xe22u,(1u<<5u)|31u);
            chip_.writePort(0xa20u,0x0004u);
        }
        p002BankEnabled_=true;
        privateWin95SourceReset_=sourceResetRequested;
        if(clockedRamRequested || autonomousPacing) {
            // Snapshot reservation is on setActive, never in process().
            if((clockedRamRequested && !p002OriginalRamNotes_.ready()) ||
               !chip_.armNativeNoteOutTransaction()) {
                std::fputs("P002 native original clock capture cannot be armed\n",stderr);
                chip_.close();return kResultFalse;
            }
            clockedNativeRam_=true;
            autonomousNativeRam_=autonomousPacing;
        }
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
    // The VST3 ProgramList is a host-facing *copy* of the exact
    // source-validated SF2 PHDR inventory. No reads from this mutable
    // vector are performed by the host on the sample callback.
    // Bxxx/Pxxx prefix preserves real bank/program identity even when
    // different PHDRs have the same original 20-byte name.
    if(p002BankEnabled_) {
        if(auto* list=getProgramList(static_cast<ProgramListID>(
                             sf2host::kProgramListId))) {
            list->clearPrograms();
            for(std::size_t i=0;i<p002RomBank_.presetCount();++i) {
                const auto* preset=p002RomBank_.presetAt(i);
                if(!preset) {
                    chip_.close();return kResultFalse;
                }
                const auto label=sf2host::labelFor(*preset);
                String128 name{};
                for(std::size_t j=0;j<label.size();++j)
                    name[j]=static_cast<TChar>(label[j]);
                list->addProgram(name); // activation thread only
            }
            (void)notifyProgramListChange(
                static_cast<ProgramListID>(sf2host::kProgramListId));
        }
        applyPendingBankState();
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
bool Minimal::stagePrivateClockedNote(
    const bool release,const int channel,const int pitch,
    const int velocity) noexcept {
    // This research-only path is strictly limited to the TWO original
    // SHA-pinned owner captures. Normal VST3 MIDI/SF2 notes remain
    // fully independent and are not spuriously limited to C4/D4.
    if(!clockedNativeRam_ || clockedNativeRamFailed_ ||
       (!autonomousNativeRam_ && !p002OriginalRamNotes_.ready()) ||
       channel<0 || channel>15 || pitch<0 || pitch>127 ||
       (!release && (velocity<=0 || velocity>127)) ||
       (!autonomousNativeRam_ &&
        (channel!=0 || (pitch!=60 && pitch!=62) ||
         (!release && velocity!=100))) ||
       nativeRamClock_.pending()!=0 || nativeRamClock_.missedDeadline()) {
        clockedNativeRamFailed_=true;
        return false;
    }
    nativeRamClock_.clear();
    // Two independent mutable states must be transactional: both the
    // original EMU8000 chip and the original Creative VXD/voice ledger.
    // An OUT mismatch previously restored the chip but kept the bank's
    // voices/key-down markers changed, contaminating the next event.
    if(!p002RomBank_.beginNativeNoteCheckpoint()) {
        clockedNativeRamFailed_=true;
        return false;
    }
    if(!chip_.beginNativeNoteOutTransaction()) {
        p002RomBank_.rollbackNativeNoteCheckpoint();
        clockedNativeRamFailed_=true;
        return false;
    }
    // Run the ACTUAL source-derived 1996 VXD MIDI->SF2 descriptor->
    // voice-allocator->chip-register compiler against REAL chip reads.
    // The speculative chip register writes are rolled back *before*
    // audio resumes. This NEVER copies the original trace's values.
    const bool nativeOk=release
        ? p002RomBank_.noteOff(chip_,static_cast<std::uint8_t>(channel),
                              static_cast<std::uint8_t>(pitch))
        : p002RomBank_.noteOn(chip_,static_cast<std::uint8_t>(channel),
                             static_cast<std::uint8_t>(pitch),
                             static_cast<std::uint8_t>(velocity));
    const bool restored=chip_.endNativeNoteOutTransaction();
    if(!nativeOk || !restored) {
        p002RomBank_.rollbackNativeNoteCheckpoint();
        clockedNativeRamFailed_=true;
        return false;
    }
    std::array<awe64::creative::SourceClockedPort,
               awe64::creative::SourceClockedPortPlayer::kMaxEvents> native{};
    std::size_t count=0u;
    // The autonomous model uses only independently generated native
    // MIDI-to-register words; original port-clock clips are never read.
    // The exact original-source comparison mode remains unchanged.
    const bool planOk=autonomousNativeRam_
        ? awe64::creative::p003CandidateAutonomousNoteTimeline(
              chip_.nativeNoteOutTransaction(),release,native,count)
        : p002OriginalRamNotes_.verifiedNativeClockPlan(
              release,channel,pitch,velocity,
              chip_.nativeNoteOutTransaction(),native,count);
    if(!planOk || !nativeRamClock_.stage(
           chip_,std::span<const awe64::creative::SourceClockedPort>(
               native.data(),count))) {
        // The original chip state has already been restored. Restore
        // the original Creative VXD and ownership ledger as well:
        // otherwise subsequent NoteOff/voice scans see ghost voices.
        p002RomBank_.rollbackNativeNoteCheckpoint();
        clockedNativeRamFailed_=true;
        return false;
    }
    p002RomBank_.commitNativeNoteCheckpoint();
    return true;
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
        if(clockedNativeRam_) {
            (void)stagePrivateClockedNote(false,channel,pitch,encoded);
            return;
        }
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
        if(clockedNativeRam_) {
            (void)stagePrivateClockedNote(true,channel,pitch,0);
            return;
        }
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
bool Minimal::privateWin95MidiOutReset() noexcept {
    // Original Win95 Creative midiOutReset releases all 32 physical
    // generators via DCYSUSV=0x807F. The SHA-attested source trace
    // proves 23 generator releases at frame 1845375, 9 at 1845376.
    // Their native time is fixed relative to the already pinned C4 frame.
    // This private diagnostic never substitutes source C4/D4 note values.
    constexpr std::uint64_t kNativeResetFrame=87475u;
    if(!privateWin95SourceReset_ || privateWin95SourceResetDone_ ||
       !p002BankEnabled_ || !chip_.ready() ||
       chip_.renderedFrames()!=kNativeResetFrame ||
       nativeRamClock_.pending()!=0u || nativeRamClock_.missedDeadline())
        return false;
    std::array<awe64::creative::SourceClockedPort,64u> events{};
    for(std::uint16_t voice=0u;voice<32u;++voice) {
        const std::uint32_t offset=voice<23u?0u:1u;
        events[2u*voice]={offset,0xe22u,
                           static_cast<std::uint16_t>((5u<<5u)|voice)};
        events[2u*voice+1u]={offset,0xa20u,0x807fu};
    }
    nativeRamClock_.clear();
    if(!nativeRamClock_.stage(chip_,events))return false;
    privateWin95SourceResetDone_=true;
    return true;
}
void Minimal::publishBankState() noexcept {
    if(!p002BankEnabled_ || !p002RomBank_.ready())return;
    const state::BankSelection value{
        p002RomBank_.currentBank(),
        p002RomBank_.currentProgram(),
        p002RomBank_.currentCc7(),
        p002RomBank_.currentCc10(),
        p002RomBank_.currentCc11()};
    if(state::valid(value))
        publishedBankState_.store(state::pack(value),
                                  std::memory_order_release);
}
void Minimal::applyPendingBankState() noexcept {
    if(!p002BankEnabled_ || !p002RomBank_.ready())return;
    const auto pending=pendingBankState_.exchange(
        state::kNoSelection,std::memory_order_acq_rel);
    if(const auto selection=state::unpack(pending)) {
        // This only modifies future logical NoteOns (no chip writes).
        // Exact SF2 PHDR validation happens atomically before mutation.
        (void)p002RomBank_.restoreSelection(
            selection->bank,selection->program,
            selection->cc7,selection->cc10,selection->cc11);
    }
    publishBankState();
}
bool Minimal::privateSustainCc64(const std::uint8_t value) noexcept {
    // This exact owner capture contains four MIDI note events only;
    // any additional controller would change register provenance.
    if(clockedNativeRam_) {
        clockedNativeRamFailed_=true;
        return false;
    }
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
        if(clockedNativeRam_ || privateWin95SourceReset_) {
            nativeRamClock_.render(
                chip_,scratch_.data(),static_cast<std::size_t>(count));
            if(nativeRamClock_.missedDeadline())
                clockedNativeRamFailed_=true;
        }else if(p002PrivateRamPreloaded_ && p002OriginalRamNotes_.ready())
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
    // Hosts are allowed to flush MIDI/parameters at numSamples==0
    // WITHOUT any audio buses or channel buffers. Process the changes
    // at sample offset zero, but never render or advance chip time.
    // A prior early return silently LOST valid Program/Bank/CC changes.
    const bool noAudioFrames=data.numSamples==0;
    float dummyLeft=0.f,dummyRight=0.f;
    float* left=&dummyLeft;
    float* right=&dummyRight;
    if(!noAudioFrames) {
        if(data.numOutputs!=1 || !data.outputs ||
           data.outputs[0].numChannels!=2 ||
           !data.outputs[0].channelBuffers32)
            return kInvalidArgument;
        left=data.outputs[0].channelBuffers32[0];
        right=data.outputs[0].channelBuffers32[1];
        if(!left || !right)return kInvalidArgument;
    } else if(data.numOutputs!=0) {
        // A zero-frame flush may include an output bus, but it must
        // have the already-negotiated stereo layout if present.
        if(data.numOutputs!=1 || !data.outputs ||
           data.outputs[0].numChannels!=2)return kInvalidArgument;
    }
    if (!active_ || !processing_ || !chip_.ready()) {
        std::fill_n(left,data.numSamples,0.f);
        std::fill_n(right,data.numSamples,0.f);
        if(data.outputs && data.numOutputs==1)
            data.outputs[0].silenceFlags=0x3;
        return kResultFalse; // no silent fake synth when ROM or activation is invalid
    }
    // A host can restore project state after setActive(). Validate
    // the ENTIRE incoming block before consuming that pending state.
    // Rejecting a malformed host block must not partially apply either
    // its automation OR a pending project recall.
    // Merge MIDI note events with the genuine VST3 IParameterChanges
    // CC64 stream in original sample-offset order. P001 receives exactly
    // the previous no-parameter path, while P002 private diagnostic can
    // reproduce source LCOD+0x656D pedal down/up sample accurately.
    IParamValueQueue* sustainQueue=nullptr;
    IParamValueQueue* win95ResetQueue=nullptr;
    IParamValueQueue* programQueue=nullptr;
    IParamValueQueue* bankQueue=nullptr;
    IParamValueQueue* presetQueue=nullptr;
    constexpr std::array<ParamID,3> controllerIds{
        kP002PrivateCc7VolumeParam,
        kP002PrivateCc10PanParam,
        kP002PrivateCc11ExpressionParam};
    constexpr std::array<std::uint8_t,3> controllerNumbers{7u,10u,11u};
    std::array<IParamValueQueue*,3> controllerQueues{};
    if(((p002DiagnosticEnabled_ &&
         p002Fixture_.verified_source_note_release) ||
        p002BankEnabled_) && data.inputParameterChanges) {
        const auto count=data.inputParameterChanges->getParameterCount();
        for(int32 q=0;q<count;++q) {
            auto* queue=data.inputParameterChanges->getParameterData(q);
            if(queue && privateWin95SourceReset_ &&
               queue->getParameterId()==kP002PrivateWin95ResetParam) {
                if(win95ResetQueue)return kInvalidArgument;
                win95ResetQueue=queue;
            }else if(queue && queue->getParameterId()==
                             kP002PrivateDiagnosticCc64Param) {
                if(sustainQueue)return kInvalidArgument; // ambiguous MIDI state
                sustainQueue=queue;
            }else if(queue && p002BankEnabled_ &&
                      queue->getParameterId()==kP002PrivateProgramParam) {
                if(programQueue)return kInvalidArgument;
                programQueue=queue;
            }else if(queue && p002BankEnabled_ &&
                      queue->getParameterId()==kP003ExactSf2BankParam) {
                if(bankQueue)return kInvalidArgument;
                bankQueue=queue;
            }else if(queue && p002BankEnabled_ &&
                      queue->getParameterId()==kP003Sf2PresetOrdinalParam) {
                if(presetQueue)return kInvalidArgument;
                presetQueue=queue;
            }else if(queue && p002BankEnabled_) {
                for(std::size_t c=0;c<controllerIds.size();++c) {
                    if(queue->getParameterId()!=controllerIds[c])continue;
                    if(controllerQueues[c])return kInvalidArgument;
                    controllerQueues[c]=queue;
                    break;
                }
            }
        }
    }
    const int32 resetCount=win95ResetQueue?win95ResetQueue->getPointCount():0;
    if(resetCount<0 || resetCount>1)return kInvalidArgument;
    const int32 ccCount=sustainQueue?sustainQueue->getPointCount():0;
    const int32 programCount=programQueue?programQueue->getPointCount():0;
    const int32 bankCount=bankQueue?bankQueue->getPointCount():0;
    const int32 presetCount=presetQueue?presetQueue->getPointCount():0;
    if(ccCount<0 || programCount<0 || bankCount<0 || presetCount<0)
        return kInvalidArgument;
    std::array<int32,3> controllerCount{};
    std::array<int32,3> controllerIndex{};
    for(std::size_t c=0;c<controllerQueues.size();++c) {
        if(controllerQueues[c]) {
            controllerCount[c]=controllerQueues[c]->getPointCount();
            if(controllerCount[c]<0)return kInvalidArgument;
        }
    }
    int32 ccIndex=0;
    int32 resetIndex=0;
    int32 programIndex=0;
    int32 bankIndex=0;
    int32 presetIndex=0;
    int32 midiIndex=0;
    const int32 events=data.inputEvents?data.inputEvents->getEventCount():0;
    if(events<0)return kInvalidArgument;
    // Reject an invalid LATE event/point BEFORE rendering any audio or
    // changing VXD MIDI state. Previously a malformed second point could
    // fail after the first point had already altered a live SoundFont,
    // leaving a partially applied DAW buffer. Fixed-size local scan:
    // no allocation, file I/O, lock or original EMU8000 modification.
    const auto validQueue=[&](IParamValueQueue* queue,
                               const bool resetOnly) noexcept -> bool {
        if(!queue)return true;
        const int32 count=queue->getPointCount();
        if(count<0)return false;
        int32 previous=-1;
        for(int32 i=0;i<count;++i) {
            int32 offset=0;
            ParamValue value=0.;
            if(queue->getPoint(i,offset,value)!=kResultOk ||
               offset<previous || offset<0 || offset>data.numSamples ||
               !std::isfinite(value) ||
               (resetOnly?value!=1.:(value<0. || value>1.)))
                return false;
            previous=offset;
        }
        return true;
    };
    if(!validQueue(sustainQueue,false) ||
       !validQueue(win95ResetQueue,true) ||
       !validQueue(programQueue,false) ||
       !validQueue(bankQueue,false) ||
       !validQueue(presetQueue,false))
        return kInvalidArgument;
    for(auto* queue:controllerQueues)
        if(!validQueue(queue,false))return kInvalidArgument;
    int32 previousMidi=-1;
    for(int32 i=0;i<events;++i) {
        Event incoming{};
        if(data.inputEvents->getEvent(i,incoming)!=kResultOk ||
           incoming.sampleOffset<previousMidi ||
           incoming.sampleOffset<0 ||
           incoming.sampleOffset>data.numSamples)
            return kInvalidArgument;
        previousMidi=incoming.sampleOffset;
    }
    // The last potentially malformed timestamp is now behind us:
    // apply an already validated DAW project recall at sample zero,
    // before any new legitimate MIDI/parameter in this block.
    if(p002BankEnabled_)applyPendingBankState();
    int32 pos=0;
    while(midiIndex<events || ccIndex<ccCount ||
          resetIndex<resetCount ||
          programIndex<programCount ||
          bankIndex<bankCount ||
          presetIndex<presetCount ||
          controllerIndex[0]<controllerCount[0] ||
          controllerIndex[1]<controllerCount[1] ||
          controllerIndex[2]<controllerCount[2]) {
        Event e{};
        bool haveMidi=midiIndex<events;
        if(haveMidi && data.inputEvents->getEvent(midiIndex,e)!=kResultOk) {
            ++midiIndex;
            continue;
        }
        int32 midiOffset=std::numeric_limits<int32>::max();
        if(haveMidi) {
            // Never silently retime malformed MIDI to the beginning or
            // end of a buffer. That hides lost/out-of-order events in
            // exact Creative original-register and VST3 automation tests.
            if(e.sampleOffset<0 || e.sampleOffset>data.numSamples)
                return kInvalidArgument;
            midiOffset=e.sampleOffset;
        }
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
        int32 resetOffset=std::numeric_limits<int32>::max();
        ParamValue resetValue=0.;
        if(resetIndex<resetCount) {
            if(win95ResetQueue->getPoint(resetIndex,resetOffset,resetValue)!=kResultOk ||
               resetOffset<0 || resetOffset>data.numSamples || resetValue!=1.)
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
        int32 bankOffset=std::numeric_limits<int32>::max();
        ParamValue bankNormalized=0.;
        const bool haveBank=bankIndex<bankCount;
        if(haveBank) {
            if(bankQueue->getPoint(bankIndex,bankOffset,bankNormalized)!=kResultOk ||
               bankOffset<0 || bankOffset>data.numSamples ||
               !std::isfinite(bankNormalized) ||
               bankNormalized<0. || bankNormalized>1.)
                return kInvalidArgument;
        }
        int32 presetOffset=std::numeric_limits<int32>::max();
        ParamValue presetNormalized=0.;
        const bool havePreset=presetIndex<presetCount;
        if(havePreset) {
            if(presetQueue->getPoint(presetIndex,presetOffset,
                                     presetNormalized)!=kResultOk ||
               presetOffset<0 || presetOffset>data.numSamples ||
               !std::isfinite(presetNormalized) ||
               presetNormalized<0. || presetNormalized>1.)
                return kInvalidArgument;
        }
        int32 controllerOffset=std::numeric_limits<int32>::max();
        ParamValue controllerNormalized=0.;
        int controllerChoice=-1;
        for(std::size_t c=0;c<controllerQueues.size();++c) {
            if(controllerIndex[c]>=controllerCount[c])continue;
            int32 nextOffset=0;
            ParamValue nextNormalized=0.;
            if(controllerQueues[c]->getPoint(
                   controllerIndex[c],nextOffset,nextNormalized)!=kResultOk ||
               nextOffset<0 || nextOffset>data.numSamples ||
               !std::isfinite(nextNormalized) ||
               nextNormalized<0. || nextNormalized>1.)
                return kInvalidArgument;
            // A simultaneous CC7/CC10/CC11 resolves in canonical
            // controller-number order, not the host queue insertion order.
            if(nextOffset<controllerOffset) {
                controllerOffset=nextOffset;
                controllerNormalized=nextNormalized;
                controllerChoice=static_cast<int>(c);
            }
        }
        // A simultaneous program change must affect the NEXT NoteOn,
        // but never rewrite active physical voices. Ties are a strict
        // deterministic private diagnostic scheduling rule, not a
        // claim about untraced original WinMM ordering. CC64 precedes
        // NoteOff at the same timestamp, as verified in the prior path.
        const bool chooseReset=resetIndex<resetCount &&
            (!haveMidi || resetOffset<=midiOffset) &&
            (!haveCc || resetOffset<=ccOffset) &&
            (!haveProgram || resetOffset<=programOffset) &&
            (!haveBank || resetOffset<=bankOffset) &&
            (!havePreset || resetOffset<=presetOffset) &&
            (controllerChoice<0 || resetOffset<=controllerOffset);
        const bool chooseController=!chooseReset && controllerChoice>=0 &&
            (!haveMidi || controllerOffset<=midiOffset) &&
            (!haveCc || controllerOffset<=ccOffset) &&
            (!haveProgram || controllerOffset<=programOffset) &&
            (!haveBank || controllerOffset<=bankOffset) &&
            (!havePreset || controllerOffset<=presetOffset);
        // Bank is resolved before program at equal sample offsets.
        // This is a literal SF2 PHDR selection, NOT a 1996 CC0 mapping.
        const bool chooseBank=!chooseReset && !chooseController && haveBank &&
            (!haveMidi || bankOffset<=midiOffset) &&
            (!haveCc || bankOffset<=ccOffset) &&
            (!haveProgram || bankOffset<=programOffset) &&
            (!havePreset || bankOffset<=presetOffset);
        const bool chooseProgram=!chooseReset && !chooseController && !chooseBank && haveProgram &&
            (!haveMidi || programOffset<=midiOffset) &&
            (!haveCc || programOffset<=ccOffset) &&
            (!havePreset || programOffset<=presetOffset);
        // On ties an explicit real PHDR row wins over Bank/Program,
        // but ALWAYS before the following sample-accurate NoteOn.
        const bool choosePreset=!chooseReset && !chooseController &&
            !chooseBank && !chooseProgram && havePreset &&
            (!haveMidi || presetOffset<=midiOffset) &&
            (!haveCc || presetOffset<=ccOffset);
        const bool chooseCc=!chooseReset && !chooseController && !chooseBank && !chooseProgram && !choosePreset && haveCc &&
            (!haveMidi || ccOffset<=midiOffset);
        const int32 offset=chooseReset?resetOffset:(chooseController?controllerOffset:
            (chooseBank?bankOffset:
            (chooseProgram?programOffset:
            (choosePreset?presetOffset:
            (chooseCc?ccOffset:midiOffset)))));
        if(offset<pos)return kInvalidArgument;
        render(pos,offset,left,right);
        pos=offset;
        if(chooseReset) {
            if(!privateWin95MidiOutReset()) {
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
            ++resetIndex;
            continue;
        }
        if(chooseController) {
            if(clockedNativeRam_) {
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
            const auto value=static_cast<std::uint8_t>(
                controllerNormalized*127. + 0.5);
            if(!p002RomBank_.controller(
                    controllerNumbers[static_cast<std::size_t>(controllerChoice)],
                    value))return kInvalidArgument;
            publishBankState();
            ++controllerIndex[static_cast<std::size_t>(controllerChoice)];
            continue;
        }
        if(chooseBank) {
            if(clockedNativeRam_) {
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
            if(!p002RomBank_.ready())return kInvalidArgument;
            const auto bank=static_cast<std::uint16_t>(
                bankNormalized*128. + 0.5);
            // Unsupported requested bank is an ignored MIDI selection,
            // never a silent alias and never an invalid DAW audio block.
            (void)p002RomBank_.selectExactSf2Bank(bank);
            publishBankState();
            ++bankIndex;
            continue;
        }
        if(chooseProgram) {
            if(clockedNativeRam_) {
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
            const auto value=static_cast<std::uint8_t>(
                programNormalized*127. + 0.5);
            // Missing SoundFont programs are NOT physical MIDI notes.
            // The bank rejects this change atomically; preserve its last
            // valid selection and continue rendering active voices.
            // Do not fail the whole DAW audio callback just because the
            // requested SF2 bank/program has no actual PHDR entry.
            // Exact Creative 1996 missing-preset policy remains unproved.
            if(!p002RomBank_.ready())return kInvalidArgument;
            (void)p002RomBank_.programChange(value);
            publishBankState();
            ++programIndex;
            continue;
        }
        if(choosePreset) {
            if(clockedNativeRam_){
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
            const auto n=p002RomBank_.presetCount();
            if(!p002RomBank_.ready() || n==0u)return kInvalidArgument;
            const auto ordinal=static_cast<std::size_t>(
                presetNormalized*static_cast<double>(n-1u)+0.5);
            // Every accepted automation value selects one REAL PHDR
            // row, including exact bank identity and MIDI program.
            if(!p002RomBank_.selectPresetIndex(ordinal))
                return kInvalidArgument;
            publishBankState();
            ++presetIndex;
            continue;
        }
        if(chooseCc) {
            if(clockedNativeRam_) {
                clockedNativeRamFailed_=true;
                return kInvalidArgument;
            }
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
    if(data.outputs && data.numOutputs==1)
        data.outputs[0].silenceFlags=0; // conservative if held note just ended
    return kResultOk;
}
tresult PLUGIN_API Minimal::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;
    const auto pending=pendingBankState_.load(std::memory_order_acquire);
    const auto published=publishedBankState_.load(std::memory_order_acquire);
    const auto bits=pending!=state::kNoSelection?pending:published;
    // Keep original P001 test projects exactly byte-compatible.
    // Only a real loaded/host-restored SF2 selection uses V2 state.
    if(const auto selection=state::unpack(bits)) {
        auto bytes=state::encode(*selection);
        int32 wrote=0;
        return stream->write(bytes.data(),static_cast<int32>(bytes.size()),
                             &wrote)==kResultOk &&
               wrote==static_cast<int32>(bytes.size())?
               kResultOk:kResultFalse;
    }
    auto legacy=state::kLegacyMagic;
    int32 wrote=0;
    return stream->write(legacy.data(),static_cast<int32>(legacy.size()),
                         &wrote)==kResultOk &&
           wrote==static_cast<int32>(legacy.size())?
           kResultOk:kResultFalse;
}
tresult PLUGIN_API Minimal::setState(IBStream* stream) {
    if(!stream)return kInvalidArgument;
    std::array<std::uint8_t,8> prefix{};
    int32 count=0;
    if(stream->read(prefix.data(),static_cast<int32>(prefix.size()),
                    &count)!=kResultOk ||
       count!=static_cast<int32>(prefix.size()))
        return kResultFalse;
    if(prefix==state::kLegacyMagic) {
        pendingBankState_.store(state::kNoSelection,std::memory_order_release);
        publishedBankState_.store(state::kNoSelection,std::memory_order_release);
        return kResultOk;
    }
    if(prefix!=state::kMagic)return kResultFalse;
    std::array<std::uint8_t,state::kStateSize> bytes{};
    std::copy(prefix.begin(),prefix.end(),bytes.begin());
    count=0;
    if(stream->read(bytes.data()+prefix.size(),
                    static_cast<int32>(bytes.size()-prefix.size()),
                    &count)!=kResultOk ||
       count!=static_cast<int32>(bytes.size()-prefix.size()))
        return kResultFalse;
    const auto parsed=state::decode(bytes);
    if(!parsed)return kResultFalse; // CRC, version and range fail-closed
    const auto bits=state::pack(*parsed);
    // No UI-thread manipulation of the live Chip or mutable SoundFont.
    // Audio thread or the next non-RT activation applies this state.
    pendingBankState_.store(bits,std::memory_order_release);
    // The bank/audio thread publishes ONLY the actually accepted SF2
    // selection. Publishing a requested but unavailable program here
    // would race against the audio callback and misrepresent the state.
    return kResultOk;
}
}
