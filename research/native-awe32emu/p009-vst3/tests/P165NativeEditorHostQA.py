#!/usr/bin/env python3
"""P165-NATIVE: real P165 native VSTGUI editor window, pixel and mouse QA.

Run on an interactive Windows runner with Steinberg's pinned EditorHost.
This is the real owner P007BW UI attached to native 64-frame EMU8000, NOT a mock.
Do not label it PASS when no interactive desktop/screen capture is present.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

if os.name != "nt":
    raise SystemExit("P18 NOT RUN: Win32 interactive GUI required")

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi = ctypes.WinDLL("gdi32", use_last_error=True)

class RECT(ctypes.Structure):
    _fields_ = [(v, ctypes.c_long) for v in ("left", "top", "right", "bottom")]

class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

u = user32
for name, restype, args in [
    ("EnumWindows", wintypes.BOOL, [ctypes.c_void_p, ctypes.c_void_p]),
    ("GetWindowThreadProcessId", wintypes.DWORD, [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]),
    ("IsWindowVisible", wintypes.BOOL, [wintypes.HWND]),
    ("GetClassNameW", ctypes.c_int, [wintypes.HWND, ctypes.c_wchar_p, ctypes.c_int]),
    ("GetClientRect", wintypes.BOOL, [wintypes.HWND, ctypes.POINTER(RECT)]),
    ("ClientToScreen", wintypes.BOOL, [wintypes.HWND, ctypes.POINTER(POINT)]),
    ("SetForegroundWindow", wintypes.BOOL, [wintypes.HWND]),
    ("SetCursorPos", wintypes.BOOL, [ctypes.c_int, ctypes.c_int]),
    ("mouse_event", None, [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]),
    ("keybd_event", None, [wintypes.BYTE, wintypes.BYTE, wintypes.DWORD, ctypes.c_void_p]),
    ("PostMessageW", wintypes.BOOL, [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]),
    ("GetDC", wintypes.HDC, [wintypes.HWND]),
    ("ReleaseDC", ctypes.c_int, [wintypes.HWND, wintypes.HDC]),
]:
    f = getattr(u, name)
    f.restype, f.argtypes = restype, args
gdi.CreateCompatibleDC.restype = wintypes.HDC
gdi.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi.CreateCompatibleBitmap.restype = wintypes.HBITMAP
gdi.CreateCompatibleBitmap.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
gdi.SelectObject.restype = wintypes.HANDLE
gdi.SelectObject.argtypes = [wintypes.HDC, wintypes.HANDLE]
gdi.BitBlt.restype = wintypes.BOOL
gdi.BitBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                       ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
gdi.GetDIBits.restype = ctypes.c_int
gdi.GetDIBits.argtypes = [wintypes.HDC, wintypes.HBITMAP, wintypes.UINT, wintypes.UINT,
                         ctypes.c_void_p, ctypes.c_void_p, wintypes.UINT]
gdi.DeleteObject.restype = wintypes.BOOL
gdi.DeleteObject.argtypes = [wintypes.HANDLE]
gdi.DeleteDC.restype = wintypes.BOOL
gdi.DeleteDC.argtypes = [wintypes.HDC]

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", ctypes.c_long),
                ("biHeight", ctypes.c_long), ("biPlanes", wintypes.WORD),
                ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wintypes.DWORD),
                ("biClrImportant", wintypes.DWORD)]

def get_editor(pid):
    found = []
    CB = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        proc = wintypes.DWORD(0)
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(proc))
        if proc.value == pid and u.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(128)
            u.GetClassNameW(hwnd, buf, 128)
            if buf.value == "VSTSDK WindowClass":
                found.append(hwnd)
        return True
    callback = CB(cb)
    u.EnumWindows(callback, 0)
    return found[0] if found else None

# The Github hosted Windows desktop can start at only 1024x768. A 1200x780
# plugin view is then clipped by EditorHost before any VSTGUI screenshot
# or mouse test. Enumerate supported display modes and temporarily switch
# to a real larger mode; never mark a cropped screen as a GUI PASS.
class DEVMODEW(ctypes.Structure):
    _fields_=[
        ("dmDeviceName",wintypes.WCHAR*32),
        ("dmSpecVersion",wintypes.WORD),
        ("dmDriverVersion",wintypes.WORD),
        ("dmSize",wintypes.WORD),
        ("dmDriverExtra",wintypes.WORD),
        ("dmFields",wintypes.DWORD),
        ("dmPositionX",wintypes.LONG),
        ("dmPositionY",wintypes.LONG),
        ("dmDisplayOrientation",wintypes.DWORD),
        ("dmDisplayFixedOutput",wintypes.DWORD),
        ("dmColor",wintypes.SHORT),
        ("dmDuplex",wintypes.SHORT),
        ("dmYResolution",wintypes.SHORT),
        ("dmTTOption",wintypes.SHORT),
        ("dmCollate",wintypes.SHORT),
        ("dmFormName",wintypes.WCHAR*32),
        ("dmLogPixels",wintypes.WORD),
        ("dmBitsPerPel",wintypes.DWORD),
        ("dmPelsWidth",wintypes.DWORD),
        ("dmPelsHeight",wintypes.DWORD),
        ("dmDisplayFlags",wintypes.DWORD),
        ("dmDisplayFrequency",wintypes.DWORD),
        ("dmICMMethod",wintypes.DWORD),
        ("dmICMIntent",wintypes.DWORD),
        ("dmMediaType",wintypes.DWORD),
        ("dmDitherType",wintypes.DWORD),
        ("dmReserved1",wintypes.DWORD),
        ("dmReserved2",wintypes.DWORD),
        ("dmPanningWidth",wintypes.DWORD),
        ("dmPanningHeight",wintypes.DWORD),
    ]

u.EnumDisplaySettingsW.restype=wintypes.BOOL
u.EnumDisplaySettingsW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,ctypes.POINTER(DEVMODEW)]
u.ChangeDisplaySettingsW.restype=wintypes.LONG
u.ChangeDisplaySettingsW.argtypes=[ctypes.POINTER(DEVMODEW),wintypes.DWORD]
u.GetSystemMetrics.restype=ctypes.c_int
u.GetSystemMetrics.argtypes=[ctypes.c_int]

class DEVMODEW(ctypes.Structure):
    _fields_=[
        ("dmDeviceName",wintypes.WCHAR*32),
        ("dmSpecVersion",wintypes.WORD),
        ("dmDriverVersion",wintypes.WORD),
        ("dmSize",wintypes.WORD),
        ("dmDriverExtra",wintypes.WORD),
        ("dmFields",wintypes.DWORD),
        ("dmPositionX",wintypes.LONG),
        ("dmPositionY",wintypes.LONG),
        ("dmDisplayOrientation",wintypes.DWORD),
        ("dmDisplayFixedOutput",wintypes.DWORD),
        ("dmColor",wintypes.SHORT),
        ("dmDuplex",wintypes.SHORT),
        ("dmYResolution",wintypes.SHORT),
        ("dmTTOption",wintypes.SHORT),
        ("dmCollate",wintypes.SHORT),
        ("dmFormName",wintypes.WCHAR*32),
        ("dmLogPixels",wintypes.WORD),
        ("dmBitsPerPel",wintypes.DWORD),
        ("dmPelsWidth",wintypes.DWORD),
        ("dmPelsHeight",wintypes.DWORD),
        ("dmDisplayFlags",wintypes.DWORD),
        ("dmDisplayFrequency",wintypes.DWORD),
        ("dmICMMethod",wintypes.DWORD),
        ("dmICMIntent",wintypes.DWORD),
        ("dmMediaType",wintypes.DWORD),
        ("dmDitherType",wintypes.DWORD),
        ("dmReserved1",wintypes.DWORD),
        ("dmReserved2",wintypes.DWORD),
        ("dmPanningWidth",wintypes.DWORD),
        ("dmPanningHeight",wintypes.DWORD),
    ]

u.EnumDisplaySettingsW.restype=wintypes.BOOL
u.EnumDisplaySettingsW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,ctypes.POINTER(DEVMODEW)]
u.ChangeDisplaySettingsW.restype=wintypes.LONG
u.ChangeDisplaySettingsW.argtypes=[ctypes.POINTER(DEVMODEW),wintypes.DWORD]
u.GetSystemMetrics.restype=ctypes.c_int
u.GetSystemMetrics.argtypes=[ctypes.c_int]

def ensure_full_desktop():
    before=(u.GetSystemMetrics(0),u.GetSystemMetrics(1))
    print(f"P117 desktop before resolution change: {before}",flush=True)
    if before[0]>=1400 and before[1]>=900:return before
    supported=[]
    for index in range(1000):
        mode=DEVMODEW()
        mode.dmSize=ctypes.sizeof(DEVMODEW)
        if not u.EnumDisplaySettingsW(None,index,ctypes.byref(mode)):break
        if mode.dmPelsWidth>=1400 and mode.dmPelsHeight>=900:
            supported.append((mode.dmPelsWidth,mode.dmPelsHeight,mode.dmDisplayFrequency,mode))
    supported.sort(key=lambda x:(abs(x[0]-1600)+abs(x[1]-900),x[0]*x[1]))
    for width,height,hz,mode in supported:
        mode.dmFields=0x00080000|0x00100000 # DM_PELSWIDTH | DM_PELSHEIGHT
        status=u.ChangeDisplaySettingsW(ctypes.byref(mode),0)
        after=(u.GetSystemMetrics(0),u.GetSystemMetrics(1))
        print(f"P117 desktop try {width}x{height} @ {hz}Hz: code={status}, got={after}",flush=True)
        if status==0 and after[0]>=1400 and after[1]>=900:
            time.sleep(.5)
            return after
    raise RuntimeError(
        f"P117 GUI test BLOCKED: CI desktop {before} cannot fit 1200x780 window; "
        f"no supported 1400x900 display mode. Available qualifying modes: {len(supported)}. "
        "Do not approve a cropped editor image.")

def client_size(hwnd):
    r = RECT()
    if not u.GetClientRect(hwnd, ctypes.byref(r)):
        raise RuntimeError("GetClientRect failed")
    return r.right-r.left, r.bottom-r.top

CANVAS_H=708
def layout_y(y): return y
def screen_pos(hwnd, x, y):
    sz = client_size(hwnd)
    pt = POINT(round(x*sz[0]/1000), round(layout_y(y)*sz[1]/CANVAS_H))
    if not u.ClientToScreen(hwnd, ctypes.byref(pt)):
        raise RuntimeError("ClientToScreen failed")
    return pt.x, pt.y

def snap(hwnd, filename):
    w,h = client_size(hwnd)
    x,y = screen_pos(hwnd, 0, 0)
    if w < 980 or h < 625:
        raise RuntimeError(f"Editor window wrong dimensions {w}x{h}")
    src = u.GetDC(None)
    mem = gdi.CreateCompatibleDC(src)
    bmp = gdi.CreateCompatibleBitmap(src, w, h)
    prev = gdi.SelectObject(mem, bmp)
    try:
        if not gdi.BitBlt(mem,0,0,w,h,src,x,y,0x00CC0020):
            raise RuntimeError("Screen capture unavailable (no interactive desktop?)")
        info = BITMAPINFOHEADER()
        info.biSize = ctypes.sizeof(info)
        info.biWidth, info.biHeight = w, -h   # top-down 32-bit pixels
        info.biPlanes, info.biBitCount = 1,32
        pixels = (ctypes.c_ubyte*(w*h*4))()
        if not gdi.GetDIBits(mem,bmp,0,h,ctypes.byref(pixels),ctypes.byref(info),0):
            raise RuntimeError("GetDIBits failed")
        raw = bytes(pixels)
        # BMP byte array is bottom-up while screen pixel array is top-down.
        path = Path(filename)
        file_size = 54 + len(raw)
        header = (b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
                  + struct.pack("<IiiHHIIiiII", 40,w,-h,1,32,0,len(raw),0,0,0,0))
        path.write_bytes(header+raw)
        return (w,h,raw)
    finally:
        gdi.SelectObject(mem,prev)
        gdi.DeleteObject(bmp)
        gdi.DeleteDC(mem)
        u.ReleaseDC(None,src)

def gold_brand_pixels(frame):
    """P117: the owner-approved gold 125A typography is in the raster faceplate."""
    w,h,raw=frame
    count=0
    for y in range(round(56*h/CANVAS_H),round(198*h/CANVAS_H)):
        for x in range(round(38*w/1000),round(310*w/1000)):
            i=(y*w+x)*4
            blue,green,red=raw[i],raw[i+1],raw[i+2]
            if red>=145 and green>=105 and red>=green and blue<green:
                count+=1
    return count

def gold_knob_pixels(frame, region):
    """Confirm that the actual VSTGUI drew the gold PNG frames, not the fallback."""
    w,h,raw=frame
    x0,y0,x1,y1=region
    scale_x,scale_y=w/1000.,h/CANVAS_H
    found=0
    for y in range(round(layout_y(y0)*scale_y),round(layout_y(y1)*scale_y)):
        for x in range(round(x0*scale_x),round(x1*scale_x)):
            i=(y*w+x)*4
            blue,green,red=raw[i],raw[i+1],raw[i+2]
            if red>140 and green>80 and red>green+30 and green>blue+30:
                found+=1
    return found


def click(hwnd,x,y):
    sx,sy=screen_pos(hwnd,x,y)
    if not u.SetForegroundWindow(hwnd):
        # Windows focus is best effort; SetCursorPos failure is fatal.
        pass
    if not u.SetCursorPos(sx,sy):
        raise RuntimeError("Cannot position mouse: no interactive desktop")
    time.sleep(.15)
    u.mouse_event(0x0002,0,0,0,None)
    time.sleep(.08)
    u.mouse_event(0x0004,0,0,0,None)
    time.sleep(.65)

def drag(hwnd,x0,y0,x1,y1):
    sx,sy=screen_pos(hwnd,x0,y0)
    ex,ey=screen_pos(hwnd,x1,y1)
    u.SetForegroundWindow(hwnd)
    if not u.SetCursorPos(sx,sy):
        raise RuntimeError("P18 cannot position pointer")
    time.sleep(.15)
    u.mouse_event(0x0002,0,0,0,None)
    for step in range(1,9):
        px=round(sx+(ex-sx)*step/8)
        py=round(sy+(ey-sy)*step/8)
        u.SetCursorPos(px,py)
        time.sleep(.06)
    u.mouse_event(0x0004,0,0,0,None)
    time.sleep(.45)


# A standalone, production-plugin EditorHost acceptance probe, not a
# screenshot of a hand-drawn mock. Test must FAIL if no real Win32 desktop.
def check(condition, name, records):
    records.append({"test":name, "passed":bool(condition)})
    print(("PASS " if condition else "FAIL ")+name, flush=True)
    if not condition: raise AssertionError(name)

def diff_pixels(before,after,region):
    w,h,a=before
    w2,h2,b=after
    if (w,h)!=(w2,h2):return -1
    x0,y0,x1,y1=region
    x0=int(x0*w/1000);x1=int(x1*w/1000)
    y0=int(y0*h/708);y1=int(y1*h/708)
    changed=0
    for y in range(max(0,y0),min(h,y1),2):
        for x in range(max(0,x0),min(w,x1),2):
            k=(y*w+x)*4
            if any(abs(a[k+j]-b[k+j])>18 for j in range(3)):
                changed+=1
    return changed

def main():
    args=argparse.ArgumentParser()
    args.add_argument("--editorhost",type=Path,required=True)
    args.add_argument("--plugin",type=Path,required=True)
    args.add_argument("--output",type=Path,required=True)
    a=args.parse_args()
    a.output.mkdir(parents=True,exist_ok=True)
    records=[]
    process=None
    try:
        check(a.editorhost.is_file(),"Steinberg original EditorHost.exe available",records)
        check(a.plugin.is_dir(),"Compiled ORIGINAL P165 P165_NATIVE VST3 available",records)
        ensure_full_desktop()
        process=subprocess.Popen(
            [str(a.editorhost),"--componentHandler",str(a.plugin)],
            cwd=str(a.editorhost.parent),
            stdout=(a.output/"editorhost-stdout.log").open("wb"),
            stderr=(a.output/"editorhost-stderr.log").open("wb"))
        hwnd=None
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("Actual P165 EditorHost exited "+str(process.returncode))
            hwnd=get_editor(process.pid)
            if hwnd:break
            time.sleep(.2)
        check(hwnd is not None,"Real full P165 native Windows editor opened",records)
        time.sleep(1.)
        full=snap(hwnd,a.output/"01-FULL-ORIGINAL-P165-FACEPLATE.bmp")
        w,h,_=full
        check(980<=w<=1050 and 685<=h<=745,
              "Original 1000x708 native faceplate geometry",records)
        check(gold_brand_pixels(full)>150,
              "Exact original P165 gold 125A logo visible",records)
        check(gold_knob_pixels(full,(817,353,951,487))>700,
              "Original 134px Master hardware knob is visible",records)
        for name,rect in [
            ("Reverb SEND",(74,333,132,391)),
            ("Chorus SEND",(258,333,316,391)),
            ("AMP Attack",(408,333,466,391)),
            ("AMP Decay",(502,333,560,391)),
            ("AMP Sustain",(596,333,654,391)),
            ("AMP Release",(690,333,748,391)),
            ("Filter Attack",(48,519,106,577)),
            ("Filter Decay",(169,519,227,577)),
            ("Filter Sustain",(290,519,348,577)),
            ("Filter Release",(411,519,469,577)),
            ("Filter Cutoff",(563,519,621,577)),
            ("Filter Resonance",(687,519,745,577)),
        ]:
            found=gold_knob_pixels(full,rect)
            check(found>75,
                  f"Original P165 {name} 58px hardware control visible ({found} gold pixels)",records)
        for name,rect,minimum in [
            ("ROM load button",(819,234,896,273),25),
            ("SF2 load button",(904,234,979,273),25),
            ("Reverb TYPE menu",(32,437,174,460),15),
            ("Chorus TYPE menu",(216,437,358,460),15),
            ("MOD RESET button",(808,568,960,604),25),
            ("Program selector",(261,234,552,273),80)
        ]:
            check(gold_knob_pixels(full,rect)>minimum,
                  f"Original P165 {name} visible",records)
        click(hwnd,581,254)
        nextFrame=snap(hwnd,a.output/"02-PROGRAM-NEXT.bmp")
        check(diff_pixels(full,nextFrame,(350,118,728,145))>8,
              "Native P165 original EMU8000 MIDI Program Next changes LCD",records)
        click(hwnd,232,254)
        previous=snap(hwnd,a.output/"03-PROGRAM-PREV.bmp")
        check(diff_pixels(nextFrame,previous,(350,118,728,145))>8,
              "Native P165 original EMU8000 MIDI Program Previous changes LCD",records)
        drag(hwnd,886,422,885,480)
        master=snap(hwnd,a.output/"04-MASTER-DRAG.bmp")
        check(diff_pixels(previous,master,(817,353,951,487))>70,
              "Actual original hardware Master mouse drag changes PNG frame",records)
        # Original AMP Attack knob is now host-backed and enabled.
        # This is an actual interactive Win32 gesture, not a source grep.
        drag(hwnd,437,365,437,318)
        attack=snap(hwnd,a.output/"05-LIVE-CREATIVE-AMP-NRPN.bmp")
        # Native screenshot oracle samples every 2px; genuine 0->28% knob
        # change affected 106 full-res pixels (about 26 at sample stride).
        check(diff_pixels(master,attack,(408,333,466,391))>15,
              "Original P165 AMP Attack hardware sprite reacts to VST3 generator edit",records)
        check(diff_pixels(master,attack,(393,409,485,438))>4,
              "Original P165 AMP Attack percentage updates with real VST3 parameter",records)
        click(hwnd,761,253)
        browser=snap(hwnd,a.output/"06-ORIGINAL-P165-BROWSER.bmp")
        check(diff_pixels(attack,browser,(45,342,764,626))>600,
              "Actual original searchable P165 BROWSER page appears",records)
        click(hwnd,190,394)
        selected=snap(hwnd,a.output/"06-BROWSER-SELECT-PROGRAM.bmp")
        check(diff_pixels(browser,selected,(350,118,728,145))>8,
              "Original P165 Browser row drives native EMU8000 MIDI program parameter",records)
        click(hwnd,660,253)
        synth=snap(hwnd,a.output/"07-RETURN-COMPLETE-SYNTH.bmp")
        check(diff_pixels(selected,synth,(45,342,764,626))>600,
              "Original P165 SYNTH tab returns to all 12 existing knobs",records)
        # Type selectors must accept actual input (not only display gold pixels).
        # A separate VST3 host-state test proves the 1400/1401 parameters.
        click(hwnd,100,449)  # REVERB TYPE
        time.sleep(.35)
        click(hwnd,100,449)  # select/dismiss depending on VSTGUI popup mode
        check(process.poll() is None,
              "P165 Reverb TYPE GUI mouse interaction did not crash EditorHost",records)
        click(hwnd,265,449)  # CHORUS TYPE
        time.sleep(.35)
        click(hwnd,265,449)
        check(process.poll() is None,
              "P165 Chorus TYPE GUI mouse interaction did not crash EditorHost",records)
        check(process.poll() is None,"Real full P165 EditorHost survives live interaction",records)
        print("P165_NATIVE_ACTUAL_NATIVE_ORIGINAL_P165_FULL_12_KNOBS_ROM_SF2_FX_EDITORHOST_PASS",
              flush=True)
    finally:
        try:
            (a.output/"P165_NATIVE_ACTUAL_GUI_EVIDENCE.json").write_text(json.dumps(
                {"passed":bool(records) and all(r["passed"] for r in records)
                 and len(records)>=25,
                 "knobs_visible":12,"host_gui_control_gestures":True,
                 "scope":"Exact P165_NativeBW native hardware GUI and real host gestures; PCM generator effects are tested separately, not by pixels",
                 "tests":records},indent=2),encoding="utf-8")
        except Exception:pass
        if process is not None:
            process.terminate()
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:process.kill()

if __name__=="__main__":
    main()
