#pragma once
// Read-only, non-realtime parser of TWO related Win95 guest-memory captures:
// 125AVXD1 (original 288-slot device/program directory) and 125AVXR1
// (six exact downstream original guest regions). These formats encode
// observations, not cryptographic proof of an original running SBAWE.VXD.
// No Creative bytes, SF2 engine, DSP, VST3 audio, or CPU heuristics here.
//
// The parser owns every byte used by its output spans. A consumer MUST
// separately authenticate original guest/session/driver provenance before
// setting independentAttestation on any source-index traversal.
#include "P005OriginalVxdGuestCaptureFile.hpp"
#include "OriginalCreativeNestedProgramPath.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <vector>

namespace awe64::vst3 {
enum class P005NestedGuestCaptureResult : std::uint8_t {
    Ok, InvalidPrimary, Malformed, InconsistentOriginalSource,
    IncompleteRegion, ResourceLimit
};

class P005OriginalNestedGuestCaptureFile final {
public:
    static constexpr std::size_t kMaxFileBytes=8u*1024u*1024u;
    static constexpr std::size_t kSectionCount=6u;
    P005OriginalNestedGuestCaptureFile()=default;
    P005OriginalNestedGuestCaptureFile(
        const P005OriginalNestedGuestCaptureFile&)=delete;
    P005OriginalNestedGuestCaptureFile& operator=(
        const P005OriginalNestedGuestCaptureFile&)=delete;

    [[nodiscard]] P005NestedGuestCaptureResult loadNonRealtime(
        std::span<const std::uint8_t> primary,
        std::span<const std::uint8_t> nested) noexcept {
        resetNonRealtime();
        using R=P005NestedGuestCaptureResult;
        P005OriginalVxdGuestCaptureFile original;
        if(original.loadNonRealtime(primary)!=P005VxdGuestCaptureFileResult::Ok)
            return R::InvalidPrimary;
        if(nested.size()<56u || nested.size()>kMaxFileBytes)
            return R::Malformed;
        constexpr std::array<std::uint8_t,8> magic{
            '1','2','5','A','V','X','R','1'};
        if(!std::equal(magic.begin(),magic.end(),nested.begin()) ||
           u32(nested,8u)!=1u)
            return R::Malformed;
        const auto deviceVa=u32(nested,12u);
        const auto guestCr3=u32(nested,16u);
        const auto root=u32(nested,20u);
        if(deviceVa!=original.deviceGuestVirtualAddress() ||
           guestCr3!=original.capturedGuestCr3())
            return R::InconsistentOriginalSource;
        std::array<std::uint32_t,7> pointers{};
        for(std::size_t i=0u;i<pointers.size();++i)
            pointers[i]=u32(nested,24u+4u*i);
        if(u32(nested,52u)!=kSectionCount ||
           pointers[0]<=root ||
           pointers[0]-root<28u ||
           pointers[0]-root>4096u)
            return R::Malformed;
        for(std::size_t i=0u;i+1u<pointers.size();++i)
            if(pointers[i]>=pointers[i+1u])
                return R::Malformed;
        // Validate against the actual *complete* source DEVICE image,
        // rather than a guessed SF2 preset number or source row count.
        constexpr std::size_t kDeviceAt=36u;
        constexpr std::size_t kDeviceBytes=
            P005OriginalVxdGuestCaptureFile::kDeviceBytes;
        bool foundRoot=false;
        for(std::size_t slot=0u;slot<288u;++slot) {
            if(u32(primary,kDeviceAt+0x6cau+4u*slot)==root) {
                foundRoot=true;break;
            }
        }
        if(!foundRoot)return R::InconsistentOriginalSource;
        // Find exact observed root record in the first capture. It has
        // the seven-pointer header and original 26-byte program rows.
        std::size_t offset=kDeviceAt+kDeviceBytes;
        std::size_t bankOffset=0u,bankLength=0u;
        const auto nLists=u32(primary,28u);
        for(std::size_t i=0u;i<nLists;++i) {
            if(offset+8u>primary.size())return R::InvalidPrimary;
            const auto address=u32(primary,offset);
            const auto size=u32(primary,offset+4u);
            offset+=8u;
            if(size>primary.size()-offset)return R::InvalidPrimary;
            if(address==root) {
                bankOffset=offset;bankLength=size;break;
            }
            offset+=size;
        }
        if(bankLength<28u)return R::InconsistentOriginalSource;
        for(std::size_t i=0u;i<7u;++i)
            if(u32(primary,bankOffset+4u*i)!=pointers[i])
                return R::InconsistentOriginalSource;
        const auto rowPrefix=
            static_cast<std::size_t>(pointers[0]-root);
        const auto rowSpan=static_cast<std::uint64_t>(pointers[1])-pointers[0];
        if(rowSpan<26u || rowSpan%26u!=0u ||
           rowSpan/26u>65536u ||
           rowPrefix>bankLength ||
           rowSpan>bankLength-rowPrefix)
            return R::IncompleteRegion;
        const auto nRows=static_cast<std::size_t>(rowSpan/26u);
        if(nRows<2u)return R::Malformed;
        try {
            std::vector<awe64::original_vxd::OriginalPresetTableRow> rows;
            rows.reserve(nRows);
            for(std::size_t i=0u;i<nRows;++i) {
                const auto at=bankOffset+rowPrefix+i*26u;
                rows.push_back({
                    u16(primary,at),primary[at+2u],primary[at+3u],
                    u16(primary,at+4u)
                });
                if((rows.back().original_source_marker==0xffffu)!=
                   (i+1u==nRows))
                    return R::Malformed;
            }
            std::array<std::vector<std::uint8_t>,kSectionCount> bytes{};
            std::size_t at=56u;
            for(std::size_t i=0u;i<kSectionCount;++i) {
                if(at+12u>nested.size())return R::IncompleteRegion;
                const auto address=u32(nested,at);
                const auto length=u32(nested,at+4u);
                const auto complete=u32(nested,at+8u);
                at+=12u;
                // 5 fully bounded table regions, then exactly 4 KiB
                // of a still-INCOMPLETE final region. Never call that
                // last 4 KiB a sample-PCM bank or a complete table.
                const auto expected=i+1u<kSectionCount?
                    static_cast<std::uint64_t>(pointers[i+2u])-
                    pointers[i+1u]:4096u;
                if(address!=pointers[i+1u] ||
                   length!=expected ||
                   complete!=(i+1u<kSectionCount?1u:0u) ||
                   length==0u)
                    return R::InconsistentOriginalSource;
                if(length>nested.size()-at)return R::IncompleteRegion;
                bytes[i].assign(nested.begin()+at,nested.begin()+at+length);
                at+=length;
            }
            if(at!=nested.size())return R::Malformed;
            // Validate all observed compact index boundary relations.
            // These are byte-layout relations only. Instrument/preset
            // semantics still need original driver instruction proofs.
            if(bytes[0].size()%4u || bytes[1].size()%3u ||
               bytes[2].size()%2u || bytes[3].size()%4u ||
               bytes[4].size()%3u ||
               bytes[0].size()<8u || bytes[2].size()<4u ||
               bytes[3].size()<8u)
                return R::Malformed;
            const std::span<const std::uint8_t> presetIndex{bytes[0]};
            const std::span<const std::uint8_t> instrumentIndex{bytes[2]};
            const std::span<const std::uint8_t> instrumentBagIndex{bytes[3]};
            if(u32(presetIndex,0u)!=0u ||
               u32(presetIndex,presetIndex.size()-4u)!=
                    bytes[1].size()/3u ||
               u16(instrumentIndex,0u)!=0u ||
               u16(instrumentIndex,instrumentIndex.size()-2u)!=
                    bytes[3].size()/4u-1u ||
               u32(instrumentBagIndex,0u)!=0u ||
               u32(instrumentBagIndex,instrumentBagIndex.size()-4u)!=
                    bytes[4].size()/3u ||
               rows.front().original_child_index!=0u ||
               rows.back().original_child_index!=
                    presetIndex.size()/4u-1u)
                return R::Malformed;
            for(std::size_t i=4u;i<presetIndex.size();i+=4u)
                if(u32(presetIndex,i)<u32(presetIndex,i-4u))
                    return R::Malformed;
            for(std::size_t i=2u;i<instrumentIndex.size();i+=2u)
                if(u16(instrumentIndex,i)<u16(instrumentIndex,i-2u))
                    return R::Malformed;
            for(std::size_t i=4u;i<instrumentBagIndex.size();i+=4u)
                if(u32(instrumentBagIndex,i)<u32(instrumentBagIndex,i-4u))
                    return R::Malformed;
            // All validation completed before mutating the owner.
            rows_=std::move(rows);
            bytes_=std::move(bytes);
            root_=root;
            deviceVa_=deviceVa;
            guestCr3_=guestCr3;
            ready_=true;
            return R::Ok;
        }catch(const std::bad_alloc&) {
            resetNonRealtime();
            return R::ResourceLimit;
        }
    }

