# Trainz 2012 → 64-bit: port plan

Goal: the game process (`trainz.exe` + Jet modules) rebuilt from reconstructed C++ source as x64, loading the
same content (.ja, .im/.kin, .texture, config.txt, TrainzScript .gs) so every installed add-on keeps working.

Method: **incremental reimplementation inside the running 32-bit game**, the way re3 (GTA III) and OpenRCT2
were done. Each reconstructed function or module replaces the original in the real game and is checked there
before the next one. The x64 build is only attempted once nothing original is left. This gives a playable game
at every step and catches mistakes while they are small.

## Phase 0: infrastructure (now)
- Ghidra project with every native module of the game process, auto-analysed.
- Free names applied: Jet DLL exports (12k mangled C++ signatures), MSVC RTTI (4186 classes, vtables),
  `Class::Method>` log strings, imported names.
- Headless scripts so analysis is reproducible: export function lists, call graphs, per-module progress.

## Phase 1: map
- Per module: functions, classes, vtables, who calls whom, which content formats it reads.
- Inventory of the formats add-ons use and the code that parses each one.
- Progress table: functions named / typed / reimplemented / verified.

## Phase 2: replace module by module (still 32-bit, still playable)
- Order: smallest, best-named Jet modules first (Kernel, Lexer, KernelResources, Render2DCore, Input*),
  then Render*, Interface, Water, then trainz.exe subsystems (asset DB, GS compiler/VM, train physics, world).
- Each rebuilt module is a drop-in DLL with the **same exports**, so the original exe loads it unchanged.
- trainz.exe functions are replaced in place through a loader DLL that patches calls to the new code.
- Verification per module: game starts, a fixed test route loads, the same JetLog output, the same frame
  (screenshot diff), the same GS script behaviour on a test session. Recorded traces act as an oracle.

- Granularity: stateless classes can move one at a time; stateful subsystems move as a whole module
  (see MODLOG "Replacement granularity"). Modules the game never loads (Lexer, RenderDirectX9, movie players)
  come last or not at all.

## Phase 3: x64
- Fix 64-bit hazards in the source: pointer-sized fields in on-disk structs, `int`↔pointer casts,
  32-bit handles in the GS VM, inline asm/x87 maths.
- Swap deps for x64 builds (table in MODLOG.md).
- Replace the embedded XULRunner 1.9 (menu HTML panels) with a 64-bit HTML engine such as WebView2.
- The live renderer is OpenGL: port RenderOpenGL first; it is also the natural base for a modern backend.
- Then real use of the hardware: multi-threaded asset loading, bigger caches, modern renderer backend.

## Phase 4: add-on compatibility
- Load-test every asset in the user's install (count, errors, warnings) on the original and on the port,
  compare.
- A set of sessions with scripted rolling stock run on both, compared by traces.

## Honest scale
trainz.exe alone is ~6.7 MB of x86 (tens of thousands of functions); with the Jet modules ~9 MB.
Comparable community projects took small teams 1–3 years. Phase 0–1 are days; Phase 2 is the bulk.
