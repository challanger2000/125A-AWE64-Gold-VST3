#include "CreativeSf2RamBankUpload.h"
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>
namespace awe64::creative {
bool p003UploadValidatedMonoSf2PoolToOriginalChip(
    awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::uint32_t base,P003RamUploadResult& result,
    std::string& error) {
    result={};
    error.clear();
    if(!chip.ready() || chip.nativeNoteOutTransactionActive()) {
        error="P003 EMU8000 not initialized or active transaction";
        return false;
    }
    // Complete all structural checks BEFORE the first chip RAM write.
    // Keep four preceding physical words available for CCCA start-4;
    // this is a safety reservation, NOT a measured Creative allocator.
    const auto originalPool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!originalPool) {
        error="P003 SF2 not a complete, supported 16-bit RAM mono bank";
        return false;
    }
    const auto n=originalPool->whole_smpl_pcm_le16.size()/2u;
    if(base<awe64::kSampleRamBase+4u ||
       base>=awe64::kFmReservedBase ||
       n>std::size_t(awe64::kFmReservedBase-base)) {
        error="P003 SF2 pool exceeds 28-MiB physical RAM/reserved tail";
        return false;
    }
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram) {
        error="P003 original EMU8000 sample RAM inaccessible";
        return false;
    }
    // Convert signed 16-bit LE words exactly. Do NOT normalize PCM,
    // add guards, loop-pad or apply speculative 1996 Creative transforms.
    std::vector<std::int16_t> words;
    try {
        words.resize(n);
    }catch(...) {
        error="P003 sample pool allocation failed before chip writes";
        return false;
    }
    const auto b=originalPool->whole_smpl_pcm_le16;
    for(std::size_t i=0;i<n;++i) {
        const auto unsignedWord=std::uint16_t(b[i*2u]) |
            (std::uint16_t(b[i*2u+1u])<<8u);
        words[i]=std::bit_cast<std::int16_t>(unsignedWord);
    }
    if(!chip.uploadRam(base,words)) {
        error="P003 EMU8000 rejected bounded sample pool upload";
        return false;
    }
    const auto offset=static_cast<std::size_t>(base-awe64::kSampleRamBase);
    for(std::size_t i=0;i<n;++i)
        if(raw->ram[offset+i]!=words[i]) {
            error="P003 real EMU8000 RAM readback differs from SF2 PCM16";
            return false;
        }
    result.physical_word_base=base;
    result.source_pool_words=n;
    result.sample_headers=originalPool->sample_headers;
    result.entire_pool_readback_equal=true;
    return true;
}
} // namespace awe64::creative
