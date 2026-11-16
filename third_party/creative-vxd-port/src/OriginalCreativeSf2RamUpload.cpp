#include "OriginalCreativeSf2RamUpload.hpp"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::original_vxd {
OriginalSf2UploadResult upload_verified_sf2_pcm_to_emu8000(
    std::span<const std::uint8_t> raw,
    OriginalSf2RamUploadSource original,OriginalSf2RamSink sink) noexcept {
    if(!original.original_guest_ram_base_verified || !sink.upload_words)
        return {OriginalSf2UploadStatus::SourceMissing,0u,0u};
    const auto bytes=source_sf2_raw_sample_pool(raw);
    if(!bytes || (bytes->size()&1u)!=0u || bytes->empty())
        return {OriginalSf2UploadStatus::MalformedBank,0u,0u};
    constexpr std::uint32_t first=0x200000u,last=0x1000000u;
    const auto total=bytes->size()/2u;
    if(total>static_cast<std::size_t>(last-first) ||
       original.original_physical_word_base<first ||
       original.original_physical_word_base>=last ||
       total>last-original.original_physical_word_base)
        return {OriginalSf2UploadStatus::AddressOutOfRange,0u,0u};
    std::array<std::int16_t,256> tmp{};
    OriginalSf2UploadResult result{OriginalSf2UploadStatus::Ok,0u,0u};
    std::size_t pos=0u;
    while(pos<total) {
        const auto n=total-pos<tmp.size()?total-pos:tmp.size();
        for(std::size_t i=0u;i<n;++i) {
            const auto u=static_cast<std::uint16_t>(
                static_cast<unsigned>((*bytes)[(pos+i)*2u])|
                (static_cast<unsigned>((*bytes)[(pos+i)*2u+1u])<<8u));
            tmp[i]=u<=0x7fffu?static_cast<std::int16_t>(u):
                   static_cast<std::int16_t>(static_cast<std::int32_t>(u)-65536);
        }
        const auto addr=original.original_physical_word_base+
                        static_cast<std::uint32_t>(pos);
        if(!sink.upload_words(sink.opaque,addr,
              std::span<const std::int16_t>(tmp.data(),n))) {
            result.status=OriginalSf2UploadStatus::UploadFailed;
            return result;
        }
        pos+=n;
        result.words_written+=static_cast<std::uint32_t>(n);
        ++result.callback_count;
    }
    return result;
}
} // namespace awe64::original_vxd

namespace awe64::original_vxd {
std::int16_t OriginalCreativePcmFilter::next(const std::int16_t sample) noexcept {
    // Equivalent integer recurrence to the original PCOD+0x28AC state
    // machine, including the early zero exit and signed arithmetic shift.
    const bool empty_history=older==0 && older2==0 && older3==0;
    const bool zero_case=older==0 &&
        ((sample==0 && recent==0) || empty_history);
    const std::int32_t filtered=zero_case?0:
        (older*6-recent-older2)>>3;
    older3=older2;
    older2=older;
    older=recent;
    recent=sample;
    return static_cast<std::int16_t>(filtered);
}

OriginalCreativeRamUploadResult upload_original_creative_sf2_ram_bank(
    const std::span<const std::uint8_t> original_sf2,
    OriginalCreativeRamBankCursor& ram,
    const OriginalSf2RamSink sink,
    const OriginalCreativeRamBlockHooks hooks) noexcept {
    OriginalCreativeRamUploadResult result{};
    if(!ram.original_28mib_device_verified || ram.faulted ||
       !sink.upload_words ||
       (static_cast<bool>(hooks.before_block)!=
        static_cast<bool>(hooks.after_block))) return result;
    const auto data=source_sf2_raw_sample_pool(original_sf2);
    if(!data || data->empty() || (data->size()&1u)!=0u){
        result.status=OriginalSf2UploadStatus::MalformedBank;
        return result;
    }
    constexpr std::uint32_t first=0x200000u,last=0x1000000u;
    constexpr std::uint32_t initial_guard=14u,allocated_guard=16u;
    constexpr std::uint32_t terminal_writes=17u;
    constexpr std::uint32_t chunk=1024u;
    const auto raw_words=data->size()/2u;
    if(ram.next_bank_word<first || ram.next_bank_word>=last ||
       raw_words>static_cast<std::size_t>(last-first) ||
       raw_words+initial_guard+terminal_writes>
           static_cast<std::size_t>(last-ram.next_bank_word) ||
       raw_words+allocated_guard>
           static_cast<std::size_t>(last-ram.next_bank_word)){
        result.status=OriginalSf2UploadStatus::AddressOutOfRange;
        return result;
    }
    const auto base=ram.next_bank_word;
    const auto sample_base=base+initial_guard;
    const auto next_bank=base+static_cast<std::uint32_t>(raw_words)+allocated_guard;
    result.placement={base,sample_base,next_bank};
    result.source_pcm_words=static_cast<std::uint32_t>(raw_words);
    // Each 1024-word transfer has its own SMALW pointer and a trailing
    // dummy zero. The first block includes the 14-word start padding.
    OriginalCreativePcmFilter filter{};
    std::array<std::int16_t,1039> block{};
    std::size_t consumed=0u;
    while(consumed<raw_words) {
        const bool initial=consumed==0u;
        const auto n=std::min<std::size_t>(chunk,raw_words-consumed);
        const auto prefix=initial?initial_guard:0u;
        if(initial)for(std::size_t i=0u;i<initial_guard;++i)block[i]=0;
        for(std::size_t i=0u;i<n;++i) {
            const auto at=(consumed+i)*2u;
            const auto u=static_cast<std::uint16_t>(
                static_cast<unsigned>((*data)[at])|
                (static_cast<unsigned>((*data)[at+1u])<<8u));
            const auto signed_word=static_cast<std::int16_t>(
                u<=0x7fffu?static_cast<std::int32_t>(u):
                static_cast<std::int32_t>(u)-65536);
            block[prefix+i]=filter.next(signed_word);
        }
        block[prefix+n]=0; // x86 PCOD explicit block-end SMLD write
        const auto at=initial?base:sample_base+static_cast<std::uint32_t>(consumed);
        const auto words=prefix+n+1u;
        if((hooks.before_block && !hooks.before_block(hooks.opaque)) ||
           !sink.upload_words(sink.opaque,at,
              std::span<const std::int16_t>(block.data(),words)) ||
           (hooks.after_block && !hooks.after_block(hooks.opaque))) {
            ram.faulted=true;
            result.status=OriginalSf2UploadStatus::UploadFailed;
            return result;
        }
        result.physical_sample_writes+=static_cast<std::uint32_t>(words);
        ++result.transfer_chunks;
        consumed+=n;
    }
    block.fill(0);
    // Separate final SMALW write for the original 17-zero end guard.
    if((hooks.before_block && !hooks.before_block(hooks.opaque)) ||
       !sink.upload_words(sink.opaque,
        sample_base+static_cast<std::uint32_t>(raw_words),
        std::span<const std::int16_t>(block.data(),terminal_writes)) ||
       (hooks.after_block && !hooks.after_block(hooks.opaque))) {
        ram.faulted=true;
        result.status=OriginalSf2UploadStatus::UploadFailed;
        return result;
    }
    result.physical_sample_writes+=terminal_writes;
    ++result.transfer_chunks;
    ram.next_bank_word=next_bank;
    result.status=OriginalSf2UploadStatus::Ok;
    return result;
}
} // namespace awe64::original_vxd
