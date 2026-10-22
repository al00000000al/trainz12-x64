# trainz12-x64

A 64-bit port of Trainz Simulator 2012 that keeps add-ons working. The approach: the game's Jet engine modules
are rewritten one by one as drop-in 32-bit DLLs that run inside the original game. Each one is checked against
the original in the same process (differential tests) and in game. Once nothing original is left, the code is
built for x64.

This repository holds **only our own work**. It contains no game files, no original DLLs, no Ghidra output and
no Jet SDK files. The modules in `src/` are a clean C++ rewrite made by studying the game's binaries. You need
your own installed copy of the game to build against it, test it or run it.

## What is here

- `PORT_PLAN.md`: the approach and the numbered queue of next steps.
- `MODLOG.md`: confirmed facts about the binaries (with addresses) and what each step did and measured.
- `notes/`: per-module analysis (`KernelStandard.md` is the main one: units, layouts, kept bugs, test notes).
  `notes/data/*.py` and `notes/ghidra/*.java` are the scripts behind it (their data dumps are not included).
- `src/`: the rewritten modules and their tests (CMake):
  - `Kernel_7c`: all exports ours.
  - `KernelStandard_7c`: 861 of 1762 exports ours; the rest forward to the renamed original.
  - `InputSystem_7c`, `InputWin32_7c`, `KernelResource_7c`, `KernelResources_7c`, `Render2DCore_7c`.
  - `include/jet/`: our declarations of the Jet classes with their binary layouts.
  - `tests/diff_*.cpp`: differential tests. Each loads our module and the original in one process and runs the
    same random scenarios on both.
- `probe/`: PowerShell scripts that start the game and check the menu, real input and the Launcher.
- `tools/`: export / import dumps, `.def` and import-library generation, thunk checks, the SSE lifter,
  screenshots through desktop duplication, census.
- `ghidra_scripts/`: headless Ghidra scripts (function stats, decompile, vtables, TrainzScript native mapping).
- `gs_native_api.tsv`, `gs_native_handlers.tsv`: the TrainzScript native API and the trainz.exe address of each.

## Building (32-bit drop-in modules)

Requirements: Visual Studio 2022 or later (x86 tools), CMake, Python 3, and an installed Trainz 2012 (build
61388). For `KernelStandard_7c`, libjpeg-turbo from vcpkg (`vcpkg install libjpeg-turbo:x86-windows-static`).

The CMake files still use the paths of the original setup (`D:/trainz12-decomp/...`, `D:/games/...`). Change
`IMPLIB`, `EXPORTS`, `ORIG_KS` / `ORIG_IS` and `VCPKG_X86` in `src/CMakeLists.txt` to your paths. The export lists
(`exports/*.tsv`) and the import libraries of the original modules (`implib/`) are generated from your own game
files with `tools/dump_exports.py` and `tools/gen_implib.py`. A few data tables are also read from your installed
game at build time and never committed.

```
cmake -S src -B build32 -A Win32
cmake --build build32 --config Release
```

To try a module in game, rename the original (for example `KernelStandard_7c.dll` to `KernelStandard_7c_orig.dll`)
and put ours next to it. To run a test, pass it that directory: `diff_ks_bitmap_draw.exe "<game>\bin"`.
Keep a backup of the original files.

## How to help

Pick the next step in `PORT_PLAN.md`, read the notes for its module, and follow the existing pattern: rewrite a
unit, add its names to `implemented.txt`, write or extend a differential test until it shows 0 mismatches, then
check in game. `MODLOG.md` records what has been measured.
