// P005 original 288-slot VXD pointer importer. Synthetic guest-memory ONLY:
// these bytes are NOT original Win95, not a Creative driver distribution,
// not proof of independent SHA authenticity or source-audio parity.
#include "P005OriginalCreativeVxdDirectoryImporter.hpp"
#include "P005OriginalVxdGuestCaptureFile.hpp"
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
    // A forged pointer-only capture has the full bank pointer table
    // but lacks the remainder of the authentic original VXD image.
    CHECK(Importer::importNonRealtime(
        catalog,std::span<const std::uint8_t>(sourceDevice).first(
            Importer::kGuestPointerOffset+
            Importer::kOriginalSlotCount*Importer::kGuestPointerBytes),
        memory,true)==Result::MalformedDeviceImage);
    std::array<std::uint8_t,Importer::kOriginalDeviceImageBytes+1u>
        oversizedDevice{};
    CHECK(Importer::importNonRealtime(
        catalog,oversizedDevice,memory,true)==Result::MalformedDeviceImage);
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
        catalog,sourceDevice,noSentinel,true)==Result::MissingSentinel);
    CHECK(catalog.sourceSlotCount()==0u);

    // The actual 16-Nov Win95 owner capture shows a bank HEADER whose
    // first internal pointer leads to an aligned 26-byte program array.
    // Exercise that extra pointer level with SYNTHETIC bytes only.
    constexpr std::uint32_t bankRoot=0xc11b891cu;
    std::array<std::uint8_t,76u+3u*Importer::kOriginalRowBytes>
        indirectBank{};
    for(std::uint32_t i=0u;i<7u;++i)
        put32(indirectBank,i*4u,bankRoot+76u+200u*i);
    put16(indirectBank,76u,1u);
    indirectBank[76u+2u]=7u;
    indirectBank[76u+3u]=1u;
    put16(indirectBank,76u+Importer::kOriginalRowBytes,1u);
    indirectBank[76u+Importer::kOriginalRowBytes+2u]=9u;
    indirectBank[76u+Importer::kOriginalRowBytes+3u]=1u;
    put16(indirectBank,76u+2u*Importer::kOriginalRowBytes,0xffffu);
    auto indirectDevice=sourceDevice;
    put32(indirectDevice,ptrOffset+5u*4u,bankRoot);
    put32(indirectDevice,ptrOffset+6u*4u,bankRoot);
    const std::array<P005OriginalGuestMemorySegment,1> indirectMemory{{
        {bankRoot,indirectBank}
    }};
    CHECK(Importer::importNonRealtime(
        catalog,indirectDevice,indirectMemory,true)==Result::Ok);
    CHECK(catalog.sourceRowCountNonRealtime(5u)==3u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,1u)->original_program_byte==9u);
    CHECK(catalog.originalSourceRowNonRealtime(6u,2u)->original_source_marker==0xffffu);
    CHECK(catalog.resetNonRealtime());
    put16(indirectBank,76u+2u*Importer::kOriginalRowBytes,0u);
    CHECK(Importer::importNonRealtime(
        catalog,indirectDevice,indirectMemory,true)==Result::MissingSentinel);
    CHECK(catalog.sourceSlotCount()==0u);
    put16(indirectBank,76u+2u*Importer::kOriginalRowBytes,0xffffu);

    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,memory,true)==Result::Ok);
    CHECK(catalog.sourceSlotCount()==original_program_bank_slots);
    CHECK(catalog.sourceRowCountNonRealtime(5u)==3u);
    CHECK(catalog.sourceRowCountNonRealtime(6u)==3u);
    CHECK(catalog.sourceRowCountNonRealtime(4u)==0u);
    CHECK(!catalog.originalSourceRowNonRealtime(4u,0u));
    CHECK(!catalog.originalSourceRowNonRealtime(5u,3u));
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,1u)->original_program_byte==9u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_bank_byte==1u);
    CHECK(catalog.originalSourceRowNonRealtime(6u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,2u)->original_source_marker==0xffffu);
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
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(6u,0u)->original_program_byte==7u);
    CHECK(catalog.sourceSlotCount()==original_program_bank_slots);
    CHECK(catalog.resetNonRealtime());
    CHECK(catalog.sourceSlotCount()==0u);
    CHECK(!catalog.runtimeLeased());

    // The original 26-byte VXD record can cross a guest-page boundary.
    // The three unordered regions cover every consecutive VIRTUAL
    // address, but split the FIRST and SECOND rows in the middle.
    const std::array<P005OriginalGuestMemorySegment,3> splitPages{{
        {guestAddress+40u,std::span<const std::uint8_t>(sourceRows).subspan(40u)},
        {guestAddress,std::span<const std::uint8_t>(sourceRows).first(13u)},
        {guestAddress+13u,std::span<const std::uint8_t>(sourceRows).subspan(13u,27u)}
    }};
    sourceRows[2u]=7u; // restore original synthetic source after mutation
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,splitPages,true)==Result::Ok);
    CHECK(catalog.sourceSlotCount()==original_program_bank_slots);
    CHECK(catalog.sourceRowCountNonRealtime(5u)==3u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,1u)->original_program_byte==9u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,2u)->original_source_marker==0xffffu);
    CHECK(catalog.resetNonRealtime());

    // A genuine missing guest byte cannot be synthesized across a page
    // boundary. This invalidated source must remain wholly unpublished.
    const std::array<P005OriginalGuestMemorySegment,2> missingMiddlePage{{
        {guestAddress,std::span<const std::uint8_t>(sourceRows).first(13u)},
        {guestAddress+14u,std::span<const std::uint8_t>(sourceRows).subspan(14u)}
    }};
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,missingMiddlePage,true)==Result::InvalidGuestMapping);
    CHECK(catalog.sourceSlotCount()==0u && catalog.zoneCount()==0u);

    // The first record is intact, but the second record has a missing
    // byte. This must fail specifically with a missing-sentinel table.
    const std::array<P005OriginalGuestMemorySegment,2> missingSecondRow{{
        {guestAddress,std::span<const std::uint8_t>(sourceRows).first(39u)},
        {guestAddress+40u,std::span<const std::uint8_t>(sourceRows).subspan(40u)}
    }};
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,missingSecondRow,true)==Result::MissingSentinel);
    CHECK(catalog.sourceSlotCount()==0u && catalog.zoneCount()==0u);

    // Real capture tools can deliver guest memory pages out of order.
    // Stress the sorted index with every original VXD record BYTE
    // independently mapped and a permutation of its virtual address.
    constexpr std::size_t nBytes=3u*Importer::kOriginalRowBytes;
    std::array<P005OriginalGuestMemorySegment,nBytes> shuffled{};
    for(std::size_t i=0u;i<nBytes;++i) {
        const auto sourceIndex=(i*5u)%nBytes; // permutation: gcd(5,78)=1
        shuffled[i]={guestAddress+static_cast<std::uint32_t>(sourceIndex),
                     std::span<const std::uint8_t>(
                         sourceRows.data()+sourceIndex,1u)};
    }
    CHECK(Importer::importNonRealtime(
        catalog,sourceDevice,shuffled,true)==Result::Ok);
    CHECK(catalog.sourceRowCountNonRealtime(5u)==3u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,1u)->original_program_byte==9u);
    CHECK(catalog.originalSourceRowNonRealtime(6u,2u)->original_source_marker==0xffffu);
    CHECK(catalog.resetNonRealtime());

    // The full original Creative VXD image includes the source 1MGM
    // device signature and 32 physical EMU8000 voice capacity.
    // The standalone raw-pointer importer does not require these,
    // but the private owner CAPTURE FILE verifier correctly does.
    put32(sourceDevice,0x158eu,0x4d474d31u);
    put16(sourceDevice,6u,32u);
    // Owner PRIVATE binary "125AVXD1" -> C++ parser -> original 288
    // VXD guest pointer importer. File bytes are wholly SYNTHETIC.
    // The binary schema itself never establishes original provenance.
    using Capture=P005OriginalVxdGuestCaptureFile;
    using CaptureStatus=P005VxdGuestCaptureFileResult;
    std::vector<std::uint8_t> ownerCapture{
        '1','2','5','A','V','X','D','1'};
    const auto append32=[&ownerCapture](std::uint32_t value) {
        const auto at=ownerCapture.size();
        ownerCapture.resize(at+4u);
        put32(ownerCapture,at,value);
    };
    append32(1u);                 // capture version
    append32(0xc30b09f0u);       // exact guest device VA
    append32(0x3000u);           // source guest CR3
    append32(0x1592u);           // complete source device size
    append32(288u);              // original physical slot count
    append32(1u);                // unique guest pointer lists
    append32(2u);                // slot5/slot6 same pointer
    ownerCapture.insert(ownerCapture.end(),
        sourceDevice.begin(),sourceDevice.end());
    append32(guestAddress);
    append32(static_cast<std::uint32_t>(sourceRows.size()));
    ownerCapture.insert(ownerCapture.end(),
        sourceRows.begin(),sourceRows.end());
    Capture parsed{};
    CHECK(parsed.loadNonRealtime(ownerCapture)==CaptureStatus::Ok);
    CHECK(parsed.sourceFormatReady());
    CHECK(parsed.originalPointerLists()==1u);
    CHECK(parsed.deviceGuestVirtualAddress()==0xc30b09f0u);
    CHECK(parsed.capturedGuestCr3()==0x3000u);
    CHECK(parsed.importIntoNonRealtime(catalog,false)==Result::Unverified);
    CHECK(catalog.sourceSlotCount()==0u); // NEVER infer real driver SHA
    // The owner buffer is no longer needed: all rows are owned.
    sourceRows[2u]=93u;
    ownerCapture.clear();
    CHECK(parsed.importIntoNonRealtime(catalog,true)==Result::Ok);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)
        ->original_program_byte==7u);
    CHECK(catalog.originalSourceRowNonRealtime(6u,0u)
        ->original_program_byte==7u);
    CHECK(catalog.resetNonRealtime());
    parsed.resetNonRealtime();
    // Source owner file can retain a real bank header (76 bytes)
    // followed by its first nested original 26-byte table.
    // No driver/session authenticity is inferred from the file format.
    std::vector<std::uint8_t> nestedCapture{
        '1','2','5','A','V','X','D','1'};
    const auto appendNested32=[&nestedCapture](std::uint32_t value) {
        const auto at=nestedCapture.size();
        nestedCapture.resize(at+4u);
        put32(nestedCapture,at,value);
    };
    appendNested32(1u);
    appendNested32(0xc30b09f0u);
    appendNested32(0x3000u);
    appendNested32(0x1592u);
    appendNested32(288u);
    appendNested32(1u);
    appendNested32(2u);
    auto verifiedIndirectDevice=indirectDevice;
    put32(verifiedIndirectDevice,0x158eu,0x4d474d31u);
    put16(verifiedIndirectDevice,6u,32u);
    nestedCapture.insert(nestedCapture.end(),
        verifiedIndirectDevice.begin(),verifiedIndirectDevice.end());
    appendNested32(bankRoot);
    appendNested32(static_cast<std::uint32_t>(indirectBank.size()));
    nestedCapture.insert(nestedCapture.end(),
        indirectBank.begin(),indirectBank.end());
    Capture nestedParsed{};
    CHECK(nestedParsed.loadNonRealtime(nestedCapture)==CaptureStatus::Ok);
    CHECK(nestedParsed.importIntoNonRealtime(catalog,false)==Result::Unverified);
    CHECK(nestedParsed.importIntoNonRealtime(catalog,true)==Result::Ok);
    CHECK(catalog.sourceRowCountNonRealtime(5u)==3u);
    CHECK(catalog.originalSourceRowNonRealtime(5u,0u)
          ->original_program_byte==7u);
    CHECK(catalog.resetNonRealtime());
    nestedParsed.resetNonRealtime();
    CHECK(!parsed.sourceFormatReady());
    CHECK(parsed.importIntoNonRealtime(catalog,true)==Result::Unverified);

    std::puts("P005_PRIVATE_125AVXD1_TO_288_ORIGINAL_DIRECTORY_PASS source_provenance_required=TRUE");
    std::puts("P005_GUEST_288_VXD_POINTER_RECORD_IMPORT_PASS no_phdr_padding=TRUE original_guest_bytes_required=TRUE cross_page=TRUE missing_bytes_rejected=TRUE shuffled_byte_pages=TRUE");
    return 0;
}
