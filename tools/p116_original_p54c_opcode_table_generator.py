#!/usr/bin/env python3
"""P116: pinned-public-original 86Box P54C opcode lookup oracle generator.

Compiles only exact git-blob-pinned original STATIC C table initializers and
the original table-selection switch, using a private temporary C program.
Emits derived numeric C++23 research tables and exhaustive public vectors.
DOES NOT decode executed guest bytes, model branches, run Win95, or alter DSP.
Research only; original 86Box fork licenses must be reviewed for any product use.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile

PINS={
 'pentium':'aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4',
 'common':'0c538fc4e87ef6d4dd26ad8b74c2b556c94a09d8',
 'header':'3d28a89deb2c10ee011d5c490f01cab971744b9d',
}
ARRAY_RE=re.compile(
 r'\b(?:static\s+)?uint64_t\s+(opcode_(?:timings_p6|deps)[A-Za-z0-9_]*)'
 r'\s*\[\s*(\d+)\s*\]\s*=\s*\{[\s\S]*?\};')
FETCH=(0x00,0x07,0x08,0x38,0x40,0x44,0x78,0x80,
       0x87,0xb8,0xc0,0xc4,0xc7,0xd8,0xf8,0xff)
PREFIX=(0x00,0x0f,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf)

def git_blob(raw:bytes)->str:
    clean=raw.replace(b'\r\n',b'\n')
    return hashlib.sha1(b'blob '+str(len(clean)).encode()+b'\x00'+clean).hexdigest()

def pinned(path:Path,which:str)->str:
    raw=path.read_bytes()
    if git_blob(raw)!=PINS[which]:
        raise ValueError('P116 original '+which+' git blob SHA mismatch')
    return raw.replace(b'\r\n',b'\n').decode('utf-8')

def arrays(src:str,kind:str)->dict[str,tuple[int,str]]:
    found={}
    for m in ARRAY_RE.finditer(src):
        name=m.group(1)
        if not name.startswith(kind): continue
        if name in found:raise ValueError('P116 duplicate original array '+name)
        found[name]=(int(m.group(2)),m.group(0))
    return found

def macros(src:str,limit:str)->str:
    part=src.split(limit,1)[0]
    results=[]
    for line in part.splitlines():
        if line.startswith('#define '):
            if line.rstrip().endswith('\\'):
                raise ValueError('P116 unhandled multiline original macro')
            results.append(line)
    return '\n'.join(results)

def original_switch(src:str)->str:
    startfn=src.find('codegen_timing_pentium_opcode(uint8_t opcode')
    if startfn<0:raise ValueError('P116 original Pentium function anchor absent')
    start=src.find('    switch (last_prefix) {',startfn)
    end=src.find('\n    if (u_pipe_full)',start)
    if start<0 or end<start or 'opcode_deps_ff' not in src[start:end]:
        raise ValueError('P116 original opcode table selection changed')
    return src[start:end]

def generate(original_pentium:Path,original_common:Path,original_header:Path,
             output_header:Path,output_vectors:Path,cc:str):
    p=pinned(original_pentium,'pentium')
    c=pinned(original_common,'common')
    h=pinned(original_header,'header')
    t=arrays(p,'opcode_timings_p6')
    d=arrays(c,'opcode_deps')
    if len(t)!=32 or len(d)!=36:
        raise ValueError('P116 original source array inventory changed')
    for n in t:
        dependency=n.replace('opcode_timings_p6','opcode_deps')
        if dependency not in d or t[n][0]!=d[dependency][0]:
            raise ValueError('P116 original timing/dependency table size mismatch '+n)
    defs=macros(p,'static int       u_pipe_full;')+'\n'+macros(h,'extern uint64_t opcode_deps[256];')
    compiled_arrays='\n'.join(z[1] for z in list(t.values())+list(d.values()))
    dump_tables='\n'.join(
        f'    printf("T {name} {size}"); '
        f'for (int j=0;j<{size};++j) '
        f'printf(" %016llX",(unsigned long long){name}[j]); '
        'puts("");'
        for name,(size,_) in list(t.items())+list(d.items()))
    # Use EXACT original table-selection switch. No retyped source oracle.
    original=original_switch(p)
    cc_src=(
        '#include <stdio.h>\n#include <stdint.h>\n'+defs+'\n'+compiled_arrays+
        '\nstatic void original_select(unsigned input_prefix, uint8_t opcode, '
        'uint32_t fetchdat, uint64_t *timing, uint64_t *dependency, '
        'unsigned *selected) {\n'
        '    uint64_t *timings; uint64_t *deps;\n'
        '    int mod3=((fetchdat & 0xc0)==0xc0);\n'
        '    unsigned last_prefix=input_prefix;\n'+original+'\n'
        '    *timing=timings[opcode];\n'
        '    *dependency=deps[opcode];\n'
        '    *selected=opcode;\n'
        '}\nint main(void) {\n'+dump_tables+
        '\n  const unsigned prefixes[]={'+','.join(str(x) for x in PREFIX)+'};'
        '\n  const unsigned fetches[]={'+','.join(str(x) for x in FETCH)+'};'
        '\n  for (unsigned p=0;p<sizeof(prefixes)/sizeof(prefixes[0]);++p)'
        '\n    for (unsigned op=0;op<256;++op)'
        '\n      for (unsigned f=0;f<sizeof(fetches)/sizeof(fetches[0]);++f) {'
        '\n        uint64_t tval=0,dval=0;unsigned selected=0;'
        '\n        original_select(prefixes[p],(uint8_t)op,fetches[f],'
        '&tval,&dval,&selected);'
        '\n        printf("V %02X %02X %08X %016llX %016llX %02X\\n",'
        'prefixes[p],op,fetches[f],(unsigned long long)tval,'
        '(unsigned long long)dval,selected);'
        '\n      }\n  return 0;\n}\n')
    with tempfile.TemporaryDirectory(prefix='p116-public-original-') as tmp:
        source=Path(tmp)/'original_tables.c'
        executable=Path(tmp)/'original_tables.exe'
        source.write_text(cc_src,encoding='utf-8')
        cmd=[cc,'-std=c11','-O2',str(source),'-o',str(executable)]
        cp=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
        if cp.returncode:
            raise ValueError('P116 original C source compile failed: '+cp.stderr[-4000:])
        run=subprocess.run([str(executable)],capture_output=True,text=True,timeout=90)
        if run.returncode:
            raise ValueError('P116 original C table oracle execution failed: '+run.stderr[-1000:])
    arrays_read={}
    vectors=[]
    for line in run.stdout.splitlines():
        f=line.split()
        if f[0]=='T':
            name,number=f[1],int(f[2])
            if name in arrays_read or number!=len(f)-3:
                raise ValueError('P116 original C array output malformed')
            arrays_read[name]=[int(n,16) for n in f[3:]]
        elif f[0]=='V':
            if len(f)!=7:raise ValueError('P116 original C vector field count')
            vectors.append(line[2:])
        else:raise ValueError('P116 unknown original oracle record')
    if len(arrays_read)!=68 or len(vectors)!=len(PREFIX)*256*len(FETCH):
        raise ValueError('P116 original source tables/vector coverage incomplete')
    head=(
      '#pragma once\n'
      '// P116 RESEARCH ONLY: derived numeric tables at BUILD TIME from exact\n'
      '// GPL-licensed 86Box-AWE32 v6.0-AWE32-0.93 source (git blob SHA pinned).\n'
      '// Not original Creative ROM/driver bytes. NOT authorized for product.\n'
      '#include <array>\n#include <cstdint>\n'
      'namespace awe64::research_p116::data {\n')
    for name,(size,_) in list(t.items())+list(d.items()):
        values=arrays_read[name]
        if len(values)!=size:raise ValueError('P116 size mismatch '+name)
        head+=f'inline constexpr std::array<std::uint64_t,{size}> {name}{{\n'
        for i in range(0,size,4):
            head+='    '+', '.join(f'0x{v:016x}ull' for v in values[i:i+4])+',\n'
        head+='};\n'
    head+='}\n'
    if output_header.resolve() in (original_pentium.resolve(),original_common.resolve(),
                                   original_header.resolve()):
        raise ValueError('P116 refuses to replace original source')
    output_header.parent.mkdir(parents=True,exist_ok=True)
    output_vectors.parent.mkdir(parents=True,exist_ok=True)
    output_header.write_text(head,encoding='utf-8')
    output_vectors.write_text('\n'.join(vectors)+'\n',encoding='ascii')
    print('P116_PINNED_ORIGINAL_86BOX_P54C_SOURCE_TABLES_PASS '
          f'tables={len(arrays_read)} vectors={len(vectors)} '
          f'output_header_sha256={hashlib.sha256(head.encode()).hexdigest()} '
          'actual_guest_execution=NOT_TESTED product_integration=NO')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-pentium',type=Path,required=True)
    ap.add_argument('--original-common',type=Path,required=True)
    ap.add_argument('--original-header',type=Path,required=True)
    ap.add_argument('--output-header',type=Path,required=True)
    ap.add_argument('--output-vectors',type=Path,required=True)
    ap.add_argument('--cc',default='cc')
    a=ap.parse_args()
    generate(a.original_pentium,a.original_common,a.original_header,
             a.output_header,a.output_vectors,a.cc)
if __name__=='__main__':main()
