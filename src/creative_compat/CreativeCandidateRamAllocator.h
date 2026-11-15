#pragma once
// P003 candidate: deterministic, bounded EMU8000 physical RAM allocation
// for previously unseen mono SoundFont sample pools. NOT certified as the
// original 1996 Creative AWEMAN32/SBAWE.VXD allocation algorithm.
//
// The first physically observed original bank begins at 0x200010.
// We reserve the first 16 words as an explicitly configurable policy,
// not a proven Creative invariant. All addresses are chip WORD addresses.
// The pinned EMU8000 core, original captured-bank path and DSP are untouched.
#include "CreativeSf2RamSamplePcm.h"
#include "Chip.h"
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
        for(const auto& slot:slots_)if(slot.active)n-=slot.words;
        return n;
    }
    // First-fit placement with 64 fixed descriptor slots, not heap;
    // stable active leases and exact nonoverlap. Deliberately separate
    // from source-captured original Creative RAM upload functions.
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocate(const std::uint32_t words) noexcept {
        if(!valid_ || words==0u || words>end_-first_)return std::nullopt;
        std::size_t freeSlot=kMaxBanks;
        for(std::size_t i=0;i<kMaxBanks;++i)
            if(!slots_[i].active) {freeSlot=i;break;}
        if(freeSlot==kMaxBanks)return std::nullopt;

        std::uint32_t address=first_;
        while(true) {
            if(words>end_-address)return std::nullopt;
            bool overlap=false;
            std::uint32_t next=address;
            for(const auto& slot:slots_) {
                if(!slot.active)continue;
                const auto slotEnd=slot.base+slot.words;
                if(address<slotEnd && slot.base<address+words) {
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
        s.base=address;s.words=words;s.active=true;
        return P003CandidateRamAllocation{
            s.base,s.words,s.generation,
            static_cast<std::uint32_t>(freeSlot)};
    }
    [[nodiscard]] std::optional<P003CandidateRamAllocation>
    allocateMonoSf2(const std::span<const std::uint8_t> sf2) noexcept {
        const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
        if(!pool || pool->whole_smpl_pcm_le16.size()/2u>
                    0xffffffffull)return std::nullopt;
        return allocate(static_cast<std::uint32_t>(
            pool->whole_smpl_pcm_le16.size()/2u));
    }
    [[nodiscard]] bool contains(const P003CandidateRamAllocation& h)
        const noexcept {
        if(h.id>=kMaxBanks || h.generation==0u)return false;
        const auto& s=slots_[h.id];
        return s.active && s.generation==h.generation &&
               s.base==h.base && s.words==h.words;
    }
    [[nodiscard]] bool release(const P003CandidateRamAllocation& h)
        noexcept {
        if(!contains(h))return false;
        slots_[h.id].active=false;
        return true;
    }
    // Do not silently move an active voice or claim Creative's
    // proprietary compaction algorithm. Source owner must stop voices
    // and explicitly release each bank before reusing its address.
private:
    struct Slot {
        std::uint32_t base=0,words=0,generation=0;
        bool active=false;
    };
    std::array<Slot,kMaxBanks> slots_{};
    std::uint32_t first_=0,end_=0;
    bool valid_=false;
};
} // namespace awe64::creative
