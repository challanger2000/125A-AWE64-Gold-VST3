# P002 RAM capture research continuation — 2026-11-13

Scope: independent research for PR #4, based on full commit
`9cc1322d8d9813e0bed05dec2f2d9c6651209236`. Development branch unchanged.
No vendor binaries, ROM, bank or VM contents are included.

Final concurrent-development check: remote developer HEAD advanced to
`ed588cbe4b99876a0d1822d9634673c28fdffab1`. Its five additional commits add
read-only FAT16 guest log/SF2 extraction, streaming clone hashing, regression
coverage and CI registration. A direct diff confirms that neither
`p002_original_ram_trace_probe.py` nor `CreativeOriginalRamPortReplay.cpp`
changed, so the findings below remain applicable. Those new extraction tests
were not run on this older, deliberately isolated research base.

## Reconstructed state and evidence boundary

Read PR #4's public conversation, the existing P001/P002 evidence reports,
Win95 capture protocol, guest source, Python RAM trace/log auditors, native
RAM source/replay code and ROM sample-address routines. GitHub GraphQL returned
Forbidden; the public PR HTML was successfully read instead. CI results in that
conversation are previous developer reports, not newly executed CI here.

Earlier independent x86 work is already incorporated: format-1 vs format-2
generator conversion, attack/rate helpers, descriptor templates, ROM geometry,
voice allocation and termination. This report does not repeat their derivation
or claim to have rerun those proprietary instruction streams.

Current guest calls original AWEMAN32 AWEManager: OPEN=101, LOAD_USER_BANK=204,
QUERY_DRAM_SIZE=308, CLEAR_USER_BANK=205, CLOSE=102. LOAD uses bank1 and the
mutable 128-byte filename CBufferObject with OPER_FILE=0. WinMM C4 and direct
AWE_SEND_MIDI=401 D4 follow in separate phases, requesting CC91/93=0.
This is an acquisition probe, not an original allocator implementation.
Guest return codes, FNV identity and DRAM deltas cannot establish PCM parity.

No files were found in the supplied `/workspace/library-files`,
`/workspace/scratch` or `/workspace/shared` data directories. No private
SBAWE.VXD, SBAWE32.DRV, AWEMAN32.DLL, guest disk, real RAM trace or PCM is
available in those locations. Consequently no new binary disassembly or live
Win95 observation is claimed. Prior binary offsets remain prior evidence.

## New reproducible capture-auditor findings

Run from repository root (Python standard library only):

```sh
PYTHONDONTWRITEBYTECODE=1 python research/p002-ram-audit/reproduce.py
PYTHONDONTWRITEBYTECODE=1 python -m unittest \
  tests.test_p002_original_ram_trace_probe \
  tests.test_p002_win95_ram_guest_log \
  tests.test_p002_win95_ram_stage -q
```

The second command executed **20 tests, all passed**. The first reproduces
three additional counterexamples against the unmodified implementation;
its exit zero means the observations reproduced, not that the auditor is correct.

| Case | Observed Python scan | Independent requirement / consequence |
|---|---|---|
| Twenty synthetic words at 0x200400 | One exact match at 0x200400 | Positive baseline |
| Same upload followed by SMALW reset and `DEAD` at 0x200400 | Still one exact match at 0x200400 | Historical occurrence remains true, but final RAM no longer matches; cannot promote this result to playback-state evidence |
| Complete sample followed by SMALW select and low half only | Still one match | Literal-port exporter rejects `trailing partial address`; scan and export disagree about completeness |
| PCM `11` repeated 32 bytes; upload `00` + 34 times `11` + `00` | Zero matches | Independent even-offset search finds one full word-aligned match at byte offset2, hardware word 0x200401 |

Causes, bounded to actual source:

1. `scan_capture.evaluate()` counts matches in completed contiguous write runs;
   it never invalidates them when later runs overwrite the same RAM. This is
   valid as historical occurrence detection, insufficient as final-state proof.
   In contrast, native `candidateReplaySourceMatchedOriginalRamSf2Sample()`
   requires addresses in the selected interval to be visited exactly once in
   increasing order and rejects later overlap. The native path therefore has
   an additional safety gate; this is not evidence of a corrupt accepted VST3 bank.
2. Scanner has no EOF partial-SMALW check. The exporter explicitly has one;
   C++ preflight also rejects a final incomplete address. `analyze()` calls the
   scanner, so its report alone must not replace the stricter replay preflight.
3. `find(expected_pcm, at+2)` skips the next offset even when the previous hit
   was **odd** and rejected. On the repeated-byte example, hits1 and3 are examined
   while the valid aligned hit2 is skipped. This is a false negative, not proof
   that the original driver transformed or failed to upload a waveform.

Developer handoff: preserve historical occurrence and final-state claims as
separate outcomes; check complete pointer transactions consistently; search
word-aligned candidates or advance one byte after an unaligned hit. Keep
overlap/ambiguity rejection for allocation evidence. No production code was
changed here, and no new allocator is proposed.

## RAM geometry and sample-end implications

