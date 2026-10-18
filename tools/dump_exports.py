"""Dump a DLL's exports (ordinal, RVA, mangled, demangled) to TSV, demangling with MSVC undname.exe.

usage: python dump_exports.py <dll> <out.tsv> <path to undname.exe>
"""
import subprocess
import sys
from pathlib import Path

import pefile

pe = pefile.PE(sys.argv[1])
exports = [(s.ordinal, s.address, s.name.decode() if s.name else "") for s in pe.DIRECTORY_ENTRY_EXPORT.symbols]
mangled = [n for _, _, n in exports if n.startswith("?")]

# undname takes several names as arguments; batch them to stay under the command-line limit
demangled = {}
cur = None
for i in range(0, len(mangled), 80):
    out = subprocess.run([sys.argv[3], *mangled[i:i + 80]], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if line.startswith("Undecoration of :- "):
            cur = line.split('"')[1]
        elif line.startswith("is :- ") and cur:
            demangled[cur] = line.split('"', 1)[1].rsplit('"', 1)[0]
            cur = None

with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("ordinal\trva\tmangled\tdemangled\n")
    for o, a, n in exports:
        f.write(f"{o}\t0x{a:x}\t{n}\t{demangled.get(n, n)}\n")
print(f"{len(exports)} exports, {len(demangled)} demangled -> {sys.argv[2]}")
