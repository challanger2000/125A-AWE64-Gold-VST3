#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::creative;
#define CHECK(x) do{ if(!(x)){std::fprintf(stderr,"P003 x86 allocator CHECK failed: %s (%s:%d)\n",#x,__FILE__,__LINE__);return 1;} }while(false)

int main(){
    std::array<OriginalWin95AllocatorVoiceSnapshot,
               kOriginalWin95PhysicalVoiceCapacity> voices{};
    for(auto& x:voices){
        x.complete=true;
        x.internal_key_marker=0x0ffeu;
        x.volume_target_hi16=0u;
    }
    // 32 physical voices are addressed in the ORIGINAL device state.
    // Mode1 starts from voice31 and first usable pair is 30+31.
    auto p=candidateOriginalWin95Mode1Pair(voices,32,32);
    CHECK(p && p->first_voice==30 && p->second_voice==31);
    CHECK(p->immediate_zero_pair && p->priority==0u);
    // Unlike the old native 30 music budget, original cap is input,
    // and a cap=30 session must not allocate voices30/31.
    p=candidateOriginalWin95Mode1Pair(voices,30,32);
    CHECK(p && p->first_voice==28 && p->second_voice==29);
    CHECK(!candidateOriginalWin95Mode1Pair(voices,1,32));
    CHECK(!candidateOriginalWin95Mode1Pair(voices,33,32));

    for(auto& x:voices)x.volume_target_hi16=500u;
    voices[0].volume_target_hi16=200;
    voices[1].volume_target_hi16=100;
    voices[2].volume_target_hi16=30;
    voices[3].volume_target_hi16=5;
    p=candidateOriginalWin95Mode1Pair(voices,4,32);
    CHECK(p && p->first_voice==2 && p->second_voice==3);
    CHECK(!p->immediate_zero_pair && p->priority==95u);
    // Formula exactly A+B+2*max(A,B). Pair state bit adds 0xC00.
    voices[2].pair_extra_penalty=true;
    p=candidateOriginalWin95Mode1Pair(voices,4,32);
    CHECK(p && p->first_voice==1 && p->second_voice==2);
    CHECK(p->priority==330u);
    voices[2].pair_extra_penalty=false;
    // Reverse scan, tie at equal scores chooses the LOWER pair ID.
    for(auto& x:voices)x.volume_target_hi16=100u;
    p=candidateOriginalWin95Mode1Pair(voices,4,32);
    CHECK(p && p->first_voice==0 && p->priority==400u);
    // Gating one blocked adjacent voice in (mode_count <= 2)
    // invalidates that pair and the next pair that needs it.
    voices[3].blocked_when_few_voices=true;
    p=candidateOriginalWin95Mode1Pair(voices,4,2);
    CHECK(p && p->first_voice==0 && p->second_voice==1);
    // Genuine source skip occurs before any live chip read.
    voices[3].complete=false;
    p=candidateOriginalWin95Mode1Pair(voices,4,2);
    CHECK(p && p->first_voice==0 && p->second_voice==1);
    voices[3].complete=true;
    // Original mode_count>2 does NOT activate this gate.
    p=candidateOriginalWin95Mode1Pair(voices,4,32);
    CHECK(p && p->first_voice==0);
    // Independently reproduced original x86 LCOD+4029..411A failure:
    // when voice3 is blocked and incomplete, voice2 is ALSO skipped
    // before any VTFT read, and previous resets to 0xFFFFF.
    // The old candidate incorrectly returned pair(1,2), priority310.
    // The original source decision path selects (0,1), priority2800.
    for(auto& row:voices) {
        row.complete=true;
        row.internal_key_marker=0x0ffeu;
        row.blocked_when_few_voices=false;
        row.volume_target_hi16=900u;
        row.stored_sample_end=0u;
        row.pair_extra_penalty=false;
    }
    voices[0].volume_target_hi16=900u;
    voices[1].volume_target_hi16=100u;
    voices[2].volume_target_hi16=10u;
    voices[3].blocked_when_few_voices=true;
    voices[3].complete=false;
    p=candidateOriginalWin95Mode1Pair(voices,4u,2u);
    CHECK(p && p->first_voice==0u && p->second_voice==1u);
    CHECK(p->priority==2800u && !p->immediate_zero_pair);
    // Higher mode MUST ignore the blocked flag, but not the missing
    // snapshot: no fabricated VTFT data for voice3.
    CHECK(!candidateOriginalWin95Mode1Pair(voices,4u,4u));
    voices[3].complete=true;
    p=candidateOriginalWin95Mode1Pair(voices,4u,4u);
    CHECK(p && p->first_voice==1u && p->second_voice==2u &&
          p->priority==310u);
    // Original upper capacity check: voice3 is not rejected by a
    // nonexistent voice4 flag; voice3 itself is the blocked source.
    voices[3].blocked_when_few_voices=false;
    p=candidateOriginalWin95Mode1Pair(voices,4u,2u);
    CHECK(p && p->first_voice==1u && p->second_voice==2u &&
          p->priority==310u);
    // Reset the synthetic state before the following older contracts.
    for(auto& row:voices) {
        row.complete=true;
        row.blocked_when_few_voices=false;
        row.volume_target_hi16=100u;
        row.stored_sample_end=0u;
        row.pair_extra_penalty=false;
    }
    voices[3].blocked_when_few_voices=false;
    voices[1].internal_key_marker=0x2000u;
    p=candidateOriginalWin95Mode1Pair(voices,2,32);
    CHECK(!p);
    voices[1].internal_key_marker=0x0ffeu;
    // Pair's sample-end reached sets individual score to zero.
    voices[0].volume_target_hi16=100u;
    voices[1].volume_target_hi16=100u;
    voices[0].stored_sample_end=123u;
    voices[0].chip_current_address=123u;
    voices[1].stored_sample_end=123u;
    voices[1].chip_current_address=123u;
    p=candidateOriginalWin95Mode1Pair(voices,2,32);
    CHECK(p && p->immediate_zero_pair && p->first_voice==0u);
    voices[0].stored_sample_end=0u;
    voices[1].stored_sample_end=0u;
    // Incomplete original live register snapshot MUST fail closed.
    voices[0].complete=false;
    CHECK(!candidateOriginalWin95Mode1Pair(voices,2,32));
    voices[0].complete=true;

    const auto pairWrites=candidateOriginalWin95Mode1PairReleaseWrites(
        OriginalWin95PairSelection{30,31,0,true});
    CHECK(pairWrites);
    CHECK((*pairWrites)[0].port==0xe22u && (*pairWrites)[0].value==0xbeu &&
          (*pairWrites)[0].access_width_bits==8u);
    CHECK((*pairWrites)[1].port==0xa20u && (*pairWrites)[1].value==0xffu &&
          (*pairWrites)[1].access_width_bits==16u);
    CHECK((*pairWrites)[2].port==0xe22u && (*pairWrites)[2].value==0xbfu);
    CHECK((*pairWrites)[3].port==0xa20u && (*pairWrites)[3].value==0xffu);
    CHECK(!candidateOriginalWin95Mode1PairReleaseWrites(
          OriginalWin95PairSelection{31,32,0,false}));
    CHECK(!candidateOriginalWin95Mode1PairReleaseWrites(
          OriginalWin95PairSelection{0,2,0,false}));

    // Mode2 range is packed [low_byte, high_bits) exclusive upper.
    for(auto& x:voices) {x.complete=false;x.stored_sample_end=0;
        x.blocked_when_few_voices=false;}
    for(unsigned i=10;i<15;++i){
        voices[i].complete=true;
        voices[i].internal_key_marker=0x0ffeu;
        voices[i].volume_target_hi16=300u;
    }
    voices[11].volume_target_hi16=50u;
    voices[12].volume_target_hi16=50u;
    voices[13].volume_target_hi16=500u;
    voices[14].volume_target_hi16=70u;
    constexpr std::uint32_t r10to15=(15u<<8u)|10u;
    auto s=candidateOriginalWin95Mode2Range(voices,r10to15,32,32);
    CHECK(s && s->physical_voice==12u && s->priority==50u);
    CHECK(s->reason==OriginalWin95MonoSelectionReason::LowestEnvelopePriority);
    // Immediate idle at first visited physical voice.
    voices[10].volume_target_hi16=0u;
    s=candidateOriginalWin95Mode2Range(voices,r10to15,32,32);
    CHECK(s && s->physical_voice==10u &&
          s->reason==OriginalWin95MonoSelectionReason::CompletelyIdle);
    voices[10].volume_target_hi16=300u;
    // Mode2 sample-end early return, even for marker >= 0x2000.
    voices[10].internal_key_marker=0x2000u;
    voices[10].stored_sample_end=20u;
    voices[10].chip_current_address=20u;
    s=candidateOriginalWin95Mode2Range(voices,r10to15,32,32);
    CHECK(s && s->physical_voice==10u &&
          s->reason==OriginalWin95MonoSelectionReason::SampleEndReached);
    voices[10].internal_key_marker=0x0ffeu;
    voices[10].stored_sample_end=0u;
    // Mode2 in original filters blocked voice when count<=2;
    // should not return the blocked earlier voice.
    voices[11].blocked_when_few_voices=true;
    s=candidateOriginalWin95Mode2Range(voices,r10to15,32,2);
    CHECK(s && s->physical_voice==12u);
    voices[11].complete=false;
    s=candidateOriginalWin95Mode2Range(voices,r10to15,32,2);
    CHECK(s && s->physical_voice==12u);
    voices[11].complete=true;
    s=candidateOriginalWin95Mode2Range(voices,r10to15,32,32);
    CHECK(s && s->physical_voice==12u);
    voices[11].blocked_when_few_voices=false;
    CHECK(!candidateOriginalWin95Mode2Range(voices,(10u<<8u)|10u,32,32));
    CHECK(!candidateOriginalWin95Mode2Range(voices,r10to15,33,32));
    CHECK(!candidateOriginalWin95Mode2Range(voices,r10to15,0,32));
    // Cap clamps packed high to the actual driver's device+0x06.
    CHECK(!candidateOriginalWin95Mode2Range(voices,r10to15,10,32));
    // Live original snapshot missing at an applicable range voice
    // must not be presented as successful voice selection.
    voices[13].complete=false;
    CHECK(!candidateOriginalWin95Mode2Range(voices,r10to15,32,32));
    voices[13].complete=true;
    const auto mode2Release=candidateOriginalWin95Mode2ReleaseWrites(
        OriginalWin95MonoSelection{12,
        OriginalWin95MonoSelectionReason::LowestEnvelopePriority,50});
    CHECK(mode2Release && (*mode2Release)[0].value==0xacu &&
          (*mode2Release)[0].access_width_bits==8u &&
          (*mode2Release)[1].port==0xa20u &&
          (*mode2Release)[1].value==0xffu &&
          (*mode2Release)[1].access_width_bits==16u);
    CHECK(!candidateOriginalWin95Mode2ReleaseWrites(
        OriginalWin95MonoSelection{32,
        OriginalWin95MonoSelectionReason::CompletelyIdle,0u}));
    // Guarded ORIGINAL x86 dispatch: never select stereo/pair mode
    // from a MIDI key, SF2 program or presumed 30-voice budget.
    std::array<OriginalWin95AllocatorVoiceSnapshot,32> dispatchVoices{};
    for(auto& v:dispatchVoices) {
        v.complete=true;
        v.internal_key_marker=0x0ffeu;
        v.volume_target_hi16=0u;
    }
    OriginalWin95AllocatorDispatchInput source{};
    source.original_device_physical_capacity=32u;
    source.original_device_mode_count=32u;
    source.original_x86_dispatch_mode=1u;
    CHECK(!candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source)); // not verified means NO access
    source.independently_verified_source_state=true;
    auto dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==2u &&
          dispatched->physical_voices[0]==30u &&
          dispatched->physical_voices[1]==31u &&
          dispatched->immediate_idle_pair);
    // The mono legacy 30-music-voice scan cannot silently run on
    // device+0x06 =32 until its source scan-limit was verified.
    source.original_x86_dispatch_mode=0u;
    CHECK(!candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source));
    source.independently_verified_mode0_scan_ceiling=30u;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==1u &&
          dispatched->physical_voices[0]==0u &&
          dispatched->physical_voices[1]==kOriginalWin95NoVoice);
    source.original_x86_dispatch_mode=2u;
    source.original_mode2_packed_range=(32u<<8u)|30u;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==1u &&
          dispatched->physical_voices[0]==30u);
    // Genuine empty range produces original 0xFF NO CANDIDATE,
    // while an unverifiable region produces nullopt instead.
    source.original_mode2_packed_range=(32u<<8u)|32u;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==0u &&
          dispatched->physical_voices[0]==kOriginalWin95NoVoice);
    source.original_mode2_packed_range=(32u<<8u)|30u;
    dispatchVoices[31].complete=false;
    // The genuine original mode-2 scorer RETURNS IMMEDIATELY on
    // physically idle voice30. It never reads later voice31; requiring
    // that unused snapshot would incorrectly reject a valid source hit.
    dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==1u &&
          dispatched->physical_voices[0]==30u);
    // Force mode-2 to CONTINUE past voice30. Now voice31's incomplete
    // original register snapshot is genuinely needed and must fail.
    dispatchVoices[30].volume_target_hi16=300u;
    CHECK(!candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source));
    dispatchVoices[30].volume_target_hi16=0u;
    dispatchVoices[31].complete=true;
    source.original_x86_dispatch_mode=1u;
    dispatchVoices[31].complete=false;
    CHECK(!candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source));
    dispatchVoices[31].complete=true;
    // Dispatcher must not preflight blocked rows never read in source.
    source.original_device_mode_count=2u;
    source.original_x86_dispatch_mode=1u;
    dispatchVoices[31].complete=false;
    dispatchVoices[31].blocked_when_few_voices=true;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==2u &&
          dispatched->physical_voices[0]==28u &&
          dispatched->physical_voices[1]==29u);
    source.original_x86_dispatch_mode=2u;
    source.original_mode2_packed_range=(32u<<8u)|31u;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==0u);
    dispatchVoices[31].blocked_when_few_voices=false;
    dispatchVoices[31].complete=true;
    source.original_device_mode_count=32u;
    source.original_x86_dispatch_mode=7u;
    dispatched=candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source);
    CHECK(dispatched && dispatched->voice_count==0u &&
          dispatched->physical_voices[0]==kOriginalWin95NoVoice);
    source.original_x86_dispatch_mode=1u;
    source.original_device_physical_capacity=33u;
    CHECK(!candidateOriginalWin95VxdDispatchAllModes(
        dispatchVoices,source));
    std::puts("P003_ORIGINAL_X86_MODE1_MODE2_NUMERIC_CONTRACT_PASS"
              " original_emu8000_source_modified=NO"
              " original_driver_runtime_replayed=NO");
    return 0;
}
