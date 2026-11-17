# P003: Original Win95 NRPN-Callbacks LCOD+0x7309/0x7350/0x740A

Source: locally private SHA-pinned SBAWE.VXD 4.38.0.2
SHA-256: d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a

## Original x86 source proof

- LCOD+0x7076 installer uses LE relocations +0x7098 and +0x70A2
  to install handlers +0x7309 (device+0xB56) and +0x740A
  (device+0xB5A). LE +0x708E installs +0x70E5 at device+0xB52.
- Original CC dispatcher LCOD+0x6682 takes 93..123 through a byte
  table at LCOD+0x686B (LE fixup at +0x66F0) and a 9-address
  relocatable jump table at +0x6847 (fixup at +0x66F7).
- Table item index 5 (CC98, 0x62) = 1, jump table item 1
  (LE fixup LCOD+0x684B) = +0x67B9, which calls device+0xB56.
  Table index 6 (CC99, 0x63) = 2, jump item 2
  (LE fixup +0x684F) = +0x67DB, calls device+0xB5A.
- +0x7309: byte-range NRPN LSB, mode 0x0200 at channel+0x45E,
  selector at +0x462 = (old & 0x7F00) | (value & 0xFF).
- +0x740A: byte-range NRPN MSB, mode 0x0200,
  selector at +0x462 = (old & 0x00FF) | ((value & 0xFF)<<8).
- Both callbacks call the shared original +0x7350 helper.
  High byte 0 resets 30 per-channel parameter flags and WORD values
  to 0x2000, clears parameter-row flag and channel+0x450, RETURNS 1.
  High byte 0x7F and low 7 bits <30 selects the stored parameter value
  only when row enabled and selected flag == 1; otherwise 0x2000.
  It writes channel+0x464 and RETURNS 0.
  All other high-byte/parameter states RETURN 1.
- Each channel uses stride 0x24 for channel state and 0x5C for the
  30-parameter state. Sixteen physical MIDI channels are supported.
- Minimum complete owner device memory span for channel 15, flag 29,
  is 0x151E bytes. An initial local 0x1500 bound was unsafe:
  the GCC build exposed an out-of-range test-write warning. Fixed to
  0x151E and verified with Clang ASan/UBSan before GitHub commit.
- These original EAX results are not ordinary success booleans:
  reset writes may occur even when the callback returns 1.

## Port and native regression

- Source-derived, heap-free C++23 functions are in
  third_party/creative-vxd-port/include/OriginalCreativeLcod73xxNrpn.hpp.
- Test is tests/test_original_creative_lcod73xx_nrpn.cpp.
  All assertions remain active even in Release (-DNDEBUG).
- Source and state scenarios: enabled/disabled parameter, invalid
  parameter 30, reset of all 30 states, FF->7F masking behavior,
  all 16 channel ranges, invalid device span, denied attestation.
- Local snapshot of the older standalone C++23 source suite:
  26/26 CTests PASS (this is NOT the newer PR6 Windows/MSVC suite).
- The new test also passed local Clang C++23 -Wall/-Wextra/-Wconversion
  -Werror/-pedantic plus AddressSanitizer/UndefinedBehaviorSanitizer.
- Mutation gate: forcing the source mode 0x0200 to 0x0100 was
  detected by the Release regression (nonzero abort). This proves
  these assertions are not compiled away.

## Limits and privacy

- No original proprietary bytes/tables, ROM, SoundFonts, or guest
  recordings are checked into this public research branch.
- Boolean owner assertion is only a caller policy gate, not a hash
  or Win95-session provenance guarantee.
- These callbacks currently require a complete private owner device
  image. The product's abstract MidiChannel fields do not yet
  expose an authenticated equivalent of every original row.
- Productive original NRPN dispatch, guest x86-versus-native callback
  execution and DRY PCM equality are NOT established.
- No second DSP, approximated EMU8000 or ad-hoc state scheduler added.
- Keep PR11 draft / NO-MERGE; do not disturb Codex' PR6.
