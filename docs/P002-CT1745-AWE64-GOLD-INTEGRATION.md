# P002 – AWE64 Gold CT1745 wavetable output integration

Based on the independently pinned original 86Box-AWE32 fork research:
https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/research/p002-awe64-gold-signal-path/docs/P002-AWE64-GOLD-WAVETABLE-SIGNAL-PATH-RESEARCH.md

## Exact original source pins

- turican0/86Box-AWE32 release v6.0-AWE32-0.93, commit fee1e81977824fd2d5eed4d239589dc30d251dd7.
- Unmodified snd_emu8k.c Git blob e60eb10f00a8e1187380faa25989f6ba0433d99a.
- Unmodified snd_sb.c CT1745 and AWE64 Gold wavetable callback blob 6d9a2da64e696237e0f3e4f210d75bc6e8ea0a63.
- Unmodified filters.h blob 572137ef02ef1c708e1b6def4487bb53f8230dcf.
- All three are SHA-pinned in CMake for Windows and Linux.

## Source-faithful digital signal path

The unchanged EMU8000 chip owns ALL of its voice interpolation, pan, envelope, chip filter, chorus, reverb, INIT EQ and 57737/65536 output correction. Never double these.

Normal AWE64 Gold VST3 output now takes the original post-chip signed 32-bit stereo audio and performs, in the original snd_sb.c order:

1. CT1745 wavetable/FM left/right attenuation, using the exact original 32-entry table.
2. CT1745 master left/right attenuation, including its original integer-truncated output.
3. CT1745 optional original bass/treble filters, using the 4-bit original source tone table and the low_iir, low_cut_iir, high_iir and high_cut_iir equations and precise original filter coefficients. Per-instance filter state is stored inside the VST3 to avoid cross-instance state leakage from 86Box's global PC-emulator filter arrays.
4. Original CT1745 output gain left/right.
5. Original floating wavetable sink: int32 / 32768.0f without additional PCM16 clipping or normalization.

At the original fork CT1745 reset: FM and master 0xF8/0xF8 -> 32767/32768 each, output gains 0x00 -> 1.0, and bass/treble 0x80 -> bypass at position 8. The source truncation means -40000 -> -39997 and 40000 -> 39997; -32768 -> -32766 and 32767 -> 32765. The full float path can exceed absolute 1.0, exactly as the original fork.

Source peculiarity deliberately retained: the original RIGHT treble-cut dry term uses LEFT out_l rather than right out_r. Replacing this with a supposedly corrected right-channel formula would break bit parity.

A source-verified register application interface accepts the original CT1745 wavetable-related register writes, including source SB16 compatibility aliases, but only when the caller has verified their independent original trace. The VST3 currently uses SOURCE-VERIFIED FORK RESET values; no unsupported guessed Creative Win95 mixer register states or musical-program overrides are generated.

## Preserve two separate acoustic test oracles

**Old chip PCM16/AWE32_WAV oracle**: unchanged original pre-card chip PCM clamped to [-32768,32767], divided by 32768.0f, explicitly selected by the PRIVATE source-regression environment flag AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC=1. This preserves all earlier original-EMU8000 register, note, 30-voice, sustain CC64 and ROM/SF2 diagnostic evidence.

**New post-CT1745 AWE64 Gold float output**: the VST3's normal output, after all authentic CT1745 wavetable card stages, independently compared to pinned original 86Box source.

Implemented in src/card_mixer/OriginalCt1745WavetableMixer.h and .cpp and routed from src/vst3/P001Minimal.cpp; the original snd_emu8k.c and its 28 MiB RAM configuration are UNCHANGED.

## Independent source verification

Linux research C oracle tests/p002_fork_awe64_wavetable_mixer_path.c directly includes and executes the unchanged pinned snd_sb.c static callback. It is not a locally rewritten reference.

