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

### Jet::LZSSCompress compression (to do)
- CompressBlock 0x6ec0 / DecompressBlock 0x7150 work on IOSlave streams (KernelStandard).
- Header byte: low nibble = level 0..9 (0 = stored), bits 4-5 = window mode. Mode 0: LZ with 32-bit flag words,
  3-bit lengths (7 = extended by a byte), 1-byte offsets. Levels 1-6: 16-bit tokens, offset bits per mode from
  the table at 0x1001814c, literal runs. Levels 7-9: Huffman-coded LZ77 with Deflate-like length tables
  (0x10018458). Likely the codec behind .ja archives and asset data: decompression must be bit-exact.

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
