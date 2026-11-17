# P003 – Original-Win95-Nachweis: Modus-0-Allocator benutzt **32 physische Stimmen** (17.11.2026)

**Status: ORIGINALMESSUNG + C++23-QUELLKONTRAKT BESTÄTIGT.** Unabhängige Forschungsarbeit; **keine sofortige Entwicklerübergabe, kein PR-Kommentar**, keine Änderung an P005 oder dem separaten direkten REA-Port.

## Warum dieser Befund wichtig ist

Die bisherige C++23-Implementierung begrenzt Modus 0 in `CreativeVxdMonoVoiceRanking` mit `kOriginalCreativeMusicVoices=30`. Im Forschungsdispatcher `CreativeVxdAdditionalAllocatorModes.cpp` wurde deshalb sogar eine gesondert zu bestätigende Grenze `mode0_scan_ceiling<=30` verlangt. Die frühere Dokumentation bezeichnete 32 Stimmen als **unbewiesen**.

Das ist inzwischen anhand von **zwei echten Windows-95-Aufnahmen** nachweislich zu eng. Der Originaltreiber bearbeitet im Modus-0-Kontrollfluss sowohl Stimme 30 als auch 31, einschließlich realer EMU8000-Register-Leseselektoren und DCYSUSV-Freigabeschreibzugriffe.

## Direkter Original-x86-Beweis

