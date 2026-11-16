#pragma once
// P005: parse the authentic Windows-95 Creative device+0x6CA array of
// 288 *GUEST VIRTUAL POINTERS* and follow each through independently
// captured guest memory to the original 26-byte VXD program records.
//
// There is NO address relocation guess, MIDI bank synthesis, PHDR padding,
// copied Creative VXD table, or DSP implementation. A source capture must
// prove both the VXD device state and the guest memory regions independently.
// This routine does NOT compute hashes; the caller must authenticate its
// private Creative capture before invoking it. Synthetic fixtures in tests
// exercise the boundary and MUST NOT be shipped as user banks.
//
// Non-realtime ONLY (may allocate). Parsing is transactional: malformed
// guest pointers, truncated records or absent sentinels do not touch the
// destination catalog. The catalog itself owns the normalized rows.
#include "P005OriginalCreativeBankCatalog.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <vector>

namespace awe64::vst3 {
struct P005OriginalGuestMemorySegment {
    // True guest VIRTUAL start address recorded by the Win95 capture.
    std::uint32_t guest_address=0;
    std::span<const std::uint8_t> bytes{};
};
enum class P005OriginalVxdDirectoryImportResult : std::uint8_t {
    Ok, Unverified, MalformedDeviceImage, MissingOriginalRows,
    InvalidGuestMapping, MissingSentinel, TooManyRows, ResourceLimit, CatalogBusy
};
class P005OriginalVxdDirectoryImporter final {
public:
    static constexpr std::size_t kGuestPointerOffset=0x6cau;
    static constexpr std::size_t kGuestPointerBytes=4u;
    static constexpr std::size_t kOriginalRowBytes=0x1au;
    static constexpr std::size_t kOriginalSlotCount=
        P005OriginalCreativeBankCatalog::kSourceSlots;
    static constexpr std::size_t kMaxTotalNormalizedRows=65536u;

