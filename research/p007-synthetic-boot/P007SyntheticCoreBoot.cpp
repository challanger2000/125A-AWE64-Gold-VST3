// Public synthetic BIOS, no Win95 / Creative binaries / private data.
// Executes ORIGINAL 86Box x86 hardware init and 1ms CPU slices headlessly.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <array>
namespace fs=std::filesystem;
using namespace std::chrono_literals;
using Init=int (*)(const char*,const char*,const char*,const char*);
using Fn=int (*)();
using Close=void (*)();
template<class T> T proc(HMODULE dll,const char* s) {
    auto fn=GetProcAddress(dll,s);
    return fn?std::bit_cast<T>(fn):nullptr;
}
static void write(const fs::path& p,const void* data,size_t n) {
    std::ofstream f(p,std::ios::binary);
    f.write(static_cast<const char*>(data),static_cast<std::streamsize>(n));
    if(!f)throw std::runtime_error("Cannot write synthetic BIOS");
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::error_code ec;
    const fs::path dll=fs::absolute(fs::path(argv[1]),ec);
    if(ec||!fs::is_regular_file(dll,ec))return 3;
    const fs::path root=fs::temp_directory_path()/
        (L"P007-public-synthetic-boot-"+std::to_wstring(GetCurrentProcessId()));
    const fs::path vm=root/L"vm",rom=root/L"roms";
    fs::create_directories(vm,ec);
    fs::create_directories(root/L"assets",ec);
    fs::create_directories(rom/L"machines"/L"p55t2p4",ec);
    fs::create_directories(rom/L"video"/L"vga",ec);
    if(ec)return 4;
    std::vector<std::uint8_t> bios(131072,0x90u);
    // Synthetic BIOS built with original 16-bit x86 machine instructions.
    // First issues real ATA PIO read of LBA 0 and verifies the synthetic
    // disk's two-byte marker. Polls until ATA DRQ (virtual CPU ticks), not
    // a too-short 65535-iteration loop that expires before disk latency.
    // Host watchdog bounds a real hardware stall. Then MIDI+CRC follows.
    // No Creative driver or proprietary ROM is used in this public probe.
    static constexpr std::uint8_t program[]={
        0xfa, 0x0e, 0x1f, 0xfc, 0xba, 0xf6, 0x01, 0xb0, 0xe0, 0xee, 0xba, 0xf2,
        0x01, 0xb0, 0x01, 0xee, 0x42, 0x30, 0xc0, 0xee, 0x42, 0xee, 0x42, 0xee,
        0xba, 0xf7, 0x01, 0xb0, 0x20, 0xee, 0xb9, 0xff, 0xff, 0xec, 0xa8, 0x01,
        0x75, 0x0a, 0xa8, 0x80, 0x75, 0x04, 0xa8, 0x08, 0x75, 0x0a, 0xeb, 0xf1,
        0xba, 0xf8, 0x02, 0xb0, 0x46, 0xee, 0xeb, 0xfe, 0xba, 0xf0, 0x01, 0xed,
        0x3d, 0x07, 0xa5, 0x75, 0xef, 0xba, 0xfb, 0x02, 0xb0, 0x80, 0xee, 0xba,
        0xf8, 0x02, 0xb0, 0x01, 0xee, 0x42, 0x30, 0xc0, 0xee, 0xba, 0xfb, 0x02,
        0xb0, 0x03, 0xee, 0xba, 0xfc, 0x02, 0xb0, 0x0b, 0xee, 0xbe, 0xa5, 0x00,
        0xe8, 0x2a, 0x00, 0xbe, 0xbd, 0x00, 0xb9, 0x06, 0x00, 0xba, 0xfd, 0x02,
        0xec, 0xa8, 0x01, 0x74, 0xfb, 0xba, 0xf8, 0x02, 0xec, 0x3a, 0x04, 0x75,
        0x0c, 0x46, 0xe2, 0xed, 0xbe, 0xc3, 0x00, 0xe8, 0x0b, 0x00, 0xf4, 0xeb,
        0xfd, 0xbe, 0xda, 0x00, 0xe8, 0x02, 0x00, 0xeb, 0xf5, 0xac, 0x84, 0xc0,
        0x74, 0x12, 0x88, 0xc3, 0xba, 0xfd, 0x02, 0xec, 0xa8, 0x20, 0x74, 0xf8,
        0xba, 0xf8, 0x02, 0x88, 0xd8, 0xee, 0xeb, 0xe9, 0xc3, 0x50, 0x30, 0x30,
        0x37, 0x2d, 0x41, 0x57, 0x45, 0x2d, 0x57, 0x49, 0x4e, 0x4d, 0x4d, 0x2d,
        0x52, 0x45, 0x41, 0x44, 0x59, 0x2d, 0x31, 0x0a, 0x00, 0xa5, 0x00, 0x90,
        0x3c, 0x6e, 0x3d, 0x50, 0x30, 0x30, 0x37, 0x2d, 0x41, 0x57, 0x45, 0x2d,
        0x57, 0x49, 0x4e, 0x4d, 0x4d, 0x2d, 0x53, 0x54, 0x4f, 0x50, 0x2d, 0x31,
        0x0a, 0x00, 0x50, 0x30, 0x30, 0x37, 0x2d, 0x41, 0x57, 0x45, 0x2d, 0x57,
        0x49, 0x4e, 0x4d, 0x4d, 0x2d, 0x45, 0x52, 0x52, 0x4f, 0x52, 0x0a, 0x00
    };
    for(size_t n=0;n<sizeof(program);++n)
        bios[0x10000+n]=program[n]; // F000:0000
    // Reset vector FFFF0: JMP FAR F000:0000, original x86 real mode.
    bios[0x1fff0]=0xeau;bios[0x1fff1]=0x00u;bios[0x1fff2]=0x00u;
    bios[0x1fff3]=0x00u;bios[0x1fff4]=0xf0u;
    write(rom/L"machines"/L"p55t2p4"/L"0207_j2.bin",bios.data(),bios.size());
    // Real emulated ATA device: our own 8-cylinder public dummy disk, never
    // the user's private 512-MiB Windows 95 disk.
    std::vector<std::uint8_t> dummyDisk(8u*16u*63u*512u,0u);
    dummyDisk[0]=0x07u;dummyDisk[1]=0xa5u; // read as AX=0xA507
    dummyDisk[510]=0x55u;dummyDisk[511]=0xaau;
    write(vm/L"test.img",dummyDisk.data(),dummyDisk.size());
    std::array<std::uint8_t,32768> vga{};
    vga[0]=0x55u;vga[1]=0xaau;vga[2]=0x40u;
    unsigned checksum=0u;
    for(auto b:vga)checksum+=b;
    vga.back()=static_cast<std::uint8_t>(0u-(checksum&255u));
    write(rom/L"video"/L"vga"/L"ibm_vga.bin",vga.data(),vga.size());
    const std::string config=R"([Machine]
machine = p55t2p4
cpu_family = pentium_p54c
cpu_speed = 133333333
cpu_multi = 2
cpu_use_dynarec = 1
fpu_type = internal
mem_size = 16384
time_sync = disabled
[Video]
gfxcard = vga
[Input devices]
keyboard_type = keyboard_at
mouse_type = ps2
[Sound]
sndcard = none
[Storage controllers]
hdc = internal
[Hard disks]
hdd_01_fn = test.img
hdd_01_parameters = 63, 16, 8, 0, ide
hdd_01_ide_channel = 0:0
hdd_01_speed = ramdisk
[Ports (COM & LPT)]
serial2_enabled = 1
)";
    write(vm/L"P007_SYNTH.cfg",config.data(),config.size());
    const std::string cfg="[General]\nconfirm_exit = 0\n";
    write(root/L"86box.cfg",cfg.data(),cfg.size());
    std::atomic<bool> finished{false};
    std::thread watchdog([&]{
        const auto limit=std::chrono::steady_clock::now()+25s;
        while(!finished.load(std::memory_order_acquire)){
            if(std::chrono::steady_clock::now()>limit){
                std::fputs("P007_SYNTHETIC_CPU_HANG_25S\n",stderr);
                std::fflush(stderr);
                TerminateProcess(GetCurrentProcess(),125);
            }
            std::this_thread::sleep_for(100ms);
        }
    });
    auto done=[&]{finished.store(true,std::memory_order_release);watchdog.join();};
    HMODULE core=LoadLibraryExW(dll.c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!core){std::printf("P007_SYNTH_CORE_LOAD_ERROR=%lu\n",GetLastError());done();return 5;}
    auto init=proc<Init>(core,"p007_core_init_profile");
    auto boot=proc<Fn>(core,"p007_core_guest_boot");
    auto step=proc<Fn>(core,"p007_core_guest_step");
    auto stage=proc<Fn>(core,"p007_core_guest_stage");
    auto ready=proc<Fn>(core,"p007_midi_guest_ready");
    struct Trace { std::uint64_t reads,readSectors,writes,writeSectors;
                   std::uint32_t lastRead,lastWrite,readId,writeId; };
    auto getTrace=proc<int (*)(Trace*)>(core,"p007_trace_get");
    using Midi=int (*)(std::uint8_t,std::uint8_t,std::uint8_t);
    auto midi=proc<Midi>(core,"p007_midi_submit");
    auto close=proc<Close>(core,"p007_core_guest_close");
    if(!init||!boot||!step||!stage||!ready||!midi||!close){FreeLibrary(core);done();return 6;}
    const auto a=vm.string(),b=rom.string(),g=(root/L"86box.cfg").string();
    const int ir=init(a.c_str(),b.c_str(),"P007_SYNTH.cfg",g.c_str());
    std::printf("P007_SYNTH_ORIGINAL_PROFILE=%d\n",ir);
    std::fflush(stdout);
    if(ir!=1){FreeLibrary(core);done();return 7;}
    const int br=boot();
    std::printf("P007_SYNTH_ORIGINAL_HARDWARE_BOOT=%d\n",br);
    std::fflush(stdout);
    if(br!=1){FreeLibrary(core);done();return 8;}
    bool noteAccepted=false,packetAcknowledged=false;
    int readySlice=-1,ackSlice=-1;
    // Match the ACTUAL VST3 worker's real 1-ms pacing. The earlier
    // accelerated probe ran 1000 virtual milliseconds in ~30 ms wall time
    // and could starve original IDE/audio device callbacks.
    auto next=std::chrono::steady_clock::now();
    for(int i=0;i<1000;++i){
        if(step()!=1){close();FreeLibrary(core);done();return 9;}
        next+=1ms;
        std::this_thread::sleep_until(next);
        if(!noteAccepted && ready()==1){
            readySlice=i;
            noteAccepted=midi(0x90u,60u,110u)==1;
            if(!noteAccepted)break;
        }else if(noteAccepted && ready()==0){
            ackSlice=i;
            packetAcknowledged=true;
            break;
        }
    }
    std::printf("P007_SYNTH_ORIGINAL_CPU_STAGE=%d READY_SLICE=%d"
                " MIDI_ACCEPTED=%d PACKET_VALIDATED_ACK_SLICE=%d\n",
                stage(),readySlice,noteAccepted,ackSlice);
    if(getTrace){
        Trace disk{};
        if(getTrace(&disk))
            std::printf("P007_SYNTH_ORIGINAL_HDD_REQUESTS=%llu SECTORS=%llu"
                        " LAST_LBA=%u DRIVE=%u\n",
                static_cast<unsigned long long>(disk.reads),
                static_cast<unsigned long long>(disk.readSectors),
                disk.lastRead,disk.readId);
    }
    std::fflush(stdout);
    close();
    const int closed=stage();
    FreeLibrary(core);
    done();
    fs::remove_all(root,ec);
    if(closed!=2)return 10;
    if(!noteAccepted){
        std::puts("P007_SYNTH_GUEST_UART_READY_NOT_RECEIVED");
        return 11;
    }
    if(!packetAcknowledged){
        std::puts("P007_SYNTH_HOST_TO_GUEST_6BYTE_MIDI_NOT_ACKNOWLEDGED");
        return 12;
    }
    std::puts("P007_ORIGINAL_86BOX_SYNTHETIC_BIOS_ATA_PIO_LBA0_PLUS_X86_COM2_MIDI_CRC_PASS");
    std::puts("P007_SYNTHETIC_ROM_ONLY_NO_REAL_CREATIVE_WINMM");
    return 0;
}
