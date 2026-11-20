# P007BW — original Creative 1MGM drum Bank128 vs Creative user bank ownership (20 Nov 2026)

## Original owner-private runtime P007BV: **NO-GO**
- Genuine original Win95 Creative AWEMAN32 acknowledged user bank 2: `p007_midi_guest_gm_rom_bank_status() == 1`. This confirms **API bank ownership**, not that the **percussion preset** has been mapped or is audible.
- Genuine EMU8000 GM melodic ROM program7 note60 played (RMS 0.004068636).
- Original **GM Standard percussion bank** before *any* SF2 load: note36 RMS 0.000004290, note38 RMS 0.000001114, failed audio; original register count 76 for each. No private synth substitute.
- After authentic three SF2 user-bank1 hot loads, original drum bank2 note36 RMS 0.000000000 and note38 RMS 0.000001333, both failed audio; real original SF2 TR808 user-bank1 note36 RMS 0.005406318, passed.
- A/B/A second SF2 identity and returned first SF2 spectral shape both passed. The **ROM drum problem is independent of SF2 loading**.

## Original soundbank structural evidence (read-only, private)
- The owner's original `SYNTHGM.SBK` 34832 bytes, SHA256 `fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586`.
- Authentic Creative RIFF/sfbk/1MGM: **129 ordinary program/preset entries + terminator**, including 128 melodic presets at `bank=0` and one **`Standard` drum preset at `bank=128, program=0`**. The drum preset references the original instrument `standard_DRUMS`, ibag start 674.
- The actual original percussion sound generators resolve note36 to instrument bag686 with ROM sample-selector **92** and note38 to instrument bag688 with ROM selector **144**. These 1MGM generator53 values are **unresolved ROM selector IDs**, NOT indices into a generic SF2 `shdr` table; format1 sample headers are **48 bytes each**. This is documented separately in `docs/P002-SYNTHGM-ROM-BANK-ZONE-EVIDENCE.md`.
- The **source file really contains the percussion preset and appropriate zones**. Missing `SYNTHGM.SBK` is disproved, and changing unrelated ROM melodic settings is not a fix.
- P007BV used original Creative `AWE_LOAD_USER_BANK=204` on user bank2 and `AWE_GET_UBANK_PARENT=310` to prove bank2 presence, then selected MIDI `CC0=2, CC32=0, PC0` on Creative channel10. **That handshake does NOT prove the `phdr bank128` percussion preset is accessible at bank2/program0.** Treat that assumption as unverified, do NOT make another blind bank assignment. The original 1996 `AWE_API.H` enumerates `AWE_GET_DRUM_KIT=311` and `AWE_LOAD_USER_INSTR=206`, but any use of these must adhere to their actual ABI and be proven on the real guest, not invented.
- Creative's contemporary manual says GM-mode MIDI channel10 automatically uses the Standard kit. Our DAW must keep 16 MIDI channels addressable; browser audition on default channel1 can be internally redirected as convenience without requiring user channel selection.

## Next evidence collection, no emulation alteration
P007BW adds a passive, bounded dump of actual original EMU8000 voice/sample address registers (CCCA, PSST, CSL) and a reproducible port/value fingerprint for each real Original Guest MIDI note. Compare silent GM note36/38 **before any SF2**, audible GM melodic note60, audible SF2 TR808 note36, and GM notes after A/B/A:
1. Missing or zero voice address writes under GM percussion: original driver kit mapping/selection failed.
2. Voice address writes present but invalid/empty ROM sample region: original EMU8000 ROM-selector-to-chip-address path mismatch, distinct from MIDI.
3. Plausible matching addresses yet zero PCM: inspect original register envelope/attenuation and actual chip audio path, not bank selection.
Only a PRIVATE guest execution with verified input provenance can distinguish these cases. Public Windows CI builds and validator checks **cannot** prove original audio.

### Primary references
- Original Creative developer header: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/AWE_API.H
- Original Creative API docs: https://www.manualslib.com/manual/2031598/Creative-Awe32.html?page=53
- Historical Creative FAQ: https://archive.gamedev.net.storage.googleapis.com/archive/reference/articles/article445.html

**Status: engineering research only, no release, original chip preserved.**
