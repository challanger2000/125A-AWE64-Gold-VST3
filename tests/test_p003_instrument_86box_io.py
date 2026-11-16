"""P003: synthetic tests for original-fork physical 16-bit callback probe.
Check OUTL/INL physical split hooks, not merely guest OUTW/INW.
No Creative ROM, Win95 driver, VM or audio used in public CI.
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
    + 'uint8_t inb(uint16_t port) {\n'
    + '    ret &= p->inb(port, p->priv);\n'
    + '    return ret;\n}\n'
    + 'void outb(uint16_t port, uint8_t val) {\n'
    + '    p->outb(port, val, p->priv);\n'
    + '    return;\n}\n'
    + 'uint16_t inw(uint16_t port) {\n'
    + '    ret &= p->inw(port, p->priv);\n'
    + '    ' + IN_HOOK + '\n    return ret;\n}\n'
    + 'void outw(uint16_t port, uint16_t val) {\n'
    + '    p->outw(port, val, p->priv);\n'
    + '    ' + OUT_HOOK + '\n    return;\n}\n'
    + 'uint32_t inl(uint16_t port) {\n'
    + '    ret16[0] &= p->inw(port, p->priv);\n'
    + '    ret16[1] &= p->inw(port + 2, p->priv);\n'
    + '    return ret;\n}\n'
    + 'void outl(uint16_t port, uint32_t val) {\n'
    + '    p->outw(port + i, val >> (i << 3), p->priv);\n'
    + '    return;\n}\n'
).encode("utf-8")

class TestPhysical16CallbackProbe(unittest.TestCase):
    def test_original_source_only_patch(self):
        original = ORIGINAL_SHAPED_SYNTHETIC
        patched = instrument(original, git_blob_sha(original))
        self.assertIn(MARKER.encode(), patched)
        self.assertIn(b'AWE64_P003_CPU_IO_TRACE', patched)
        self.assertIn(b'AWE64_P003_GUEST_CALLSTACK',patched)
        self.assertIn(b'P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK',patched)
        self.assertIn(b'p003_guest_stack_returns(retn)',patched)
        self.assertIn(b'lookup=readlookup2[linear >> 12]',patched)
        self.assertIn(b'lookup==(uintptr_t)-1',patched)
        self.assertIn(b'next<=frame || next-frame>65536u',patched)
        self.assertIn(b'#include <86box/mem.h>',patched)
        self.assertNotIn(b'write_mem_l(',patched)
        self.assertIn(b"p003_cpu_io_note('R', port, value)", patched)
        self.assertIn(b"p003_cpu_io_note('W', port, value)", patched)
        self.assertIn(b'CS, (unsigned) cpu_state.pc', patched)
        self.assertIn(b'(unsigned long long) tsc', patched)
        self.assertIn(b'(int) cycles', patched)
        self.assertEqual(original.count(b'io_log('), patched.count(b'io_log('))
        self.assertNotIn(b'emu8k_outw(', patched)
        # Ordinary word handler, two word handlers in guest INL,
        # guest OUTL physically splits into two word callbacks.
        self.assertEqual(patched.count(b'p003_physical_inw('), 4)
        self.assertEqual(patched.count(b'p003_physical_outw('), 3)
        self.assertEqual(patched.count(b'p003_physical_inb('), 2)
        self.assertEqual(patched.count(b'p003_physical_outb('), 2)
        self.assertIn(b"p003_cpu_io_note('r', port &",patched)
        self.assertIn(b"(uint16_t) value << 8",patched)
        self.assertIn(b'p003_physical_outb(port, val, p->priv, p->outb)',
                      patched)
        self.assertIn(
            b'p003_physical_outw(port + i, val >> (i << 3), p->priv, p->outw)',
            patched)
        self.assertIn(
            b'p003_physical_inw(port + 2, p->priv, p->inw)', patched)
        # Never record only the guest OUTW/INW at the END of the routine.
        self.assertNotIn(b"p003_cpu_io_note('W', port, val);", patched)
        self.assertNotIn(b"p003_cpu_io_note('R', port, ret);", patched)
    def test_wrong_fork_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "hash differs"):
            instrument(ORIGINAL_SHAPED_SYNTHETIC)
    def test_changed_source_fails_closed(self):
        original = ORIGINAL_SHAPED_SYNTHETIC + b"\n// mutation"
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
    def test_missing_guest_outl_split_refused(self):
        changed = ORIGINAL_SHAPED_SYNTHETIC.replace(
            b'p->outw(port + i, val >> (i << 3), p->priv)',
            b'/* missing outl word dispatch */')
        with self.assertRaisesRegex(ValueError,"Unexpected original callback count"):
            instrument(changed,None)
    def test_missing_byte_outb_probe_refused(self):
        altered=ORIGINAL_SHAPED_SYNTHETIC.replace(
            b'p->outb(port, val, p->priv)',
            b'/* hardware word dispatch absent */')
        with self.assertRaisesRegex(ValueError,"Unexpected original callback count"):
            instrument(altered,None)
    def test_missing_byte_inb_probe_refused(self):
        altered=ORIGINAL_SHAPED_SYNTHETIC.replace(
            b'p->inb(port, p->priv)',
            b'/* physical input callback absent */')
        with self.assertRaisesRegex(ValueError,"Unexpected original callback count"):
            instrument(altered,None)
    def test_crlf_pinned_original_source_matches_git_hash(self):
        original=ORIGINAL_SHAPED_SYNTHETIC
        crlf=original.replace(b'\n',b'\r\n')
        self.assertEqual(git_blob_sha(original),git_blob_sha(crlf))
        self.assertEqual(instrument(original,git_blob_sha(original)),
                         instrument(crlf,git_blob_sha(original)))

if __name__ == "__main__":
    unittest.main()
