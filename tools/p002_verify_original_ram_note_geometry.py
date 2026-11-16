#!/usr/bin/env python3
"""P002 read-only PROOF of the original 1996 Creative RAM-SF2 MIDI notes.

Requires three independently retained owner-PRIVATE source inputs:
original Win95/P002RAM.LOG, *original* 86Box EMU8K_TRACE, original SF2.
Unlike the prior wrong-CC1/ROM-only capture, proves *all* CC(C)A/PSST/CSL
sample addresses for two exact original WinMM C4 and AWEManager D4
NoteOn groups lie INSIDE the measured original SoundFont DRAM window.

This does NOT prove native VST3 RAM voice programming/PCM parity.
No copyrighted guest, driver, SF2, WAV or raw trace enters public CI.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

try:
    from .p002_verify_original_win95_ram_guest_log import audit
    from .p002_original_ram_trace_probe import RAM_FIRST, RAM_END_EXCLUSIVE
    from .p002_original_bank_transform_evidence import original_bank_pcm16, chunks
except ImportError:
    from p002_verify_original_win95_ram_guest_log import audit
    from p002_original_ram_trace_probe import RAM_FIRST, RAM_END_EXCLUSIVE
    from p002_original_bank_transform_evidence import original_bank_pcm16, chunks

LINE = re.compile(r"^(R )?([0-9]+) ([0-9a-fA-F]{1,4}) ([0-9a-fA-F]{1,4})$")
SAMPLES_IN_OWNER_BANK = 260921
ORIGINAL_PREFIX_WORDS = 16
EXPECTED_VOICES = ((0, 3), (6, 9))
MAX_TRACE_BYTES = 1_200_000_000
MAX_TRACE_LINES = 32_000_000


def sha_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def captured_voice_registers(lines, first_frame: int, second_frame: int):
    if not (0 <= first_frame < second_frame < 44100*4*3600
            and second_frame-first_frame > 10000):
        raise ValueError("C4/D4 independent original chip timing invalid")
    selected = None
    write = None
    event_count = 0
    previous = -1
    collected = []
    relevant = {v for pair in EXPECTED_VOICES for v in pair}
    def flush():
        if write is not None and "low" in write and "high" in write:
            full = write["low"] | write["high"] << 16
            collected.append(
                (write["frame"], write["voice"], write["register"], full))

    for n, raw in enumerate(lines, 1):
        if n > MAX_TRACE_LINES:
            raise ValueError("Original 86Box source trace exceeds bound")
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = LINE.fullmatch(line)
        if m is None:
            raise ValueError(f"Invalid original source 86Box event at {n}")
        rd, frame, port, value = m.groups()
        frame = int(frame)
        port, value = int(port,16), int(value,16)
        if frame < previous or frame > 44100*4*3600:
            raise ValueError("Nonmonotonic original EMU8000 chip clock")
        previous = frame
        event_count += 1
        if rd:
            continue
        if port == 0xE22:
            flush()
            selected = value
            voice = selected & 31
            register = selected >> 5
            if (voice in relevant and register in (0,6,7)
                    and first_frame-16 <= frame <= second_frame+16):
                write = {"frame":frame,"voice":voice,"register":register}
            else:
                write = None
        elif write is not None:
            if write["register"] == 0 and port == 0xA20:
                write["low"] = value
            elif write["register"] == 0 and port == 0xA22:
                write["high"] = value
            elif write["register"] in (6,7) and port == 0x620:
                write["low"] = value
            elif write["register"] in (6,7) and port == 0x622:
                write["high"] = value
    flush()
    return collected, event_count


def inspect_note_geometry(lines, c4_frame: int, d4_frame: int,
                          source_bank_first_word: int):
    if not (RAM_FIRST <= source_bank_first_word <
            RAM_END_EXCLUSIVE-SAMPLES_IN_OWNER_BANK):
        raise ValueError("Independent original SF2 RAM base outside chip")
    records, total = captured_voice_registers(lines,c4_frame,d4_frame)
    bank_end=source_bank_first_word+SAMPLES_IN_OWNER_BANK
    details={}
    for label, start, voices in (
            ("winmm_C4",c4_frame,EXPECTED_VOICES[0]),
            ("creative_AWE_SEND_MIDI_D4",d4_frame,EXPECTED_VOICES[1])):
        per_voice={}
        for voice in voices:
            by_register={}
            for frame, physical_voice, register, full in records:
                if physical_voice==voice and start <= frame <= start+2:
                    by_register.setdefault(register,[]).append(full)
            missing=[reg for reg in (0,6,7) if reg not in by_register]
            if missing:
                raise ValueError(
                    f"{label} original voice {voice} missing source CCCA/PSST/CSL {missing}")
            def address(reg):
                return by_register[reg][-1]&0xffffff
            ccca,psst,csl=address(0),address(6),address(7)
            # AWe32 CCCA upper byte contains playback control flags,
            # not part of the 24-bit ROM/RAM sample physical address.
            if not (RAM_FIRST <= ccca < RAM_END_EXCLUSIVE and
                    RAM_FIRST <= psst < RAM_END_EXCLUSIVE and
                    RAM_FIRST <= csl < RAM_END_EXCLUSIVE):
                raise ValueError(
                    f"{label} original voice {voice} played ROM, not RAM")
            if not (source_bank_first_word <= ccca < bank_end and
                    source_bank_first_word <= psst < bank_end and
                    source_bank_first_word <= csl < bank_end and
                    ccca < psst < csl):
                raise ValueError(
                    f"{label} original voice {voice} RAM addresses not inside "
                    "the exact original loaded SoundFont bank")
            per_voice[str(voice)]={
                "actual_CCCA_source_register_values_hex":
                    [f"0x{x:08x}" for x in by_register[0]],
                "physical_CCCA_hex":f"0x{ccca:06x}",
                "physical_PSST_hex":f"0x{psst:06x}",
                "physical_CSL_hex":f"0x{csl:06x}",
                "inside_original_Creative_RAM_bank":True,
                "source_noteon_frame":start,
            }
        details[label]=per_voice
    return {
        "status":"ORIGINAL_CREATIVE_C4_D4_VERIFIED_REAL_RAM_NOT_ROM",
        "original_chip_register_event_count":total,
        "original_bank_first_word_hex":f"0x{source_bank_first_word:x}",
        "original_bank_words":SAMPLES_IN_OWNER_BANK,
        "original_winmm_C4_noteon_frame":c4_frame,
        "original_creative_D4_noteon_frame":d4_frame,
        "original_ram_voice_groups":details,
        "full_original_CCCA_PSST_CSL_records_for_all_four_voices":True,
        "original_vst3_RAM_NOTEON_PARITY":"NOT_TESTED",
        "original_vst3_DRY_PCM_PARITY":"NOT_TESTED",
        "production_approved":False,
    }



def verify_original_ram_sample_binding(sf2: bytes, proof: dict,
                                      physical_base: int) -> dict:
    """Bind every captured RAM voice to one ORIGINAL SF2 shdr sample.

    The real Win95 Creative VXD NoteOn source shows initial CCCA=start-4
    and looping PSST/CSL=loopStart-1/loopEnd-1; those source rules were
    independently recovered at LCOD+0x5B81/+0x5F28. This check only
    establishes sample geometry for the single captured source bank,
    NOT a universal RAM sample loader, piano timbre, or dry PCM parity.
    """
    words, count = original_bank_pcm16(sf2)
    if len(words) != SAMPLES_IN_OWNER_BANK or count != 83:
        raise ValueError("Source bank sample count/PCM geometry not original")
    roots = list(chunks(sf2, 12, len(sf2)))
    pdta = [(at, size) for tag, at, size in roots
            if tag == b"LIST" and size >= 4 and sf2[at:at+4] == b"pdta"]
    if len(pdta) != 1:
        raise ValueError("Original source pdta not unique")
    at, size = pdta[0]
    tables = [(pos, n) for tag, pos, n in
              chunks(sf2, at+4, at+size) if tag == b"shdr"]
    if len(tables) != 1 or tables[0][1] != (count+1)*46:
        raise ValueError("Original source shdr sample count invalid")
    shdr, _ = tables[0]
    headers = []
    for sample_id in range(count):
        header = shdr + sample_id*46
        start, end, loop_start, loop_end = struct.unpack_from(
            "<IIII", sf2, header+20)
        # SoundFont shdr may legitimately start at sample word ZERO
        # (the owner's genuine original sample 0 does). The historical
        # -4/-1 offsets are applied AFTER the verified physical RAM
        # allocation base, not to the relative shdr index itself.
        if (not 0 <= start < end <= len(words) or
                not start <= loop_start < loop_end <= min(len(words),end+2) or
                physical_base+start < RAM_FIRST+4 or
                physical_base+loop_start < RAM_FIRST+1):
            raise ValueError("Original source sample address bounds invalid")
        headers.append((start,end,loop_start,loop_end))
    result = {}
    all_identifiers = []
    for label in ("winmm_C4", "creative_AWE_SEND_MIDI_D4"):
        group = proof["original_ram_voice_groups"][label]
        for voice in EXPECTED_VOICES[0 if label == "winmm_C4" else 1]:
            observed = group[str(voice)]
            ccca = int(observed["physical_CCCA_hex"],16)
            psst = int(observed["physical_PSST_hex"],16)
            csl = int(observed["physical_CSL_hex"],16)
            matches = [sample_id for sample_id,(start,end,loop_start,loop_end)
                       in enumerate(headers)
                       if physical_base+start-4 == ccca and
                          physical_base+loop_start-1 == psst and
                          physical_base+loop_end-1 == csl]
            if len(matches) != 1:
                raise ValueError(
                    f"{label} physical voice {voice} lacks UNIQUE original "
                    "SF2 shdr start/loop sample binding")
            sample_id = matches[0]
            result[f"{label}_voice_{voice}"] = sample_id
            all_identifiers.append(sample_id)
    # The genuine source was played twice through two independent MIDI
    # APIs, retaining two layers of the same instrument sample on each.
    # Changing any one sample address or header cannot mint this fact.
    if len(set(all_identifiers)) != 1:
        raise ValueError("Original C4/D4 source routes disagree on sample ID")
    return {
        "source_sample_header_count":count,
        "original_ram_midi_voice_sample_ids":result,
        "shared_original_sf2_shdr_sample_id":all_identifiers[0],
        "initial_CCCA_start_word_offset":-4,
        "initial_PSST_loop_start_word_offset":-1,
        "initial_CSL_loop_end_word_offset":-1,
        "all_four_original_voice_sample_bindings_exact":True,
        "inferred_other_bank_address_rules":False,
        "original_ram_dry_pcm_parity_verified":False,
    }


def analyze(original_trace: Path, guest_log: Path, original_sf2: Path,
            c4_frame: int, d4_frame: int, source_bank_first_word: int):
    for p,minimum,maximum in (
        (original_trace,100,MAX_TRACE_BYTES),
        (guest_log,100,128*1024),
        (original_sf2,96,32*1024*1024)):
        if not p.is_file() or not minimum<=p.stat().st_size<=maximum:
            raise ValueError("Private original Creative 86Box/Win95/SF2 source absent")
    source_bytes=original_sf2.read_bytes()
    guest=audit(guest_log.read_bytes(),source_bytes)
    if (guest["original_user_bank_midi_cc0_controller"]!=0 or
            guest["original_user_bank_midi_cc0_value"]!=1):
        raise ValueError("Actual Creative Win95 user bank CC0=1 not authorized")
    if guest["original_driver_ram_allocated_words"]!=(
            SAMPLES_IN_OWNER_BANK+ORIGINAL_PREFIX_WORDS):
        raise ValueError("Original Win95 Creative driver RAM allocation not matched")
    with original_trace.open("rt",encoding="ascii",errors="strict") as src:
        proof=inspect_note_geometry(
            src,c4_frame,d4_frame,source_bank_first_word)
    # Actual source sample ID is now independently tied to physical
    # CCCA/PSST/CSL positions, not merely "some address inside RAM".
    proof.update(verify_original_ram_sample_binding(
        source_bytes,proof,source_bank_first_word))
    proof.update({
        "original_trace_sha256":sha_file(original_trace),
        "original_guest_log_sha256":sha_file(guest_log),
        "original_private_bank_sha256":sha_file(original_sf2),
        "original_Win95_Creative_API_load_and_MIDI_CC0_1_audited":True,
    })
    return proof


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-emu8k-trace",type=Path,required=True)
    parser.add_argument("--original-guest-log",type=Path,required=True)
    parser.add_argument("--original-sf2",type=Path,required=True)
    parser.add_argument("--original-winmm-c4-chip-frame",type=int,required=True)
    parser.add_argument("--original-creative-d4-chip-frame",type=int,required=True)
    parser.add_argument("--observed-bank-base",type=lambda x:int(x,0),required=True)
    parser.add_argument("--private-report",type=Path,required=True)
    a=parser.parse_args()
    dst=a.private_report.resolve()
    repo=Path(__file__).resolve().parents[1]
    if (dst.exists() or dst==repo or repo in dst.parents or
            dst in (a.original_emu8k_trace.resolve(),
                    a.original_guest_log.resolve(),a.original_sf2.resolve())):
        parser.error("Private report must be a NEW file outside Git/inputs")
    proof=analyze(a.original_emu8k_trace,a.original_guest_log,
                  a.original_sf2,a.original_winmm_c4_chip_frame,
                  a.original_creative_d4_chip_frame,a.observed_bank_base)
    dst.parent.mkdir(parents=True,exist_ok=True)
    with dst.open("x",encoding="utf-8") as f:
        json.dump(proof,f,indent=2,sort_keys=True);f.write("\n")
    print(proof["status"],"original_NoteOn_CCCA_PSST_CSL_in_RAM=4_of_4",
          "native_VST3_voice_PCM_parity=NOT_TESTED")
    return 0


if __name__=="__main__":
    try:sys.exit(main())
    except (OSError,ValueError) as exc:
        print("P002_ORIGINAL_RAM_C4_D4_GEOMETRY_NO_GO",exc,file=sys.stderr)
        sys.exit(1)
