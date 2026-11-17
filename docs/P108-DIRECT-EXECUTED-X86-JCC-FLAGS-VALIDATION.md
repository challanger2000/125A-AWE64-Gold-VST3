# P108 – Unmittelbare x86-CF/ZF-Aufzeichnung an den originalen Creative-Jcc-Entscheidungen

**Status (17.11.2026):** Sourcecode und synthetische Gegenproben implementiert; Original-Windows-95-Vergleich dieser **neuen** Instrumentierung noch **nicht** ausgeführt. Dies ist kein P003-CPU-Timing- oder VST3-PCM-GO. PR #8 bleibt Draft.

## Ausgangslage

Der **echte** private P107-Originaltest vom 16.11.2026 lieferte im exakt restaurierten Original-Win95-Gast 4.176 fehlerfreie Creative-MIDI-Aufrufe und 36.784 tatsächlich beobachtete x86-Guest-PC-Ereignisse. Originale bedingte Sprünge im LCOD-Allocator: **3.474**, davon **3.458** direkte Sprungnachfolger eindeutig. Bei **16** Jcc (15× `LCOD+0x40F1`, 1× `+0x40CB`) fehlte der unmittelbar nächste protokollierte x86-PC. P107 lieferte für diese Fälle bewusst **UNKNOWN**.

## Neue technische Implementierung

`tools/p108_direct_executed_jcc_flags.py` wird **nach** der SHA-festen P107-JIT- und P107-Interpreter-Quellpatchung auf eine **wegwerfbare Kopie** des Original-86Box-Forks `turican0/86Box-AWE32 v6.0-AWE32-0.93` angewendet. **Nur** `src/codegen/codegen_x86-64.c` wird weiter modifiziert – der Original-EMU8000-DSP, die Win95-VxD, der ROM-Chipdatensatz, Creative-SoundFonts und das Produkt-VST3 **bleiben unverändert**.

P108 hängt seine neue Messung an den bestehenden P107-Callback `p107_x86_executed_guest_pc` an. Dieser Callback läuft **tatsächlich beim ausgeführten Gast-x86-Instruktionsereignis**, und zwar sowohl für aus JIT-Blöcken ausgeführte Befehle als auch für zuvor fehlende Interpreter-Anweisungen. Keine Compiler-/Disassembler-Spekulation wird als ausgeführter Sprung gezählt.

Für genau die fünf originalen, bereits independently disassemblierten Jcc-Stellen liest der neue Beobachter die **originalen** 86Box-Lazy-Flag-Reader `ZF_SET()` und `CF_SET()` aus `src/cpu/x86_flags.h`. Die Werte werden direkt in der ursprünglichen 86Box-Codegenerierung `src/codegen/codegen_ops_jump.h` für bedingte Sprünge verwendet. Für diesen nicht-mutierenden Flag-Leseweg gilt:

| Original LCOD | Jcc | Predicate |
| --- | --- | --- |
| `+0x4027` | JNE | `!ZF` |
| `+0x40CB` | JA | `!CF && !ZF` |
| `+0x40E4` | JE | `ZF` |
| `+0x40F1` | JAE | `!CF` |
| `+0x4107` | JB | `CF` |

Die neue Aufzeichnung ist **standardmäßig deaktiviert** und braucht in der bereits privaten 86Box-Laufzeit explizit `AWE64_P108_DIRECT_BRANCH_TRACE` zusätzlich zu den P107-Kontrollen. Die neue Original-Branch-Datei beginnt mit `# P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1`, gefolgt von `B <P107-I-seq> <86Box-TSC> <CS> <guest-pc> <ZF> <CF> <taken>`. So wird jede direkt erfasste Flagentscheidung **eindeutig** einer tatsächlich ausgeführten P107-Guest-Instruktion zugeordnet. Der TSC ist **nur Diagnostik auf Blockebene**, keine selbständige instruktionsgenaue x86-Clock. Der Logger gibt keine Originalbytes/ROM/Originalregisterwerte nach GitHub aus.

Der neue `tools/p108_verify_direct_jcc.py` liest ausschließlich **private** Original-P107-I-Spur plus P108-B-Spur. Der Audit besteht **nur**, wenn **jeder** P107-Original-Jcc genau einen passenden P108-Flagrecord hat, Sequenz/PC/CS/TSC übereinstimmen und **jeder** vorhandene direkte Nachfolger in P107 exakt dasselbe `taken`-Ergebnis ergibt. Widersprüche, fehlende Records, doppelte Branchereignisse, fremde LCOD-Adressen und unvollständige Aufzeichnungen führen zu **NO-GO**. Für ehemals fehlende Next-PC-Ereignisse kann der Auditor erst *nach* realem Gastlauf die ursprünglichen CF/ZF-Kriterien als zusätzlichen, eigenständigen Originalbefund ausweisen; er schreibt **keinen erfundenen Folge-PC**.

## Tests / Freigabegrenzen

`tests/test_p108_direct_executed_jcc.py` und `tools/p108_verify_direct_jcc.py --selftest` verwenden **nur fiktive** x86-PCs und CPU-Flag-Kombinationen. Der **tatsächlich erfolgreiche** [GitHub-Actions-Lauf #43987959999](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43987959999) bestätigt **3/3 PASS**: originale Quell-/Negativtests, echter instrumentierter **Ubuntu-x64-86Box-Build** und echter instrumentierter **Windows-2022-x64-86Box-Build**. Beide verwenden den SHA-verifizierten unveränderten 86Box-Fork als Ausgangsbasis und schalten P108 ausschließlich in der eigenen Wegwerfkopie frei. Es werden **keine** Original-Win95-, Creative-VxD-, ROM-, SF2-, WAV-, MIDI-, CPU-TSC- oder x86-Rohspuren in GitHub verwendet.

**Der Nachweis 16/16 nun korrekt erfasster Original-Zweigbedingungen steht noch aus**, solange der neue Build nicht privat mit der unveränderten Original-Win95-VM und exakt dem früheren 3-Bank-Stresstest ausgeführt und gegen die authentifizierte P107-I-Aufzeichnung ausgewertet wurde. Selbst ein PASS wäre nur ein Nachweis der **Branchlogik**, nicht die automatische 91/93/95-Takt-EMU8000-Ausgabe und nicht bitidentische VST3-Audio-Parität. Kein Produkt-Merge.

**Nutzeraktion erforderlich: keine.**
