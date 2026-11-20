#!/usr/bin/env python3
"""Fail-closed P001 independent original-Creative/Win95/86Box PCM differential.

Requires privately captured Win95 WinMM log, 86Box EMU8K_TRACE + AWE32_WAV,
and a native *unchanged fork core* awe64_replay binary. No proprietary data
is shipped with or generated inside the public repository.
"""
from __future__ import annotations

import argparse
from array import array
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import wave

ROM_SHA = "4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
NOTES = [(48, 100), (60, 96), (64, 96), (67, 96), (72, 108)]


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def verify_guest_log(path: Path) -> dict:
    text = path.read_bytes().decode("latin-1")
    require("P001 ORIGINAL CREATIVE WIN95 MIDI PROBE ENTRYPOINT" in text,
            "No original Windows 95 guest PE32 entrypoint evidence")
    require("ORIGINAL_CREATIVE_AWE_WINMM_SELECTED" in text,
            "Guest did not select the original Creative WinMM MIDI synth")
    require("name=SB AWE32 MIDI Synth [620]" in text,
            "Unexpected MIDI endpoint: no software/MPU fallback permitted")
    require("AWE MIDI OPEN result=0x00000000" in text,
            "Original Creative midiOutOpen did not succeed")
    position = 0
    for pitch, velocity in NOTES:
        on = 0x90 | (pitch << 8) | (velocity << 16)
        off = 0x80 | (pitch << 8)
        for event in (
            f"NOTE_ON value=0x{on:08X} result=0x00000000",
            f"NOTE_OFF value=0x{off:08X} result=0x00000000",
        ):
            at = text.find(event, position)
            require(at >= position, "Missing or failing WinMM event: " + event)
            position = at + len(event)
    require(text.count("NOTE_ON value=") == 5 and text.count("NOTE_OFF value=") == 5,
            "Expected exactly five original-driver note-on/off pairs")
    require("P001_GUEST_WINMM_MESSAGES_FAILED" not in text,
            "Guest WinMM reported a failure")
    return {
        "creative_midi_endpoint": "SB AWE32 MIDI Synth [620]",
        "note_on": 5, "note_off": 5,
        "all_note_winmm_returns_zero": True,
        # This marker is optional to the *note* parity test; do not claim the
        # helper exited normally when the final marker was never persisted.
        "helper_normal_exit_marker_observed":
            "P001_GUEST_WINMM_MESSAGES_SENT_OK" in text,
    }


