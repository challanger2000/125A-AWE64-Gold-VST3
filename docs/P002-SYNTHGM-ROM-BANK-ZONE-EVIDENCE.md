# P002: Creative 1MGM/SYNTHGM ROM bank key-zone evidence, not a full soundbank loader

**12 November 2026 – read-only original Creative bank descriptor interpretation is verified against the owner's authentic bank and the independently recorded Win95 Creative EMU8000 IFATN outputs. P003 bank upload and full P002 voice generation are NOT complete.**

## Authentic owner sample (kept private)

- File: SYNTHGM.SBK, **34,832 bytes**, SHA256 **fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586**.
- RIFF form sfbk; original INFO fields isng=EMU8000, irom=1MGM and IPRD=SBAWE32.
- A historical Creative **48-byte** shdr record, NOT a standard later SF2 46-byte sample-header record: 51 such records in this owner bank. Other descriptor sizes include 130 phdr × 38 bytes, 180 inst × 22 bytes, 744 ibag × 4 bytes and 3,644 igen × 4 bytes.
- These are **real original bank metadata fields**, not substituted synthetic bank audio, and no original binary is committed to GitHub.

## Actual MIDI-key-dependent original generator selection

A strict read-only parser in tools/p002_sbk_descriptor_probe.py checks RIFF list boundaries, correct Creative metadata and all required record strides, preset program/bank selection, instrument links, global instrument generators and key range 43 per zone, original generator 8 (raw filter cutoff), raw preset generator 48 (initial attenuation metadata) and original attack VolEnv metadata. Unsupported/multiple zones are rejected, not silently guessed.

For the owner's exact original 1MGM bank, Program 0, note 60 (middle C4):
- Preset Piano 1 → instrument 0, piano1; appropriate instrument zone ibag **3**, key range **58..66**.
- Instrument global raw initial filter cutoff generator 8 = **110**, overridden in this one key zone by **89**.
- Under the measured Creative original 1MGM chip convention, this gives IFATN initial cutoff high byte **89×2 = 178**; true original hardware trace at Velocity 1/64/127: 178.
- The original preset raw initial attenuation generator 48 = **107**, which MUST NOT be mistaken for the previously experimental combined_volume_index=16. Conversion of raw attenuation and actual bank volume/envelope inputs is still unverified.

Program 0, note 67 crosses the key-zone boundary:
- Instrument zone ibag **4** has no local generator 8 override; raw global cutoff **110**, candidate IFATN cutoff high byte **220**.
- Independently recorded P002 original Creative multi-note test measured **IFATN cutoff 220** for NoteOn 67/velocity 110 (versus 178 for NoteOn 64/velocity 90). Hence a single cutoff per GM preset would FAIL actual hardware.

Program 40, note 60:
- Preset Violin → instrument 53, violin; instrument zone ibag **219**, key range **0..70**; raw global filter cutoff generator 8 = **105**.
- Raw initial filter base from true original zone: **210**. The observed velocity-dependent IFATN cutoff is **116** at v≤70, **117** at v71, **165** at v100 and **209** at v127. These exactly match the separately established shared filter candidate that takes the **true bank-derived base 210**.
- Raw preset attenuation generator 48 = **116**; original global attackVolEnv generator 34 = **10**. We record these metadata without inventing an unverified global register mapping.

Program 40, note 72:
- Instrument zone ibag **220**, same raw global cutoff **105**; bank-derived base **210**.
- With v=90, the independently verified conditional filter model yields **(90×210+160)>>7 = 148**, exactly the original 62-message Creative Win95 note72 IFATN cutoff **148**.

The four independent key-zone observations therefore corroborate the key-selected parameter source and the candidate native filter calculation. They do NOT yet establish the universal mapping of every original filter/envelope generator, nor a complete instrument-zone/preset voice allocator.

## Reproducing without publishing Creative assets

The strict script tools/p002_sbk_descriptor_probe.py uses only Python standard library, and accepts private input for read-only checks:

    python tools/p002_sbk_descriptor_probe.py --sbk PRIVATE/SYNTHGM.SBK --verify-original --out PRIVATE/P002-SBK-DESCRIPTOR-REPORT.json

The script refuses an invalid SHA, non-EMU8000/Creative INFO, malformed RIFF lengths, ambiguous key zones and the wrong Creative 1MGM sample-header stride. It verifies the 178, 210, 220 and note72-v90=148 key-zone predictions. The private original package and output report stay outside GitHub.

Public CI at [P002 Windows](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/workflows/p002-windows.yml) runs **synthetic, entirely self-created metadata fixtures** with missing/wrong instrument/zone/chunk negative controls. This is clearly separate from the earlier genuine private-file observation; CI does NOT claim to possess the owner's licensed original file.

## Follow-up engineering gates

1. Derive initial volume and attenuation from the real Creative preset generator 48, ROM sample metadata and 1996 original driver rules. Current volume_index=16/13 inputs are only measured combined terms.
2. Derive the exact original volume-envelope attack-to-velocity-filter-enable rule; seeing original raw attack generator 34 in Violin is evidence, but NOT enough for a universally valid boolean mapping.
3. Move the strictly verified 1MGM descriptor/zone selection into an independently tested **native C++** bank reader, rather than assuming an unsafe generic SF2 parser.
4. Complete multi-zone/layer handling, 32-voice original allocator, sustain/pitch/envelope/FX registers, event ordering and sample-clock; full SF2/SBK sample import remains P003.
5. Continue native Windows tests and the real 125A Plugin Test Lab during all development. Physical Studio One/ASIO only for final completed GUI/product acceptance.

**Never release original Creative binaries or emulate them via ad-hoc per-preset special-cases. P002 PR #4 remains Draft and the P001 original EMU8000 C kernel remains unchanged.**

## P002 native C++ 1MGM bank/zone selector — bounded follow-up

A new native CreativeSbkDescriptor.h/.cpp reads original Creative-shaped
RIFF/sfbk descriptors from caller-owned bytes off the audio thread. It resolves
program/bank/key through preset and instrument bags, global and key-specific
generators, and requires the **Creative-specific 48-byte shdr** stride.
It refuses malformed, unsupported, missing, or overlapping zones rather than
falling back to a constant GM-program cutoff.

