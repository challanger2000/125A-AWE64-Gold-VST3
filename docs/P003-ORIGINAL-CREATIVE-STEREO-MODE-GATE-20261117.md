# P003 – Original Creative Stereo-Allocator: Modus-Schwellwert und 14-Takt-Abstand (17.11.2026)

## Zweck / Trennung der beiden Entwicklungswege

**Dieser Befund ist unabhängige Original-Zuarbeit für den C++23-Entwickler**, nicht eine zweite REA-Portierung des Treibers. Die andere direkte REA-Portierung bleibt eigenständig. Keine Änderungen an produktivem P005, EMU8000, ROM oder Original-Win95-Gast.

## Originalbelege (private Owner-Dateien, NICHT committen)

- Win95-V5-Caller-Frame-Sitzung A: `P003-20261116T184835Z-c56deac6-ERGEBNIS.zip`, SHA-256 `6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944`.
- Win95-V5-Caller-Frame-Sitzung B: `P003-20261117T010413Z-9c86c813-ERGEBNIS.zip`, SHA-256 `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`.
- Beide Spuren enthalten tatsächliche originale Creative-CALL-/EMU8000-I/O-Aufrufe und den korrigierten `P003_V5_CALLER_FRAME` mit gültigem `[ESI+0x18]`. A enthält 466 direkt aufeinanderfolgende CCCA→VTFT-Übergänge, B enthält 464. Jeweils 25 abschließende originalrekonstruierte Stereo-Allocator-Durchläufe. Die neun letzten vollständig stabilisierten Scans enthalten jeweils exakt 243 CCCA→VTFT-Übergänge.
- B: 4.176 Original-Win95-MIDI-Aufrufe, 0 API-Fehler, 1.529.747 per CPU-/EMU8000-Join bestätigte Portereignisse. Das zusätzlich geplante `Original-Dynarec-Blocks.trace` **ist nicht im hochgeladenen Ergebnisarchiv**; dessen Erzeugung und Inhalt sind folglich ungeprüft.

## **Schlüsselbefund: andere tatsächlich ausgeführte originale x86-Instruktionen**

Quelle: original SHA-verifizierte Creative `SBAWE.VXD`, LCOD-Segment im privaten Audit.

```asm
LCOD+3FFD: cmp dword ptr [esi+18h], 2
LCOD+4001: seta al
LCOD+4010: mov [ebp-18h], eax
LCOD+4023: cmp dword ptr [ebp-18h], 0
LCOD+4027: jne LCOD+4048      ; >2: vorherige Nachbarstimmen-Prüfung ÜBERSPRINGEN
LCOD+4029: cmp byte ptr [ebx+0Dh], 0
LCOD+402D: jne LCOD+4111      ; <=2: markierte Stimme überspringen
LCOD+4033: movzx eax, word ptr [esi+06h]
LCOD+4037: lea ecx, [edi+1]
LCOD+403A: cmp eax, ecx
LCOD+403C: jbe LCOD+4048
LCOD+403E: cmp byte ptr [ebx+2Dh], 0
LCOD+4042: jne LCOD+4111      ; weitere Nachbarprüfung
LCOD+4048: movzx eax, word ptr [ebx-0Ah] ; normale VTFT/Score-Prüfung
...
LCOD+4111: mov dword ptr [ebp-0Ch], 0FFFFFh ; übersprungene Voice
LCOD+4118: mov eax,edi
LCOD+411A: sub ebx,20h       ; nächster 32-Byte-VoiceRow
```

Die genuine Win95-Caller-Frame-Aufzeichnung bestätigt: **A: `[ESI+0x18]=4` in allen 243 späten Vergleichsintervallen; B: `[ESI+0x18]=2` in allen 243.** Frühere 93/91/95-Takt-Forschungsprognosen waren ausschließlich für einen anderen Codepfad kalibriert. **Eine konstante 14-Takt-Differenz zwischen diesen Sitzungen beweist keinen Fehler des 86Box-Dynarec.** Wir hatten zuvor verschiedene Originaltreiber-Instruktionsfolgen miteinander verglichen.

