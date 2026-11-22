# P205: Source-derived original Creative Win95 allocator VTFT reset

The independently reconstructed original Win95 `OriginalCreativeMidiEngine.cpp`
in branch `codex/p005-vxd-full-port` contains `physical_reset`: after
the allocator's physical DCYSUSV=0x00FF stop, it writes
`VTFT=0x0000FFFF` via original EMU8000 DATA0 (E22 0x60|voice, 0x620).
This precedes *any* note-main registers. For a stereo pair, both
allocator stops precede both VTFT resets; independent mono allocations
each reset their own voice. These are distinct from the two
`StartVoice` VTFT resets in each subsequent original main body.

P205 adds this exact prologue only to P204's bounded Win95/SF2
worklist. Original 86Box EMU8000 C core, ROM/SBK, DOS, VST3
interface, GUI and saved-state format are untouched.

Acceptance: authentic original 86Box C chip logs in three valid
public synthetic SF2 setups (3 mono, mono+linked stereo, linked stereo
+mono). Require exactly three one-per-physical VTFT=FFFF writes
strictly after each allocator DCYSUSV stop and before first note-main
stop. Require nonzero generated original-chip PCM and existing P204
voice order. Separate test-only macro suppresses just these
allocation resets, and is rejected by the specific P205 causal
oracle. This does not assert real owner Win95 guest timing/PCM parity,
13-SF2 bank coverage, ASIO CPU acceptance or full release approval.