CreativeSbkCandidate.h/.cpp is the *bounded native bridge* to the existing
MIDI→IFATN candidate: the selected C++ zone determines the IFATN **cutoff
base**; only the existing observed velocity computation determines its value.
The caller must still supply explicitly unverified combined-volume,
sample-attenuation and velocity-filter-enable terms; raw Creative gen48 is
**not** substituted for any of these. Stale keys are rejected. Voice selection,
program/bank state ownership and clock scheduling remain the caller's work.

The new p002_native_creative_sbk_key_zone_to_ifatn_contract Windows CTest
uses an entirely **synthetic** 1MGM-shaped bank with key 60→base178,
key 67→base220, a real MIDI NoteOn→E22/E20 bridge, and destructive negative
controls for malformed RIFF, forged metadata, 46-byte shdr, invalid generator
indices, wrong program/bank/key and ambiguous overlapping zones. It does not
embed the owner's original Creative bank or claim an actual private-bank
retest. Local Linux GCC C++23 isolated checks were run with address/UB
sanitizers for the reader and a bounded bridge test; Windows Actions on
the resulting branch is the authoritative Windows compilation/test result.

**Limit:** this is a read-only metadata selector plus one candidate register
pair, not a full original bank loader, not a 32-voice allocator, not an
end-to-end independent Creative Win95 MIDI→register→PCM proof, and not
wired into the P001 VST3. P002 remains DRAFT/OPEN; P001 core untouched.

## P002 native zone safety extension — key AND velocity (2026-11-12)

The native C++ descriptor now requires the actual NoteOn velocity, validates
**preset key/velocity ranges (gen43/gen44)** and instrument key/velocity
ranges. The prior generic-SF2 shdr-index check was disproved by
the original Creative 1MGM file and replaced by preservation of raw ROM IDs;
their address resolution is deliberately deferred. The IFATN bridge rejects a stale
selection when either note number **or velocity** differs.

CI's invented metadata fixture deliberately separates instrument velocity
64..127, preset velocity 32..127 and preset keys 50..90; negative tests
require each exclusion to reject the note instead of choosing a wrong voice.
The original owner Creative SYNTHGM.SBK is still private and **was not run**
through this revision in public CI. No bank-agnostic attenuation, attack-
envelope, original Creative allocator or full chip-clock parity is implied.
This remains a bounded safety correction to P002, not a P002 approval.

## Private owner-file gate: real 1MGM sample selectors, not SF2 shdr indices

After retrieving the previously uploaded exact owner file `SYNTHGM(2).SBK`
(34,832 bytes, SHA-256 `fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586`),
the **previous** native C++ selector passed only **4/8** direct original-bank
cutoff checks: Piano program 0 succeeded, but Violin program 40 failed.
A falsely assumed SF2/EOS `sampleID < shdr_count-1` bound caused the rejection.
Authentic Creative **1MGM** descriptors contain 51 special 48-byte shdr records
and ROM generator-53 sample selector values through **151**; program 40 violin
note 60 uses selector **55**, note 72 uses **56**. This demonstrates that these
ROM selectors are **not** generic SF2 sample header indices.

The native 1MGM parser now requires the private-bank `irom=1MGM` identity
in addition to `EMU8000/SBAWE32`, preserves generator-53 selectors as
**unresolved raw ROM IDs**, and no longer indexes shdr with them.
This is not a completed sample-address validation (P003).

**Actual private, native C++23 execution with original owner bank: 8/8**:
Piano GM0 note 60 at velocities 1/64/127 resolves raw89 → 178,
GM0 note 67 velocity 110 resolves raw110 → 220,
Violin GM40 note 60 velocities 1/64/127 resolves raw105 → 210,
and Violin GM40 note 72 velocity 90 resolves raw105 → 210
(base; original velocity-modulated IFATN high byte 148).
The original raw SBK bytes were not committed, and neither are they
present in public Actions.

**Synthetic Windows regression** now requires a ROM selector 151 to remain
readable despite an invented shdr table of only six 48-byte records, and
rejects corrupted `irom` instead of silently parsing a different bank.
Native GCC C++23 tests with ASan/UBSan and the actual owner-file probe
passed locally; CI is a separate Windows proof.

P002 is still OPEN: volume/attack conversion, ROM selector-to-chip address,
voice allocation, all other register writes and exact MIDI-to-register
timing/PCM have not been proven from these tests.


## P002 authentic-bank multilayer inventory and native C++ implementation

The previously uploaded original SYNTHGM(2).SBK, with pinned SHA-256
fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586,
was inspected across ALL 128 melodic GM programs at key 60/velocity 100.
**71 programs produce one instrument layer and 57 produce two.** All 128
have one or two matching layers at this key/velocity. The earlier
single-zone selector can only answer single-layer cases and must NOT
silently represent multilayer GM programs as a single voice.

New independent native CreativeSbkLayers.h/.cpp resolves ALL
matching original RIFF/1MGM preset bags, instrument bags, inclusive
key/velocity zones and raw Creative ROM sample selectors, in order.
It preserves the original 0..63 generator words separately for
**preset-global, preset-local, instrument-global, instrument-local**
scopes. Missing cutoff/envelope/attenuation values remain missing:
no default/gain/attack formula is invented from metadata alone.
This avoids treating raw generator 48 as a native IFATN attenuation
byte, and it avoids treating ROM sample selector 53 as an shdr index.
No dynamic allocation occurs inside the parser; it is intended to be
used off the audio thread, before real-time event handling.

**Real private C++23 original-bank test:** native 128/128 GM programs
at key60 velocity100 matched the independently parsed 71/57
single/two-layer distribution. GM0 Piano 1 raw cutoff89 and
GM40 Violin raw cutoff105 were verified separately, while dual-layer
GM3 Honky-Tonk had *two* independently retained layers with
absent explicit instrument cutoff (no fabricated value).
The actual proprietary source bank remains PRIVATE and is NOT
in GitHub Actions.

**Independent public synthetic Windows CI:** new test
p002_native_creative_sbk_multilayer_contract models two simultaneous
preset/instrument layers with differing generator scopes, key/velocity
ranges and unresolved ROM sample selectors. Negative checks cover
malformed RIFF, missing original irom identity, illegal generator IDs,
invalid MIDI and nonmatching ranges. Native isolated GCC C++23
ASan/UBSan checks passed locally on the synthetic fixture.
This parser is not yet wired into the production P001 VST3.
No original Creative voice allocation or end-to-end register/PCM
equivalence is implied.

