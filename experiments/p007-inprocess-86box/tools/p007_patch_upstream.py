#!/usr/bin/env python3
"""Source-pinned no-EXE Windows DLL build spike for the full 86Box core.
Never alter the upstream checkout in place: use a disposable CI clone.
Do not patch original EMU8000 DSP or x86 decoder or proprietary files.
"""
from pathlib import Path
from argparse import ArgumentParser
import subprocess
import shutil
import hashlib

PINNED="fee1e81977824fd2d5eed4d239589dc30d251dd7"

def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()

def main():
    ap=ArgumentParser()
    ap.add_argument("--fork", type=Path, required=True)
    args=ap.parse_args()
    fork=args.fork.resolve()
    if git(fork,"rev-parse","HEAD")!=PINNED:
        raise SystemExit("P007_REJECT_WRONG_UPSTREAM_FORK_COMMIT")
    cm=fork/"src/CMakeLists.txt"
    original=cm.read_text(encoding="utf-8")
    before="add_executable(86Box\n    86box.c\n"
    after="add_library(86Box SHARED\n    86box.c\n    P007CoreExports.c\n    P007AudioRing.c\n    P007MidiUart.c\n    P007LiveSf2.c\n    P007BootTrace.c\n"
    if original.count(before)!=1:
        raise SystemExit("P007_REJECT_UPSTREAM_BUILD_LAYOUT")
    dst=fork/"src/P007CoreExports.c"
    source=Path(__file__).resolve().parents[1]
    if dst.exists():
        raise SystemExit("P007_REJECT_ALREADY_PATCHED")
    core=(fork/"src/sound/snd_emu8k.c")
    if not core.exists() or not (fork/"src/86box.c").exists():
        raise SystemExit("P007_REJECT_MISSING_CPU_OR_EMU8000")
    dsp_before=hashlib.sha256(core.read_bytes()).hexdigest()
    dst.write_bytes((source/"fork/P007CoreExports.c").read_bytes())
    shutil.copyfile(source/"fork/P007AudioRing.c",fork/"src/P007AudioRing.c")
    shutil.copyfile(source/"fork/P007MidiUart.c",fork/"src/P007MidiUart.c")
    shutil.copyfile(source/"fork/P007LiveSf2.c",fork/"src/P007LiveSf2.c")
    shutil.copyfile(source/"fork/P007BootTrace.c",fork/"src/P007BootTrace.c")
    include=fork/"src/include/P007CoreABI.h"
    shutil.copyfile(source/"include/P007CoreABI.h",include)
    shutil.copyfile(source/"include/P007AudioRing.h",fork/"src/include/P007AudioRing.h")
    shutil.copyfile(source/"include/P007MidiUart.h",fork/"src/include/P007MidiUart.h")
    shutil.copyfile(source/"include/P007LiveSf2.h",fork/"src/include/P007LiveSf2.h")
    shutil.copyfile(source/"include/P007SerialRxGuard.h",fork/"src/include/P007SerialRxGuard.h")
    shutil.copyfile(source/"include/P007BootTrace.h",fork/"src/include/P007BootTrace.h")
    # Connect directly to the ORIGINAL 86Box post-mixer wavetable
    # before its SDL/OpenAL audio device. Never mutate snd_emu8k.c.
    sound_file=fork/"src/sound/sound.c"
    sound_original=sound_file.read_text(encoding="utf-8")
    original_hook="""        if (sound_is_float)
            givealbuffer_wt(outbuffer_w_ex);
        else
            givealbuffer_wt(outbuffer_w_ex_int16);
"""
    replacement_hook="""        /* P007: passive in-process PCM tap of ORIGINAL 86Box mixer. */
        if (sound_is_float)
            p007_audio_capture_float(outbuffer_w_ex, WTBUFLEN);
        else
            p007_audio_capture_pcm16(outbuffer_w_ex_int16, WTBUFLEN);
        /* In a VST3 the host owns audio: no SDL/OpenAL device output. */
"""
    if sound_original.count(original_hook)!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_WAVETABLE_SIGNATURE")
    if sound_original.count("#include <86box/sound.h>")!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_WAVETABLE_HEADER")
    # No second host audio device may be opened from a DAW instrument.
    # Original EMU8000 wavetable mixer and CPU/device state are untouched;
    # only the original emulator's OUTPUT calls to OpenAL are suppressed.
    other_output_calls=(
        "givealbuffer(outbuffer_ex);",
        "givealbuffer(outbuffer_ex_int16);",
        "givealbuffer_music(outbuffer_m_ex);",
        "givealbuffer_music(outbuffer_m_ex_int16);",
        "givealbuffer_cd(cd_out_buffer);",
        "givealbuffer_cd(cd_out_buffer_int16);",
        "givealbuffer_fdd(fdd_float_buffer, SOUNDBUFLEN * 2);",
        "givealbuffer_hdd(hdd_float_buffer, SOUNDBUFLEN * 2);",
    )
    sound_patched=sound_original.replace(
        "#include <86box/sound.h>", "#include <86box/sound.h>\n#include <P007AudioRing.h>",1
    ).replace(original_hook,replacement_hook,1)
    for call in other_output_calls:
        if sound_patched.count(call)!=1:
            raise SystemExit("P007_REJECT_ORIGINAL_HOST_AUDIO_SIGNATURE: "+call)
        sound_patched=sound_patched.replace(call,
            "(void)0; /* P007: no host OpenAL playback. */",1)
    # sound_reset() must not initialize a host OpenAL playback device.
    if sound_patched.count("    inital();")!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_OPENAL_INITIALIZER_SIGNATURE")
    sound_patched=sound_patched.replace("    inital();",
        "    /* P007: no host OpenAL playback; the DAW owns audio. */",1)
    sound_file.write_text(sound_patched,encoding="utf-8",newline="\n")

    # Replace host Named Pipe consumption with internal COM2 UART bytes at
    # the ORIGINAL emulated UART's receive timer. One byte per guest clock.
    # Do NOT change original UART timing, FIFO, PIC/IRQ, or Creative driver.
    uart=fork/"src/device/serial.c"
    uart_text=uart.read_text(encoding="utf-8")
    uart_origin="""    if (dev->char_port.chardev.read) {
        uint8_t val;
        if (dev->char_port.chardev.read(&val, sizeof(val), dev->char_port.chardev.priv) > 0)
            serial_write_fifo(dev, val);
    }
"""
    uart_replace="""    /* P007BA: in-process COM2, but never overflow the ORIGINAL
     * emulated 16550's 16-byte receiver FIFO with MIDI chords/NRPN.
     * Check the actual hardware FIFO BEFORE taking another wire byte.
     * Once full, keep the byte in our bounded host UART ring until the
     * original Creative Win95 guest reads the hardware port. */
    if (dev->inst == 1 && p007_midi_uart_active()) {
        const int fifo_mode=(dev->type >= SERIAL_16550) && dev->fifo_enabled;
        const unsigned filled=fifo_mode && dev->rcvr_fifo
                              ?(unsigned)fifo_get_count(dev->rcvr_fifo):0u;
        if (p007_serial_rx_has_room(fifo_mode,filled,
                (dev->lsr & 0x01)!=0,dev->out_new != 0xffff)) {
            uint8_t val;
            if (p007_midi_uart_take_byte(&val))
                serial_write_fifo(dev, val);
        }
    } else if (dev->char_port.chardev.read) {
        uint8_t val;
        if (dev->char_port.chardev.read(&val, sizeof(val), dev->char_port.chardev.priv) > 0)
            serial_write_fifo(dev, val);
    }
"""
    if uart_text.count(uart_origin)!=1 or \
       uart_text.count("#include <86box/serial.h>")!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_UART_RECEIVE_SIGNATURE")
    # Serial guest TX emits a ready signal only when the original Win95
    # helper actually opened original Creative WinMM. All COM2 guest traffic
    # stays in memory; no external host COM port, Named Pipe or 86Box process.
    tx_origin="""    if (dev->mctrl & 0x10)
        write_fifo(dev, val);
    else if (dev->sd && dev->sd->dev_write)
        dev->sd->dev_write(dev, dev->sd->priv, val);
    else if (dev->char_port.chardev.write)
        dev->char_port.chardev.write(&val, sizeof(val), dev->char_port.chardev.priv);
"""
    tx_replace="""    if (dev->mctrl & 0x10)
        write_fifo(dev, val);
    else if (dev->inst == 1)
        p007_midi_guest_tx_byte(val);
    else if (dev->sd && dev->sd->dev_write)
        dev->sd->dev_write(dev, dev->sd->priv, val);
    else if (dev->char_port.chardev.write)
        dev->char_port.chardev.write(&val, sizeof(val), dev->char_port.chardev.priv);
"""
    if uart_text.count(tx_origin)!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_UART_TRANSMIT_SIGNATURE")
    uart.write_text(uart_text.replace(
        "#include <86box/serial.h>",
        "#include <86box/serial.h>\n#include <P007MidiUart.h>\n#include <P007SerialRxGuard.h>",1
    ).replace(uart_origin,uart_replace,1
    ).replace(tx_origin,tx_replace,1),encoding="utf-8",newline="\n")


    # Expose diagnostic counts at the ORIGINAL disk-image read/write entry
    # points. These are passive counters, never replace I/O or alter timing
    # or HDD contents; intended for private Win95 boot diagnostics only.
    hdd=fork/"src/disk/hdd_image.c"
    hddtext=hdd.read_text(encoding="utf-8")
    old_hdd_header="#include <86box/hdd.h>"
    old_hdd_read="""    size_t num_read;

    if (hdd_images[id].type == HDD_IMAGE_VHD) {"""
    old_hdd_write="""    size_t num_write;

    if (hdd_images[id].type == HDD_IMAGE_VHD) {"""
    if (hddtext.count(old_hdd_header)!=1 or
        hddtext.count(old_hdd_read)!=1 or
        hddtext.count(old_hdd_write)!=1):
        raise SystemExit("P007_REJECT_ORIGINAL_HDD_IMAGE_SIGNATURE")
    hddtext=hddtext.replace(old_hdd_header,
        old_hdd_header+"\n#include <P007BootTrace.h>",1)
    hddtext=hddtext.replace(old_hdd_read,
        """    size_t num_read;
    p007_trace_hdd_read(id,sector,count);

    if (hdd_images[id].type == HDD_IMAGE_VHD) {""",1)
    hddtext=hddtext.replace(old_hdd_write,
        """    size_t num_write;
    p007_trace_hdd_write(id,sector,count);

    if (hdd_images[id].type == HDD_IMAGE_VHD) {""",1)
    hdd.write_text(hddtext,encoding="utf-8",newline="\n")

    # The original SDL frontend assumes a window and mutex. The VST3 never
    # creates either; guard only host-side UI calls, never CPU/EMU8000 code.
    sdlmain=fork/"src/unix/sdl_main.c"
    maintext=sdlmain.read_text(encoding="utf-8")
    old_blit="""void
startblit(void)
{
    SDL_LockMutex(blitmtx);
}

void
endblit(void)
{
    SDL_UnlockMutex(blitmtx);
}"""
    new_blit="""void
startblit(void)
{
    if (blitmtx) SDL_LockMutex(blitmtx);
}

void
endblit(void)
{
    if (blitmtx) SDL_UnlockMutex(blitmtx);
}"""
    if maintext.count(old_blit)!=1: raise SystemExit("P007_REJECT_HEADLESS_BLIT_SIGNATURE")
    sdlmain.write_text(maintext.replace(old_blit,new_blit,1),encoding="utf-8",newline="\n")

    render=fork/"src/unix/sdl_render.c"
    rendertext=render.read_text(encoding="utf-8")
    for old_guard,new_guard in (
        ("    SDL_LockMutex(sdl_mutex);\n    SDL_SetRelativeMouseMode",
         "    if (!sdl_mutex) { mouse_capture = on; return; }\n    SDL_LockMutex(sdl_mutex);\n    SDL_SetRelativeMouseMode"),
        ("    SDL_LockMutex(sdl_mutex);\n    resize_w",
         "    if (!sdl_mutex) return;\n    SDL_LockMutex(sdl_mutex);\n    resize_w"),
        ("    SDL_SetWindowTitle(sdl_win, sdl_win_title);",
         "    if (sdl_win) SDL_SetWindowTitle(sdl_win, sdl_win_title);"),
    ):
        if rendertext.count(old_guard)!=1: raise SystemExit("P007_REJECT_HEADLESS_RENDER_SIGNATURE")
        rendertext=rendertext.replace(old_guard,new_guard,1)
    render.write_text(rendertext,encoding="utf-8",newline="\n")

    ui=fork/"src/unix/sdl_ui.c"
    uitext=ui.read_text(encoding="utf-8")
    old_modal="    SDL_ShowMessageBox(&msgdata, &button);"
    if uitext.count(old_modal)!=1: raise SystemExit("P007_REJECT_HEADLESS_UI_MODAL")
    uitext=uitext.replace(old_modal,"    (void)msgdata; /* No modal dialog inside host DAW. */",1)
    for line in (
        "    osd_ui_sb_update_icon_state(tag, state);",
        "    osd_ui_sb_update_icon(tag, active);",
        "    osd_ui_sb_update_icon_write(tag, active);",
        "    osd_ui_sb_update_icon_wp(tag, state);",
    ):
        if uitext.count(line)!=1: raise SystemExit("P007_REJECT_HEADLESS_UI_OSD")
        uitext=uitext.replace(line,"    /* No host emulator window or OSD. */",1)
    ui.write_text(uitext,encoding="utf-8",newline="\n")

    cm.write_text(original.replace(before,after),encoding="utf-8",newline="\n")
    dsp_after=hashlib.sha256(core.read_bytes()).hexdigest()
    if dsp_before!=dsp_after:
        raise SystemExit("P007_REJECT_ORIGINAL_EMU8000_MODIFIED")
    print("P007_PINNED_86BOX_ORIGINAL_CORE_DLL_CMAKE_PATCH_PASS")
    print("P007_ORIGINAL_EMU8000_UNMODIFIED_SHA256",dsp_after)

if __name__=="__main__":
    main()