Authentisches, privat SHA-geprüftes `SBAWE.VXD`-LCOD (Treiber-SHA-256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`):

```asm
LCOD+3ED4: movzx ecx,word ptr [esi+06h]  ; tatsächliche Gerätestimmenzahl
LCOD+3EDE: cmp ecx,eax                   ; obere Original-Grenze
LCOD+3EE0: jbe +3FA0                     ; nächste Dreier-Lane
LCOD+3F07: mov eax,[ebp-04h]            ; aktuelle Voice-Nummer
LCOD+3F0A: or al,60h                    ; VTFT index 3 | Stimme
LCOD+3F0E: call +3D44                   ; tatsächliche EMU8000-Lesung
LCOD+3F71..+3F7E: bedingter CCCA-Read
LCOD+3F8C: add dword ptr [ebp-04h],3     ; nächste Stimme derselben Lane
LCOD+3FA0: add dword ptr [ebp-14h],20h
LCOD+3FA4: inc dword ptr [ebp-0Ch]      ; nächste von 3 Lanes
LCOD+3FA7: cmp dword ptr [ebp-0Ch],3
LCOD+3FC9: call +3CB1                   ; gewählte Stimme, DCYSUSV-Freigabe
```

Die originale Grenze ist **dynamisch `[device+0x06]`** (im Live-Vollscan: 32). Der Quellcode enthält in diesem Pfad **keinen** harten 30-Stimmen-Deckel. Die Hardware hat 32 physische Voices; die drei Lanes können daher auch 30 und 31 erreichen.

## Zweifacher authentischer Laufzeitbeweis

Die originalen privaten CPU-/EMU8000-Port-Spuren sind **nicht öffentlich**. Die beiden Owner-Archive sind SHA-256-gepinnt und alle unten angegebenen Werte wurden direkt aus ihren ursprünglichen `Original-CPU-IO.trace`-Ereignissen ausgezählt:

| Original-Quelle / Rücksprungadresse | Sitzung A (16.11.) | Sitzung B (17.11.) |
|---|---:|---:|
| LCOD+3F13 VTFT-Read für **Stimme 30**, E22=**0x7E** | **2.686** | **2.688** |
| LCOD+3F13 VTFT-Read für **Stimme 31**, E22=**0x7F** | **2.649** | **2.625** |
| LCOD+3FCE DCYSUSV-Freigabe für **Stimme 30**, E22=**0xBE** | **101** | **108** |
| LCOD+3FCE DCYSUSV-Freigabe für **Stimme 31**, E22=**0xBF** | **117** | **122** |
| Gesamtzahl ursprünglicher Modus-0 VTFT-E22-Selektoren | 91.138 | 92.278 |
| Gesamtzahl ursprünglicher Modus-0 Freigabe-E22-Selektoren | 5.878 | 5.878 |

Beide Aufnahmen enthalten bei beiden genannten echten Original-Rücksprungadressen **sämtliche 32 physische Voice-IDs 0–31**. Der erste Original-VTFT-Aufruf für Stimme 30 verwendet außerdem den tatsächlichen 86Box-helper-EIP `C1018F3F` und den zurückgemeldeten LCOD-Rücksprung `C10190F7`; für die Freigabe von Stimme 31 kommt der tatsächliche helper-EIP `C1018EAB` mit Rücksprung `C10191B2`. Es handelt sich somit nicht um einen aus MIDI-API-Erfolg erschlossenen, sondern um einen tatsächlichen **ausgeführten Original-Treiberpfad**.

- Private ältere ZIP: `P003-20261116T184835Z-c56deac6-ERGEBNIS.zip`, SHA-256 `6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944`.
- Private neuere ZIP: `P003-20261117T010413Z-9c86c813-ERGEBNIS.zip`, SHA-256 `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`.
- [SHA-gepinnter, privater Read-only-Gegenprüfer (nur Quellcode, keine Gästedaten)](../tools/p003_audit_win95_original_mode0_32_private.py)

## Zusätzlicher echter Win95-Nachweis: Rangwahl und unmittelbarer Voice-Start

Die bisherigen Originalzahlen belegen nicht bloß, dass die Hardware-Register für physische Stimmen 30/31 gelesen werden. Sie belegen eine **tatsächliche Auswahl, anschließende Freigabe und sofortige VTFT-Initialisierung**:

- Sitzung A: Stimme **30** insgesamt **101-mal**, Stimme **31** insgesamt **117-mal** als Original-Modus-0-Ergebnis ausgegeben.
- Sitzung B: Stimme **30** insgesamt **108-mal**, Stimme **31** insgesamt **122-mal** als Original-Modus-0-Ergebnis ausgegeben.
- **Rangbasierte Auswahl nach vollständigem Dreier-Lane-Scan:** In Sitzung A erfolgt eine Original-Freigabe der Stimme 30 **33-mal** beziehungsweise der Stimme 31 **50-mal**, obwohl die **unmittelbar letzte VTFT-Registerlesung** die letzte Stimme der dritten Lane (**29**) betraf. In Sitzung B sind es entsprechend **40** und **56** Fälle. Weil der Originalcode bei einem sofortigen Idle- oder Sample-End-Treffer **vor** dem Durchlauf bis Stimme 29 abbricht, sind dies konkrete Fälle einer später freigegebenen **bereits gespeicherten Bestscore-Stimme**, keine bloßen frühen Leer-Stimmen-Treffer. Nicht als vollständige akustische Voice-Steal-/PCM-Parität interpretieren.
- **Ausnahmslos in allen 448 Fällen beider Sitzungen (218 + 230):** Der originale Programmablauf schreibt direkt nach dem Auswahl-Callback `LCOD+3FCE` zunächst `DCYSUSV=0x00FF` an `A20`, dann über den nachfolgenden Original-Caller `LCOD+3E0C` den echten `VTFT-DATA0`-Wert `0x0000FFFF` an dieselbe physische Voice. Die vier aufeinanderfolgenden I/O-Transaktionsereignisse sind `A20=0x00FF`, `E22=0x60|voice` (`0x7E`/ `0x7F`), `620=0xFFFF`, `622=0x0000`. **448/448 vollständig** und ohne I/O-Sequenz-Abweichung. Die letzten beiden halben Portdaten ergeben das echte 32-Bit-Originalwort.
- Originalbezug: `LCOD+3FBA..3FCE` gibt den ausgewählten physikalischen Voice-Index frei; `LCOD+3DDC..3E0C` initialisiert anschließend `VTFT` mit `0x0000FFFF` (Schreiben über den originalen DATA0-Writer). Der numerische Stimme-Index bleibt in beiden Schritten unverändert.

Der neue [SHA-256-gepinnte, nur lokal auf den privaten Originalarchiven ausführbare Auditor](../tools/p003_audit_original_mode0_highvoice_select_start_private.py) kontrolliert **beide unabhängigen Sitzungen**, die originalen Rücksprungstellen und die komplette Folge der vier Original-I/O-Ereignisse; er gibt nur aggregierte Zahlen aus. Auf GitHub werden **keine** Windows-95-Tracebytes, Creative-Binärdaten oder ROM-/SF2-Daten gespeichert.

Ergänzende **synthetische** C++23-Regression: [Stimmen 30/31 bei normaler Score-Rangwahl, Gleichstand, Sample-Ende und frühem Return](../tests/p003_original_mode0_highvoice_selection_ties.cpp), [GitHub CI – PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44008082076). Dies ist kein Ersatz für den oben ausdrücklich getrennten Original-Win95-Laufzeitbeweis.

## Separat geprüfte Forschungsimplementierung

Im isolierten Branch `research/p003-v5-voice-row-capture` wertet der Modus-0-Dispatcher nun **alle `device+0x06` physischen Stimmen** aus, einschließlich 30 und 31. Die gesonderte Source-Ceiling-Eingabe muss exakt mit der verifizierten Gerätegrenze übereinstimmen; ein widersprechendes `30` bei originalem `32` scheitert bewusst **fail closed**. Die bisherigen dreigeteilten Scans, vorzeitigen Return-Pfade, Einzelregister-Nachweise und Fehlerstatus bleiben unverändert.

Die Ausgabehelfer für originale VTFT- und DCYSUSV-Port-Wörter akzeptieren **physische Voice 30/31**, aber weiterhin nicht 32 oder höhere. Die alte ausdrücklich auf 30 musikalische Stimmen begrenzte Mono-Kurzhilfe bleibt für ihren bisherigen, engeren Vertragsumfang erhalten; der **Original-Dispatcher** ist davon unabhängig und benutzt die richtige physische 32-Voice-Reichweite. Eine spätere produktive C++23-Portierung muss entscheiden, ob diese zwei Bereiche getrennt bleiben oder als ein originalgetreues Interface zusammengeführt werden.

**Negative Regressionen:** Die echte, unveränderte gepinnte Entwickler-Revision `940098bd3e35f5b2b2b7c94ec9283b5b45067990` kann den Quellfall „nur Stimme 30/31 ist frei“ weiterhin nicht in Modus 0 auswählen. P003 besitzt einen unabhängigen C++23-Gegentest, der den Fehler als echten Unterschied reproduziert.

- [Gezielter 30/31-Original-Regressionsfall](../tests/p003_original_win95_mode0_32_physical_voices.cpp).
- [15.000 deterministische Testzustände × alle drei Original-Modi](../tests/p003_original_modes_deterministic_differential_15000.cpp), inzwischen einschließlich **vollständiger 32-Voice-Modus-0-Dispatcher-Prüfung**.
- [GitHub-CI-Build mit bestehender Modus-0/1/2-Regressionssuite + Negativtests: PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44006536172).

## Abnahmegrenze

Nachgewiesen sind **Original-x86-Codepfad**, **tatsächliche Registerauswahl und Freigabeschreibzugriffe** der Original-Voices 30 und 31, sowie **synthetische Auswahlgleichheit** des korrigierten Forschungscodes. **Nicht** nachgewiesen sind: ein vollständiger nativer Creative-Treiber, autonom identisches Timing, allgemeine 86Box-/AWE64-Hardware-Parität oder bitidentischer trockener VST3-PCM-Ausgang im Gesamtsystem.

Dieser Befund bleibt gesammelt im Forschungszweig. **Keine Einzelübergabe an den Entwickler und keine Unterbrechung seiner Arbeit.**
