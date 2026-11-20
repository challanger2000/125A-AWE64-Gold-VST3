#pragma once
// P003 candidate: deterministic, bounded EMU8000 physical RAM allocation
// for previously unseen mono SoundFont sample pools. NOT certified as the
// original 1996 Creative AWEMAN32/SBAWE.VXD allocation algorithm.
//
// Genuine 2026-11-15 two-bank Win95 source capture:
// bank #1 sample 0x200010 (260921 words), bank #2 sample 0x23FB59
// (32768 words). Both original AWE_QUERY_DRAM_SIZE deltas include
// +16 words. Thus bank2 starts at bank1 sample end +16 words.
// This is ORIGINAL CAPTURE EVIDENCE for these two mono banks only,
// not proof of arbitrary Creative compaction/reallocation rules.
// All addresses are chip WORD addresses.
// The pinned EMU8000 core, original captured-bank path and DSP are untouched.
#include "CreativeSf2RamSamplePcm.h"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::creative {
struct P003CandidateRamAllocation {
    std::uint32_t base=0;
    std::uint32_t words=0;
    std::uint32_t generation=0;
    std::uint32_t id=0;
};
class P003CandidateRamAllocator {
public:
    static constexpr std::size_t kMaxBanks=64;
    static constexpr std::uint32_t kFirstRam=0x200000u;
    static constexpr std::uint32_t kReservedFmBase=0xffffe0u;
    static constexpr std::uint32_t kHistoricalFirstSample=0x200010u;
    // Original Win95 2-bank source trace: 260921->260937 total,
    // 32768->32784 total. Candidate sample-lease guard, not a claim
    // that all proprietary allocator internals are known.
    static constexpr std::uint32_t kSourceObservedPerBankOverhead=16u;

    explicit P003CandidateRamAllocator(
        std::uint32_t firstAvailable=kHistoricalFirstSample,
        std::uint32_t endExclusive=kReservedFmBase) noexcept
        : first_(firstAvailable),end_(endExclusive) {
        valid_=first_>=kFirstRam+4u &&
               first_<end_ && end_<=kReservedFmBase;
    }

    [[nodiscard]] bool valid() const noexcept {return valid_;}
    [[nodiscard]] std::size_t usedBanks() const noexcept {
        std::size_t n=0;
        for(const auto& slot:slots_)if(slot.active)++n;
        return n;
    }
    [[nodiscard]] std::uint32_t freeWords() const noexcept {
        if(!valid_)return 0;
        std::uint32_t n=end_-first_;
        for(const auto& slot:slots_)
            if(slot.active)n-=slot.words+slot.guard_after;
        return n;
    }
    // First-fit placement with 64 fixed descriptor slots, not heap;
    // stable active leases and exact nonoverlap. Deliberately separate
    // from source-captured original Creative RAM upload functions.
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocate(const std::uint32_t words) noexcept {
        return allocateWithGuard(words,0u);
    }
    // Source-observed Creative mono-bank placement: each BANK consumes
    // 16 additional physical words. Keep the lease's .words as
    // playable sample words for MIDI source-address resolution.
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocateCreativeSampleWords(const std::uint32_t words) noexcept {
        return allocateWithGuard(words,kSourceObservedPerBankOverhead);
    }
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocateMonoSf2(const std::span<const std::uint8_t> sf2) noexcept {
        const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
        if(!pool || pool->whole_smpl_pcm_le16.size()/2u>
                    0xffffffffull)return std::nullopt;
        return allocateCreativeSampleWords(static_cast<std::uint32_t>(
            pool->whole_smpl_pcm_le16.size()/2u));
    }
    [[nodiscard]] bool contains(const P003CandidateRamAllocation& h)
        const noexcept {
        if(h.id>=kMaxBanks || h.generation==0u)return false;
        const auto& s=slots_[h.id];
        return s.active && s.generation==h.generation &&
               s.base==h.base && s.words==h.words;
    }
    // Prevent a still-playing voice from referencing RAM recycled for
    // another bank. The caller must pin on successful NoteOn and unpin
    // only after the original VXD voice has fully terminated.
    [[nodiscard]] bool pin(const P003CandidateRamAllocation& h) noexcept {
        if(!contains(h) || slots_[h.id].pins==0xffffffffu)return false;
        ++slots_[h.id].pins;
        return true;
    }
    [[nodiscard]] bool unpin(const P003CandidateRamAllocation& h) noexcept {
        if(!contains(h) || slots_[h.id].pins==0u)return false;
        --slots_[h.id].pins;
        return true;
    }
    [[nodiscard]] std::uint32_t activePins(
        const P003CandidateRamAllocation& h) const noexcept {
        return contains(h) ? slots_[h.id].pins : 0u;
    }
    [[nodiscard]] bool release(const P003CandidateRamAllocation& h)
        noexcept {
        if(!contains(h) || slots_[h.id].pins!=0u)return false;
        slots_[h.id].active=false;
        return true;
    }
    // Do not silently move an active voice or claim Creative's
    // proprietary compaction algorithm. Source owner must stop voices
    // and explicitly release each bank before reusing its address.
private:
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocateWithGuard(const std::uint32_t words,
                      const std::uint32_t guard) noexcept {
        if(!valid_ || words==0u ||
           guard>end_-first_ || words>end_-first_-guard)
            return std::nullopt;
        std::size_t freeSlot=kMaxBanks;
        for(std::size_t i=0;i<kMaxBanks;++i)
            if(!slots_[i].active) {freeSlot=i;break;}
        if(freeSlot==kMaxBanks)return std::nullopt;

        std::uint32_t address=first_;
        while(true) {
            if(words+guard>end_-address)return std::nullopt;
            bool overlap=false;
            std::uint32_t next=address;
            for(const auto& slot:slots_) {
                if(!slot.active)continue;
                const auto slotEnd=slot.base+slot.words+slot.guard_after;
                if(address<slotEnd && slot.base<address+words+guard) {
                    overlap=true;
                    if(slotEnd>next)next=slotEnd;
                }
            }
            if(!overlap)break;
            if(next<=address)return std::nullopt;
            address=next;
        }
        auto& s=slots_[freeSlot];
        // A stale handle may never release a new occupant of this slot.
        ++s.generation;
        if(s.generation==0u)++s.generation;
        s.base=address;s.words=words;s.guard_after=guard;
        s.pins=0u;s.active=true;
        return P003CandidateRamAllocation{
            s.base,s.words,s.generation,
            static_cast<std::uint32_t>(freeSlot)};
    }
    struct Slot {
        std::uint32_t base=0,words=0,guard_after=0,generation=0,pins=0;
        bool active=false;
    };
    std::array<Slot,kMaxBanks> slots_{};
    std::uint32_t first_=0,end_=0;
    bool valid_=false;
};
} // namespace awe64::creative
