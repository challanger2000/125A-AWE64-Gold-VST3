#!/usr/bin/env python3
"""P184: genuine Win32 two-editor close/reopen test on the real native VST3.

Uses the pinned Steinberg EditorHost with a CI-only window-lifecycle hook.
Production VST3 code is not patched. The original stock EditorHost exits on
either close, so unmodified stock behavior cannot exercise this contract.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time

from P165NativeEditorHostQA import (
    u, ensure_full_desktop, get_editors, snap, drag, diff_pixels, check
)

WM_CLOSE=0x0010
MASTER=(817,353,951,487)

def run(editor, plugin, out, records):
    ensure_full_desktop()
    stdout=(out/"editorhost-stdout.log").open("wb")
    stderr=(out/"editorhost-stderr.log").open("wb")
    process=None
    try:
        process=subprocess.Popen(
            [str(editor),"--componentHandler","--secondWindow",str(plugin)],
            cwd=str(editor.parent),stdout=stdout,stderr=stderr)
        stdout.close()
        stderr.close()
        windows=[]
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("P184 EditorHost exited during initial two-view attach")
            windows=get_editors(process.pid)
            if len(windows)==2:
                break
            time.sleep(.2)
        check(len(windows)==2,"P184 two genuine native EditorHost windows",records)
        a,b=windows
        check(all(u.GetWindow(hwnd,5) for hwnd in windows),
              "P184 both windows contain actual native VSTGUI children",records)

        def capture(hwnd,name):
            check(bool(u.SetWindowPos(hwnd,None,0,0,0,0,0x0001|0x0002)),
                  f"P184 bring {name} to front",records)
            u.SetCursorPos(0,0)
            time.sleep(.35)
            return snap(hwnd,out/f"{name}.bmp")

        original_a=capture(a,"first-before")
        original_b=capture(b,"peer-before")
        drag(a,886,422,885,480)
        changed_a=capture(a,"first-master-changed")
        changed_b=capture(b,"peer-master-changed")
        check(diff_pixels(original_a,changed_a,MASTER)>70,
              "P184 initial master gesture changes original view",records)
        check(diff_pixels(changed_a,changed_b,MASTER)==0,
              "P184 initial controller sync across both live views",records)
        check(bool(u.PostMessageW(a,WM_CLOSE,0,0)),
              "P184 close first editor while second remains attached",records)
        new_windows=[]
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("P184 host died when first of two editors closed")
            new_windows=get_editors(process.pid)
            if len(new_windows)==2 and b in new_windows:
                break
            time.sleep(.2)
        check(len(new_windows)==2 and b in new_windows,
              "P184 original peer survived and a new editor reopened",records)
        c=next(hwnd for hwnd in new_windows if hwnd!=b)
        check(bool(u.GetWindow(c,5)),
              "P184 reopened third editor actually attached native VSTGUI",records)
        surviving=capture(b,"surviving-peer")
        reopened=capture(c,"reopened-third")
        check(diff_pixels(surviving,reopened,MASTER)==0,
              "P184 reopened editor restores actual master state",records)
        drag(c,885,480,885,417)
        updated_c=capture(c,"reopened-master-changed")
        updated_b=capture(b,"original-peer-after-reopened-edit")
        check(diff_pixels(reopened,updated_c,MASTER)>70,
              "P184 new editor gesture changes the live controller",records)
        check(diff_pixels(updated_c,updated_b,MASTER)==0,
              "P184 original surviving peer mirrors reopened editor",records)
        check(bool(u.PostMessageW(c,WM_CLOSE,0,0)),
              "P184 close replacement editor",records)
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("P184 host died with original peer still attached")
            if get_editors(process.pid)==[b]:
                break
            time.sleep(.2)
        check(get_editors(process.pid)==[b],
              "P184 original peer remains the sole attached editor",records)
        check(bool(u.PostMessageW(b,WM_CLOSE,0,0)) and process.wait(timeout=8)==0,
              "P184 final editor closes and host exits without a crash",records)
        log=(out/"editorhost-stdout.log").read_text(encoding="utf-8",errors="replace")
        check("P184_EDITOR_WINDOW_DETACHED_AND_REOPEN_REQUESTED" in log,
              "P184 test-only actual Steinberg host executed detach/reopen",records)
        print("P184_ACTUAL_WINDOWS_EDITOR_CLOSE_REOPEN_PEER_SURVIVAL_PASS",flush=True)
    finally:
        stdout.close()
        stderr.close()
        if process is not None and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:process.kill()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--editorhost",type=Path,required=True)
    ap.add_argument("--plugin",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    a=ap.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    records=[]
    try:
        check(a.editorhost.is_file(),"P184 original Steinberg EditorHost available",records)
        check(a.plugin.is_dir(),"P184 production native VST3 bundle available",records)
        run(a.editorhost.resolve(),a.plugin.resolve(),a.output,records)
    finally:
        (a.output/"P184_NATIVE_EDITOR_LIFECYCLE_EVIDENCE.json").write_text(
            json.dumps({"passed":bool(records) and all(r["passed"] for r in records),
                        "scope":"One real controller, two real editors, close/reopen peer, gestures, clean exit",
                        "tests":records},indent=2),encoding="utf-8")

if __name__=="__main__":
    main()
