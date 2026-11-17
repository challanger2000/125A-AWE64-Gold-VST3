# P107 — Tatsächlich ausgeführte Creative-x86-Sprungpfade unter originalem Windows 95

**Stand 16.11.2026:** Isolierte, echte Original-VM-Messung erfolgreich aufgezeichnet. Vollständige, instruktionslückenlose Original-CPU-Zeitparität weiterhin **NO-GO**. Originaler AWE64-Gold-VST3-Produktcode und originaler EMU8000-DSP unverändert.

## Exakt passende Originalreferenz — nicht irgendein synthetischer Win95-Test

Private Original-VM aus dem bereits vom Nutzer gelieferten Gesamtpaket `125A-P003-CREATIVE-ORIGINAL-DYNAMISCHE-AUFRUFER-V4-GESAMTPAKET.zip`, darin originaler Win95-FAT16-Datenträger (536.870.912 Bytes), originale private Creative-VxD/DLL und privater ROM-Datensatz. Die **erste** P107-Linux-VM wurde aus einem zuvor mehrfach verwendeten Win95-Klon abgeleitet und hatte unter anderem eine andere/duplizierte `P003BANK.SF2`; dieser Lauf lieferte 0 Gast-x86-Instruktionen aus dem Stereo-Allocator und wurde **verworfen**. Im zweiten, qualifizierten Versuchsaufbau wurde die **unveränderte originale Basis-VM neu ausgepackt** und ausschließlich in eine private Wegwerfkopie geladen:

