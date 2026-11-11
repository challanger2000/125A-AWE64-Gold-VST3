#!/usr/bin/env python3
"""Verify truly independent original Win95/Creative/86Box musical PCM, privately.

Requires guest WinMM evidence, the same-time original fork register trace and
digital WAV, 100k+ audible samples, exact native PCM and an audible bit-flip
negative control. No VM disks, original ROM or original WAV are uploaded.
"""
from __future__ import annotations
import argparse
import array
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import wave

ROM_SHA = "4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
PITCHES = (48, 60, 64, 67, 72)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for part in iter(lambda: f.read(1 << 20), b""):
            h.update(part)
    return h.hexdigest()


def guest_check(path):
    t = path.read_text(encoding="latin-1")
    for marker in ("P001 ORIGINAL CREATIVE WIN95 MIDI PROBE ENTRYPOINT",
                   "MIDI_DEVICE 0x00000000 name=SB AWE32 MIDI Synth [620]",
                   "ORIGINAL_CREATIVE_AWE_WINMM_SELECTED"):
        if marker not in t:
            raise ValueError("Original Creative guest not proven: " + marker)
    if "P001_GUEST_WINMM_MESSAGES_FAILED" in t:
        raise ValueError("Original driver signaled MIDI failure")
    if "P001_GUEST_WINMM_MESSAGES_SENT_OK" not in t:
        raise ValueError("Original Win95 MIDI probe lacks clean successful completion marker")
    for marker in ("RESET_ALL_CONTROLLERS", "PROGRAM_0_ROM_GM",
                   "CC7_VOLUME", "CC11_EXPRESSION"):
        if not re.search(r"^" + marker + r" value=0x[0-9A-F]{8} result=0x00000000$", t, re.M):
            raise ValueError("Original driver control event failed: " + marker)
    rows = re.findall(
        r"^(NOTE_ON|NOTE_OFF) value=0x([0-9A-F]{8}) result=0x([0-9A-F]{8})$", t, re.M
    )
    observed = []
    for typ, value, rc in rows:
        if rc != "00000000":
            raise ValueError("Original Creative WinMM error for " + typ)
        v = int(value, 16)
        observed.append((typ, v & 255, (v >> 8) & 255, (v >> 16) & 127))
    expected = [(typ, 0x90 if typ == "NOTE_ON" else 0x80, pitch)
                for pitch in PITCHES for typ in ("NOTE_ON", "NOTE_OFF")]
    if [(t, status, pitch) for t, status, pitch, vel in observed] != expected:
        raise ValueError("Not exactly five successful original WinMM note pairs")
    if any(vel == 0 for typ, _, _, vel in observed if typ == "NOTE_ON"):
        raise ValueError("Unexpected zero-velocity note on")
    return len(PITCHES)


