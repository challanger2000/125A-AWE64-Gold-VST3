#!/usr/bin/env python3
"""Read-only OMF fingerprint inventory of *archival* Creative DOS libraries.

Produces hashes of 16-byte CODE/DATA fragments; never copies original code
bytes into output. A hash match proves an identical byte sequence; absence
does not prove algorithms differ (relocations/compilers/optimizations).
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def idx(data, p):
    n=data[p];p+=1
    if n & 128:
        v=((n&127)<<8)|data[p]
        return v,p+1
    return n,p

def iter_omf(path):
    data=Path(path).read_bytes()
    if data[0]!=0xf0:raise ValueError('Not OMF LIBRARY')
    page=3+struct.unpack_from('<H',data,1)[0]
    if page not in (16,32,64,128,256,512,1024,2048,4096):
        raise ValueError('Unsupported page size')
    p=page;module='';chunks=[]
    while p<len(data):
        t=data[p]
        if t==0xF1:break
        if t in (0,255):
            p=(p//page+1)*page
            continue
        if p+3>len(data):raise ValueError('Broken record')
        length=struct.unpack_from('<H',data,p+1)[0]
        end=p+3+length
        if length<1 or end>len(data):raise ValueError('Truncated OMF record')
        record=data[p:end]
        if sum(record)&255:raise ValueError('OMF checksum disagrees')
        payload=record[3:-1]
        if t in (0x80,0x82):
            name_len=payload[0]
            module=payload[1:1+name_len].decode('cp437','replace')
            chunks=[]
        elif t in (0xa0,0xa1):
            seg,pos=idx(payload,0)
            if t==0xa0:
                offset=struct.unpack_from('<H',payload,pos)[0];pos+=2
            else:
                offset=struct.unpack_from('<I',payload,pos)[0];pos+=4
            source=payload[pos:]
            if source:chunks.append((seg,offset,source))
        if t in (0x8a,0x8b):
            yield module,chunks
            p=((end+page-1)//page)*page
        else:p=end

def fingerprint(path, name):
    raw=Path(path).read_bytes()
    mods=[]
    for mod,chunks in iter_omf(path):
        records=[]
        for seg,off,source in chunks:
            # One-way 64-bit BLAKE2s fingerprints; small false-positive risk,
            # confirm any match against original files before a parity claim.
            sig=[hashlib.blake2s(source[i:i+16],digest_size=8).hexdigest()
                 for i in range(max(0,len(source)-15))]
            records.append({'segment':seg,'offset':off,'bytes':len(source),
                            'f16':sig})
        mods.append({'module':mod,'chunks':records})
    return {'name':name,'sha256':hashlib.sha256(raw).hexdigest(),
            'modules':mods}

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--sf2',required=True)
    p.add_argument('--sbk',required=True)
    p.add_argument('--output',required=True)
    a=p.parse_args()
    r={'schema':'125A-P003-ORIGINAL-CREATIVE-OMF-FINGERPRINT-V1',
       'note':'F16 hashes only; no Creative binary code bytes included',
       'libraries':[fingerprint(a.sf2,'Creative SF2 32-bit flat'),
                    fingerprint(a.sbk,'Creative SBK 32-bit flat')]}
    Path(a.output).write_text(json.dumps(r,separators=(',',':'))+'\n',encoding='utf8')
    for lib in r['libraries']:
        print('ORIGINAL_OMF_FINGERPRINT',lib['name'],
              'modules',len(lib['modules']),
              'chunks',sum(len(m['chunks']) for m in lib['modules']),
              'sha256',lib['sha256'])