**Next evidence required:** map genuine ROM sample selectors and
all layer-specific initial filter/attenuation/envelopes to exact
original Creative Win95 driver register output; reproduce original
32-voice allocation and event order/clock. Do not mistake a bank
layer count for physical EMU8000 voice allocation. P002 remains
OPEN/DRAFT pending the full differential result.


## P002 decoded Creative 1MGM packed ROM sample-address records

Previously the 48-byte Creative shdr record was treated as one generic
SF2-like header, which is WRONG. Authentic 1MGM/SYNTHGM.SBK has **51
48-byte shdr records, each containing 3 consecutive 16-byte ROM sample
records**, for 153 address slots. One 16-byte record consists of four
little-endian uint32 addresses: sample start, sample end, loop start
and loop end. Generator-53 ROM sample ID is the index into these
16-byte records, NOT the 48-byte outer record index.

The author's actual original SYNTHGM(2).SBK and exact original 1MiB
awe32(1).raw (SHA256 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e)
were locally verified: ALL **152 actually referenced ROM sample IDs
0..151** map to 16-byte descriptors with
start < end, start <= loopStart < loopEnd <= end, and sample end no
larger than **524288 original ROM 16-bit words**. For example,
ROM sample ID3 gives start 0xFEB9, end 0x152FE; ID55 (Violin)
start 0x3765B, loopEnd 0x37D18; ID151 end 0x7FC31.
These are the metadata's ROM *word positions*, not yet proven
exact hardware CCCA/PSST/CSL register encoding.

Native **CreativeSbkLayers.h/.cpp** now adds a separately tested
resolveCreative1MgmRomAddress() decoding function.
Original ROM and bank are private and never placed in public CI.
Public synthetic tests check packed 16-byte records, ROM memory bounds,
out-of-range sample IDs, and invalid loop geometry.
Windows CI must separately confirm this source change.

**P002 remaining:** original-driver chip-address translation
including any hardware address-base offset, voice allocation, per-layer
filter/volume/ADSR mapping, clock-exact register ordering, and native
PCM equivalence. An address-descriptor match is NOT an independent
MIDI-to-hardware-register PASS.


## P002 native WinMM MIDI/Program -> original Creative ROM note-plan

New CreativeMidiRomPlan.h/.cpp bridges actual decoded 16-channel
WinMM NoteOn and the channel's live MIDI Program Change state to
all matching original Creative 1MGM instrument layers. It also
resolves each layer's original ROM sample ID to its independently
decoded start/end/loop ROM word addresses. The active Creative bank
is an **explicit** input, not a guessed CC0/CC32 or channel-10 bank.
A correct empty bank/program match is distinguished from malformed
original media. NoteOff and zero-velocity NoteOn cannot create
a new voice plan.

**Actual original private owner-bank test:** WinMM Program Change on
channel 1 to GM3 Honky Tonk followed by NoteOn note60/velocity100
produced two separate original layers and ROM sample address 0xFEB9;
GM40 Violin on channel2 produced one layer starting at 0x3765B;
GM channel10 kick note36 with explicitly selected original SBK bank128
resolved its authentic ROM sample selector 92. No GM-program-specific
DSP workaround and no private bank bytes were committed.

**Public synthetic Windows regression:** p002_winmm_program_to_original_rom_address_contract
checks program switching on two MIDI channels, explicit bank0/128,
ROM word bounds, rejecting NoteOff/velocity-zero and mismatched
programs; isolated GCC C++23 ASan/UBSan checks also passed.
This produces a per-layer *note plan*, not original physical
EMU8000 allocation, register I/O, envelopes, or sample-clock timing.
The P001 VST3 and original EMU8000 kernel remain unchanged.


## P002 complete 1MGM program/key/velocity native-vs-independent audit

The owner's SHA-pinned original SYNTHGM(2).SBK was tested off-CI
over **66048 distinct original-bank note queries**:
128 melodic programs x 128 MIDI keys x four velocities (1, 64, 100, 127)
= 65536, plus original SBK drum bank128/program0 x 128 keys x four
velocities = 512. An independent Python parser of the raw Creative
preset/instrument metadata was compared to the new native C++23
CreativeSbkLayers API on each query, preserving physical layer order,
instrument bag, ROM sample ID and **instrument-scoped** raw cutoff.
**66048/66048 comparisons passed, zero mismatches.**

At the four velocity positions:
- Melodic bank0: 36,380 single-layer and 29,156 two-layer matches;
  no zero-layer matches.
- Drum bank128: 268 zero-layer, 216 single-layer, 28 two-layer matches.

The independent oracle initially produced a false cutoff discrepancy
at GM72 due to its own incorrect fallback from *instrument* generator
8 to *preset* generator 8. Those must remain separate in the
metadata because the original Creative combining rule is not proven.
After comparing matching raw generator scopes independently, there
were **zero** differences. No Creative attenuation, attack, physical
voice assignment, MIDI clock, or chip PCM parity is claimed by this
pure descriptor test. No private SBK or ROM bytes are committed.

Public Windows CI at run
https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43140724440
passed **7/7 CTests and 49/49 Python tests**, plus the actual 125A
Plugin Test Lab P74/P73 headless VST3 inventory. This CI does NOT
have the owner's original bank, so the full 66048 original-bank
oracle was run privately as described, not in public CI.

The next P002 gates remain: source-validated Creative volume and
envelope conversions, original 30/32 voice allocation and controller
events, complete independently generated sample-clock EMU8000 port
sequence, and digital PCM equality under the original Win95 Creative
driver. P002 is DRAFT/OPEN until those gates pass.


## Original Creative Win95: independent MIDI → initial pitch (IP) candidate

Native source `CreativePitchCandidate.{h,cpp}` now generates a **real EMU8000
IP register port pair** (selector E22, IP value E20) from the original
1MGM instrument layer, its raw generator **55**, the MIDI key, and
the active 14-bit MIDI pitchbend state. The physical voice number is
currently supplied explicitly because original Creative voice selection
has not been independently recreated.

**Actual authentic original-bank measurements reproduced mathematically:**
Piano GM0 note60 original zone generator55=7696 gives IP=0xC962;
Violin GM40 note60 original zone generator55=8670 gives IP=0xBC66.
This uses an *experimental original-1MGM relationship*:
base_IP = 0xE000 + floor((key*100 - sourceGen55)*4096/1200).
These are two independent original-driver register observations, not
two hard-coded program branches. At the Win95-measured default bend
range of 200 cents, the signed 14-bit bend coordinate generates
Piano original IP 0xC6B8 (min), 0xC962 (center),
0xCC0C (maximum), exactly. No arbitrary floating-point rounding.

