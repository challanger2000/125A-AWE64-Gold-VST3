"""Public synthetic-only RAM guest trace and sample matching contract.

The raw original Win95/Creative trace & SF2 owner assets are PRIVATE.
No original RAM placement or sample bytes are embedded here.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools import p002_original_ram_trace_probe as probe

def chunk(name: bytes, value: bytes) -> bytes:
    assert len(name)==4
    return name+struct.pack("<I",len(value))+value+(b"\0" if len(value)&1 else b"")

def sf2_bank(*, ram_type=1, wrong_loop=False):
    words=[((i*3431+17) ^ (i*61)) & 0xffff for i in range(64)]
    pcm=struct.pack("<64H",*words)
    shdr=(b"RAM-SAMPLE".ljust(20,b"\0")+
          struct.pack("<5I",4,44,8,45 if wrong_loop else 40,44100)+
          struct.pack("<BbHH",60,-7,0,ram_type))
    assert len(shdr)==46
    eos=b"EOS".ljust(20,b"\0")+bytes(26)
    assert len(eos)==46
    info=chunk(b"LIST",b"INFO"+chunk(b"ifil",struct.pack("<HH",2,0)))
    sdta=chunk(b"LIST",b"sdta"+chunk(b"smpl",pcm))
    pdta=chunk(b"LIST",b"pdta"+chunk(b"shdr",shdr+eos))
    body=b"sfbk"+info+sdta+pdta
    result=b"RIFF"+struct.pack("<I",len(body))+body
    return result,words[4:44]

def captured(words,addr=0x200400):
    lines=[
        "0 E22 0001", "0 A20 00FE",  # ordinary original note register
        "R 1 E20 0000",                 # harmless original read
        "2 E22 0036", "2 A20 0100", "2 A22 0020",
        "3 E22 003A", "3 A20 0011",     # OTHER original soundbank word
        "4 E22 0036",
        f"4 A20 {addr&65535:04X}",
        f"4 A22 {addr>>16:04X}",
        "5 E22 003A",
    ]
    for i,w in enumerate(words):
        # Interleave an original different-register operation without
        # changing the original SMALW/SMLD pointer state.
        if i==11:
            lines.extend([f"{5+i} E22 00A1",
                          f"{5+i} A20 00FE",
                          f"{5+i} E22 003A"])
        lines.append(f"{5+i} A20 {w:04X}")
    return lines

class OriginalRamTraceProbeTests(unittest.TestCase):
    def setUp(self):
        self.sf2,self.sample_words=sf2_bank()
        self.trace=captured(self.sample_words)
        self.expected=struct.pack("<40H",*self.sample_words)

    def test_source_sample_bytes_and_recover_trace_address(self):
        sample=probe.extract_sf2_mono_sample(self.sf2,0)
        self.assertEqual(sample["data"],self.expected)
        self.assertEqual(sample["source_loop_start"],8)
        self.assertEqual(sample["source_loop_end"],40)
        self.assertEqual(sample["correction"],-7)
        result=probe.scan_capture(self.trace,self.expected)
        self.assertEqual(result["direct_pcm16_upload_matches"],1)
        self.assertEqual(result["first_match_emu8000_word_address"],
                         0x200400)
        self.assertEqual(result["ram_word_writes"],41)
        self.assertEqual(result["original_register_reads"],1)

    def test_independent_file_hash_provenance_is_not_asserted(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td);bank=p/"synthetic.sf2";trace=p/"captured.trace"
            bank.write_bytes(self.sf2)
            trace.write_text("\n".join(self.trace)+"\n",encoding="ascii")
            report=probe.analyze(trace,bank,0)
            self.assertEqual(report["status"],
                             "ORIGINAL_CAPTURE_DIRECT_RAM_PCM16_MATCH_CANDIDATE")
            self.assertEqual(report["first_matched_captured_emu8000_word_address"],
                             0x200400)
            self.assertEqual(report["source_sf2_sha256"],
                             hashlib.sha256(self.sf2).hexdigest())
            self.assertFalse(report["original_driver_native_RAM_allocator_reimplemented"])
            self.assertFalse(report["production_ram_synth_approved"])
            self.assertEqual(report["source_guest_win95_SBAWE_VXD_provenance"],
                             "MUST_BE_VERIFIED_SEPARATELY")

    def test_literal_guest_RAM_port_export_without_synthesis(self):
        filtered=list(probe.filter_original_ram_upload_port_writes(
            self.trace))
        # 2 original SMALW setters (3 writes each), 3 captured
        # SMLD selectors (including original re-selection after an
        # unrelated register), and 41 exact source PCM words.
        self.assertEqual(len(filtered),6+3+41)
        self.assertEqual(filtered[:5],[
            (2,0xE22,0x36),(2,0xA20,0x0100),
            (2,0xA22,0x0020),(3,0xE22,0x3A),
            (3,0xA20,0x0011)])
        self.assertEqual(filtered[5:9],[
            (4,0xE22,0x36),(4,0xA20,0x0400),
            (4,0xA22,0x0020),(5,0xE22,0x3A)])
        self.assertEqual(filtered[-1][1:],(0xA20,self.sample_words[-1]))
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            source=root/"source-original-guest.trace"
            result=root/"private-upload-filtered.trace"
            source.write_text("\n".join(self.trace)+"\n",encoding="ascii")
            written=probe.write_private_upload_replay(source,result)
            self.assertEqual(written,len(filtered))
            output=result.read_text(encoding="ascii")
            self.assertIn("PROVENANCE REQUIRES INDEPENDENT",output)
            self.assertIn("4 A20 0400",output)
            self.assertNotIn("0 E22 0001",output)
            self.assertNotIn("R 1 E20 0000",output)
            with self.assertRaisesRegex(ValueError,"overwrite"):
                probe.write_private_upload_replay(source,result)

    def test_guest_port_export_refuses_incomplete_or_outside_DRAM(self):
        truncated=[x for x in self.trace if x!="4 A22 0020"]
        with self.assertRaisesRegex(ValueError,"SMALW pointer"):
            list(probe.filter_original_ram_upload_port_writes(truncated))
        illegal=list(self.trace)
        illegal[10]="4 A22 0001"
        with self.assertRaisesRegex(ValueError,"outside installed"):
            list(probe.filter_original_ram_upload_port_writes(illegal))
        with self.assertRaisesRegex(ValueError,"no complete"):
            list(probe.filter_original_ram_upload_port_writes(
                ["0 E22 0001","0 A20 00FE"]))

    def test_pcm_bitflip_and_wrong_endianness_rejected(self):
        mutant=list(self.trace)
        mutant[16]=(mutant[16][:-4]+
                    f"{int(mutant[16][-4:],16)^1:04X}")
        result=probe.scan_capture(mutant,self.expected)
        self.assertEqual(result["direct_pcm16_upload_matches"],0)
        wrong_order=bytearray(self.expected)
        wrong_order[0],wrong_order[1]=wrong_order[1],wrong_order[0]
        self.assertEqual(probe.scan_capture(self.trace,bytes(wrong_order))
                         ["direct_pcm16_upload_matches"],0)
        with tempfile.TemporaryDirectory() as td:
            p=Path(td);(p/"sample.sf2").write_bytes(self.sf2)
            (p/"wrong.trace").write_text("\n".join(mutant)+"\n")
            with self.assertRaisesRegex(ValueError,"does not contain"):
                probe.analyze(p/"wrong.trace",p/"sample.sf2",0)

    def test_pointer_provenance_and_range_controls(self):
        bad=list(self.trace)
        bad[10]="4 A22 0001"  # below installed RAM base
        with self.assertRaisesRegex(ValueError,"outside installed"):
            probe.scan_capture(bad,self.expected)
        bad=list(self.trace);bad[9]="4 A20 FFE0"
        bad[10]="4 A22 00FF"
        with self.assertRaisesRegex(ValueError,"outside installed"):
            probe.scan_capture(bad,self.expected)
        bad=[e for e in self.trace if e!="4 A22 0020"]
        with self.assertRaisesRegex(ValueError,"missing safe"):
            probe.scan_capture(bad,self.expected)
        bad=list(self.trace);bad[11]="5 E22 003B"
        self.assertEqual(probe.scan_capture(bad,self.expected)
                         ["direct_pcm16_upload_matches"],0)

    def test_invalid_source_bank_and_trace_no_go(self):
        with self.assertRaisesRegex(ValueError,"EOS"):
            probe.extract_sf2_mono_sample(self.sf2,1)
        with self.assertRaisesRegex(ValueError,"RAM mono"):
            probe.extract_sf2_mono_sample(sf2_bank(ram_type=0x8001)[0],0)
        with self.assertRaisesRegex(ValueError,"RAM mono"):
            probe.extract_sf2_mono_sample(sf2_bank(wrong_loop=True)[0],0)
        with self.assertRaisesRegex(ValueError,"RIFF length"):
            probe.extract_sf2_mono_sample(self.sf2[:-1],0)
        bad=list(self.trace)
        bad[-1]="0 A20 1111"
        with self.assertRaisesRegex(ValueError,"nonmonotonic"):
            probe.scan_capture(bad,self.expected)
        bad=list(self.trace);bad[-1]="123 ZZZZ 1111"
        with self.assertRaisesRegex(ValueError,"invalid original"):
            probe.scan_capture(bad,self.expected)
        with self.assertRaisesRegex(ValueError,"no EMU8000"):
            probe.scan_capture(["0 E22 0001","0 A20 9999"],self.expected)

if __name__=="__main__":
    unittest.main()
