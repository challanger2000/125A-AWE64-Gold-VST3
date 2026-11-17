#!/usr/bin/env python3
"""P003 PRIVATE original Win95 evidence: MODE0 physical voices 30 and 31.

Run against both owner-provided SHA-pinned RESULT archives; never upload
original guest traces, Creative VXD, ROM, SF2, WAV or private original data.
This tool streams the compressed trace and prints aggregate counters only.

Authentic Win95 Creative LCOD instruction offsets:
  +3ED4: MOVZX ECX, WORD [ESI+0x06] (physical scan bound)
  +3F13: return after original EMU8000 VTFT read; E22 voice pointer 0x60|v
  +3FCE: return after original DCYSUSV release; E22 voice ptr 0xA0|v
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import zipfile

BASE=0xC10151E4
EXPECTED={
 'older':{
  'sha256':'6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944',
  'vtft_total':91138,'vtft_30':2686,'vtft_31':2649,
  'release_total':5878,'release_30':101,'release_31':117
 },
 'newer':{
  'sha256':'28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c',
  'vtft_total':92278,'vtft_30':2688,'vtft_31':2625,
  'release_total':5878,'release_30':108,'release_31':122
 }
}
SITES={0x3f13:('vtft',0x60,'C1018F3F'),0x3fce:('release',0xA0,'C1018EAB')}
def hash_file(path):
 h=hashlib.sha256()
 with open(path,'rb') as f:
  for part in iter(lambda:f.read(1<<20),b''):h.update(part)
 return h.hexdigest()

def audit(path,which):
 if hash_file(path)!=EXPECTED[which]['sha256']:
  raise ValueError('Owner archive SHA-256 mismatch: refuse unverified trace')
 data=Counter()
 seen={x:set() for x in ('vtft','release')}
 with zipfile.ZipFile(path) as z:
  with z.open('Original-CPU-IO.trace') as f:
   if not f.readline().startswith(b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK '):
    raise ValueError('Missing genuine original CPU source marker')
   for line in f:
    if not line.startswith(b'W '):continue
    tokens=line.split()
    if len(tokens)!=17 or tokens[5]!=b'0E22':continue
    site=int(tokens[9],16)-BASE
    if site not in SITES:continue
    label,bits,helper_eip=SITES[site]
    if tokens[4].decode()!=helper_eip:
     raise ValueError('Unexpected 86Box original helper PC')
    word=int(tokens[6],16)
    voice=word&31
    if word!=(bits|voice):
     raise ValueError('Source returns not a genuine 32-voice selector')
    data[label+'_total']+=1
    data[f'{label}_{voice}']+=1
    seen[label].add(voice)
 for label in seen:
  if seen[label]!=set(range(32)):
   raise ValueError(f'Original {label} did not reach all 32 voices')
 expected=EXPECTED[which]
 for key,value in expected.items():
  if key=='sha256':continue
  if data[key]!=value:
   raise ValueError(f'Original {which} {key}: {data[key]} != {value}')
 return {k:data[k] for k in expected if k!='sha256'}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--older',type=Path,required=True)
 p.add_argument('--newer',type=Path,required=True)
 args=p.parse_args()
 results={kind:audit(getattr(args,kind),kind)
          for kind in ('older','newer')}
 print(json.dumps({'status':'P003_ORIGINAL_WIN95_MODE0_32_PHYSICAL_VOICES_PASS',
       'original_driver_bytes_published':False,
       'original_guest_trace_bytes_published':False,
       'values':results},sort_keys=True,indent=2))
if __name__=='__main__':main()
