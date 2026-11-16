# P003: V5 Original-Win95-Stereo-Zustandsprüfung (16.11.2026)

## Unabhängig geprüfte private Owner-Aufnahme

Quelle: Owner-Archiv `P003-20261116T175607Z-35e54581-ERGEBNIS.zip`; private Audio-, SoundFont- und Creative-Treiberbytes sind **nicht** Bestandteil dieses Repositories. Alle sieben SHA-256-Evidence-Einträge stimmen mit den Mitgliedern des Owner-Archivs überein. CPU- und EMU8000-Quellspuren stimmen für **1.524.133 von 1.524.133** physische I/O-Ereignisse überein. Der Win95-Stresstest enthält **4.176** originale MIDI-API-Aufrufe ohne API-Fehler.

Die unabhängig aus dem VXD-Aufrufstack und den originalen Release-/Start-Portereignissen rekonstruierte Stereo-Allokation läuft **25-mal**, mit jeweils zwei tatsächlichen Stimmenfreigaben und zwei Starts. Der originalverifizierte `LCOD+0x40C4`-Caller wird **493-mal** im CCCA-Lesepfad ausgeführt. Alle 493 zugehörigen V5-Voice-Row-Datensätze haben lesbare RAM-Felder, passen exakt zu Portsequenz/TSC/EBX/EBP und besitzen den korrekten 32-Byte-Stimmenzeilenabstand.

Von den 493 CCCA-V5-Voice-Rows haben **490** `sample_end > (CCCA & 0x00ffffff)` und **3** `sample_end <= (CCCA & 0x00ffffff)`. Bei allen 493 Datensätzen ist Bit 0 von `[EBX+0x10]` gelöscht. Der Original-x86-Allocator verwendet genau dieses Bit für die Strafwertung; die beiden anderen gesetzten Bits im Byte dürfen nicht als Strafe interpretiert werden.

## CPU-Modell – nur Forschungsdiagnostik, keine autonome VST3-Zeitsteuerung

Der neue Originallauf enthält **478** unmittelbar benachbarte CCCA→VTFT-Intervalle:
- Erste 16 Allokationen: **226** Intervalle. Alte, I/O-basierte Score-Näherung: **125/226** Treffer. Nach Einbeziehung der gültigen V5-Voice-Row-Zustände in die noch teilweise angenäherte Score-Fortschreibung: **140/226** Treffer; **86** Abweichungen verbleiben.
- Letzte neun Allokationen: **252/252** Treffer der zuvor kalibrierten, auf diesen Vollscanpfad begrenzten Zeitfunktion.
- Gesamt: **392/478** Treffern einer begrenzten *Forschungsprognose*, kein Nachweis instruktionstreuer CPU-Emulation und keine VST3/PCM-Freigabe.

Alle **86** verbleibenden Abweichungen in diesem neuen Lauf sind positive Abstände: Der echte Originalpfad benötigt **1 bis 16 Zyklen mehr** als die begrenzte Forschungsprognose. Die ältere Zahl **81/225** ist ein Ergebnis einer anderen Win95-Sitzung und darf nicht als neuer Befund ausgegeben werden.

## Kritischer Recorderfehler bei den alten „Score“-Feldern

Bei `LCOD+0x3D44` führt die originale I/O-Routine `push ebp; mov ebp,esp; push esi` aus. Der während des physischen I/O-Callbacks beobachtete `EBP` ist damit der **I/O-Helferframe**. Der ursprüngliche V5-Recorder hat `[EBP-4]`, `[EBP-12]`, `[EBP-20]` falsch als Allocator-Scorewerte etikettiert. Diese drei Werte sind **nicht** die echten Scorewerte und dürfen in keiner Beweisführung dafür verwendet werden.

Die drei per EBX gelesenen Voice-Row-Felder (`sample_end`, `key_marker`, `pair_flag`) sind davon nicht betroffen: Der I/O-Helfer verändert EBX nicht.

Die korrigierte, rein lesende Forschungserweiterung `P003_V5_CALLER_FRAME` erfasst künftig:
- `SS:[helper_EBP]` → gesicherter Allocator-EBP, mit Abstand/Ausrichtung geprüft.
- `SS:[helper_EBP-4]` → gesicherter Allocator-ESI.
- `SS:[allocator_EBP-4/-12/-20]` → aktuelle/vorherige/beste Scorevariable.
- `DS:[allocator_ESI+0x18]` → Originalmodus-Prüfbedingung am `LCOD+0x4023`-Pfad.
- `DS:[EBX-0x20+0x0D]`, `DS:[EBX-0x20+0x2D]` → nächste Voice-Row-Statusprüfungen.

Die bisherigen 17-spaltigen Portzeilen und V5_ROW_STATE-Kommentare bleiben unverändert; die Ergänzung schreibt keine Gastregister oder Speicher. Ein synthetischer kompilierter C-Gegenversuch bestätigt die Parent-Frame-Ableitung. Das ist **noch keine** echte Win95-Capture-Gegenprobe dieser zusätzlichen Felder.

## Grenze

Der funktionierende Original-V5-Lauf ist umfassend geprüft. Für die noch fehlenden frühen CPU-Instruktionspfade sind die korrigierten echten Allocator-Stackwerte erforderlich. Keine aufgefüllten CPU-Konstanten, kein Merge nach P005, kein neues Zwischenarchiv, keine Entwicklerübergabe.
