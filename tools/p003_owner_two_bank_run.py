#!/usr/bin/env python3
"""P003 one-click ORIGINAL Windows95 Creative two-SF2 allocator/audio capture.

Uses the owner's existing calibrated private P002 86Box ZIP as a READ-ONLY
source. Extracts a fresh disposable VM clone, installs the independently
built P003 Win95 guest into the clone, adds a different second SF2, runs
the ORIGINAL installed Creative 1996 driver inside 86Box, and makes a
small ZIP of actual register/WAV/guest evidence after emulator exit.

Never modifies the owner's original ZIP, ROM, Windows disk or Creative DLL.
Never claims real original PCM == native VST3 PCM without a comparison.
"""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path,PurePosixPath
import secrets
import struct
import subprocess
import sys
import wave
import zipfile

from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
from p002_verify_original_win95_ram_guest_log import audit
from p003_private_fat16_replace import patch_disposable_guest,digest
from p003_join_original_guest_cpu_chip_clocks import join_traces

PINNED_FORK_SHA="67ec5e58643e32a6dc4efd6e1b42ce2ce7b7da048d218355a54ff39cdc327f16"
PINNED_ROM_SHA="4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
PINNED_OWNER_SF2_SHA="d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b"
PINNED_VXD_SHA="d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
PINNED_DLL_SHA="53b502d0824b75cf8d14c06173ccba5baf53b0c6eff6cd617f771d42ca2d7176"
NEEDED=("86Box/86Box.exe","86Box/roms/sound/creative/awe32.raw",
        "86Box/roms/sound/creative/CT4540 PnP.BIN",
        "VM/86box.cfg","VM/win95.img")
DEFAULT_PRIVATE_ZIP="125A-AWE64-P002-Win95-CC0-korrigiert-PRIVATE.zip"
MAX_SOURCE_BYTES=1_150_000_000

def sha256(path:Path)->str:
    hash=hashlib.sha256()
    with path.open("rb") as stream:
        for piece in iter(lambda:stream.read(1024*1024),b""):
            hash.update(piece)
    return hash.hexdigest()

def require(cond:bool,message:str):
    if not cond: raise ValueError(message)

def chunk(name:bytes,content:bytes)->bytes:
    assert len(name)==4
    return name+struct.pack("<I",len(content))+content+(b"\x00" if len(content)&1 else b"")

def list_chunk(label:bytes,chunks:list[bytes])->bytes:
    return chunk(b"LIST",label+b"".join(chunks))

def sf_name(text:str)->bytes:
    raw=text.encode("ascii")
    assert len(raw)<=20
    return raw.ljust(20,b"\x00")

