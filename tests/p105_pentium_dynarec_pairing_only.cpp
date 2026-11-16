// P105 standalone synthetic C++23 regression, no original Creative media.
// Tests exactly the bounded P54C U/V pairing rules; not original CPU timing.
#include "../src/creative_compat/P105PentiumDynarecPairing.hpp"
#include <cstdint>
#include <iostream>
#include <type_traits>
using namespace awe64::research_p105;
#define REQUIRE(x) do { if(!(x)) { \
 std::cerr<<"P105_FAIL line="<<__LINE__<<" check="<<#x<<"\n"; return 1;} } while(false)

static constexpr auto reg(std::uint32_t read,std::uint32_t write) {
    return DecodedOp{PentiumOpClass::Reg,PentiumPairClass::Either,
                     read,write,false,false,true};
}
static constexpr auto branch() {
    return DecodedOp{PentiumOpClass::Branch,PentiumPairClass::VOnly,
                     0u,0u,false,false,true};
}
static constexpr auto mem(std::uint32_t read,bool displaced=false) {
    return DecodedOp{PentiumOpClass::ReadMem,PentiumPairClass::Either,
                     read,0u,displaced,false,true};
}

static_assert(unpaired_p54c_cycles(PentiumOpClass::Reg)==1);
static_assert(unpaired_p54c_cycles(PentiumOpClass::ReadMem)==2);
static_assert(unpaired_p54c_cycles(PentiumOpClass::ReadModifyWrite)==3);
static_assert(unpaired_p54c_cycles(PentiumOpClass::Branch)==2);
static_assert(paired_p54c_cycles(PentiumOpClass::Reg,PentiumOpClass::Reg)==1);
static_assert(paired_p54c_cycles(PentiumOpClass::Reg,PentiumOpClass::ReadMem)==2);
static_assert(paired_p54c_cycles(PentiumOpClass::Reg,PentiumOpClass::ReadModifyWrite)==3);
static_assert(paired_p54c_cycles(PentiumOpClass::Reg,PentiumOpClass::Branch)==2);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadMem,PentiumOpClass::Reg)==2);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadMem,PentiumOpClass::ReadMem)==2);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadMem,PentiumOpClass::ReadModifyWrite)==3);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadMem,PentiumOpClass::Branch)==3);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadModifyWrite,PentiumOpClass::Reg)==3);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadModifyWrite,PentiumOpClass::ReadMem)==4);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadModifyWrite,PentiumOpClass::ReadModifyWrite)==5);
static_assert(paired_p54c_cycles(PentiumOpClass::ReadModifyWrite,PentiumOpClass::Branch)==4);
static_assert(paired_p54c_cycles(PentiumOpClass::Branch,PentiumOpClass::Reg)==0);
static_assert(!PentiumP54cPairingOnly::autonomous_scheduler_ready());
static_assert(std::is_nothrow_default_constructible_v<PentiumP54cPairingOnly>);

int main() {
    constexpr std::uint32_t EAX=1, EBX=2, EDI=4;
    // Genuine paired independent register operations: one P54C U/V cycle.
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(reg(EDI,EAX)));
        REQUIRE(s.add(reg(EBX,EBX)));
        REQUIRE(s.valid() && s.partial_pairing_cycles()==1);
        REQUIRE(s.paired_instruction_count()==2);
        s.end_x86_block();
        REQUIRE(s.partial_pairing_cycles()==1);
    }
    // A true source-register hazard defeats U/V pairing.
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(reg(EDI,EAX)));
        REQUIRE(s.add(reg(EAX,EBX)));
        REQUIRE(s.partial_pairing_cycles()==2);
        REQUIRE(s.paired_instruction_count()==0);
    }
    // Short 0x7x and long 0F 8x conditional branch both class Branch.
    // Neither the predicate value nor a captured TSC is a pairer input.
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(branch()));
        REQUIRE(s.partial_pairing_cycles()==2);
        REQUIRE(s.add(reg(EBX,EAX)));
        REQUIRE(s.partial_pairing_cycles()==3);
    }
    // Correct P54C pairing table, not fake "each Jcc always costs 2".
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(reg(EBX,EAX)));
        REQUIRE(s.add(branch()));
        REQUIRE(s.partial_pairing_cycles()==2);
        REQUIRE(s.paired_instruction_count()==2);
    }
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(mem(EBX)));
        REQUIRE(s.add(branch()));
        REQUIRE(s.partial_pairing_cycles()==3);
        REQUIRE(s.paired_instruction_count()==2);
    }
    // Address displacement invalidates this simplified pairing contract.
    {
        PentiumP54cPairingOnly s;
        REQUIRE(s.add(reg(EDI,EAX)));
        REQUIRE(s.add(mem(EBX,true)));
        REQUIRE(s.partial_pairing_cycles()==3);
        REQUIRE(s.paired_instruction_count()==0);
    }
    // Unverified decoder and AGI cannot be converted into an invented TSC.
    for(int i=0;i<2;++i) {
        PentiumP54cPairingOnly s;
        auto bad=reg(EBX,EAX);
        if(i==0)bad.supported_dependency_decode=false;
        else bad.agi_stall=true;
        REQUIRE(!s.add(bad));
        REQUIRE(!s.valid());
        REQUIRE(!s.add(reg(EAX,EBX)));
    }
    // Repeated U/V pairs stay heap-free and deterministic across blocks.
    {
        PentiumP54cPairingOnly s;
        for(unsigned i=0;i<100000;++i) {
            REQUIRE(s.add(reg(EDI,EAX)));
            REQUIRE(s.add(reg(EBX,EBX)));
            s.end_x86_block();
        }
        REQUIRE(s.partial_pairing_cycles()==100000u);
        REQUIRE(s.observed_instruction_count()==200000u);
        REQUIRE(s.paired_instruction_count()==200000u);
    }
    std::cout<<"P105_PENTIUM_DYNAREC_SOURCE_PAIRING_PASS "
       "model=p54c nonmmx source_pair_matrix=12/12 "
       "unsupported_agi=REJECT "
       "actual_creative_instruction_path=NOT_TESTED "
       "autonomous_cpu_timing=NOT_APPROVED\n";
    return 0;
}
