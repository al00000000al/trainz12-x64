# Trainz Simulator 2012: 64-bit port, working log

Original install (read-only, never modified): `D:\games\trainz 2012 VDK`
Working copy for experiments: `D:\games\trainz 2012 x64`
Decompiler projects and decompiled output (private, never publish): `D:\trainz12-decomp`
Tools: `D:\tools` (Ghidra), scripts: `trainz12-x64/tools/`

## Facts confirmed (2026-10-18)

### Binaries
- Game process: `bin\trainz.exe`, version 1.5.0.61388. Top-level `Trainz.exe` (77 KB) is only a stub launcher.
- x86 (machine 0x14c), MSVC 2005 (linker 8.0, Rich build 50727), CRT `msvcr80/msvcp80`.
- **Not packed, no DRM wrapper**: standard sections, `.text` entropy 6.47. Has `.reloc` (relocatable).
- **LargeAddressAware is already set** on trainz.exe and the Jet DLLs, so the game already gets 4 GB on x64 Windows.
- trainz.exe: 6.69 MB `.text`, 1309 RTTI classes, 1 export. Imports ~1600 functions from the Jet DLLs by name.
- Extra section `.shr` (4 bytes, shared): likely a single-instance flag.
- Census of all native modules: `census.json` (from `tools/census.py`). Native code across bin/ = 23.5 MB,
  12 431 mangled C++ exports, 4186 RTTI classes.

### Engine ("Jet", Auran)
- Modular: `*_7c.dll` plugins registered by GUID in `bin\plugins.ini` (resource factories per file extension:
  `texture`, `kin`, `msh`, `fx`, `wav`, `mp3`, `terrain.txt`...). Modules can be swapped one at a time.
- Module sizes (code): RenderSystem 396 KB, Render3DCore 504 KB, Render3DFoundation 221 KB,
  InterfaceSystem 368 KB, KernelStandard 328 KB, WaterSystem 112 KB, InputSystem 81 KB, Kernel 60 KB,
  RenderOpenGL 152 KB, RenderDirectX9 78 KB, Lexer 22 KB, plus small ones.
- Jet DLL exports are **full C++ signatures** (e.g. RenderSystem 2686 of them): the module APIs are named for free.
- Renderers: `RenderDirectX9_7c.dll` (uses `d3dx9_42.dll`) and `RenderOpenGL_7c.dll`; shaders are NVIDIA Cg 2.2.
- TrainzScript runtime lives in trainz.exe (`GSRuntime`, `GSScriptInstance`, `GSMessage` RTTI).
- `JetLog.txt` lines are prefixed with `Class::Method>`: these strings are in the exe and name functions.

### Ghidra project (D:\trainz12-decomp\project\trainz12, Ghidra 12.1.4, JDK 21)
- Run with `tools/ghidra_analyze.cmd <log> <binaries...>`; per-module function lists land in
  `D:\trainz12-decomp\out\*.functions.tsv` (script `ghidra_scripts/ExportFunctionStats.java`).
- 21 Jet modules analysed: **21 223 functions, 14 348 named (68%)** straight from exports/RTTI.
  Best: Lexer 86%, RenderSystem 83%, KernelStandard 77%, InterfaceSystem 74%. Worst are the plugin modules
  with one export (RenderDirectX9 21%, InputDirectX 25%, DSMoviePlayer 26%): named via vtables/RTTI later.
- Gotcha: do not touch the JDK while Ghidra runs. A JDK upgrade replaced `jdk-21.0.11` mid-analysis and
  killed the first run of trainz.exe (2026-10-18 22:47); finished modules were already saved.
- No PDB exists for any module.

### Module replacement harness (confirmed in game 2026-10-18 23:25)
- Source of replacements: `D:\trainz12-decomp\src` (private), CMake + VS 2026 (MSVC 14.51), `-A Win32`,
  static CRT. Build: `cmake -S D:\trainz12-decomp\src -B D:\trainz12-decomp\build32 -G "Visual Studio 18 2026" -A Win32`.
- `tools/gen_def.py` makes the .def: every original export keeps name + ordinal; names in
  `<module>/implemented.txt` come from our code, the rest forward to `<module>_orig.dll`.
  link.exe (x86) rejects a .def forwarder for an undecorated C name (`JetGetPluginsKernel`): those go through
  `#pragma comment(linker, "/export:...")` in a generated `<module>_forwards.cpp`.
- Kernel_7c ABI: no STL types in any export; 14 vftables + 5 vbtables (virtual base PluginInstance) and
  9 static data exports. No malloc/new from msvcr80 (objects use Jet::Alloc from KernelStandard), so a
  different CRT is safe for this module.
