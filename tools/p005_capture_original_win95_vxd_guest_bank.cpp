// P005 -- READ-ONLY ORIGINAL Win95 Creative VXD guest bank capture.
// Attach only to the user's original private 86Box Win95 instance.
// ReadProcessMemory: no guest writes, hooks, Creative binary patches,
// synthetic preset rows, copied ROM, copied VXD, or DSP replacements.
//
// Output is a PRIVATE research artifact, not a verified release bank.
// Independent original Win95 driver/disk/source hashes, bank/session
// labels, and a double-snapshot provenance check are still required.
// Never commit, distribute, or upload a produced binary to public GitHub.
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
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
constexpr std::size_t GuestBytes=64u*1024u*1024u;
constexpr std::size_t DeviceBytes=0x1592u, PointerOffset=0x6cau;
constexpr std::size_t Slots=288u, RowBytes=26u, MaxRows=65536u;
constexpr std::uint32_t OriginalMgm=0x4d474d31u;
using Ram=std::vector<std::uint8_t>;
using Device=std::array<std::uint8_t,DeviceBytes>;
std::uint16_t u16(const std::uint8_t* p) noexcept {
    return std::uint16_t{p[0]} | (std::uint16_t{p[1]}<<8u);
}
std::uint32_t u32(const std::uint8_t* p) noexcept {
    return std::uint32_t{u16(p)} | (std::uint32_t{u16(p+2)}<<16u);
}
void put32(std::uint8_t* p,std::uint32_t value) noexcept {
    p[0]=static_cast<std::uint8_t>(value);
    p[1]=static_cast<std::uint8_t>(value>>8u);
    p[2]=static_cast<std::uint8_t>(value>>16u);
    p[3]=static_cast<std::uint8_t>(value>>24u);
}
void require(bool ok,const char* what) {
    if(!ok)throw std::runtime_error(what);
}
// Walk the actual Windows-95 x86 paging structures in a single 64 MiB
// private 86Box RAM snapshot. Page table physical pointers must be
// present and in bounds; both 4 KiB and 4 MiB PDEs are supported.
bool virtualRead(const Ram& ram,std::uint32_t cr3,
                 std::uint64_t virtualAddress,std::uint8_t* out,
                 std::size_t count) noexcept {
    constexpr std::uint64_t Limit=std::uint64_t{1}<<32u;
    if(ram.size()!=GuestBytes || (cr3&4095u) ||
       cr3>GuestBytes-4096u || virtualAddress+count>Limit)
        return false;
    std::size_t done=0;
    while(done<count) {
        const auto va=static_cast<std::uint32_t>(virtualAddress+done);
        const auto pde=u32(ram.data()+cr3+4u*((va>>22u)&1023u));
        if(!(pde&1u))return false;
        std::uint64_t phys=0;
        if(pde&0x80u) {
            phys=std::uint64_t{pde&0xffc00000u}+(va&0x3fffffu);
        } else {
            const auto pt=pde&0xfffff000u;
            if(pt>GuestBytes-4096u)return false;
            const auto pte=u32(ram.data()+pt+4u*((va>>12u)&1023u));
            if(!(pte&1u))return false;
            phys=std::uint64_t{pte&0xfffff000u}+(va&4095u);
        }
        const auto n=std::min(count-done,
            std::size_t{4096u-(va&4095u)});
        if(phys+n>GuestBytes)return false;
        std::memcpy(out+done,ram.data()+static_cast<std::size_t>(phys),n);
        done+=n;
    }
    return true;
}
struct Evidence {
    std::uint32_t guestDeviceAddress=0,cr3=0;
    Device device{};
    // Exact ORIGINAL VXD 26-byte records including 0xFFFF row, keyed
    // only by the original 32-bit guest virtual pointer. Aliases share.
    std::map<std::uint32_t,std::vector<std::uint8_t>> rows{};
    std::uint32_t nonzeroSlotPointers=0;
    bool sameSourceBytes(const Evidence& other) const noexcept {
        return guestDeviceAddress==other.guestDeviceAddress &&
               device==other.device && rows==other.rows;
    }
};
bool probe(const Ram& ram,std::uint32_t cr3,std::uint32_t deviceVa,
           Evidence& out) {
    Evidence next{};
    next.guestDeviceAddress=deviceVa;
    next.cr3=cr3;
    if(!virtualRead(ram,cr3,deviceVa,next.device.data(),DeviceBytes))
        return false;
    if(u32(next.device.data()+0x158eu)!=OriginalMgm ||
       u16(next.device.data()+6u)!=32u)
        return false;
    std::size_t totalRows=0u,sourceActiveRows=0u;
    for(std::size_t slot=0u;slot<Slots;++slot) {
        const auto ptr=u32(next.device.data()+PointerOffset+4u*slot);
        if(ptr==0u)continue;
        ++next.nonzeroSlotPointers;
        if(next.rows.contains(ptr))continue;
        // Real Creative bank slots can point to a bank HEADER containing
        // further guest VAs. The first points to the 26-byte preset table.
        // Preserve the original root VA and all original bytes between
        // root and the final aligned child-table sentinel.
        std::array<std::uint8_t,28> header{};
        if(!virtualRead(ram,cr3,ptr,header.data(),header.size()))
            return false;
        const auto child=u32(header.data());
        const auto second=u32(header.data()+4u);
        bool ordered=true;
        for(std::size_t i=0;i<6u;++i)
            if(u32(header.data()+4u*i)>=u32(header.data()+4u*(i+1u)))
                ordered=false;
        const bool candidateHeader=child>ptr && child-ptr>=28u &&
            child-ptr<=4096u && child<second &&
            (child&0xff000000u)==(ptr&0xff000000u);
        if(candidateHeader && !ordered)return false;
        const bool indirect=candidateHeader;
        if(indirect) {
            const auto prefix=std::size_t{child-ptr};
            // The second authentic bank-header pointer gives the exact
            // end of the first program list. Do not scan until a later
            // coincidental 0xFFFF in another bank allocation.
            const auto span=std::uint64_t{second}-child;
            if(span<RowBytes || span%RowBytes!=0u ||
               span/RowBytes>MaxRows ||
               span/RowBytes>MaxRows-totalRows)
                return false;
            const auto rowCount=static_cast<std::size_t>(span/RowBytes);
            if(rowCount>MaxRows-sourceActiveRows) return false;
            const auto byteCount=prefix+static_cast<std::size_t>(span);
            std::vector<std::uint8_t> originalBytes(byteCount);
            if(!virtualRead(ram,cr3,ptr,originalBytes.data(),byteCount))
                return false;
            for(std::size_t i=0u;i<rowCount;++i) {
                const auto marker=u16(originalBytes.data()+prefix+i*RowBytes);
                const bool last=i+1u==rowCount;
                if((marker==0xffffu)!=last)return false;
                if(!last && marker!=0u)++sourceActiveRows;
            }
            totalRows+=rowCount;
            next.rows.emplace(ptr,std::move(originalBytes));
            continue;
        }
        std::vector<std::uint8_t> originalBytes{};
        bool terminated=false;
        for(std::size_t index=0u;index<MaxRows;++index) {
            if(totalRows>=MaxRows)return false;
            const auto guestAddr=std::uint64_t{ptr}+index*RowBytes;
            std::array<std::uint8_t,RowBytes> row{};
            if(!virtualRead(ram,cr3,guestAddr,row.data(),row.size()))
                return false;
            originalBytes.insert(originalBytes.end(),row.begin(),row.end());
            ++totalRows;
            const auto marker=u16(row.data());
            if(marker==0xffffu) {
                // A non-NULL source pointer CAN reference just a
                // terminal FFFF row. Preserve that original empty
                // list instead of inventing a playable PHDR preset.
                terminated=true;
                break;
            }
            if(marker!=0u)++sourceActiveRows;
        }
        if(!terminated)return false;
        next.rows.emplace(ptr,std::move(originalBytes));
    }
    if(next.rows.empty() || next.nonzeroSlotPointers==0u ||
       sourceActiveRows==0u)
        return false; // no loaded/active original source program evidence
    out=std::move(next);
    return true;
}
struct Candidate {
    Evidence capture{};
};
// Discover the live device's NONFIXED virtual address by reversing the
// actual 86Box x86 physical 1MGM marker through present Win95 PTE/PDEs.
// Do not use a previous session's hard-coded VXD base or CR3.
std::vector<Candidate> discover(const Ram& ram) {
    std::vector<Candidate> results{};
    if(ram.size()!=GuestBytes)return results;
    std::map<std::uint32_t,std::vector<std::uint32_t>> markers{};
    std::size_t found=0u;
    for(std::size_t off=0u;off+4u<=ram.size();++off) {
        if(ram[off]=='1' && ram[off+1u]=='M' &&
           ram[off+2u]=='G' && ram[off+3u]=='M') {
            if(++found>128u)return {};
            markers[static_cast<std::uint32_t>(off)&0xfffff000u]
                .push_back(static_cast<std::uint32_t>(off));
        }
    }
    if(markers.empty())return {};
    // Locate possible PTE entries which map a physical marker page.
    // No guest memory modifications or privileged guest instructions.
    std::vector<std::uint32_t> pteLocations{};
    for(std::size_t off=0u;off+4u<=ram.size();off+=4u) {
        const auto value=u32(ram.data()+off);
        if((value&1u) && markers.contains(value&0xfffff000u)) {
            if(pteLocations.size()>=8192u)return {};
            pteLocations.push_back(static_cast<std::uint32_t>(off));
        }
    }
    for(auto pteAt:pteLocations) {
        const auto pte=u32(ram.data()+pteAt);
        const auto page=pte&0xfffff000u;
        const auto pt=pteAt&0xfffff000u;
        const auto ti=(pteAt&4095u)/4u;
        for(std::size_t pdeAt=0u;pdeAt+4u<=ram.size();pdeAt+=4u) {
            const auto di=(pdeAt&4095u)/4u;
            if(di<768u)continue;
            const auto pde=u32(ram.data()+pdeAt);
            if(!(pde&1u) || (pde&0x80u) ||
               (pde&0xfffff000u)!=pt)continue;
            // A correct present x86 mapping of the candidate physical
            // marker page must have a matching guest virtual address.
            for(auto marker:markers[page]) {
                const auto va64=(std::uint64_t{di}<<22u)|
                    (std::uint64_t{ti}<<12u)|(marker&4095u);
                if(va64<0xc000158eu)continue;
                const auto deviceVa=va64-0x158eu;
                if(deviceVa<0xc0000000ull ||
                   deviceVa>0xffffffffull-DeviceBytes)continue;
                Evidence evidence{};
                const auto cr3=static_cast<std::uint32_t>(
                    pdeAt&~std::size_t{4095u});
                if(!probe(ram,cr3,static_cast<std::uint32_t>(deviceVa),
                          evidence))continue;
                bool duplicate=false;
                for(auto const& previous:results) {
                    if(evidence.sameSourceBytes(previous.capture)) {
                        duplicate=true;break;
                    }
                }
                if(!duplicate)results.push_back({std::move(evidence)});
                if(results.size()>1u)return results; // fail closed
            }
        }
    }
    return results;
}
void put16(std::uint8_t* dst,std::uint16_t x) noexcept {
    dst[0]=static_cast<std::uint8_t>(x);
    dst[1]=static_cast<std::uint8_t>(x>>8u);
}
void mapRegion(Ram& ram,std::uint32_t cr3,std::uint32_t pt,
               std::uint32_t virtualAddress,std::uint32_t physicalPage,
               const std::uint8_t* content,std::size_t count) {
    require(count>0u && (virtualAddress&4095u)+count<=4096u,
            "SYNTHETIC_REGION_CROSSES_PAGE");
    const auto di=(virtualAddress>>22u)&1023u;
    const auto ti=(virtualAddress>>12u)&1023u;
    put32(ram.data()+cr3+4u*di,pt|3u);
    put32(ram.data()+pt+4u*ti,physicalPage|3u);
    std::memcpy(ram.data()+physicalPage+(virtualAddress&4095u),
                content,count);
}
int selfTest() {
    Ram ram(GuestBytes);
    constexpr std::uint32_t cr3=0x3000u,pt=0x6000u;
    constexpr std::uint32_t va=0xc30b09f0u;
    Device source{};
    put32(source.data()+0x158eu,OriginalMgm);
    put16(source.data()+6u,32u);
    constexpr std::uint32_t list=0xc30c0ff0u;
    put32(source.data()+PointerOffset+5u*4u,list);
    put32(source.data()+PointerOffset+6u*4u,list); // original alias
    const auto split=4096u-(va&4095u);
    mapRegion(ram,cr3,pt,va,0x28000u,source.data(),split);
    mapRegion(ram,cr3,pt,va+static_cast<std::uint32_t>(split),
              0x71000u,source.data()+split,DeviceBytes-split);
    std::array<std::uint8_t,2u*RowBytes> rows{};
    put16(rows.data(),1u);
    rows[2]=7u;rows[3]=1u;
    put16(rows.data()+RowBytes,0xffffu);
    const auto first=4096u-(list&4095u);
    mapRegion(ram,cr3,pt,list,0x85000u,rows.data(),first);
    mapRegion(ram,cr3,pt,list+static_cast<std::uint32_t>(first),
              0x92000u,rows.data()+first,rows.size()-first);
    Evidence check{};
    require(probe(ram,cr3,va,check),"SYNTHETIC_CAPTURE_FAILED");
    require(check.nonzeroSlotPointers==2u && check.rows.size()==1u,
            "ORIGINAL_ALIAS_NOT_PRESERVED");
    require(check.rows.at(list).size()==52u &&
            check.rows.at(list)[2u]==7u,"VXD_ROW_BYTES_CHANGED");
    auto discovered=discover(ram);
    require(discovered.size()==1u &&
            discovered[0].capture.sameSourceBytes(check),
            "GUEST_DYNAMIC_VXD_DISCOVERY_FAILED");
    // Independently exercise the real Win95 indirect bank layout:
    // root header -> first child VA -> second child VA is the exact
    // end of a 26-byte-aligned list, including the FFFF row.
    Ram indirectRam(GuestBytes);
    Device indirectDevice=source;
    put32(indirectDevice.data()+PointerOffset+5u*4u,list);
    put32(indirectDevice.data()+PointerOffset+6u*4u,list);
    mapRegion(indirectRam,cr3,pt,va,0x28000u,
              indirectDevice.data(),split);
    mapRegion(indirectRam,cr3,pt,va+static_cast<std::uint32_t>(split),
              0x71000u,indirectDevice.data()+split,DeviceBytes-split);
    constexpr std::size_t Prefix=76u;
    std::array<std::uint8_t,Prefix+2u*RowBytes> nested{};
    put32(nested.data(),list+static_cast<std::uint32_t>(Prefix));
    put32(nested.data()+4u,
          list+static_cast<std::uint32_t>(Prefix+2u*RowBytes));
    for(std::size_t i=2u;i<7u;++i)
        put32(nested.data()+4u*i,
              list+static_cast<std::uint32_t>(Prefix+2u*RowBytes+
                        (i-1u)*100u));
    put16(nested.data()+Prefix,1u);
    nested[Prefix+2u]=7u;
    nested[Prefix+3u]=1u;
    put16(nested.data()+Prefix+RowBytes,0xffffu);
    mapRegion(indirectRam,cr3,pt,list,0x85000u,
              nested.data(),first);
    mapRegion(indirectRam,cr3,pt,list+static_cast<std::uint32_t>(first),
              0x92000u,nested.data()+first,nested.size()-first);
    Evidence verifiedNested{};
    require(probe(indirectRam,cr3,va,verifiedNested),
            "INDIRECT_BANK_HEADER_CAPTURE_FAILED");
    require(verifiedNested.rows.size()==1u &&
            verifiedNested.rows.at(list).size()==nested.size(),
            "INDIRECT_BANK_HEADER_LENGTH_NOT_BOUNDED");
    // Early FFFF within the child list must never hide later records.
    put16(indirectRam.data()+0x92000u+Prefix-first,0xffffu);
    Evidence rejectedNested{};
    require(!probe(indirectRam,cr3,va,rejectedNested),
            "EARLY_INDIRECT_SENTINEL_ACCEPTED");
    put16(indirectRam.data()+0x92000u+Prefix-first,1u);
    // The final row must remain the end marker.
    put16(indirectRam.data()+0x92000u+Prefix+RowBytes-first,0u);
    require(!probe(indirectRam,cr3,va,rejectedNested),
            "MISSING_INDIRECT_SENTINEL_ACCEPTED");
    // Missing second page: not a permitted original row terminator.
    const auto ti=((list+static_cast<std::uint32_t>(first))>>12u)&1023u;
    put32(ram.data()+pt+4u*ti,0u);
    Evidence missing{};
    require(!probe(ram,cr3,va,missing),"MISSING_GUEST_PAGE_ACCEPTED");
    put32(ram.data()+pt+4u*ti,0x92003u);
    // Corrupt the REAL source terminator. Never scan beyond the
    // provided rows to synthesize a source FFFF marker.
    put16(ram.data()+0x92000u+(RowBytes-first),0u);
    require(!probe(ram,cr3,va,missing),"MISSING_ORIGINAL_SENTINEL_ACCEPTED");
    std::cout<<"P005_ORIGINAL_WIN95_GUEST_VXD_BANK_CAPTURE_SELFTEST_PASS "
             <<"same_bank_alias=TRUE split_page=TRUE no_fabricated_rows=TRUE "
             <<"dynamic_va=TRUE\n";
    return 0;
}
#ifdef _WIN32
struct Region {
    std::uint64_t begin=0,end=0,allocation=0;
};
std::vector<Region> regions(HANDLE p) {
    std::vector<Region> out{};
    std::uint64_t cursor=0u;
    for(unsigned i=0;i<500000u;++i) {
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQueryEx(p,reinterpret_cast<const void*>(cursor),
                           &m,sizeof(m)))break;
        const auto begin=reinterpret_cast<std::uint64_t>(m.BaseAddress);
        const auto size=std::uint64_t{m.RegionSize};
        if(size==0u || begin+size<=cursor)break;
        if(m.State==MEM_COMMIT &&
           (m.Type==MEM_PRIVATE || m.Type==MEM_MAPPED) &&
           !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && m.Protect)
            out.push_back({begin,begin+size,
                reinterpret_cast<std::uint64_t>(m.AllocationBase)});
        cursor=begin+size;
    }
    return out;
}
std::vector<std::uint64_t> bases(const std::vector<Region>& r) {
    std::map<std::uint64_t,std::vector<Region>> groups{};
    for(auto const& x:r)groups[x.allocation].push_back(x);
    std::set<std::uint64_t> found{};
    for(auto& [allocation,g]:groups) {
        (void)allocation;
        std::sort(g.begin(),g.end(),[](auto const& a,auto const& b){
            return a.begin<b.begin;
        });
        for(std::size_t i=0u;i<g.size();++i) {
            auto start=g[i].begin,end=g[i].end;
            if(start&4095u)continue;
            for(std::size_t j=i+1u;j<g.size() && g[j].begin<=end;++j)
                end=std::max(end,g[j].end);
            if(end-start>=GuestBytes)found.insert(start);
        }
    }
    return {found.begin(),found.end()};
}
void append32(std::vector<std::uint8_t>& b,std::uint32_t x) {
    const auto start=b.size();b.resize(start+4u);
    put32(b.data()+start,x);
}
std::vector<std::uint8_t> encode(const Evidence& e) {
    std::vector<std::uint8_t> output{};
    // Private exact-byte format; no host struct padding, no pointers,
    // no guessed source region. "125AVXD1" (8 ASCII bytes).
    constexpr std::array<std::uint8_t,8> magic{
        '1','2','5','A','V','X','D','1'};
    output.insert(output.end(),magic.begin(),magic.end());
    append32(output,1u); // version
    append32(output,e.guestDeviceAddress);
    append32(output,e.cr3);
    append32(output,static_cast<std::uint32_t>(DeviceBytes));
    append32(output,static_cast<std::uint32_t>(Slots));
    append32(output,static_cast<std::uint32_t>(e.rows.size()));
    append32(output,e.nonzeroSlotPointers);
    output.insert(output.end(),e.device.begin(),e.device.end());
    for(auto const& [ptr,bytes]:e.rows) {
        append32(output,ptr);
        append32(output,static_cast<std::uint32_t>(bytes.size()));
        output.insert(output.end(),bytes.begin(),bytes.end());
    }
    return output;
}
int capture(DWORD pid,const std::string& path) {
    HANDLE p=OpenProcess(
        PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    if(!p)throw std::runtime_error("READ_ONLY_86BOX_ACCESS_DENIED");
    const auto allRegions=regions(p);
    const auto candidates=bases(allRegions);
    std::size_t validRamReads=0u;
    std::vector<Evidence> sources{};
    for(auto base:candidates) {
        Ram first(GuestBytes);SIZE_T n=0;
        if(!ReadProcessMemory(p,reinterpret_cast<const void*>(base),
             first.data(),first.size(),&n)||n!=first.size())continue;
        ++validRamReads;
        const auto result=discover(first);
        if(result.size()!=1u)continue;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Ram second(GuestBytes);SIZE_T m=0;
        if(!ReadProcessMemory(p,reinterpret_cast<const void*>(base),
             second.data(),second.size(),&m)||m!=second.size())continue;
        const auto confirm=discover(second);
        if(confirm.size()!=1u ||
           !result[0].capture.sameSourceBytes(confirm[0].capture))
            continue;
        bool duplicate=false;
        for(auto const& prior:sources)
            if(prior.sameSourceBytes(result[0].capture))
                duplicate=true;
        if(!duplicate)sources.push_back(result[0].capture);
        if(sources.size()>1u)break;
    }
    CloseHandle(p);
    std::cout<<"P005_WIN95_READ_ONLY_GUEST_CAPTURE "
             <<"candidate_ram_allocations="<<candidates.size()
             <<" readable_64mib_images="<<validRamReads
             <<" stable_banks="<<sources.size()<<"\n";
    if(sources.size()!=1u)return 4; // NEVER guess between two VXD banks
    auto encoded=encode(sources.front());
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,
                   CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)
        throw std::runtime_error("PRIVATE_OUTPUT_MUST_BE_NEW_FILE");
    DWORD written=0u;
    const bool ok=encoded.size()<=
        std::numeric_limits<DWORD>::max() &&
        WriteFile(file,encoded.data(),
                  static_cast<DWORD>(encoded.size()),&written,nullptr) &&
        written==encoded.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if(!ok) {
        DeleteFileA(path.c_str());
        throw std::runtime_error("PRIVATE_GUEST_CAPTURE_SHORT_WRITE");
    }
    std::cout<<"P005_SOURCE_VXD_GUEST_BANK_PRIVATE_CAPTURE_WRITTEN "
             <<"device_bytes="<<DeviceBytes
             <<" nonzero_pointers="<<sources[0].nonzeroSlotPointers
             <<" unique_original_row_lists="<<sources[0].rows.size()
             <<" file_bytes="<<encoded.size()
             <<" original_driver_sha_and_session_provenance="
             <<"STILL_REQUIRED\n";
    return 0;
}
#endif
} // namespace

int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--self-test")
            return selfTest();
#ifdef _WIN32
        if(argc==5 && std::string(argv[1])=="--pid" &&
           std::string(argv[3])=="--output") {
            const auto pid=std::stoull(argv[2]);
            if(pid==0u || pid>std::numeric_limits<DWORD>::max())
                return 2;
            return capture(static_cast<DWORD>(pid),argv[4]);
        }
#endif
        std::cerr<<"USAGE: --self-test OR --pid <86Box-pid> "
                    "--output <new-private-local-path>\n";
        return 2;
    }catch(std::exception const& e) {
        std::cerr<<"P005_ORIGINAL_GUEST_CAPTURE_NO_GO "<<e.what()<<"\n";
        return 1;
    }
}