## Unabhängige Zahlen aus beiden echten Originalspuren

Vorhandene begrenzte **Forschungs**-Formel nur für den damaligen Vollscan:

`T0 = 93 + 2*(original_candidate_replaces_best) - 2*(previous_score < current_score)`.

| Stabile Scans 17–25 | Treffer mit T0 | Gemessene Restdifferenzen zu T0 | Genauigkeitskontrolle |
|---|---:|---|---|
| Sitzung A, Originalfeld **4** | **243/243** | 0:243 | alle tatsächlich direkt aufeinanderfolgenden Stimmen, EBX −32 |
| Sitzung B, Originalfeld **2** | **0/243** | +14:227; +17:7; +50:9 | zusätzliche Creative-Vorstufen und tatsächliche Voice-Skips |

Für B ist anhand der nächsten originalen Voice-Row ebenfalls nachgewiesen:
- **234/243** Übergänge führen genau eine Stimme weiter (`EBX_new=EBX_old−32`); davon **227** mit +14 und **7** mit +17.
- Bei den weiteren **9/243** wird die unmittelbar nächste Voice aufgrund von `[next_voice+0x0D]!=0` verworfen und auch die benachbarte Stimme übersprungen. Der nächste VTFT liegt **drei Voice-Rows** weiter (`EBX_new=EBX_old−96`); diese neun Fälle weisen **+50** Takte gegenüber T0 auf.
- Bei der aktuellen Sitzung B entsprechen **7** verbleibende +3-Restabweichungen **ausschließlich der physischen Stimme 4 in Scans 17–23**. Deren konkrete Ursache ist noch nicht bewiesen.

Eine ausschließlich **datengebundene Forschungsprognose** aus diesen beobachteten Unterklassen ist:
- Für das in A belegte Feld >2 und vollständige Vollscans: `T = T0`.
- Für das in B belegte Feld 2 und denselben **direkten Vergleichspfad**: `T = T0 + 14 + 36*(next_voice_flag != 0)`.

Diese zweite Formel trifft **236/243** späte Originalintervalle in Sitzung B; zusammen mit A **479/486**. **Diese 14 und 36 sind noch empirische Unterschiede, keine aus 86Box-Instruktionskosten autonom hergeleitete allgemeingültige Zeitsteuerung.** Sie dürfen weder unbesehen in einen C++23-DSP-Timer noch als behauptete Beweise für alle Creative-Stimmenzustände eingehen. Insbesondere ist die C++23-Regel für `next_voice_flag` als tatsächlich zu überprüfende `0x0D/0x2D`-Statusentscheidung zu portieren, statt bloß die beobachtete Voice-Skip-Anzahl aus dem Ergebnis zu replizieren.

## Verbindliche technische Entwickler-Zuarbeit

1. Die x86-Kontrollflusskante `LCOD+3FFD/4001/4023/4027` anhand des originalen `[ESI+0x18]` rekonstruieren: **>2** umgeht die Nachbarstimmen-Gates, **<=2** durchläuft sie.
2. Im kleineren Pfad die **beiden echten Original-Flags** `[EBX+0x0D]` und `[EBX+0x2D]` samt `[ESI+0x06]`, `EDI` und Skip auf `LCOD+4111/411A` portieren. Es gilt weiterhin 32 Byte pro Voice-Row.
3. Mit autonomen C++23-Unit-Tests beweisen, dass die Entscheidung über die nächste Stimme aus dem **aktuellen State** entsteht, nicht aus einer aufgenommenen Sequenz oder festen Note-Nummern.
4. Die TSC-Phase nur als Diagnose benutzen. Wenn die portierte Creative-Logik aus jedem Originalzustand die richtige Registerfolge und **bitgleichen trockenen PCM-Ausgang** liefert, brauchen wir keine vollständige Pentium-/Dynarec-Nachemulation.

**Status:** Wesentlicher Original-Codepfad und beobachtete Auswirkungen geklärt; 7 späte B-Intervalle noch offen. P005 NO-CHANGE / autonome vollständige Originalparität weiterhin **nicht freigegeben**.
