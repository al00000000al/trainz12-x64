"""Census of every native PE in bin/: arch, .text size, named exports, RTTI classes, imported DLLs.

usage: uv run --with pefile python census.py <bin dir> [out.json]
"""
import json
import re
import sys
from pathlib import Path

import pefile

RTTI = re.compile(rb"\.\?A[VU][A-Za-z0-9_@?$]{2,200}@@")

rows = []
for p in sorted(Path(sys.argv[1]).iterdir()):
    if p.suffix.lower() not in (".exe", ".dll", ".ocx"):
        continue
    try:
        pe = pefile.PE(str(p))
    except pefile.PEFormatError:
        continue
    clr = pe.OPTIONAL_HEADER.DATA_DIRECTORY[14].VirtualAddress != 0
    text = sum(s.Misc_VirtualSize for s in pe.sections if s.Characteristics & 0x20000000)
    exports = [s.name.decode(errors="replace") for s in getattr(pe, "DIRECTORY_ENTRY_EXPORT", None).symbols
               if s.name] if hasattr(pe, "DIRECTORY_ENTRY_EXPORT") else []
    imports = [i.dll.decode() for i in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]
    rows.append(dict(name=p.name, machine=hex(pe.FILE_HEADER.Machine), dotnet=clr, code=text,
                     exports=len(exports), cxx_exports=sum(e.startswith("?") for e in exports),
                     rtti=len(set(RTTI.findall(p.read_bytes()))), imports=imports))

for r in rows:
    print(f"{r['name']:30} {'.NET' if r['dotnet'] else 'native':6} code={r['code']:>9} "
          f"exports={r['exports']:>5} c++={r['cxx_exports']:>5} rtti={r['rtti']:>5}")
native = [r for r in rows if not r["dotnet"]]
print(f"\nnative code total: {sum(r['code'] for r in native)/1e6:.1f} MB, "
      f"C++ exports: {sum(r['cxx_exports'] for r in native)}, RTTI: {sum(r['rtti'] for r in native)}")
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text(json.dumps(rows, indent=1))
