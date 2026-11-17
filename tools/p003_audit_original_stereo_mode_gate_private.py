#!/usr/bin/env python3
"""PRIVATE P003 source-evidence auditor, never a production timing algorithm.

Pass two owner-provided Win95 V5 result ZIPs. Reads original Creative allocator
caller/row states and CCCA/VTFT port events; emits aggregate evidence only.
The old formula and the new +14/+36 candidate are EMPIRICAL, scope limited.
Do not upload private driver/SF2/ROM/EMU8000 traces to GitHub.
"""
import argparse
from collections import Counter
import hashlib
from pathlib import Path
import zipfile

ORIGINAL_BASE=0xC10151E4
SITES={0x4065:'V',0x40A7:'D',0x40C4:'C',0x414F:'F1',0x415D:'F2'}
SHA={
    'older':'6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944',
    'newer':'28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c',
}
LAST_ROW=0xC1180E1C

def research_prediction(base, mode, next_flag):
    """Observed source-scoped research candidate; NOT exact CPU cycles."""
    return base if mode>2 else base+14+(36 if next_flag else 0)

def checksum(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for part in iter(lambda:f.read(1048576),b''):h.update(part)
    return h.hexdigest()

def audit_owner(path,expected):
    if checksum(path)!=expected:raise ValueError('Unrecognized private owner ZIP checksum')
    caller,voice,events={},{},[]
    with zipfile.ZipFile(path) as z:
        if z.testzip():raise ValueError('Damaged owner evidence archive')
        with z.open('Original-CPU-IO.trace') as f:
            if not f.readline().startswith(b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK '):
                raise ValueError('Missing original V4 CPU trace')
            for line in f:
                if line.startswith(b'# P003_V5_CALLER_FRAME '):
                    t=line.split();caller[int(t[2])]=t;continue
                if line.startswith(b'# P003_V5_ROW_STATE '):
                    t=line.split();voice[int(t[2])]=t;continue
                if line.startswith(b'#'):continue
                t=line.split()
                if len(t)!=17 or t[0]!=b'W' or t[5]!=b'0E22':continue
                site=int(t[9],16)-ORIGINAL_BASE
                if site in SITES:
                    events.append((SITES[site],int(t[7]),int(t[1]),int(t[14],16)))
    runs=[];current=[]
    for event in events:
        current.append(event)
        if event[0]=='F2':runs.append(current);current=[]
    if len(runs)!=25:raise ValueError(f'Expected 25 original stereo scans, found {len(runs)}')
    stable=[]
    for runid,run in enumerate(runs,1):
        if runid<17:continue
        for x,y in zip(run,run[1:]):
            if x[0]!='C' or y[0]!='V':continue
            seq=x[1]+2
            a,b=caller.get(seq),voice.get(seq)
            if not a or not b or int(a[9],16)!=255 or int(b[7],16)!=63:
                raise ValueError('Invalid original caller/VoiceRow RAM evidence')
            if int(a[4],16)!=ORIGINAL_BASE+0x40C4 or int(b[4],16)!=ORIGINAL_BASE+0x40C4:
                raise ValueError('Wrong original CCCA call site')
            if int(a[5],16)!=x[3] or int(b[5],16)!=x[3]:
                raise ValueError('Unmatched original VoiceRow EBX')
            previous,current_score,best=(int(a[i],16) for i in (11,10,12))
            penalty=int(b[10],16)&1
            if penalty or previous+current_score==0:
                raise ValueError('Research formula applied outside its source-valid path')
            candidate=previous+current_score+2*max(previous,current_score)
            baseline=93+2*int(candidate<=best)-2*int(previous<current_score)
            mode,next_flag=(int(a[i],16) for i in (13,14))
            delta_ebx=x[3]-y[3]
            voice_nr=(LAST_ROW-x[3])//32
            if delta_ebx<=0 or delta_ebx%32 or (LAST_ROW-x[3])%32 or not 0<=voice_nr<=31:
                raise ValueError('Original VoiceRow stride / voice index invalid')
            measured=y[2]-x[2]
            stable.append((runid,mode,voice_nr,delta_ebx//32,next_flag,
                           measured-baseline,measured-research_prediction(baseline,mode,next_flag)))
    if len(stable)!=243:raise ValueError(f'Expected 243 stable original intervals, got {len(stable)}')
    return stable

def summarize(rows,label):
    print(label,'mode',dict(Counter(x[1] for x in rows)),
          'legacy_diff',dict(Counter(x[5] for x in rows)),
          'scoped_diff',dict(Counter(x[6] for x in rows)),
          'next_flag_step',dict(Counter((x[4],x[3]) for x in rows)))
    return sum(x[6]==0 for x in rows)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--older',type=Path,required=True)
    p.add_argument('--newer',type=Path,required=True)
    x=audit_owner(p.parse_args().older,SHA['older'])
    # Get separately so both originals are checked against their own hashes.
    args=p.parse_args()
    y=audit_owner(args.newer,SHA['newer'])
    a,b=summarize(x,'OLDER'),summarize(y,'NEWER')
    assert a==243 and b==236
    assert Counter(k[1] for k in x)=={4:243}
    assert Counter(k[1] for k in y)=={2:243}
    assert Counter((k[4],k[3]) for k in y)=={(0,1):234,(1,3):9}
    assert [(k[0],k[2],k[6]) for k in y if k[6]]==[(r,4,3) for r in range(17,24)]
    print('P003_STEREO_ORIGINAL_MODE_GATE_RESEARCH_PASS 479/486 matched; '
          'seven_mode2_voice4_3_cycle_residual=UNRESOLVED '
          'exact_cpu_model=NOT_PROVEN production_code_modified=NO')

if __name__=='__main__':main()
