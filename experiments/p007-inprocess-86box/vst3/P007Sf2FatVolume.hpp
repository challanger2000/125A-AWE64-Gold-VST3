#pragma once
// P007AU: an ORIGINAL Windows-95/Creative guest sees the user's SF2 through
// a private, separate 64-MiB FAT16 IDE SLAVE volume. No external host
// programs, no writes to the original 512-MiB Win95 system drive, no
// replacement EMU8000 sample engine and no RT-thread file operations.
// The guest's actual AWEMAN32/AWEManager must acknowledge the load.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
namespace p007::sf2_volume {
namespace fs=std::filesystem;
inline constexpr std::uint32_t bytesPerSector=512u;
inline constexpr std::uint32_t sectorsPerCluster=8u;
inline constexpr std::uint32_t cylinders=130u;
inline constexpr std::uint32_t heads=16u;
inline constexpr std::uint32_t sectorsPerTrack=63u;
inline constexpr std::uint32_t totalSectors=cylinders*heads*sectorsPerTrack;
inline constexpr std::uint32_t partitionStart=63u;
inline constexpr std::uint32_t partitionSectors=totalSectors-partitionStart;
inline constexpr std::uint32_t reservedSectors=1u;
inline constexpr std::uint32_t fatSectors=64u;
inline constexpr std::uint32_t rootEntries=512u;
inline constexpr std::uint32_t rootSectors=rootEntries*32u/bytesPerSector;
inline constexpr std::uint32_t dataStart=partitionStart+reservedSectors+
                                         2u*fatSectors+rootSectors;
inline constexpr std::uint32_t clusters=(totalSectors-dataStart)/sectorsPerCluster;
inline constexpr std::uint64_t imageBytes=std::uint64_t(totalSectors)*bytesPerSector;
inline constexpr std::uint32_t maxSf2Bytes=28u*1024u*1024u;
static_assert(clusters==16352u && clusters+2u<fatSectors*bytesPerSector/2u);
static_assert(imageBytes==67092480u);
inline void le16(std::uint8_t* p,std::uint16_t x) noexcept {
    p[0]=static_cast<std::uint8_t>(x);
    p[1]=static_cast<std::uint8_t>(x>>8u);
}
inline void le32(std::uint8_t* p,std::uint32_t x) noexcept {
    for(unsigned j=0;j<4;++j)p[j]=static_cast<std::uint8_t>(x>>(8u*j));
}
inline std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|
           (std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);
}
inline bool build(const fs::path& target,const fs::path& soundFont,
                  std::string& error) noexcept {
    try {
        std::error_code ec;
        if(!fs::is_regular_file(soundFont,ec)) {
            error="Selected SF2 file is missing";return false;
        }
        const auto size=fs::file_size(soundFont,ec);
        if(ec||size<96u||size>maxSf2Bytes) {
            error="SF2 must be RIFF/sfbk, between 96 bytes and 28 MiB";
            return false;
        }
        std::ifstream source(soundFont,std::ios::binary);
        std::array<std::uint8_t,12> head{};
        if(!source.read(reinterpret_cast<char*>(head.data()),head.size())||
           std::memcmp(head.data(),"RIFF",4u)!=0 ||
           std::memcmp(head.data()+8u,"sfbk",4u)!=0 ||
           read32(head.data()+4u)!=size-8u) {
            error="Selected SF2 is not a complete RIFF/sfbk file";return false;
        }
        source.clear();source.seekg(0);
        const std::uint32_t needed=static_cast<std::uint32_t>(
            (size+sectorsPerCluster*bytesPerSector-1u)/
            (sectorsPerCluster*bytesPerSector));
        if(needed==0u||needed>clusters) {
            error="SF2 exceeds FAT16 guest-disk capacity";return false;
        }
        fs::create_directories(target.parent_path(),ec);
        if(ec) {error="Cannot create private SF2 working folder";return false;}
        fs::path stage=target;stage+=L".p007-staging";
        fs::path backup=target;backup+=L".p007-backup";
        if(fs::exists(stage,ec)||fs::exists(backup,ec)) {
            error="Unfinished private SF2-disk transaction exists; refusing overwrite";
            return false;
        }
        std::ofstream out(stage,std::ios::binary|std::ios::trunc);
        if(!out) {error="Cannot create private guest SF2 disk";return false;}
        // Create a sparse, fixed-geometry IDE image, never a 512-MiB copy.
        out.seekp(static_cast<std::streamoff>(imageBytes-1u));
        out.put('\0');
        std::array<std::uint8_t,512> mbr{};
        // LBA 63 is CHS cylinder 0, HEAD 1, sector 1 at 16H/63S.
        // An earlier MBR wrote head 0 (LBA 0) while the LBA start was
        // 63, confusing Win95 geometry checks on its second IDE disk.
        mbr[446+1]=1u; mbr[446+2]=1u; mbr[446+3]=0u;
        mbr[446+4]=0x06u; // FAT16 CHS
        mbr[446+5]=15u; mbr[446+6]=63u; mbr[446+7]=129u;
        le32(mbr.data()+446u+8u,partitionStart);
        le32(mbr.data()+446u+12u,partitionSectors);
        mbr[510]=0x55u;mbr[511]=0xaau;
        out.seekp(0);out.write(reinterpret_cast<char*>(mbr.data()),mbr.size());
        std::array<std::uint8_t,512> boot{};
        boot[0]=0xebu;boot[1]=0x3cu;boot[2]=0x90u;
        std::memcpy(boot.data()+3,"MSWIN4.1",8u);
        le16(boot.data()+11,bytesPerSector);
        boot[13]=sectorsPerCluster;
        le16(boot.data()+14,reservedSectors);
        boot[16]=2u;le16(boot.data()+17,rootEntries);
        le16(boot.data()+19,0u);
        boot[21]=0xf8u;le16(boot.data()+22,fatSectors);
        le16(boot.data()+24,sectorsPerTrack);
        le16(boot.data()+26,heads);
        le32(boot.data()+28,partitionStart);
        le32(boot.data()+32,partitionSectors);
        boot[36]=0x80u;boot[38]=0x29u;
        le32(boot.data()+39,0x125a007u);
        std::memcpy(boot.data()+43,"125A SF2    ",11u);
        std::memcpy(boot.data()+54,"FAT16   ",8u);
        boot[510]=0x55u;boot[511]=0xaau;
        out.seekp(static_cast<std::streamoff>(partitionStart)*bytesPerSector);
        out.write(reinterpret_cast<char*>(boot.data()),boot.size());
        std::array<std::uint8_t,fatSectors*bytesPerSector> fat{};
        le16(fat.data(),0xfff8u);le16(fat.data()+2u,0xffffu);
        for(std::uint32_t n=0;n<needed;++n)
            le16(fat.data()+2u*(2u+n),
                 n+1u==needed?0xffffu:static_cast<std::uint16_t>(3u+n));
        for(unsigned i=0;i<2u;++i) {
            const auto at=partitionStart+reservedSectors+i*fatSectors;
            out.seekp(static_cast<std::streamoff>(at)*bytesPerSector);
            out.write(reinterpret_cast<char*>(fat.data()),fat.size());
        }
        std::array<std::uint8_t,32> entry{};
        std::memcpy(entry.data(),"P007FONTSF2",11u);
        entry[11]=0x20u;
        le16(entry.data()+26u,2u);
        le32(entry.data()+28u,static_cast<std::uint32_t>(size));
        out.seekp(static_cast<std::streamoff>(
            partitionStart+reservedSectors+2u*fatSectors)*bytesPerSector);
        out.write(reinterpret_cast<char*>(entry.data()),entry.size());
        out.seekp(static_cast<std::streamoff>(dataStart)*bytesPerSector);
        std::array<char,32768> buf{};
        std::uint64_t remaining=size;
        while(remaining!=0u) {
            const auto bytes=static_cast<std::streamsize>(
                std::min<std::uint64_t>(remaining,buf.size()));
            if(!source.read(buf.data(),bytes)||!out.write(buf.data(),bytes)) {
                error="SF2 guest disk copy incomplete";out.close();
                fs::remove(stage,ec);return false;
            }
            remaining-=static_cast<std::uint64_t>(bytes);
        }
        out.flush();
        if(!out) {error="Cannot flush guest SF2 disk";out.close();
                  fs::remove(stage,ec);return false;}
        out.close();source.close();
        if(fs::file_size(stage,ec)!=imageBytes||ec) {
            error="Guest SF2 volume has wrong byte size";
            fs::remove(stage,ec);return false;
        }
        // Verify the guest-facing bytes BEFORE replacing the prior volume.
        std::ifstream input(soundFont,std::ios::binary),verified(stage,std::ios::binary);
        verified.seekg(static_cast<std::streamoff>(dataStart)*bytesPerSector);
        std::array<char,32768> reread{};
        remaining=size;
        while(remaining!=0u) {
            const auto n=static_cast<std::streamsize>(
                std::min<std::uint64_t>(remaining,buf.size()));
            if(!input.read(buf.data(),n)||!verified.read(reread.data(),n)||
               std::memcmp(buf.data(),reread.data(),
                           static_cast<std::size_t>(n))!=0) {
                error="Guest FAT16 SoundFont readback differs from selected SF2";
                input.close();verified.close();fs::remove(stage,ec);return false;
            }
            remaining-=static_cast<std::uint64_t>(n);
        }
        input.close();verified.close();
        bool movedOld=false;
        if(fs::exists(target,ec)) {
            fs::rename(target,backup,ec);
            if(ec) {error="Could not preserve old private SoundFont disk";
                    fs::remove(stage,ec);return false;}
            movedOld=true;
        }
        fs::rename(stage,target,ec);
        if(ec) {
            if(movedOld) {
                std::error_code restore;
                fs::rename(backup,target,restore);
            }
            error="Could not commit guest SoundFont disk atomically";
            return false;
        }
        if(movedOld)fs::remove(backup,ec);
        error.clear();
        return true;
    }catch(...) {
        error="Unexpected error while preparing guest SoundFont volume";
        return false;
    }
}
} // namespace p007::sf2_volume
