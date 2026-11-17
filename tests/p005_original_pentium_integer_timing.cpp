// P005 SOURCE-CORRELATED P54C integer translation timing; no guest
// image, instruction trace, original ROM or Creative binary included.
#include "P005OriginalPentiumIntegerTiming.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
using awe64::creative::P005OriginalPentiumIntegerTiming;
using awe64::creative::P005OriginalP54cDecodedIntegerOp;
#define CHECK(c) do {if(!(c)){std::printf("P005_P54C_INTEGER_FAIL line=%d",__LINE__);return 1;}}while(false)
using Model=P005OriginalPentiumIntegerTiming;
static P005OriginalP54cDecodedIntegerOp op(
    std::uint64_t t,std::uint64_t dep=0u,std::uint32_t fetch=0u,
    std::uint16_t flags=0x300u,std::uint8_t opcode=0x89u) {
    return {t,dep,fetch,flags,opcode,{}};
}
int main() {
    Model m{};
    CHECK(m.beginBlock());
    CHECK(m.append(op(2u,1ull<<12u))); // writes EAX
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==2 && (m.lastModifiedRegisterMask()&1u));
    // Original codegen_timing_pentium_block_start does NOT clear the
    // modified-register mask: a cross-block EAX address use stalls.
    CHECK(m.beginBlock());
    CHECK(m.lastModifiedRegisterMask()==1u);
    CHECK(m.append(op(2u,Model::kModRm,0x00u)));
    CHECK(m.endBlock());
    CHECK(m.lastBlockCycles()==3 && m.totalCycles()==5);
    // 16-bit address-form source quirk: original 86Box ORs numeric
    // register IDs (3|6=7) in its AGI mask, rather than shifting
    // logical BX/SI bits. Preserve the ORIGINAL behavior exactly.
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(2u,1ull<<12u)));
    CHECK(m.append(op(2u,Model::kModRm,0u,0x100u)));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==5);
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(Model::kPairU)));
    CHECK(m.lastBlockCycles()==0);
    CHECK(m.append(op(Model::kPairV)));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==1); // original Reg+Reg U/V table
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(Model::kPairU,1ull<<12u)));
    CHECK(m.append(op(Model::kPairV,1ull<<4u)));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==2); // EAX read/write dependence blocks pair
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(Model::kPairU)));
    CHECK(m.append(op(Model::kPairV|1u))); // Reg+RM timing table=2
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==2);
    // Original COUNT() ignores immediate-width bits of a PAIR_V
    // integer class; they do not turn a legal opcode into INVALID.
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(Model::kPairU)));
    CHECK(m.append(op(Model::kPairV|(1ull<<2u))));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==1);
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(2u,(1ull<<(12u+4u))))); // explicit ESP write
    CHECK(m.append(op(2u,Model::kImplicitEsp)));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==5); // implicit ESP AGI follows explicit ESP
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(2u,Model::kImplicitEsp)));
    CHECK(m.append(op(2u,Model::kImplicitEsp)));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==4); // consecutive implicit ESP: no AGI
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(1u)));
    auto withPrefix=op(1u);
    constexpr std::array<std::uint8_t,1> prefix66{{0x66u}};
    withPrefix.prefixes=prefix66;
    CHECK(m.append(withPrefix));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==3); // prefix consumes 1 decode cycle
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(m.append(op(3u)));
    CHECK(m.append(withPrefix));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==4); // prefix shadowed by previous 3-cycle op
    m.resetSession();
    CHECK(m.beginBlock());
    auto jcc=op(2u);
    constexpr std::array<std::uint8_t,1> prefix0f{{0x0fu}};
    jcc.prefixes=prefix0f;
    jcc.fetchdat=0x85u;
    CHECK(m.append(jcc));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==2); // original Pentium near-Jcc 0F free
    m.resetSession();
    CHECK(m.beginBlock());
    auto d16=op(Model::kMultiCycles|(2u<<8u)|3u);
    d16.op_32=0x200u; // no 32-bit operand flag
    CHECK(m.append(d16));
    auto d32=d16;d32.op_32=0x300u;
    CHECK(m.append(d32));
    CHECK(m.endBlock());
    CHECK(m.totalCycles()==5);
    m.resetSession();
    CHECK(m.beginBlock());
    CHECK(!m.append(op(Model::kFpu))); // FPU requires separate model
    CHECK(m.failed() && !m.blockOpen() && !m.beginBlock());
    m.resetSession();
    CHECK(!m.endBlock());
    CHECK(m.beginBlock());
    const auto invalid=op(0u); // zero CPU-cycle entry is unsupported
    CHECK(!m.append(invalid));
    std::puts("P005_ORIGINAL_P54C_INTEGER_TRANSLATION_PIPELINE_PASS source_86box=PINNED U_V_AGI_PREFIX_BLOCK_MASK=TESTED actual_executed_guest_TSC=NOT_PROVEN");
    return 0;
}
