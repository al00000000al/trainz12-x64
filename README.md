# trainz12-x64

Tooling and notes for a 64-bit port of Trainz Simulator 2012 that keeps add-ons working.

This repository holds **only our own work**: analysis scripts, Ghidra scripts, notes and the plan.
It contains no game files and no decompiled code. You need your own copy of the game to use any of it.

- `PORT_PLAN.md`: the approach (incremental reimplementation inside the running game, then x64).
- `MODLOG.md`: confirmed facts about the binaries, with addresses.
- `tools/`: PE census, export demangling, TrainzScript API inventory, backup.
- `ghidra_scripts/`: headless Ghidra scripts (function stats, decompile, TrainzScript native mapping).
- `gs_native_api.tsv`: the TrainzScript native API as declared in the game's `.gs` headers.
- `gs_native_handlers.tsv`: which address in trainz.exe 1.5.0.61388 implements each native.
