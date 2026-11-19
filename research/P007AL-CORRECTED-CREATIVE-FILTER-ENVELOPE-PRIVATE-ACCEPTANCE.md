# P007AL — korrigierte Original-Creative-Klangabnahme, 18.11.2026

**Ausgangspunkt:** P007AJ beweist 4/4 originale Generatorregister, P007AK-Originalklangmessung meldete fälschlich pauschal „0/4“, spätere Offline-Reanalyse der echten PCM-Banddaten: Attack/Decay/Sustain jeweils reproduzierbar messbare Unterschiede, Release nicht verwertbar wegen schneller Amp-Stummschaltung.

## Drei konkrete Fehler im bisherigen Prüfaufbau

1. **Falsche Polarität nach Lautstärkestatistik ausgewählt:** P007AK gab zur Wahl −96 (Score 0,00854 vs. +96 0,00784), was am tatsächlichen Chip **PEFE=0x0040** (+64, Filteröffnung) produziert. +96 ergibt **PEFE=0x00C0** (−64, Filterabsenkung). Mit IFATN=0xB232 liegt Startcutoff im hohen Bereich; eine zusätzliche Öffnung kann an die obere Begrenzung geraten. P007AL wählt +96 **bewusst nach verifizierter Hardwarepolarität**, nicht mehr nach Ausgangs-RMS. Diese Entscheidung ist ein Testverfahren, keine Änderung im Treiber/Fork.
2. **Amp-Release zu kurz:** In P007AK war das Signal nach NoteOff schon ab ca. 750ms stumm. Der unabhängige Lautstärke-Release läuft über **Original-Creative-NRPN 15 (Volume Envelope 2 Release)**; Dokumentation Creative ADIP v3.00: unsigned raw 0…5940, Einheit 4ms. P007AL setzt 1800 = **7,2 s** vor **jedem** A/B/A-Ton der Messserie (Scenario 4–12). NRPN24 (Filtertiefe) wird ebenfalls nach jeder Controller-Rückstellung erneut gesetzt.
3. **Unpassender 4-dB-Filterwert:** P007AK akzeptierte nur >=4dB Spectral-Shape in direkt aufeinanderfolgenden 250ms-Fenstern. P007AL benutzt >max(0,25dB,5*Referenzdrift+0,05dB) für mindestens 3 aufeinanderfolgende Fenster gleicher Vorzeichenrichtung, Referenzdrift-Grenze 0,30dB und eine zusätzliche **−100-dBFS-/3-Fenster-Amp-Signalbedingung** für Release. Ein einzelner Frequenzpeak oder eine fast stumme Restfahne reicht nicht.

## Implementierte Original-Gates

- `P007ActualHostQA.exe <VST3> --private-filter-corrected`: **nur Original-Win95+Creative+unveränderter 86Box-AWE32/EMU8000-Kern**; keine künstliche Wellenform.
- Jede tatsächliche EMU8000-Portregisterfolge wird passiv erfasst. Für Szenarien 4–12: **PEFE muss exakt 0x00C0** sein; Filter-A/B/A darf nicht die Amp-Release-Register variieren; das **Volume-Release `DCYSUSV` muss vom alten Wert 0x8029 weg zu einem neuen, in allen Szenarien gleichen Wert wechseln**.
- Filter-Release `DCYSUS` muss B abweichend A sein und nachher auf A zurückkehren. Eine bloße Registeränderung wird **nicht** als Klangnachweis gezählt.
- Spektral-CSV: `P007AL-ORIGINAL-PCM-SPECTRAL-BANDS.csv`; Register-CSV: `P007AL-ORIGINAL-EMU8000-REGISTER-EVENTS.csv`. Beides erst außerhalb des Audio-Prozesses auf den Datenträger geschrieben.
- Ergebniszeilen: `P007AL_CORRECTED_ORIGINAL_PCM_RESULT`; 4/4 nur dann `P007AL_FOUR_ORIGINAL_FILTER_ENVELOPES_PCM_PROVEN`.
- Bereits vorhandener `--private-spectrum` und `--private-regtrace` bleiben erhalten. `P007ALCorrectedSpectralProofTest.cpp` kontrolliert nur den Algorithmus und Creative-NRPN-Encoder mit synthetischen Signalen und beansprucht **nie** einen originalen Win95-Klangnachweis.

## Keine Änderungen am Produkt-DSP

Unverändert bleiben Original-Emulatorkern `snd_emu8k.c`, der originale Creative-Windows-95-Treiber, die Hardware-GUI, der eigenständige VST3-Adapter und 0%-FX-Sends.

**Abnahme:** Die echte P007AL-Ein-Klick-Messung auf Windows ist die einzig gültige Grundlage für 4/4. Wenn sie weniger meldet, bleiben die betreffenden Fälle unbestätigt. Keine nachträgliche Umetikettierung.

**Referenzen:**
- Creative: *SB AWE32 Developer's Information Pack v3.00*, Part IV, NRPN 9/15/24: https://www.phatcode.net/res/244/files/adip301.pdf
- Pin: turican0/86Box-AWE32 `fee1e81977824fd2d5eed4d239589dc30d251dd7`; `src/include/86box/snd_emu8k.h` defines signed PEFE byte, `src/sound/snd_emu8k.c` uses fixed modulation height to compute filter target, independent volume envelope fades the voice.