Linux cross-language differential test tests/p002_gold_ct1745_fork_differential.cpp directly compares the original C callback to the Windows-ready C++ VST3 adapter, every 32-bit sample of **1,216 stereo frames**. Coverage includes the original reset, nine mixed source-relevant register transitions with persistent filter state, all 16 bass/treble positions on both channels (64 transitions), eight values for each of four original SB16/CT1745 alias registers (32 transitions), six boundary values for all six attenuation/output-gain registers (36 transitions), and rejection of unauthenticated or unsupported register writes with no audible state change. It retains the original right treble-cut asymmetry. The **Linux differential and RAM scanner regression job** succeeded on [Actions #43450886516](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43450886516) at commit `097600c`; this job success is **not** a claim about the separate Windows job or final release approval.

Windows native test tests/p002_gold_ct1745_adapter_contract.cpp checks exact original reset vectors, source tone/filter state transitions, source gate, float no-clamp and untouched PCM16 tap. The actual exported VST3 is independently tested in tests/p001_vst3_host.cpp in both output modes with per-sample original fork reference calculations; its P001 RT heap auditor runs 1200 original ROM MIDI callbacks for EACH path under positive-controlled RtlAllocateHeap interception. The source-derived P002 VST3 diagnostic host remains on the original pre-card PCM16 oracle.

## Real exported VST3 stereo PCM16 WAV capture (P001 baseline)

The independent Windows x64 `tests/p001_vst3_host.cpp` now accepts an
optional **third argument**, a never-existing PRIVATE destination file.
The original required two arguments (VST3 DLL, exact original SHA-verified
external ROM) remain backward compatible. The opt-in host uses the real
exported VST3, requests its original pre-card PCM diagnostic output,
captures **both** left/right channels for 36 × 512 = **18,432 frames** at
44.1 kHz, and after rendering the complete note, creates a standard
44-byte RIFF stereo PCM16 WAV, only with Windows `CREATE_NEW`. It refuses
non-PCM16-exact float values rather than silently rounding or clipping.
The capture starts at sample 0; the actual test-host NoteOn event is at
frame 137. All host file I/O happens **outside process()**.

`tools/p002_verify_real_vst3_pcm_capture.py` independently checks the
resulting WAV lengths, RIFF/fmt/data headers, sample rate/channel format,
137 initial silent frames, audible source material and sample-identical
SHA256 between **two separate exported-VST3 instances**. The Windows
workflow keeps both temporary WAVs under RUNNER_TEMP and deletes them
in a finally block; it does **not upload** any ROM-derived WAV or
private source ROM. Synthetic-only negative tests reject a one-LSB
difference, a one-frame timing shift, early NoteOn PCM, silence,
truncated frames, a wrong sample rate, forged RIFF length, and using
the same file twice. The actual positive exported-VST3 rendering is
separately executed and tested in Windows CI.

This **P001 fixed original ROM MIDI note** is the first genuine
native-exported-VST3 WAV source for the comparison infrastructure;
it is **not yet RAM-SF2** or historical Win95 Creative bank output.
The source-original DRY comparison lives in
`tools/p002_compare_original_dry_pcm.py`, and the private integrated
Win95+SF2+SMALW/SMLD+PCM join lives in
`tools/p002_join_original_win95_ram_evidence.py`. The new host capture
**does not** prove that both full original Win95 RAM-SF2 and native
VST3 captures exist or share identical clocks; those original private
inputs and full VST3 RAM-bank integration remain open.

## Remaining release gates

The original Win95 Creative driver may write non-neutral CT1745 registers; those cannot be inferred from EMU8000 register captures and must await actual original guest mixer-port recording. The test above demonstrates parity with the calibrated original 86Box digital code, not physical analog AWE64 Gold line-out equality. Arbitrary RAM SoundFont Win95-to-VST3 driver and PCM parity, full GUI, DAW/ASIO acceptance and commercial release approval remain OPEN. PR #4 stays DRAFT. No new DSP or waveform normalization is authorized.
