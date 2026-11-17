#include "OriginalCreativeMidiEngine.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace awe64::original_vxd;
#define CHECK(expr) do { if(!(expr)) {std::fprintf(stderr, "NRPN_MIDI_BRIDGE_FAIL line %d %s\n", __LINE__,#expr);std::exit(1);} } while(false)
static void w16(std::vector<std::uint8_t>& d,std::size_t off,std::uint16_t v){
    d[off]=static_cast<std::uint8_t>(v);
    d[off+1u]=static_cast<std::uint8_t>(v>>8u);
}
static std::uint16_t r16(std::vector<std::uint8_t> const& d,std::size_t off){
    return static_cast<std::uint16_t>(static_cast<unsigned>(d[off]) |
           (static_cast<unsigned>(d[off+1u])<<8u));
}
int main(){
    constexpr std::uint8_t channel=3u;
    constexpr std::size_t base=static_cast<std::size_t>(channel)*0x24u;
    constexpr std::size_t param=0x0f5eu+static_cast<std::size_t>(channel)*0x5cu;
    MidiEngine e{};const PortIo io{};const SoundFontSource src{};
    auto cc=[&](unsigned number,unsigned value){
        return dispatch_midi(e,0xb3u,static_cast<std::uint8_t>(number),
           static_cast<std::uint8_t>(value),io,src);
    };
    // There must be no speculative NRPN state change without a verified
    // original installed callback and owner image. In particular, no
    // unrelated GM/RPN emulator fallback on CC98/99.
    CHECK(cc(98,14)==MidiResult::MissingSource);
    CHECK(cc(99,127)==MidiResult::MissingSource);
    CHECK(e.channel[channel].original_rpn_mode==0u);
    std::vector<std::uint8_t> owner(0x1592u,0u);
    e.original_nrpn_callbacks_installed_verified=true;
    e.original_nrpn_memory={owner,true};
    w16(owner,param,1u);
    owner[param+0x3eu+14u]=1u;
    w16(owner,param+2u+2u*14u,0x4657u);
    CHECK(cc(99,127)==MidiResult::Applied);
    CHECK(r16(owner,0x462u+base)==0x7f00u);
    CHECK(cc(98,14)==MidiResult::Applied);
    CHECK(r16(owner,0x45eu+base)==0x0200u);
    CHECK(r16(owner,0x462u+base)==0x7f0eu);
    CHECK(r16(owner,0x464u+base)==0x4657u);
    CHECK(e.channel[channel].original_rpn_mode==0x0200u);
    CHECK(e.channel[channel].original_rpn_aux_462==0x7f0eu);
    CHECK(e.channel[channel].original_rpn_data_lsb==0x4657u);
    CHECK(cc(6,64)==MidiResult::MissingSource);
    CHECK(cc(38,32)==MidiResult::MissingSource);
    CHECK(r16(owner,0x464u+base)==0x4657u);
    CHECK(cc(98,30)==MidiResult::Applied); // original EAX==1, not MIDI error
    CHECK(r16(owner,0x462u+base)==0x7f1eu);
    CHECK(r16(owner,0x464u+base)==0x4657u);

    // After switching back to RPN, owner device and MidiChannel MUST agree.
    CHECK(cc(101,0)==MidiResult::Applied);
    CHECK(cc(100,2)==MidiResult::Applied);
    CHECK(e.channel[channel].original_rpn_mode==0x0100u);
    CHECK(e.channel[channel].original_rpn_number==2u);
    CHECK(r16(owner,0x45eu+base)==0x0100u);
    CHECK(r16(owner,0x460u+base)==2u);
    CHECK(r16(owner,0x464u+base)==0u); // source RPN MSB reset
    CHECK(cc(38,23)==MidiResult::Applied);
    CHECK(r16(owner,0x464u+base)==23u);
    CHECK(cc(6,64)==MidiResult::Applied);
    CHECK(e.channel[channel].original_coarse_tune_verified);

    // Real CC121 runs installed B5A callback at the END, and so the
    // final owner mode is 0x0200, not 0. A null callback instead yields
    // a different source/driver state and must never be guessed.
    CHECK(cc(121,0)==MidiResult::Applied);
    CHECK(e.channel[channel].original_rpn_mode==0x0200u);
    CHECK(r16(owner,0x45eu+base)==0x0200u);
    CHECK(r16(owner,0x460u+base)==0u);
    CHECK(r16(owner,0x462u+base)==0u);
    CHECK(r16(owner,param)==0u);
    for(std::size_t i=0u;i<30u;++i){
        CHECK(owner[param+0x3eu+i]==0u);
        CHECK(r16(owner,param+2u+2u*i)==0x2000u);
    }
    CHECK(r16(owner,0x464u+base)==23u);
    CHECK(e.channel[channel].original_rpn_data_lsb==23u);

    // Invalid/truncated source: fail before mutating any channel state.
    auto const priorMode=e.channel[channel].original_rpn_mode;
    e.original_nrpn_memory.device=std::span<std::uint8_t>(owner).first(0x1500u);
    CHECK(cc(98,4)==MidiResult::MissingSource);
    CHECK(cc(100,1)==MidiResult::MissingSource);
    CHECK(cc(121,0)==MidiResult::MissingSource);
    CHECK(e.channel[channel].original_rpn_mode==priorMode);
    e.original_nrpn_memory.device=owner;
    e.original_reset_callback_absent_verified=true;
    CHECK(cc(98,4)==MidiResult::MissingSource);
    CHECK(cc(121,0)==MidiResult::MissingSource);
    CHECK(e.channel[channel].original_rpn_mode==priorMode);

    // Two MIDI engines with independent owner memories cannot share a
    // mutable driver channel-row reference accidentally.
    MidiEngine other{};std::vector<std::uint8_t> otherOwner(0x1592u,0u);
    other.original_nrpn_callbacks_installed_verified=true;
    other.original_nrpn_memory={otherOwner,true};
    CHECK(dispatch_midi(other,0xb3u,99u,127u,io,src)==MidiResult::Applied);
    CHECK(dispatch_midi(other,0xb3u,98u,1u,io,src)==MidiResult::Applied);
    CHECK(r16(otherOwner,0x462u+base)==0x7f01u);
    CHECK(r16(owner,0x462u+base)==0u);
    std::puts("P003_REAL_MIDI_NRPN_CC98_CC99_CC121_ORIGINAL_DEVICE_BRIDGE_PASS");
}
