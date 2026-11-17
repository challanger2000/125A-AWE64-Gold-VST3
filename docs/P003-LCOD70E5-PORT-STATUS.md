# Source-based C++23 port: original LCOD+0x70E5 (17 Nov 2026)

## Original provenances and call edges

SHA-pinned owner copy of Win95 SBAWE.VXD v4.38.0.2:
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.
Original LE objects are object-relative, NOT disk-file offsets.

- LCOD+0x7076 installs the callback address LCOD+0x70E5 at original
  device+0xB52. Separate callback LCOD+0x7309 at device+0xB56.
  Verified from original LE internal fixups at LCOD+0x708E/+0x7098.
- LCOD+0x70E5..+0x7308 has the two original parameter paths:
  originalSelector high-byte bit 7 -> export active controls; otherwise
  mutate original channel/parameter state and update live voice registers.
- Three original PCOD+0x37BA call relocations at LCOD+0x7137,
  +0x720B and +0x7238.
- Source 30-entry/6-byte parameter table at LCOD+0x2FF0.
  LE fixups +0x7104 and +0x7156 identify its EXACT start/end
  (LCOD+0x2FF0..+0x30A4). Several per-field fixups confirm
  offsets +0, +1, +2 and +4.
- Live register reads: LCOD+0x72BF, +0x72CE call LCOD+0x3CE6.
  Live register writes: LCOD+0x72E7 calls LCOD+0x3CB1.
  An x86 selector OUT8 precedes every IN16 or OUT16.
- Original channel-control layout: device+0x442 + channel*0x24.
  Original 92-byte parameter row: device+0xF5E + channel*0x5C.
  Physical voice rows: device+0x42 + voice*0x20.
  This function can touch up to the first 0x1ADE bytes of original
  device state when the parameter selector index reaches 31.
  P005's separate original-bank capture device image length 0x1592
  is NOT a full replacement for this parameter callback's memory.

## Implementation

Header-only research function:
third_party/creative-vxd-port/include/OriginalCreativeLcod70e5.hpp

- Full high-bit 30-entry export path, including active flag == 1,
  16-bit source value arithmetic and externally authenticated PCOD
  lookup when an operation requires it.
- Live path: source-row selection checks, source state writes, special
  ID 4 signed shift/divide + direct OUT16, ID 5 controller offsets,
  original byte-preserving IN16/OUT16 for other enabled entries.
- Original voice matching (skip 0xFF markers), 16-bit hardware selector
  / routing, 32-bit x86 wrap, and signed high-value clamp only.
- Fixed-size stack temporaries; no allocation, file I/O, Windows API
  or locking in production function.
- Bounds and attestation-assertion checks fail closed. A bool by itself
  is NOT cryptographic authentication.

## Validation

1. Local GCC gnu++23 strict warning build: PASS.
2. Clang C++23 with ASan/UBSan: PASS.
3. Synthetic device/PortIo regression: PASS; checks byte retention,
   16-bit read-write order, distinct direct write, source writes,
   30-entry high-bit export, invalid selectors / truncated sources.
4. Private offline owner-table verification (never committed):
   30 original source records; 10 enabled (one direct 16-bit writer,
   nine read-modify-write), 20 disabled; simulated device state
   and original tables processed successfully, high-bit export PASS.
   This is NOT a Win95 live hardware trace comparison.

## Explicit remaining gates

- Real P005 runtime must own/authenticate the complete source device
  memory and both original tables before the callback is connected.
- Original callback LCOD+0x7309 and its common helper +0x7350 are
  NOT fully ported by this PR.
- No autonomous original x86 CPU-instruction scheduler or register TSC
  parity has been established.
- No live guest x86-versus-native hardware event replay or DRY PCM
  parity of the actual exported VST3 has been established.
- Existing Codex development PR #6 must not be overwritten.
- Keep PR #11 DRAFT/NO-MERGE. Private original binaries/ROM/SF2
  and table contents remain outside public source and CI artifacts.
