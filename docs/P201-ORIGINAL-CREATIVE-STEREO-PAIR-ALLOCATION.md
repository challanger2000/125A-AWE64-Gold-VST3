# P201 — Creative Win95 physical adjacent stereo voice pair

**Candidate:** `fix/p201-original-stereo-adjacent-pair-allocation-20261122` from P200 diagnostic HEAD `3f6b8811ff5ae0d731fde20e260dd79ebd839699`. Original Creative reference is from the owner's existing separately verified original Win95 VXD and C++23 clean-room research port, not from modern SoundFont playback. The original 86Box DSP and stable main, PR #46 and owner-installed private P192 are untouched.

## Independent original evidence

The existing original VXD research sources under `third_party/creative-vxd-port/src/OriginalCreativeStereoWorklist.cpp`, `OriginalCreativeMidiEngine.cpp` and `OriginalCreativeVxdLive.cpp` implement source-verified `allocate_pair_mode1` and document 25 authentic original owner Win95 stereo starts. The Worklist assigns **LEFT** to physical **base+1**, **RIGHT** to physical **base**, programs left then right, and after both main voice programs starts **RIGHT(base) first, LEFT(base+1) second**. Hardware pair voices are necessarily adjacent. This is distinct from an independent mono two-layer instrument.

**P200 failed this full hardware contract:** It called the regular three-lane mono `AllocateVoiceVxd` twice and then triggered the two returned voices in input order. A newly created P200 native port-trace test exposed additional *harness bugs*: the first Windows run treated legitimate `R` read events as malformed write records; the second counted both the zero-initialization and final nonzero CPF write as voice main completion. Those failures are retained; no claim of prior positive Windows acceptance.

## Correction

P201 introduces a separate, bounded Win95 stereo pair allocator in the pinned native reconstructed Synth (NOT the 86Box chip DSP), derived from the original descending adjacent mode1 scan and physical register reads. It assigns two physical adjacent voices and observes original LEFT upper/main -> RIGHT lower/main -> RIGHT lower note-on -> LEFT upper note-on. P200 immediate-trigger mono and all SF1/SBK/ROM, host states and parameter IDs are unchanged. It reuses the actual EMU8000 port path with no new heap allocation.

**Original-port boundary:** The pinned existing Synth does not model all original voice+0x1a pair penalties and mutable device+0x18 flags; the approximation of those flags is NOT original VXD full fidelity. The public synthetic test proves cold and ordinary chip allocation/control order only. No claim of bit-identical original guest/hardware selection for all polyphonic/ROM/stealing states.

## Independent Windows regression oracle

On the actual compiled Win64 original-chip source, the port trace parser correctly excludes read-marked `R` lines and zeroed CPF prologue writes, then checks:
- reciprocal one-zone SF2 stereo: physical pair adjacency, left upper first main, right base second main, then right base start and left upper start;
- explicit left-plus-right stereo: identical physical order;
- mono single layer: main then note trigger;
- two independent mono zones: first note trigger before second main.

The new adjacency/trigger assertions **must reject old P200** (two independent mono allocator voices). Preserve P199 PCM and every P192–P200 regression, Steinberg 47/47, 125A Tester and native installed VST3 QA. No private original ROM/VXD/SBK/SF2 in public CI.

## Completed native Windows x64 result — exact tested product commit

[Windows Actions #45043350615](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45043350615), **SUCCESS**, native Windows actual VST3 source and tests at commit `74d04059b48e1f45b9b7d676f195aee33dd68175`. The present documentation-only commit is not a new VST3 executable.

- Real hardware-register port trace from the pinned native Win95-compatible Synth / original 86Box EMU8000, **4/4 causal control cases PASS**: one linked stereo zone; two explicit reciprocal stereo zones; one mono zone; two independent mono zones.
- Stereo source MAIN completion before both trigger writes: first_main index **79**, last_main **152**, first_trigger **154**, last_trigger **156** (both stereo cases). Actual physical voice indices obey **left = base+1, right = base** and start **right(base) then left(base+1)**, per the **new asserted voice-ID checks**, not merely the trace printing these numbers.
- Mono single: main **76**, start **78**. Two independent mono voices: first main **76**, first start **78**, second main **157**, second start **159**. Thus a blanket "defer all multilayer starts" mutant fails the mono-specific oracle.
- Native trace markers `P200_ORIGINAL_CREATIVE_STEREO_JOINT_TRIGGER_NATIVE_PORT_ORDER_4_CASES_PASS`, `P201_ORIGINAL_CREATIVE_ADJACENT_STEREO_REVERSE_BASE_TRIGGER_4CASES_PASS` both emitted.
- The actual Windows VST3 synthetic SF2 P199 **3-rate/2-key PCM equality and contrasting right-sample/mono negative controls PASS**; P192–P198 focused audio regressions also passed.
- Steinberg validator **47 passed / 0 failed**; published 125A tester v0.2.10 and development tester v0.3.3 each **41 PASS / 0 WARNING / 0 FAIL**; actual EditorHost and P189 installer checks passed.
- Sounding N08: **1,956,105 accepted host saves**, **0 stale / 0 mixed / 0 rejected**; P179 **500/500** paced audible project saves passed.
- P200 raw port trace harness originally failed twice due **test oracle errors**, not independently demonstrated runtime faults: (1) genuine `R`-marked chip READ lines were misread as malformed writes; (2) the initial **zero** CPF prologue write was counted as an independently completed voice program. The P201 test fixes both and retains strict fail-closed parsing for any other unknown record. The P200 genuine-physical-pair defect remains independently proven by P200's TWO MONO allocator calls versus the original source's ADJACENT MODE1 allocator, but no separate **old-P200 product with new P201 test** red Windows job was executed. Do not mislabel its earlier harness FAIL as the product negative mutant.

## Release and fidelity boundary

**TARGETED P201 NATIVE WINDOWS AUTOMATED QA: PASS.** This independently confirms matching source-structured pair/control order on the rebuilt native instrument. It is **NOT** a fresh capture from the owner's running original Win95 Creative guest and does **NOT** prove all register values/timestamps or full hardware sonic fidelity.

The original 25-pair Win95 trace corpus was available in earlier owner research and inspected through its independently documented source conclusions, but its licensed bytes were NOT rerun here. Original VXD mutable device+0x18 state, per-voice pair-penalty bit+0x1a, concurrent voice stealing, original sample/ROM owner PCM and full pan/release/FX behavior remain **NOT VERIFIED**. Fender Studio Pro ASIO p95/p99 CPU deadlines, full modulators and independent Codex audit remain release blockers. **RELEASE NO-GO.**
