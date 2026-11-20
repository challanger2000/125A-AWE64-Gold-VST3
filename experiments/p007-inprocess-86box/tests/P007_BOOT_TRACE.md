## P007-H: direct original 86Box HDD I/O boot telemetry
Source-locked `hdd_image_read` / `hdd_image_write` are instrumented
at their ENTRY without replacing I/O, data or original timing. At every
10,000 original `pc_run()` slices the private Win95 boot-to-PCM
research probe prints read requests, last LBA, write requests, MIDI
ready and nonzero audio peak. It can distinguish an emulator that never
starts reading the boot drive from one that reads Windows95 sectors but
never opens the original Creative WinMM device. The public synthetic
BIOS probe prints identical counters when the latest trace-capable
emulator DLL is used. No guest disk contents are exposed.
