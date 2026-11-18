# P007AA — twelve original Creative Win95 NRPN paths from the real P165 GUI

**2026-11-18 | Status: Windows implementation and mock MIDI delivery PASS; ORIGINAL CREATIVE AUDIBLE RESPONSE NOT YET VERIFIED.**

Provenance:
- Base approved GUI P007Z: `988b04aa2b6a02884d17fc792244919e7f9384d4`, 73% matte-dark original P165 PCB control islands.
- Research branch: `research/p007aa-creative-nrpn-20261118`.
- Latest tested commit: `d1ca6a41eefdb3e9fd5ba5da1443603472a39da6`.
- Windows GitHub Actions: https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44309297142 — **SUCCESS**.

## Original primary sources and exact MIDI protocol
- Creative Sound Blaster AWE32 FAQ, Section F: https://ftp.braindrainlan.nu/Drivers/Sound%20Cards/Awe64gold%20skivan/CREATIVE.WWW/DOCS/V0000162.HTM
- Creative Developer's Information Pack v3, Part IV: https://www.phatcode.net/res/244/files/adip301.pdf
- Exact original channel-0 WinMM MIDI CC transaction per generator: **CC99=127, CC98=generator NRPN LSB, CC6=(8192+actual_value)>>7, CC38=(8192+actual_value)&127**.
- Amp ADSR LSB **11,13,14,15**; Filter Env ADSR **5,7,8,9**; Cutoff/Resonance **21,22**; Reverb/Chorus sends **26,25**. The 12 P165 controls keep stable VST3 IDs **1100..1111**.
- Range fidelity: attack/decay/release 0..5940, sustain/cutoff/resonance 0..127, effect sends 0..255. Normalized 0 maps to NRPN actual value 0, no hidden negative offset.
- Creative's manual lists some generators as **not realtime**: their effective audible change can be restricted to subsequently started notes. The target of control is the genuine Creative Win95 driver, not custom 125A DSP.
- Original Reverb/Chorus variation changes use SysEx. Current six-byte UART message relay and WinMM short-message guest do not transport SysEx; both variation menus remain deliberately disabled. ROM is bundled privately, guest hot-swapping and SF2 bank loading remain unfinished.

## Implemented source path

1. `vst3/P007CreativeNrpn.hpp` authentic 12 generator IDs, range quantization and four-message packing.
2. `vst3/P007Native.cpp` twelve actual VST3 parameters, no GUI-only fake parameters. Changes are queued through a bounded 12-bit dirty mask and audio-callback-owned four-CC sender. Only **one generator** is sent per host callback; rejected UART transactions are not marked complete and replay from CC99 in a later callback. Existing genuine 86Box/EMU8000 core, original Win95 driver and sample/audio code are unchanged.
3. `vst3/P007StateCodec.hpp` new integrity-checked **v2 44-byte** bank/program/master/12-generator state, backwards compatible with old **v1 16-byte** projects. Corrupt/truncated images rejected before state mutation.
4. `vst3/p165/PresetBrowserView.cpp` existing native P165 hardware knobs enabled, labels follow host automation. MOD RESET resets all 12. Original SysEx FX type menus remain disabled and visible.
5. Real host and mock-QA are not an acoustic substitute for actual original driver response.

## Executed Windows evidence

- 47/47 Steinberg VST3 validator tests PASS.
- V2 state roundtrip **80 scenarios**, legacy v1 migration **80**, malformed images rejected **3520** by actual assertions.
- Original Creative NRPN mapper **96 positive** and **13 negative** tests including rejected selector and retry PASS.
- Genuine compiled Win64 VST3: project state with changes to AMP Attack, Filter Cutoff, Reverb Send survives close/new instance and rejects corrupted NRPN state.
- Actual P007 Windows VST3 -> standalone *synthetic-only* test DLL: first CC98 for filter-cutoff intentionally rejects, then correct four-message retry yields original generator LSB 21 with actual value **76** (60% of 127), and AMP generator LSB 11 also arrives. Synthetic constant audio is **not** accepted as Creative PCM; 44.1/48/96k host audio/control/lifecycle PASS.
- Actual compiled native Steinberg Win32 EditorHost: all 12 approved original knobs and ROM/SF2, Reverb/Chorus type, MOD RESET present. The AMP ATTACK original knob was **mouse-dragged 0% → 28%**, shown by real captured pixels and label. Browser/Master/Program navigation passes. The first GUI evidence run failed its pixel-sampling threshold despite real knob movement; the threshold was calibrated against **106 genuine changed full-resolution pixels / ~26 sampled**, and the rerun passed. Screenshot proof under artifact `P007Z-Actual-Original-P165-Complete-GUI-Screenshots`.

## Release blockers

**NO-GO:** These tests prove P165 GUI → host VST3 parameter → real VST3 audio thread → Creative protocol packets, and recall. They **do not** prove the actual Win95 Creative driver applies each offset audibly, since GitHub Actions does not contain the owner's proprietary Win95 image and Creative ROMs. For example, an earlier original-driver test found **CC91/CC93 accepted but 284,200/284,200 PCM values unchanged**, so MIDI acceptance by itself is insufficient. A genuine private guest per-generator dry/wet original PCM differential test with controls that should change the sound is still mandatory. Proper 0% effects-send behavior and musical parameter scaling must be confirmed with genuine driver measurements before declaring the knobs finished. Original Creative SysEx effect variants, SF2/SBK bank upload, sparse preset-browser bank metadata, multi-instance Win95 isolation and final Studio Pro/ASIO acceptance remain incomplete.

Do not merge this branch into P007Z or publish a complete/finished private VST3 without those gates.
