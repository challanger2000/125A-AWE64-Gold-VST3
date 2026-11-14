// 125A P002 research-only ORIGINAL Creative Win95 VXD idle state capture.
// ReadProcessMemory only. No original driver, EMU8000 or guest modification.
// Discover real kernel virtual address from live 86Box x86 paging;
// NEVER assume an address observed in an earlier Win95 boot.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace {
constexpr std::size_t GuestBytes=64u*1024u*1024u, StateBytes=0x1592u;
constexpr std::uint32_t Bank=0x4d474d31u;
using State=std::array<std::uint8_t,StateBytes>;
using Ram=std::vector<std::uint8_t>;
std::uint16_t u16(const std::uint8_t* b) {
    return std::uint16_t(b[0])|(std::uint16_t(b[1])<<8);
}
std::uint32_t u32(const std::uint8_t* b) {
    return std::uint32_t(u16(b))|(std::uint32_t(u16(b+2))<<16);
}
void p16(std::uint8_t* b,std::uint16_t v) {b[0]=std::uint8_t(v);b[1]=std::uint8_t(v>>8);}
void p32(std::uint8_t* b,std::uint32_t v) {p16(b,std::uint16_t(v));p16(b+2,std::uint16_t(v>>16));}
void check(bool b,const char* e){if(!b)throw std::runtime_error(e);}
bool idle(const State& s){
    if(u32(s.data()+0x158e)!=Bank||u16(s.data()+6)!=32||
       u32(s.data()+0x18)!=32||u16(s.data()+0x12)!=0)return false;
    for(unsigned i=0;i<30;++i)
        if(u16(s.data()+0x42+0x20*i)!=0x0ffe)return false;
    return true;
}
bool virtualRead(const Ram& ram,std::uint32_t pd,std::uint32_t va,
                 std::uint8_t* dst,std::size_t left){
    if(ram.size()!=GuestBytes||(pd&4095u)||pd>GuestBytes-4096)return false;
    while(left){
        const auto pde=u32(ram.data()+pd+4u*((va>>22)&1023u));
        if(!(pde&1u))return false;
        std::uint64_t phys=0;
        if(pde&0x80u)phys=(pde&0xffc00000u)+(va&0x3fffffu);
        else{
            const auto pt=pde&0xfffff000u;
            if(pt>GuestBytes-4096)return false;
            const auto pte=u32(ram.data()+pt+4u*((va>>12)&1023u));
            if(!(pte&1u))return false;
            phys=(pte&0xfffff000u)+(va&4095u);
        }
        const auto n=std::min(left,std::size_t(4096u-(va&4095u)));
        if(phys+n>GuestBytes)return false;
        std::memcpy(dst,ram.data()+std::size_t(phys),n);
        dst+=n;left-=n;va+=std::uint32_t(n);
    }
    return true;
}
struct Hit {State state{};std::uint32_t va=0;};
struct Report{
    std::size_t markers=0, pageRefs=0, kernelRefs=0, parsed=0, busy=0;
    std::vector<Hit> hits;
};
Report scan(const Ram& ram){
    Report r{};
    if(ram.size()!=GuestBytes)return r;
    std::vector<std::uint32_t> anchors;
    for(std::size_t at=0;at+4<=ram.size();++at){
        if(ram[at]!='1'||ram[at+1]!='M'||ram[at+2]!='G'||ram[at+3]!='M')continue;
        ++r.markers;
        if(anchors.size()<128)anchors.push_back(std::uint32_t(at));
    }
    if(anchors.size()!=r.markers)return r; // fail closed on pathological RAM
    std::map<std::uint32_t,std::vector<std::uint32_t>> references;
    for(auto at:anchors)references.emplace(at&0xfffff000u,std::vector<std::uint32_t>{});
    for(std::uint32_t at=0;at+4u<=ram.size();at+=4u){
        const auto entry=u32(ram.data()+at);
        if(!(entry&1u))continue;
        auto it=references.find(entry&0xfffff000u);
        if(it!=references.end()&&it->second.size()<8192)
            it->second.push_back(at);
    }
    for(auto anchor:anchors){
        for(auto pteAt:references.at(anchor&0xfffff000u)){
            ++r.pageRefs;
            const auto pt=pteAt&0xfffff000u;
            const auto ti=(pteAt&4095u)/4u;
            // Reverse-map the physical original 1MGM page into a present
            // WIN95 kernel PDE/PTE, then read the *whole virtual image*.
            for(std::uint32_t pdAt=0;pdAt+4u<=ram.size();pdAt+=4u){
                const auto di=(pdAt&4095u)/4u;
                if(di<768)continue;
                const auto pde=u32(ram.data()+pdAt);
                if(!(pde&1u)||(pde&0x80u)||(pde&0xfffff000u)!=pt)continue;
                ++r.kernelRefs;
                const auto magic=(di<<22u)|(ti<<12u)|(anchor&4095u);
                if(magic<0xc000158eu)continue;
                const auto va=magic-0x158eu;
                if(va<0xc0000000u||va>0xffffffffu-StateBytes)continue;
                State s{};
                if(!virtualRead(ram,pdAt&0xfffff000u,va,s.data(),s.size()))continue;
                if(u32(s.data()+0x158e)!=Bank)continue;
                ++r.parsed;
                if(!idle(s)){++r.busy;continue;}
                bool duplicate=false;
                for(const auto& prior:r.hits)if(prior.state==s)duplicate=true;
                if(!duplicate)r.hits.push_back({s,va});
                if(r.hits.size()>1)return r;
            }
        }
    }
    return r;
}
void testState(State& s){
    s={};p32(s.data()+0x158e,Bank);p16(s.data()+6,32);
    p32(s.data()+0x18,32);
    for(unsigned i=0;i<30;++i)p16(s.data()+0x42+0x20*i,0x0ffe);
}
void testMap(Ram& ram,std::uint32_t pd,std::uint32_t pt,
             std::uint32_t va,std::uint32_t page1,std::uint32_t page2,const State& s){
    const auto di=(va>>22u)&1023u, ti=(va>>12u)&1023u;
    check(((va+StateBytes-1u)>>22u)==(va>>22u),"SELFTEST_PDE_CROSS");
    p32(ram.data()+pd+di*4u,pt|3u);
    p32(ram.data()+pt+ti*4u,page1|3u);
    p32(ram.data()+pt+(ti+1u)*4u,page2|3u);
    for(std::size_t i=0;i<StateBytes;++i){
        const auto addr=va+std::uint32_t(i);
        const auto page=(addr>>12u)==(va>>12u)?page1:page2;
        ram[page+(addr&4095u)]=s[i];
    }
}
int selftest(){
    Ram ram(GuestBytes);
    State source{};testState(source);
    constexpr std::uint32_t va=0xc30b09f0u; // NOT old fixed C11809F0
    testMap(ram,0x3000,0x6000,va,0x28000,0x71000,source);
    const auto first=scan(ram);
    check(first.hits.size()==1&&first.hits[0].va==va&&
          first.hits[0].state==source,"DYNAMIC_VA_DISPERSED_MEMORY_FAIL");
    // Same VXD mapped from another CR3: valid duplicate, not ambiguity.
    p32(ram.data()+0x4000+4u*((va>>22u)&1023u),0x6003u);
    check(scan(ram).hits.size()==1,"MULTIPLE_PAGE_DIRECTORIES_FAIL");
    const auto bankPhys=0x71000u+((va+0x158eu)&4095u);
    ram[bankPhys]^=1u;check(scan(ram).hits.empty(),"BAD_BANK_ACCEPTED");
    ram[bankPhys]^=1u;
    const auto voicePhys=0x28000u+((va+0x42u)&4095u);
    ram[voicePhys]^=1u;check(scan(ram).hits.empty(),"BAD_VOICE_ACCEPTED");
    ram[voicePhys]^=1u;
    const auto lastPte=0x6000u+4u*(((va>>12u)&1023u)+1u);
    p32(ram.data()+lastPte,0u);check(scan(ram).hits.empty(),"ABSENT_PTE_ACCEPTED");
    p32(ram.data()+lastPte,0x71003u);
    std::memcpy(ram.data()+0x200000,source.data(),StateBytes);
    check(scan(ram).hits.size()==1,"ORPHAN_MARKER_ACCEPTED");
    State other=source;other[0x1000]^=1u;
    testMap(ram,0x5000,0x9000,0xc4400800u,0xa1000,0xb2000,other);
    check(scan(ram).hits.size()==2,"AMBIGUOUS_SOURCE_NOT_REJECTED");
    std::cout<<"P002_VXD_DYNAMIC_REVERSE_PAGING_SELFTEST_PASS"
             <<" nonfixed_address=PASS physical_pages=NONCONTIGUOUS"
             <<" mutations=4 ambiguous_source=REJECTED\n";
    return 0;
}
#ifdef _WIN32
struct Region{std::uint64_t start=0,end=0,allocation=0;};
std::vector<Region> regions(HANDLE p){
    std::vector<Region> out;
    std::uint64_t cursor=0;
    for(unsigned i=0;i<500000;++i){
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQueryEx(p,reinterpret_cast<const void*>(cursor),&m,sizeof(m)))break;
        const auto b=reinterpret_cast<std::uint64_t>(m.BaseAddress);
        const auto n=std::uint64_t(m.RegionSize);
        if(!n||b+n<=cursor)break;
        if(m.State==MEM_COMMIT&&(m.Type==MEM_PRIVATE||m.Type==MEM_MAPPED)&&
           !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&m.Protect)
            out.push_back({b,b+n,reinterpret_cast<std::uint64_t>(m.AllocationBase)});
        cursor=b+n;
    }
    return out;
}
std::vector<std::uint64_t> bases(const std::vector<Region>& r){
    std::map<std::uint64_t,std::vector<Region>> groups;
    for(const auto& x:r)groups[x.allocation].push_back(x);
    std::set<std::uint64_t> result;
    for(auto& pair:groups){
        auto& g=pair.second;
        std::sort(g.begin(),g.end(),[](const Region& a,const Region& b){return a.start<b.start;});
        for(std::size_t i=0;i<g.size();++i){
            auto start=g[i].start,end=g[i].end;
            if(start&4095u)continue;
            for(std::size_t j=i+1;j<g.size()&&g[j].start<=end;++j)end=std::max(end,g[j].end);
            if(end-start>=GuestBytes)result.insert(start);
        }
    }
    return {result.begin(),result.end()};
}
int capture(DWORD pid,const std::string& path){
    HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    if(!p)throw std::runtime_error("READ_ONLY_OPENPROCESS_DENIED");
    const auto rr=regions(p);
    const auto bb=bases(rr);
    std::size_t reads=0,markers=0,valid=0,busy=0;
    std::vector<Hit> matches;
    for(auto base:bb){
        Ram memory(GuestBytes);
        SIZE_T got=0;
        if(!ReadProcessMemory(p,reinterpret_cast<const void*>(base),
                              memory.data(),memory.size(),&got)||got!=memory.size())continue;
        ++reads;
        auto r=scan(memory);
        markers+=r.markers;valid+=r.parsed;busy+=r.busy;
        for(const auto& x:r.hits){
            bool duplicate=false;
            for(const auto& y:matches)if(x.state==y.state)duplicate=true;
            if(!duplicate)matches.push_back(x);
        }
        if(matches.size()>1)break;
    }
    std::cout<<"P002_VXD_CAPTURE_DIAGNOSTICS committed_regions="<<rr.size()
             <<" possible_guest_ram_bases="<<bb.size()
             <<" successful_64MiB_reads="<<reads
             <<" physical_1MGM_markers="<<markers
             <<" mapped_original_state_candidates="<<valid
             <<" non_idle_candidates="<<busy
             <<" distinct_idle_states="<<matches.size()<<"\n";
    if(matches.size()!=1){CloseHandle(p);return 4;}
    // Second independent live read to reject stale/transient device data.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bool stable=false;
    for(auto base:bb){
        Ram again(GuestBytes);SIZE_T got=0;
        if(!ReadProcessMemory(p,reinterpret_cast<const void*>(base),
                              again.data(),again.size(),&got)||got!=again.size())continue;
        auto r=scan(again);
        if(r.hits.size()==1&&r.hits[0].state==matches[0].state){stable=true;break;}
    }
    CloseHandle(p);
    if(!stable){std::cerr<<"P002_GUEST_SNAPSHOT_UNSTABLE\n";return 5;}
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,
                            CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("OUTPUT_FILE_EXISTS_OR_DENIED");
    DWORD done=0;
    bool ok=WriteFile(file,matches[0].state.data(),DWORD(StateBytes),&done,0)&&
            done==StateBytes&&FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok){DeleteFileA(path.c_str());throw std::runtime_error("SHORT_WRITE");}
    std::cout<<"P002_ORIGINAL_CREATIVE_VXD_IDLE_CAPTURE_PASS"
             <<" bytes=5522 discovered_win95_virtual=0x"<<std::hex
             <<matches[0].va<<std::dec
             <<" stable_double_capture=TRUE\n";
    return 0;
}
#endif
}
int main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--self-test")return selftest();
#ifdef _WIN32
        if(argc==5&&std::string(argv[1])=="--pid"&&std::string(argv[3])=="--output"){
            const auto pid=std::stoull(argv[2]);
            if(pid==0||pid>std::numeric_limits<DWORD>::max())return 2;
            return capture(DWORD(pid),argv[4]);
        }
#endif
        std::cerr<<"USAGE --self-test OR --pid <86box-pid> --output <new-path>\n";
        return 2;
    }catch(const std::exception& e){
        std::cerr<<"P002_SOURCE_VXD_CAPTURE_NO_GO "<<e.what()<<"\n";return 1;
    }
}
