// Private owner-side Windows x64 host for the REAL exported P002 VST3.
// It DOES NOT synthesize original Win95 evidence or replay C4/D4 port clips.
// Only P002 native MIDI->original-EMU8000 path supplies note registers.
// Original SHA-pinned SF2/ROM/VXD/SMALW source files stay outside GitHub.
// Captures genuine native pre-CT1745 PCM16 and native EMU8K OUTs in one
// activation, with an explicit external MIDI schedule (NO auto alignment).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/eventlist.h"

namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace fs=std::filesystem;
constexpr std::uint64_t kMaxFrames=44100u*30u;
constexpr int kBlock=512;
void check(bool condition,const char* why) {
    if(!condition)throw std::runtime_error(why);
}
std::uint64_t frameNumber(const wchar_t* arg) {
    check(arg && *arg,"invalid frame");
    wchar_t* end=nullptr;
    if(*arg==L'-' || *arg==L'+')throw std::runtime_error("frame must be unsigned");
    const auto n=std::wcstoull(arg,&end,10);
    check(end && *end==0 && end!=arg && n<=kMaxFrames,
          "frame is invalid or exceeds the native 30s recording bound");
    return n;
}
struct Schedule {
    std::array<std::uint64_t,4> when{};
    std::uint64_t total=0;
};
bool validSchedule(const Schedule& s) noexcept {
    return s.when[0]<s.when[1] && s.when[1]<s.when[2] &&
        s.when[2]<s.when[3] && s.when[3]<s.total &&
        s.total>=256 && s.total<=kMaxFrames;
}
Schedule scheduleFromArgs(wchar_t** argv) {
    Schedule s{{frameNumber(argv[9]),frameNumber(argv[10]),
                frameNumber(argv[11]),frameNumber(argv[12])},
               frameNumber(argv[13])};
    check(validSchedule(s),
          "require C4-on < C4-off < D4-on < D4-off < total frames");
    return s;
}
void setEnv(const wchar_t* name,const wchar_t* value) {
    check(_wputenv_s(name,value)==0,"unable to set a private source path");
}
std::string hashFileSha256(const fs::path& path) {
    // Called ONLY after native processing has stopped. Do not use a
    // host-side hash in any audio callback or transfer proprietary
    // source bytes into the compiled plugin or GitHub CI.
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    check(in.good(),"cannot SHA-attest a private evidence file");
    const auto size=in.tellg();
    check(size>=0 && size<=64ll*1024ll*1024ll,
          "private evidence file exceeds bounded host SHA size");
    in.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if(!bytes.empty())
        check(static_cast<bool>(in.read(
            reinterpret_cast<char*>(bytes.data()),size)),
              "private evidence input truncated while hashing");
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    check(BCryptOpenAlgorithmProvider(
        &algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,
        "Windows SHA256 provider unavailable");
    std::array<unsigned char,32> digest{};
    const auto status=BCryptHash(algorithm,nullptr,0,
        bytes.empty()?nullptr:bytes.data(),
        static_cast<ULONG>(bytes.size()),digest.data(),
        static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);
    check(status>=0,"Windows SHA256 evidence hash failed");
    constexpr char hex[]="0123456789abcdef";
    std::string out;
    out.reserve(digest.size()*2u);
    for(unsigned char byte:digest) {
        out.push_back(hex[byte>>4u]);out.push_back(hex[byte&15u]);
    }
    return out;
}
void saveClockManifest(const fs::path& path,const Schedule& schedule,
                       const std::array<std::size_t,2>& audible,
                       wchar_t** argv) {
    const std::array<const char*,8> keys{{
        "exported_vst3_sha256","original_rom_sha256",
        "original_owner_sf2_sha256","original_ram_ports_sha256",
        "original_final_ram_pcm_sha256","original_vxd_pack_sha256",
        "native_chip_out_trace_sha256","native_precard_wav_sha256"}};
    std::ostringstream json;
    json<<"{\n  \"schema\": \"P002_NATIVE_REAL_VST3_HOST_CLOCKS_V1\",\n"
        <<"  \"sample_rate\": 44100,\n"
        <<"  \"host_activation_origin_frame\": 0,\n"
        <<"  \"note_c4_on_frame\": "<<schedule.when[0]<<",\n"
        <<"  \"note_c4_off_frame\": "<<schedule.when[1]<<",\n"
        <<"  \"note_d4_on_frame\": "<<schedule.when[2]<<",\n"
        <<"  \"note_d4_off_frame\": "<<schedule.when[3]<<",\n"
        <<"  \"pcm_frames\": "<<schedule.total<<",\n"
        <<"  \"midi_channel\": 0,\n"
        <<"  \"midi_velocity\": 100,\n"
        <<"  \"audible_c4_on_frames\": "<<audible[0]<<",\n"
        <<"  \"audible_d4_on_frames\": "<<audible[1]<<",\n";
    for(std::size_t i=0;i<keys.size();++i)
        json<<"  \""<<keys[i]<<"\": \""<<hashFileSha256(fs::path(argv[i+1]))
            <<"\""<<","<<"\n";
    json<<"  \"original_win95_vs_native_pcm_parity\": \"NOT_COMPARED\",\n"
        <<"  \"original_creative_driver_provenance\": \"NOT_PROVEN_BY_HOST\"\n}\n";
    const std::string bytes=json.str();
    check(bytes.size()<=65536u,"native evidence JSON unexpectedly large");
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,
                                 CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(file!=INVALID_HANDLE_VALUE,
          "refuse overwrite/missing parent for private clock manifest");
    DWORD done=0;
    const bool saved=WriteFile(file,bytes.data(),
                               static_cast<DWORD>(bytes.size()),&done,
                               nullptr)!=0 &&
                     done==bytes.size() && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(!saved) {
        DeleteFileW(path.c_str());
        throw std::runtime_error("private native clock manifest write failed");
    }
}
struct Instance {
    HMODULE dll=nullptr;
    IPluginFactory* factory=nullptr;
    IComponent* component=nullptr;
    IAudioProcessor* processor=nullptr;
    bool active=false,processing=false;
    explicit Instance(const wchar_t* dllpath) {
        dll=LoadLibraryW(dllpath);
        check(dll!=nullptr,"cannot load actual exported VST3 DLL");
        const auto get=reinterpret_cast<IPluginFactory*(*)()>(
            GetProcAddress(dll,"GetPluginFactory"));
        check(get!=nullptr,"VST3 factory symbol missing");
        factory=get();
        check(factory!=nullptr,"VST3 factory is null");
        PClassInfo info{};
        bool found=false;
        for(int32 i=0;i<factory->countClasses();++i) {
            check(factory->getClassInfo(i,&info)==kResultOk,
                  "VST3 class info failed");
            if(std::strcmp(info.category,kVstAudioEffectClass)==0) {
                found=true;break;
            }
        }
        check(found,"no exported VST3 audio processor class");
        check(factory->createInstance(info.cid,IComponent::iid,
              reinterpret_cast<void**>(&component))==kResultOk &&
              component,"cannot instantiate real VST3");
        check(component->initialize(nullptr)==kResultOk,
              "VST3 initialization failed");
        check(component->queryInterface(IAudioProcessor::iid,
              reinterpret_cast<void**>(&processor))==kResultOk &&
              processor,"real VST3 processor interface missing");
        SpeakerArrangement stereo=SpeakerArr::kStereo;
        check(processor->setBusArrangements(nullptr,0,&stereo,1)==kResultTrue,
              "native VST3 must have one stereo output");
        ProcessSetup setup{kRealtime,kSample32,kBlock,44100.0};
        check(processor->setupProcessing(setup)==kResultOk,
              "original 44100 Hz native processor unavailable");
        check(component->setActive(true)==kResultOk,
              "real SHA-pinned original RAM/VXD source rejected by VST3");
        active=true;
        check(processor->setProcessing(true)==kResultOk,
              "actual VST3 processing unavailable");
        processing=true;
    }
    void stop() {
        if(processing) {
            check(processor->setProcessing(false)==kResultOk,
                  "VST3 processing stop failed");
            processing=false;
        }
        if(active) {
            // Both native evidence files are emitted by VST3 ONLY here;
            // both must be new, outside the audio callback.
            check(component->setActive(false)==kResultOk,
                  "native VST3 original RAM port/WAV evidence capture refused");
            active=false;
        }
    }
    ~Instance() {
        if(processor) {
            if(processing)processor->setProcessing(false);
            processor->release();
        }
        if(component) {
            if(active)component->setActive(false);
            component->terminate();
            component->release();
        }
        if(factory)factory->release();
        if(dll)FreeLibrary(dll);
    }
};
void noteEvent(EventList& list,int key,bool on,int offset) {
    Event e{};
    e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
    e.busIndex=0;
    e.sampleOffset=offset;
    if(on) {
        e.noteOn.channel=0;
        e.noteOn.pitch=static_cast<Steinberg::int16>(key);
        e.noteOn.velocity=100.f/127.f;
        e.noteOn.noteId=key;
    }else {
        e.noteOff.channel=0;
        e.noteOff.pitch=static_cast<Steinberg::int16>(key);
        e.noteOff.velocity=0.f;
        e.noteOff.noteId=key;
    }
    check(list.addEvent(e)==kResultOk,"host MIDI event enqueue failed");
}
std::array<std::size_t,2> play(IAudioProcessor* processor,const Schedule& s) {
    // Four explicitly timed actions matching the OWNER's Win95 C4/D4
    // MIDI experiment. The native clock starts at VST3 activation:
    // no guess about original VM boot silence or an original audio offset.
    constexpr std::array<int,4> keys{{60,60,62,62}};
    constexpr std::array<bool,4> on{{true,false,true,false}};
    std::uint64_t position=0;
    std::size_t next=0;
    std::array<std::size_t,2> noteAudible{};
    while(position<s.total) {
        const auto count=static_cast<int>(
            std::min<std::uint64_t>(kBlock,s.total-position));
        std::array<float,kBlock> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers out{};
        out.numChannels=2;
        out.channelBuffers32=buffers;
        EventList eventList{};
        while(next<s.when.size() && s.when[next]<position+count) {
            check(s.when[next]>=position,"MIDI schedule out of order");
            noteEvent(eventList,keys[next],on[next],
                      static_cast<int>(s.when[next]-position));
            ++next;
        }
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=count;
        data.outputs=&out;
        data.numOutputs=1;
        data.inputEvents=&eventList;
        check(processor->process(data)==kResultOk,
              "real exported VST3 rejected host RAM MIDI render");
        for(int i=0;i<count;++i) {
            check(std::isfinite(left[i]) && std::isfinite(right[i]),
                  "native VST3 returned non-finite output");
            // Audible signal does not establish source identity; it only
            // prevents a silent fake evidence file from being accepted.
            const auto frame=position+static_cast<std::uint64_t>(i);
            const int which=(frame>=s.when[0] && frame<s.when[1])?0:
                            ((frame>=s.when[2] && frame<s.when[3])?1:-1);
            if(which>=0 && (std::abs(left[i])>0.00002f ||
                            std::abs(right[i])>0.00002f))
                ++noteAudible[static_cast<std::size_t>(which)];
        }
        position+=static_cast<std::uint64_t>(count);
    }
    check(next==s.when.size(),"host did not deliver all four MIDI events");
    check(noteAudible[0]>=64u && noteAudible[1]>=64u,
          "native C4 or D4 source note produced no audible PCM");
    return noteAudible;
}
void validateFiles(wchar_t** argv,const Schedule& s) {
    std::array<fs::path,8> files{
        fs::path(argv[1]),fs::path(argv[2]),fs::path(argv[3]),
        fs::path(argv[4]),fs::path(argv[5]),fs::path(argv[6]),
        fs::path(argv[7]),fs::path(argv[8])};
    for(std::size_t i=0;i<files.size();++i) {
        check(files[i].is_absolute(),"all native evidence paths must be absolute");
        for(std::size_t j=0;j<i;++j)
            check(files[i].lexically_normal()!=files[j].lexically_normal(),
                  "source/evidence file paths must all be distinct");
        if(i<6u)check(fs::is_regular_file(files[i]),
                       "private input or exported VST3 DLL does not exist");
        else {
            check(!fs::exists(files[i]),
                  "native evidence output already exists; refusing overwrite");
            check(fs::is_directory(files[i].parent_path()),
                  "native evidence output parent directory missing");
        }
    }
    check(s.total<=kMaxFrames,"PCM capture exceeds 30 seconds");
    // Optional third host-origin private file. The source and two chip
    // captures must never be overwritten, even on repeated executions.
    if(argv[14]) {
        const fs::path manifest(argv[14]);
        check(manifest.is_absolute() && !fs::exists(manifest) &&
              fs::is_directory(manifest.parent_path()),
              "clock manifest must be NEW absolute private file");
        for(const auto& original:files)
            check(original.lexically_normal()!=manifest.lexically_normal(),
                  "clock manifest path aliases an original/native input");
    }
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc==2 && std::wcscmp(argv[1],L"--help")==0) {
        std::wcout<<L"Usage (all Windows ABSOLUTE paths):\n"
          L"awe64_p002_native_ram_evidence_host.exe <VST3_DLL> <ROM_RAW> "
          L"<ORIGINAL_SF2> <ORIGINAL_FILTERED_RAM_PORTS> "
          L"<ORIGINAL_FINAL_RAM_PCM16> <AUTHENTIC_VXD_PACK> "
          L"<NEW_NATIVE_OUT_TRACE> <NEW_NATIVE_PRECARD_WAV> "
          L"<C4_ON_FRAME> <C4_OFF_FRAME> <D4_ON_FRAME> "
          L"<D4_OFF_FRAME> <TOTAL_FRAMES> [NEW_CLOCK_MANIFEST]\n"
          L"Optional clocked native source mode: append <ORIGINAL_C4_D4_NOTE_PORTS> "
          L"after the NEW_CLOCK_MANIFEST. Authentic source values VERIFY "
          L"native OUTs; only original sample offsets schedule native writes.\n"
          L"No Win95 clock is inferred. Original/native parity is NOT proven by this host.\n";
        return 0;
    }
    if(argc==2 && std::wcscmp(argv[1],L"--self-test")==0) {
        const Schedule valid{{137u,17864u,46932u,65098u},88200u};
        Schedule mutation=valid;
        if(!validSchedule(valid))return 3;
        mutation.when[1]=mutation.when[0];
        if(validSchedule(mutation))return 4;
        mutation=valid;mutation.when[2]=mutation.when[1];
        if(validSchedule(mutation))return 5;
        mutation=valid;mutation.total=kMaxFrames+1u;
        if(validSchedule(mutation))return 6;
        // Native owner evidence SHA256 is computed off-RT through
        // Windows CNG. Exercise the exact file-hash implementation,
        // not a copied digest literal in Python alone.
        wchar_t tmpDir[MAX_PATH]{},tmpFile[MAX_PATH]{};
        const DWORD tempLen=GetTempPathW(MAX_PATH,tmpDir);
        if(tempLen==0 || tempLen>=MAX_PATH ||
           !GetTempFileNameW(tmpDir,L"P2H",0,tmpFile))return 7;
        const HANDLE temp=CreateFileW(tmpFile,GENERIC_WRITE,0,nullptr,
                                      TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(temp==INVALID_HANDLE_VALUE) {
            DeleteFileW(tmpFile);return 8;
        }
        DWORD wrote=0;
        const bool created=WriteFile(temp,"abc",3,&wrote,nullptr)!=0 &&
                           wrote==3 && FlushFileBuffers(temp)!=0;
        CloseHandle(temp);
        bool verified=false;
        if(created) {
            try {
                verified=hashFileSha256(fs::path(tmpFile))==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
            }catch(const std::exception&) {verified=false;}
        }
        DeleteFileW(tmpFile);
        if(!verified)return 9;
        std::cout<<"P002_PRIVATE_WINDOWS_NATIVE_HOST_SCHEDULE_SELFTEST_PASS "
                 <<" original_driver_and_pcm_parity=NOT_TESTED\n";
        return 0;
    }
    if(argc!=14 && argc!=15 && argc!=16)return 2;
    bool outputPathsVerifiedNew=false;
    try {
        const Schedule sch=scheduleFromArgs(argv);
        validateFiles(argv,sch);
        if(argc==16) {
            // A real source capture is an additional readonly input;
            // never confuse it with newly emitted independent native OUTs.
            const fs::path sourceClock(argv[15]);
            check(sourceClock.is_absolute() &&
                  fs::is_regular_file(sourceClock) &&
                  fs::file_size(sourceClock)==5741u,
                  "clocked native input requires original SHA-pinned note source");
            for(int i=1;i<=14;++i)
                check(sourceClock.lexically_normal()!=
                      fs::path(argv[i]).lexically_normal(),
                      "clocked native source cannot alias any input/output");
        }
        outputPathsVerifiedNew=true;
        setEnv(L"AWE64_P001_ROM_PATH",argv[2]);
        setEnv(L"AWE64_P002_PRIVATE_ORIGINAL_RAM_SF2",argv[3]);
        setEnv(L"AWE64_P002_PRIVATE_ORIGINAL_RAM_PORTS",argv[4]);
        setEnv(L"AWE64_P002_PRIVATE_ORIGINAL_RAM_FINAL_PCM16",argv[5]);
        setEnv(L"AWE64_P002_PRIVATE_VXD_RESOURCES_PATH",argv[6]);
        setEnv(L"AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES",L"1");
        setEnv(L"AWE64_P002_NATIVE_REGISTER_OUT_PATH",argv[7]);
        setEnv(L"AWE64_P002_NATIVE_DRY_PCM_WAV_PATH",argv[8]);
        setEnv(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",L"");
        setEnv(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",L"");
        setEnv(L"AWE64_P002_PRIVATE_ORIGINAL_RAM_NOTE_PORTS",
               argc==16?argv[15]:L"");
        setEnv(L"AWE64_P002_CLOCKED_NATIVE_ORIGINAL_RAM",
               argc==16?L"1":L"");
        Instance vst(argv[1]);
        const auto audible=play(vst.processor,sch);
        vst.stop();
        const auto nativeWav=fs::path(argv[8]);
        const auto nativeTrace=fs::path(argv[7]);
        check(fs::is_regular_file(nativeWav) &&
              fs::file_size(nativeWav)==44u+4u*sch.total,
              "native VST3 pre-card PCM16 WAV size invalid");
        check(fs::is_regular_file(nativeTrace) &&
              fs::file_size(nativeTrace)>120u,
              "native VST3 original RAM note port trace missing/empty");
        if(argc==15)
            saveClockManifest(fs::path(argv[14]),sch,audible,argv);
        std::cout<<"P002_REAL_EXPORTED_NATIVE_RAM_EVIDENCE_CAPTURED"
                 <<" C4_D4_velocity=100"
                 <<" original_chip_DSP=UNCHANGED"
                 <<" dry_pcm_rate=44100"
                 <<" frames="<<sch.total
                 <<" C4_audible_samples="<<audible[0]
                 <<" D4_audible_samples="<<audible[1]
                 <<" private_clock_sha_manifest="<<(argc==15?"WRITTEN":"NOT_REQUESTED")
                 <<" native_original_port_parity="
                 <<(argc==16?"ALL_C4_D4_WRITES_MATCHED_SOURCE":
                               "NOT_COMPARED")
                 <<" Win95_vs_native_dry_pcm=NOT_COMPARED"
                 <<" release_approved=FALSE\n";
        return 0;
    }catch(const std::exception& err) {
        // A failed actual MIDI render may still call VST3 setActive(false)
        // during unwinding and emit an incomplete, apparently plausible
        // OUT/WAV pair. Never leave that partial evidence on disk.
        // Do NOT remove anything if validation refused an existing
        // file or malformed source path before arming the capture.
        if(outputPathsVerifiedNew) {
            std::error_code ignore;
            fs::remove(fs::path(argv[7]),ignore);
            ignore.clear();
            fs::remove(fs::path(argv[8]),ignore);
            if(argc==15) {
                ignore.clear();
                fs::remove(fs::path(argv[14]),ignore);
            }
        }
        std::cerr<<"P002_NATIVE_RAM_EVIDENCE_NO_GO "<<err.what()<<"\n";
        return 1;
    }
}
