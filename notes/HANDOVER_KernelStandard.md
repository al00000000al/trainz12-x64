# Hand-over: KernelStandard_7c plan, where I stopped (2026-10-19)

Task: write `D:\trainz12-decomp\notes\KernelStandard.md` (reconstruction plan for KernelStandard_7c.dll, five points: export
inventory, per-group state/deps/64-bit, inline layouts, unit order + diff tests, risks + recommended first unit).
Read-only outside `notes\` and `decomp\`. Do not edit src, CMake, implemented.txt, MODLOG, git. Never launch the game.
Always quote paths under `D:\universal modder` (an unquoted `start` there made a "cannot find D:\universal" dialog).

## State

Analysis is done; the document is drafted but **not yet rendered into its final file**.

| Item | Where | State |
|---|---|---|
| Full decompile (2250 functions, 0 failures) | `D:\trainz12-decomp\decomp\KernelStandard_7c.dll.c` | done |
| Draft of the deliverable with `{{T1}} {{T2}} {{T4}}` placeholders | `notes\KernelStandard.md.tmpl` | written, all 6 sections |
| Correction script for the draft | `notes\data\_patch.py` | written, **NOT run yet** (it fixes ~30 wrong or unverified statements found while re-checking, see below) |
| Generated tables (per group, per importer, per class) | `notes\data\tables.json` (made by `notes\data\tables.py`) | done |
| Final file `notes\KernelStandard.md` | | **missing** |

## To finish (about 5 minutes)

1. `python D:\trainz12-decomp\notes\data\_patch.py` (prints `MISSING:` for any replacement whose anchor text did not match; fix those by hand in the .tmpl).
2. Render: replace `{{T1}}`, `{{T2}}`, `{{T4}}` in `KernelStandard.md.tmpl` with the strings in `notes\data\tables.json` keys `T1`, `T2`, `T4`
   and write `notes\KernelStandard.md` (UTF-8). Small python one-liner, nothing else needed.
3. Re-read the result once: the "Summary" numbers (1762 exports = 1649 code + 113 data; 666 imported by something; 1096 only internal;
   `.text` 327 509 B; 2276 Ghidra functions / 292 521 B; class-attributed bytes 199 036) are verified; section 3 rows marked with `?` or "lower bound" are not.
4. Optionally publish/send the .md to the parent session; delete `*.tmpl`, `_patch.py` after rendering if not wanted (keep `notes\data\` and `notes\ghidra\`, they are the reproducible inputs).

## Main findings (all in the draft)

* 1762 exports; only 666 are imported by anything (trainz.exe 447, 6 tool exes 191-311, 21 Jet modules 6-168). The module is also a static
  JPEG *encoder* (41.6 KB, only `Bitmap::SaveJPG`); JPEG decode goes through ijl10.dll ordinals 2/3/4.
* Foundation state is mostly **not exported**: string heap `#StringData` (0x100adc18), PStringCache lock (0x100acb58), AnsiString null buffer (0x100ae254),
  Alloc free lists + 16 striped critical sections (0x10074fa8..), FileSub subst/archive list (0x100adbc4..d4). Hence the strategy: replace
  **consumers first, foundations last** (Alloc/PackedHeap/PString/strings/Thread/Log as one closed final unit).
* CRT gotchas: `Alloc` = `malloc` from msvcr80 with a 4-byte tag; `File` stores a CRT fd at +0, so anything touching `File` must link the same msvcr80
  (static CRT is fine only for units that exchange plain data / Alloc memory). Several exported objects embed MSVC8 STL members
  (`PackedHeap`/`MP` 0x20/0x2c, `File::Find` 0x440, `IOSlaveMemoryDynamic` 0x18): sizeof must be preserved by hand-rolled containers.
* x64 hazards: `Window` keeps `this` in window extra bytes via `Get/SetWindowLongA(h,0)`; ExceptionHandler is x86-specific (EBP walk, CONTEXT.Eip, FS:[0]);
  Rand/LCRand/Time use x87 with `fistp` (bit-exactness risk); Alloc returns pointers 4 mod 8.
* Units: U0 forwarding proxy; U1 Crc+StdMath(+2 KB `s_sqrtLUT`, read inline by 10 modules)+Vector2/3+int-log helpers+OptimalRects+Eigen3;
  U2 Matrix3/4+Quaternion; U3 colour/small PODs; U4 sync primitives; U5 Rand; U6 small platform; U7 Config(+flex); U8 Bitmap; U9 JIRF+streams;
  U10 File family (+FileSub = .ja archive reader, most compatibility-critical); U11 OS/Window; U12 diagnostics; U13 Thread/TLS/Time; U14 foundation.
* **Recommended first unit: U0 + U1** (35 exports, 14 imported, ~4.3 KB, bit-testable in one process).

## Things I corrected in the draft but you should know are *unverified*

File size (>= 0x39), Options layout (0xc, DLList head/tail/count by analogy with SDK), DLTreeNode/RBTreeNode sizes, JIRFFile 0x144 (lower bound),
FileSubArchive = .ja reader (inferred from names + archive strings), the `.jet` sections (purpose unknown), `Crc` polynomial, `s_sqrtLUT` generator
(my guess formula did not match; table is a static 2 KB array in `.data`).

## Reproducing the data

* Ghidra scripts I added: `notes\ghidra\ClassGraph.java` (calls/data refs), `InsnStats.java`; run with
  `analyzeHeadless D:\trainz12-decomp\project trainz12/bin -process KernelStandard_7c.dll -noanalysis -readOnly -scriptPath D:\trainz12-decomp\notes\ghidra -postScript <Script> <out>`
  (use PowerShell with the JDK path from `tools\ghidra_run.cmd`; Git Bash mangles the quoting).
* Python helpers in `notes\data\`: `model.py` (export -> class -> group), `owners.py`, `edges.py`, `classtab.py`, `units.py`, `tables.py`, `fx.py <name>` (print decompiled functions by header substring).
  They read import tables from the scratchpad `imports.json` (made from `D:\games\trainz 2012 VDK\bin`; if lost, rebuild with pefile).
- Note: model.py reads imports.json/exp.json from the scratchpad path; copies are now in notes\data (edit SP path in model.py to D:/trainz12-decomp/notes/data).

## Done (2026-10-19)
Rendered `notes\KernelStandard.md` (patch applied with no MISSING anchors) and added the Options, IOSlave and x64-harness notes. The `.tmpl` and `_patch.py` are kept as inputs.