- Exakt der originale **Drei-Bank-Stimmenstresstest** aus [P003-Win95-Voice-Stress-Run #43787795937](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43787795937) mit PE32-SHA256 `3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f`.
- Originale zweite Vergleichs-SF2 aus dem privaten, bereits aufgezeichneten erfolgreichen Originalergebnis vom 16.11., SHA256 `cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb`. Auf der temporären FAT16-Kopie genau **ein** `P003BANK.SF2`; no duplicated 8.3 directory entry.
- Original-EMU8000, originale SBAWE.VXD, RAM/ROM/Win95 unverändert; `86Box` old x64 Dynarec `NEW_DYNAREC=OFF`, Pentium P54C 133.333.333 Hz, Gast-TSC vom physischen P003-I/O-Beobachter.
- Windows-95-Gastprogramm schließt **4.176** Creative-AWE-MIDI-API-Aufrufe mit **0** gemeldeten MIDI-Fehlern ab (`P003MODE.LOG`, nach Lauf privat aus FAT16 gelesen).

Die Bootmeldungen über die abgeschaltete NE2000-Netzwerkkarte wurden lokal automatisch weggeklickt; keine Nutzeraktion. Original-VM/Win95-Datenträger/Originaltreiber/ROM/SoundFont/CPU-Rohspuren wurden **nie nach GitHub hochgeladen**.

## Echte private x86-Ausführungsdaten

| Beobachtung | V1: JIT-only | V2: JIT plus tatsächlich interpretierte Gastbefehle |
| --- | ---: | ---: |
| Echt aufgezeichnete Gast-x86-Instruktionsereignisse | **36.537** | **36.784** |
| Ausgeführte Jcc-Kontrollstellen im erfassten Original-LCOD-Ausschnitt | **3.399** | **3.474** |
| Jcc mit unmittelbar nachgewiesener Ziel-/Fallthrough-Adresse | **3.338** | **3.458** |
| Jcc mit fehlendem unmittelbar nachfolgenden x86-Ereignis | **61** | **16** |
| `LCOD+0x4027`: tatsächlicher `[EBP-0x18]`-RAM-Wert lesbar | **730/730** | **750/750** |
| Wert an `[EBP-0x18]` | durchgehend **1** | durchgehend **1** |
| Mit unmittelbarem Jcc-Nachfolger nachgewiesener JNE am `+0x4027` | **725 taken**, 0 not-taken | **750 taken**, 0 not-taken |
| Nachgewiesener Widerspruch zwischen lesbarem lokalen Wert und direktem Jcc-Ausgang | **0** | **0** |

Die zweite echte private Aufzeichnung wurde aus dem **SHA-verifizierten öffentlichen CI-Build** vom [GitHub Actions #43934296297](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43934296297) durchgeführt, **dessen drei Jobs alle PASS** sind: originaler Quellcode-/Negativtest, echter Ubuntu-x64-86Box-Build und echter Windows-2022-x64-86Box-Build. Die echten Windows-95-Gastausführungen fanden **privat unter Linux-86Box** statt, nicht auf dem GitHub Windows Runner.

Private V2-Beweisprovenienz (ausschließlich Hashes und aggregierte Metadaten hier, **keine Rohdaten**):

- Ausgeführte originale x86-Instruktionsspur: **36.784 Ereignisse**, SHA256 `319d12cf9e29dbbfab7dfa59313b1a50e4652d56c8b38ee0c658572ce796d0a8`; 1.975.376 Bytes. Header markiert den **host/86Box-blockbezogenen TSC**, ausdrücklich **keine** eigenständigen Gast-Instruktionszähler.
- Originale physische CPU/EMU8000-I/O-Spur: **1.534.006 Ereignisse**, SHA256 `d30b55e704803accd5291ba743202937039cf4cc756b199672b6741a2c68234c`; 199.476.595 Bytes.
- Originaler `P003MODE.LOG`-Stressabschluss im Gast: SHA256 `4fbdb499550fb10ddf0dd0bf756b475d05667b0293457efb51e0b1227fd135c1`, 722 Bytes, `ORIGINAL_MIDI_CALLS_TOTAL=0x1050` und `ORIGINAL_MIDI_CALL_ERRORS=0`.

## Was jetzt **belegt** ist

Die im P106-Quellcode bisher nicht unmittelbar aus einer Original-RAM-Aufnahme aufgelöste Verzweigung `LCOD+0x4027 (JNE)` wurde **750-mal** im Originaltreiber ausgeführt, `[EBP−0x18]` konnte jedes Mal read-only aus der Gast-RAM-Abbildung gelesen werden und war **immer 1**, der nächste x86-PC belegte **750-mal den genommenen JNE-Zweig**. Dies ist ausdrücklich der lokale Wert **`[EBP-0x18]`**, nicht automatisch ein Nachweis für `DeviceState.mode_count=32`, das im nativen C++23-Port einen anderen semantischen Status haben kann.

Zusätzlich sind in der V2-Originalspur nachweisbar, aus tatsächlich aufeinanderfolgenden erfassten x86-PCs:

- `LCOD+0x40CB (JA)`: **450 taken**, 0 not-taken, **1 Nachfolger nicht erfasst**.
- `LCOD+0x40E4 (JE)`: **762 taken**, 0 not-taken, 0 nicht erfasst.
- `LCOD+0x40F1 (JAE)`: **495 taken**, **253 not-taken**, **15 Nachfolger nicht erfasst**.
- `LCOD+0x4107 (JB)`: **676 taken**, **72 not-taken**, 0 nicht erfasst.

**Fehlerkategorie sauber korrigiert:** Die V1-Diagnose nannte zunächst 66 Prüfmeldungen; darin waren **fünf doppelte Fehlerkennzeichen** an denselben fehlenden JNE-Nachfolgern. Es waren **61** unterschiedliche unterbrochene Jcc-Nachfolger. Die gezielte Erweiterung `tools/p107_patch_old_dynarec_interpreter_fallback.py` instrumentiert den tatsächlichen Interpreter-Ausführungspunkt bei der ersten Dynarec-Kompilierung und reduziert diese von 61 auf **16**, statt fehlende Instruktionen durch Vermutungen aufzufüllen.

## Explizite Grenzen / richtiger nächster technischer Schritt

Die V2-Spur enthält noch **16** nicht direkt verbundene Jcc-Nachfolger (**15** an `+0x40F1`, **1** an `+0x40CB`). Daraus darf man keine eindeutige vollständige **Instruktionsfolge**, kein exaktes U/V-Pairing/AGI-Blockkostenprofil und insbesondere **keinen eigenständigen 91/93/95-TSC-Scheduler** ableiten. Der P107-Original-Spurprüfer muss mit vollständigem lückenlosen Jcc-Nachweis fail-closed bleiben: **P107 Forschungsnachweis = TEILPASS**, P003 autonome Win95-VST3-CPU-Zeitparität = **NO-GO**. Mehr Instrumentierung im alten x64-Dynarec ist gezielt für diese beiden Standorte nötig, nicht weitere hypothetische Zeitkonstanten.

Das P107-Build trägt **kein** Creative-Binary/ROM/SF2 und wird **nicht in die VST3-DSP-Engine** integriert. Alle Änderungen auf separatem Draft-PR, **kein Merge**; kein DAW/ASIO-Test in dieser Phase; **keine Nutzeraktion**.
