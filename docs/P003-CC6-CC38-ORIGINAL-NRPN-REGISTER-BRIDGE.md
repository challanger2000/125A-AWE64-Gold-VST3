# P003: Original CC6/CC38 -> device+B52 -> LCOD+0x70E5

Date: 2026-11-17. Branch: research/p003-pcod37ba-source-port-20261117.
**DRAFT / NOT COMPLETE / NO-MERGE / not product-enabled.**

## Original x86 source

Owner's independently SHA-pinned Win95 SBAWE.VXD v4.38.0.2:
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.

At LCOD+0x6AB1 (MIDI CC6) and LCOD+0x6BA5 (MIDI CC38),
the original driver routes mode 0x0100 to RPN, and mode 0x0200
to NRPN. Both NRPN branches require an installed non-NULL callback
in owner device+0xB52, pointing to original LCOD+0x70E5.
They update the ORIGINAL channel's 16-bit data-entry word
device+0x464+channel*0x24 BEFORE invoking the callback:

- CC6: next = ((value << 7) | (previous & 0x007F)) as uint16
  (original x86 LCOD+0x6B81..+0x6B9B).
- CC38: next = ((previous & 0xFF80) | value) as uint16
  (original x86 LCOD+0x6BF9..+0x6C0F).
- Callback argument: (owner_device, MIDI_channel, zero).
  It updates the per-channel parameter records and the matching
  physical EMU8000 voices with the source-proven register writer.
- Invalid parameter selector produces original EAX=1, but does not
  mean a MIDI I/O error; the CC handler has already written +0x464.
- Genuine partial hardware I/O failure cannot be undone. The
  C++23 engine latches PortError to prevent continued bad state.

## Actual live MIDI dispatch integration

Changed:
- third_party/creative-vxd-port/include/OriginalCreativeMidiEngine.hpp
- third_party/creative-vxd-port/src/OriginalCreativeMidiEngine.cpp
- third_party/creative-vxd-port/tests/test_original_creative_nrpn_cc6_cc38.cpp
- third_party/creative-vxd-port/CMakeLists.txt

Uses the already implemented source-derived originalLcod70e5(), NOT
a replacement filter or new SF2/EMU8000 algorithm.

Source data MUST be installed by the same private session:
- OriginalNrpnMemory contains owner device span, at least 0x151E bytes
  and valid original channel+voice state, managed off the RT thread.
- Authenticated owner attests original device+B52 callback installation.
- Parameter table is exactly 30 x 6 bytes, and PCOD lookup exactly
  128 uint16 values; both must derive from the same original driver.
- Physical voice row mapping must correspond to the SAME
  hardware device/PortIo attached to this MidiEngine instance.
- Physical voice capacity in owner device+6 equals the live
  DeviceState physical_capacity (0..32).
- Original device+0x45E must match mode 0x0200 and
  source+0x462 must match the selected NRPN parameter.
- Hardware PortIo provides actual out8/out16/in16 callbacks.

All these are fail-closed preconditions. Their C++23 boolean fields
are **caller assertions**, not cryptographic attestation. The complete
trusted owner/source initializer for the product is NOT implemented by
this PR. In particular the normal VST3 does not install these fields,
and therefore this cannot be claimed as a product feature yet.

## Tests

- Local GCC C++23 source suite: **28/28 PASS**.
- Local Clang+AddressSanitizer+UndefinedBehaviorSanitizer:
  **28/28 PASS**.
- A dedicated synthetic two-voice PortIo regression verifies:
  missing source and mismatched voice counts cause no writes;
  CC6 and CC38 preserve original bitfields; originalLcod70e5
  issues E22 IN16/OUT16 operations in order; source rows update;
  direct 16-bit write-only special parameter; invalid selector;
  partial hardware write failure latches the engine fault.
- Native MSVC Windows-2022 CI must be confirmed for this
  precise commit; the prior 36/36 green run validated an earlier
  source state only. No original guest runtime callback replay or
  DRY PCM bit parity is claimed by a synthetic CTest.

## Required completion before merge

1. Authenticated original owner-device memory, 30x6 and 128xWORD
   tables tied to exact SHA/session and same physical EMU8000 bridge.
2. Lifecycle and no-allocation/no-lock checks on the **real VST3**.
3. Real original Win95 CC98/99/6/38/121 live register event sequence
   versus native C++23 (including timing); then dry PCM comparison.
4. Verify all original bank/program/velocity states and the separate
   autonomous x86 instruction cycle scheduler.

Original Creative binaries, ROM, SF2, sample bytes, lookup tables,
and capture recordings remain private and have not been committed.
Do not merge research PR11 into the release or Codex PR6 yet.
