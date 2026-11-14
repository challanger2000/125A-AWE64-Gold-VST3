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

    def test_real_source_guest_log_sha_gate_rejects_cross_session_log(self):
        # A real recorded bank/trace/WAV plus a different but seemingly
        # successful guest log may describe TWO different driver sessions.
        # Production hash attestation must fail before translating notes.
        real_hashes={
            "original_emu8k_trace_sha256":combined.ORIGINAL_TRACE_SHA256,
            "original_owner_sf2_sha256":combined.ORIGINAL_SF2_SHA256,
            "original_win95_precard_wav_sha256":combined.ORIGINAL_WAV_SHA256,
            "original_guest_log_sha256":combined.ORIGINAL_GUEST_LOG_SHA256,
        }
        original_sha=combined._sha
        def pinned_except_guest(path):
            if path==self.original_trace:return real_hashes["original_emu8k_trace_sha256"]
            if path==self.sf2:return real_hashes["original_owner_sf2_sha256"]
            if path==self.source_wav:return real_hashes["original_win95_precard_wav_sha256"]
            return original_sha(path)
        with patch.object(combined,"_sha",side_effect=pinned_except_guest):
            with self.assertRaisesRegex(ValueError,
                                        "original_guest_log_sha256"):
                self.run_join(synthetic=False)

    def test_real_join_requires_actual_lcod_voice_start_prologue(self):
        # Even bit-exact IFATN and unrelated PCM from a separately
        # selected window cannot be the claimed original RAM NoteOn.
        with patch.object(combined.transfer,"analyze",return_value={
            "converted_source_words_mismatched":0,
            "converted_source_words_bit_equal":260921,
            "source_window_overwritten_words":255,
        }), patch.object(combined,"_sha",side_effect=lambda p:{
            self.original_trace:combined.ORIGINAL_TRACE_SHA256,
            self.sf2:combined.ORIGINAL_SF2_SHA256,
            self.source_wav:combined.ORIGINAL_WAV_SHA256,
            self.log:combined.ORIGINAL_GUEST_LOG_SHA256,
        }.get(p,"native-diagnostic")), patch.object(
            combined,"verify_native_clock_manifest",
            return_value={"native_midi_clock_host_attested":True}), patch.object(
            combined.ports,"compare_windows",
            side_effect=ValueError("allocator prologue")) as compare_call:
            with self.assertRaisesRegex(ValueError,"allocator prologue"):
                combined.analyze(
                    self.original_trace,self.sf2,self.log,
                    self.source_wav,self.native_ports,self.native_wav,
                    original_port_origin=100,native_port_origin=3,
                    port_frames=2,original_pcm_start=0,
                    native_pcm_start=0,pcm_frames=1024,
                    native_manifest=self.root/"private-host-clock.json")
            self.assertTrue(compare_call.call_args.kwargs[
                "require_complete_note_start"])

    def test_owner_clock_manifest_attests_actual_native_file_bytes_and_midi(self):
        manifest=self.root/"native-host-clock.json"
        item={
            "schema":combined.NATIVE_CLOCK_SCHEMA,
            "sample_rate":44100,
            "host_activation_origin_frame":0,
            "note_c4_on_frame":3,
            "note_c4_off_frame":400,
            "note_d4_on_frame":500,
            "note_d4_off_frame":900,
            "pcm_frames":1024,
            "midi_channel":0,
            "midi_velocity":100,
            "audible_c4_on_frames":200,
            "audible_d4_on_frames":230,
            "original_win95_vs_native_pcm_parity":"NOT_COMPARED",
            "original_creative_driver_provenance":"NOT_PROVEN_BY_HOST",
            "native_chip_out_trace_sha256":ports.sha(self.native_ports),
            "native_precard_wav_sha256":ports.sha(self.native_wav),
            "original_owner_sf2_sha256":ports.sha(self.sf2),
            "original_rom_sha256":combined.ORIGINAL_ROM_SHA256,
            "exported_vst3_sha256":"a"*64,
            "original_ram_ports_sha256":"b"*64,
            "original_final_ram_pcm_sha256":"c"*64,
            "original_vxd_pack_sha256":"d"*64,
        }
        def verify(**override):
            manifest.write_text(json.dumps(item|override),encoding="utf-8")
            return combined.verify_native_clock_manifest(
                manifest,self.native_ports,self.native_wav,self.sf2,
                note="C4",native_port_origin=3,native_pcm_start=3)
        good=verify()
        self.assertTrue(good["native_midi_clock_host_attested"])
        self.assertEqual(good["host_note_on_frame"],3)
        for mutation in (
            {"native_chip_out_trace_sha256":"a"*64},
            {"native_precard_wav_sha256":"a"*64},
            {"original_owner_sf2_sha256":"a"*64},
            {"note_c4_on_frame":4},
            {"note_d4_off_frame":1024},
            {"midi_velocity":99},
            {"midi_channel":1},
            {"audible_d4_on_frames":0},
            {"pcm_frames":1025},
            {"sample_rate":48000},
            {"host_activation_origin_frame":1},
            {"original_win95_vs_native_pcm_parity":"PASSED"},
            {"exported_vst3_sha256":"not-a-sha"},
        ):
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):verify(**mutation)
        verify()
        self.native_ports.write_bytes(self.native_ports.read_bytes()+b"\n")
        with self.assertRaisesRegex(ValueError,"SHA256 mismatch"):
            combined.verify_native_clock_manifest(
                manifest,self.native_ports,self.native_wav,self.sf2,
                note="C4",native_port_origin=3,native_pcm_start=3)

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
