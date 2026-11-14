"""P002 original Creative Win95 RAM bank guest API audit – synthetic log only.

No original Win95 driver, private SF2, ROM, DLL, VXD or guest evidence
is included. The positive case proves *parser/gate correctness only*.
"""
from __future__ import annotations
import unittest

from tools import p002_verify_original_win95_ram_guest_log as auditor

def sf2_bank():
    data = bytes([i % 251 for i in range(512)])
    return b"RIFF" + (len(data) + 4).to_bytes(4, "little") + b"sfbk" + data

def synth_guest_log(bank_size: int, bank_bytes: bytes | None = None) -> bytes:
    source=sf2_bank() if bank_bytes is None else bank_bytes
    lines = [
        "P002_WIN95_ORIGINAL_CREATIVE_RAM_BANK_CAPTURE_START",
        "PRIVATE_WIN95_SF2_SIZE=" + hex32(bank_size),
        "PRIVATE_WIN95_SF2_FNV1A32=" + hex32(auditor.fnv1a32(source)),
        "ORIGINAL_AWEMAN32_CBUFFER_PACK2_SIZE_BYTES=0x0000000E",
        "LOADED_CREATIVE_MODULE_PATH=C:\\WINDOWS\\SYSTEM\\AWEMAN32.DLL",
        "ORIGINAL_AWEMAN32_AWE_OPEN_RC=0x00000000",
        "WIN95_ORIGINAL_MIDI_ENDPOINTS=0x00000002",
        "WIN95_MIDI_DEVICE=0x00000000 NAME=OPL3 FM Synth",
        "WIN95_MIDI_DEVICE=0x00000001 NAME=SB AWE32 MIDI Synth",
        "CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT=0x00000001",
        "ORIGINAL_CREATIVE_MIDIOUT_OPEN_RESULT=0x00000000",
        "PRE_LOAD_SYNTH_BANK_RESULT=0x00000000",
        "PRE_LOAD_SYNTH_BANK_MODE_ID=0x00000000",
        "PRE_LOAD_RESULT=0x00000000",
        "PRE_LOAD_TOTAL_16BIT_WORDS=0x00E00000",
        "PRE_LOAD_FREE_16BIT_WORDS=0x00D00000",
        "ORIGINAL_API_LOAD_BEGIN_TICK_MS=0x00000200",
        "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_BEGIN",
        "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000",
        "ORIGINAL_API_LOAD_END_TICK_MS=0x00000300",
        "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END",
        "POST_LOAD_SYNTH_BANK_RESULT=0x00000000",
        "POST_LOAD_SYNTH_BANK_MODE_ID=0x00000000",
        "POST_LOAD_RESULT=0x00000000",
        "POST_LOAD_TOTAL_16BIT_WORDS=0x00E00000",
        "POST_LOAD_FREE_16BIT_WORDS=0x00CF0000",
        "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x00010000",
        "GUEST_DRAM_ALLOCATION_API_ALONE_NOT_HARDWARE_UPLOAD_PROOF",
        "ORIGINAL_USER_BANK_CC0_VALUE1_ORIGINAL_FAQ_F6",
        "ORIGINAL_RESET_CONTROLLERS_RC=0x00000000",
        "ORIGINAL_BANK_SELECT_MSB1_RC=0x00000000",
        "ORIGINAL_PROGRAM0_RC=0x00000000",
        "ORIGINAL_CC91_REVERB_SEND_ZERO_RC=0x00000000",
        "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC=0x00000000",
        "ORIGINAL_WINMM_C4_ON_SUBMIT_TICK_MS=0x00000500",
        "ORIGINAL_MIDI_C4_VEL100_RC=0x00000000",
        "ORIGINAL_WINMM_C4_OFF_SUBMIT_TICK_MS=0x00000700",
        "ORIGINAL_MIDI_C4_OFF_RC=0x00000000",
        "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_BEGIN",
        "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_PROGRAM0_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_CC91_ZERO_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_CC93_ZERO_RC=0x00000000",
        "ORIGINAL_DIRECT_D4_ON_SUBMIT_TICK_MS=0x00000A00",
        "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC=0x00000000",
        "ORIGINAL_DIRECT_D4_OFF_SUBMIT_TICK_MS=0x00000C00",
        "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC=0x00000000",
        "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END",
        "ORIGINAL_AWEMAN32_CLEAR_USER_BANK_RC=0x00000000",
        "ORIGINAL_AWEMAN32_AWE_CLOSE_RC=0x00000000",
        "P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_API_SENT_OK",
        "GUEST_CAPTURE_EMU8000_RAM_REGISTER_AND_PCM_EVIDENCE_NOT_ASSERTED_BY_API",
    ]
    return ("\r\n".join(lines) + "\r\n").encode("ascii")

