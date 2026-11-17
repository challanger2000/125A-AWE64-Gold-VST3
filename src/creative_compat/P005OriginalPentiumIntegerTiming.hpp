#pragma once
// P005 source-derived P54C integer translation timing.
//
// Reconstructs the NON-FPU pipeline accounting of the unchanged 86Box
// codegen_timing_pentium.c + codegen_timing_common.h from the pinned
// turican0/86Box-AWE32 v6.0-AWE32-0.93 fork. Dependencies and timing
// words MUST be supplied by a separately verified original x86 decoder.
// In particular this is NOT a guessed cost per EMU8000 OUT or a complete
// autonomous Win95 CPU emulator. The original dynarec's compiler-side
// cycle accounting is distinct from actually executed guest TSCs.
// No heap, platform calls, pointers into a guest, or original media.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace awe64::creative {
struct P005OriginalP54cDecodedIntegerOp {
    std::uint64_t timing_word=0; // Original 86Box P54C timing entry
    std::uint64_t dependency_word=0; // Original 86Box dependency entry
    std::uint32_t fetchdat=0; // Original decoder ModR/M + SIB field
    std::uint16_t op_32=0; // Original 86Box operand/address bits 0x100/0x200
    std::uint8_t opcode=0;
    std::span<const std::uint8_t> prefixes{};
};

class P005OriginalPentiumIntegerTiming final {
public:
    static constexpr std::uint64_t kPairU=1ull<<29u;
    static constexpr std::uint64_t kPairV=2ull<<29u;
    static constexpr std::uint64_t kPairUV=3ull<<29u;
    static constexpr std::uint64_t kPairMask=7ull<<29u;
    static constexpr std::uint64_t kMultiCycles=1ull<<28u;
    static constexpr std::uint64_t kFpu=4ull<<29u;
    static constexpr std::uint64_t kSrcReg=1ull;
    static constexpr std::uint64_t kSrcRm=2ull;
    static constexpr std::uint64_t kDstReg=4ull;
    static constexpr std::uint64_t kDstRm=8ull;
    static constexpr std::uint64_t kModRm=1ull<<20u;
    static constexpr std::uint64_t kImplicitEsp=1ull<<21u;
    static constexpr std::uint32_t kImplicitEspMask=1u<<8u;

