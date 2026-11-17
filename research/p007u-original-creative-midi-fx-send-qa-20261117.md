# P007-U — Original Creative Win95 MIDI CC91/CC93 effect-send verification

**Date:** 2026-11-17. PRIVATE original guest test. No proprietary ROM/Win95 assets are stored in the repository.

Two separate complete original-AWE64/Win95 boots were made with the same source-pinned 86Box/EMU8000 core and the same original private Win95 Creative driver, ROM, guest relay and original MIDI Program 40. Each test sent its selected CC91 (General MIDI reverb send) and CC93 (chorus send) values **before MIDI NoteOn**, then played Note 60 (velocity 110), and emitted actual EMU8000 PCM. Both original driver WinMM transitions, original UART sends, NoteOn/NoteOff and stereo PCM measurement returned PASS.

- Dry: `midi(0xB0,91,0)` + `midi(0xB0,93,0)`, accepted 1/1.
- Wet: `midi(0xB0,91,127)` + `midi(0xB0,93,127)`, accepted 1/1.
- Both passes recorded exactly **284,200 float32 PCM values** (142,100 stereo frames) after the NoteOn event with the same original hardware mixer.
- Exact raw local PCM SHA256 in both: `46e743294e64b66bbe251ee4bf202dece844f35d966689a840d275fdd664549a`.
- Float-by-float difference: **0 of 284,200**; peak difference 0, RMS difference 0.
- Both test passes independently reported Creative READY, NoteOn/Off PASS, original nonzero sample count 270,106 and peak 0.02462769.

**Conclusion with narrow scope:** Although the original Win95 Creative WinMM relay acknowledges these standard MIDI controller messages through the original emulated UART, they **do not affect the actual original PCM for this test program and original guest configuration**. Therefore it is **not proven** that exposing reverb/chorus knobs mapped to MIDI CC91/CC93 would work. The P165 Reverb/Chorus controls must remain unavailable until a specific original Creative driver register/effect control path is identified and an A/B PCM difference is demonstrated. Do not replace that path with a modern 125A-owned fake DSP or label the knobs functional.

No licensed binaries, guest disk data or recorded original PCM is uploaded into public GitHub. This is a sanitized QA note only.