**Important limitation:** generator55 is *format-specific* in
SoundFont 1.0; other historical conversion tools describe it
as a pitch-increment/sample-rate value. The above relationship has
been verified against the cited ORIGINAL 1MGM Win95 IP values,
not proven generically against all SF1 banks, alternate tunings, root
overrides, or sample-rate interpretations. Unsupported nonzero
coarse/fine/forced-key/root/scale variants reject instead of silently
using fabricated defaults. No generator55 present also rejects;
initial pitch outside the EMU8000 16-bit range rejects instead of
wrapping. Pitchbend range beyond the established 200-cent default
is an explicit parameter, not claimed original-driver-validated.

The public new native CTest
`p002_native_original_creative_pitch_bend_candidate`
contains adversarial stale key/velocity/voice, missing bank
pitch, invalid MIDI bend and root/tuning controls.
This is **bounded original-pitch register coverage**, not a complete
note-on sequence or claim of original timer/voice/PCM parity.
The frozen P001 original EMU8000 DSP and VST3 remain unchanged.

## Original installed Creative driver binary: private static reverse-analysis

A local, private copy of the author's existing retro Windows 95 VM was
unpacked from the previously supplied ZIP. The actual installed
Creative binary `WINDOWS/SYSTEM/SBAWE.VXD` (LE, 77,370 bytes, SHA-256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`)
was **read in place**. The driver binary has NOT been copied into
the project or CI.

The original VxD locked-code region contains a cents→IP encoder
(its code object relative offset around 0x53AA) that adds a
16800-cent reference, divides by 1200 per octave and encodes
the remainder with the integer expression 3*r + floor(31*r/75),
with the octave field saturated at 15. This is the mathematically
exact 4096-step/octave fraction. The native
CreativePitchCandidate.cpp now uses this actual driver-derived
integer encoder instead of the prior merely equivalent
approximation. The extracted original-file SHA is the private
provenance lock. Original observed GM0/GM40 C4 IP and the three GM0
bend extremes continue to be required by tests.

Separately, the same real Win95 VxD locked-code branch near 0x4D36
checks the melodic instrument bank (<128) and the **already encoded**
volume-envelope attack rate (<0x7D) before the
((max(70,velocity) * base_cutoff + 0xA0) >> 7)
velocity-filter calculation. This corroborates observed original
Violin 90 velocity cutoff 148 from base210, plus the stationary
GM0 cutoff. The new explicit
candidateOriginalIfatnFromEncodedAttack() accepts that **already
encoded attack rate** and original bank as distinct inputs, with
invalid attack rates rejected; it does NOT assert that SF1/SF2 raw
generator34 directly equals this hardware rate.

**Evidence boundary:** the original binary validates specific
instructions and arithmetic; it does not establish a full
MIDI-program/volume/voice trace equivalence, sample-accurate
register scheduling, exact channel-controller combination or PCM
identity for independently generated P002 sound.


## Original installed Creative Win95 driver: CC7 / CC11 / velocity and ROM attenuation

The original user-supplied Windows 95 image includes actual installed
SBAWE.VXD (original owner SHA-256:
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a).
Static disassembly of its LCOD object around offsets +0x4C8B..+0x4D32
exposes a concrete attenuation calculation, not an imagined soundbank
volume model. The original binary has **not** been uploaded to GitHub.

Native new CreativeControllerVolume.h/.cpp reconstructs the original
numeric conversion from MIDI velocity, channel main volume CC7,
expression CC11, an already-encoded sample attenuation, a signed
amplitude field, and whether the voice uses the original 1MGM ROM.
The bounded implementation uses reconstructed numerical CC7/CC11
conversion tables and the independently measured shared 127-velocity
table, **with original operation order**:

- CC7 <= 10 saturates to 255 (silent).
- For CC7 > 10, add its original conversion table to the shared
  velocity coordinate, and the original encoded amplitude-bucket
  (amplitude+12)/24. Scale by integer 8/3.
- Add the supplied **already encoded** sample attenuation, clamp 255.
- Apply CC11 correction (256-currentAttenuation)*expressionTable/128
  when CC11 < 127.
- **Only for the original 1MGM ROM sample**, add 16, clamp at 255.
  ROM-specific +16 must come AFTER expression, not before.
- Assemble original IFATN high cutoff by the separately recovered
  encoded-volume-attack filter gate and original data3 E22/E20 pair.

For the two genuinely observed original instruments, explicit
sample attenuation inputs (Piano 16 and Violin 8) reproduce
**254/254 original 127-velocity IFATN attenuation values**.
They are measured *encoded* terms, **not an asserted generic
conversion of original SF1 generator48** (Piano raw107/Violin raw116).

Four more independently observed values from the real Creative
Win95 62-MIDI-event 7-scenario matrix are exactly reproduced
for GM0 C4 held at velocity 96:
CC7=20 -> IFATN 0xB285 (atten 133),
CC7=127 -> 0xB22D (45),
CC11=30 -> 0xB26C (108),
CC11=127 -> 0xB22D (45).
These were measured in the original guest and reproduced by the
new native driver-derived formula from MIDI channel state; they
were not copied from an EMU8000 register event stream.
Independent adversarial tests cover CC7 mute threshold,
CC11/ROM calculation order, missing/invalid encoded terms, MIDI
note-on/off and physical-voice bounds.

This is **real additional original-driver register behavior coverage**,
but does NOT establish that every 1MGM bank generator maps to its
correct native encoded sample attenuation and attack, nor does it
supply the original 30-voice allocator, complete register event
order or chip-frame/PCM equivalence. P002 remains OPEN.


## Held-note original Creative CC7/CC11 live register writes

The original 62-event Win95/Cakewalk-free driver matrix holds GM0 C4
velocity96 and changes CC7/CC11 on that **same active voice**.
Native candidateOriginalCreativeLiveVolumeWrites now accepts the
actual MIDI Controller event, already-applied 16-channel MIDI state
and an explicitly assigned/sounding physical EMU8000 voice. It emits
the real DATA3 E22/E20 IFATN word independently from the original
driver trace, preserving the note's original velocity.

The actual original original-driver register samples:
- held C4 velocity96, CC7=20 -> B285,
- same held note, CC7=127 -> B22D,
- same held note, CC11=30 -> B26C,
- same held note, CC11=127 -> B22D.

The Windows CTest asserts all four **held-note updates** (distinct
from fresh NoteOn IFATN initialization) and rejects controllers on
another channel, unrelated controls, stale un-applied MIDI states,
retired voices and invalid physical voice indices. This still requires
the original Creative allocator and sampleclock scheduling to be
implemented separately; testing supplied physical voice 3 does not
validate the original driver's voice selection.


## Pitchbend on a held physical voice (native E22/E20)

The original Creative 62-event Win95 matrix includes live pitchbend
min/center/max while GM0 C4 remains sounding. Native
candidateOriginalCreativeLivePitchBendWrites() now validates the
actual MIDI PitchBend event, the already-applied 14-bit MIDI
channel state, an existing held note plan, the selected instrument
layer and the explicitly assigned physical EMU8000 voice. It emits
the real DATA3 IP E22/E20 port sequence.

Independent original Win95 measured IP values are generated anew:
C6B8 (minimum), C962 (center), CC0C (maximum). Tests include
stale/unapplied event, channel mismatch, non-PitchBend events and
out-of-range physical voice. No event time or voice allocation is
invented. It does NOT itself implement note stealing, original
voice selection, full port ordering or exact frame/PCM parity.

## P002 live stateful voice/controller bridge — bounded native implementation

The first P002 native CreativeBoundVoiceController.h/.cpp
binds a concrete original 1MGM note-plan layer to a **caller-assigned**
real 0..29 physical EMU8000 voice and validated **already encoded**
Creative IFATN terms. The binding independently generates
IP and IFATN hardware port pairs from the same original-bank note
and decoded MIDI state; neither pair is trace replay.

The class owns no sample clock, no 32-voice allocator and no fabricated
complete original NoteOn register sequence. It tracks which physical
voices are bound to MIDI channel/key/velocity, uses fixed-capacity
arrays without allocation and declines duplicate physical assignments.
It does not steal a voice without an external original-allocator
decision.

For live messages, original 16-channel MIDI state updates CC7/CC11
and pitchbend; each currently sounding/sustained bound voice receives
the proper independently generated IFATN or IP port pair.
NoteOff under CC64 sustain does **not** immediately release the voice;
CC64 pedal-up issues an explicit **semantic** release request.
CC123 requests note-off respecting sustain, CC120 requests immediate
kill, and CC121 controller reset preserves program/bank and releases
previously pedal-held voices. These release/kill requests **do not**
invent EMU8000 DCYSUSV values, timings or port writes.

A new public C++ Windows contract tests the actually observed
Win95 original held Piano note 60 velocity96:
CC7 20=>B285, CC7 127=>B22D, CC11 30=>B26C,
CC11 127=>B22D; and IP for bend low/center/high
C6B8/C962/CC0C. Negative controls cover wrong bank, malformed
ROM address, occupied/reserved physical voice, cross-channel
isolation, sustain-held NoteOff and controller state reset.
This is an expanded independent native MIDI->register **subset**;
the full original VXD voice allocation / envelope release register
values / exact event sample clock / full PCM parity remain required.
No private Creative bytes were committed.

## Original Win95 SBAWE.VXD free-mono-voice allocator — proven idle path

The actual private installed SBAWE.VXD LCOD object at approximately
+0x3EA1 contains a physical-voice allocator with **three outer passes
and an inner voice-index increment of three**. The observed idle-mono
search order is 0,3,6,...,27; then 1,4,7,...,28; then 2,5,8,...,29.
The chip uses physical voices 30 and 31 for refresh, not regular
musical allocation. This is corroborated by the previously recorded
actual original Win95 7-scenario matrix, including starts at
physical 0,3,6,9,12.

New C++23 CreativeIdleVoiceAllocator.{h,cpp} implements the exact
**totally idle** branch of that scan, requiring ALL of:
- original driver's assigned-key idle marker 0x0FFE,
- observed EMU8000 register target high16 = 0,
- caller-verified original driver eligibility predicate,
- native voice not currently occupied.

When any input is missing, or all voices are busy, it FAILS CLOSED.
The VXD's separate per-voice ENV/CCCA priority scores and its
mono/stereo/narrow-range steal modes are **NOT** silently simulated.

CreativeBoundVoiceController::bindFirstOriginalIdleMono() integrates
this verified idle subset with the existing original-note ROM
layer plan, independently generated pitch and IFATN port pairs.
No user-supplied physical index is needed **for this confirmed
completely-idle case**. Test cases verify free hardware order
0,3,6, then skip a non-idle voice 9 in favor of 12, then reuse
physical 3 after explicit hardware-released notification. An
independent 30-slot test exercises all three lanes and negative
controls for VXD marker, register highword and unverified eligibility.

This is a real original-driver allocator improvement, not a full
voice-stealing/full-register/PCM parity claim. Original Creative
driver and private VM bytes are not copied into public GitHub.

## P002 actual SBAWE.VXD mode-0 mono VOICE PRIORITY (bounded, read-only)

In addition to free-voice three-lane scanning, the installed original
SBAWE.VXD LCOD procedure +0x3EA1..+0x3FE8 contains a complete
**selection-only** mode-0 mono candidate-ranking branch.

Independent authored C++23 CreativeVxdMonoVoiceRanking.h/.cpp
accepts exact caller-supplied VXD bookkeeping and actual chip register
readings PER PHYSICAL VOICE: internal assigned-marker word,
volume-target register upper 16 bits, envelope status word,
driver-stored sample endpoint, EMU8000 current sample address,
original mode count and the original low-count exclusion flag.
The function declines incomplete hardware/driver snapshots and any
requested voice count above 30 (refresh channels 30/31 excluded).

In the VXD-confirmed order of physical 0,3,...;1,4,...;2,5,...
it (a) immediately chooses a wholly idle marker 0x0FFE whose actual
target high16 is zero, (b) computes exact VXD per-voice score terms
including +0x1200/+0x200 marker penalties, +0x300 when marker
lowbyte=0xFF and +0x1300 for missing envelope 0x8000 status,
with later voices winning score ties, and (c) immediately returns
a sample whose observed current chip address reached its stored
sample endpoint. The original driver also has a separate
low-count admissibility gate.

Public native C++ Windows regression independently exercises all
three branches and deliberate counterexamples, including nonzero
register target, marker range >=0x2000, sample-completion override,
low-count exclusion, equal-score tie, missing snapshot, and physical
30/31 exclusion.

**Important:** a selection result is not a complete original voice
STEAL, not an EMU8000 release/stop instruction sequence and not a
clock-exact native driver. The caller still needs real driver-record
and chip-register inputs; private original traces were not bundled.
In particular, original mode 1 stereo, mode 2 restricted allocation,
precise side effects and the full MIDI→register→PCM original-differential
acceptance remain unimplemented, so P002 remains DRAFT.

## Original VXD selected-voice DCYSUSV register write (2026-11-12)

The original owner's installed SBAWE.VXD LCOD disassembly has the
selection-exit branch at approximately +0x3FBA. Whether the voice
was completely idle, the sample read pointer reached the end, or
it was selected by the lowest priority score, the code sends a word
0x00FF using the original 16-bit register writer (+0x3CB1) with
voice register selector (5 << 5)|physicalVoice to EMU8000 DATA1.
The original Win95 captured trace independently identifies these
physical note-start register events as DCYSUSV=0x00FF.

New native candidateOriginalWin95SelectedVoiceDcysusvWrites()
produces the concrete E22 selector / A20 DATA1 port pair for
all real musical physical voices 0..29. It rejects refresh voices
30/31 and invalid ranking outcomes. The original completely-idle
voice-binding path now returns the concrete DCYSUSV reset pair
separately and ahead of the existing independently calculated
IP and IFATN pairs. This does NOT establish that all other
register writes, release sequence, clock schedule, full voice steal
or sample PCM are correct. The original VXD bytes stay private.

### Port access width: correction from original x86 I/O instructions

The private original SBAWE.VXD writer at LCOD +0x3CB1
uses x86 OUT AL for the E22 selector and OUT AX for the A20
data register. These are **different hardware I/O widths**.
The first native draft represented both as PortWord (no access
width), which was insufficient for strict future bit-exact
original port-trace comparison. The corrected API now exposes
OriginalWin95VxdPortWrite {port,value,access_width_bits}
and the synthetic Windows regressions verify precisely
E22/8-bit followed by A20/16-bit, independently of the
hardware register index and voice number.
The other P002 pitch/IFATN candidates still use the earlier
PortWord abstraction; their original exact I/O widths and
frame-level ordering remain separate unproven gates.

## Driver-derived immediate VTFT hardware write after voice assignment

Original installed Win95 `SBAWE.VXD` LCOD function around
`+0x3DDB..+0x3E07` calls the original 32-bit register writer with
`selector=(3<<5)|physicalVoice` and **32-bit datum 0x0000FFFF**,
after the earlier mono voice-selection branch performed its
`DCYSUSV=0x00FF` 16-bit register write.

The private exact VXD writer around `+0x3D1B` uses `OUT DX,AL` to
select on `E22` (8 bits), and `OUT DX,EAX` to deliver **32 bits** on
`620`. The pinned original fork's register map independently labels
DATA0/index3 as **VTFT**. This is not the same as an A20 16-bit write.

Native `candidateOriginalWin95AssignedVoiceVtftWrites()` produces
these two typed port writes, rejects physical 30/31, and the verified
idle-mono binding returns the required **ordered groups**:
(1) original DCYSUSV selection exit, (2) original DWORD VTFT
assignment, (3) independently calculated IP and IFATN candidates.

The native tests now explicitly reject wrong data-width assumptions
and verify index and pitch/IFATN composition. The remaining
original hardware registers, their ordering and timing, and a
full MIDI-generated PCM comparison are still NOT proven.

## Original SBAWE.VXD group-dependent forced voice termination

Further private disassembly of the installed original Win95 Creative
`SBAWE.VXD` LCOD routine +0x4AB5, invoked by its original voice/note
setup paths near +0x4BEE and +0x5430, identifies a separate
**forced group-termination** sequence for a verified original
channel/group/voice match:

1. Select physical voice's DATA3 index1, write `IFATN=0x00FF`
   (full attenuation) through 8-bit `E22` selector and 16-bit
   `E20` DATA3.
2. Select physical voice's DATA1 index5, write `DCYSUSV=0x807F`
   (release-phase bit plus release value) through 8-bit `E22`
   selector and 16-bit `A20` DATA1.
3. Only if BOTH original per-voice stereo flags are set, repeat
   these exact four hardware accesses for physical voice + 1.
4. Clear the original VXD's per-voice group bookkeeping (not yet
   reconstructed in the native code).

The independent native `CreativeVxdVoiceTermination` implementation
emits typed E22/E20/E22/A20 hardware port writes in correct order
and with exact 8/16-bit widths; it fail-closes unless a caller
**explicitly proves** the original group match and both stereo
conditions. It rejects physical voice 30/31 and a stereo pair
straddling the reserved refresh voices.

This verified VXD routine is **NOT** evidence that general MIDI
NoteOff, sustain-off or CC120 always executes that same sequence.
No live controller behavior is rewired to this routine without
that original-driver path match. The real P002 full NoteOff,
sustain, voice steal side effects, timing and PCM parity gates
remain open. Private original VXD, ROM and SBK bytes stay outside
the repository and public Windows CI.

## P002 actual original 86Box-EMU8000 chip execution of VXD-proven ports

Previous C++ P002 tests only compared the independently generated
port-write arrays. The actual product's P001 Chip adapter takes 16-bit
port values while the proven VXD paths require an 8-bit `E22`
register selector, 16-bit DATA1/DATA3 writes and a **32-bit**
DATA0 VTFT word. The original pinned fork itself implements
`emu8k_outb(E22)` as a zero-extended call to `emu8k_outw(E22)`,
and supports 32-bit VTFT via two consecutive 16-bit halves
(`620` low and `622` high), both through the real chip engine.

New `CreativeChipPortBridge` validates **the entire sequence**
BEFORE any hardware write and only accepts the explicitly verified
selector/register/width combinations:
- E22 pointer width8 then E20 width16 for DATA3 idx1 IFATN;
- E22 pointer width8 then A20 width16 for DATA1 idx5 DCYSUSV;
- E22 pointer width8 then 620 width32 for DATA0 idx3 VTFT.

It rejects malformed widths, values, refresh voices 30/31,
unauthorized register addresses and any unavailable chip.
When the sequence is valid, it executes directly through the
real P001 Chip adapter without modifying the calibrated
`snd_emu8k.c` fork.

The new `p002_real_original_dsp_vxd_register_execution` native
Windows test creates a **synthetic zero-ROM only**, opens the
authentic 28-MiB fork core, applies independently generated
VXD original idle-voice DCYSUSV=0x00FF and VTFT=0x0000FFFF,
and checks the **actual EMU8000 voice[3] register state**.
It then applies the verified VXD group-choke path and checks
physical voice3 IFATN=0x00FF, DCYSUSV=0x807F and independent
voice6/7 stereo register state. Negative controls reject wrong
selector width, corrupt DWORD port, high refresh voice numbers
and truncated 16-bit values, with no partial mutation.

The synthetic zero-ROM test does NOT and cannot prove non-silent
original-Creative PCM equality, true NoteOff release, full NoteOn
instrument envelopes, original VXD timing, or end-to-end P002
audio fidelity. This is the first bounded genuine VXD-derived
register **execution** path on the original fork, not just candidate
array comparison. Public CI still ships no original driver or ROM.

### Extended original-bank/MIDI IP + IFATN executed on pinned EMU8000

The P002 bridge now also validates original DATA3 register0 initial
pitch IP, in addition to DATA3 register1 IFATN and the previous
VXD voice-init and termination commands. This permits a genuine
WinMM MIDI note/velocity and live controller state to reach the
real original fork's **voice registers**, not only an in-memory
list of candidate port writes.

The updated C++ Windows test runs entirely against the *actual*
pinned unmodified EMU8000 core with synthetic ROM content and
explicitly documented original GM0 note60 bank data:
original pitch generator55=7696, encoded original 1MGM
amplitude/attenuation terms, cutoff 178 and original controller
table. The native sequence generated from the MIDI/bank candidate
writes is executed and checked **on chip**:

- original GM0 C4 hardware IP = 0xC962 (from MIDI and bank);
- hardware IFATN cutoff high byte = 178;
- live channel CC7=20 updates the real voice's IFATN to 0xB285;
- live pitchbend maximum updates that voice's hardware IP to 0xCC0C.

No real Creative ROM data or captured reference trace is in CI.
This verifies correct *hardware register state* for a proven
MIDI/controller subset, but **not** original VXD complete register
write ordering, real audio output, FX sends, chip-clock timestamps
or original Win95 chip PCM identity. The actual P001 chip source
has NOT been modified.


## Original Creative 1MGM ROM CSL/PSST update: VXD verified case5/case6

The private actual installed Windows95 SBAWE.VXD LCOD sections +0x1B4A..+0x1C14
and +0x1C19..+0x1CF7 implement two DISTINCT sample address UPDATE branches
selected by the VXD's actual case value 5 or 6. They are not generally proven
to run on all MIDI NoteOn events.

Driver case 5 writes DATA0 register7 CSL low24 = sample loop end + 1;
then DATA0 register6 PSST low24 = sample loop start. The original PSST high
byte from a LIVE chip register read is preserved unchanged.

Driver case 6 writes CSL low24 = sample end + 8; then PSST low24 =
sample end + 4, preserving the current high PSST byte.

Both cases write CSL BEFORE PSST using E22 OUT AL (8-bit selector) and 620
OUT EAX (32-bit DATA0). The pinned, unchanged original fork independently
names DATA0 register7 CSL and register6 PSST.

Native CreativeVxdSampleAddress.h/.cpp implements both alternatives with
explicit caller-specified original driver case, observed current PSST,
and the exact original Creative 1MGM ROM sample-address descriptor.
It rejects corrupt and out-of-1MiB ROM geometry, reserved voices 30/31,
and any unsupported case. No guessed note flags are used.

Synthetic public Windows tests check only numerical facts from the owner's
authentic ROM sample ID3 (start FEB9, end 152FE, loop-start 152BA,
loop-end 152F8): physical voice3/case5 CSL=0x152F9 PSST=0x470152BA
(preserves high byte 47); case6 CSL=0x15306 PSST=0x47015302.
Last original ROM sample selector151 and negative cases are also covered.

CreativeChipPortBridge extends its whitelist to actual 32-bit
DATA0 registers6/7. A native P002 test applies these original-driver
updates to physical voice3 of the **actual pinned unchanged EMU8000**,
checks real chip voice[3].csl and voice[3].psst for both operations,
and uses a real prior PSST hardware write as its high-byte source.
No original private ROM/SBK/VXD bytes are in public CI.

This is evidence for authentic sample loop address I/O subset only.
Full NoteOn register order/timing and audible PCM parity still NOT proven.


## P002 genuine Windows95 SBAWE.VXD MIDI NoteOff versus sustain (new)

A direct disassembly of the user's already installed private Win95 Creative
SBAWE.VXD found the **genuine MIDI note-off VXD routine** +0x6072..+0x6185,
whose +0x60E4 helper differs materially from the previously reconstructed
**forced group choke** (+0x4AB5). The original driver first matches its own
per-channel/per-note voice identity. If the note's original sustain-state
byte is nonzero, the VXD changes the stored identity's low byte to 0xFF
and **issues no chip writes**. It only releases the voice when its original
sustain gate permits.

For a verified matching note and an allowed release, the original writer
emits **in order**:

1. DATA1 index5 **DCYSUSV = 0x8000 | encoded volume release byte**,
   via OUT AL E22 (8-bit) and OUT AX A20 (16-bit).
2. DATA1 index7 **DCYSUS = 0x8000 | encoded modulation release byte**,
   same 8/16-bit I/O widths. This is **NOT** an IFATN=0xFF force choke.
3. Only if the original driver's stored sample-end address is nonzero,
   DATA0 index7 CSL = (old CSL high byte) | ((sampleEnd+4)&0xFFFFFF),
   then DATA0 index6 PSST = (old PSST high byte) | (sampleEnd&0xFFFFFF).
   The original VXD uses 32-bit DATA0 OUT EAX and reads both actual chip
   high bytes before overwriting the low address fields.
4. Finally the driver marks its internal voice key word 0x0FFE. This
   internal bookkeeping is reported, **not** mistaken for a completed
   audio release or automatic available hardware voice.

New native CreativeVxdNoteRelease.h/.cpp returns exactly 0/4/8 typed port
writes given **verified original record and hardware inputs**. The raw
SoundFont generator mappings into the already encoded two release bytes
are not guessed; an absent driver voice-state snapshot fails closed.

Windows tests exercise genuine release ordering, both independent envelope
bytes, deferred sustain NoteOff with ZERO chip writes, continued sample
playout using prior CSL/PSST high bytes, reserved physical voices, missing
original matching IDs/record, and 24-bit address overflow. The extended
real chip-port test applies that genuine original release to an **actual
pinned EMU8000 voice**, checking both independent 16-bit envelope
registers, CSL and PSST in the chip's internal voice structure, without
shipping any original ROM/VXD/SBK private bytes.

**Evidence boundary:** P002 now has a bounded genuine original-VXD NoteOff
path, but the live event controller still needs the original driver voice
record and encoded rate conversion to dispatch it automatically. Complete
ADSR/program/filter/FX/voice-start native MIDI translation, clock-level
original trace differential and independently generated PCM remain OPEN.


## P002 original Creative Win95 NoteOn envelope/LFO prelude, 7 actual writes

The actual private installed SBAWE.VXD LCOD +0x5BE4..+0x5CA4 proves
that a new voice first writes these **seven distinct** hardware registers
in this exact order, via real 8-bit E22 pointer port and 16/32-bit data:

1. DATA0 index3 VTFT = 0x0000FFFF (32-bit 620 DATA0).
2. DATA0 index2 CVCF = 0x0000FFFF (32-bit 620 DATA0).
3. DATA2 index4 ATKHLDV = combine(encoded voice record word+0x46,
   byte from original word+0x44; an exact raw 0x0080 is replaced with
   0x7F before byte combination) through A22 (16 bit).
4. DATA2 index5 LFO1VAL = original voice record word+0x2A (A22).
5. DATA2 index6 ATKHLD = combine(original record word+0x36,
   lowbyte word+0x34 with same 0x0080 => 0x007F substitution) (A22).
6. DATA1 index7 DCYSUS = combine(original record word+0x3A,
   original lowbyte record+0x38) (A20).
7. DATA2 index7 LFO2VAL = original voice record word+0x2E (A22).

This is an independently transcribed **exact original-VXD register
sequence subset**. It does not claim the pre-encoded 0x94-byte original
record is reconstructed from every SoundFont generator. The new native
CreativeVxdVoiceEnvelopePrelude.{h,cpp} requires an explicitly verified
original encoded record; it does not invent volume/modulation attack,
sustain or LFO values. Separate from the MIDI NoteOff and VXD group choke
paths, and separate from the subsequent native initial IP/IFATN writes.

The Windows C++ test enumerates all seven original hardware writes with
8/16/32-bit access widths, exact selector and data ports and the original
0x0080→0x007F special case. Additional **actual real pinned EMU8000**
chip tests now execute all seven on a physical voice3, then inspect its
true VTFT, CVCF, ATKHLDV, LFO1VAL, ATKHLD, DCYSUS and LFO2VAL fields.
The original DSP source has not changed. Full original event timing and
MIDI-derived audio PCM parity are still open.


## Original VXD note-start CCCA, physical ROM playback cursor and filter Q

The installed original SBAWE.VXD LCOD around +0x5B81 and +0x5F1B..+0x5F58
reveals **two different writes** to the actual DATA1 index0 CCCA 32-bit register
at the start of a voice. Its first CCCA word uses the original sample-start
word address **minus four**; its later CCCA word adds the **already encoded
filter resonance field** in bits 31..28. These two writes must remain
separate because other real VXD envelope and voice-register writes occur
BETWEEN them. The chip's original index0 DATA1 maps to E22 8-bit pointer
and A20 32-bit data port, not to the 620 VTFT/CSL/PSST DATA0 group.

Native CreativeVxdSampleAddress now exposes both distinct CCCA phases,
using the exact original ROM sample descriptor (start>=4 and within
1MiB) and explicitly verified original filter-Q value 0..15. It fails
closed if the Q conversion from original VXD driver state is unavailable.
There is no per-preset filter tuning or guessed default.

The pinned-chip bridge now supports a VXD-verified DWORD A20 for DATA1
CCCA, splitting it over A20/A22 only when its input access width is
**32 bits**, and explicitly rejects unauthorized index/port combinations.
The real-chip Windows test checks physical voice3 CCCA=0000FEB5 from
original sample3 start FEB9; then CCCA=5000FEB5 for an example verified
encoded Q=5. The test input Q=5 is a **synthetic encoded-record probe**,
not an unverified assertion that the original GM0 preset has Q=5.

The original complete voice sequencing, subsequent generator mapping,
register readback differential and audible original-driver PCM parity
remain open. All original bank/ROM/driver bytes remain private.


## Original Voice NoteOn ROM PSST/CSL geometry from installed VXD

Actual owner's SBAWE.VXD LCOD +0x5B81..+0x5BE4 and
+0x5EEB..+0x5F1B provides the **original new-voice geometry**,
independently of its earlier sample-UPDATE case5/6 routines.
From the already encoded original voice record's 0x6C flag byte:

* **Loop bit0 set**: PSST low24=loopStart−1, CSL low24=loopEnd−1.
  If bit1 is ALSO set, store the sampleEnd+4 as the original VXD
  per-voice future NoteOff end marker. Otherwise store zero.
* **Loop bit0 clear**: PSST low24=sampleEnd+4, CSL low24=sampleEnd+8,
  and original NoteOff end marker zero.

The VXD writes PSST **before** CSL during initial NoteOn (the
sample-update case5/6 functions write CSL before PSST). Both
preserve the correctly encoded pan and chorus send bytes from the
original VXD 0x94-byte generator record, respectively, in bits31..24.
Data0 32-bit on port 620, E22 8-bit selector. The sample start cursor
CCCA (start-4 and later Q) is a separate later part of the NoteOn path.

Native CreativeVxdSampleAddress::candidateOriginalVxdInitialRomNoteGeometry
implements this authentic branching from actual 1MGM sample word
addresses, caller-supplied verified original mode and pan/chorus bytes,
without per-preset hardcoding or invented SF2 generator-to-byte mappings.

Windows tests verify original ROM sample3: with example encoded pan=0x47,
chorus=0x53 and mode3, PSST=0x470152B9, CSL=0x530152F7, retained
release marker=0x15302. In one-shot mode0 PSST=0x47015302,
CSL=0x53015306. **These pan/chorus/mode numbers are synthetic
verified-record inputs for deterministic chip testing, NOT proven
actual original GM0 voice program values.** The register pairs are
executed on the real unchanged pinned EMU8000, inspecting its physical
voice[3].psst and voice[3].csl after both runs.

Original generator-to-voice-record conversion, full trigger/FX timing,
MIDI→original register oracle and native PCM parity remain open.
