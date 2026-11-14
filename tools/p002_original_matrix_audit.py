#!/usr/bin/env python3
"""P002: strict PRIVATE original-Creative Win95 62-message MIDI matrix audit.

This verifies a real guest transcript, a separate 86Box EMU8K port trace and
audible fork PCM, positive original-core adapter replay, and a deliberate
mutant bit flip in the actual music. It correlates 13 MIDI note-ons with
13 original EMU8000 hardware voice starts. It does NOT implement, certify or
falsely assert an independent native Creative MIDI-to-register translator.
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

from p002_register_oracle import read_trace

ORIGINAL_ROM_SHA256 = "4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
DEVICE = "SB AWE32 MIDI Synth [620]"
EXPECTED_SCENARIOS = (
    "RESET_AND_ROM_PROGRAM0",
    "VELOCITY_ATTACK_RELEASE",
    "LIVE_CC_PITCH_PRESSURE",
    "SUSTAIN_AND_RELEASE",
    "THREE_VOICES_RETRIGGER",
    "OTHER_CHANNELS_AND_DRUMS",
    "BANK_PROGRAM_CONTROLS_RESET",
)
MESSAGES = re.compile(
    r"^MIDI seq=0x([0-9A-F]{8}) tick_ms=0x([0-9A-F]{8}) "
    r"name=(\S+) value=0x([0-9A-F]{8}) rc=0x([0-9A-F]{8})$", re.M
)
SCENARIOS = re.compile(
    r"^SCENARIO index=0x([0-9A-F]{8}) tick_ms=0x([0-9A-F]{8}) name=(\S+)$", re.M
)


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for data in iter(lambda: source.read(1 << 20), b""):
            digest.update(data)
    return digest.hexdigest()


def parse_guest(text: str):
    if ("P002_ORIGINAL_CREATIVE_WIN95_MATRIX_START" not in text or
            "P002_ORIGINAL_CREATIVE_WINMM_SELECTED" not in text or
            f"NAME={DEVICE}" not in text or
            "P002_CREATIVE_OPEN_RESULT=0x00000000" not in text or
            "P002_GUEST_ORIGINAL_WINMM_MATRIX_FAILED" in text or
            not re.search(
                r"^P002_GUEST_ORIGINAL_WINMM_MATRIX_SENT_OK count=0x0000003E$",
                text, re.M)):
        raise ValueError("Original Win95 Creative MIDI device or successful guest termination not verified")
    rows = [dict(index=int(i,16), tick_ms=int(t,16), name=name,
                 raw=int(v,16), rc=int(rc,16))
            for i,t,name,v,rc in MESSAGES.findall(text)]
    if len(rows) != 62 or any(r["index"] != i or r["rc"] for i,r in enumerate(rows)):
        raise ValueError("Original Creative WinMM matrix incomplete, non-contiguous or reported errors")
    if any(rows[i]["tick_ms"] > rows[i+1]["tick_ms"] for i in range(len(rows)-1)):
        raise ValueError("Original guest MIDI timestamp regression")
    scenes = [(int(i,16),int(t,16),name) for i,t,name in SCENARIOS.findall(text)]
    if (len(scenes) != 7 or [s[0] for s in scenes] != list(range(7)) or
            tuple(s[2] for s in scenes) != EXPECTED_SCENARIOS):
        raise ValueError("Original Creative seven-scenario MIDI matrix is incomplete")
    notes = [r for r in rows if (r["raw"] & 0xF0) == 0x90 and
             ((r["raw"] >> 16) & 0x7F) != 0]
    if len(notes) != 13:
        raise ValueError("Expected exactly thirteen effective original MIDI NoteOn messages")
    if [r["name"] for r in notes[:3]] != [
            "VELOCITY_1_ON","VELOCITY_64_ON","VELOCITY_127_ON"]:
        raise ValueError("Original Creative velocity trio is missing or reordered")
    if [((r["raw"] >> 16) & 0x7f) for r in notes[:3]] != [1,64,127]:
        raise ValueError("Original Win95 Creative velocity stimulus was modified")
    if not any(r["name"]=="ZERO_VELOCITY_NOTEON_OFF" and
               ((r["raw"] >> 16) & 0x7f)==0 for r in rows):
        raise ValueError("Missing velocity-zero original Creative MIDI control")
    return rows, notes, scenes


def voice_starts(trace):
    ptr = None
    starts = []
    for frame,kind,port,value in trace:
        if kind != "WRITE":
            continue
        if port == 0xE22:
            ptr = (value & 31, (value >> 5) & 7)
            continue
        if ptr is None:
            continue
        voice, reg = ptr
        # DCYSUSV=00FF is the observed original Creative voice
        # initialization signature in THIS documented seven-scenario run.
        if port == 0xA20 and reg == 5 and value == 0x00FF:
            starts.append(dict(frame=frame, voice=voice, ip=None, ifatn=None))
        if starts and frame-starts[-1]["frame"] <= 128 and voice == starts[-1]["voice"]:
            if port == 0xE20 and reg == 0:
                starts[-1]["ip"] = value
            if port == 0xE20 and reg == 1:
                starts[-1]["ifatn"] = value
    if len(starts) != 13 or any(s["ip"] is None or s["ifatn"] is None for s in starts):
        raise ValueError("Original Creative note/EMU8000 voice-start count or pitch/filter/attenuation incomplete")
    return starts


def clock_fit(notes, starts):
    if len(notes) != len(starts) or len(notes) < 3:
        raise ValueError("Cannot correlate independent original guest and chip timebases")
    x = [float(r["tick_ms"]) for r in notes]
    y = [float(s["frame"]) for s in starts]
    mx, my = sum(x)/len(x), sum(y)/len(y)
    xx = sum((a-mx)**2 for a in x)
    if not xx:
        raise ValueError("Original guest log has no usable time variance")
    slope = sum((a-mx)*(b-my) for a,b in zip(x,y))/xx
    intercept = my-slope*mx
    residuals = [(b-(intercept+slope*a))/44.1 for a,b in zip(x,y)]
    # This is a clock correlation sanity check, NOT a MIDI -> chip
    # conversion algorithm or a tolerance on register-stream parity.
    if not (44.0 < slope < 44.2) or max(map(abs,residuals)) > 1.0:
        raise ValueError("Guest original MIDI ticks do not match observed chip voice starts")
    return slope, intercept, residuals


def musical_wav(path: Path):
    seconds = []
    significant = 0
    peak = 0
    with wave.open(str(path),"rb") as original:
        if (original.getframerate(),original.getnchannels(),
                original.getsampwidth(),original.getcomptype()) != (44100,2,2,"NONE"):
            raise ValueError("Original 86Box fork WAV not signed 44.1-kHz stereo PCM16")
        frames = original.getnframes()
        if frames < 44100 * 130:
            raise ValueError("Original VM WAV too short for full MIDI test")
        for i in range((frames+44099)//44100):
            a = array.array("h")
            a.frombytes(original.readframes(44100))
            if sys.byteorder != "little":
                a.byteswap()
            n=sum(abs(v)>4 for v in a)
            significant+=n
            peak=max(peak,max((abs(v) for v in a),default=0))
            if n>10000:
                seconds.append(i)
    if significant < 100000 or peak < 500 or not any(
            all(i in seconds for i in range(s,s+4)) for s in seconds):
        raise ValueError("No sustained actual original Creative driver musical PCM")
    return frames, significant, peak, seconds


def run_replay(exe, rom, trace, wav, out):
    r = subprocess.run([str(exe),"--rom",str(rom),
                        "--trace",str(trace),"--fork-wav",str(wav),
                        "--out",str(out)],
                       capture_output=True,text=True,timeout=300)
    return r.returncode, r.stdout+"\n"+r.stderr


def verify(rom, log, tracefile, wavefile, replay, out):
    if (rom.stat().st_size != 1048576 or sha(rom) != ORIGINAL_ROM_SHA256):
        raise ValueError("Original private AWE32 ROM signature mismatch")
    rows, notes, scenes = parse_guest(log.read_text(encoding="latin-1"))
    trace = read_trace(tracefile)
    starts = voice_starts(trace)
    slope, intercept, residuals = clock_fit(notes,starts)
    frames, significant, peak, seconds = musical_wav(wavefile)
    if max(row[0] for row in trace) >= frames:
        raise ValueError("Original Creative register capture extends beyond paired digital PCM")
    if out.exists():
        raise ValueError("Refuse to overwrite private independent Original Win95 evidence")
    out.mkdir(parents=True)
    native = out/"native.wav"
    rc, result = run_replay(replay,rom,tracefile,wavefile,native)
    total=len(trace)
    marker=f"IDENTICAL: {frames} stereo frames, {2*frames} individual samples, {total} register events"
    if rc != 0 or marker not in result or sha(wavefile)!=sha(native):
        raise ValueError("Original-Win95 Creative fork vs native digital PCM mismatch: "+result)
    # Force a 1-bit corruption INSIDE THE AUDIBLE original, not quiet boot.
    music_second=seconds[0]
    with wave.open(str(wavefile),"rb") as w:
        w.setpos(music_second*44100)
        chunk=array.array("h")
        chunk.frombytes(w.readframes(44100))
    if sys.byteorder!="little":chunk.byteswap()
    sample=next((i for i,v in enumerate(chunk) if abs(v)>100),None)
    if sample is None:
        raise ValueError("Cannot locate genuine audible original negative control")
    bad_frame=music_second*44100+sample//2
    bad_channel=sample%2
    mutated=out/"mutated-private-original.wav"
    shutil.copyfile(wavefile,mutated)
    with mutated.open("r+b") as f:
        f.seek(44+bad_frame*4+bad_channel*2)
        before=struct.unpack("<H",f.read(2))[0]
        f.seek(-2,1)
        f.write(struct.pack("<H",before^1))
    rc, bad_result=run_replay(replay,rom,tracefile,mutated,out/"mutated-native.wav")
    if rc!=11 or f"frame {bad_frame} channel {bad_channel}" not in bad_result:
        raise ValueError("Real original musical-sample negative control failed: "+bad_result)
    mutated.unlink(missing_ok=True)
    (out/"mutated-native.wav").unlink(missing_ok=True)
    report={
        "verdict":"P002 ORIGINAL WIN95 CREATIVE 62-MESSAGE MATRIX AND CHIP REPLAY PASS",
        "scope":"real Win95 Creative driver data -> original calibrated chip -> native chip adapter ONLY",
        "native_midi_to_register_translator":"NOT IMPLEMENTED / NOT VERIFIED",
        "original_rom_sha256":ORIGINAL_ROM_SHA256,
        "guest_log_sha256":sha(log),"original_register_sha256":sha(tracefile),
        "original_wav_sha256":sha(wavefile),"native_wav_sha256":sha(native),
        "midi_messages":len(rows),"scenarios":len(scenes),"positive_note_ons":len(notes),
        "emu8000_voice_starts":len(starts),"register_events":total,
        "port_writes":sum(r[1]=="WRITE" for r in trace),
        "port_reads":sum(r[1]=="READ" for r in trace),
        "stereo_frames":frames,"pcm16_samples_exact":2*frames,
        "meaningful_pcm16_samples_abs_gt_4":significant,"peak_pcm16":peak,
        "active_music_seconds":seconds,
        "guest_tick_to_chip_frame_slope":slope,
        "clock_fit_max_residual_ms":max(map(abs,residuals)),
        "audible_mutant_rejected_frame":bad_frame,
        "audible_mutant_rejected_channel":bad_channel,
        "audible_mutant_replay_exit":rc,
        "original_note_on_observations":[
            {"name":ev["name"],"midi_note":(ev["raw"]>>8)&127,
             "velocity":(ev["raw"]>>16)&127,"guest_tick_ms":ev["tick_ms"],
             "physical_voice":s["voice"],"chip_frame":s["frame"],
             "IFATN":f"0x{s['ifatn']:04X}","IP":f"0x{s['ip']:04X}",
             "attenuation":s["ifatn"]&255,"cutoff":s["ifatn"]>>8}
            for ev,s in zip(notes,starts)
        ],
        "GM_all_presets_and_SF2":"NOT TESTED",
        "Studio_One_ASIO":"DEFERRED UNTIL FINAL GUI/PRODUCT ACCEPTANCE",
    }
    (out/"P002-original-Win95-Creative-audit.json").write_text(
        json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("P002_REAL_CREATIVE_MATRIX_AND_ORIGINAL_CHIP_REPLAY_PASS",
          f"midi={len(rows)}","voice_starts=13",f"pcm16={2*frames}",
          f"meaningful_samples={significant}",f"registers={total}",
          "native_Creative_driver_implementation=NOT_VERIFIED")
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ("rom","guest-log","trace","fork-wav","replay","out"):
        p.add_argument("--"+key,type=Path,required=True)
    a=p.parse_args()
    root=Path(__file__).resolve().parent.parent
    out=a.out.resolve()
    if out==root or root in out.parents:
        raise ValueError("Original Win95/private ROM/WAV audit must stay outside Git checkout")
    verify(a.rom.resolve(),a.guest_log.resolve(),a.trace.resolve(),
           a.fork_wav.resolve(),a.replay.resolve(),out)


if __name__=="__main__":
    try:main()
    except (OSError,ValueError,UnicodeError,subprocess.TimeoutExpired) as e:
        print("P002_ORIGINAL_CREATIVE_MATRIX_NO_GO",e,file=sys.stderr)
        sys.exit(1)