- **Pure-forwarding Kernel_7c.dll proxy works in game**: copy reaches the main menu, both `Kernel_7c.dll`
  (ours) and `Kernel_7c_orig.DLL` are loaded (checked with 32-bit PowerShell; 64-bit can't list a WOW64
  process's modules), JetLog identical to the baseline line for line, clean exit.
- Test launch: `Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64'
  -ArgumentList '-windowed','-Width=1280','-Height=800'` (args taken from Launcher.exe). Main menu in ~20 s.
  Close with CloseMainWindow(): JetLog is only written on exit. The white "Forbidden" panel is the dead
  N3V news feed, same in the original.

### First reconstructed code: Jet::Version and Jet::UId (2026-10-18)
- `src/include/jet/{Version,UId}.h`, `src/Kernel_7c/{Version,UId}.cpp` (private tree). 25 of 256 Kernel_7c
  exports now ours; Version default-ctor closure and UId Read/Write/GetString/SetRnd/UIdNull still forwarded
  (they need DynamicBuffer/PString/Rand from KernelStandard_7c).
- ABI gotcha: most UId members are **`__cdecl` member functions** (mangled `QBA`/`QAA`, `this` on the stack);
  Ghidra shows them as thiscall with `in_stack_` vars. Constructors, operator= and Zero are `__thiscall`.
- Version = u16 build | u8 minor | u8 major, compared as one u32. UId = GUID layout; `<`/`>` compare
  data1, data2, data3 unsigned, then data4 bytewise. `UId(const char*)` copies 37 chars but zeroes index 36.
- **Differential test** `src/tests/diff_kernel_uid_version.cpp` loads ours and Kernel_7c_orig.dll in one
  32-bit process and calls both through the raw exports (thiscall emulated with __fastcall): 600 000 rounds of
  random, near-equal, zero and malformed-string inputs, **0 mismatches**.
- In game: main menu reached, JetLog identical to baseline, clean exit.
- **UId complete** (30/256 Kernel_7c exports ours): Read/Write/GetString/SetRnd and UIdNull added. Only the
  Version default-ctor closure is still forwarded in these two classes.
  Differential test extended with real KernelStandard objects (DynamicBuffer incl. truncated buffers, PString
  through the live PStringCache, SetRnd shape): 621 000 rounds, 0 mismatches. In game: identical JetLog.

### Calling into the original modules
- `tools/gen_implib.py` builds x86 import libs for every Jet module from its export dump
  (`D:\trainz12-decomp\implib\*.lib`; data exports marked DATA → declare `__declspec(dllimport)`).
  If our header declarations link against these, their mangled names are right.
- `tools/dump_imports.py <dll> <from dll> <undname>` lists what a module uses from another, demangled.
- `Jet::PString` = one `PStringCache::Node*`. Its ctor/copy/dtor are inlined everywhere (not exported);
  reconstructed: copy = Lock, ++node->refs (+0x10), ++cache->liveRefs (+4), Unlock; dtor = cache->Destroy(node)
  if both non-null. `PString::cache` is a private static, imported as DATA.
- A `__cdecl` member returning a class by value takes (this, hidden result*) on the stack in that order.

### What actually loads (main menu, 2026-10-18, 32-bit module list of the running game)
- Jet: Kernel, KernelStandard, KernelResource, KernelResources, Input{System,DirectX,Win32}, Interface{System,Resources},
  Render2DCore, Render3DCore, Render3DFoundation, **RenderOpenGL**, RenderSystem, WaterSystem, DebugInterface.
  `out\loaded_modules_menu.txt`.
- **The renderer in use is OpenGL** (RenderOpenGL_7c), not DirectX 9.
- **Not loaded at the menu**: Lexer_7c (nothing references it; not in plugins.ini, so probably dead),
  DebugSupport, DSMoviePlayer, VFWMoviePlayer, Render3DExtension, RenderDirectX9. Re-check inside a driving session.
- **Embedded Mozilla XULRunner 1.9pre** (`bin\mozilla\`: xul, xpcom, js3250, nspr4, nss3/ssl3/smime3/softokn3, sqlite3):
  the HTML panels of the menus. No x64 build of that version exists: the x64 port must replace it (e.g. WebView2).
  Also loaded: cg/cgGL, cudart, nvtt, ijl10, OpenAL32 + wrap_oal, LIBEAY32, MSVCR71, UnicoWS.

### Replacement granularity (lesson from Task/TaskManager)
- Inside a module, functions call each other directly, not through the export table. So replacing one class
  of a module is only safe when that class holds no state shared with the rest of the module (UId: fine).
  Stateful subsystems (App, Task/TaskManager + the internal LogTask, plugin system, EditItemManager) must move
  together: Kernel_7c's core gets replaced as one unit.

### Naming pass via RTTI vftables (`ghidra_scripts/NameVtableFunctions.java`, all programs via `tools/ghidra_all.ps1`)
- Unnamed functions referenced from a labelled vftable become `Class::vfN`. +~9 900 names; trainz.exe alone
  +6 804 (5 268 vftables). trainz.exe now 65.6% named (was 51.0%); Jet modules 70–91%.
- Kernel_7c internal classes revealed: BFEncrypt (Blowfish), LZSSCompress, PluginSystem, PluginLibrary,
  PluginGroup, PluginFactoryOf<...>, LogTask, Mutex, ReferenceCount.

### Auran Jet SDK 1.0 (2001–2003) as a reference (found by the user, 2026-10-19)
- Source: archive.org item `auran-jet-demo` (public downloads from auran.com/jet), saved in
  `D:\download\auran-jet-demo`. Licence: "AURAN Jet Free Access" (copyright Auran): private reference only,
  never commit or publish SDK files.
- Unpacked (without running installers) to `D:\trainz12-decomp\jet-sdk\{RC1,Beta2,Demo}\out`. The installers
  are InstallShield 6 cabs: 7-Zip can't open them; `unshield` 1.6.2 built from source with zlib 1.3.2
  (`D:\tools\src\unshield-build\src\Release\unshield.exe -d <out> x data1.cab`). The separate
  `Auran Jet Documentation.exe` uses an older format unshield rejects, but RC1 already contains the same
  `Doc\doc\JetAPI.chm`, extracted to `jet-sdk\RC1\chm` (10 003 HTML pages).
- RC1: 517 engine headers (`Inc\inc\*.h`), 111 .cpp (examples, extensions, JIVE), 51 import libs, 98 DLLs
  (`Kernel.dll`, `KernelStandard.dll`, ... the same modules without `_7c`, plus `.Dev` builds), docs
  (JetKernelOverview.doc, RenderOverview.doc, whitepapers incl. "Jet DLLs & The Plugin Management System",
  "Jet Program Flow"). No PDBs.
- **Checked against TS12**: `UId.h` matches our reconstruction (union data[4] / data1..data4, same compare,
  same Read/Write). `Version.h`: same packing, fields named `minor`(u16) `major`(u8) `main`(u8).
  `Task.h` matches the TS12 binary to the byte: taskName +0x10, updatedTime (double) +0x18, priority +0x20,
  bitfields isUpdating/isOnUpdateList/isInitialized at +0x24; virtuals ~Task, GetName, Init, Done, Update.
- Differences seen so far (SDK is ~10 years older than TS12): many members became `__cdecl` in TS12;
  `DynamicBuffer::Write(const void*, ulong, ulong)` gained a third argument. The SDK guides names and intent;
  the TS12 binaries remain the authority.

### Jet::BFEncrypt = a modified Blowfish (reconstructed and verified 2026-10-19)
- `src/Kernel_7c/AuranBlowfish.{h,cpp}`; tables generated from pi by `tools/gen_pi_tables.py` (Machin formula),
  checked equal to the DLL's tables (P at RVA 0x18478, S at 0x184c0). No data copied from the binary.
- Differences from standard Blowfish (all confirmed in the disassembly, `ghidra_scripts/DumpAsm.java`):
  key repeated/cut to 56 bytes read as little-endian words; blocks are two LE words (L first);
  F(x) = ((((S1[b]&1) + S0[a]) ^ 0x20) + (S3[d]&1) + S2[c]) with a = low byte; R ^= F(L) ^ 0x20;
  **the S-box fill rewinds every outer pass** so only S0[0], S1[0], S2[1], S3[1] change;
  a non-multiple-of-8 tail is one more block ending at the buffer end (decrypt does it first).
- Methods (thiscall, `RET 0xC`, hidden Result* first): GetCaps 0x21f0 {keyMin 1, keyMax 256, bufMin 8,
  bufMax 0xffffffff, align 0x40}, SetKey 0x2230, GetOptimalSize 0x1db0 (round up to 8), EncryptData 0x24d0,
  DecryptData 0x2550. Codes: 2 null, 3 key size 0, 4 key > 256, 5 buffer < 8.
- `src/tests/diff_blowfish.cpp` calls the original methods by RVA on a zeroed 0x1060-byte object:
  75 008 checks (key-schedule state, encrypt, decrypt of random data, round trips, error codes), 0 mismatches.
- Technique: internal (non-exported) functions of `<module>_orig.dll` can be diff-tested by calling them at
  base + RVA with the right calling convention.

### Jet::LZSSCompress decompression (reconstructed and verified 2026-10-19)
- `src/Kernel_7c/JetLzss.{h,cpp}`: header, mode 0, token mode (levels 1-6), adaptive Huffman (levels 7-9) with
  the original's model init (0x10005cf0), update (0x100060b0) and in-place rebuild (0x10005e60) carried over.
  Tree 1 has 0x111 symbols rounded to 0x112 leaves (256 literals, 16 length codes, 0x110 = end); tree 2 has
  16/64/64 symbols with 4/6/8 raw low distance bits for modes 0/1/2. Bits are read LSB first.
- `src/tests/diff_lzss.cpp`: a real original LZSSCompress (ctor RVA 0x5a00, flag 1) compresses 5 kinds of data
  (random, config-like text, runs, zeros, low-entropy binary) at 16 sizes up to 200 KB, levels 0-9:
  **6 287 checks, 0 mismatches** — our decoder restores every input exactly.
- The original *decoder* is not a usable oracle from a test harness: driven through IOSlaveMemoryDynamic it
  fails reading back across that class's 256-byte blocks; through IOSlaveMemory a stream ending in a long
  back-reference comes out short (buffered IOSlaveWriteCursor + WriteCopy). Valid data in the game goes
  through other paths; our decoder is checked against the original *encoder's input* instead.
- Corrupted streams (informational, not replicated): the original treats distance 0 as "copy what the
  destination already holds at the cursor" (undefined), and it corrupts its heap on bad Huffman streams.
  Ours fails cleanly. Distance 0 cannot occur in valid streams (mode 0 is only used below 256 bytes).

### Jet::LZSSCompress compression (reconstructed and verified 2026-10-19)
- `JetLzss::Compress` produces **byte-identical** output to CompressBlock: 4 800 comparisons (5 data kinds,
  16 sizes up to 200 KB, levels 0-9), 0 differences; whole LZSS test now 11 087 checks, 0 mismatches.
- Level table 0x100194c0 (stride 0x28): {mode, nice length, emitMatch PMF, emitLiterals PMF}; the PMFs are
  zero in the file and filled by the static initialiser 0x1000f540: levels 1-6 use the token emitters
  0x10006410 / 0x10006600, levels 7-9 the Huffman emitters 0x100062c0 / 0x10006390 (Ghidra had no functions
  there: only the table points at them; `ghidra_scripts/CreateFunctionsAt.java`, `XrefsTo.java`).
- Match finder 0x100067d0: hash h = ((h << s) ^ b) & mask over 3 bytes, s = 3/4/5 for buffer < 2000 / < 24000 /
  larger, head + prev chains of u16 positions compared as **signed shorts** (mode 2 positions pass 32767),
  whole chain walked down to pos - window, stop early at the level's nice length; bytes 0, best, 1 checked
  first, byte 2 implied by the hash. Window slides when pos reaches 2*window (0x10006700).
- Main loop 0x10006940: pending literals flushed above 0x101; matches over 0x102 split into 0x102 chunks, a
  1-2 byte remainder goes back to the input. Literal runs of 6+ in modes 1-2 become raw runs.
- Huffman encoder: symbol bits gathered leaf->root as (node & 1), LSB first, 16-bit flushes with x86 shift
  semantics; length table 0x10018158 {code, extra value, extra bits} is derived from the decode table;
  end symbol 0x110 written without a model update, then zero padding to 16 bits.

### Kernel_7c core: inventory and exported layouts (2026-10-19)
- Function areas (non-thunk): unnamed 129 fn / 22.1 KB (mostly plugin system, TaskManager internals, Jet
  container templates), LZSS 37 / 8.9 KB (done), plugin system 85 / 6.9 KB, EditItem* 50 / 5.3 KB,
  CRT 132 / 3.1 KB, App/Task/EntryPoint/LogTask 52 / 2.8 KB, Blowfish 19 / 2.6 KB (done), UId/Version (done).
  `ghidra_scripts/NeighboursOfUnnamed.java` → `out\unnamed_Kernel_7c.tsv` (callers, callees, strings).
- Plugin system internals: PluginSystem (0x48 bytes: ReferenceCount + 5 hash tables at +8/+0x14/+0x20/+0x2c/
  +0x38 + a circular list at +0x44), plugins.ini loader 0x1000acd0 (keys Plugin, PluginGroup, PluginList,
  PluginRemap, AutoPlugin, PluginDir; looks next to Kernel_7c.dll and next to the process), library loader
  0x10009920 (name + "_7c.dll"/"_7c.exe", GetProcAddress "JetGetPlugins" + suffix). Internal only: may be
  rebuilt with our own containers as long as behaviour matches.
- Layouts other modules depend on (must match byte for byte; they match the SDK field order):
  PluginType 0x20 {vptr; UId id +4; PString name +0x14; PluginLibrary* lib +0x18; PluginGroup* parent +0x1c},
  vtable {AddReference, ReleaseReference, scalar deleting dtor}.
  PluginConfig 0x28 {PString name; UId id +4; parent +0x14; lib +0x18; type +0x1c; children array +0x20;
  bool isGroup +0x24; bool isMissing +0x25}.
  PluginInstance: `virtual` ReferenceCount base (vbptr +0, pluginFactory* +4, ReferenceCount {vptr, refs}
  placed last). Differs from the SDK, where the base is not virtual.
- JetGetPluginsKernel registers PluginFactoryOf<LZSSCompress,Compress> (uid 8DBDFEB1-52F4-4c1b-BA91-
  6A7B1CCC6D3E, group 8DBDFEB0-...) and PluginFactoryOf<BFEncrypt,Encrypt> "BlowFish" (AAF296E1-17A7-11d5-
  A9C2-00E01898A4A9, group AAF296E0-...).
- TaskManager options read in 0x1000cb80: "Frame End" task, vSync, FrameRate, lockFPS; WaveServer task
  (0x1000d160). The frame-rate cap lives here: relevant for the performance goals.
- Ghidra note: `FixCdeclMembers.java` sets every function's convention from its mangled name (and adds an
  explicit `this` to __cdecl members); in Kernel_7c they were already right, the remaining noise in calls
  like `Options::Get(this_04, ...)` is lost ECX tracking, so read the asm where arguments matter.

### Plugin system: behaviour spec (read from Kernel_7c, 2026-10-19)
Globals: PluginSystem* at 0x10019c24 (ref-counted; created on first use by 0x1000c460, which also loads the
ini files), one CriticalSection at 0x10019ccc guarding everything below.
- **Loading** (0x1000acd0): Options opts; process `<dir of Kernel_7c.dll>/plugins.ini` if File::Exists, then
  `<dir of process>/plugins.ini` if it is a different path and exists. If OS options have `AutoPlugin`:
  scan "" and every `PluginDir` value (0x1000afd0): Find "<dir>/*.dll" (or "*.dll"), DynamicLibrary::Open,
  Import "PluginList" -> call -> NULL-terminated `const char*` list, each line -> opts.ProcessRaw. Then:
  `PluginGroup` lines "Name:GUID[:ParentGUID]" (0x1000a3e0), `Plugin` lines "Dll:Name:GUID[:ParentGUID]"
  (0x1000a710), `PluginRemap` lines "FromGUID:ToGUID" (0x1000aad0). A line whose parent GUID is unknown is
  skipped. PluginConfig {name, id, parent, lib (Plugin only), isGroup}; added to the parent's children and to
  the id table; also to a name index where a new entry goes *before* existing ones with the same name.
  Libraries are found or created by name (list at +0x44), reused while consecutive lines name the same dll.
- **Library load** (0x10009920, lazy): name "*" -> GetModuleHandle(NULL); ext "exe" -> drop ext, add
  "_7c.exe"; otherwise drop "dll" ext if any, add "_7c.dll"; LoadLibrary. Entry = "JetGetPlugins" + file name
  (no suffix for "*"); missing -> FreeLibrary, mark failed, result 2; else call it, result 0. Load failure ->
  mark failed, result 1. PluginLibrary {vptr, PString name, refs +8, types +0xc, HMODULE +0x10,
  bool +0x14, bool failed +0x15}; refs 0 -> vf1: delete its types, FreeLibrary (unless the name is "*").
- **PluginType ctor**: copy id/name, AddRef system, lib = 0; parent GUID non-zero -> find type in types table
  or make a PluginGroup type (0x28: PluginType + refs +0x20 + member list +0x24) from the group config;
  add to the group's members; register (0x10009fb0): lib = config's lib (and add to lib's type list), add to
  the types-by-parent and types tables. **dtor**: lock; unlink from group (group refs--, deleted at 0);
  unregister; release system. **AddReference/ReleaseReference**: lock; lib->refs++/--, 0 -> unload lib.
- **Lookup by id** (0x1000a020): remap; types table hit -> lib==0 ? result 4 : AddReference, result 0. Else
  config (not missing, lib not failed and not loaded) -> load lib -> retry; still nothing -> config.missing=1,
  unload lib; result 1. By parent+name (0x1000a140/0x1000b290) and by list (Enumerate: non-group children,
  recursing into groups). PluginCollection(uid[, name]) = FastDynamicArray<PluginConfig*> filled the same way.
- **CreateInstance** (Compress/Encrypt/EditItemManager): lookup factory, factory vtable +0xc =
  CreateInstance, then Init (+8 for Compress, +0x24 for EditItemManager) and release on failure; the no-arg
  versions build LZSSCompress / BFEncrypt directly.
- **FastDynamicArray<T,13>** as other modules see it inline: a pointer to a ring of 0x40-byte chunks
  {next, prev, count, T items[13]}; Add appends to the last chunk, a full last chunk gets a new one.
  PluginCollection *is* that array, so the chunk format must stay exact.

### FastDynamicArray<PluginConfig*> reconstructed (2026-10-19): 63/256 Kernel_7c exports ours
- `src/include/jet/FastDynamicArray.h` (own code; layout and behaviour of TS12, not a copy of the SDK header):
  array {head}, Chunk {next, prev, count, data[SIZ]} ring from Jet::Alloc, Iterator {node, head, chunk}.
  All 33 exported members link against the original mangled names via one explicit instantiation
  (`Kernel_7c/PluginConfigList.cpp`).
- TS12 differs from the SDK in: Insert/InsertUnique (first chunk whose last item is not less, then
  BinarySearch 0x10008ca0 inside it, which lands before the first equal item), Index(n) without a bounds
  check (past the end: {null, head, head}), Index(Iterator) computed as items-before-chunk +
  (node - chunk - 0xc) >> 2 (so -3 for End() of an empty array), RemovePack building its result after moving
  the head (the SDK returned the freed head), Pop() on empty reads address 0 (ours returns T()).
- `src/tests/diff_fda.cpp`: 400 random runs of 23 operation kinds with many duplicates, whole chunk ring and
  returned iterator positions compared after every step: **100 306 checks, 0 mismatches**.
- In game: menu, clean exit, JetLog identical to the baseline.
- Method note: the exit "Packed Heap Summary" lines (e.g. `#StringData ... 507k/508k used`) change between
  identical runs (a control run with the old DLL showed it), so `tools/menu_check.ps1` ignores them.
  `tools/bisect_exports.ps1` builds a DLL with a slice of an export group, for hunting in-game differences.

### Plugin system unit reconstructed (2026-10-19): 140/256 Kernel_7c exports ours
- `Kernel_7c/PluginSystem.cpp` (PluginType/Config/Instance/Base/Collection + the internal system),
  `CompressEncrypt.cpp` (Compress/Encrypt bases, LZSSCompress and BFEncrypt over our codecs, the kernel's
  `JetGetPluginsKernel`, CreateInstance helpers incl. EditItemManager's), `KernelStandardRuntime.cpp`.
  Object sizes asserted against the original: PluginType 0x20, PluginInstance 0x14, PluginGroup 0x28,
  LZSSCompress 0x115c (trainz.exe and the tools construct it themselves), BFEncrypt 0x1060.
- Stays forwarded: the vbtables and vbase destructors of Compress/Encrypt (MSVC emits them only for complete
  objects of these abstract classes; the original's copies are fine for derived classes elsewhere).
- Corrections to the spec above: the library list is always searched by name (0x1000a260), and Unload
  calls FreeLibrary only when the name *is* "*" — plugin DLLs are never unloaded, a later lookup reloads them
  (LoadLibrary refcount) and calls JetGetPlugins again.
- Nothing left in the original module reaches the old state: the only forwarded caller of the internals is
  EditItemManager::CreateItem (0x100036f0, lookup by name through vtable slot 10's group id), and no module
  of the game imports any EditItem* symbol.
- `src/tests/plugin_probe.cpp` (+ `D:\trainz12-decomp\probe\run.ps1`): one exe linked to "Kernel_7c.dll" by
  name, run in a directory where that is the original and in one where it is ours, on the real plugins.ini:
  collections by group and by group+name before and after, remaps, lookups by id and by name (these load the
  real plugin DLLs), LZSS and BlowFish through the plugin interface. **2 732 lines, identical** except one
  known original bug (below).
  - Method: the probe must hold a system reference for the whole run, as the game does. Without one the
    system is destroyed and rebuilt (ini re-read) on every call, and "missing"/"failed" flags vanish.
  - Original crashes (null library) on lookups of *group* ids by id or by name; ours returns 1 / 3. When it
    crashes it leaks a system reference, which made the first comparison look different.
  - LZSS DecompressBlock at levels 1 and 5 on 20 000 bytes: the original returns 0 on its own output (the
    IOSlaveMemory short-write bug noted above), ours restores the data. Compressed bytes are identical.

### Main loop reconstructed (2026-10-19): 195/256 Kernel_7c exports ours
- `Kernel_7c/Task.cpp`, `App.cpp`, `dllmain.cpp`, `include/jet/Task.h`: Task, TaskManager, App, EntryPoint,
  TimeEventSystem, GetTaskManager, the "Log" task and the module attach/detach (run under
  ExceptionHandler::Main like the original). SDK 1.0 names; TS12 layout checked by static_assert
  (Task 0x28: MSVC pads the vfptr to the class's 8-byte alignment, so the IListNode links sit at +8;
  TaskManager 0x38; App 0xc) and the vftables of ours and the original compared slot by slot.
- TaskManager (0x38): os +0, lastTime +8, frameEndTask +0x10, taskArray +0x14, taskChangeArray +0x18,
  taskUpdateList +0x1c (IList head, count +0x24), flags +0x28 (initialised, taskChanges), frameRate +0x2c,
  **lockFPS +0x30 (TS12)**, waveServer +0x34. Statics shutdown 0x10019c2c, running 0x10019c2d.
- Frame (0x1000d3c0): pending changes first (Init new tasks, insert updating tasks before the first task of
  equal or higher priority, unlink suspended ones); then the update list works as a queue: pop, Update,
  back of the queue (STOP leaves the list, EXIT ends the loop) until the "Frame End" task (priority FLT_MAX)
  comes round; it goes to the back too.
- Main loop (0x1000cf00): `FrameRate` option (ignored with `vSync`) paces frames with Time::Delay in whole ms
  and carries the error into the next frame, clamped to ±half a frame; `lockFPS` gives a fixed dt =
  1/lockFPS with Time::IncFrame and Time::SetFrameLockTime. This is where frame pacing can be improved later.
- Options read: vSync, FrameRate, lockFPS, WaveServer=On (loads WaveServer_7c.dll unless a task named
  WaveServerTask exists).
- Module attach also interns 21 names ("OnMinZoom" ... "UserInterface") kept by the original, so the string
  pool sees the same strings.
- `gen_def.py`: `name=symbol` lines export our symbol under an original name (used for
  `??_FVersion@Jet@@QAEXXZ`, the compiler's default constructor closure).
- `src/tests/task_probe.cpp` (+ `probe/run_task.ps1`): scripted tasks through Start/Update/Stop on both
  modules (priorities incl. equal and negative, STOP, EXIT, failing Init, Suspend/Resume, delete while
  running, task added while running, UpdateTask, GetTask, restart): **logs identical**. Plugin probe and the
  older diff tests unchanged.
- In game: menu reached on our main loop, clean exit, JetLog identical to the baseline.

### Kernel_7c complete (2026-10-19): 256/256 exports ours, the game runs without the original module
- `Kernel_7c/EditItem.cpp`, `include/jet/EditItem.h`: EditItem, EditItemStack, EditItemManager (61 exports, layout
  below); Compress/Encrypt vbtables and vbase destructors in `CompressEncrypt.cpp`, exported under the original
  names through `name=symbol` lines. vbtable bytes match the original ({0, 0xc} at RVA 0x1051c, shared by both).
  Our `??_D` skips the original's re-store of the Compress/Encrypt vftable pointers on the dying object (not
  observable), then does the PluginInstance reset and `~ReferenceCount` like the original.
- `src/tests/edititem_probe.cpp` (+ `probe/run_edititem.ps1`): items (caps, full caps growing in steps of 13,
  Save/Load flags), stacks (push/pop across chunk boundaries, paged items, failing Load/Undo/Redo, collapse,
  page out), managers made by the plugin system (Initialize with a named mutex held by another thread, temp
  dir clean-up, unique file names, GetItem/SubmitItem/BinItem incl. wrong handles, undo/redo to the ends,
  PageToDisk, CleanupThisTempDataOnDisk): **307 lines, identical**. The random per-process UUID is masked.
- No regressions: plugin probe (2 732 lines, only the known original LZSS bug differs), task probe identical,
  diff tests 0 mismatches (Blowfish 75 008, LZSS 11 087, FDA 100 306, UId/Version 621 000).
- Our DLL imports only KernelStandard_7c, ole32 and kernel32; the generated .def has no forwarders left.
- In game: menu reached, clean exit, JetLog identical to the baseline; **also with `Kernel_7c_orig.dll` moved
  out of bin\** (the 32-bit module list shows only our Kernel_7c).
- Gotcha for agents: the Bash tool turns `\\` into `\` inside heredocs, so a Python fix-up script passed that
  way corrupted every `"\n"` in a C++ file. Write such scripts with the file tool (or use `chr(92)`).

### EditItem block: analysis notes (2026-10-19)
- 0x10002c80-0x10004a20 (~7 KB, 61 exports). No module of the game imports any EditItem* symbol: dead code
  in TS12, but needed for a Kernel_7c without the original. Asm: `D:\trainz12-decomp` scratch dumps.
- EditItem (0x34): own vfptr +0, PluginInstance at +4 (vbptr +4, factory +8), manager +0xc, PString type
  +0x10, caps list (FDA<PString,13> chunks) +0x14, Array<FullCap,13> {count +0x18, capacity +0x1c, data +0x20}
  (FullCap = 2 PStrings, grown in steps of 13), bool paged +0x24, vtordisp +0x28, ReferenceCount +0x2c.
  Ctor takes the PString *by value* plus the vbase flag. Init stores the manager; Save/Load flip +0x24.
- EditItemManager (0x30): vfptr +0, vbptr +4, factory +8, item list (DynamicArray<PString>) +0xc,
  current item +0x10, undo stack +0x14 and redo stack +0x18 (EditItemStack, 0x18 bytes each, ctor(bool)),
  Mutex* +0x1c, PString name +0x20, vtordisp +0x24, ReferenceCount +0x28. Ctor registers
  CleanupThisTempDataOnDisk with atexit. Vtable slots used: +0x28 group id for CreateItem, +0x2c CreateItem,
  +0x30 GetSystemLevelIdentifier, +0x34 item default name, +0x38 GetApplicationUniqueTempPath.
- Temp files: tempFileDir (static PString) = app temp path + "\" + GetUUID() (UId::SetRnd → GetString,
  stripped), made with File::Md; names "Temp_%d.bin" from tempFileCounter under tempFileMutex; File::Prune
  cleans up. Initialize(name) takes a named Mutex and cleans old data when it gets the lock first.

### Kernel_7c on x64: standalone harness (2026-10-19)
- `build64` (`cmake -S src -B build64 -G "Visual Studio 18 2026" -A x64`): Kernel_7c as an object library
  (`Kernel_7c_x64`, `JET_KS_STATIC`) linked with `tests/x64/KernelStandardStub.cpp`, a stand-in for the ~95
  KernelStandard functions Kernel_7c imports. **It is not a reconstruction**: just enough behaviour, following
  the 32-bit module where a probe's output depends on it (PString interning: Destroy only drops the reference;
  Options: one entry per key {key, value, separators, cursor}, `+Key=v` appends `,v` (separator byte read from
  the DLL), values run up to the whitespace before the next `/ - +` switch; ReferenceCount; Mutex).
  No DllMain when linked statically: `StaticModuleAttach/Detach` (dllmain.cpp) via `JET_STATIC_KERNEL` in main.
  Static mode finds plugins.ini next to the exe (`GetModuleHandle("Kernel_7c.dll")` is null).
- Results (`probe/run_x64.ps1`): **task probe 92 lines and EditItem probe 307 lines identical to the original
  32-bit module's output**; new `tests/golden_kernel.cpp`, built for both arches (LZSS all levels on 80 inputs +
  300 corrupted streams, Blowfish key schedule/encrypt/decrypt/error codes, 200 FastDynamicArray runs =
  50 126 steps with the chunk ring hashed after each, 2 000 UIds incl. GetString round trip, 2 000 Versions):
  **305 lines identical** on Win32 and x64. Its 50 failed LZSS round trips are the size-0 input on both (a
  header with no payload is a failure, as in DecompressBlock).
- Compiles clean on x64 at /W3 and with C4267/C4244/C4311/C4312/C4302 enabled: no inline asm, no truncating
  casts (earlier sessions cast explicitly everywhere).
- **64-bit hazards found and fixed** (the Win32 build is unchanged: probes, diff tests, menu check all as before):
  1. FastDynamicArray::Index(Iterator) hard-coded `(node - chunk - 0xc) >> 2`; on x64 the chunk header is 0x18
     and items 8 bytes → now `offsetof(Chunk, data)` / `sizeof(T)`.
  2. PString read the node at raw +8 (length) / +0x18 (text); node is now a struct {next, prev, length,
     capacity, refs, unknown, text[]} (KernelStandard 0x10039cc0); on x64 text is at +0x20.
  3. 17 size asserts are the 32-bit ABI → `JET_LAYOUT32` (include/jet/Platform.h); UId/Version stay exact.
  4. KernelStandard objects Kernel_7c embeds without seeing their members (OS 0x18, Events 0x2c, File::Find
     0x440, IOSlaveMemory 0xc) → `JET_OPAQUE_SIZE(32-bit, provisional x64)`. The real x64 KernelStandard must
     publish real layouts; File::Find holds MSVC8 STL members (see the KernelStandard hand-over notes).
  5. DestroyTimeEvents empties Events' lists at raw 32-bit offsets (+8/+0x14/+0x20, links at +4) → skipped on
     x64; an x64 KernelStandard must empty them in ~Events.
  6. KernelStandardRuntime binds File::Exists by its 32-bit mangled name with GetProcAddress → not built on x64.
  7. Export plumbing is x86-only: gen_def.py and implemented.txt use x86 mangled names, `name=symbol` aliases
     point at `__fastcall` helpers (vbase destructors, Version default-ctor closure). An x64 Kernel_7c.dll
     needs its own export list (x64 manglings differ: `QEAA` for `QAE`/`QAA`). Since trainz.exe will be
     rebuilt too, compatibility with the 32-bit manglings is not needed there.
  8. Only off Windows: UId::data1 and Version::Packed use `unsigned long` as 32 bits (LLP64).
- x64 sizes vs 32-bit: pointer 8/4, PString 8/4, FastDynamicArray<T*,13>::Chunk 0x80/0x40 (data at 0x18/0xc),
  Task 0x30/0x28, TaskManager 0x58/0x38, Mutex 0x10/8; UId 0x10 and Version 4 on both.
- Not portable by design: the differential tests against the original (diff_*, plugin_probe) need the 32-bit
  original DLLs and call them through emulated thiscall; on x64 the golden output and recorded probe logs act
  as the oracle instead.
- Menu-check note: from about 06:40 the exit line `#StringData` reads ~1 100-1 200k used instead of ~508k.
  The older verified DLL (02:24 build) gives the same number, so the cause is the game copy's environment, not
  our module (JetLog otherwise identical).

### KernelStandard_7c: inventory and unit order (2026-10-19)
Full plan in `D:\trainz12-decomp\notes\KernelStandard.md` (private; reproducible inputs in `notes\data`, `notes\ghidra`).
- 1 762 exports (1 649 code, 113 data/vftables), `.text` 327 509 bytes, 2 276 functions. **Only 666 exports are
  imported by anything** (trainz.exe 447, six tool exes 191-311, 21 Jet modules 6-168, Kernel_7c 121). The other
  1 096 are used only inside the module; StringTables has no caller at all.
- About 13% of `.text` is a statically linked IJG JPEG *encoder* (only `Bitmap::SaveJPG`). JPEG *decoding* goes
  through ijl10.dll (32-bit only). Both are to be replaced by libjpeg-turbo; decoding will need a tolerance test.
- The foundation state is **not exported** and orig's own code reaches it by address: the Alloc size-class free
  lists with 16 striped critical sections, the `#StringData` PackedHeapMP, the PStringCache lock, the shared null
  AnsiString buffer, the FileSub (.ja archive) list. So the Kernel_7c method is reversed here: **consumers first,
  foundation last**. Units: U0 proxy, U1-U6 leaves (math, PODs, sync, Rand, small platform), U7 Config (flex
  scanner), U8 Bitmap, U9 JIRF and streams, U10 File family incl. the .ja reader, U11 OS/Window, U12 diagnostics,
  U13 thread/time, and U14 foundation as one cut-over.
- CRT: Alloc blocks are msvcr80 `malloc` with a 4-byte size-class tag (pointers 4 mod 8, a 16-byte alignment
  problem for x64). `File` keeps a CRT file descriptor at +0, so the File family and Log must link the game's
  msvcr80, unlike Kernel_7c.
- Exported objects with MSVC8 STL members (PackedHeap/MP, File::Find, IOSlaveMemoryDynamic) keep their size only
  with hand-rolled containers.
- x64 hazards: Window keeps `this` in window extra bytes via `Set/GetWindowLongA(h, 0)`. ExceptionHandler walks
  EBP frames and uses `CONTEXT.Eip` and `FS:[0]`. Rand, LCRand and Time scale with x87 `fistp` (bit-exactness).
- Options semantics (one entry per key, `+key=v` appends `,v`, values run to the next switch) are recorded in the
  plan and implemented in the x64 stand-in.
- Recommended first: U0 + U1 (35 exports, ~4.3 KB, bit-testable; `StdMath::s_sqrtLUT` is read inline by 10 modules).

### KernelStandard_7c U0 + U1 (2026-10-19): 34/1762 exports ours, bit-exact
- **U0**: `jet_module(KernelStandard_7c)` with 1762 forwarders to `KernelStandard_7c_orig.dll` (all names are
  decorated, so a plain .def covers them, data and vftables included). In game both modules are loaded, menu
  reached, JetLog identical. Kernel probes (plugin, task, EditItem) unchanged through it.
- **Our code calls the not-yet-replaced foundation in the original**: `implib\KernelStandard_7c_orig.lib`
  (gen_implib.py with that module name). Our DLL imports only KERNEL32 and KernelStandard_7c_orig.dll.
- **U1** (34 exports: Crc, StdMath, Vector2/3 conversions, IntLog2/ILog2/CeilPow2/FloorPow2,
  GetBaryCentricWeights, Eigen3, OptimalRects): `src/KernelStandard_7c/{Math,Eigen3,Barycentric,OptimalRects}.cpp`.
  The 35 in the plan was one too many.
- `tests/diff_ks_math.cpp` (ours vs `_orig` in one process, raw bits): **all 2^32 inputs** of the five float
  StdMath functions, IntLog2, ILog2(float), ILog2(int), CeilPow2 and FloorPow2; 4 M doubles for Sqrt/InvSqrt(double);
  Crc 200 k; the LUT; Vector ctors incl. signalling NaNs; 2 M Eigen3 matrices (every step separately, incl.
  QL giving up after 32 iterations on 12 231 of them); 3 M barycentric inputs (394 k degenerate); 3 000 random
  OptimalRects runs, 193 k steps with both lists compared after each, plus Process on hand-built pending lists.
  **About 43 billion checks, 0 mismatches.** For inputs that already hold NaNs, Eigen3 (111) and the barycentric
  weights (1 708) differ only in which NaN payload propagates (x87 keeps the larger significand, SSE the first
  operand). Matching that would need the original's SSE operand order, so it is accepted. In game: menu,
  clean exit, JetLog identical.
- Things that only showed up in the asm:
  - StdMath works in x87 at the CRT's 53-bit precision and **returns floats unrounded in st(0)**. We compute
    in double and return through a naked `jmp` thunk on Win32, so an MSVC8 x87 caller sees the same value.
  - `InvSqrt(double)` takes its seed from the **low** 32 bits of the double (a bug, kept). `Sqrt(double)`
    takes it from the value rounded to float.
  - Constants: 0.5f, 1.5f, 1.5, 0.5, 1.0f, 2.0f, ±1e-6 (doubles).
  - `s_sqrtLUT` (512 mantissas with 8 significant bits): the best simple formula (1/sqrt of the bucket
    midpoint, rounded) matches only 474 of 512 entries, so the table looks tuned. It is read at build time from
    the installed game's original DLL by `tools/extract_data.py` into a generated header; the source has no
    copy of it.
  - Copies through x87 `fld`/`fstp` quiet signalling NaNs (Vector ctors, Eigen3 diag[0] and the sort swaps):
    `X87Copy` does the same on Win32.
  - Eigen3 mixes SSE single precision with x87 double precision step by step: the QL convergence test sums
    in double, the rotation's new off-diagonal uses the unrounded double square root, c and s are floats. Its
    4x unrolled scan loops only run for counts above 3, so they are dead for 3x3. `operator[](i)` indexes the
    object as a flat float array from offset 0. Layout: m[3][3], diag[3], subd[3] (0x3c).
  - OptimalRects ({set head, tail, count, pending head, tail, count}, 0x18-byte nodes {Rect{x,y,w,h},
    next link, prev link}, links pointing at link fields) uses custom register conventions in its 15 split
    helpers. Kept off-by-ones: the strips cut in cases 3, 6, 10 start at the member's last column, and in
    case 12 at its last row, ending one row short. Validate is an empty function (ICF-folded with an empty
    destructor).
- Win32 build note: `/arch` default (SSE2) plus /fp:precise gives the original's single-precision SSE steps
  exactly. The `double` expressions stand in for its x87 code. The game uses OpenGL, which leaves the x87
  precision at 53 bits; D3D9 without FPU_PRESERVE would set 24 bits and change the original's results.

### KernelStandard_7c U2-U5 (2026-10-19): 242/1762 exports ours, bit-exact
- **U3** small PODs (`SmallTypes.cpp`): 11.15 M checks, 0 mismatches.
- **U4** sync objects (`SyncObjects.cpp`: SyncObject, Mutex, Semaphore, WEvent, CriticalSection, SingleLock,
  MultiLock, SWMRGuard): `tests/diff_ks_sync.cpp` runs a 319-step two-thread scenario on both modules,
  identical results over 3 runs. `SingleLock::operator=` copies the whole object, padding bytes included. The
  default-constructor closures (`??_F`) are exported as aliases of `__fastcall` workers that use `::new`
  (the class `operator new` hides placement new). Data still owned by the original is declared
  `JET_KS_FORWARDED` (dllimport) in our headers.
- **U5** Rand / LCRand (`Rand.cpp`): Rand 16 M checks, 0 mismatches. LCRand 8 M checks, 1 difference, which is
  NaN-only (NextDouble with two NaN arguments). Details from the asm:
  - Scale 1/(2^32-1) (bits 0x3DF0000000100000).
  - Schrage step for 16807 x mod (2^32-1); t <= 0 becomes t - 1.
  - Integer results go through `_ftol2_sse`, which is a 32-bit `cvttsd2si`, so out-of-range values give 0x80000000.
  - The Gaussian's spare value lives in one global for all generators.
  - Randomise reads `Time::frameLock`/`frameLockTime`. The test sets these to make it deterministic.
  - Float results come back unrounded in st(0). Their exports are aliases of `__fastcall(this, edx, ...)`
    workers returning double, which has the thiscall ABI.
- **U2** Matrix3 / Matrix4 / Quaternion (`Math2.cpp`, 29 exports):
  - `tests/diff_ks_math2.cpp` makes 200 k random cases per member. Entries come from four distributions, 2 %
    of them are special values, and some inputs are aimed at each branch (rotation matrices, equal/close/
    negated quaternions for Slerp, near-gimbal quaternions, up parallel to the axis).
  - The whole object is compared as raw bits, and GetDeterminant as the st(0) double.
  - Result: **5.8 M checks, 0 mismatches**, plus 1 962 differences that are NaN payloads only, accepted as in U1.
  - The SSE expressions were lifted from the listing with the new `tools/sse_lift.py`. It turns a block of SSE
    scalar code into SSA, or into nested expressions with the original association.
  - Matrix3 members are thiscall, Matrix4 members `__cdecl` with `this` on the stack (QAA), Quaternion thiscall.
    Matrices are row-major. Rotate(x,y,z) rewrites rows 0-2 (all 4 columns for Matrix4). It skips `fsincos` for
    an angle that is exactly 0, so -0 gives sin +0. RotateX/Y/Z and SetRotate* have no such check.
  - The Matrix4 RotateX/Y/Z and SetRotAxisAngle functions touch only the upper 3x3. SetRotate*(a, keepRest)
    set column 3 and row 3 to the identity when keepRest is false. Negations are `-0.0f - x` (subss), not
    sign flips.
  - Invert computes the adjugate in place and multiplies it by 1/det. There is no singularity check.
    GetDeterminant uses the same SSE cofactors but expands them in x87 double: ((m12 C3 + m8 C2) + m4 C1) + m0 C0.
    It returns the value unrounded, through an alias to a `__cdecl double (const Matrix4*)` worker.
  - Quaternion::Set(x,y,z) builds three identity Matrix3s, applies Rotate(x,0,0), Rotate(0,y,0) and
    Rotate(0,0,z), then calls Set(Rx * (Ry * Rz)).
  - Set(Matrix3) and Set(Matrix4) take the square roots in x87 double, then normalise. The length is
    computed in x87 and the scaling in SSE.
    - Matrix3 branch tests: trace >= 0.
    - Matrix4 branch tests: trace > 1e-6f, and |r| > 1e-6f before dividing.
    - When |r| <= 1e-6f, the scale factor is r itself (a bug, kept).
  - Slerp copies `a` when a == b, using x87 copies for y, z and w. It negates b when the dot product is
    negative. It uses the linear weights when 1 - dot <= 0.001f. Otherwise it runs `_CIacos` and three `fsin`
    in extended precision.
  - GetEulerAngles takes the gimbal branch when |sp| > 0.99999f. That branch gives (0, pi/2, acos(clamped)).
    The normal branch uses `_CIasin` and two `fpatan`.
  - Set*Axis / SetViewVector:
    - Normalise the axis (InvSqrt from U1).
    - Cross it with up. Below |c| <= 0.0001 the default basis rows are used.
    - Feed the Matrix3 to Set(Matrix3).
    - Row orders: X (n, c, t), Y (c, n, t), Z (t, c, n), View (c, t, -n).
    - SetXAxis uses c = up x n. The others use c = n x up with t in the opposite order.
  - x87 transcendentals (`fsincos`, `fsin`, `fpatan`, msvcr80 `_CIacos`/`_CIasin`) are inline asm on Win32.
    The CRT intrinsics are looked up in the process's msvcr80.dll, the one the original calls. On x64 they fall
    back to <cmath>. Math2.cpp is also compiled into the x64 stand-in.
- Checks after U2: diff_ks_math/small/sync/rand unchanged. Kernel probes (task 92, EditItem 307, plugin 2 732
  lines) match the previous run, and the original's known plugin-probe difference is unchanged. The x64 harness
  shows 0 differences. In game, with all of U1-U5 installed: menu reached, clean exit, JetLog identical to
  the baseline.

### Step 5, small Jet modules (2026-10-19, in progress)
- Sizes of code in the five modules (Ghidra function sizes): KernelResources ~10 KB, InputWin32 ~13 KB,
  Render2DCore ~17 KB, KernelResource ~31 KB, InputSystem ~73 KB.
- Dependencies:
  - KernelResource is the base. KernelResources, Render2DCore and InputSystem import it.
  - Render2DCore also imports RenderSystem.
  - InputWin32 is a plugin built on InputSystem.
  - All five also import Kernel_7c and KernelStandard_7c.
- **U0 proxies for all five** (`jet_module` + empty `implemented.txt`): all exports forward to `<module>_orig.dll`.
  In game: menu, clean exit, JetLog identical.
- **KernelResources_7c: 51/51 exports ours** (`src/KernelResources_7c/BitmapResources.cpp`, headers
  `jet/{Resource,KernelResources,Bitmap,Config,Result}.h`). It holds:
  - BitmapBaseResource, BitmapFrameBaseResource and BitmapResource, plus their instances.
  - Two internal resources: BitmapFrameResource ("bmframe.txt": `Frame { Image, Area }`, an area of another
    bitmap resource) and ColourResource ("colour.txt"/"color.txt": `Color { Red Green Blue Alpha Blend }`,
    rendered as a box, mode 5 when blended, else 1).
  - The plugin table: ResourceType<T> factories for tga (UId ...81), bmp/jpg/pcx (all ...82), bmframe.txt (...83),
    colour.txt (...84) and color.txt (...85), in the group 4C414E80-BBDA-11d4-9AD7-0001025364B8.
- Things from the asm:
  - Resource has 18 virtuals, all `__cdecl` except the destructor. The order is Lock, Unlock, LoadAll,
    IsAllLoaded, SetPriority, Add/RemoveObserver, GetMustResetWhenFinished, ~Resource, CreateInstance, Load,
    LoadTxt, LoadComplete, OnLoadNotify, Unload, CalculateSize, CalculateDataSize, GetRedirection.
  - The bitmap classes add Render x3 (slots 18-20, overloads in reverse declaration order), GetBitmap, GetArea,
    GetOffset, and SetBitmap for BitmapResource.
  - Result codes are `ResultClass<&X::results, X::Results>` (one int, returned in memory). `jet/Result.h`
    reproduces the mangling, so exports such as `BitmapResource::Load` link under the original names.
  - The rectangle intersection (0x10001840) clears the wrong fields when the result is empty: x for an empty
    height and y for an empty width. Kept.
  - The tiled Render clips each tile to the area's right/bottom edge (not the left/top) before the clip rect.
  - BitmapResource::Load halves both dimensions until w*h <= MAX_STREAMING_TEXTURE_SIZE^2 (default 2048) and fails
    if either reaches 0. It scales with Bitmap::Scale.
  - BitmapBaseResource::GetBitmap throws `Jet::Exception` ("GetBitmap() not implimented", line 0x118).
- Checks:
  - `tools/compare_vtables.py` shows the three exported vftables match slot for slot (same import in every
    imported slot).
  - `tests/diff_krs.cpp` drives both modules in one process with a stand-in resource whose vtable logs calls, and
    with real KernelStandard bitmaps:
    - the three Render variants (random areas, clips, justifications, repeat limits) are compared on destination
      pixels and call logs;
    - all BitmapBaseInstance members;
    - BitmapResource is built through its constructor and loads generated TGA files through a stand-in database,
      with small streaming limits.
  - Result: **1 096 760 checks, 0 mismatches**.
  - Limit: in this harness Bitmap::Scale returns null for the 32-bit format, so the scaled loads end with no
    bitmap in both modules.
  - The factories and the two internal resources are only covered in game: menu, clean exit, JetLog identical.
- **Render2DCore_7c: 85/85 exports ours** (`src/Render2DCore_7c/{Canvas,Sprites}.cpp`, headers
  `jet/{RenderSystem,Render2DCore,VCall}.h`):
  - Canvas: an off-screen surface drawn as up to 7 x 5 textures, holding at most 16 dirty rectangles per tile;
    more than 16 turn into the whole tile.
  - SelectionBox: a translucent quad with a dark and a light outline, at a 0.375 pixel offset.
  - SpriteInstance and SpriteManager: priority-sorted sprites, drawn higher priority first via qsort.
  - SpriteResource ("sprite.txt": `Sprite { Texture UV UVI Color Size Position Rotation Scale AlphaBlendMode
    AlphaTestMode AlphaTestFunction AlphaRef SrcFactor DstFactor }`; UId ...8A).
- RenderSystem is only partly declared. Layer and Overlay are declared with their TS12 layout (0x20, 0x3c) and
  11-slot vtable: dtor, RenderLayer, UpdateLayer, Collides x2, Reset, Render, Update, Begin, End, CanRender.
  Display, Texture and the kernel are opaque. They are called through `VCall` wrappers by vtable offset:
  - Display: +0x80 texel offset (a float in st(0)), +0x98 create texture, +0xa0 set texture, +0xe0/+0xe4 draw,
    +0x10c vertex declaration.
  - Texture: +0x04 create, +0x0c upload, +0x10 update a rectangle, +0x20/+0x44 bind, +0x38 size, +0x3c touch.
  - Kernel: +0x7c Display, +0x94 texture manager.
  - Display +0x54 is the state block that Display's non-virtual setters cache.
- Things from the asm:
  - SpriteInstance's 0x50-byte attribute block is a copy of the resource's: position (z is the priority), size,
    uv, colour, rotation, scale, alpha flags and factors.
  - The vertices are 4 x {x, y, z, colour, u, v}.
  - EnableFixedPixelSize divides both sizes by the screen *height* (kept).
  - Canvas::Render shifts the texture coordinates by Display's texel offset over the tile size.
  - The rotation's cos/sin are separate x87 fcos/fsin rounded to float.
  - The same rectangle intersection as KernelResources, with the same wrong-field clearing, is kept.
- Bug found by the test: SpriteInstance and the layers need `JET_HEAP_OBJECT`. Their deleting destructors free
  with `Alloc::Delete`, and SpriteManager::Purge deletes sprites that callers allocated in Jet's heap.
- `tests/diff_r2d.cpp` drives both modules from identical memory against a stand-in renderer. The Display's
  virtual slots log every call, draws included with all vertex data. Textures, the texture manager and the kernel
  log their calls too. RenderSystem's real non-virtual Display setters run on the stand-in's state block.
  - Covered: SelectionBox render; the whole Canvas cycle (Create / setters / Update with random dirty lists /
    Render with texel offsets and wire frame / Destroy); sprites (Open with UVI handling, all setters, the
    attribute update with rotation and aspect correction, Render with the alpha save/restore, SpriteManager
    Add / Render / Update / Remove / Purge).
  - Compared: the call logs, the Display state and the objects' memory.
  - Result: **249 020 checks, 0 mismatches.**
  - SpriteResource is not exported. The test finds its vtable through RTTI.
  - Limit: its loader cannot be compared outside the game. ConfigParser opens the file, then MemoryFile::Open
    fails (result 3), even for the game's own config files. Only the failure path is compared.
- In game: menu, clean exit, JetLog identical. The menu does not seem to draw sprites.
- Harness notes:
  - Resource's constructor reads `Resource::resourceSystem` (+0xdc). The test points it at a zeroed buffer.
  - KernelStandard's 32-bit bitmap format is enum value 2.
- New public tools:
  - `tools/annotate_asm.py`: labels a dumpbin listing with import names (demangled), string literals and export
    names.
  - `tools/compare_vtables.py`.
- Modules in the game process at the menu (32-bit PowerShell, `probe/modules_run.ps1`):
  - Kernel_7c, KernelResources_7c and Render2DCore_7c run without their `_orig`.
  - Both input drivers are loaded: InputWin32 (with its `_orig`) and InputDirectX, plus DINPUT8. Which one takes
    the input is still open.
- Offline file reading:
  - Outside the game, Jet's MemoryFile cannot read a file opened through File. The read goes through
    IOSlaveResource / IOSlaveFile, and IOSlaveFile reports a size of 0.
  - So ConfigParser-based loaders (sprite.txt, bmframe.txt, colour.txt, and KernelResource itself) cannot be
    diff-tested in-process yet. Fixing this means finding what Jet initialises for File I/O.

### Third-party deps and their x64 availability
| 32-bit dep | x64 replacement |
|---|---|
| msvcr80/msvcp80 | modern MSVC CRT (rebuild) |
| cg.dll / cgGL.dll 2.2 | Cg 3.1 x64 (last NVIDIA release), or port shaders off Cg |
| d3dx9_42.dll | d3dx9_43 x64 (June 2010 redist), or remove d3dx |
| OpenAL32.dll / wrap_oal.dll | OpenAL Soft x64 |
| nvtt.dll 2.0.6 | nvtt is open source, build x64 |
| cudart.dll | only for nvtt GPU path, optional |
| libeay32/ssleay32 0.9.8e | OpenSSL x64 (API migration needed) |
| ijl10.dll (Intel JPEG) | libjpeg-turbo (who imports it: TODO) |

Tools that are separate processes and can stay 32-bit: Launcher, ContentManager (CMP), CCP, TADDaemon,
TrainzUtil, TrainzDiag, JAConverter, XCrashReport.

### TrainzScript native API (the add-on compatibility contract)
- `scripts\*.gs` (104 headers) declare **859 `native` methods in 60 classes** (Vehicle 82, Train 76, World 72,
  Interface 40, Browser 38, MapObject 31, Asset 29...). Inventory: `gs_native_api.tsv` (`tools/gs_natives.py`).
- trainz.exe holds 1009 `Class::Method` strings in 140 classes (`qualified_names_trainz.txt`) with the same
  per-class counts: almost certainly the native-binding registration table (name string ↔ C++ implementation).
  Following xrefs from these strings maps every GS native to its implementation. TODO after Ghidra analysis.
- **Binding format, confirmed**: each native is a signature string `$<ret>@<Class>::<Method>(<params>)`,
  e.g. `$string@Asset::GetCreatorName()` at 0x00ae53e0. A per-class registrar (Asset: FUN_009cfd80) interns
  it with `Jet::PStringCache::Create`, passes the handler address (PUSH imm in Asset, MOV EBP,imm in Vehicle)
  and calls a register function (Asset: FUN_00611230(node,&sig,&cls,handler)); nodes go into a global list
  at FUN_007b2f80()+0x4c.
- `ghidra_scripts/MapGsNatives.java` (signature-driven forward scan) → `gs_native_handlers.tsv`:
  936 signature strings, **850 natives with handler, 113 engine→script callback refs (83 distinct)**.
  Handlers renamed `gs_<Class>_<Method>` in the project. `tools/verify_gs_map.py` checks: 782 header natives
  matched; 0 handlers shared between methods; 0 wrong names. Remaining diffs are a header-parser bug
  (`OnlineAccess` read as `<global>`) plus a few natives not in the headers (e.g. Interface::EnableMessageBar).
- trainz.exe after mapping: 46 420 functions, 51.0% named.
- Only 82 strings use the `Class::Method>` log form (`log_names_trainz.txt`); JetLog builds most prefixes at runtime.

### Config
- `Trainz.cfg` (88 bytes) is binary, not the options file. Real options: TODO (look for `trainzoptions.txt`).

## Open questions
- Who loads `ijl10.dll`, `cudart.dll`, `dx7vb.dll` at runtime?
- Does `trainznativeinterface.dll` + OpenSSL do licence checks that a rebuilt exe must keep?
- Exact GS (TrainzScript) bytecode format and whether it stores 32-bit pointers in the VM.
