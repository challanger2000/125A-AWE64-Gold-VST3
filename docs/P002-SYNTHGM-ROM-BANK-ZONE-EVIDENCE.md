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