    // Caller provides actual device byte image and captured guest memory
    // regions. Every nonzero source pointer must be completely mapped.
    // A 26-byte row MAY cross a captured page/segment boundary only
    // when every intervening guest address is explicitly present.
    // The FIRST 0xFFFF row ends the table; no fabricated gaps or rows.
    // The source's 288-slot order and row ordinal are never changed.
    [[nodiscard]] static P005OriginalVxdDirectoryImportResult
    importNonRealtime(
        P005OriginalCreativeBankCatalog& catalog,
        std::span<const std::uint8_t> authenticatedDeviceImage,
        std::span<const P005OriginalGuestMemorySegment> guestMemory,
        bool deviceAndGuestMemoryIndependentlyAuthenticated) {
        using Result=P005OriginalVxdDirectoryImportResult;
        using Row=P005OriginalCreativeBankCatalog::Row;
        using Slot=P005OriginalCreativeBankCatalog::Slot;
        if(!deviceAndGuestMemoryIndependentlyAuthenticated)
            return Result::Unverified;
        if(authenticatedDeviceImage.size()<
           kGuestPointerOffset+kOriginalSlotCount*kGuestPointerBytes)
            return Result::MalformedDeviceImage;
        if(catalog.sourceSlotCount()!=0u || catalog.ready() ||
           catalog.runtimeLeased() || catalog.zoneCount()!=0u)
            return Result::CatalogBusy;

        // Reject impossible 32-bit guest mappings and overlap. Different
        // Win95 pointers can ALIAS the SAME bytes, but overlapping segment
        // declarations make the read result ambiguous and are forbidden.
        constexpr std::uint64_t kGuestAddressSpace=
            std::uint64_t{1}<<32u;
        for(std::size_t i=0u;i<guestMemory.size();++i) {
            auto const& a=guestMemory[i];
            const auto alo=std::uint64_t{a.guest_address};
            const auto ahi=alo+std::uint64_t{a.bytes.size()};
            if(ahi>kGuestAddressSpace || a.bytes.empty())
                return Result::InvalidGuestMapping;
            for(std::size_t j=0u;j<i;++j) {
                auto const& b=guestMemory[j];
                const auto blo=std::uint64_t{b.guest_address};
                const auto bhi=blo+std::uint64_t{b.bytes.size()};
                if(alo<bhi && blo<ahi)
                    return Result::InvalidGuestMapping;
            }
        }

        try {
        std::array<std::vector<Row>,kOriginalSlotCount> rows{};
        std::array<Slot,kOriginalSlotCount> views{};
        std::size_t totalRows=0u,nonzeroPointers=0u;
        for(std::size_t slot=0u;slot<kOriginalSlotCount;++slot) {
            const auto at=kGuestPointerOffset+slot*kGuestPointerBytes;
            const auto ptr=read32(authenticatedDeviceImage.data()+at);
            if(ptr==0u)continue; // REAL NULL source bank pointer
            ++nonzeroPointers;
            bool terminated=false;
            for(std::size_t ordinal=0u;
                ordinal<P005OriginalCreativeBankCatalog::kMaxRowsPerSlot;
                ++ordinal) {
                const auto vaddr=std::uint64_t{ptr}+
                                 std::uint64_t{ordinal}*kOriginalRowBytes;
                if(vaddr+kOriginalRowBytes>kGuestAddressSpace)
                    return Result::InvalidGuestMapping;
                // A VXD record may straddle the end of an original
                // Windows-95 guest page. Assemble only the ACTUAL
                // captured bytes at their exact consecutive addresses.
                // Gaps or conflicting overlapping segments remain
                // untrusted: never insert filler or guess a PHDR row.
                std::array<std::uint8_t,kOriginalRowBytes> joined{};
                std::size_t copied=0u;
                std::uint64_t address=vaddr;
                while(copied<kOriginalRowBytes) {
                    bool mapped=false;
                    for(auto const& segment:guestMemory) {
                        const auto begin=std::uint64_t{segment.guest_address};
                        const auto end=begin+
                            std::uint64_t{segment.bytes.size()};
                        if(address>=begin && address<end) {
                            const auto sourceOffset=
                                static_cast<std::size_t>(address-begin);
                            const auto count=std::min(
                                kOriginalRowBytes-copied,
                                static_cast<std::size_t>(end-address));
                            std::copy_n(
                                segment.bytes.begin()+sourceOffset,count,
                                joined.begin()+copied);
                            copied+=count;
                            address+=count;
                            mapped=true;
                            break;
                        }
                    }
                    if(!mapped)break;
                }
                if(copied!=kOriginalRowBytes)
                    return ordinal==0u ? Result::InvalidGuestMapping :
                                         Result::MissingSentinel;
                if(totalRows==kMaxTotalNormalizedRows)
                    return Result::TooManyRows;
                const auto marker=read16(joined.data());
                // This is the source's 26-byte row byte 0..3 ONLY.
                // No synthesizer metadata is inferred from the tail.
                rows[slot].push_back(
                    Row{marker,joined[2],joined[3]});
                ++totalRows;
                if(marker==0xffffu) {
                    terminated=true;
                    break;
                }
            }
            if(!terminated)return Result::MissingSentinel;
            views[slot].rows=rows[slot];
        }
        if(nonzeroPointers==0u)return Result::MissingOriginalRows;
        // On success, the catalog copies every row into its OWN vector.
        // The source memory regions and temporary arrays can then die.
        if(!catalog.importAuthenticatedOriginalDirectory(
                views,deviceAndGuestMemoryIndependentlyAuthenticated))
            return Result::CatalogBusy;
        return Result::Ok;
        } catch(const std::bad_alloc&) {
            // Never bubble an allocation failure across a VST3 host
            // boundary: a partial guest directory must remain unusable.
            return Result::ResourceLimit;
        }
    }

private:
    [[nodiscard]] static std::uint16_t
    read16(const std::uint8_t* b) noexcept {
        return std::uint16_t{b[0]} |
               static_cast<std::uint16_t>(std::uint16_t{b[1]}<<8u);
    }
    [[nodiscard]] static std::uint32_t
    read32(const std::uint8_t* b) noexcept {
        return std::uint32_t{read16(b)} |
               (std::uint32_t{read16(b+2u)}<<16u);
    }
};
} // namespace awe64::vst3