    void resetSession() noexcept { *this={}; }
    [[nodiscard]] bool beginBlock() noexcept {
        if(block_open_ || failed_) return false;
        // ORIGINAL codegen_timing_pentium_block_start() resets these,
        // crucially NOT regmask_modified.
        pipe_={};
        decode_delay_=0;
        decode_offset_=0;
        block_cycles_=0;
        block_open_=true;
        return true;
    }
    [[nodiscard]] bool append(const P005OriginalP54cDecodedIntegerOp& op) noexcept {
        if(!block_open_ || failed_ || op.prefixes.size()>8u ||
           (op.op_32 & ~std::uint16_t{0x300u})!=0u ||
           (op.timing_word & kFpu)!=0u ||
           (op.timing_word & kPairMask)>kPairUV)
            return fail();
        // Only the original integer dependencies/prefix rules are
        // admitted here. Any FPU/MMX operation needs the full model.
        if(op.dependency_word & ((3ull<<22u)|(0x3ffull<<24u)))
            return fail();
        for(const auto prefix:op.prefixes) {
            // 0F is free on P54C's near conditional jumps.
            if(prefix==0x0fu && (op.fetchdat & 0xf0u)==0x80u)
                continue;
            if(prefix==0x0fu || prefix==0x66u || prefix==0x67u ||
               prefix==0x2eu || prefix==0x36u || prefix==0x3eu ||
               prefix==0x26u || prefix==0x64u || prefix==0x65u ||
               prefix==0xf2u || prefix==0xf3u || prefix==0xf0u)
                ++decode_offset_;
            else return fail(); // no invented prefix semantics
        }
        const auto dst=destinationMask(op);
        if(pipe_.valid) {
            const auto pairKind=(op.timing_word & kPairMask);
            const auto src=sourceMask(op,pipe_.op_32);
            const bool allowPair=
                (pairKind&kPairV)!=0u &&
                (pipe_.destination_mask&src)==0u &&
                (decode_delay_+decode_offset_+pipe_.decode_offset)<=0 &&
                !hasDisplacement(op);
            if(allowPair) {
                const auto a=static_cast<unsigned>(pipe_.timing_word&3u);
                const auto b=static_cast<unsigned>(op.timing_word&3u);
                constexpr std::int32_t pairs[4][4]={
                    {1,2,3,2},{2,2,3,3},{3,4,5,4},{-1,-1,-1,-1}
                };
                if(pairs[a][b]<=0)return fail();
                const auto stall=(agi(op)||agi(pipe_.op))?1:0;
                if(!instructionCycles(pairs[a][b],stall,0))
                    return fail();
                regmask_modified_=dst|pipe_.destination_mask;
                pipe_={};
                decode_offset_=0;
                return true;
            }
            if(!instructionCycles(count(pipe_.op),agi(pipe_.op)?1:0,
                                  pipe_.decode_offset))
                return fail();
            regmask_modified_=pipe_.destination_mask;
            pipe_={};
        }
        if((op.timing_word&kPairU)!=0u &&
           (decode_delay_+decode_offset_)<=0 &&
           !hasDisplacement(op)) {
            pipe_={true,op,dst,decode_offset_};
            decode_offset_=0;
            return true;
        }
        if(!instructionCycles(count(op),agi(op)?1:0,decode_offset_))
            return fail();
        regmask_modified_=dst;
        decode_offset_=0;
        return true;
    }
    [[nodiscard]] bool endBlock() noexcept {
        if(!block_open_ || failed_)return false;
        if(pipe_.valid) {
            // Same original block_end split: the unpaired U instruction
            // charges COUNT + delay, plus one possible AGI cycle.
            const auto n=count(pipe_.op);
            const auto extra=agi(pipe_.op)?1:0;
            const auto delta=static_cast<std::int64_t>(n)+
                             decode_delay_+decode_offset_+extra;
            if(n<=0 || !add(delta))return fail();
            pipe_={};
        }
        if(block_cycles_<0)return fail();
        if(total_cycles_>std::numeric_limits<std::int64_t>::max()-
                         block_cycles_)return fail();
        total_cycles_+=block_cycles_;
        block_open_=false;
        return true;
    }
    [[nodiscard]] std::int64_t totalCycles() const noexcept {return total_cycles_;}
    [[nodiscard]] std::int64_t lastBlockCycles() const noexcept {return block_cycles_;}
    [[nodiscard]] std::uint32_t lastModifiedRegisterMask() const noexcept {
        return regmask_modified_;
    }
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    [[nodiscard]] bool blockOpen() const noexcept {return block_open_;}
private:
    struct Pipe {
        bool valid=false;
        P005OriginalP54cDecodedIntegerOp op{};
        std::uint32_t destination_mask=0;
        std::int64_t decode_offset=0;
    };
    [[nodiscard]] bool fail() noexcept {
        failed_=true;block_open_=false;pipe_={};return false;
    }
    [[nodiscard]] bool add(const std::int64_t delta) noexcept {
        constexpr std::int64_t cap=100'000'000;
        if(delta>cap || delta< -cap || block_cycles_>cap-delta ||
           block_cycles_<-cap-delta)return false;
        block_cycles_+=delta;
        return true;
    }
    [[nodiscard]] bool instructionCycles(
        std::int64_t count_cycles,std::int64_t stall,
        std::int64_t offset) noexcept {
        if(count_cycles<=0)return false;
        const auto instr=count_cycles+stall;
        const auto delay=decode_delay_+offset;
        if(!add(instr+(delay>0?delay:0)))return false;
        decode_delay_=1-instr;
        return true;
    }
    [[nodiscard]] static std::int64_t count(
        const P005OriginalP54cDecodedIntegerOp& op) noexcept {
        const auto t=op.timing_word;
        if(t&kMultiCycles)
            return static_cast<std::int64_t>(
                (t>>((op.op_32&0x100u)?8u:0u))&0xffu);
        if(!(t&kPairMask))return static_cast<std::int64_t>(t&0xffffu);
        switch(t&0x7fu) {
            case 0: return 1;
            case 1: return 2;
            case 2: return 3;
            case 3: return 2; // P54C, non-MMX
            default:return -1;
        }
    }
    [[nodiscard]] static bool hasDisplacement(
        const P005OriginalP54cDecodedIntegerOp& op) noexcept {
        if((op.timing_word&(3ull<<2u))==0u)return false;
        const auto f=op.fetchdat;
        if(op.op_32&0x200u) {
            if((f&7u)==4u && (f&0xc0u)!=0xc0u)
                return (f&0xc0u)==0x40u ||
                       (f&0xc0u)==0x80u || (f&0x700u)==0x500u;
            return (f&0xc0u)==0x40u ||
                   (f&0xc0u)==0x80u || (f&0xc7u)==0x05u;
        }
        return (f&0xc0u)==0x40u ||
               (f&0xc0u)==0x80u || (f&0xc7u)==0x06u;
    }
    [[nodiscard]] static std::uint32_t addressMask(
        const P005OriginalP54cDecodedIntegerOp& op,
        std::uint16_t flags) noexcept {
        std::uint32_t mask=0;
        const auto dep=op.dependency_word;
        const auto f=op.fetchdat;
        const auto m=static_cast<std::uint8_t>(f);
        if((dep&kModRm)!=0u && (m&0xc0u)!=0xc0u) {
            if(flags&0x200u) {
                if((m&7u)==4u) {
                    const auto sib=static_cast<std::uint8_t>(f>>8u);
                    if((sib&7u)!=5u) {
                        mask=1u<<(sib&7u);
                        if((sib&0x38u)!=0x20u)
                            mask|=1u<<((sib>>3u)&7u);
                    }
                }else if((m&0xc7u)!=5u)mask=1u<<(m&7u);
            }else if((m&0xc7u)!=6u) {
                constexpr std::uint8_t regs[8]{
                    (1u<<3u)|(1u<<6u),(1u<<3u)|(1u<<7u),
                    (1u<<5u)|(1u<<6u),(1u<<5u)|(1u<<7u),
                    (1u<<6u),(1u<<7u),(1u<<5u),(1u<<3u)};
                mask=regs[m&7u];
            }
        }
        if(dep&kImplicitEsp)mask|=kImplicitEspMask;
        return mask;
    }
    [[nodiscard]] static std::uint32_t destinationMask(
        const P005OriginalP54cDecodedIntegerOp& op) noexcept {
        const auto dep=op.dependency_word;
        const bool bit8=(op.opcode&1u)==0u;
        std::uint32_t mask=0;
        if(dep&kDstReg)mask|=1u<<(((op.fetchdat>>3u)&7u)&(bit8?3u:7u));
        if(dep&kDstRm)mask|=1u<<((op.fetchdat&7u)&(bit8?3u:7u));
        mask|=static_cast<std::uint32_t>((dep>>12u)&0xffu);
        if(dep&kImplicitEsp)mask|=kImplicitEspMask|(1u<<4u);
        return mask;
    }
    [[nodiscard]] static std::uint32_t sourceMask(
        const P005OriginalP54cDecodedIntegerOp& op,
        std::uint16_t flags) noexcept {
        const auto dep=op.dependency_word;
        const bool bit8=(op.opcode&1u)==0u;
        std::uint32_t mask=0;
        if(dep&kSrcReg)mask|=1u<<(((op.fetchdat>>3u)&7u)&(bit8?3u:7u));
        if(dep&kSrcRm)mask|=1u<<((op.fetchdat&7u)&(bit8?3u:7u));
        mask|=static_cast<std::uint32_t>((dep>>4u)&0xffu);
        mask|=addressMask(op,flags);
        return mask;
    }
    [[nodiscard]] bool agi(
        const P005OriginalP54cDecodedIntegerOp& op) const noexcept {
        auto addr=addressMask(op,op.op_32);
        if((addr&kImplicitEspMask)!=0u &&
           (regmask_modified_&(1u<<4u))!=0u &&
           (regmask_modified_&kImplicitEspMask)==0u)
            addr|=(1u<<4u);
        return (regmask_modified_&addr&~kImplicitEspMask)!=0u;
    }
    Pipe pipe_{};
    std::uint32_t regmask_modified_=0;
    std::int64_t block_cycles_=0,total_cycles_=0;
    std::int64_t decode_delay_=0,decode_offset_=0;
    bool block_open_=false,failed_=false;
};
} // namespace awe64::creative
