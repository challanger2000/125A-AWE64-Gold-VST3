#!/usr/bin/env python3
"""P002 register-semantic decoder, derived from Creative EMU8000 Programmer's Guide.

Input is ORIGINAL running 86Box Creative EMU8K_TRACE or a separately produced
native P002 trace. This decodes *observed chip writes*, never invents MIDI
events, hidden state or driver algorithms. A semantic summary is NOT parity.
Hardware guide: EMU8000 Programmer's Guide rev 1.00, pp. 7-10, 15-18.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import sys
from p002_register_oracle import read_trace

# Selector comes from 0xE22, low five bits = channel/voice 0..31,
# next three bits = register index 0..7. A voice is NOT a MIDI channel.
DATA0=("CPF","PTRX","CVCF","VTFT","Z2","Z1","PSST","CSL")
DATA1={0:"CCCA",2:"INIT1",3:"INIT3",4:"ENVVOL",5:"DCYSUSV",
       6:"ENVVAL",7:"DCYSUS"}
DATA2={2:"INIT2",3:"INIT4",4:"ATKHLDV",5:"LFO1VAL",
       6:"ATKHLD",7:"LFO2VAL"}
DATA3={0:"IP",1:"IFATN",2:"PEFE",3:"FMMOD",4:"TREMFRQ",5:"FM2FRQ2"}
# Registers 1 / Data1 use selected *voice* as different global register
# locations, NEVER as regular note-voice envelope values.
GLOBAL1={
    9:"HWCF4",10:"HWCF5",13:"HWCF6",
    20:"SMALR",21:"SMARR",22:"SMALW",23:"SMARW",
    26:"SMLD",29:"HWCF1",30:"HWCF2",31:"HWCF3"}
GLOBAL2={26:"SMRD",27:"WC"}
PORTS={0x620:("DATA0","low"),0x622:("DATA0","high"),
       0xA20:("DATA1","low"),0xA22:("DATA2","word"),
       0xE20:("DATA3","word")}

def interpret(name, raw, complete):
    """Extract *documented* chip fields, not guessed Creative mappings."""
    if name=="DCYSUSV":
        return {"release_phase":bool(raw&0x8000),
                "sustain_7bit":(raw>>8)&0x7f,
                "envelope_engine_disabled":bool(raw&0x80),
                "decay_release_7bit":raw&0x7f}
    if name=="IFATN":
        return {"filter_cutoff_8bit":raw>>8,
                "attenuation_8bit":raw&255}
    if name=="IP":
        return {"raw_pitch_offset":raw-0xE000,
                "nominal_semitones_from_E000":round((raw-0xE000)*12/4096,6)}
    if name=="DCYSUS":
        return {"release_phase":bool(raw&0x8000),
                "sustain_7bit":(raw>>8)&0x7f,
                "decay_release_7bit":raw&0x7f}
    if name=="PTRX" and complete:
        return {"reverb_send_8bit":(raw>>8)&255,
                "pitch_target_16bit":(raw>>16)&65535}
    if name=="PSST" and complete:
        return {"pan_8bit":(raw>>24)&255,
                "loop_start_24bit":raw&0xffffff}
    if name=="CSL" and complete:
        return {"chorus_send_8bit":(raw>>24)&255,
                "loop_end_24bit":raw&0xffffff}
    if name=="CCCA" and complete:
        return {"filter_resonance_Q_4bit":(raw>>28)&15,
                "current_sample_address_24bit":raw&0xffffff}
    if name=="PEFE":
        return {"pitch_envelope_signed_8bit":(raw>>8)&255,
                "filter_envelope_signed_8bit":raw&255}
    return {}

def decode(events):
    pointer=None
    # pair word halves *per selected actual voice and register*. The
    # original driver may write high half before low half; incomplete
    # values MUST NOT be treated as a full 32-bit word.
    halves={}
    writes=[]
    activity=Counter()
    reads=0
    unknown=0
    for index,(frame,direction,port,value) in enumerate(events):
        if direction=="READ":
            reads+=1
            continue
        if port==0xE22:
            pointer=(value&31,(value>>5)&7)
            continue
        info=PORTS.get(port)
        if info is None or pointer is None:
            unknown+=1
            continue
        data,part=info
        voice,reg=pointer
        # A22 is HIGH 16 of Data1 for CCCA / global 32-bit
        # registers, but an independent Data2 word for envelopes.
        if port==0xA22 and (reg==0 or
            (reg==1 and voice in (9,10,13,20,21,22,23))):
            data,part="DATA1","high"
        if data=="DATA0": name=DATA0[reg]
        elif data=="DATA1":
            name=GLOBAL1.get(voice) if reg==1 else DATA1.get(reg)
            if name is None and reg==1:name="UNKNOWN_GLOBAL_DATA1"
        elif data=="DATA2":
            name=GLOBAL2.get(voice) if reg==1 else DATA2.get(reg)
        else:name=DATA3.get(reg)
        if name is None:
            unknown+=1
            continue
        value32=value
        complete=True
        if data=="DATA0" or (data=="DATA1" and (reg==0 or
            (reg==1 and voice in (9,10,13,20,21,22,23)))):
            # Single 16-bit memory registers are not doublewords, and
            # HWCF1/2/3 live on Data1-word.
            key=(voice,reg,data)
            low,high=halves.get(key,(None,None))
            if part=="high":
                high=value
            else:
                low=value
            halves[key]=(low,high)
            complete=low is not None and high is not None
            value32=((high<<16)|low) if complete else value
        fields=interpret(name,value32,complete)
        writes.append({"index":index,"frame":frame,"voice":voice,
                       "register_index":reg,"port":f"{port:03X}",
                       "name":name,"half":part,"value":f"{value:04X}",
                       "complete_32bit":complete,
                       **({"value32":f"{value32:08X}"} if complete and data in ("DATA0","DATA1") and name in
                           ("PTRX","PSST","CSL","CCCA","CPF","CVCF","VTFT","Z1","Z2") else {}),
                       **fields})
        activity[name]+=1
    return {"observed_events":len(events),"read_events":reads,
            "decoded_writes":len(writes),"unclassified_writes":unknown,
            "register_counts":dict(sorted(activity.items())),
            "writes":writes}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--trace",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--min-note-writes",type=int,default=0)
    args=p.parse_args()
    data=decode(read_trace(args.trace))
    if not data["decoded_writes"]:
        raise ValueError("empty/unclassified trace: no register semantics evidence")
    note_related=sum(data["register_counts"].get(name,0) for name in
        ("IP","IFATN","DCYSUSV","CCCA","PTRX","PSST","CSL"))
    if note_related < args.min_note_writes:
        raise ValueError("actual voice parameter registers absent")
    data.update({"source_sha256":"not supplied, bind private manifest separately",
                 "original_Creative_driver_compliance":"NOT INFERRED",
                 "p002_voice_implementation":"NOT PROVEN",
                 "manual":"Creative EMU8000 Programmer's Guide rev 1.00, pp 7-18",
                 "voice_number_is_NOT_midi_channel":True})
    if args.out.exists():
        raise ValueError("refuse to overwrite register evidence")
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(data,indent=2)+"\n",encoding="utf-8")
    print("P002_OBSERVED_REGISTER_SEMANTICS_DECODED",data["decoded_writes"],
          "no_original_driver_equivalence_claim=TRUE")

if __name__=="__main__":
    try:main()
    except (ValueError,OSError) as e:
        print("P002_REGISTER_AUDIT_NO_GO",str(e),file=sys.stderr)
        raise SystemExit(1)