def generated_comparison_sf2()->bytes:
    """Source-CLEAN mono SF2 (not Creative media), 32,768 distinct words.

    Unlike a clone of the owner's bank, this second bank has different
    dimensions and a real playable preset 7/sample 0. A guest driver
    rejection is reported as NO-GO, never replaced by a fake capture.
    """
    n=32768
    pcm=bytearray()
    for i in range(n):
        v=0 if i<128 or i>=n-128 else (11100 if (i//96)%2==0 else -11100)
        pcm.extend(struct.pack("<h",v))
    info=list_chunk(b"INFO",[
        chunk(b"ifil",struct.pack("<HH",2,1)),
        chunk(b"isng",b"EMU8000\x00"),
        chunk(b"INAM",b"125A P003 Compare\x00"),
    ])
    sdta=list_chunk(b"sdta",[chunk(b"smpl",bytes(pcm))])
    def ph(name:str,program:int,bag:int)->bytes:
        return sf_name(name)+struct.pack("<HHHIII",program,0,bag,0,0,0)
    def inst(name:str,bag:int)->bytes:
        return sf_name(name)+struct.pack("<H",bag)
    def sh(name:str,start:int,end:int,lo:int,hi:int,sample_type:int)->bytes:
        return (sf_name(name)+struct.pack("<IIIII",start,end,lo,hi,44100)
                +struct.pack("<BBHH",60,0,0,sample_type))
    pdta=list_chunk(b"pdta",[
        chunk(b"phdr",ph("P003-GUEST",7,0)+ph("EOP",0,1)),
        chunk(b"pbag",struct.pack("<HHHH",0,0,1,0)),
        chunk(b"pmod",bytes(10)),
        chunk(b"pgen",struct.pack("<HH",41,0)),
        chunk(b"inst",inst("P003-INST",0)+inst("EOI",1)),
        chunk(b"ibag",struct.pack("<HHHH",0,0,1,0)),
        chunk(b"imod",bytes(10)),
        chunk(b"igen",struct.pack("<HH",53,0)),
        chunk(b"shdr",sh("P003-SAMPLE",0,n,128,n-128,1)+sh("EOS",0,0,0,0,0)),
    ])
    body=b"sfbk"+info+sdta+pdta
    return b"RIFF"+struct.pack("<I",len(body))+body

def package_directory()->Path:
    return (Path(sys.executable) if getattr(sys,"frozen",False)
            else Path(__file__)).resolve().parent

def choose_private_zip(folder:Path,explicit:Path|None)->Path:
    if explicit is not None: return explicit.resolve()
    matching=folder/DEFAULT_PRIVATE_ZIP
    if matching.is_file():return matching
    possible=list(folder.glob("*P002*PRIVATE*.zip"))
    if len(possible)==1:return possible[0]
    if os.name=="nt":
        try:
            from tkinter import Tk,filedialog
            window=Tk();window.withdraw()
            filename=filedialog.askopenfilename(
                title="125A: Vorhandenes privates P002-Win95-86Box-Paket auswaehlen",
                filetypes=[("Private 86Box ZIP","*.zip")])
            window.destroy()
            if filename:return Path(filename)
        except (ImportError,OSError):pass
    raise ValueError("Privates kalibriertes 86Box-Paket nicht gefunden. "
                     "Lege die vorhandene P002-Win95-PRIVATE.zip neben die BAT.")

def verify_private_archive(archive:Path)->dict:
    require(archive.is_file(),"Privates Original-86Box-ZIP fehlt")
    with zipfile.ZipFile(archive) as src:
        files=src.infolist()
        names=[row.filename for row in files]
        require(len(set(names))==len(names) and
                all(x in names for x in NEEDED),
                "Privates 86Box-ZIP unvollstaendig oder doppelte Eintraege")
        require(src.getinfo("VM/win95.img").file_size==536870912,
                "Privater Win95 FAT16-Datentraeger ist nicht genau 512MiB")
        total=0
        for row in files:
            parts=PurePosixPath(row.filename).parts
            require(parts and not row.filename.startswith(("/","\\"))
                    and "\\" not in row.filename and ":" not in row.filename
                    and all(x not in ("",".","..") for x in parts),
                    "Ungeltiger/unsicherer Pfad im privaten ZIP")
            total+=row.file_size
            require(total<=MAX_SOURCE_BYTES,"Privates VM-ZIP zu gross")
        def member_sha(name):
            hash=hashlib.sha256()
            with src.open(name) as item:
                for block in iter(lambda:item.read(1024*1024),b""):
                    hash.update(block)
            return hash.hexdigest()
        require(member_sha("86Box/86Box.exe")==PINNED_FORK_SHA,
                "Nicht der hardwarekalibrierte originale 86Box-Fork")
        require(member_sha("86Box/roms/sound/creative/awe32.raw")==
                PINNED_ROM_SHA,"Nicht die originale SHA-verifizierte AWE32-ROM")
        return {"private_archive_sha256":sha256(archive),
                "pinned_emulator_sha256":PINNED_FORK_SHA,
                "pinned_rom_sha256":PINNED_ROM_SHA}

def verify_pinned_guest(disk:Path)->bytes:
    with PrivateFat16Reader(disk) as image:
        first=image.read_root_file(b"P002RAM SF2",32*1024*1024)
        require(digest(first)==PINNED_OWNER_SF2_SHA,
                "Die erste SoundFont entspricht nicht der originalen P002-Referenz")
        for short_name,expected in (
                (b"SBAWE   VXD",PINNED_VXD_SHA),
                (b"AWEMAN32DLL",PINNED_DLL_SHA)):
            vendor=image.read_short_path_file(
                (b"WINDOWS    ",b"SYSTEM     ",short_name),2*1024*1024)
            require(digest(vendor)==expected,
                    "Originaler Creative Windows-95-Treiber hat falschen SHA")
        try:
            image.read_root_file(b"P002RAM LOG",128*1024)
        except ValueError:
            pass
        else:
            raise ValueError("Alter Gast-Test-LOG bereits vorhanden; "
                             "kein sauberer Originaltest")
    return first

def _new_zip(target:Path,items:dict[str,Path],report:dict)->None:
    target.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(target,"x",compression=zipfile.ZIP_DEFLATED,
                         compresslevel=5,allowZip64=True) as z:
        z.writestr("P003-BERICHT.json",
                   json.dumps(report,indent=2,sort_keys=True,ensure_ascii=False)+"\n")
        z.writestr("START-HIER.txt",
                   "125A P003 echte Win95-Creative-Zweibankmessung\r\n"
                   "Ergebnisstatus und SHA-Pruefsummen: P003-BERICHT.json\r\n"
                   "Private Original-VM, ROM, Creative-DLL/VXD NICHT enthalten.\r\n"
                   "Quell-EMU8000-Registerspur/WAV sind keine VST3-Paritaet.\r\n")
        for label,file in items.items():
            if file.is_file() and file.stat().st_size>0:
                z.write(file,label)

def check_wav(path:Path)->dict:
    require(path.is_file(),"Originales WAV von 86Box fehlt")
    with wave.open(str(path),"rb") as input:
        meta={"sample_rate":input.getframerate(),
              "channels":input.getnchannels(),
              "bits":input.getsampwidth()*8,
              "frames":input.getnframes()}
    require(meta["sample_rate"]==44100 and meta["channels"]==2 and
            meta["bits"]==16 and meta["frames"]>=44100,
            "Originales 86Box-WAV ist nicht 44.1kHz Stereo PCM16")
    return meta

def run(archive:Path,guest_exe:Path,comparison:Path|None,
        results_root:Path,cpu_probe_fork:Path|None=None,
        expected_cpu_probe_sha256:str|None=None)->tuple[Path,dict]:
    require(os.name=="nt",
            "Echter Original-86Box-Zweibanktest braucht Windows 11")
    verified=verify_private_archive(archive)
    if cpu_probe_fork is not None:
        require(bool(expected_cpu_probe_sha256) and
                len(expected_cpu_probe_sha256)==64 and
                all(c in "0123456789abcdef" for c in expected_cpu_probe_sha256),
                "P003 CPU-Probe-Fork benoetigt gepinnte 64-stellige SHA256")
        require(cpu_probe_fork.is_file() and
                sha256(cpu_probe_fork)==expected_cpu_probe_sha256,
                "P003 CPU-Probe-86Box muss exakt dem separaten CI-Build entsprechen")
    else:
        require(expected_cpu_probe_sha256 is None,
                "Kein CPU-Probe-EXE angegeben, SHA allein unzulaessig")
    require(guest_exe.is_file(),"Das neue P003-Gastprogramm fehlt im Paket")
    guest_bytes=guest_exe.read_bytes()
    second=comparison.read_bytes() if comparison else generated_comparison_sf2()
    stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    nonce=secrets.token_hex(4)
    session=f"P003-{stamp}-{nonce}"
    work=results_root/(session+"-PRIVATE")
    result_zip=results_root/(session+"-ERGEBNIS.zip")
    require(not work.exists() and not result_zip.exists(),
            "Der P003-Test darf keine bestehende private Sitzung ueberschreiben")
    results_root.mkdir(parents=True,exist_ok=True)
    work.mkdir()
    originals=work/"OWNER-ORIGINAL-VM-KOPIE"
    originals.mkdir()
    media=work/"P003-Nachweise"
    media.mkdir()
    original_sf2_path=media/"ERSTE-BANK-ORIGINAL.sf2"
    second_sf2_path=media/"ZWEITE-BANK-VERGLEICH.sf2"
    second_sf2_path.write_bytes(second)
    trace=media/"ORIGINAL-EMU8000-REGISTER.trace"
    wav=media/"ORIGINAL-EMU8000-DRY.wav"
    cpu_trace=media/"ORIGINAL-86BOX-GUEST-CPU-IO.trace"
    guest_log=media/"ORIGINAL-CREATIVE-P002RAM.LOG"
    report={"schema":("125A-P003-OWNER-CPU-IO-TRACE-AND-CHIP-PCM-V1"
                      if cpu_probe_fork is not None else
                      "125A-P003-OWNER-TWO-BANK-CAPTURE-V1"),
            "session":session,"status":"NO_GO_NOT_RUN",
            "real_original_Creative_RAM_bank2_parity":"NOT_YET_PROVEN",
            "real_original_vs_native_VST3_PCM":"NOT_COMPARED",
            "source_only_no_GUI_ASIO_test":True,
            "source_identifiers":verified}
    deliver={"Original-Register.trace":trace,"Original-DRY.wav":wav,
             "Creative-Win95-Gast.LOG":guest_log,
             "Original-Bank-1.sf2":original_sf2_path,
             "Vergleichs-Bank-2.sf2":second_sf2_path}
    if cpu_probe_fork is not None:
        deliver["Original-CPU-IO.trace"]=cpu_trace
        report["original_fork_cpu_io_trace_enabled"]=True
        report["cpu_tsc_to_chip_frame_calibrated"]=False
    try:
        with zipfile.ZipFile(archive) as source:
            source.extractall(originals)
        disk=originals/"VM"/"win95.img"
        first=verify_pinned_guest(disk)
        original_sf2_path.write_bytes(first)
        staged=patch_disposable_guest(
            disk,guest_bytes,second,PINNED_OWNER_SF2_SHA)
        report["staging"]=staged
        report["source_identifiers"]["guest_exe_sha256"]=sha256(guest_exe)
        emulator=originals/"86Box"/"86Box.exe"
        require(sha256(emulator)==PINNED_FORK_SHA,
                "Privater Emulator hat sich nach dem Entpacken geaendert")
        if cpu_probe_fork is not None:
            # Replace executable ONLY inside the already extracted disposable
            # private copy. The source ZIP and calibrated original exe remain
            # immutable. The instrumented fork only changed x86 src/io.c.
            with cpu_probe_fork.open("rb") as source, emulator.open("wb") as dest:
                for part in iter(lambda:source.read(1024*1024),b""):
                    dest.write(part)
            require(sha256(emulator)==expected_cpu_probe_sha256,
                    "CPU-Probe-86Box changed after copying")
            report["source_identifiers"]["cpu_probe_fork_sha256"]=(
                expected_cpu_probe_sha256)
            report["source_identifiers"]["baseline_fork_sha256"]=PINNED_FORK_SHA
        report["status"]="ORIGINAL_VM_STAGED_NOT_YET_EXECUTED"
        env=os.environ.copy()
        env.update({"EMU8K_TRACE":str(trace),"AWE32_WAV":str(wav)})
        if cpu_probe_fork is not None:
            env["AWE64_P003_CPU_IO_TRACE"]=str(cpu_trace)
            env["AWE64_P003_GUEST_CALLSTACK"]="1"
        print("\n125A P003: Original-Windows-95 und Creative-Treiber starten.")
        print("In der VM muss der Test automatisch starten.")
        print("Sobald er fertig ist, 86Box ueber Datei > Beenden schliessen.")
        print("Danach wird die Ergebnis-ZIP automatisch erstellt.\n")
        process=subprocess.run([str(emulator),"-P",str(originals/"VM"),
                       "-R",str(originals/"86Box"/"roms")],
                       cwd=emulator.parent,env=env,check=False)
        report["86box_exit_code"]=process.returncode
        require(process.returncode==0,"86Box wurde mit einem Fehler beendet")
        with PrivateFat16Reader(disk) as image:
            require(image.read_root_file(b"P002RAM EXE",1_000_000)==guest_bytes,
                    "Windows-95-Testprogramm im Guest veraendert")
            require(image.read_root_file(b"P002RAM SF2",32*1024*1024)==first,
                    "Erste Originalbank wurde im Guest veraendert")
            require(image.read_root_file(b"P003BANKSF2",32*1024*1024)==second,
                    "Zweite SoundFont wurde im Guest veraendert")
            log=image.read_root_file(b"P002RAM LOG",128*1024)
        guest_log.write_bytes(log)
        # Original Creative API LOG is NOT enough by itself: demand
        # independently recorded source chip ports and original wav.
        api=audit(log,first,second)
        report["original_win95_api_audit"]=api
        require(trace.is_file() and 1024<=trace.stat().st_size<=
                1024*1024*1024,
                "Keine echte EMU8000-Registerspur aus dieser 86Box-Sitzung")
        report["original_source_wav"]=check_wav(wav)
        report["original_source_trace_bytes"]=trace.stat().st_size
        if cpu_probe_fork is not None:
            require(cpu_trace.is_file() and cpu_trace.stat().st_size>10000,
                    "CPU-Zeitspur des instrumentierten Originals fehlt")
            with trace.open(encoding="utf-8") as chip_source, \
                 cpu_trace.open(encoding="utf-8") as cpu_source:
                joined=join_traces(chip_source,cpu_source)
            require(joined["guest_stack_probe_event_count"]==
                    joined["matched_io_events"],
                    "V4 Gast-CPU Ruecksprungadressen fehlen/alt-V3-Messung")
            require(joined["matched_io_events"]>1000 and
                    joined["physical_write_events"]>1000 and
                    joined["event_sequence_exact"],
                    "CPU-Ereignisse nicht vollstaendig an Chip-EMU8K gebunden")
            report["cpu_chip_event_join"]={
                "matched_port_events":joined["matched_io_events"],
                "physical_writes":joined["physical_write_events"],
                "cpu_tsc_regressions":joined["guest_tsc_regressions"],
                "unverified_guest_top_ret_eip_unavailable":
                    joined["guest_stack_top_return_eip_unavailable"],
                "guest_caller_direct_call_opcode_proven":False,
                "guest_return_stack_v4_enabled":True,
                "cpu_clock_to_chip_sample_calibrated":False,
                "emu8000_original_core_unchanged":True,
            }
            report["status"]="SOURCE_CREATIVE_TWO_BANK_API_TRACE_WAV_CPU_IO_CAPTURED"
        else:
            report["status"]="SOURCE_CREATIVE_TWO_BANK_API_TRACE_WAV_CAPTURED"
        report["real_original_Creative_RAM_bank2_parity"]=(
            "SOURCE_ONLY_SOURCE_PORT_ADDRESS_DIFF_PENDING")
        report["real_original_vs_native_VST3_PCM"]="NOT_COMPARED"
    except (OSError,ValueError,zipfile.BadZipFile,EOFError,
            wave.Error) as exc:
        report["status"]="NO_GO"
        report["error"]=str(exc)
    finally:
        report["evidence_sha256"]={
            label:sha256(file) for label,file in deliver.items()
            if file.is_file() and file.stat().st_size>0}
        report["private_original_archive_and_ROM_embedded"]=False
        report["original_emu8000_core_modified"]=False
        _new_zip(result_zip,deliver,report)
    return result_zip,report

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-archive",type=Path)
    p.add_argument("--guest-exe",type=Path)
    p.add_argument("--second-sf2",type=Path)
    p.add_argument("--results-root",type=Path)
    p.add_argument("--cpu-probe-fork-exe",type=Path)
    p.add_argument("--cpu-probe-fork-sha256")
    args=p.parse_args()
    root=package_directory()
    archive=choose_private_zip(root,args.private_original_archive)
    guest=args.guest_exe if args.guest_exe else root/"P002RAM.EXE"
    destination=(args.results_root if args.results_root
                 else root).resolve()
    result,report=run(archive,guest,args.second_sf2,destination,
                      args.cpu_probe_fork_exe,args.cpu_probe_fork_sha256)
    print("\nP003-Ergebnis: "+str(result))
    print("Status: "+report["status"])
    if "error" in report:print("Fehler: "+report["error"])
    print("Die private VM befindet sich im PRIVATE-Unterordner.")
    return 0 if report["status"].startswith("SOURCE_CREATIVE_") else 1

if __name__=="__main__":
    try:sys.exit(main())
    except (OSError,ValueError,zipfile.BadZipFile) as exc:
        print("125A P003 TEST NO-GO:",exc,file=sys.stderr)
        sys.exit(2)
