"""Fast V5 source compatibility checks for a Windows 86Box build."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from instrument_p003_v5_voice_rows import instrument_v5

SRC = r'''#include <86box/mem.h>
static FILE *p003_cpu_io_file;
static int p003_guest_callstack_enabled;
static unsigned long long p003_cpu_io_seq;
static void p003_guest_stack_returns(uint32_t *retn) { (void)retn; }
static void p003_cpu_io_close(void) {}
/* P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK */
void probe(void) {
    uintptr_t lookup;
    lookup=readlookup2[linear >> 12];
    p003_cpu_io_note('R', port, value);
    p003_guest_stack_returns(retn);
                    (unsigned) ECX, (unsigned) EDX);
}
'''

class TestV5Smoke(unittest.TestCase):
    def test_preserves_existing_port_events(self):
        patched = instrument_v5(SRC.encode()).decode()
        self.assertIn('P003_V5_ROW_STATE', patched)
        self.assertIn('p003_v5_guest_row(direction,port,retn[0]',patched)
        self.assertEqual(patched.count("p003_cpu_io_note('R', port, value)"),1)
        self.assertNotIn('writemem',patched)

    def test_duplicate_instrumentation_refused(self):
        once=instrument_v5(SRC.encode())
        with self.assertRaises(ValueError):instrument_v5(once)

    def test_changed_v4_header_refused(self):
        with self.assertRaises(ValueError):
            instrument_v5(SRC.replace('V4_STACK','V3_STACK').encode())

if __name__=='__main__': unittest.main()