def audio_check(path):
    active = []
    nonzero = peak = 0
    with wave.open(str(path), "rb") as wav:
        if (wav.getframerate(), wav.getnchannels(), wav.getsampwidth(),
                wav.getcomptype()) != (44100, 2, 2, "NONE"):
            raise ValueError("Original 86Box digital PCM not 44100 Hz stereo PCM16")
        frames = wav.getnframes()
        if frames < 441000:
            raise ValueError("Independent VM reference less than 10 seconds")
        for sec in range((frames + 44099) // 44100):
            samples = array.array("h")
            samples.frombytes(wav.readframes(44100))
            if sys.byteorder != "little":
                samples.byteswap()
            count = sum(x != 0 for x in samples)
            nonzero += count
            peak = max(peak, max((abs(x) for x in samples), default=0))
            if count > 10000:
                active.append(sec)
    if nonzero < 100000 or peak < 500:
        raise ValueError("Original VM reference effectively silent; no musical proof")
    if not any(all(i in active for i in range(first, first + 4)) for first in active):
        raise ValueError("No four consecutive seconds of real musical activity")
    return frames, nonzero, peak, active


def trace_check(path, frames, start, stop):
    writes = reads = musical_writes = 0
    previous = -1
    with path.open(encoding="ascii") as f:
        for row in f:
            row = row.strip()
            if not row or row.startswith("#"):
                continue
            fields = row.split()
            is_read = fields[0] == "R"
            if is_read:
                fields = fields[1:]
            if len(fields) != 3:
                raise ValueError("Malformed actual 86Box register trace")
            frame, port, value = int(fields[0]), int(fields[1], 16), int(fields[2], 16)
            if frame < previous or frame >= frames or not (0 <= port <= 65535) or not (0 <= value <= 65535):
                raise ValueError("Invalid VM register clock, port, or value")
            previous = frame
            if is_read:
                reads += 1
            else:
                writes += 1
                musical_writes += (start <= frame < stop)
    if writes < 100 or reads == 0 or musical_writes < 50:
        raise ValueError("No meaningful original Creative port activity during notes")
    return writes, reads, musical_writes


def run(exe, rom, trace, fork_wav, out):
    completed = subprocess.run(
        [str(exe), "--rom", str(rom), "--trace", str(trace),
         "--fork-wav", str(fork_wav), "--out", str(out)],
        capture_output=True, text=True, timeout=180
    )
    return completed.returncode, completed.stdout + "\n" + completed.stderr


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("rom", "trace", "fork-wav", "guest-log", "replay", "out"):
        p.add_argument("--" + key, required=True, type=Path)
    a = p.parse_args()
    rom, trace, fork_wav = a.rom.resolve(), a.trace.resolve(), a.fork_wav.resolve()
    log, exe, out = a.guest_log.resolve(), a.replay.resolve(), a.out.resolve()
    root = Path(__file__).resolve().parent.parent
    if out == root or root in out.parents:
        raise ValueError("Private original Creative capture MUST remain outside Git repository")
    if out.exists():
        raise ValueError("Refusing to overwrite original private evidence")
    if not rom.is_file() or rom.stat().st_size != 1048576 or digest(rom) != ROM_SHA:
        raise ValueError("Exact original owner ROM SHA256 not verified")
    pairs = guest_check(log)
    frames, nonzero, peak, active = audio_check(fork_wav)
    writes, reads, music_writes = trace_check(
        trace, frames, active[0] * 44100, min(frames, (active[-1] + 1) * 44100))
    out.mkdir(parents=True)
    native = out / "native.wav"
    code, msg = run(exe, rom, trace, fork_wav, native)
    m = re.search(r"IDENTICAL:\s*(\d+) stereo frames,\s*(\d+) individual samples,\s*(\d+) register events", msg)
    if code != 0 or not m or tuple(map(int, m.groups())) != (frames, frames * 2, writes + reads):
        raise ValueError("Real independent Creative PCM mismatch:\n" + msg)
    if digest(fork_wav) != digest(native):
        raise ValueError("Real independent Creative PCM WAV hashes differ")
    # Deliberate destructive negative check INSIDE a known loud note.
    with wave.open(str(fork_wav), "rb") as wav:
        wav.setpos(active[0] * 44100)
        samples = array.array("h")
        samples.frombytes(wav.readframes(44100))
    if sys.byteorder != "little":
        samples.byteswap()
    audible = [i for i, sample in enumerate(samples) if sample]
    if len(audible) < 10000:
        raise ValueError("Unable to find genuine sounding sample for corruption")
    index = audible[len(audible) // 2]
    bad_frame = active[0] * 44100 + index // 2
    channel = index % 2
    altered = out / "mutant-fork.wav"
    shutil.copyfile(fork_wav, altered)
    with altered.open("r+b") as f:
        f.seek(44 + bad_frame * 4 + 2 * channel)
        old = struct.unpack("<H", f.read(2))[0]
        f.seek(-2, 1)
        f.write(struct.pack("<H", old ^ 1))
    mutant_native = out / "mutant-native.wav"
    code, message = run(exe, rom, trace, altered, mutant_native)
    if code != 11 or f"frame {bad_frame} channel {channel}" not in message:
        raise ValueError("Audible one-bit negative mutation NOT located by replay: " + message)
    altered.unlink(missing_ok=True)
    mutant_native.unlink(missing_ok=True)
    report = {
        "gate": "INDEPENDENT 86BOX AWE32 ORIGINAL WIN95 CREATIVE MUSICAL PCM PASS",
        "original_driver": "SB AWE32 MIDI Synth [620]",
        "guest_note_on_off_pairs": pairs,
        "guest_completion_marker_observed": True,
        "guest_completion": "Original Win95 MIDI probe logged SENT_OK and returned to desktop; OS exit status not independently captured",
        "rom_sha256": ROM_SHA,
        "trace_sha256": digest(trace),
        "fork_wav_sha256": digest(fork_wav),
        "native_wav_sha256": digest(native),
        "frames": frames, "pcm16_samples_exact": frames * 2,
        "nonzero_pcm16_samples": nonzero, "peak_pcm16": peak,
        "musical_seconds": active, "port_writes": writes, "port_reads": reads,
        "port_writes_during_music": music_writes,
        "negative_frame": bad_frame, "negative_channel": channel,
        "negative_exit_code": code,
        "original_sbk_sf2_all_programs": "NOT TESTED",
        "physical_AWE64_gold_analog_line_out": "NOT TESTED",
        "Studio_One_ASIO_real_time_no_alloc": "NOT TESTED",
    }
    (out / "P001-independent-original-Creative-verdict.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("P001 INDEPENDENT ORIGINAL CREATIVE MIDI PCM PARITY PASS",
          f"exact_pcm16={frames*2}", f"nonzero={nonzero}",
          f"registers={writes+reads}", f"mutant_rejected_frame={bad_frame}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.TimeoutExpired) as e:
        print("P001 INDEPENDENT ORIGINAL CREATIVE MIDI PCM NO-GO:", e, file=sys.stderr)
        sys.exit(1)
