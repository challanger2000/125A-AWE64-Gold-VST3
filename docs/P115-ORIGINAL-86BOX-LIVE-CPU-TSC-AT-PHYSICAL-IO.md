# P115 – Original-86Box-CPU-Zustand direkt bei EMU8000-Portcallbacks

**17. November 2026 · P003-REA-Forschung · Kein Produkt-/VST3-Release.**

## Tatsächlicher Eingriff

In `src/cpu/386_dynarec.c` (Original-86Box-SHA `e9765a423b5d675337aab33fd060204e62dc20d7`) sind `cycles_old` und `tsc_old` **static**. Der vorhandene P003-IO-Trace in `src/io.c` zeichnet `tsc` und `cycles` auf, nicht aber die alte Blockreferenz, bereits gebuchte Zwischenzeiten und den optionalen `USE_ACYCS`-Zuschlag. Daher fehlt eine direkt prüfbare **effektive** TSC-Abrechnung an jedem physikalischen EMU8000-Portcallback.

`tools/p115_original_live_cpu_tsc_probe.py` verändert ausschließlich eine **wegwerfbare Kopie** dieser beiden Original-86Box-Dateien. Es fügt einen schreibfreien CPU-Snapshot-Getter hinzu; der existierende P003-Portcallback ruft ihn mit derselben Port-Sequenznummer auf. Beide Opt-ins müssen gesetzt sein: `AWE64_P003_CPU_IO_TRACE` und `AWE64_P115_LIVE_TSC_TRACE`, jeweils auf eine **andere private Ausgabedatei**. Ohne ursprünglichen P003-Tracer werden keine P115-Datensätze geschrieben. Soundchip-Dateien, Windows 95, Creative-VXD, ROM und Produkt-VST3 bleiben unverändert.

Die projizierte TSC folgt original `update_tsc()`: `cycdiff = cycles_old-cycles [+ acycs bei inrecomp]`, abzüglich bereits in `tsc-tsc_old` gebuchter Zeit. Der Getter **ruft update_tsc nicht auf** und verändert weder CPU-Zähler noch Timer noch EMU8000-Register. Die separate native P114-C++23-Blockuhr kann bei einem neuen Originalgastlauf gegen diese Originalzustände geprüft werden.

## Quellen und Prüfungen

- Originalquelle: `turican0/86Box-AWE32`, Tag `v6.0-AWE32-0.93`.
- `src/cpu/386_dynarec.c`: originale `update_tsc()`-Abrechnung und CPU-Blocksteuerung.
- `src/io.c`: Original-EMU8000-8/16/32-Bit-Callback-Kette, durch P003 bereits physisch beobachtet.
- `tools/p115_join_p003_original_cpu_snapshot.py`: Prüft private P003-/P115-Spuren auf gleiche Reihenfolge, Richtung, Port, rohen TSC, Restzyklen, Originalformel, Monotonie, Overflow und Abbruchmarker; gibt nur aggregierte Zahlen/Hashes aus.
- Der unveränderte C-Getter wurde unter Linux mit und ohne `USE_ACYCS` nativ kompiliert und ausgeführt. Vier Quell-/Negativ-/C-Testfälle und der synthetische P003-/P115-Auditor bestanden lokal.

**Beweisgrenze:** Noch keine echte neue Windows-95-Originalmessung, kein autonomes Rekonstruieren sämtlicher x86-Instruktionskosten, keine 91/93/95-Zykluskausalität, keine bitgenaue autonome VST3-PCM-Parität. Die separate GitHub-Actions-Abnahme prüft die echten, gepinnten 86Box-Quellen und den Original-OLD-Dynarec-Build. Der jeweilige Workflowstatus ist maßgeblich.

**Nächster notwendiger Beleg:** Ein neuer Originalgastlauf mit synchroner P003-/P115-Portmessung und P107-/P111-/P113-Ausführungsspur; erst danach die Instruktionskosten mit dem tatsächlichen CPU-Zustand verknüpfen. Kein Merge in P005 und kein Studio-One-Vorabtest.
