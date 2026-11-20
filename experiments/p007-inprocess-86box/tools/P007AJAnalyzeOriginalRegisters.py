#!/usr/bin/env python3
"""Read owner-private bounded EMU8000 register CSV from P007AJ test.
Pure offline decode; never opens ROMs, Win95 images or uploads anything.
No "audio verified" assertion from register differences alone.
"""
import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path

PORTS = {
  0x620: ("CPF", "PTRX", "CVCF", "VTFT", "Z2", "Z1", "PSST", "CSL"),
  0x622: ("CPF", "PTRX", "CVCF", "VTFT", "Z2", "Z1", "PSST", "CSL"),
  0xA20: ("CCCA", "HWCF", "INIT1", "INIT3",
           "ENVVOL", "DCYSUSV", "ENVVAL", "DCYSUS"),
  0xA22: ("CCCA", "HWCF", "INIT2", "INIT4",
           "ATKHLDV", "LFO1VAL", "ATKHLD", "LFO2VAL"),
  0xE20: ("IP", "IFATN", "PEFE", "FMMOD",
           "TREMFRQ", "FM2FRQ2", "UNK6", "UNK7"),
}
ENVELOPE={"ENVVAL","DCYSUS","ATKHLD","FMMOD","ENVVOL","DCYSUSV","ATKHLDV"}

def read(path):
    result=defaultdict(list)
    with path.open(encoding="utf-8-sig",newline="") as f:
        reader=csv.DictReader(f)
        expected={"scenario","scenario_name","serial","chip_frame_44100",
                  "io_port_hex","register_word_hex"}
        if not expected.issubset(reader.fieldnames or []):
            raise ValueError("P007AJ CSV schema incomplete")
        for row in reader:
            index=int(row["scenario"])
            if index<0 or index>=14:
                raise ValueError("unknown original fourteen-note scenario")
            result[index].append((row["scenario_name"],
                   int(row["serial"]),
                   int(row["chip_frame_44100"]),
                   int(row["io_port_hex"],16),
                   int(row["register_word_hex"],16)))
            if len(result[index])>8192:
                raise ValueError("unbounded trace; reject instead of truncating")
    return result

def decode(rows):
    cur_reg=cur_voice=None
    values=Counter()
    with_voice=defaultdict(set)
    unknown=0
    for _,_,_,port,value in sorted(rows,key=lambda x:x[1]):
        if port==0xE22:
            cur_reg=(value>>5)&7
            cur_voice=value&31
            continue
        if port not in PORTS:
            unknown+=1
            continue
        if cur_reg is None:
            unknown+=1
            continue
        label=PORTS[port][cur_reg]
        # Each register may have low/high halves at different ports;
        # report component port separately, rather than splicing wrong pairs.
        label=f"{label}@{port:03X}"
        values[(label,value)]+=1
        with_voice[label].add(cur_voice)
    return values, with_voice, unknown

def fmt(counter,limit=7):
    return ", ".join(f"{name}={value:04X}x{count}"
                     for (name,value),count in counter.most_common(limit)) or "-"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv",type=Path)
    args=ap.parse_args()
    rows=read(args.csv)
    print("P007AJ ORIGINAL 86BOX EMU8000 14-NOTE REGISTER ANALYSIS")
    print("Counts are original voice-programming I/O, NOT automatically proof of audible effects.")
    decoded={}
    for index in sorted(rows):
        data=rows[index]
        vals,voices,unknown=decode(data)
        decoded[index]=vals
        key=Counter({k:v for k,v in vals.items() if k[0].split("@")[0] in ENVELOPE})
        print(f"take={index:02d} name={data[0][0]} writes={len(data)}"
              f" register_words={sum(key.values())} undeclared_ports={unknown}")
        print("  filter ENV registers: "+fmt(key))
    print("P007AJ ORIGINAL FILTER ENV pairs; register differences are NOT equivalent to valid NRPN receipt.")
    for a,b,back,title in ((4,5,6,"ATTACK"),(6,7,8,"DECAY"),
                           (8,9,10,"SUSTAIN"),(10,11,12,"RELEASE")):
        av=decoded.get(a,Counter())
        bv=decoded.get(b,Counter())
        cv=decoded.get(back,Counter())
        reg=lambda ctr:Counter({k:n for k,n in ctr.items()
                                      if k[0].split("@")[0] in ENVELOPE})
        aenv,benv,cenv=map(reg,(av,bv,cv))
        added=benv-aenv
        removed=aenv-benv
        restored=cenv==aenv
        print(f"{title}: B_minus_A={fmt(added)}"
              f" A_minus_B={fmt(removed)}"
              f" A_return_register_values_exact={int(restored)}"
              f" records_A={sum(aenv.values())} B={sum(benv.values())}")
    print("P007AJ_ORIGINAL_REGISTER_DIFFERENTIAL_ANALYSIS_COMPLETED")
if __name__=="__main__":
    main()
