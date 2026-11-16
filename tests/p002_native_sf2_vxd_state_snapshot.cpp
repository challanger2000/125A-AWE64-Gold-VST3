#include "CreativeSf2VxdStateSnapshot.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace awe64::creative;
#define CHECK(x) do {if(!(x)){std::cerr<<"P002_VXD_SNAPSHOT_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}} while(0)
namespace {
void word(CreativeSf2VoiceDescriptorBytes& b,unsigned g,int n) noexcept {
    const auto v=static_cast<std::uint16_t>(n);
    b[2u*g]=static_cast<std::uint8_t>(v);
    b[2u*g+1u]=static_cast<std::uint8_t>(v>>8u);
}
void dword(CreativeSf2VoiceDescriptorBytes& b,unsigned i,std::uint32_t n) noexcept {
    for(unsigned j=0;j<4u;++j)b[i+j]=static_cast<std::uint8_t>(n>>(8u*j));
}
void write16(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& b,
             std::size_t offset,std::uint16_t n) noexcept {
    b[offset]=static_cast<std::uint8_t>(n);
    b[offset+1]=static_cast<std::uint8_t>(n>>8u);
}
void write32(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& b,
             std::size_t offset,std::uint32_t n) noexcept {
    for(unsigned j=0;j<4u;++j)b[offset+j]=static_cast<std::uint8_t>(n>>(8u*j));
}
OriginalSf2VxdSnapshotVoice layer(
    std::uint8_t physical,int root,int fine,
    int lfo1,int trem,std::uint32_t start,std::uint32_t end,
    std::uint32_t loopStart,std::uint32_t loopEnd,int chorus) noexcept {
    OriginalSf2VxdSnapshotVoice source{};
    source.original_midi_channel=0u;
    auto& v=source.assigned_voice;
    v.physical_voice=physical;
    v.played_midi_key=60u;
    v.source_original_voice_verified=true;
    v.lfo={true,0,0};
    v.target={true,0xdeadu,0xbeefu}; // deliberately WRONG: must be replaced
    auto& b=v.verified_after_start_record;
    b[0x8cu]=1u; // original ROM mono poststart
    b[0x88u]=0xe0u;b[0x8au]=60u;
    word(b,58u,root);word(b,56u,100);word(b,52u,fine);
    word(b,8u,255);word(b,19u,42);word(b,5u,lfo1);
    word(b,34u,128);word(b,35u,127);
    word(b,26u,125);word(b,27u,127);
    word(b,28u,127);word(b,29u,0);
    word(b,21u,static_cast<std::int16_t>(0x8000u));
    word(b,23u,static_cast<std::int16_t>(0x8000u));
    word(b,25u,static_cast<std::int16_t>(0x8000u));
    word(b,33u,static_cast<std::int16_t>(0x8000u));
    word(b,22u,trem);word(b,54u,1);
    word(b,15u,chorus);word(b,16u,27);
    word(b,17u,127);word(b,18u,129);
    word(b,36u,9);word(b,37u,0);
    dword(b,0x76u,start);dword(b,0x7au,end);
    dword(b,0x7eu,loopStart);dword(b,0x82u,loopEnd);
    return source;
}
}
int main(){
    // The original VXD layout is a 30*0x20 voice table from +0x42,
    // followed by 16*0x24 MIDI channels from +0x442. A full ROM ID is
    // stored separately at +0x158e. The data here is SYNTHETIC but its
    // layout/addresses/endianness match the executed original x86 code.
    static_assert(kOriginalVxdVoiceRowStart+30*kOriginalVxdVoiceRowStride
                  <=kOriginalVxdChannelRowStart);
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> originalState{};
    write32(originalState,kOriginalVxdRomIdOffset,0x4d474d31u);
    const auto row0=kOriginalVxdChannelRowStart;
    const auto v0=kOriginalVxdVoiceRowStart;
    const auto v3=kOriginalVxdVoiceRowStart+3*kOriginalVxdVoiceRowStride;
    write16(originalState,row0+0x0e,0);
    write16(originalState,row0+0x10,0); // ORIGINAL channel pitch term #1
    write16(originalState,row0+0x12,0); // ORIGINAL channel pitch term #2
    write32(originalState,row0+0x14,0);
    // Original source LCOD+0x574F writes these FROM the encoded
    // descriptor in the *first* NoteOn pass; no synthetic pitch-word
    // defaults are allowed before that source transition runs.
    write16(originalState,v0+0x0e,0);
    write16(originalState,v3+0x0e,0);
    OriginalSf2VerifiedVxdDeviceSnapshot snapshot{};
    snapshot.original_state_bytes=originalState;
    snapshot.independently_verified_original_vxd_snapshot=true;
    snapshot.original_lcod_31word_table_verified=true;
    // One specific authentic scalar 0x9830 is known; rest is synthetic
    // zero and never accepted as full private table extraction.
    snapshot.source_positive_16word_table[10]=0x9830u;
    std::array<OriginalSf2VxdSnapshotVoice,2> pair{{
        layer(0,85,-32,7,254,0x74a48,0x76dd5,0x74a6b,0x76dd1,118),
        layer(3,94,4,0,128,0x34116,0x36804,0x3460e,0x36800,122)
    }};
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    // Execute EXACT source x86 LCOD+0x574F storage for both assigned
    // voices transactionally before interpreting later source state.
    CHECK(candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,true,pair));
    const auto zeroInputs=candidateOriginalSf2TargetStateFromVxdImage(
        snapshot,0u,0u);
    const auto threeInputs=candidateOriginalSf2TargetStateFromVxdImage(
        snapshot,0u,3u);
    CHECK(zeroInputs && threeInputs);
    CHECK(zeroInputs->channel_word_plus_0e==0);
    CHECK(zeroInputs->allocated_voice_word_plus_0e==0xbe3bu);
    CHECK(threeInputs->allocated_voice_word_plus_0e==0xb2b5u);
    CHECK(zeroInputs->channel_dword_plus_14==0u);
    // Existing caller-provided tune and LFO metadata is intentionally
    // wrong; it must be overridden by source channel+0x0A/B/10/12.
    pair[0].assigned_voice.verified_channel_tune_1_cents=999;
    pair[0].assigned_voice.verified_channel_tune_2_cents=-999;
    pair[0].assigned_voice.lfo={false,99,99};
    pair[1].assigned_voice.lfo={false,99,99};
    const auto got=candidateOriginalSf2FullNoteOnFromVxdSnapshot(
        pair,snapshot);
    CHECK(got && got->count==122u && got->voices==2u);
    CHECK(got->out[0].value==0x60u &&
          got->out[5].value==0x63u);
    CHECK(got->out[10+14+1].value==0xbe3bu);
    CHECK(got->out[66+14+1].value==0xb2b5u);
    CHECK(got->out[10+34+12+1].value==0x260cff00u);
    CHECK(got->out[66+34+12+1].value==0x260cff00u);
    CHECK(got->out[10+34+16+1].value==0x12541b81u);
    CHECK(got->out[66+34+16+1].value==0x0ae71b81u);
    CHECK(got->out[10+34+18+1].value==0x12540000u);
    CHECK(got->out[66+34+18+1].value==0x0ae70000u);
    CHECK(got->out[65].value==9u && got->out[121].value==9u);
    CHECK(got->out[65].port==0xa20u);
    // Invariance: resolved state must match the FULL previously source-
    // verified 122 OUT generator, word-for-word, and must OVERRIDE the
    // deliberately fake caller target DEADBEEF above.
    std::array<OriginalSf2FullNoteOnVoice,2> manual{{
        pair[0].assigned_voice,pair[1].assigned_voice
    }};
    manual[0].target={true,0x260cu,0x1254u};
    manual[1].target={true,0x260cu,0x0ae7u};
    // Reference is the separately source-verified original VXD plan;
    // unlike the NEW state adapter, that old composer cannot accept
    // intentionally bogus pitch/LFO operands supplied by the test.
    manual[0].lfo={true,0,0};
    manual[1].lfo={true,0,0};
    manual[0].verified_channel_tune_1_cents=0;
    manual[0].verified_channel_tune_2_cents=0;
    manual[1].verified_channel_tune_1_cents=0;
    manual[1].verified_channel_tune_2_cents=0;
    const auto expected=candidateOriginalSf2FullNoteOnPorts(manual);
    CHECK(expected && expected->count==got->count);
    for(std::size_t i=0;i<got->count;++i) {
        CHECK(got->out[i].port==expected->out[i].port);
        CHECK(got->out[i].access_width_bits==expected->out[i].access_width_bits);
        CHECK(got->out[i].value==expected->out[i].value);
    }
    // A stale or tampered physical voice IP cannot be used as an
    // independent pitch-target parameter. Original LCOD+0x574F MUST
    // have written the same IP from the source descriptor first.
    write16(originalState,v3+0x0eu,0xc800u);
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    CHECK(candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,true,pair));
    CHECK(candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));

    // Original channel state 0x0E signed and 0x14 DWORD are not
    // "device master" fields. Mutate only channel+0x14 and confirm
    // both physical voice target words change, while generator/loop
    // pointer geometry stays unchanged.
    write32(originalState,row0+0x14u,0x1000u);
    const auto channelChanged=candidateOriginalSf2FullNoteOnFromVxdSnapshot(
        pair,snapshot);
    CHECK(channelChanged && channelChanged->out[61].value!=got->out[61].value);
    CHECK(channelChanged->out[117].value!=got->out[117].value);
    CHECK(channelChanged->out[10+34+1].value==got->out[10+34+1].value);
    write32(originalState,row0+0x14u,0);
    // Original LCOD+0x56DE reads two signed channel tuning WORDs.
    // Editing one now invalidates stored voice pitch until the source
    // LCOD+0x574F transition is executed again.
    write16(originalState,row0+0x10u,100u);
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    CHECK(candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,true,pair));
    const auto tuned=candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot);
    CHECK(tuned && tuned->out[25].value!=got->out[25].value);
    write16(originalState,row0+0x10u,0u);
    CHECK(candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,true,pair));
    CHECK(candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    // ORIGINAL LCOD+0x5CEB reads two CHANNEL BYTES at +0x0A/+0x0B.
    // They must affect FMMOD, not the unrelated device+0x0A/+0x0B.
    originalState[row0+0x0au]=5u;
    originalState[row0+0x0bu]=9u;
    const auto channelLfo=candidateOriginalSf2FullNoteOnFromVxdSnapshot(
        pair,snapshot);
    CHECK(channelLfo && channelLfo->out[10+18+3].value==0x1500u);
    CHECK(channelLfo->out[66+18+3].value==0x0e00u);
    originalState[row0+0x0au]=0u;
    originalState[row0+0x0bu]=0u;
    write16(originalState,row0+0x0eu,0xffffu);
    const auto signedInput=candidateOriginalSf2TargetStateFromVxdImage(
        snapshot,0u,0u);
    CHECK(signedInput && signedInput->channel_word_plus_0e==-1);
    write16(originalState,row0+0x0eu,0);

    auto missing=snapshot;
    missing.independently_verified_original_vxd_snapshot=false;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,missing));
    missing=snapshot;missing.original_lcod_31word_table_verified=false;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,missing));
    missing=snapshot;
    missing.original_state_bytes=std::span<const std::uint8_t>(
        originalState.data(),kOriginalVxdMinDeviceImage-1u);
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,missing));
    write32(originalState,kOriginalVxdRomIdOffset,0);
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    write32(originalState,kOriginalVxdRomIdOffset,0x4d474d31u);
    write16(originalState,0x12u,1u);
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(pair,snapshot));
    write16(originalState,0x12u,0u);
    CHECK(!candidateOriginalSf2TargetStateFromVxdImage(snapshot,16u,0u));
    CHECK(!candidateOriginalSf2TargetStateFromVxdImage(snapshot,0u,30u));
    auto wrong=pair;wrong[1].original_midi_channel=1u;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(wrong,snapshot));
    wrong=pair;wrong[1].assigned_voice.played_midi_key=61u;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(wrong,snapshot));
    wrong=pair;wrong[1].assigned_voice.physical_voice=0u;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(wrong,snapshot));
    wrong=pair;wrong[1].assigned_voice.lfo.source_original_state_verified=false;
    CHECK(candidateOriginalSf2FullNoteOnFromVxdSnapshot(wrong,snapshot));
    wrong=pair;wrong[1].assigned_voice.verified_after_start_record[0x8du]=0x80u;
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot(wrong,snapshot));
    const auto originalVoice0Ip=static_cast<std::uint16_t>(
        originalState[v0+0xeu]|(originalState[v0+0xfu]<<8u));
    CHECK(!candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,true,wrong));
    CHECK(originalState[v0+0xeu]==static_cast<std::uint8_t>(originalVoice0Ip));
    CHECK(originalState[v0+0xfu]==static_cast<std::uint8_t>(originalVoice0Ip>>8u));
    CHECK(!candidateOriginalSf2WriteInitialIpToVxdState(
        originalState,false,pair));
    CHECK(!candidateOriginalSf2FullNoteOnFromVxdSnapshot({},snapshot));

    std::cout<<"P002_SOURCE_ORIGINAL_VXD_STATE_SNAPSHOT_PASS "
             <<"30_physical_voices_16_channels_1MGM_dry_mode=TRUE "
             <<"signed_channel_and_unsigned_voice_le=TRUE "
             <<"two_voice_122_TYPED_OUT_IDENTICAL=TRUE "
             <<"original_LCOD_574F_per_voice_IP_state_write=TRUE "
             <<"original_channel_0A_0B_lfo_10_12_tune=TRUE "
             <<"reject_stale_voice_pitches_and_atomic_source_state_write=TRUE "
             <<"state_mutation_changes_only_relevant_voice_targets=TRUE "
             <<"missing_original_snapshot_fail_closed=TRUE "
             <<"real_Win95_runtime_state_acquisition=NOT_PROVEN "
             <<"original_driver_PCM=NOT_PROVEN\n";
    return 0;
}
