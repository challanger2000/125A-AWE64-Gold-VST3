// P118: ACTUALLY EXECUTE x64 host machine code that loads the exact
// third immediate argument as the original old-86Box callback does.
// Native Windows x64 (R8D), native Linux x64 (EDX). Synthetic values,
// no Creative/EMU8000 binaries, Win95 guest, DSP or product source.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#ifdef _WIN32
# include <windows.h>
#else
# include <sys/mman.h>
# include <unistd.h>
#endif
#if !defined(__x86_64__) && !defined(_M_X64)
# error "P118 original JIT site callback test requires x86-64"
#endif

namespace {
volatile std::uint32_t got_pc=0;
volatile std::uint32_t got_ebp=0;
volatile std::uint32_t got_site=0;
extern "C" void checked_callback(std::uint32_t pc,std::uint32_t ebp,
                                  std::uint32_t site) noexcept {
    got_pc=pc;
    got_ebp=ebp;
    got_site=site;
}
void imm32(std::uint8_t* p,std::uint32_t v) noexcept {
    for(int i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(v>>(8*i));
}
void imm64(std::uint8_t* p,std::uint64_t v) noexcept {
    for(int i=0;i<8;i++)p[i]=static_cast<std::uint8_t>(v>>(8*i));
}
using Generated=void(*)();
}

int main() {
#ifdef _WIN32
    auto* page=static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
#else
    auto* page=static_cast<std::uint8_t*>(mmap(nullptr,4096,
        PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    if(page==MAP_FAILED)page=nullptr;
#endif
    if(!page) {std::puts("P118_ABI_EXEC_PAGE_UNAVAILABLE");return 2;}
    bool pass=true;
    for(std::uint32_t i=0;i<1000u && pass;i++) {
        const std::uint32_t pc=0xC101920Bu + (i%19u);
        const std::uint32_t ebp=0x12340000u ^ (i*0x4321u);
        const std::uint32_t site=1u+(i*499u)%500000u;
        std::array<std::uint8_t,64> code{};
        std::size_t pos=0;
        auto b=[&](std::uint8_t v){code[pos++]=v;};
        auto d=[&](std::uint32_t v){imm32(code.data()+pos,v);pos+=4;};
        auto q=[&](std::uint64_t v){imm64(code.data()+pos,v);pos+=8;};
#ifdef _WIN32
        b(0x48);b(0x83);b(0xec);b(0x28); // 32-byte shadow + alignment
        b(0xb9);d(pc);                    // mov ecx, original guest PC
        b(0xba);d(ebp);                   // mov edx, actual guest EBP
        b(0x41);b(0xb8);d(site);          // mov r8d, P117 compile-site token
#else
        b(0x48);b(0x83);b(0xec);b(0x08); // align host stack to 16 bytes
        b(0xbf);d(pc);                    // mov edi, guest PC
        b(0xbe);d(ebp);                   // mov esi, actual guest EBP
        b(0xba);d(site);                  // mov edx, P117 compile-site token
#endif
        b(0x48);b(0xb8);
        q(static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(&checked_callback)));
        b(0xff);b(0xd0);                 // call rax
#ifdef _WIN32
        b(0x48);b(0x83);b(0xc4);b(0x28); // restore shadow stack
#else
        b(0x48);b(0x83);b(0xc4);b(0x08);
#endif
        b(0xc3);                         // ret
        std::memcpy(page,code.data(),pos);
#ifdef _WIN32
        FlushInstructionCache(GetCurrentProcess(),page,pos);
#else
        __builtin___clear_cache(reinterpret_cast<char*>(page),
                                reinterpret_cast<char*>(page+pos));
#endif
        got_pc=0u;
        got_ebp=0u;
        got_site=0u;
        auto fn=reinterpret_cast<Generated>(page);
        fn();
        if(got_pc!=pc || got_ebp!=ebp || got_site!=site) {
            std::printf("P118_ACTUAL_NATIVE_JIT_CALLBACK_ABI_FAIL iteration=%u\n",i);
            pass=false;
        }
    }
#ifdef _WIN32
    VirtualFree(page,0,MEM_RELEASE);
#else
    munmap(page,4096);
#endif
    if(!pass)return 1;
    std::puts("P118_ACTUAL_EXECUTED_JIT_THIRD_IMMEDIATE_ARG_PASS "
              "iterations=1000 original_win95_guest=NOT_RUN");
    return 0;
}
