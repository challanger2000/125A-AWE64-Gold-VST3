#!/usr/bin/env python3
"""PRIVATE P003 audit: real Creative mode0 voice30/31 selection and voice-start.

Read two owner-provided Win95 RESULT ZIPs. Fail closed on exact original
SHA-256, source return PCs and original EMU8000 port sequences. Never upload
the private ZIP, trace, ROM, VXD or any snippets of private bytes to GitHub.

Real original SBAWE.VXD LCOD:
+3F13 : VTFT value read via original EMU8000 helper
+3FCE : selected physical voice DCYSUSV=0x00FF via E22/A20
+3E0C : caller's initial VTFT=0x0000FFFF via E22/620 DWORD
+3ED4 : dynamic physical-voice scan bound [device+0x06] = 32
+3F8C : advance by 3 voices in lane; final third lane ends with voice29
"""
import hashlib
import json
import sys
import zipfile
from collections import Counter
from pathlib import Path

B=0xC10151E4
ORIGINAL=[
    {
        'sha256':'6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944',
        'selected30':101,'selected31':117,
        'ranked_after_voice29_30':33,'ranked_after_voice29_31':50
    },
    {
        'sha256':'28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c',
        'selected30':108,'selected31':122,
        'ranked_after_voice29_30':40,'ranked_after_voice29_31':56
    }
]

def sha256(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for part in iter(lambda:f.read(1<<20),b''):
            h.update(part)
    return h.hexdigest()

def audit(path,expected):
    if sha256(path)!=expected['sha256']:
        raise ValueError('Not the independently SHA-pinned original owner trace')
    counts=Counter()
    last_vtft=None
    followup={} # sequence -> expected (voice, original source site, port, value)
    with zipfile.ZipFile(path) as z,z.open('Original-CPU-IO.trace') as f:
        if not f.readline().startswith(b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK '):
            raise ValueError('Unrecognized genuine original CPU I/O capture')
        for line in f:
            if not line.startswith(b'W '):continue
            x=line.split()
            if len(x)!=17:continue
            seq=int(x[7]);site=int(x[9],16)-B
            port=int(x[5],16);word=int(x[6],16)
            if seq in followup:
                v,required_site,required_port,required_word=followup.pop(seq)
                if (site,port,word)!=(required_site,required_port,required_word):
                    raise ValueError('Original selected voice did not receive expected immediate VTFT write')
                counts['voice%d_verified_write_%04x'%(v,required_port)]+=1
            if site==0x3f13 and port==0xe22:
                voice=word&31
                if word!=(0x60|voice) or x[4]!=b'C1018F3F':
                    raise ValueError('Original source VTFT selector inconsistent')
                last_vtft=(voice,seq)
            elif site==0x3fce and port==0xe22:
                voice=word&31
                if word!=(0xa0|voice) or x[4]!=b'C1018EAB':
                    raise ValueError('Original source release selector inconsistent')
                if voice not in (30,31):continue
                counts['selected%d'%voice]+=1
                if last_vtft:
                    previous_voice,previous_seq=last_vtft
                    if previous_voice==29 and 0<seq-previous_seq<=12:
                        counts['ranked_after_voice29_%d'%voice]+=1
                # Verify the genuine original driver's NEXT 4 port writes,
                # including selection E22 0x7E/0x7F and DATA0 DWORD 0xFFFF:
                next_words=(
                    (0x3fce,0xa20,0x00ff),
                    (0x3e0c,0xe22,0x60|voice),
                    (0x3e0c,0x620,0xffff),
                    (0x3e0c,0x622,0)
                )
                for i,(src,prt,value) in enumerate(next_words,1):
                    if seq+i in followup:
                        raise ValueError('Overlapping unmatched original release')
                    followup[seq+i]=(voice,src,prt,value)
    if followup:
        raise ValueError('Truncated original register init after release')
    for k,n in expected.items():
        if k=='sha256':continue
        if counts[k]!=n:
            raise ValueError(f'Original-session {k}: {counts[k]} != pinned {n}')
    for v in (30,31):
        for port,required in [(0xa20,1),(0xe22,1),(0x620,1),(0x622,1)]:
            if counts['voice%d_verified_write_%04x'%(v,port)]!=counts['selected%d'%v]*required:
                raise ValueError('Original post-selection hardware initialization missing')
    return {k:counts[k] for k in expected if k!='sha256'}

def main():
    if len(sys.argv)!=3:
        raise SystemExit('usage: audit ORIGINAL-OLDER-RESULT.zip ORIGINAL-NEWER-RESULT.zip')
    outputs=[audit(Path(path),pinned) for path,pinned in zip(sys.argv[1:],ORIGINAL)]
    print(json.dumps({
        'status':'P003_ORIGINAL_MODE0_30_31_SELECT_RANK_AND_INIT_PASS',
        'sessions':outputs,
        'original_emu8000_source_modified':False,
        'private_bytes_exported':False
    },sort_keys=True))

if __name__=='__main__':
    main()
