"""List what a module imports from another DLL, demangled with MSVC undname.exe.

usage: python dump_imports.py <module.dll> <imported dll name, e.g. KernelStandard_7c.dll> <undname.exe>
"""
import subprocess
import sys

import pefile

pe = pefile.PE(sys.argv[1])
want = sys.argv[2].lower()
names = []
for imp in pe.DIRECTORY_ENTRY_IMPORT:
    if imp.dll.decode().lower() == want:
        names = [i.name.decode() for i in imp.imports if i.name]
out = []
for i in range(0, len(names), 80):
    r = subprocess.run([sys.argv[3], *names[i:i + 80]], capture_output=True, text=True).stdout
    out += [l.split('"', 1)[1].rsplit('"', 1)[0] for l in r.splitlines() if l.startswith("is :- ")]
for m, d in zip(names, out):
    print(f"{m}\t{d}")