Current adapter specifies 28672 KiB = 29,360,128 bytes = 14,680,064 words.
Absolute installed word interval is `[0x200000,0x1000000)`.
The sample-upload policy reserves `[0xFFFFE0,0x1000000)` for FM:
32 words / 64 bytes. Thus the permitted direct-upload interval is
`[0x200000,0xFFFFE0)`, 14,680,032 words / 29,360,064 bytes.
This distinction does not reduce the required physical 28 MiB allocation.

`smpl` offsets and shdr start/end/loop positions are word indices, not byte
addresses. Source data extraction `[start,end)` alone does not prove placement
of guard words, memory-manager headers, alignment or free-list overhead.
A match at hardware word A for source start S implies a **candidate** relocation
delta A-S for that sample; it does not prove whole-bank relocation or allocator
contiguity. The current probe handles mono type1 only, rejects sm24, and does
not establish stereo/linked sample placement.

Previously evidenced ROM note geometry in `CreativeVxdSampleAddress.cpp` uses
CCCA start-4, loop PSST/CSL loopStart-1/loopEnd-1, or nonloop end+4/end+8.
The implementation deliberately restricts those functions to ROM <=0x80000.
It would be unjustified to remove that restriction and assume RAM uses the same
geometry. If future original RAM traces demonstrate those same sentinel
addresses, allocator accounting must accommodate accesses outside the matched
`[start,end)` payload and prove they remain below the FM-reserved boundary.
Neither SF2 file size nor sample byte count gives that proof.

Native `candidateReplayOriginalCreativeCapturedRamUpload()` preserves event
order but loops over writes without advancing rendering to each stored chip
frame. It can validate upload/readback, not source timing or PCM equivalence.
The complete original trace and separate frame-aware replay remain necessary.

Pinned-fork source cross-check completed after initializing the existing
submodule: commit `fee1e81977824fd2d5eed4d239589dc30d251dd7`, chip Git blob
`e60eb10f00a8e1187380faa25989f6ba0433d99a`. `snd_emu8k.c` lines924–939
mask writes to 24-bit addresses and reject ROM/FM regions; lines1513–1523
implement SMALW/SMLD and increment the left write pointer modulo 24 bits.
Lines2961–2970 clamp allocation to 0x7000 KiB and calculate word end with
`onboard_ram << 9`. Those are chip facts, not proof of Creative allocation.
The separate right writer SMARW is also present in the fork; the current
left-only trace probe is not a complete decoder of all possible RAM traffic.

## Current-instance build validation

CMake was absent; system installer could not run because sudo is unavailable.
Installed CMake3.31.6 and Ninja1.11.1.4 in the external Python virtual environment
`/workspace/research-tools`, using normal verified HTTPS package installation.
GCC14.2 and Python3.12 were already available. With four parallel jobs:

```sh
export PATH=/workspace/research-tools/bin:$PATH
cmake -S /workspace/125A-AWE64-Gold-VST3 \
  -B /workspace/build-p002-research -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAWE64_BUILD_P001_VST3=OFF
cmake --build /workspace/build-p002-research --parallel 4
PYTHONDONTWRITEBYTECODE=1 ctest --test-dir /workspace/build-p002-research \
  --output-on-failure --parallel 4
```

All 127 build steps completed; **29/29 CTests passed**, zero failures.
This is a Linux native research build, not Windows VST3 validation or original
driver execution. Even tests named `original` do not supply missing proprietary
guest execution; their existing evidence scopes still apply. Windows-only
VST3/Win95 executable builds were not run here. The 20 Python tests and new
counterexamples described above were executed separately.

## Precise next original-driver experiments (blocked only by private inputs)

Retain the existing P002RAM guest and owner-controlled clone procedure. A real
capture must bind driver hashes/versions (all three files), fork commit/core
blob, ROM hash, 28672-KiB machine configuration, exact SF2 hash, guest log,
full trace and original digital PCM. Preserve boot/preload/clear traffic to
distinguish RAM detection, FM initialization, sample writes and free operations.
Do not treat a whole-boot scanner rejection as evidence against Creative load.

To infer alignment and allocation, use distinguishable synthetic waveforms with
odd/even word lengths, nonzero source starts and explicit leading/trailing
sentinels. Compare sizes immediately around candidate boundaries. Observe
multiple samples, overlapping source intervals, clear/reload and fragmented
multi-bank loads. For each record capture SMALW placement, exact SMLD writes,
pre/post free words and note CCCA/PSST/CSL. These distinguish whole-smpl upload,
per-sample packing, alignment, guards, reuse and management overhead; no such
policy is yet demonstrated here. All-zero padding cannot uniquely identify an
allocation by content.

The WinMM and direct phases use different notes and the direct phase sends no
independent bank-select message. Differences between those phases are therefore
not automatically API-route bugs: pitch, state retention and bank selection
must first be controlled or observed. Guest millisecond markers also are not
an established mapping to the fork's 44.1-kHz frame clock.

Finally compare full-register order/values/time, then digital chip PCM, and
separately the original fork audio/mixer output. This audit did not validate the
whole original mixer path or hardware line-out. Do not change the calibrated
EMU8000, introduce DSP compensation, normalize gain, or infer sound fidelity
from these synthetic capture tests. Real Win95 RAM loading, memory-management
policy and acoustic parity remain open; PR #4 should remain draft.
