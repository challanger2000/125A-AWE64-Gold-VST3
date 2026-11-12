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
