"""P108 source-only PRIVATE win95 runner negative tests. Never opens owner media."""
import sys
import unittest
from unittest.mock import patch,Mock
from pathlib import Path
from types import SimpleNamespace
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
        self.assertEqual(env["AWE64_P107_EXECUTED_X86_TRACE"],str(Path("/fictional/i.trace")))
        self.assertEqual(env["AWE64_P108_DIRECT_BRANCH_TRACE"],str(Path("/fictional/b.trace")))
        self.assertEqual(env["AWE64_P107_LCOD_BASE"],"0xC10151E4")
        self.assertEqual(env["AWE64_P107_LCOD_SELECTOR"],"0x28")
        self.assertEqual(env["AWE64_P003_GUEST_CALLSTACK"],"1")
        self.assertEqual(env["KEEP"],"yes")
    def test_original_p110_translation_probe_off_by_default_and_private_opt_in(self):
        env=p.make_probe_environment(
            {"AWE64_P110_TIMING_TRANSLATION_TRACE":"stale",
             "AWE64_P110_LCOD_BASE":"bogus"},
            Path("/fake/I"),Path("/fake/B"),Path("/fake/IO"))
        self.assertNotIn("AWE64_P110_TIMING_TRANSLATION_TRACE",env)
        self.assertNotIn("AWE64_P110_LCOD_BASE",env)
        trace=Path("/fake/T")
        selected=p.make_probe_environment({},Path("/fake/I"),
                 Path("/fake/B"),Path("/fake/IO"),trace)
        self.assertEqual(selected["AWE64_P110_TIMING_TRANSLATION_TRACE"],str(trace))
        self.assertEqual(selected["AWE64_P110_LCOD_BASE"],"0xC10151E4")
        self.assertEqual(selected["AWE64_P110_LCOD_SELECTOR"],"0x28")

    def test_original_p110_translator_requested_requires_real_private_trace(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        valid={"executed_jcc_total":3450,"unobserved_or_unexplained_jcc":0,
               "next_pc_independently_confirms_flags":3410}
        physical={"original_tsc_first":0,"original_tsc_last":2000}
        with (patch.object(p,"completed_original_guest",return_value=proof),
              patch.object(p,"read_trace",
                           return_value=([SimpleNamespace(tsc=1000)],"fixtureI")),
              patch.object(p,"read_direct",return_value=({0:object()},"fixtureB")),
              patch.object(p,"compare_original",return_value=valid),
              patch.object(p,"verify_original_physical_cpu_io",return_value=physical),
              patch.object(p,"audit_p110_translator",
                           side_effect=ValueError("absent P110 translation source"))):
            with self.assertRaisesRegex(ValueError,"absent P110"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/I"),
                                         Path("/fake/B"),Path("/fake/IO"),
                                         Path("/fake/P110"))
        with (patch.object(p,"completed_original_guest",return_value=proof),
              patch.object(p,"read_trace",
                           return_value=([SimpleNamespace(tsc=1000)],"fixtureI")),
              patch.object(p,"read_direct",return_value=({0:object()},"fixtureB")),
              patch.object(p,"compare_original",return_value=valid),
              patch.object(p,"verify_original_physical_cpu_io",return_value=physical),
              patch.object(p,"audit_p110_translator",return_value={
                  "status":"PENTIUM_SOURCE_TRANSLATION_ONLY_NOT_EXECUTED_TIMING",
                  "original_agi_positive":0}) as auditor):
            report=p.verify_private_capture(Path("/fake/img"),Path("/fake/I"),
                          Path("/fake/B"),Path("/fake/IO"),Path("/fake/P110"))
            self.assertEqual(report["original_p110_compile_time_evidence"]["original_agi_positive"],0)
            self.assertFalse(report["original_tsc_reproduction_proved"])
            self.assertFalse(report["native_vst3_pcm_bit_exact_proved"])
            auditor.assert_called_once_with(Path("/fake/P110"),min_translations=50)

    def test_p112_private_original_agreement_only_for_same_session(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        flags={"executed_jcc_total":3450,
               "unobserved_or_unexplained_jcc":0,
               "next_pc_independently_confirms_flags":3410}
        physical={"original_tsc_first":0,"original_tsc_last":2000}
        with (patch.object(p,"completed_original_guest",return_value=proof),
              patch.object(p,"read_trace",
                           return_value=([SimpleNamespace(tsc=1000)],"fake-I")),
              patch.object(p,"read_direct",return_value=({1:object()},"fake-B")),
              patch.object(p,"compare_original",return_value=flags),
              patch.object(p,"verify_original_physical_cpu_io",return_value=physical),
              patch.object(p,"audit_p110_translator",return_value={"status":"fictional-P110"}),
              patch.object(p,"audit_p111_join",return_value={"status":"fictional-P111"}),
              patch.object(p,"join_p112",return_value={
                  "status":"SOURCE_REGMASK_ORIGINAL_JIT_GENERATION_JOIN_NOT_AGI_CAUSALITY",
                  "91_93_95_original_tsc_causality_proven":False}) as joined):
            with self.assertRaisesRegex(ValueError,"both authenticated original"):
                p.verify_private_capture(Path("/fake/win95.img"),Path("/session/I"),
                    Path("/session/B"),Path("/session/io"),None,
                    Path("/session/P111"),True)
            with self.assertRaisesRegex(ValueError,"same private original VM session"):
                p.verify_private_capture(Path("/fake/win95.img"),Path("/session/I"),
                    Path("/session/B"),Path("/session/io"),Path("/session/P110"),
                    Path("/other-session/P111"),True)
            report=p.verify_private_capture(Path("/fake/win95.img"),Path("/session/I"),
                    Path("/session/B"),Path("/session/io"),Path("/session/P110"),
                    Path("/session/P111"),True)
            self.assertFalse(report["original_p112_carried_register_masks_vs_executed_jit"][
                "91_93_95_original_tsc_causality_proven"])
            self.assertFalse(report["native_vst3_pcm_bit_exact_proved"])
            joined.assert_called_once_with(Path("/session/P110"),
                                            Path("/session/P111"),min_common_starts=20)

    def test_p111_original_jit_run_join_is_opt_in_only(self):
        existing={"AWE64_P111_JIT_BLOCK_JOIN_TRACE":"stale",
                  "AWE64_P111_LCOD_SELECTOR":"bad"}
        env=p.make_probe_environment(existing,Path("/fake/i"),Path("/fake/b"),
                                     Path("/fake/io"))
        self.assertNotIn("AWE64_P111_JIT_BLOCK_JOIN_TRACE",env)
        self.assertNotIn("AWE64_P111_LCOD_SELECTOR",env)
        candidate=Path("/fake/p111")
        selected=p.make_probe_environment({},Path("/fake/i"),Path("/fake/b"),
                                          Path("/fake/io"),None,candidate)
        self.assertEqual(selected["AWE64_P111_JIT_BLOCK_JOIN_TRACE"],str(candidate))
        self.assertEqual(selected["AWE64_P111_LCOD_BASE"],"0xC10151E4")
        self.assertEqual(selected["AWE64_P111_LCOD_SELECTOR"],"0x28")
        self.assertEqual(existing["AWE64_P111_JIT_BLOCK_JOIN_TRACE"],"stale")

    def test_requested_original_p111_block_run_requires_real_trace(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        orig={"executed_jcc_total":3450,
              "unobserved_or_unexplained_jcc":0,
              "next_pc_independently_confirms_flags":3410}
        physical={"original_tsc_first":0,"original_tsc_last":2000}
        with (patch.object(p,"completed_original_guest",return_value=proof),
              patch.object(p,"read_trace",
                           return_value=([SimpleNamespace(tsc=1000)],"fixtureI")),
              patch.object(p,"read_direct",return_value=({0:object()},"fixtureB")),
              patch.object(p,"compare_original",return_value=orig),
              patch.object(p,"verify_original_physical_cpu_io",return_value=physical),
              patch.object(p,"audit_p111_join",
                           side_effect=ValueError("missing original JIT execution"))):
            with self.assertRaisesRegex(ValueError,"missing original JIT"):
                p.verify_private_capture(Path("/fake/win95.img"),Path("/fake/i"),
                     Path("/fake/b"),Path("/fake/io"),None,Path("/fake/p111"))
        with (patch.object(p,"completed_original_guest",return_value=proof),
              patch.object(p,"read_trace",
                           return_value=([SimpleNamespace(tsc=1000)],"fixtureI")),
              patch.object(p,"read_direct",return_value=({0:object()},"fixtureB")),
              patch.object(p,"compare_original",return_value=orig),
              patch.object(p,"verify_original_physical_cpu_io",return_value=physical),
              patch.object(p,"audit_p111_join",return_value={
                  "original_compiled_but_unexecuted":5,
                  "per_instruction_cpu_tsc_proven":False})):
            result=p.verify_private_capture(Path("/fake/win95.img"),Path("/fake/i"),
                     Path("/fake/b"),Path("/fake/io"),None,Path("/fake/p111"))
            self.assertEqual(result["original_p111_jit_compilation_execution_join"][
                "original_compiled_but_unexecuted"],5)
            self.assertFalse(result["original_tsc_reproduction_proved"])
            self.assertFalse(result["native_vst3_pcm_bit_exact_proved"])

    def test_fake_three_bank_guest_success_is_still_not_enough_without_x86(self):
        fake_log={"original_win95_midi_send_calls":4176,
                  "original_win95_midi_api_failures":0}
        with patch.object(p,"completed_original_guest",return_value=fake_log),\
             patch.object(p,"read_trace",return_value=([SimpleNamespace(tsc=1000)], "fake-i")) as rt,\
             patch.object(p,"read_direct",return_value=({0:object()},"fake-b")) as rd,\
             patch.object(p,"compare_original",return_value={
                 "executed_jcc_total":3000,
                 "unobserved_or_unexplained_jcc":0,
                 "next_pc_independently_confirms_flags":2999
             }),\
             patch.object(p,"verify_original_physical_cpu_io",return_value={"original_tsc_first":0,"original_tsc_last":2000}):
            with self.assertRaisesRegex(ValueError,"too few independently"):
                p.verify_private_capture(Path("/fictional/img"),Path("/fake/i"),Path("/fake/b"),Path("/fake/io"))
            rt.assert_called_once()
            rd.assert_called_once()
    def test_completely_crosschecked_synthetic_requires_status_not_pcm(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        fake={"executed_jcc_total":3450,"unobserved_or_unexplained_jcc":0,
              "next_pc_independently_confirms_flags":3410}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"read_trace",return_value=([SimpleNamespace(tsc=1000)],"sha-i")) as rt,\
             patch.object(p,"read_direct",return_value=({3:object()},"sha-b")) as rd,\
             patch.object(p,"compare_original",return_value=fake),\
             patch.object(p,"verify_original_physical_cpu_io",return_value={"original_tsc_first":0,"original_tsc_last":2000}):
            result=p.verify_private_capture(Path("/fictional/img"),Path("/fake/i"),Path("/fake/b"),Path("/fake/io"))
            self.assertTrue(result["status"].startswith("ORIGINAL_X86"))
            self.assertFalse(result["original_tsc_reproduction_proved"])
            self.assertFalse(result["native_vst3_pcm_bit_exact_proved"])
            self.assertFalse(result["private_trace_or_creative_rom_published"])
    def test_fail_if_private_cpu_branch_missing(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        with patch.object(p,"completed_original_guest",return_value=proof),\
             patch.object(p,"read_trace",return_value=([SimpleNamespace(tsc=1000)],"sha-i")) as rt,\
             patch.object(p,"read_direct",return_value=({3:object()},"sha-b")) as rd,\
             patch.object(p,"compare_original",return_value={
                 "executed_jcc_total":3450,
                 "unobserved_or_unexplained_jcc":1,
                 "next_pc_independently_confirms_flags":3410
             }),\
             patch.object(p,"verify_original_physical_cpu_io",return_value={"original_tsc_first":0,"original_tsc_last":2000}):
            with self.assertRaisesRegex(ValueError,"unconfirmed Jcc"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/i"),Path("/fake/b"),Path("/fake/io"))
    def test_private_original_audit_refuses_missing_physical_cpu_io(self):
        with patch.object(p,"completed_original_guest",return_value={
                  "original_win95_midi_send_calls":4176,
                  "original_win95_midi_api_failures":0}), \
             patch.object(p,"read_trace",return_value=([SimpleNamespace(tsc=1000)],"fake-i")), \
             patch.object(p,"read_direct",return_value=({1:object()},"fake-b")), \
             patch.object(p,"compare_original",return_value={
                  "executed_jcc_total":3450,
                  "unobserved_or_unexplained_jcc":0,
                  "next_pc_independently_confirms_flags":3410}):
            with self.assertRaisesRegex(ValueError,"physical CPU V4 I/O evidence required"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/i"),
                                         Path("/fake/b"),None)

    def test_guest_tsc_disjoint_from_physical_original_io_is_no_go(self):
        with patch.object(p,"completed_original_guest",return_value={
                  "original_win95_midi_send_calls":4176,
                  "original_win95_midi_api_failures":0}), \
             patch.object(p,"read_trace",return_value=([SimpleNamespace(tsc=9000)],"fake-i")), \
             patch.object(p,"read_direct",return_value=({1:object()},"fake-b")), \
             patch.object(p,"compare_original",return_value={
                  "executed_jcc_total":3450,
                  "unobserved_or_unexplained_jcc":0,
                  "next_pc_independently_confirms_flags":3410}), \
             patch.object(p,"verify_original_physical_cpu_io",return_value={
                  "original_tsc_first":0,"original_tsc_last":2000}):
            with self.assertRaisesRegex(ValueError,"disjoint guest-TSC"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/i"),
                                         Path("/fake/b"),Path("/fake/io"))
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
    @staticmethod
    def _fictional_physical_rows(n=145, bad_at=None, mutation=None):
        lines=[b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK synthetic-only\n"]
        for i in range(n):
            port=0x620 if i%2==0 else 0xe22
            seq=i
            tsc=1000+i*4
            if i==bad_at and mutation=="seq":seq=i+1
            if i==bad_at and mutation=="tsc":tsc=1
            if i==bad_at and mutation=="port":port=0x123
            line=(f"r {tsc} 0 0028 C10151E4 {port:04X} 1234 {seq} "+
                  ("00000000 "*8)+"00000000\n")
            lines.append(line.encode("ascii"))
        return b"".join(lines)

    def test_private_physical_guest_source_trace_is_verified_and_redacted(self):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/"fictional-io.trace"
            f.write_bytes(self._fictional_physical_rows())
            result=p.verify_original_physical_cpu_io(f,min_samples=120)
            self.assertEqual(result["original_physical_callback_count"],145)
            self.assertEqual(len(result["original_physical_io_sha256"]),64)
            self.assertFalse(result["original_p54c_cpu_tsc_reconstructed"])
            self.assertNotIn("1234",str(result))

    def test_missing_or_mutated_physical_callbacks_fail_closed(self):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/"fictional-io.trace"
            for mutation,err in (("seq","sequence"),("tsc","regressed"),
                                 ("port","outside EMU8000")):
                with self.subTest(mutation=mutation):
                    f.write_bytes(self._fictional_physical_rows(145,55,mutation))
                    with self.assertRaisesRegex(ValueError,err):
                        p.verify_original_physical_cpu_io(f,min_samples=120)
            f.write_bytes(self._fictional_physical_rows())
            with self.assertRaisesRegex(ValueError,"too few"):
                p.verify_original_physical_cpu_io(f,min_samples=150)
            f.write_bytes(b"WRONG HEADER\n"+self._fictional_physical_rows())
            with self.assertRaisesRegex(ValueError,"header"):
                p.verify_original_physical_cpu_io(f,min_samples=2)

    def test_private_live_original_gate_demands_io_trace(self):
        proof={"original_win95_midi_send_calls":4176,
               "original_win95_midi_api_failures":0}
        with (patch.object(p,"completed_original_guest",return_value=proof),
             patch.object(p,"read_trace",return_value=([object()],"synthetic-i")),
             patch.object(p,"read_direct",return_value=({0:object()},"synthetic-b")),
             patch.object(p,"compare_original",return_value={
                 "executed_jcc_total":3450,
                 "unobserved_or_unexplained_jcc":0,
                 "next_pc_independently_confirms_flags":3410}),
             patch.object(p,"verify_original_physical_cpu_io",side_effect=ValueError("missing IO"))):
            with self.assertRaisesRegex(ValueError,"missing IO"):
                p.verify_private_capture(Path("/fake/img"),Path("/fake/i"),
                                         Path("/fake/b"),Path("/fake/io"))

    def test_private_report_sha_only_does_not_require_trace_in_public_ci(self):
        with tempfile.TemporaryDirectory() as t:
            f=Path(t)/"synthetic.txt"
            f.write_bytes(b"fictional public test")
            self.assertEqual(len(p.sha_file(f)),64)
            self.assertNotIn("original",p.sha_file(f))
if __name__=="__main__":
    unittest.main()
