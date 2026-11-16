#!/usr/bin/env python3
"""P107: supplement OLD x64 86Box JIT observer with executed interpreter path.

The pinned original 86Box emits JIT guest instructions in
src/codegen/codegen_x86-64.c, BUT first-time/other instructions can execute
inside src/cpu/386_dynarec.c via x86_opcodes[...] during recompilation.
The observed original Creative P107 JIT-only private trace had 66 nonadjacent
Jcc successors, so calling that trace "complete" would be a false positive.

This adds an opt-in callback immediately before the actual original
x86_opcodes[...] instruction execution, restricted to the same LCOD window.
A JIT code-generation event is NOT logged as a guest instruction by itself.

Only source-owned copies of the pristine, Git SHA-locked 86Box fork may be
modified; the original EMU8000 chip, user's VxD/Win95/ROM and VST3 are intact.
"""
from pathlib import Path
import argparse
import hashlib
import sys

ORIGINAL_386_DYNAREC_GIT_BLOB="e9765a423b5d675337aab33fd060204e62dc20d7"
ORIGINAL_OLD_JIT_GIT_BLOB="673141762345c1195458bd840461302493f7d365"
OLD_MARKER="P107_OLD_DYNAREC_LCOD_EXECUTED_V1"
NEW_MARKER="P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2"

INTERP_INCLUDE='#include "386_common.h"'
INTERP_DISPATCH='''                codegen_generate_call(opcode, x86_opcodes[(opcode | cpu_state.op32) & 0x3ff], fetchdat, cpu_state.pc, cpu_state.pc - 1);

                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);'''
JIT_CALLBACK_TAIL='''    if ((p107_x86_trace_count & 2047ULL) == 0ULL)
        fflush(p107_x86_trace_file);
}
'''

def blob_sha(raw:bytes)->str:
    raw=raw.replace(b'\r\n',b'\n')
    return hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()

def replace_once(s,original,replacement,tag):
    n=s.count(original)
    if n!=1:raise ValueError(f'{tag} requires one exact pristine source anchor, found {n}')
    return s.replace(original,replacement,1)

def patch_pair(pristine_dynarec:bytes,already_p107_codegen:bytes,
               require_original_pin=True):
    if require_original_pin and blob_sha(pristine_dynarec)!=ORIGINAL_386_DYNAREC_GIT_BLOB:
        raise ValueError('Pinned original src/cpu/386_dynarec.c SHA mismatch')
    native=pristine_dynarec.replace(b'\r\n',b'\n').decode('utf8')
    code=already_p107_codegen.replace(b'\r\n',b'\n').decode('utf8')
    if OLD_MARKER not in code:
        raise ValueError('P107 old x64 source execution probe missing')
    if NEW_MARKER in native or NEW_MARKER in code:
        raise ValueError('P107 original interpreter fallback already injected')

    decl='''#if defined(USE_DYNAREC) && !defined(USE_NEW_DYNAREC) && (defined(__amd64__) || defined(_M_X64))
/* P107 original OLD x64 Dynarec interpreter callback; public source-only. */
extern void p107_old_dynarec_interpreter_exec(uint32_t, uint32_t, uint8_t);
#endif
'''
    native=replace_once(native,INTERP_INCLUDE,INTERP_INCLUDE+'\n'+decl,'86Box old CPU include')
    native=replace_once(native,INTERP_DISPATCH,INTERP_DISPATCH.replace(
        '                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);',
        '''#if defined(USE_DYNAREC) && !defined(USE_NEW_DYNAREC) && (defined(__amd64__) || defined(_M_X64))
                /* This is a real guest instruction, interpreted on first
                 * compilation; logging speculative generation alone is wrong. */
                p107_old_dynarec_interpreter_exec(cpu_state.pc - 1u, EBP, opcode);
#endif
                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);'''),
        '86Box old Dynarec interpreted instruction dispatch')

    supplement='''/* P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2
 * This global symbol is called ONLY by the pinned old Dynarec interpreter
 * after code generation, BEFORE its real guest x86 opcode execution.
 * It forwards to the exact same executed-instruction recorder as the JIT.
 * No guest memory, chip, register values or original code is modified.
 */
void
p107_old_dynarec_interpreter_exec(uint32_t pc, uint32_t actual_ebp, uint8_t opcode)
{
    if (p107_x86_in_original_lcod(pc, opcode))
        p107_x86_executed_guest_pc(pc, actual_ebp);
}
'''
    code=replace_once(code,JIT_CALLBACK_TAIL,JIT_CALLBACK_TAIL+'\n'+supplement,
                      'old JIT executed instruction logger')
    return native.encode(),code.encode()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-dynarec',type=Path,required=True)
    ap.add_argument('--copy-dynarec',type=Path,required=True)
    ap.add_argument('--copy-old-codegen',type=Path,required=True)
    ap.add_argument('--selftest-only',action='store_true')
    args=ap.parse_args()
    try:
        original=args.original_dynarec.read_bytes()
        copied=args.copy_dynarec.read_bytes()
        if blob_sha(original)!=blob_sha(copied):
            raise ValueError('disposable interpreter copy not equal to source pristine')
        original_codegen=args.copy_old_codegen.read_bytes()
        patched_dynarec,patched_codegen=patch_pair(original,original_codegen)
        if args.copy_dynarec.resolve()==args.original_dynarec.resolve():
            raise ValueError('refuse to mutate pinned submodule original')
        if not args.selftest_only:
            args.copy_dynarec.write_bytes(patched_dynarec)
            args.copy_old_codegen.write_bytes(patched_codegen)
        print('P107_OLD_DYNAREC_INTERPRETER_EXECUTED_CALLBACK_SOURCE_PASS '
              'original_chip_untouched=TRUE original_guest_executed=FALSE '
              'prior_JIT_only_66_missing_successors_not_explained_yet=TRUE '
              'new_old_dynarec_guest_exec_runtime=NOT_TESTED')
        return 0
    except (OSError,ValueError,UnicodeError) as e:
        print('P107_INTERPRETER_FALLBACK_NO_GO '+str(e),file=sys.stderr)
        return 2

if __name__=='__main__':
    sys.exit(main())
