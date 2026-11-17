#include "../src/creative_compat/P114OriginalDynarecBlockClock.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
using awe64::research_p114::OriginalDynarecBlockClock;
#define CHECK(x) do {if(!(x)){std::cerr << "P114_FAIL line=" << __LINE__ << " expr=" << #x << "\n";return 1;}}while(false)

// Separate simple oracle matching source execution order, not a test of
// original Win95 instruction costs or wall-clock/digital audio parity.
static std::uint64_t source_update_tsc(std::int64_t spent,
                                      std::uint64_t initial,
                                      std::uint64_t now) {
    const std::uint64_t delta = now-initial;
    if(spent > 0 && static_cast<std::uint64_t>(spent)>delta)
        return now + (static_cast<std::uint64_t>(spent)-delta);
    return now;
}
static_assert(std::is_nothrow_default_constructible_v<OriginalDynarecBlockClock>);
static_assert(!OriginalDynarecBlockClock::autonomous_noteon_ready());

int main() {
    std::uint64_t t=0;
    // Three physical accesses with NO new CPU charge share the same TSC.
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(1000,100, true));
        CHECK(c.charge_executed_cycles(4));
        CHECK(c.port_timestamp(t) && t==104);
        CHECK(c.port_timestamp(t) && t==104);
        CHECK(c.port_timestamp(t) && t==104);
        CHECK(c.charge_executed_cycles(3));
        CHECK(c.port_timestamp(t) && t==107);
        CHECK(c.finish_block(t) && t==107);
        CHECK(!c.port_timestamp(t));
    }
    // A cycle charged before a callback must not be booked twice.
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(500,1000,true));
        CHECK(c.charge_executed_cycles(8));
        CHECK(c.port_timestamp(t) && t==1008);
        CHECK(c.port_timestamp(t) && t==1008);
        CHECK(c.charge_executed_cycles(5));
        CHECK(c.port_timestamp(t) && t==1013);
        CHECK(c.finish_block(t) && t==1013);
        CHECK(c.begin_block(500,t,true));
        CHECK(c.charge_executed_cycles(2));
        CHECK(c.port_timestamp(t) && t==1015);
        CHECK(c.finish_block(t) && t==1015);
    }
    // Timer updates are ALREADY accounted for in tsc, do not double-book.
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(100,2000,true));
        CHECK(c.charge_executed_cycles(3));
        CHECK(c.advance_interim_tsc(12));
        CHECK(c.port_timestamp(t) && t==2012);
        CHECK(c.charge_executed_cycles(15));
        CHECK(c.port_timestamp(t) && t==2018);
        CHECK(c.finish_block(t) && t==2018);
    }
    // Optional USE_ACYCS contributes at update_tsc, NOT at block finish.
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(100,3000,true));
        CHECK(c.charge_executed_cycles(4));
        CHECK(c.set_acycs(3));
        CHECK(c.port_timestamp(t) && t==3007);
        CHECK(c.finish_block(t) && t==3007);
    }
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(100,3000,false));
        CHECK(c.charge_executed_cycles(4));
        CHECK(c.port_timestamp(t) && t==3004);
        CHECK(!c.set_acycs(3) && !c.valid());
    }
    // Explicit rejects for unknown CPU events, illegal lifecycle, overflow.
    {
        OriginalDynarecBlockClock c;
        CHECK(!c.charge_executed_cycles(1));
        CHECK(!c.begin_block(100,0,true));
    }
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(100,0,true));
        CHECK(!c.begin_block(100,0,true));
    }
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(100,0,true));
        CHECK(!c.charge_executed_cycles(-1));
    }
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(0,std::numeric_limits<std::uint64_t>::max()-1,true));
        CHECK(c.charge_executed_cycles(3));
        CHECK(!c.port_timestamp(t));
    }
    {
        OriginalDynarecBlockClock c;
        CHECK(c.begin_block(0,0,true));
        CHECK(c.charge_executed_cycles(std::numeric_limits<std::int32_t>::max()));
        CHECK(!c.charge_executed_cycles(2));
    }
    // Explore many synthetic source-state combinations independently and
    // compare every port callback and block-end with the source formula.
    {
        std::mt19937 rng(0x50114);
        for(int i=0;i<20000;++i) {
            const std::int32_t budget=50000;
            const std::int32_t a=static_cast<std::int32_t>(rng()%201);
            const std::int32_t b=static_cast<std::int32_t>(rng()%201);
            const std::int32_t aux=static_cast<std::int32_t>(rng()%41);
            const std::uint64_t starting=static_cast<std::uint64_t>(i)*100000;
            const std::uint64_t interim=rng()%300;
            const bool jit=(rng()&1u)!=0;
            OriginalDynarecBlockClock c;
            CHECK(c.begin_block(budget,starting,jit));
            CHECK(c.charge_executed_cycles(a));
            CHECK(c.advance_interim_tsc(interim));
            if(jit) CHECK(c.set_acycs(aux));
            CHECK(c.port_timestamp(t));
            std::uint64_t oracle=source_update_tsc(a+(jit?aux:0),starting,starting+interim);
            CHECK(t==oracle);
            CHECK(c.port_timestamp(t) && t==oracle);
            CHECK(c.charge_executed_cycles(b));
            CHECK(c.port_timestamp(t));
            oracle=source_update_tsc(a+b+(jit?aux:0),starting,oracle);
            CHECK(t==oracle);
            CHECK(c.finish_block(t));
            oracle=source_update_tsc(a+b,starting,oracle);
            CHECK(t==oracle);
        }
    }
    std::cout << "P114_BLOCK_TSC_ACCOUNTING_PASS 20000_synthetic_differentials="
                 "PASS consecutive_port_same_tsc=PASS timers_no_double_count=PASS "
                 "full_original_noteon_path=NOT_TESTED autonomous_noteon=NO_GO\n";
}
