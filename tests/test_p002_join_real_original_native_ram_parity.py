"""Strict final P002 joint proof: public synthetic-only NEGATIVE controls.

Authentic Win95 source full 260,921-word bit parity is separately tested.
No real Creative ROM/SF2/VXD PCM or original port trace is committed.
"""
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import wave

from tools import p002_join_real_original_native_ram_parity as combined
from tools import p002_compare_original_native_ram_note_ports as ports

class OriginalNativeBothDomainTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="p002-both-synthetic-")
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.original_trace=self.root/"source.trace"
        self.native_ports=self.root/"native.port"
        self.sf2=self.root/"not-real-bank.sf2"
        self.log=self.root/"not-real-guest.log"
        self.source_wav=self.root/"original.wav"
        self.native_wav=self.root/"native.wav"
        self.sf2.write_bytes(b"NOT ORIGINAL OWNER SF2")
        self.log.write_bytes(b"NOT ORIGINAL CREATIVE LOG")
        self.original_trace.write_bytes((
            "99 E22 0\n"
            "100 E22 0\n"
            "100 A20 23F8\n"
            "R 100 E20 1234\n"
            "101 E22 C0\n"
            "101 620 F8D7\n"
            "101 622 23\n"
            "101 E22 E0\n"
            "101 620 FA68\n").encode("ascii"))
        self.native_lines=[
            "3 E22 0","3 A20 23F8","4 E22 C0","4 620 F8D7",
            "4 622 23","4 E22 E0","4 620 FA68"]
        self.write_ports(self.native_lines)
        self.write_wav(self.source_wav)
        self.write_wav(self.native_wav)

    def write_ports(self,lines):
        self.native_ports.write_bytes(
            ports.HEADER+ports.CLOCK_HEADER+
            ("\n".join(lines)+"\n").encode("ascii"))

    def write_wav(self,path,delta=0,zero=False):
        with wave.open(str(path),"wb") as out:
            out.setnchannels(2);out.setsampwidth(2);out.setframerate(44100)
            pcm=[]
            for n in range(1024):
                left=0 if zero else 1000+n%31
                right=0 if zero else -(950+n%27)
                if n==100:left+=delta
                pcm.extend([left,right])
            out.writeframes(struct.pack("<"+"h"*len(pcm),*pcm))

    def run_join(self,**kwargs):
        actual={
            "converted_source_words_mismatched":0,
            "converted_source_words_bit_equal":260921,
            "source_window_overwritten_words":255,
            "original_driver_ram_allocated_words":260937,
        }
        with patch.object(combined.transfer,"analyze",return_value=actual) as called:
            result=combined.analyze(
                kwargs.get("trace",self.original_trace),
                self.sf2,self.log,
                kwargs.get("original_wav",self.source_wav),
                kwargs.get("ports",self.native_ports),
                kwargs.get("native_wav",self.native_wav),
                original_port_origin=kwargs.get("original_port_origin",100),
                native_port_origin=kwargs.get("native_port_origin",3),
                port_frames=kwargs.get("port_frames",2),
                original_pcm_start=kwargs.get("original_pcm_start",0),
                native_pcm_start=kwargs.get("native_pcm_start",0),
                pcm_frames=kwargs.get("pcm_frames",1024),
                _synthetic_unit_test_only=kwargs.get("synthetic",True))
            called.assert_called_once()
            self.assertEqual(called.call_args.args[-1],0x200010)
            return result

    def test_both_synthetic_domains_equal_never_release_approved(self):
        x=self.run_join()
        self.assertTrue(x["both_windows_bit_exact"])
        self.assertTrue(x["native_original_ram_ports"]
                        ["sample_exact_16bit_port_writes"])
        self.assertTrue(x["native_original_dry_pcm"]["sample_exact"])
        self.assertEqual(x["original_ram_source_rewrites_preserved"],255)
        self.assertFalse(x["source_assets_authentic"])
        self.assertFalse(x["real_Win95_vs_exported_VST3_release_approved"])
        self.assertNotEqual(x["clock_origin_provenance"],"VERIFIED")

    def test_one_register_bit_changes_status_without_pcm_forgery(self):
        wrong=list(self.native_lines);wrong[3]="4 620 F8D6"
        self.write_ports(wrong)
        x=self.run_join()
        self.assertFalse(x["both_windows_bit_exact"])
        self.assertEqual(x["native_original_ram_ports"]
                         ["first_mismatch"]["field"],"value")
        self.assertTrue(x["native_original_dry_pcm"]["sample_exact"])

    def test_one_pcm_lsb_changes_status_without_register_forgery(self):
        self.write_wav(self.native_wav,delta=1)
        x=self.run_join()
        self.assertFalse(x["both_windows_bit_exact"])
        self.assertEqual(x["native_original_dry_pcm"]
                         ["mismatched_channel_samples"],1)
        self.assertTrue(x["native_original_ram_ports"]
                        ["sample_exact_16bit_port_writes"])
        self.assertFalse(x["real_Win95_vs_exported_VST3_release_approved"])

    def test_original_source_sha_must_be_pinned_in_production(self):
        with self.assertRaisesRegex(ValueError,"SHA-pinned ORIGINAL"):
            self.run_join(synthetic=False)

    def test_missing_wrong_or_overflowed_source_ram_proof(self):
        for value in (1,2):
            with self.subTest(value=value):
                fake={"converted_source_words_mismatched":value,
                      "converted_source_words_bit_equal":260921-value,
                      "source_window_overwritten_words":255}
                with patch.object(combined.transfer,"analyze",return_value=fake):
                    with self.assertRaisesRegex(ValueError,"RAM final-state"):
                        combined.analyze(
                            self.original_trace,self.sf2,self.log,
                            self.source_wav,self.native_ports,self.native_wav,
                            original_port_origin=100,native_port_origin=3,
                            port_frames=2,original_pcm_start=0,
                            native_pcm_start=0,pcm_frames=1024,
                            _synthetic_unit_test_only=True)

    def test_original_rewrite_count_alone_cannot_authorize_bad_source(self):
        fake={"converted_source_words_mismatched":0,
              "converted_source_words_bit_equal":260921,
              "source_window_overwritten_words":254}
        with patch.object(combined.transfer,"analyze",return_value=fake):
            with self.assertRaisesRegex(ValueError,"255 rewrite"):
                combined.analyze(
                    self.original_trace,self.sf2,self.log,
                    self.source_wav,self.native_ports,self.native_wav,
                    original_port_origin=100,native_port_origin=3,
                    port_frames=2,original_pcm_start=0,
                    native_pcm_start=0,pcm_frames=1024,
                    _synthetic_unit_test_only=True)

    def test_missing_native_evidence_and_identical_inputs_no_go(self):
        self.native_ports.unlink()
        with self.assertRaisesRegex(ValueError,"Missing original"):
            self.run_join()
        self.write_ports(self.native_lines)
        with self.assertRaisesRegex(ValueError,"independent source"):
            self.run_join(ports=self.original_trace)

    def test_no_auto_alignment_or_silent_match(self):
        x=self.run_join(native_port_origin=2)
        self.assertFalse(x["both_windows_bit_exact"])
        self.assertEqual(x["native_original_ram_ports"]
                         ["first_mismatch"]["field"],"relative_frame")
        with self.assertRaisesRegex(ValueError,"frame origins"):
            self.run_join(original_port_origin=-1)
        self.write_wav(self.source_wav,zero=True)
        self.write_wav(self.native_wav,zero=True)
        with self.assertRaisesRegex(ValueError,"silence|silent|nonzero"):
            self.run_join()

    def test_bad_native_recorder_header_no_go(self):
        self.native_ports.write_bytes(b"0 E22 3\n")
        with self.assertRaisesRegex(ValueError,"recorder header"):
            self.run_join()

if __name__=="__main__":
    unittest.main()
