#pragma once
// P005: decode the PRIVATE 125AVXD1 owner-side 86Box Win95 bank capture.
// Source-format parsing is NOT SHA/driver/session authentication.
// This code cannot turn a synthetically forged file into an original bank.
// No disk access, allocation, or parsing may occur in VST3 process().
#include "P005OriginalCreativeVxdDirectoryImporter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <new>
#include <span>
#include <vector>

namespace awe64::vst3 {
enum class P005VxdGuestCaptureFileResult : std::uint8_t {
    Ok, Malformed, InvalidSourceRows, ResourceLimit
};

class P005OriginalVxdGuestCaptureFile final {
public:
    static constexpr std::size_t kDeviceBytes=
        P005OriginalVxdDirectoryImporter::kOriginalDeviceImageBytes;
    static constexpr std::size_t kMaxBytes=8u*1024u*1024u;

    P005OriginalVxdGuestCaptureFile()=default;
    P005OriginalVxdGuestCaptureFile(
        P005OriginalVxdGuestCaptureFile const&)=delete;
    P005OriginalVxdGuestCaptureFile& operator=(
        P005OriginalVxdGuestCaptureFile const&)=delete;
    P005OriginalVxdGuestCaptureFile(
        P005OriginalVxdGuestCaptureFile&&)=delete;
    P005OriginalVxdGuestCaptureFile& operator=(
        P005OriginalVxdGuestCaptureFile&&)=delete;

    [[nodiscard]] P005VxdGuestCaptureFileResult
    loadNonRealtime(std::span<const std::uint8_t> raw) noexcept {
        resetNonRealtime();
        using Result=P005VxdGuestCaptureFileResult;
        try {
            constexpr std::array<std::uint8_t,8> magic{
                '1','2','5','A','V','X','D','1'};
            if(raw.size()<36u+kDeviceBytes || raw.size()>kMaxBytes ||
               !std::equal(magic.begin(),magic.end(),raw.begin()))
                return Result::Malformed;
            const auto version=read32(raw,8u);
            const auto va=read32(raw,12u);
            const auto cr3=read32(raw,16u);
            const auto nDevice=read32(raw,20u);
            const auto nSlots=read32(raw,24u);
            const auto nLists=read32(raw,28u);
            const auto nNonzero=read32(raw,32u);
            constexpr auto slots=P005OriginalVxdDirectoryImporter::kOriginalSlotCount;
            if(version!=1u || nDevice!=kDeviceBytes ||
               nSlots!=slots || nLists==0u ||
               nLists>slots || nNonzero<nLists || nNonzero>slots ||
               va<0xc0000000u ||
               std::uint64_t{va}+kDeviceBytes>(std::uint64_t{1}<<32u) ||
               (cr3&4095u)!=0u || cr3>=64u*1024u*1024u)
                return Result::Malformed;
            const auto image=raw.subspan(36u,kDeviceBytes);
            if(read32(image,0x158eu)!=0x4d474d31u ||
               read16(image,6u)!=32u)
                return Result::Malformed;
            std::array<std::uint32_t,slots> pointers{};
            std::size_t observed=0u;
            for(std::size_t slot=0u;slot<slots;++slot) {
                pointers[slot]=read32(image,0x6cau+slot*4u);
                if(pointers[slot]!=0u)++observed;
            }
            if(observed!=nNonzero)return Result::Malformed;
            std::map<std::uint32_t,std::vector<std::uint8_t>> lists{};
            std::size_t offset=36u+kDeviceBytes,rows=0u;
            for(std::size_t i=0u;i<nLists;++i) {
                if(offset+8u>raw.size())return Result::Malformed;
                const auto pointer=read32(raw,offset);
                const auto length=read32(raw,offset+4u);
                offset+=8u;
                if(pointer==0u ||
                   !std::any_of(pointers.begin(),pointers.end(),
                           [pointer](auto p){return p==pointer;}) ||
                   lists.contains(pointer) ||
                   length<P005OriginalVxdDirectoryImporter::kOriginalRowBytes ||
                   length%P005OriginalVxdDirectoryImporter::kOriginalRowBytes ||
                   length>raw.size()-offset ||
                   std::uint64_t{pointer}+length>(std::uint64_t{1}<<32u))
                    return Result::InvalidSourceRows;
                const auto nRows=
                    length/P005OriginalVxdDirectoryImporter::kOriginalRowBytes;
                if(nRows>65536u || rows>65536u-nRows)
                    return Result::ResourceLimit;
                for(std::size_t ri=0u;ri<nRows;++ri) {
                    const auto marker=read16(raw,offset+ri*26u);
                    if((ri==nRows-1u && marker!=0xffffu) ||
                       (ri<nRows-1u && marker==0xffffu))
                        return Result::InvalidSourceRows;
                }
                rows+=nRows;
                lists.emplace(pointer,std::vector<std::uint8_t>(
                    raw.begin()+offset,raw.begin()+offset+length));
                offset+=length;
            }
            if(offset!=raw.size() || lists.size()!=nLists)
                return Result::Malformed;
            for(const auto pointer:pointers)
                if(pointer!=0u && !lists.contains(pointer))
                    return Result::InvalidSourceRows;
            // The capture owns every guest row and state byte.
            // The source container is allowed to go out of scope
            // immediately after this transactional successful copy.
            std::copy(image.begin(),image.end(),device_.begin());
            lists_=std::move(lists);
            guestSegments_.reserve(lists_.size());
            for(auto const& [address,bytes]:lists_)
                guestSegments_.push_back({address,bytes});
            deviceGuestVirtualAddress_=va;
            originalCr3_=cr3;
            ready_=true;
            return Result::Ok;
        }catch(std::bad_alloc const&) {
            resetNonRealtime();
            return Result::ResourceLimit;
        }
    }

