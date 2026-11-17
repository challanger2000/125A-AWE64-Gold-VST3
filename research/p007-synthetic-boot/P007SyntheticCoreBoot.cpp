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
    // Pentium RESET at FFFF0: short jump to self, a legal infinite loop.
    bios[0x1fff0]=0xebu;bios[0x1fff1]=0xfeu;
    write(rom/L"machines"/L"p55t2p4"/L"0207_j2.bin",bios.data(),bios.size());
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
    auto close=proc<Close>(core,"p007_core_guest_close");
    if(!init||!boot||!step||!stage||!close){FreeLibrary(core);done();return 6;}
    const auto a=vm.string(),b=rom.string(),g=(root/L"86box.cfg").string();
    const int ir=init(a.c_str(),b.c_str(),"P007_SYNTH.cfg",g.c_str());
    std::printf("P007_SYNTH_ORIGINAL_PROFILE=%d\n",ir);
    std::fflush(stdout);
    if(ir!=1){FreeLibrary(core);done();return 7;}
    const int br=boot();
    std::printf("P007_SYNTH_ORIGINAL_HARDWARE_BOOT=%d\n",br);
    std::fflush(stdout);
    if(br!=1){FreeLibrary(core);done();return 8;}
    for(int i=0;i<100;++i){
        if(step()!=1){close();FreeLibrary(core);done();return 9;}
    }
    std::printf("P007_SYNTH_ORIGINAL_CPU_SLICES=100 STAGE=%d\n",stage());
    close();
    const int closed=stage();
    FreeLibrary(core);
    done();
    fs::remove_all(root,ec);
    if(closed!=2)return 10;
    std::puts("P007_ORIGINAL_86BOX_SYNTHETIC_BIOS_HEADLESS_CPU_BOOT_PASS");
    return 0;
}
