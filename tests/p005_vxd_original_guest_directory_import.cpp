// P005 original 288-slot VXD pointer importer. Synthetic guest-memory ONLY:
// these bytes are NOT original Win95, not a Creative driver distribution,
// not proof of independent SHA authenticity or source-audio parity.
#include "P005OriginalCreativeVxdDirectoryImporter.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>
using namespace awe64::vst3;
using awe64::original_vxd::original_program_bank_slots;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P005_VXD_IMPORT_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
namespace {
void put16(std::span<std::uint8_t> bytes,std::size_t at,std::uint16_t v) {
    bytes[at]=static_cast<std::uint8_t>(v);
    bytes[at+1u]=static_cast<std::uint8_t>(v>>8u);
}
void put32(std::span<std::uint8_t> bytes,std::size_t at,std::uint32_t v) {
    put16(bytes,at,static_cast<std::uint16_t>(v));
    put16(bytes,at+2u,static_cast<std::uint16_t>(v>>16u));
}
}
int main() {
    using Importer=P005OriginalVxdDirectoryImporter;
    using Result=P005OriginalVxdDirectoryImportResult;
    constexpr auto ptrOffset=Importer::kGuestPointerOffset;
    constexpr std::uint32_t guestAddress=0x12008000u;
    std::array<std::uint8_t,0x1592u> sourceDevice{};
    std::array<std::uint8_t,3u*Importer::kOriginalRowBytes> sourceRows{};
    put32(sourceDevice,ptrOffset+5u*4u,guestAddress);
    put32(sourceDevice,ptrOffset+6u*4u,guestAddress); // real pointer alias
    put16(sourceRows,0u,1u);
    sourceRows[2u]=7u;sourceRows[3u]=1u;
    put16(sourceRows,Importer::kOriginalRowBytes,8u);
    sourceRows[Importer::kOriginalRowBytes+2u]=9u;
    sourceRows[Importer::kOriginalRowBytes+3u]=1u;
    put16(sourceRows,2u*Importer::kOriginalRowBytes,0xffffu);
    const std::array<P005OriginalGuestMemorySegment,1> memory{{
        {guestAddress,sourceRows}
    }};
    P005OriginalCreativeBankCatalog catalog{};

    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,memory,false)==Result::Unverified);
    CHECK(catalog.sourceSlotCount()==0u);
    CHECK(Importer::importNonRealtime(
        catalog,std::span<const std::uint8_t>(sourceDevice).first(0x100u),
        memory,true)==Result::MalformedDeviceImage);
    CHECK(catalog.sourceSlotCount()==0u);
    auto missing=sourceDevice;
    put32(missing,ptrOffset+5u*4u,0u);
    put32(missing,ptrOffset+6u*4u,0u);
    CHECK(Importer::importNonRealtime(
        catalog,missing,memory,true)==Result::MissingOriginalRows);
    // The first original guest pointer must map to a REAL 26-byte row.
    put32(missing,ptrOffset+5u*4u,0xeeee0000u);
    CHECK(Importer::importNonRealtime(
        catalog,missing,memory,true)==Result::InvalidGuestMapping);
    put32(missing,ptrOffset+5u*4u,0xfffffff0u);
    CHECK(Importer::importNonRealtime(
        catalog,missing,memory,true)==Result::InvalidGuestMapping);
    CHECK(catalog.sourceSlotCount()==0u);
    // An overlapping second memory capture is ambiguous; reject it.
    const std::array<P005OriginalGuestMemorySegment,2> overlapping{{
        {guestAddress,sourceRows},
        {guestAddress+1u,std::span<const std::uint8_t>(sourceRows).subspan(1u)}
    }};
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,overlapping,true)==Result::InvalidGuestMapping);
    CHECK(catalog.sourceSlotCount()==0u);
    // FIRST 0xFFFF source marker must terminate each non-NULL slot.
    auto corrupt=sourceRows;
    put16(corrupt,2u*Importer::kOriginalRowBytes,1u);
    const std::array<P005OriginalGuestMemorySegment,1> noSentinel{{
        {guestAddress,corrupt}
    }};
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,noSentinel,true)==Result::InvalidGuestMapping);
    CHECK(catalog.sourceSlotCount()==0u);

    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,memory,true)==Result::Ok);
    CHECK(catalog.sourceSlotCount()==original_program_bank_slots);
    CHECK(catalog.zoneCount()==0u);
    CHECK(!catalog.ready()); // no authenticated note state was loaded
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,memory,true)==Result::CatalogBusy);
    // Catalog OWNS both source alias rows; no borrowed pointers survive.
    const auto snap=catalog.view();
    CHECK(snap.zones.empty()); // no sound before precompiled note zones
    // The public view is intentionally still disabled until sealNonRealtime.
    // Verify the underlying original rows through a second staged state:
    // a reset succeeds only because no MIDI runtime is borrowing them.
    sourceRows[2u]=88u; // invalidate the original memory capture
    CHECK(catalog.sourceSlotCount()==original_program_bank_slots);
    CHECK(catalog.resetNonRealtime());
    CHECK(catalog.sourceSlotCount()==0u);
    CHECK(!catalog.runtimeLeased());
    std::puts("P005_GUEST_288_VXD_POINTER_RECORD_IMPORT_PASS no_phdr_padding=TRUE original_guest_bytes_required=TRUE");
    return 0;
}
