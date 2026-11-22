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

**Before new Windows run:** NOT RUN. Original Creative live Win95/86Box 25-pair source corpus is independent, but was not rerun here. The owner's genuine media/physical hardware, complete allocator/voice steal/time, Studio Pro ASIO CPU deadlines, independent audit and distribution licensing remain **NOT VERIFIED — RELEASE NO-GO**.