    [[nodiscard]] bool sourceFormatReady() const noexcept {
        return ready_;
    }
    [[nodiscard]] std::uint32_t deviceGuestVirtualAddress() const noexcept {
        return ready_?deviceGuestVirtualAddress_:0u;
    }
    [[nodiscard]] std::uint32_t capturedGuestCr3() const noexcept {
        return ready_?originalCr3_:0u;
    }
    [[nodiscard]] std::size_t originalPointerLists() const noexcept {
        return ready_?guestSegments_.size():0u;
    }
    // Must be backed by an INDEPENDENT owner-side original Win95 session
    // attestation; the capture-file header or pinned local VXD hash is
    // NOT in itself enough to permit original source activation.
    [[nodiscard]] P005OriginalVxdDirectoryImportResult
    importIntoNonRealtime(
        P005OriginalCreativeBankCatalog& catalog,
        bool originalGuestSessionIndependentlyAttested) const {
        return P005OriginalVxdDirectoryImporter::importNonRealtime(
            catalog,ready_?std::span<const std::uint8_t>{device_}:
                             std::span<const std::uint8_t>{},
            ready_?std::span<const P005OriginalGuestMemorySegment>{guestSegments_}:
                   std::span<const P005OriginalGuestMemorySegment>{},
            ready_ && originalGuestSessionIndependentlyAttested);
    }
    void resetNonRealtime() noexcept {
        ready_=false;
        device_.fill(0u);
        guestSegments_.clear();
        lists_.clear();
        deviceGuestVirtualAddress_=0u;
        originalCr3_=0u;
    }
private:
    static std::uint16_t read16(std::span<const std::uint8_t> b,
                                std::size_t n) noexcept {
        return std::uint16_t{b[n]} |
               static_cast<std::uint16_t>(std::uint16_t{b[n+1u]}<<8u);
    }
    static std::uint32_t read32(std::span<const std::uint8_t> b,
                                std::size_t n) noexcept {
        return std::uint32_t{read16(b,n)} |
               (std::uint32_t{read16(b,n+2u)}<<16u);
    }
    std::array<std::uint8_t,kDeviceBytes> device_{};
    std::map<std::uint32_t,std::vector<std::uint8_t>> lists_{};
    std::vector<P005OriginalGuestMemorySegment> guestSegments_{};
    std::uint32_t deviceGuestVirtualAddress_=0u,originalCr3_=0u;
    bool ready_=false;
};
} // namespace awe64::vst3
