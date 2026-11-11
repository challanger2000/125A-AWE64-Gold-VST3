#!/usr/bin/env python3
"""P001: render the real owner ROM with the pinned direct EMU8000 C core and the native chip adapter.

This compares two invocations of the exact original chip core. It DOES NOT
satisfy the independent running-86Box/Creative-driver PCM parity gate.
No real ROM, SBK, register samples or private PCM are uploaded to GitHub.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import wave

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_original_media import verify as verify_media, ROM_SHA, SBK_SHA


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def command(*args) -> str:
    completed = subprocess.run(
        [str(x) for x in args], capture_output=True, text=True, timeout=240
    )
    result = completed.stdout + completed.stderr
    if completed.returncode:
        raise RuntimeError(
            f"Command failed with exit={completed.returncode} "
            f"({Path(str(args[0])).name}):\n{result}"
        )
    return result


def pcm_stats(path: Path) -> tuple[int, int, int]:
    import array
    with wave.open(str(path), "rb") as audio:
        if (
            audio.getframerate(), audio.getnchannels(),
            audio.getsampwidth(), audio.getcomptype()
        ) != (44100, 2, 2, "NONE"):
            raise ValueError("Invalid 44.1-kHz PCM16 fork reference")
        total = audio.getnframes()
        nonzero = 0
        max_abs = 0
        for start in range(0, total, 4096):
            chunk = array.array("h")
            chunk.frombytes(audio.readframes(min(total - start, 4096)))
            if sys.byteorder != "little":
                chunk.byteswap()
            nonzero += sum(x != 0 for x in chunk)
            max_abs = max(max_abs, max((abs(x) for x in chunk), default=0))
    return total, nonzero, max_abs


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--sbk", type=Path, required=True)
    p.add_argument("--direct", type=Path, required=True, help="Original-fork fixture executable")
    p.add_argument("--replay", type=Path, required=True, help="Native VST3-chip adapter replay executable")
    p.add_argument("--out", type=Path, required=True, help="New PRIVATE output directory outside repository")
    args = p.parse_args()
    verify_media(args.rom, args.sbk)
    root = Path(__file__).resolve().parent.parent
    private = args.out.resolve()
    if private == root or root in private.parents:
        raise ValueError("Private output MUST remain outside the Git checkout")
    if private.exists():
        raise ValueError("Output already exists; refusing to overwrite any private capture")
    private.mkdir(parents=True)
    try:
        direct = command(args.direct, "--real-rom", args.rom.resolve(), private)
        frames, nonzero, max_abs = pcm_stats(private / "fork.wav")
        if frames != 88200 or nonzero <= 2000 or max_abs < 32:
            raise ValueError("Real-ROM direct-fork audio is silent or below acceptance floor")
        native = command(
            args.replay, "--rom", args.rom.resolve(), "--trace",
            private / "fork.trace", "--fork-wav", private / "fork.wav",
            "--out", private / "native.wav"
        )
        nframes, native_nonzero, native_max = pcm_stats(private / "native.wav")
        if (frames, nonzero, max_abs) != (nframes, native_nonzero, native_max):
            raise ValueError("PCM audit metadata mismatch")
        match = re.search(r"IDENTICAL:\s*(\d+) stereo frames,\s*(\d+) individual samples,\s*(\d+) register events", native)
        if not match or tuple(map(int, match.groups()))[:2] != (88200, 176400):
            raise ValueError("Native replay did not report 176,400 exactly matching PCM samples")
        if sha(private / "fork.wav") != sha(private / "native.wav"):
            raise ValueError("PCM WAV SHA-256 mismatch despite replay success")
        report = {
            "p001": "original-ROM direct-core-vs-native-wrapper audit, NOT independent VM parity",
            "fork_core_commit": "fee1e81977824fd2d5eed4d239589dc30d251dd7",
            "fork_chip_blob": "e60eb10f00a8e1187380faa25989f6ba0433d99a",
            "private_rom_sha256": ROM_SHA,
            "private_sbk_sha256": SBK_SHA,
            "pcm_44100_stereo_frames": frames,
            "pcm16_samples_equal": frames * 2,
            "nonzero_pcm16": nonzero,
            "max_absolute_pcm16": max_abs,
            "port_events": int(match.group(3)),
            "direct_wav_sha256": sha(private / "fork.wav"),
            "native_wav_sha256": sha(private / "native.wav"),
            "real_rom_plays_via_unmodified_core": True,
            "driver_original_win95_trace": "NOT RUN",
            "independent_fork_vm_pcm_parity": "NOT RUN",
            "vst3_gui_and_host_midi": "NOT RUN",
            "product_release": "BLOCKED"
        }
        (private / "real-rom-audit.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8"
        )
        print(direct.strip())
        print(native.strip())
        print(
            "P001 PRIVATE ORIGINAL-ROM CHIP PCM PASS",
            f"frames={frames}",
            f"pcm16_samples_identical={frames * 2}",
            f"nonzero_pcm16={nonzero}",
            f"max_abs={max_abs}"
        )
        print("P001 INDEPENDENT ORIGINAL CREATIVE DRIVER / 86BOX VM: NOT RUN / NO-GO")
    except Exception:
        # Deliberately retain private failure evidence; never downgrade to PASS.
        print("P001 PRIVATE ORIGINAL-ROM AUDIT FAILED; INCOMPLETE OUTPUT LEFT PRIVATE", file=sys.stderr)
        raise


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError) as e:
        print("P001_REAL_ROM_NOT_VERIFIED: " + str(e), file=sys.stderr)
        sys.exit(1)