def analyze_wav(path: Path) -> dict:
    with wave.open(str(path), "rb") as audio:
        require(
            (audio.getframerate(), audio.getnchannels(),
             audio.getsampwidth(), audio.getcomptype()) ==
            (44100, 2, 2, "NONE"), "Reference WAV must be 44.1-kHz stereo PCM16")
        frames = audio.getnframes()
        require(frames >= 88200, "Reference too short to contain note playback")
        audible_seconds = []
        nonzero = 0
        peak = 0
        mutant_frame = None
        mutant_channel = None
        for second in range((frames + 44099) // 44100):
            chunk = audio.readframes(min(44100, frames - second * 44100))
            samples = array("h")
            samples.frombytes(chunk)
            if sys.byteorder != "little":
                samples.byteswap()
            count = sum(sample != 0 for sample in samples)
            nonzero += count
            peak = max(peak, max((abs(x) for x in samples), default=0))
            if count >= 1000:
                audible_seconds.append(second)
                if mutant_frame is None:
                    first = next(i for i, sample in enumerate(samples) if sample)
                    mutant_frame = second * 44100 + first // 2
                    mutant_channel = first % 2
    require(nonzero >= 100000, "Only boot pop or silence; no genuine musical PCM")
    require(peak >= 64, "All purported musical PCM is near digital silence")
    require(any(audible_seconds[i + 2] == audible_seconds[i] + 2
                for i in range(max(0, len(audible_seconds) - 2))),
            "No contiguous three-second musical note section")
    return {
        "stereo_frames": frames, "pcm16_samples": frames * 2,
        "nonzero_samples": nonzero, "peak_pcm16": peak,
        "audible_seconds": audible_seconds,
        "negative_frame": mutant_frame, "negative_channel": mutant_channel,
    }


def analyze_trace(path: Path, frame_count: int, active_seconds: set[int]) -> dict:
    writes = reads = write_during_music = 0
    last_frame = -1
    with path.open("r", encoding="utf-8", errors="strict") as stream:
        for line in stream:
            fields = line.split()
            if not fields or fields[0].startswith("#"):
                continue
            reading = fields[0] == "R"
            if reading:
                fields = fields[1:]
            require(len(fields) == 3, "Invalid original EMU8K_TRACE event format")
            frame = int(fields[0], 10)
            port = int(fields[1], 16)
            value = int(fields[2], 16)
            require(last_frame <= frame < frame_count,
                    "Nonmonotonic trace or register event beyond WAV length")
            require(0 <= port <= 65535 and 0 <= value <= 65535,
                    "Out-of-range EMU8000 port or value")
            last_frame = frame
            if reading:
                reads += 1
            else:
                writes += 1
                if frame // 44100 in active_seconds:
                    write_during_music += 1
    require(writes > 100 and reads > 0, "Real guest chip trace lacks meaningful I/O")
    require(write_during_music >= 100,
            "No actual EMU8000 register writes during audible note window")
    return {
        "writes": writes, "reads": reads, "total_events": writes + reads,
        "writes_during_music": write_during_music,
        "final_event_frame": last_frame,
    }


def run(*argv) -> tuple[int, str]:
    p = subprocess.run([str(item) for item in argv], capture_output=True,
                       text=True, timeout=240)
    return p.returncode, p.stdout + "\n" + p.stderr


def audit(args) -> dict:
    require(sha256(args.rom) == ROM_SHA, "Original Creative ROM digest mismatch")
    guest = verify_guest_log(args.guest_log)
    audio = analyze_wav(args.fork_wav)
    registers = analyze_trace(args.trace, audio["stereo_frames"],
                              set(audio["audible_seconds"]))
    out = args.private_out.resolve()
    require(not out.exists(), "Private output directory exists; no overwrite")
    out.mkdir(parents=True)
    native_wav = out / "native.wav"
    code, output = run(args.replay, "--rom", args.rom,
                       "--trace", args.trace, "--fork-wav", args.fork_wav,
                       "--out", native_wav)
    require(code == 0, "Original Win95 guest vs native PCM mismatch:\n" + output[-2500:])
    match = re.search(r"IDENTICAL: (\d+) stereo frames, (\d+) individual samples, (\d+) register events", output)
    require(bool(match), "Replay reported no genuine bit-exact PCM verdict")
    require(tuple(map(int, match.groups())) ==
            (audio["stereo_frames"], audio["pcm16_samples"], registers["total_events"]),
            "Replay output counts do not match independent original captures")
    fork_sha = sha256(args.fork_wav)
    require(sha256(native_wav) == fork_sha, "Full source WAV and replay WAV bytes differ")
    mutant_ok = None
    if args.negative:
        mutant_wav = out / "one-bit-mutant.wav"
        shutil.copyfile(args.fork_wav, mutant_wav)
        frame = audio["negative_frame"]
        channel = audio["negative_channel"]
        with mutant_wav.open("r+b") as f:
            f.seek(44 + 2 * (2 * frame + channel))
            raw = f.read(2)
            require(len(raw) == 2, "Cannot locate non-silent sample to mutate")
            f.seek(-2, 1)
            f.write(bytes([raw[0] ^ 1, raw[1]]))
        wrong_pcm = out / "one-bit-mutant-native.wav"
        fail_code, fail_output = run(args.replay, "--rom", args.rom,
                                     "--trace", args.trace,
                                     "--fork-wav", mutant_wav, "--out", wrong_pcm)
        expected = f"first differing frame {frame} channel {channel}"
        require(fail_code == 11 and expected in fail_output,
                "Replay failed to reject a single-bit altered actual note:\n" + fail_output[-1500:])
        print("EXPECTED_MUSICAL_PCM_BIT_MUTATION_REJECTED", expected)
        mutant_ok = True
        mutant_wav.unlink()
        if wrong_pcm.exists():
            wrong_pcm.unlink()
    return {
        "scope": "Independent booted Win95 with genuine Creative WinMM driver inside pinned 86Box-AWE32, against native adapter",
        "original_chip_commit": "fee1e81977824fd2d5eed4d239589dc30d251dd7",
        "unmodified_chip_blob": "e60eb10f00a8e1187380faa25989f6ba0433d99a",
        "original_rom_sha256": ROM_SHA,
        "trace_sha256": sha256(args.trace),
        "guest_log_sha256": sha256(args.guest_log),
        "fork_wav_sha256": fork_sha,
        "native_wav_sha256": sha256(native_wav),
        "guest": guest, "audio": audio, "registers": registers,
        "all_pcm_samples_identical": True,
        "one_bit_mutation_rejected": mutant_ok,
        "physical_awe64_gold_output": "NOT_TESTED",
        "original_creative_sbk_sf2_program_translation": "NOT_TESTED",
        "studio_one_asio_and_rt_allocations": "NOT_TESTED",
    }


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--trace", type=Path, required=True)
    p.add_argument("--fork-wav", type=Path, required=True)
    p.add_argument("--guest-log", type=Path, required=True)
    p.add_argument("--replay", type=Path, required=True)
    p.add_argument("--private-out", type=Path, required=True)
    p.add_argument("--negative", action="store_true")
    args = p.parse_args()
    result = audit(args)
    (args.private_out / "audit.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("P001_INDEPENDENT_GUEST_CREATIVE_PCM_PASS",
          "samples_exact=" + str(result["audio"]["pcm16_samples"]),
          "nonzero=" + str(result["audio"]["nonzero_samples"]),
          "events=" + str(result["registers"]["total_events"]),
          "note_on=" + str(result["guest"]["note_on"]),
          "note_off=" + str(result["guest"]["note_off"]),
          "helper_exit_marker=" + str(result["guest"]["helper_normal_exit_marker_observed"]))
    print("PHYSICAL_HARDWARE_AND_SBK_SF2_FIDELITY_NOT_TESTED")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print("P001_INDEPENDENT_GUEST_CREATIVE_PCM_NO_GO:", error, file=sys.stderr)
        sys.exit(1)
