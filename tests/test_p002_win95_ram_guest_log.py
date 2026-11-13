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
        "LOADED_CREATIVE_MODULE_PATH=C:\\WINDOWS\\SYSTEM\\AWEMAN32.DLL",
        "ORIGINAL_AWEMAN32_AWE_OPEN_RC=0x00000000",
        "WIN95_ORIGINAL_MIDI_ENDPOINTS=0x00000002",
        "WIN95_MIDI_DEVICE=0x00000000 NAME=OPL3 FM Synth",
        "WIN95_MIDI_DEVICE=0x00000001 NAME=SB AWE32 MIDI Synth",
        "CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT=0x00000001",
        "ORIGINAL_CREATIVE_MIDIOUT_OPEN_RESULT=0x00000000",
        "PRE_LOAD_RESULT=0x00000000",
        "PRE_LOAD_TOTAL_16BIT_WORDS=0x00E00000",
        "PRE_LOAD_FREE_16BIT_WORDS=0x00D00000",
        "ORIGINAL_API_LOAD_BEGIN_TICK_MS=0x00000200",
        "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_BEGIN",
        "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000",
        "ORIGINAL_API_LOAD_END_TICK_MS=0x00000300",
        "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END",
        "POST_LOAD_RESULT=0x00000000",
        "POST_LOAD_TOTAL_16BIT_WORDS=0x00E00000",
        "POST_LOAD_FREE_16BIT_WORDS=0x00CF0000",
        "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x00010000",
        "GUEST_DRAM_ALLOCATION_API_ALONE_NOT_HARDWARE_UPLOAD_PROOF",
        "ORIGINAL_RESET_CONTROLLERS_RC=0x00000000",
        "ORIGINAL_BANK_SELECT_MSB1_RC=0x00000000",
        "ORIGINAL_PROGRAM0_RC=0x00000000",
        "ORIGINAL_CC91_REVERB_SEND_ZERO_RC=0x00000000",
        "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC=0x00000000",
        "ORIGINAL_MIDI_C4_VEL100_RC=0x00000000",
        "ORIGINAL_MIDI_C4_OFF_RC=0x00000000",
        "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_BEGIN",
        "ORIGINAL_AWEMAN32_DIRECT_PROGRAM0_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_CC91_ZERO_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_CC93_ZERO_RC=0x00000000",
        "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC=0x00000000",
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
        with self.assertRaisesRegex(ValueError,"source phase|phase"):
            auditor.audit(self.mutate(
                "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC=0x00000000\r\n"
                "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END",
                "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END\r\n"
                "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC=0x00000000"),
                self.bank)

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
