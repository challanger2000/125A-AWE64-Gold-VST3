# P001 — echte private Creative-Mediendaten (11. November 2026)

Die Nutzerdateien wurden **aus der privaten ChatGPT-Dateibibliothek** materialisiert und direkt in einer isolierten Arbeitsumgebung überprüft. Ihre Bytes werden **nicht** in den öffentlichen Quellcode, ein GitHub-Issue, einen Workflow oder ein öffentliches Artifact übernommen.

| Private Eingabe | Gemessene Bytes | SHA-256 | Prüfung |
|---|---:|---|---|
| `awe32(1).raw` (Original-EMU8000-ROM) | 1.048.576 | `4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e` | SHA und AWE-DUMP-2-Byte-Layout bestätigt |
| `SYNTHGM(2).SBK` (Creative `1MGM`-Bank) | 34.832 | `fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586` | RIFF/sfbk, `EMU8000`, `1MGM`, `SBAWE32`, 130 phdr-Datensätze (mit Endmarkierung), 51 48-Byte-shdr-Datensätze |

Diese SHA-256 sind konsistent mit der schon früher in 125A dokumentierten originalen Creative-ROM/SBK-Paarung. Das SBK ist ein **ROM-Instrumentdeskriptor**, kein 1-MiB-Wave-ROM und keine beliebige SF2-Samplebank; insbesondere enthält dessen `sdta` keine gewöhnlichen eingebetteten `smpl`-Wellenformen. Die 48-Byte-Creative-shdr-Daten dürfen nicht still als 46-Byte-SF2-shdr interpretiert werden.

Reproduzierbare Prüfung:

```powershell
python tools/verify_original_media.py --rom "PRIVATE/awe32.raw" --sbk "PRIVATE/SYNTHGM.SBK"
```

Es wurde eine **manipulierte Kopie** des ROMs als Negativkontrolle geprüft: SHA-Abweichung führte erwartungsgemäß zu Fehlerstatus. Keine Medien werden durch diesen Test verändert.

**Wichtige Beweisgrenze:** Diese real ausgeführte Prüfung beweist unverfälschte Originaldateien, nicht bereits die ROM-Klangwiedergabe über das gepinnte VST-Chipmodul und nicht die Bitidentität mit einer unabhängig gestarteten Win95/86Box-VM. Die in der Bibliothek vorhandene `AWE32-20261111-140857.wav` besitzt 8.531.880 Stereo-Frames bei 44,1 kHz/PCM16; eine dazugehörige zeitgleiche `EMU8K_TRACE`-Registerspur war nicht verfügbar. Ohne diese Original-Registerdaten darf die WAV nicht als für P001 identisch getriggerte PCM-Referenz gelten.

**P001 genuine-ROM direct original-core PCM parity: PASS** (Windows #43068548526, 176.400 PCM16-Samples identisch, nicht stumm); **P001 independent Win95/Creative/86Box VM parity: NOT RUN / NO-GO**. Das echte ROM wurde aus dem gepinnten offiziellen 86Box-ROM-Repository byteidentisch zum privat vorliegenden Nutzer-ROM bezogen und nach dem Test gelöscht. Ein echter Windows-VST3 mit festen ROM-Testnoten wurde separat mit 537 SDK-Validierungen und SampleOn/Off-Hostprüfung abgenommen, ersetzt jedoch weder die unabhängige VM-Referenz noch den späteren Bankparser.