    // Non-RT inspection only. Attestation is *not* granted by parsing:
    // it is an external original-driver/session provenance decision.
    [[nodiscard]] awe64::original_vxd::OriginalNestedProgramTables
    tables(bool independentlyAuthenticated=false) const noexcept {
        awe64::original_vxd::OriginalNestedProgramTables t{};
        if(!ready_)return t;
        t.programs=rows_;
        t.preset_bag_offsets=bytes_[0];
        t.preset_generators=bytes_[1];
        t.instrument_bag_offsets=bytes_[2];
        t.instrument_generator_offsets=bytes_[3];
        t.instrument_generators=bytes_[4];
        t.independently_authenticated=independentlyAuthenticated;
        return t;
    }
    [[nodiscard]] bool sourceFormatReady() const noexcept {return ready_;}
    [[nodiscard]] std::uint32_t sourceRootGuestAddress() const noexcept {
        return ready_?root_:0u;
    }
    [[nodiscard]] std::size_t sourceProgramRows() const noexcept {
        return ready_?rows_.size():0u;
    }
    [[nodiscard]] bool finalGuestRegionComplete() const noexcept {
        return false; // source explicitly recorded only first 4096 bytes
    }
    void resetNonRealtime() noexcept {
        ready_=false;root_=0u;deviceVa_=0u;guestCr3_=0u;
        rows_.clear();
        for(auto& v:bytes_)v.clear();
    }
private:
    static std::uint16_t u16(
        std::span<const std::uint8_t> data,std::size_t at) noexcept {
        return static_cast<std::uint16_t>(
            data[at] | (static_cast<unsigned>(data[at+1u])<<8u));
    }
    static std::uint32_t u32(
        std::span<const std::uint8_t> data,std::size_t at) noexcept {
        return static_cast<std::uint32_t>(u16(data,at)) |
              (static_cast<std::uint32_t>(u16(data,at+2u))<<16u);
    }
    std::array<std::vector<std::uint8_t>,kSectionCount> bytes_{};
    std::vector<awe64::original_vxd::OriginalPresetTableRow> rows_{};
    std::uint32_t root_=0u,deviceVa_=0u,guestCr3_=0u;
    bool ready_=false;
};
} // namespace awe64::vst3
