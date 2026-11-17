"""P108 source-only PRIVATE win95 runner negative tests. Never opens owner media."""
import sys
import unittest
from unittest.mock import patch,Mock
from pathlib import Path
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import p108_private_win95_original_capture as p

class TestP108PrivateOriginalWin95Capture(unittest.TestCase):
    def test_guarded_env_has_real_both_execution_channels_and_original_lcod(self):
        existing={"PATH":"/fictional","KEEP":"yes"}
        orig=dict(existing)
        env=p.make_probe_environment(existing,Path("/fictional/i.trace"),
                                     Path("/fictional/b.trace"),Path("/fictional/io.trace"))
        self.assertEqual(existing,orig)
        self.assertEqual(env["AWE64_P107_EXECUTED_X86_TRACE"],"/fictional/i.trace")
        self.assertEqual(env["AWE64_P108_DIRECT_BRANCH_TRACE"],"/fictional/b.trace")
        self.assertEqual(env["AWE64_P107_LCOD_BASE"],"0xC10151E4")
        self.assertEqual(env["AWE64_P107_LCOD_SELECTOR"],"0x28")
        self.assertEqual(env["AWE64_P003_GUEST_CALLSTACK"],"1")
        self.assertEqual(env["KEEP"],"yes")
    def test_fake_three_bank_guest_success_is_still_not_enough_without_x86(self):
        fake_log={"original_win95_midi_send_calls":4176,
                  "original_win95_midi_api_failures":0}
        with patch.object(p,"completed_original_guest",return_value=fake_log),\
             patch.object(p,"read_trace",return_value=([object()], "fake-i")) as rt,\
             patch.object(p,"read_direct",return_value=({0:object()},"fake-b")) as rd,\
             patch.object(p,"compare_original",return_value={
                 "executed_jcc_total":3000,
                 "unobserved_or_unexplained_jcc":0,
                 "next_pc_independently_confirms_flags":2999
             }):
            with self.assertRaisesRegex(ValueError,"too few independently"):
                p.verify_private_capture(Path("/fictional/img"),Path("/fake/i"),Path("/fake/b"))
            rt.assert_called_once()
            rd.assert_called_once()
    def test_completely_crosschecked_synthetic_requires_status_not_pcm(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        fake={"executed_jcc_total":3450,"unobserved_or_unexplained_jcc":0,
              "next_pc_independently_confirms_flags":3410}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"read_trace",return_value=([object()],"sha-i")) as rt,\
             patch.object(p,"read_direct",return_value=({3:object()},"sha-b")) as rd,\
             patch.object(p,"compare_original",return_value=fake):
            result=p.verify_private_capture(Path("/fictional/img"),Path("/fake/i"),Path("/fake/b"))
            self.assertTrue(result["status"].startswith("ORIGINAL_X86"))
            self.assertFalse(result["original_tsc_reproduction_proved"])
            self.assertFalse(result["native_vst3_pcm_bit_exact_proved"])
            self.assertFalse(result["private_trace_or_creative_rom_published"])
    def test_fail_if_private_cpu_branch_missing(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"read_trace",return_value=([object()],"sha-i")) as rt,\
             patch.object(p,"read_direct",return_value=({3:object()},"sha-b")) as rd,\
             patch.object(p,"compare_original",return_value={
                 "executed_jcc_total":3450,
                 "unobserved_or_unexplained_jcc":1,
                 "next_pc_independently_confirms_flags":3410
             }):
            with self.assertRaisesRegex(ValueError,"unconfirmed Jcc"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/i"),Path("/fake/b"))
    def test_no_automatic_success_without_guest_log(self):
        proc=Mock()
        proc.poll.return_value=0
        with patch.object(p,"completed_original_guest",return_value=None):
            with self.assertRaisesRegex(ValueError,"exited before completed"):
                p.watch_guest_until_done(proc,Path("/fake/img"),{"DISPLAY":":99"},10,.01)
    def test_guest_completed_does_not_count_without_graceful_emulator_close(self):
        proc=Mock()
        proc.poll.return_value=None
        proof={"original_win95_midi_send_calls":4176}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"close_x11_emulator_gracefully",return_value=False),\
             patch.object(p.time,"sleep",return_value=None):
            with self.assertRaisesRegex(ValueError,"no graceful local X11"):
                p.watch_guest_until_done(proc,Path("/fake/img"),{"DISPLAY":":99"},1,0.001)
    def test_graceful_close_and_original_exit_are_both_mandatory(self):
        proc=Mock()
        proc.poll.return_value=None
        proc.wait.return_value=1
        proof={"original_win95_midi_send_calls":4176}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"close_x11_emulator_gracefully",return_value=True),\
             patch.object(p.time,"sleep",return_value=None):
            with self.assertRaisesRegex(ValueError,"nonzero"):
                p.watch_guest_until_done(proc,Path("/fake/img"),{"DISPLAY":":99"},1,.001)
    def test_private_sha_rejects_missing_original_sources(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaisesRegex(ValueError,"original 512 MiB"):
                p.verify_original_guest_sources(Path(t),Path(t)/"fake.exe",Path(t)/"fake.sf2")
    def test_private_report_sha_only_does_not_require_trace_in_public_ci(self):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/"synthetic.txt"
            f.write_bytes(b"fictional public test")
            self.assertEqual(len(p.sha_file(f)),64)
            self.assertNotIn("original",p.sha_file(f))
if __name__=="__main__":
    unittest.main()
