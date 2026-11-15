"""P003: synthetic tests for original-fork CPU/I/O probe fail-closed patcher.
No Creative ROM, driver, VM or audio material used in public CI.
"""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from p003_instrument_86box_io import (
    instrument, git_blob_sha, HEADER, IN_HOOK, OUT_HOOK, MARKER)

ORIGINAL_SHAPED_SYNTHETIC = (
    '#include <stdio.h>\n'
    '#include <86box/timer.h>\n'
    '#include "cpu.h"\n'
    + HEADER + '\n'
    + 'uint16_t inw(uint16_t port) {\n    '
    + IN_HOOK + '\n    return ret;\n}\n'
    + 'void outw(uint16_t port, uint16_t val) {\n    '
    + OUT_HOOK + '\n    return;\n}\n'
).encode("utf-8")

class TestOriginal86BoxPortOnlyProbe(unittest.TestCase):
    def test_original_only_patch(self):
        original = ORIGINAL_SHAPED_SYNTHETIC
        patched = instrument(original, git_blob_sha(original))
        self.assertIn(MARKER.encode(), patched)
        self.assertIn(b'AWE64_P003_CPU_IO_TRACE', patched)
        self.assertIn(b'p003_cpu_io_note(\'R\', port, ret)', patched)
        self.assertIn(b'p003_cpu_io_note(\'W\', port, val)', patched)
        self.assertIn(b'CS, (unsigned) cpu_state.pc', patched)
        self.assertIn(b'(unsigned long long) tsc', patched)
        self.assertIn(b'(int) cycles', patched)
        self.assertEqual(original.count(b'io_log('), patched.count(b'io_log('))
        self.assertNotIn(b'emu8k_outw(', patched)
    def test_wrong_fork_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "hash differs"):
            instrument(ORIGINAL_SHAPED_SYNTHETIC)
    def test_changed_source_fails_closed(self):
        original = ORIGINAL_SHAPED_SYNTHETIC + b"\n// intentional mutation"
        with self.assertRaisesRegex(ValueError, "hash differs"):
            instrument(original, git_blob_sha(ORIGINAL_SHAPED_SYNTHETIC))
    def test_cannot_instrument_twice(self):
        once = instrument(ORIGINAL_SHAPED_SYNTHETIC, None)
        with self.assertRaisesRegex(ValueError, "already-instrumented"):
            instrument(once, None)
    def test_duplicate_hook_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "Missing/duplicated"):
            instrument(ORIGINAL_SHAPED_SYNTHETIC + OUT_HOOK.encode(), None)
    def test_missing_tsc_dependency_fails_closed(self):
        changed = ORIGINAL_SHAPED_SYNTHETIC.replace(
            b'#include <86box/timer.h>\n', b'')
        with self.assertRaisesRegex(ValueError, "prerequisites"):
            instrument(changed, None)

if __name__ == "__main__":
    unittest.main()
