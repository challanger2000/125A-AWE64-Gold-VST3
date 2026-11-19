# P007AZ — Original-Win95-Creative-SF2/ROM: End-to-End-Klangnachweis

**Datum:** 19.11.2026 · **Nachweis:** Privater Windows-Hosttest des Eigentümers im vollständigen P007AZ-Paket, Ergebnis `P007AV-ORIGINAL-SF2-ERGEBNIS(4).txt` und `P007AX-ORIGINAL-EMU8000-SF2-ROM-REGISTER-EVENTS.csv`. Diese originalen, privaten Dateien werden **nicht** ins öffentliche Repository hochgeladen.

## Akzeptanz — ausschließlich der nachgewiesene Prüfumfang

- `P007W_REAL_WINDOWS_VST3_TWO_INSTANCE_PROJECT_STATE_REOPEN_PASS` — VST3-State auf zwei Instanzen / Reopen; **kein** Nachweis von zwei gleichzeitig bootenden originalen Win95-86Box-Gästen.
- `P007AV_REAL_WINDOWS_VST3_SF2_PATH_V3_RECALL_AND_CORRUPTION_REJECTION_PASS` — SF2-Pfad-Projektzustand und absichtliche Beschädigung korrekt behandelt.
- `P007AV_REAL_WIN95_CREATIVE_SF2_LOAD_ACK_PASS sf2_bytes=555754 original_cpu=1 winmm_ready=1` — originaler 86Box-x86-Gast, Win95-Creative AWEMAN32 SF2-Load-ACK sowie WinMM-Gerät.
- `P007AX_REAL_SF2_PRESET_SELECTED bank=1 program=7 note=60` — tatsächlich in SF2 vorhandenes Testprogramm 7 statt ROM-GM-Programm 0.
- `P007AX_ORIGINAL_EMU8000_BANK_REGISTER_DIAG sf2_events=304 ... rom_events=76 ... real_guest=1 synthetic=0` — echte, passive Original-EMU8000-Registerspur. Deutlich andere Werte und Stimmenallokation, nicht nur unterschiedliche Stimme-Hashes.
- SF2 Bank 1: 108544 Audioframes, RMS **0.003406328**, Peak **0.042419434**, Roughness **0.043107747**.
- ROM Bank 0: 108544 Audioframes, RMS **0.002989094**, Peak **0.021423340**, Roughness **0.261048875**.
- `P007AV_REAL_ORIGINAL_CREATIVE_SF2_BANK1_TO_EMU8000_PCM_PASS`.
- `P007AV_REAL_WIN95_ORIGINAL_ROM_BANK0_RETURN_PCM_PASS`.
- `P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS`.
- Abschluss: `P007AV_OWNER_PRIVATE_ORIGINAL_CREATIVE_SF2_AND_ROM_E2E_PASS`.

## Negativkontrollen und Ursache

P007AX und P007AY scheiterten **korrekt**: SF2- und ROM-Audio sowie je 76 originale Registerwrites waren gleich. Der ursprüngliche Host sendete CC0, CC32 und ProgramChange als drei aufeinanderfolgende 6-Byte-Tunnelpakete (18 Byte) und ließ zeitgleich NoteOn zu; der originale emulierte COM2-16550-Empfangspuffer fasst 16 Byte. `P007AZ` serialisiert Auswahlbefehle einzeln, mit mindestens 20 ms Abstand plus Settling-Gap, und verzögert hostseitige Noten bis zum Abschluss. Diese Korrektur ist eine **plausible und durch den erfolgreichen Vorher/Nachher-Test gestützte Ursache**, nicht der Beweis jeder einzelnen verlorenen UART-Byteposition. Es wurde keine Änderung am Original-EMU8000-Klangkernel vorgenommen.

P007AZ fast Windows CI: `44531243994` **success**, echter MSVC-VST3-Build inklusive vorhandenem originalem 86Box-Kern für Steinberg-Validator und adversarialer FIFO-Pacer-Test. Separater kompletter Windows-/86Box-Workflow `44530568224` **success** mit Stand kurz vor dem letzten CI-only Packaging-Commit. Private Sounddaten sind nicht Teil dieser CI.

## Nicht daraus ableiten

Dieser Test gibt **nicht** frei: Studio One / Fender Studio Pro auf realer Audiohardware, dauerhaft stabile gleichzeitige zwei Win95-Gäste (86Box-Prozessglobals), vollständige Automation unter Blockgrößen/Host-Sampleraten, Original-SBK-Pfad unabhängig von SF2, alle FX-Typen und Reverb-/Chorus-Sends unter allen Bedingungen oder bitidentisches PCM mit physischem AWE64-Gold-Line-Out. Die einmalige Win95-CPU-Exception `0F FF` bleibt ausschließlich ein Debuglog-Detail und darf weder als Plugin-UI-Fehler erscheinen noch als genereller Stabilitätsfehler missverstanden werden.

**Entscheid:** P007AZ SF2-User-Bank-1 ↔ ROM-Bank-0 Originalklang-Gate **PASS**, übergeordneter **Release Candidate weiterhin DRAFT** bis zu den verbleibenden technischen Abnahmen; Studio-One-Praxistest ausschließlich am Ende.
