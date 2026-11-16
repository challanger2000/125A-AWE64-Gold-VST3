"""P002 private original ROM-bank resource-pack schema and fail-closed inputs.

Synthetic LE data only; NEVER include Creative original proprietary bytes.
"""
import unittest
from unittest import mock
from tools import p002_make_private_vst3_rom_bank_resources as mod
from tools.p002_extract_original_vxd_sf2_resources import extract
from test_p002_extract_original_vxd_sf2_resources import synthetic_le
import struct
import hashlib

def idle_state():
    state=bytearray(0x1592)
    struct.pack_into("<I",state,0x158e,0x4d474d31)
    struct.pack_into("<H",state,0x06,32)
    struct.pack_into("<I",state,0x18,32)
    for slot in range(30):
        struct.pack_into("<H",state,0x42+slot*0x20,0x0ffe)
    return state

class OriginalRomBankPackTest(unittest.TestCase):
    def make(self, state=None, **args):
        # Only this PUBLIC SYNTHETIC unittest monkey-patches the exact
        # original hash oracle. The CLI does not provide this bypass.
        fake=extract(synthetic_le(),require_original_hash=False)
        digest=hashlib.sha256(mod.vxd_original_table_bytes(fake)).hexdigest()
        with (mock.patch.object(mod,"extract",return_value=fake),
              mock.patch.object(mod,"EXPECTED_ORIGINAL_NATIVE_TABLE_SHA256",
                                digest)):
            return mod.build(synthetic_le(),
                             bytes(idle_state() if state is None else state),
                             program=args.get("program",7),
                             channel=args.get("channel",2),
                             bank=args.get("bank",0))

    def test_exact_little_endian_size_and_source_table_positions(self):
        b=self.make()
        self.assertEqual(len(b),6508)
        self.assertEqual(b[:8],mod.MAGIC)
        self.assertEqual(b[8:40],
                         bytes.fromhex(mod.EXPECTED_VXD_SHA256))
        self.assertEqual(struct.unpack_from("<BBHI",b,40),
                         (7,2,0,524288))
        state_end=48+0x1592
        self.assertEqual(b[48:state_end],bytes(idle_state()))
        original_base=state_end+118
        self.assertEqual(struct.unpack_from("<59h",b,original_base),
                         tuple(range(-29,30)))
        pitch_base=original_base+118
        self.assertEqual(struct.unpack_from("<I",b,pitch_base)[0],1000)
        self.assertEqual(struct.unpack_from("<I",b,pitch_base+127*4)[0],
                         1000+127*10000)
        self.assertEqual(struct.unpack_from("<b",b,pitch_base+512)[0],64)

    def test_source_hash_field_cannot_authorize_forged_original_tables(self):
        # Even after a mocked authenticated LE parser (ONLY public
        # synthetic test), source-table content must match an independent
        # immutable digest. A correct magic/VXD SHA header is not enough.
        fake=extract(synthetic_le(),require_original_hash=False)
        self.assertNotEqual(
            hashlib.sha256(mod.vxd_original_table_bytes(fake)).hexdigest(),
            mod.EXPECTED_ORIGINAL_NATIVE_TABLE_SHA256)
        with mock.patch.object(mod,"extract",return_value=fake):
            with self.assertRaisesRegex(ValueError,"LCOD/PDAT"):
                mod.build(synthetic_le(),bytes(idle_state()),
                          program=0,channel=0,bank=0)
        self.assertEqual(len(mod.vxd_original_table_bytes(fake)),820)

    def test_missing_original_source_sha_cannot_be_bypassed_by_cli(self):
        with self.assertRaisesRegex(ValueError,"exact original"):
            mod.build(synthetic_le(),bytes(idle_state()),
                      program=7,channel=2,bank=0)

    def test_unknown_snapshot_and_busy_voices_fail_closed(self):
        s=idle_state();s[0x158e]^=1
        with self.assertRaisesRegex(ValueError,"dry 1MGM"):
            self.make(s)
        s=idle_state();s[0x42]=60
        with self.assertRaisesRegex(ValueError,"not idle"):
            self.make(s)
        # Former synthetic 30-voice capacity was not the original
        # SBAWE.VXD snapshot: 32 physical vs 30 music. Reject it
        # even though all 30 music voice rows appear idle.
        for wrong in (30,31,33):
            s=idle_state();s[0x06]=wrong
            with self.subTest(capacity=wrong):
                with self.assertRaisesRegex(ValueError,"32-physical-voice"):
                    self.make(s)
        s=idle_state();struct.pack_into("<I",s,0x18,30)
        with self.assertRaisesRegex(ValueError,"32-physical-voice"):
            self.make(s)
        with self.assertRaisesRegex(ValueError,"0x1592"):
            self.make(s[:-1])

    def test_program_channel_and_bank_bounds(self):
        for args in (dict(program=128),dict(channel=16),dict(bank=129),
                     dict(program=-1),dict(channel=-1)):
            with self.subTest(args=args):
                with self.assertRaisesRegex(ValueError,"Unsupported"):
                    self.make(**args)

if __name__=="__main__":
    unittest.main()
