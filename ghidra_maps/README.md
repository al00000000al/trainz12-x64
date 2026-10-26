# Ghidra maps

Symbol maps exported from the Ghidra project of Trainz Simulator 2012 (32-bit build, original DLLs and trainz.exe).
They hold addresses, sizes and names only: no decompiled code, no disassembly, no strings from the binaries.
To get the decompiled C, import your own copy of the game into Ghidra and run `ghidra_scripts/DecompileAll.java`
(see `tools/ghidra_analyze.cmd`, `tools/ghidra_all.ps1`).

- `jet/`: the Jet engine modules (`*_7c.dll`, including WaterSystem_7c), one folder per README group: `core/`,
  `input/`, `graphics/`, `ui/`, `debug/`, `not_loaded/`.
- `trainz/`: the game itself (`trainz.exe`, `trainznativeinterface.dll`).

- `<module>.functions.tsv` (from `ExportFunctionStats.java`): `entry size namespace name source thunk`.
  `entry` is the virtual address at the module's preferred image base; `source` is Ghidra's symbol source
  (`IMPORTED` = export/import name, `ANALYSIS` = RTTI/vtable naming, `USER_DEFINED` = named during the port, `DEFAULT` = unnamed).
- `vtnames_<module>.tsv` (from `NameVtableFunctions.java`): `entry new_name all_vftable_slots`, the class and slot
  of each virtual function found through RTTI vtables (`Class::vfN`); a function shared by several slots lists them all.