def hex32(n: int):
    return f"0x{n:08X}"

class OriginalWin95RAMGuestLogTests(unittest.TestCase):
    def setUp(self):
        self.bank = sf2_bank()
        self.log = synth_guest_log(len(self.bank))

    def mutate(self, old, new):
        body = self.log.decode("ascii")
        assert body.count(old) == 1, old
        return body.replace(old, new).encode("ascii")

    def test_original_creative_pack2_cbuffer_abi_required(self):
        # Original Creative INDICES.H sets #pragma pack(2) for Win32.
        # Our previous default-packed 16-byte guest struct was an
        # error in OUR own test harness. No historical Creative ABI
        # fact is inferred from the old guest program.
        good=auditor.audit(self.log,self.bank)
        self.assertEqual(
            good["original_creative_cbuffer_pack2_size_bytes"],14)
        for wrong in (0,12,16,20):
            with self.subTest(wrong=wrong):
                with self.assertRaisesRegex(ValueError,
                                            "CBufferObject ABI"):
                    auditor.audit(self.mutate(
                        "ORIGINAL_AWEMAN32_CBUFFER_PACK2_SIZE_BYTES=0x0000000E",
                        f"ORIGINAL_AWEMAN32_CBUFFER_PACK2_SIZE_BYTES=0x{wrong:08X}"),
                        self.bank)
        with self.assertRaisesRegex(ValueError,"Missing original Creative result"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_CBUFFER_PACK2_SIZE_BYTES=0x0000000E\r\n",
                ""),self.bank)

    def test_original_creative_synth_mode_queries_before_after_load(self):
        report=auditor.audit(self.log,self.bank)
        self.assertEqual(report["original_creative_synth_mode_before_load_id"],0)
        self.assertEqual(report["original_creative_synth_mode_after_load_id"],0)
        self.assertFalse(report["original_synth_mode_change_observed"])
        self.assertEqual(report["original_midi_CC0_user_bank_mapping"],
                         "NOT_PROVEN_BY_SYNTH_MODE_QUERY")
        # Creative INDICES.H enumerates exactly GM, GS, MT32 and USER
        # CUSTOM (=0..3). Observing a mode is NOT authentic MIDI CC0.
        for mode in range(4):
            changed=self.mutate(
                "POST_LOAD_SYNTH_BANK_MODE_ID=0x00000000",
                f"POST_LOAD_SYNTH_BANK_MODE_ID=0x{mode:08X}")
            result=auditor.audit(changed,self.bank)
            self.assertEqual(result["original_creative_synth_mode_after_load_id"],mode)
            self.assertEqual(result["original_synth_mode_change_observed"],
                             mode!=0)
        for invalid in (4,128,65535):
            with self.subTest(invalid=invalid):
                with self.assertRaisesRegex(ValueError,"synth bank enum"):
                    auditor.audit(self.mutate(
                        "POST_LOAD_SYNTH_BANK_MODE_ID=0x00000000",
                        f"POST_LOAD_SYNTH_BANK_MODE_ID=0x{invalid:08X}"),
                        self.bank)
        with self.assertRaisesRegex(ValueError,"Unexpected original Creative synth bank query result"):
            auditor.audit(self.mutate(
                "POST_LOAD_SYNTH_BANK_RESULT=0x00000000",
                "POST_LOAD_SYNTH_BANK_RESULT=0x00000001"),self.bank)
        with self.assertRaisesRegex(ValueError,"before/after load order"):
            auditor.audit(self.mutate(
                "POST_LOAD_SYNTH_BANK_RESULT=0x00000000\r\n"
                "POST_LOAD_SYNTH_BANK_MODE_ID=0x00000000",
                "POST_LOAD_SYNTH_BANK_MODE_ID=0x00000000\r\n"
                "POST_LOAD_SYNTH_BANK_RESULT=0x00000000"),self.bank)

    def test_real_original_win95_0xcb_optional_bank_query_with_successful_load(self):
        # Derived from a REAL Win95 AWEMAN32 run: Creative returned
        # 0xCB for optional GET_SYN_BANK=305 both before and after a
        # successful LOAD_USER_BANK=204. Our auditor previously marked
        # the source as failed despite the actual bank being loaded.
        # Synthetic guest log only; no original user data in CI.
        original=self.log
        text=original.decode("ascii")
        for prefix in ("PRE_LOAD_SYNTH_BANK","POST_LOAD_SYNTH_BANK"):
            text=text.replace(
                prefix+"_RESULT=0x00000000",
                prefix+"_RESULT=0x000000CB")
            text=text.replace(
                prefix+"_MODE_ID=0x00000000",
                prefix+"_MODE_ID=0xFFFFFFFF")
        unknown=text.encode("ascii")
        evidence=auditor.audit(unknown,self.bank)
        self.assertEqual(evidence["status"],
                         "ORIGINAL_CREATIVE_WIN95_API_GUEST_LOG_CONTRACT_PASS")
        self.assertIsNone(evidence["original_creative_synth_mode_before_load_id"])
        self.assertIsNone(evidence["original_creative_synth_mode_after_load_id"])
        self.assertIsNone(evidence["original_synth_mode_change_observed"])
        self.assertEqual(evidence["original_creative_synth_mode_query_results"],
                         {"before":0xCB,"after":0xCB})
        self.assertEqual(evidence["original_midi_CC0_user_bank_mapping"],
                         "NOT_PROVEN_BY_SYNTH_MODE_QUERY")
        self.assertFalse(evidence["release_approved"])
        # Unsupported response codes and fake mode evidence remain NO-GO.
        for old,new in (
            ("PRE_LOAD_SYNTH_BANK_RESULT=0x000000CB",
             "PRE_LOAD_SYNTH_BANK_RESULT=0x000000CA"),
            ("PRE_LOAD_SYNTH_BANK_MODE_ID=0xFFFFFFFF",
             "PRE_LOAD_SYNTH_BANK_MODE_ID=0x00000000"),
            ("POST_LOAD_SYNTH_BANK_RESULT=0x000000CB",
             "POST_LOAD_SYNTH_BANK_RESULT=0x00000001"),
        ):
            with self.subTest(old=old):
                with self.assertRaisesRegex(ValueError,"synth bank|synth mode sentinel"):
                    auditor.audit(unknown.replace(old.encode(),new.encode()),self.bank)
        # Claimed successful query with unknown enum is also rejected.
        forged=unknown.replace(b"POST_LOAD_SYNTH_BANK_RESULT=0x000000CB",
                               b"POST_LOAD_SYNTH_BANK_RESULT=0x00000000")
        with self.assertRaisesRegex(ValueError,"synth bank enum"):
            auditor.audit(forged,self.bank)

    def test_guest_c4_d4_submission_timestamps_and_negative_events(self):
        report=auditor.audit(self.log,self.bank)
        self.assertEqual(report["original_guest_winmm_C4_submission_ticks_ms"],
                         {"note_on":0x500,"note_off":0x700})
        self.assertEqual(report["original_guest_direct_D4_submission_ticks_ms"],
                         {"note_on":0xA00,"note_off":0xC00})
        self.assertEqual(report["guest_GetTickCount_to_86Box_EMU8000_frame_time"],
                         "NOT_PROVEN_INDEPENDENT_CLOCK_DOMAINS")
        before={
            "ORIGINAL_WINMM_C4_ON_SUBMIT_TICK_MS":0x500,
            "ORIGINAL_WINMM_C4_OFF_SUBMIT_TICK_MS":0x700,
            "ORIGINAL_DIRECT_D4_ON_SUBMIT_TICK_MS":0xA00,
            "ORIGINAL_DIRECT_D4_OFF_SUBMIT_TICK_MS":0xC00,
        }
        for key,original in before.items():
            line=f"{key}=0x{original:08X}\r\n"
            with self.subTest(missing=key):
                with self.assertRaisesRegex(ValueError,
                                            "Missing original Creative result"):
                    auditor.audit(self.mutate(line,""),self.bank)
        # The actual guest sleeps 400ms between NoteOn and NoteOff,
        # and 500+150ms between the two routes. Never pass a forged
        # short note or a MIDI event predating SF2 upload.
        for key,bad in (
            ("ORIGINAL_WINMM_C4_OFF_SUBMIT_TICK_MS",0x510),
            ("ORIGINAL_DIRECT_D4_ON_SUBMIT_TICK_MS",0x710),
            ("ORIGINAL_DIRECT_D4_OFF_SUBMIT_TICK_MS",0xA01),
            ("ORIGINAL_WINMM_C4_ON_SUBMIT_TICK_MS",0x10),
        ):
            with self.subTest(invalid=key):
                with self.assertRaisesRegex(ValueError,"submission timestamps"):
                    auditor.audit(self.mutate(
                        f"{key}=0x{before[key]:08X}",
                        f"{key}=0x{bad:08X}"),self.bank)
        on_tick="ORIGINAL_DIRECT_D4_ON_SUBMIT_TICK_MS=0x00000A00"
        on_result="ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC=0x00000000"
        with self.assertRaisesRegex(ValueError,"log position/order invalid"):
            auditor.audit(self.mutate(on_tick+"\r\n"+on_result,
                                      on_result+"\r\n"+on_tick),self.bank)

    def test_original_creative_faq_correct_cc0_midi_byte_order_required(self):
        # REAL Win95 ROM-only C4/D4 cause was OUR BAD MIDI packet:
        #   0x000001B0 -> CC1=0 (modulation wheel), NOT bank change
        #   0x000100B0 -> CC0=1 (User Bank 1), original Creative FAQ F.6.
        result=auditor.audit(self.log,self.bank)
        self.assertEqual(result["original_user_bank_midi_cc0_controller"],0)
        self.assertEqual(result["original_user_bank_midi_cc0_value"],1)
        self.assertEqual(
            result["C4_D4_actually_use_RAM_samples"],
            "NOT_PROVEN_REQUIRES_INDEPENDENT_CCCA_PSST_CSL_ADDRESS_CAPTURE")
        # Compiler-only source protocol check: guard against silently
        # reintroducing the same actual developer error in either route.
        from pathlib import Path
        source = (Path(__file__).resolve().parents[1]/
                  "src/win95/p002_original_creative_ram_bank_capture.c")
        program = source.read_text(encoding="utf-8")
        self.assertEqual(program.count("ORIGINAL_MIDI_CC(0u,0u,1u)"),3)
        self.assertIn("#define ORIGINAL_MIDI_CC(",program)
        self.assertIn("ORIGINAL_MIDI_CC(0u,0u,1u)==0x000100B0u",program)
        self.assertIn("ORIGINAL_MIDI_CC(0u,1u,0u)==0x000001B0u",program)
        self.assertNotIn("0x000001B0u,",program)
        self.assertNotIn("ORIGINAL_AWE_SELECT_SYN_BANK 203u",program)
        marker="ORIGINAL_USER_BANK_CC0_VALUE1_ORIGINAL_FAQ_F6"
        with self.assertRaisesRegex(ValueError,"Missing original Creative FAQ"):
            auditor.audit(self.mutate(marker+"\r\n",""),self.bank)
        with self.assertRaisesRegex(ValueError,"marker order invalid"):
            auditor.audit(self.mutate(
                marker+"\r\nORIGINAL_RESET_CONTROLLERS_RC=0x00000000",
                "ORIGINAL_RESET_CONTROLLERS_RC=0x00000000\r\n"+marker),
                self.bank)
        with self.assertRaisesRegex(ValueError,"Unsupported original synth-mode"):
            auditor.audit(self.mutate(marker,
                         "ORIGINAL_AWEMAN32_SELECT_USER_CUSTOM_BANK_RC=0x00000000\r\n"+
                         marker),self.bank)

    def test_positive_source_log_gate_not_claim_original_pcm(self):
        result = auditor.audit(self.log, self.bank)
        self.assertEqual(
            result["status"], "ORIGINAL_CREATIVE_WIN95_API_GUEST_LOG_CONTRACT_PASS")
        self.assertEqual(result["source_sf2_bytes"], len(self.bank))
        self.assertEqual(result["original_driver_total_ram_words"], 0xE00000)
        self.assertEqual(result["original_driver_ram_allocated_words"], 0x10000)
        self.assertEqual(result["original_guest_load_begin_ms"], 0x200)
        self.assertEqual(result["original_guest_load_end_ms"], 0x300)
        self.assertEqual(result["original_86box_emu8000_register_trace"],
                         "REQUIRES_INDEPENDENT_CAPTURE")
        self.assertEqual(result["original_86box_guest_dry_pcm"],
                         "REQUIRES_INDEPENDENT_CAPTURE")
        self.assertEqual(
            result["original_Creative_AWE_SEND_MIDI_401_D4_vel100_zero_send_status"],
            "ORIGINAL_AWEMAN32_API_ACCEPTED")
        self.assertFalse(result["release_approved"])

    def test_suspected_midi_mapper_or_unknown_dll_fails_closed(self):
        with self.assertRaisesRegex(ValueError,"AWEMAN32"):
            auditor.audit(self.mutate(
                "C:\\WINDOWS\\SYSTEM\\AWEMAN32.DLL",
                "C:\\WINDOWS\\SYSTEM\\FAKEMAN32.DLL"), self.bank)
        with self.assertRaisesRegex(ValueError, "AWE MIDI device"):
            auditor.audit(self.mutate(
                "CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT=0x00000001",
                "CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT=0x00000002"),
                self.bank)

    def test_failed_load_and_missing_dry_midi_are_no_go(self):
        with self.assertRaisesRegex(ValueError,"LOAD_USER_BANK_RC"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000",
                "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000108"),
                self.bank)
        with self.assertRaisesRegex(ValueError,"CC93"):
            auditor.audit(self.mutate(
                "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC=0x00000000",
                "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC=0x00000001"),
                self.bank)

    def test_no_real_memory_delta_or_over_capacity(self):
        with self.assertRaisesRegex(ValueError,"no positive"):
            auditor.audit(self.mutate(
                "POST_LOAD_FREE_16BIT_WORDS=0x00CF0000",
                "POST_LOAD_FREE_16BIT_WORDS=0x00D00000"),
                self.bank)
        with self.assertRaisesRegex(ValueError,"RAM installed size"):
            auditor.audit(self.mutate(
                "PRE_LOAD_TOTAL_16BIT_WORDS=0x00E00000",
                "PRE_LOAD_TOTAL_16BIT_WORDS=0x00E00001"),
                self.bank)

    def test_forged_delta_or_duplicate_record_rejected(self):
        with self.assertRaisesRegex(ValueError,"delta"):
            auditor.audit(self.mutate(
                "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x00010000",
                "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x0000FFFF"),self.bank)
        body = self.log + b"PRE_LOAD_RESULT=0x00000000\r\n"
        with self.assertRaises(ValueError):
            auditor.audit(body,self.bank)
        double=self.mutate(
            "PRE_LOAD_RESULT=0x00000000",
            "PRE_LOAD_RESULT=0x00000000\r\nPRE_LOAD_RESULT=0x00000000")
        with self.assertRaisesRegex(ValueError,"Duplicated"):
            auditor.audit(double,self.bank)

    def test_truncated_source_bank_wrong_length_or_nonascii(self):
        with self.assertRaisesRegex(ValueError,"invalid private SF2"):
            auditor.audit(self.log,self.bank[:-1])
        with self.assertRaisesRegex(ValueError,"SF2"):
            auditor.audit(self.log,self.bank+b"x"*8)
        with self.assertRaisesRegex(ValueError,"Non-ASCII"):
            auditor.audit(self.log.replace(
                b"ORIGINAL_PROGRAM0_RC", b"ORIGINAL_PROGRAM0_R\xff"),self.bank)

    def test_same_size_different_soundfont_fails_whole_file_identity(self):
        changed=bytearray(self.bank)
        changed[-7]^=1
        self.assertEqual(len(changed),len(self.bank))
        with self.assertRaisesRegex(ValueError,"whole-file identity"):
            auditor.audit(self.log,bytes(changed))
        with self.assertRaisesRegex(ValueError,"whole-file identity"):
            auditor.audit(self.mutate(
                "PRIVATE_WIN95_SF2_FNV1A32=" +
                    hex32(auditor.fnv1a32(self.bank)),
                "PRIVATE_WIN95_SF2_FNV1A32=0x00000000"),self.bank)

    def test_direct_original_creative_midi_route_mandatory(self):
        with self.assertRaisesRegex(ValueError,"DIRECT_D4_VEL100"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC=0x00000000",
                "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC=0x00000001"),
                self.bank)
        with self.assertRaisesRegex(
                ValueError,"source phase|phase|timestamp log position/order"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC=0x00000000\r\n"
                "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END",
                "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END\r\n"
                "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC=0x00000000"),
                self.bank)

    def test_both_original_routes_require_bank_select_before_program(self):
        result=auditor.audit(self.log,self.bank)
        self.assertEqual(result["winmm_and_direct_bank_msb1_probe_order"],
                         "API_CALLS_CONFIRMED")
        self.assertEqual(result["original_bank_msb1_selects_loaded_RAM_bank"],
                         "NOT_PROVEN")
        # A broken or absent direct source bank select is never waived
        # because the WinMM route previously selected the bank.
        with self.assertRaisesRegex(ValueError,"DIRECT_BANK_SELECT_MSB1"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC=0x00000000",
                "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC=0x00000001"),
                self.bank)
        with self.assertRaisesRegex(ValueError,"DIRECT_BANK_SELECT_MSB1"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC=0x00000000",
                "REMOVED_BANK_SELECT=0x00000000"),self.bank)
        # Zero-returning source API calls in wrong order are not an
        # authentic comparable bank→program→NoteOn test sequence.
        for first,second in (
            ("ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC",
             "ORIGINAL_AWEMAN32_DIRECT_PROGRAM0_RC"),
            ("ORIGINAL_BANK_SELECT_MSB1_RC","ORIGINAL_PROGRAM0_RC"),
        ):
            original=(first+"=0x00000000\r\n"+
                      second+"=0x00000000")
            reordered=(second+"=0x00000000\r\n"+
                       first+"=0x00000000")
            with self.assertRaisesRegex(ValueError,"phase/order"):
                auditor.audit(self.mutate(original,reordered),self.bank)

    def test_missing_or_reordered_capture_marker(self):
        no_begin=self.mutate(
            "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_BEGIN",
            "MISSING_BEGIN")
        with self.assertRaises(ValueError):
            auditor.audit(no_begin,self.bank)
        displaced = self.mutate(
            "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000\r\n"
            "ORIGINAL_API_LOAD_END_TICK_MS=0x00000300\r\n"
            "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END",
            "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END\r\n"
            "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000\r\n"
            "ORIGINAL_API_LOAD_END_TICK_MS=0x00000300")
        with self.assertRaisesRegex(ValueError,"marker|escaped"):
            auditor.audit(displaced,self.bank)

    def test_original_load_duration_and_bank_size(self):
        with self.assertRaisesRegex(ValueError,"time order"):
            auditor.audit(self.mutate(
                "ORIGINAL_API_LOAD_END_TICK_MS=0x00000300",
                "ORIGINAL_API_LOAD_END_TICK_MS=0x00000010"),self.bank)
        with self.assertRaisesRegex(ValueError,"SoundFont bytes"):
            auditor.audit(self.mutate(
                f"PRIVATE_WIN95_SF2_SIZE={hex32(len(self.bank))}",
                "PRIVATE_WIN95_SF2_SIZE=0x00002000"),self.bank)

if __name__=="__main__":
    unittest.main()
