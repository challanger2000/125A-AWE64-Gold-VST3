# P003: Original NRPN source owner in the real P005 VST3 host adapter

17 November 2026. Research PR #11. NOT COMPLETE. NO-MERGE.

## Implemented
- Original Creative CC98/99, CC6/38 and CC121 callbacks: source-derived
  C++23 functions, routed through the existing single MidiEngine.
- Before any CC6/38 hardware writes: compare all 32-byte original VXD
  physical voice records against native DeviceState voice ownership.
  The original parameter opcode 4 additionally demands a matching
  channel bend delta and source-verified pitch bases.
- Stale historical guest snapshots are rejected BEFORE chip I/O or
  channel data-entry mutations; no guessed voice remapping.
- OriginalCreativeNrpnOwnedSource.hpp retains the original-source
  device image (0x1592 bytes), parameter descriptors (30x6 bytes),
  and PCOD lookup (128 16-bit words) in per-instance fixed-size storage.
- The 16 original channel NRPN/RPN selection words and bend-delta
  words are imported directly from the attested owner device image.
- P005OriginalCreativeHostAdapter.hpp now offers a NON-RT source
  binding entry point and detaches all owner views before destroying
  the shared original MIDI/chip runtime.
- All private inputs are copied; their caller buffers may be freed
  after non-RT import. No Creative proprietary table bytes, driver
  binary, ROM or SoundFont have been checked into GitHub.

## Verification
GitHub Actions run 44067696747: Windows Server 2022, MSVC, C++23,
**39/39 CTests PASS**.

New test #38: original_creative_nrpn_owner_lifetime_no_heap.
New test #39: original_creative_nrpn_p005_host_lifecycle.
Updated test #37: source/native voice drift, pitch mismatch and
channel bend mismatch rejected before register writes.

These tests use synthetic source fixtures. Their success does NOT
prove a complete Creative driver or Windows95-versus-VST3 audio parity.

## Source provenance
SHA-pinned private SBAWE.VXD:
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a

The 0x1592 guest capture footer 0x4D474D31 is a format marker.
Boolean provenance flags are external assertions, not SHA proof.
The private loader still must verify the exact driver SHA, source
table provenance and common device/chip/guest session.

## Blocking issues
1. Trusted end-to-end private original source importer for all three
   data buffers, the full 288-slot bank directory and chip identity.
2. Native NoteOn/Off mutates DeviceState voices. A historical guest
   byte-image does not automatically follow those mutations. The
   new guard safely rejects drift; original x86-proven voice-row
   synchronization is needed for unrestricted NRPN after NoteOn.
3. Original instruction-cycle scheduler, register/TSC comparison,
   full SF2 banks, real VST3/ASIO lifecycle and original DRY PCM
   bit-parity remain open.

P001Minimal.cpp is unchanged and does not activate this path by default.
Codex PR #6 remains untouched. Keep research PR #11 DRAFT/NO-MERGE.
