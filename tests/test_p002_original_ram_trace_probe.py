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
        self.assertEqual(result["historical_pcm16_upload_matches"],1)
        self.assertEqual(result["final_state_unoverwritten_pcm16_matches"],1)
        self.assertEqual(result["first_final_state_emu8000_word_address"],0x200400)
        self.assertEqual(result["selected_sample_overlapped_ram_words"],0)

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
            self.assertIn("4 0A20 0400",output)
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

    def test_native_python_port_parity_after_valid_source_sample(self):
        # A previously complete sample is NOT an excuse to silently
        # discard bad physical writes in a later original capture segment.
        # Prior scanner/exporter silently ignored unexpected A24/A22.
        tails=[
            # SMALW low-half interrupted by a fresh pointer selector.
            ["60 E22 0036","60 A20 0800","60 E22 0036",
             "60 A20 0900","60 A22 0020",
             "60 E22 003A","60 A20 1234"],
            # Malformed data port while the original pointer is selected.
            ["60 E22 0036","60 A24 1234",
             "60 A20 0800","60 A22 0020",
             "60 E22 003A","60 A20 1234"],
            # SMLD has ONLY the original A20 16-bit sample-data port.
            ["60 E22 003A","60 A22 1234"],
        ]
        for tail in tails:
            trace=self.trace+tail
            with self.subTest(tail=tail):
                with self.assertRaisesRegex(
                        ValueError,"SMALW|SMLD"):
                    probe.scan_capture(trace,self.expected)
                with self.assertRaisesRegex(
                        ValueError,"SMALW|SMLD|pointer truncated"):
                    list(probe.filter_original_ram_upload_port_writes(trace))
                with tempfile.TemporaryDirectory() as td:
                    root=Path(td)
                    source=root/"source.sf2"
                    capture=root/"bad.original.trace"
                    private_export=root/"must-not-exist.trace"
                    source.write_bytes(self.sf2)
                    capture.write_text("\n".join(trace)+"\n",encoding="ascii")
                    with self.assertRaisesRegex(ValueError,"SMALW|SMLD"):
                        probe.analyze(capture,source,0)
                    with self.assertRaisesRegex(ValueError,"SMALW|SMLD|pointer truncated"):
                        probe.write_private_upload_replay(
                            capture,private_export)
                    self.assertFalse(private_export.exists())

    def test_real_86box_win95_boot_register_bank_interleaving(self):
        # These 15 exact lines came from our real 86Box AWE32 calibrated
        # v6.0-AWE32-0.93 Windows95 boot TRACE. The FOLLOWING sample
        # waveform is synthetic-only: source ROM/SF2 parity is not proven.
        real_original_boot=[
            "680527 E22 0036","680527 E20 FF00",
            "680527 E22 0056","680527 E20 0000",
            "680530 E22 0036","680530 A20 0000",
            "680530 A22 8000","680530 E22 0037",
            "681585 E22 0036","681585 A20 0000",
            "681585 A22 0100","681585 E22 003A",
            "681585 A20 AA55","681585 E22 0034",
            "R 681585 A20 0002",
        ]
        synthetic_source=real_original_boot+[
            "681586 E22 0036","681586 E20 D000",
            "681586 A20 0400","681586 A22 0020",
            "681587 E22 003A",
        ]+[f"681588 A20 {word:04X}" for word in self.sample_words]
        report=probe.scan_capture(synthetic_source,self.expected)
        self.assertEqual(report["register_events"],len(synthetic_source)-1)
        self.assertEqual(report["original_register_reads"],1)
        self.assertEqual(report["ram_word_writes"],len(self.sample_words))
        self.assertEqual(report["direct_pcm16_upload_matches"],1)
        self.assertEqual(report["first_final_state_emu8000_word_address"],
                         0x200400)
        filtered=list(probe.filter_original_ram_upload_port_writes(
            synthetic_source))
        self.assertEqual(len(filtered),3+1+len(self.sample_words))
        self.assertEqual(filtered[:4],[
            (681586,0xE22,0x36),(681586,0xA20,0x0400),
            (681586,0xA22,0x0020),(681587,0xE22,0x3A)])
        self.assertTrue(all(p in (0xE22,0xA20,0xA22)
                            for _,p,_ in filtered))
        self.assertNotIn((681585,0xA20,0xAA55),filtered)
        # Invalid off-chip data that is NOT an observed original
        # AA55/55AA capacity probe must still fail closed.
        bad=[line.replace("681585 A20 AA55","681585 A20 1234")
             for line in synthetic_source]
        with self.assertRaisesRegex(ValueError,"outside installed"):
            probe.scan_capture(bad,self.expected)
        with self.assertRaisesRegex(ValueError,"outside installed"):
            list(probe.filter_original_ram_upload_port_writes(bad))
        # No new bank-port tolerance for unexpected A24 or A22 writes.
        for tail in (["681600 E22 0036","681600 A24 1234"],
                     ["681600 E22 003A","681600 A22 1234"],
                     ["681600 E22 0036","681600 A20 1000",
                      "681600 E22 003A","681600 A20 1234"]):
            with self.subTest(tail=tail):
                with self.assertRaisesRegex(ValueError,"SMALW|SMLD|pointer"):
                    probe.scan_capture(synthetic_source+tail,self.expected)
                with self.assertRaisesRegex(ValueError,"SMALW|SMLD|pointer"):
                    list(probe.filter_original_ram_upload_port_writes(
                        synthetic_source+tail))

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
            with self.assertRaisesRegex(ValueError,"EXACTLY ONE"):
                probe.analyze(p/"wrong.trace",p/"sample.sf2",0)

    def test_duplicate_captured_physical_sample_address_fails_closed(self):
        # Repeating an identical sample at another RAM location is an
        # AMBIGUOUS original allocation, not a source-approved
        # address. Also exercise two copies in a single contiguous
        # SMALW/SMLD run: both occurrences must be counted.
        second=[
            "45 E22 0036","45 A20 0800","45 A22 0020",
            "46 E22 003A",
        ]
        second.extend(
            f"{46+i} A20 {word:04X}"
            for i,word in enumerate(self.sample_words))
        twice=self.trace+second
        result=probe.scan_capture(twice,self.expected)
        self.assertEqual(result["direct_pcm16_upload_matches"],2)
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)
            (p/"bank.sf2").write_bytes(self.sf2)
            (p/"double.trace").write_text("\n".join(twice)+"\n",
                                          encoding="ascii")
            with self.assertRaisesRegex(ValueError,"EXACTLY ONE"):
                probe.analyze(p/"double.trace",p/"bank.sf2",0)

        contiguous=[
            "0 E22 0036","0 A20 0010","0 A22 0020",
            "0 E22 003A"
        ]
        data=self.sample_words+self.sample_words
        contiguous.extend(
            f"{1+i} A20 {word:04X}"
            for i,word in enumerate(data))
        result=probe.scan_capture(contiguous,self.expected)
        self.assertEqual(result["direct_pcm16_upload_matches"],2)

    def test_codex_repro_1_historical_pcm_overwritten_not_final_ram_state(self):
        # First real-looking source sample matched in full. The
        # original hardware then overwrites the FIRST physical word
        # after a new SMALW address: historical == 1; final == 0.
        # The same byte value must not excuse duplicate physical
        # writes: the original native preflight is deliberately strict.
        for last_value in (0xDEAD, self.sample_words[0]):
            later=[
                "48 E22 0036", "48 A20 0400", "48 A22 0020",
                "48 E22 003A", f"48 A20 {last_value:04X}",
            ]
            trace=self.trace+later
            scan=probe.scan_capture(trace,self.expected)
            self.assertEqual(scan["direct_pcm16_upload_matches"],1)
            self.assertEqual(scan["historical_pcm16_upload_matches"],1)
            self.assertEqual(scan["first_match_emu8000_word_address"],
                             0x200400)
            self.assertEqual(scan["final_state_unoverwritten_pcm16_matches"],0)
            self.assertEqual(scan["first_final_state_emu8000_word_address"],None)
            self.assertEqual(scan["selected_sample_overlapped_ram_words"],1)
            self.assertEqual(scan["physical_ram_word_rewrite_events"],1)
            with tempfile.TemporaryDirectory() as td:
                p=Path(td)
                bank=p/"same-private-bank.sf2"
                tracefile=p/"overwritten-original-shaped.trace"
                bank.write_bytes(self.sf2)
                tracefile.write_text("\n".join(trace)+"\n",encoding="ascii")
                with self.assertRaisesRegex(ValueError,"overwritten|overlapped"):
                    probe.analyze(tracefile,bank,0)

    def test_codex_repro_2_dangling_smalw_eof_rejected_by_scan_and_export(self):
        # A complete prior soundbank sample is NOT enough to accept a
        # truncated new SMALW write at EOF. Both scanner and literal
        # exporter must require a COMPLETE hardware trace.
        # A bare 0x36 selector is NOT an attempted RAM pointer:
        # the original fork regularly selects it for unrelated E20/
        # 620/622 data-bank accesses. Only a partial A20/A22 pointer
        # actually in progress is a truncated hardware RAM address.
        bare=self.trace+["49 E22 0036"]
        self.assertEqual(
            probe.scan_capture(bare,self.expected)[
                "final_state_unoverwritten_pcm16_matches"],1)
        self.assertEqual(len(list(probe.filter_original_ram_upload_port_writes(
            bare))),len(list(probe.filter_original_ram_upload_port_writes(
            self.trace))))
        for tail in (["49 E22 0036","49 A20 1234"],
                     ["49 E22 0036","49 A22 0020"],
                     ["49 E22 0036","49 A20 1234",
                      "49 E22 003A"]):
            trace=self.trace+tail
            with self.assertRaisesRegex(ValueError,"SMALW|SMLD|partial address"):
                probe.scan_capture(trace,self.expected)
            with self.assertRaisesRegex(ValueError,"SMALW|SMLD|partial address"):
                list(probe.filter_original_ram_upload_port_writes(trace))
            with tempfile.TemporaryDirectory() as td:
                p=Path(td)
                bank=p/"sample.sf2"
                tracefile=p/"partial.trace"
                bank.write_bytes(self.sf2)
                tracefile.write_text("\n".join(trace)+"\n",encoding="ascii")
                with self.assertRaisesRegex(ValueError,"SMALW|SMLD|partial address"):
                    probe.analyze(tracefile,bank,0)

    def test_codex_repro_3_odd_byte_match_must_not_skip_aligned_offset_two(self):
        # Historical bug: b'\x11' * 32 has an initial match
        # at ODD byte offset 1 and real valid WORD-aligned match at 2.
        # find(expected, at+2) incorrectly advanced 1 -> 3 and
        # missed physical word address 0x200401.
        expected=b"\x11"*32
        raw=b"\x00"+b"\x11"*34+b"\x00"
        self.assertEqual(len(raw),36)
        self.assertEqual([i for i in range(0,len(raw)-len(expected)+1,2)
                          if raw[i:i+len(expected)]==expected],[2])
        words=struct.unpack("<18H",raw)
        trace=[
            "0 E22 0036","0 A20 0400","0 A22 0020",
            "0 E22 003A",
        ]+[f"1 A20 {word:04X}" for word in words]
        scan=probe.scan_capture(trace,expected)
        self.assertEqual(scan["historical_pcm16_upload_matches"],1)
        self.assertEqual(scan["final_state_unoverwritten_pcm16_matches"],1)
        self.assertEqual(scan["first_final_state_emu8000_word_address"],
                         0x200401)
        # A SECOND valid word-aligned copy stays ambiguous (NO-GO).
        additional=[
            "2 E22 0036","2 A20 0600","2 A22 0020",
            "2 E22 003A",
        ]+[f"2 A20 {word:04X}" for word in struct.unpack(
            "<16H",expected)]
        duplicate=probe.scan_capture(trace+additional,expected)
        self.assertEqual(duplicate["historical_pcm16_upload_matches"],2)
        self.assertEqual(duplicate["final_state_unoverwritten_pcm16_matches"],0)

    def test_research_historical_sample_overwrite_cannot_certify_final_ram(self):
        # Exactly the independent Codex counterexample: same physical
        # word overwritten AFTER successful full original sample upload.
        # Historical occurrence remains one, final unoverwritten
        # placement MUST be NO-GO even if overwritten with same value.
        for replacement in (0xDEAD, self.sample_words[0]):
            trace=self.trace+[
                "45 E22 0036","45 A20 0400","45 A22 0020",
                "46 E22 003A",f"47 A20 {replacement:04X}",
            ]
            evidence=probe.scan_capture(trace,self.expected)
            self.assertEqual(evidence["historical_pcm16_upload_matches"],1)
            self.assertEqual(evidence["direct_pcm16_upload_matches"],1)
            self.assertEqual(evidence["final_state_unoverwritten_pcm16_matches"],0)
            self.assertEqual(evidence["selected_sample_overlapped_ram_words"],1)
            self.assertEqual(evidence["physical_ram_word_rewrite_events"],1)
            self.assertIsNone(evidence["first_final_state_emu8000_word_address"])
            with tempfile.TemporaryDirectory() as td:
                p=Path(td)
                (p/"source.sf2").write_bytes(self.sf2)
                (p/"overwrite.trace").write_text(
                    "\n".join(trace)+"\n",encoding="ascii")
                with self.assertRaisesRegex(ValueError,"overwritten or overlapped"):
                    probe.analyze(p/"overwrite.trace",p/"source.sf2",0)

    def test_research_trailing_incomplete_smalw_rejected_by_both_scanners(self):
        # Original Codex case 2. A historical full match does not
        # excuse a dangling pointer low half at capture EOF.
        partial=self.trace+["50 E22 0036","50 A20 1234"]
        with self.assertRaisesRegex(ValueError,"incomplete original SMALW"):
            probe.scan_capture(partial,self.expected)
        with self.assertRaisesRegex(ValueError,"trailing partial address"):
            list(probe.filter_original_ram_upload_port_writes(partial))
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)
            (p/"source.sf2").write_bytes(self.sf2)
            (p/"partial.trace").write_text(
                "\n".join(partial)+"\n",encoding="ascii")
            with self.assertRaisesRegex(ValueError,"incomplete original SMALW"):
                probe.analyze(p/"partial.trace",p/"source.sf2",0)

    def test_research_odd_byte_hit_does_not_skip_aligned_second_byte(self):
        # Original Codex 32-byte repeat counterexample: byte matches
        # at offsets 1,2,3, ONLY offset2 is aligned to 16-bit words.
        expected=b"\x11"*32
        raw=b"\0"+b"\x11"*34+b"\0"
        words=struct.unpack("<18H",raw)
        trace=[
            "0 E22 0036","0 A20 0400","0 A22 0020","0 E22 003A"
        ]+[f"{1+i} A20 {word:04X}" for i,word in enumerate(words)]
        evidence=probe.scan_capture(trace,expected)
        self.assertEqual(evidence["direct_pcm16_upload_matches"],1)
        self.assertEqual(evidence["first_match_emu8000_word_address"],0x200401)
        self.assertEqual(evidence["final_state_unoverwritten_pcm16_matches"],1)
        self.assertEqual(evidence["first_final_state_emu8000_word_address"],
                         0x200401)

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
        with self.assertRaisesRegex(ValueError,"SMALW pointer"):
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
