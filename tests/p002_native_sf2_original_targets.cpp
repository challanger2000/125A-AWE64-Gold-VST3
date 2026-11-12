#include "CreativeSf2OriginalTargets.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(c) do{if(!(c)){std::cerr<<"P002_SF2_ORIGINAL_TARGETS_FAIL line="<<__LINE__<<" "<<#c<<"\n";return 1;}}while(0)
namespace {
void set(CreativeSf2VoiceDescriptorBytes& b,unsigned id,std::int16_t val){
    const auto u=static_cast<std::uint16_t>(val);
    b[2u*id]=static_cast<std::uint8_t>(u);
    b[2u*id+1u]=static_cast<std::uint8_t>(u>>8u);
}
}
int main(){
    CreativeSf2VoiceDescriptorBytes note{};
    note[0x8cu]=1u; // actual original post-note ROM mono qualifier
    // Original private x86 output 0x260c for gen19=42 and attack128;
    // a single independently sourced LCOD lookup word 0x9830, not a
    // baked 16-entry copyrighted driver table.
    OriginalSf2VxdFinalTargetInputs state{};
    state.source_original_device_voice_state_verified=true;
    state.original_lcod_2fa0_16word_table_verified=true;
    state.source_lcod_2fa0_table[10]=0x9830u;
    set(note,19u,42);set(note,34u,128);
    set(note,33u,static_cast<std::int16_t>(0x8000u));
    struct Case {std::uint16_t normalized,pitch;};
    constexpr std::array<Case,4> originals{{
        {0xc800u,0x1c48u}, // original x86 GM0
        {0xbc00u,0x10cfu}, // original x86 GM40
        {0xbe00u,0x1254u}, // original x86 GM100 layer0
        {0xb200u,0x0ae7u}  // original x86 GM100 layer1
    }};
    for(const auto& v:originals){
        // The first per-voice accumulator source is still verified
        // caller input: here injected through the actual VXD +0x0E
        // voice-state word, NOT inferred as a function of GM program.
        state.channel_word_plus_0e=0;
        state.allocated_voice_word_plus_0e=v.normalized;
        state.channel_dword_plus_14=0;
        set(note,7u,0);
        const auto got=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
            note,state);
        CHECK(got && got->original_driver_state_verified);
        CHECK(got->original_pitch_target==v.pitch);
        CHECK(got->original_voice_volume_target==0x260cu);
    }
    // One actual generator delta changes the high PTRX/CPF target:
    // source GEN7 signed*16, not the SF2 generic cents formula.
    state.allocated_voice_word_plus_0e=0xbdf0u;
    set(note,7u,1);
    const auto delta=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(delta && delta->original_pitch_target==0x1254u);
    // Raw volume lookup shifts gen19/16, independent of instrument.
    set(note,7u,0);set(note,19u,58);
    const auto atten58=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(atten58 && atten58->original_voice_volume_target==0x1306u);
    set(note,19u,-16);
    const auto neg=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(neg && neg->original_voice_volume_target==0u);
    set(note,19u,42);
    // Without original special attack condition, volume target is zero.
    set(note,34u,127);
    const auto ordinary=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(ordinary && ordinary->original_voice_volume_target==0u);
    set(note,34u,128);set(note,33u,0x7fffu);
    const auto shortDelay=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(shortDelay && shortDelay->original_voice_volume_target==0u);

    // Source x86 unsigned overflow cap: original initial source sum
    // that wraps negative saturates to FFFF; preserve x86 semantics.
    set(note,33u,static_cast<std::int16_t>(0x8000u));
    state.channel_word_plus_0e=-1;
    state.allocated_voice_word_plus_0e=0u;
    state.channel_dword_plus_14=0u;
    const auto wrap=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(wrap && wrap->original_pitch_target==0xffffu);
    state.channel_word_plus_0e=0;
    state.allocated_voice_word_plus_0e=0;
    set(note,7u,-1);
    const auto negGen=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,state);
    CHECK(negGen && negGen->original_pitch_target==0xffffu);

    // No invented VXD channel-state or proprietary table defaults.
    CHECK(!candidateOriginalSf2ComputeFinalTargetsFromVxdState(
        note,OriginalSf2VxdFinalTargetInputs{}));
    auto notVerified=state;
    notVerified.source_original_device_voice_state_verified=false;
    CHECK(!candidateOriginalSf2ComputeFinalTargetsFromVxdState(note,notVerified));
    notVerified=state;
    notVerified.original_lcod_2fa0_16word_table_verified=false;
    CHECK(!candidateOriginalSf2ComputeFinalTargetsFromVxdState(note,notVerified));
    note[0x8du]=0x80u;
    CHECK(!candidateOriginalSf2ComputeFinalTargetsFromVxdState(note,state));
    note[0x8du]=0u;set(note,34u,129);
    CHECK(!candidateOriginalSf2ComputeFinalTargetsFromVxdState(note,state));

    std::cout<<"P002_ORIGINAL_SF2_FINAL_TARGET_ARITHMETIC_PASS "
             <<"x86_pitch_1C48_10CF_1254_0AE7=4_4 "
             <<"private_source_lcod_table_word_10_verified=TRUE "
             <<"attack128_lookup_260C=TRUE "
             <<"verified_channel_and_voice_state_required=TRUE "
             <<"true_original_x86_execution_and_PCM=NOT_PROVEN\n";
    return 0;
}
