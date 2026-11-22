# P199 — Reciprocal SF2 stereo samples, original EMU8000 VST3

**Candidate:** \`fix/p199-original-sf2-stereo-linked-pair-20261122\` from exact P198 \`c636c5feba38a7200befcafc0ba2c3ddaf596596\`. This is **not** a new release and does not modify PR #46, the installed P192, the original 86Box EMU8000 DSP, the SF1/SBK path or VST3 state IDs.

**Source defect:** Pinned \`turican0/AWE32Emu\` SF2 parser dropped both 16-bit \`wSampleLink\` at shdr byte 42 and the non-ROM \`sfSampleType\` at byte 44. A legal stereo pair referenced by only one instrument zone consequently loaded one mono sample. A second linked sample's PCM could not reach the original chip. The published SoundFont 2.01 technical specification requires simultaneous linked left/right playback. This is **format-specification-derived**, not yet directly validated against the original Creative Win95 SBAWE.VXD execution.

**Correction:** Retain left/right sample link metadata, check reciprocal partner IDs, direction, equal lengths and rates, and select two original-chip voices if there is only one active instrument zone. If both sides have explicit matching zones, do not double them. Force left/right sample pan in the existing hardware pan representation. Keep all mono and SF1 logic unchanged.

**Adversarial native Windows host test:** Independent synthetic SF2 generator creates four banks with exactly the same LEFT PCM: linked with distinct RIGHT, two explicit matching stereo zones, linked with a zeroed RIGHT, and unlinked mono LEFT. The real built Windows VST3 must emit nonzero EMU8000 PCM and bit-identical single-zone vs explicit-zone stereo, but different PCM when the RIGHT content is zero or linking is disabled. Run 44.1, 48, 96 kHz and notes 60/72. The old loader will fail because it cannot play the right sample. The previous P192–P198, VST3 Validator, 125A Testers and N08 host-state gates stay enforced.

**Before Windows build:** NOT RUN. A green suite is only a scoped SF2 format/audio test.

**Still NOT VERIFIED / release NO-GO:** Original Creative Win95 stereo register/voice ordering and controller interaction, hardware PCM, arbitrary malformed SF2 stereo/ROM references, owner real SF2 bank, Fender Studio Pro/ASIO, performance p95/p99/max/deadlines, full modulation audit and independent Codex review. No private/licensed assets in this branch.
