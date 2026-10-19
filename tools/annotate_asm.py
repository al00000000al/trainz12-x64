"""Annotate a dumpbin /disasm listing of a 32-bit PE: import slots become their demangled names, addresses of
string literals get the string, and calls/jumps to local functions get the name from an exports TSV.

usage: python annotate_asm.py <module.dll> <listing.txt> [exports.tsv] [undname.exe] > annotated.txt
  The listing may be raw dumpbin output or the compact "ADDR mnemonic operands" form. Without undname.exe the
  import names stay mangled.
"""
import re
import struct
import subprocess
import sys

dll, listing = sys.argv[1], sys.argv[2]
exports_tsv = sys.argv[3] if len(sys.argv) > 3 else None
undname = sys.argv[4] if len(sys.argv) > 4 else None

d = open(dll, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
opt = pe + 24
base = struct.unpack_from("<I", d, opt + 28)[0]
secs = [struct.unpack_from("<8sIIII", d, opt + optsz + 40 * i) for i in range(nsec)]


def off(rva):
    for _, vs, va, rs, ro in secs:
        if va <= rva < va + max(vs, rs):
            return ro + rva - va
    return None


def cstr(o, limit=120):
    e = d.find(b"\0", o)
    s = d[o:e]
    if 0 < len(s) <= limit and all(32 <= c < 127 for c in s):
        return s.decode("latin1")
    return None


# imports: IAT slot address -> (dll, name)
names = {}
imp_rva = struct.unpack_from("<I", d, opt + 96 + 8)[0]
o = off(imp_rva)
while True:
    oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", d, o)
    if not name_rva:
        break
    dname = cstr(off(name_rva))
    lookup = off(oft or ft)
    k = 0
    while True:
        entry = struct.unpack_from("<I", d, lookup + 4 * k)[0]
        if not entry:
            break
        if entry & 0x80000000:
            n = f"#{entry & 0xFFFF}"
        else:
            n = cstr(off(entry) + 2, 4096)
        names[base + ft + 4 * k] = (dname, n)
        k += 1
    o += 20

if undname:
    mangled = sorted({n for _, n in names.values() if n and n.startswith("?")})
    demangled = {}
    for i in range(0, len(mangled), 60):
        out = subprocess.run([undname, *mangled[i:i + 60]], capture_output=True, text=True).stdout
        got = [l.split('"', 1)[1].rsplit('"', 1)[0] for l in out.splitlines() if l.startswith("is :- ")]
        demangled.update(zip(mangled[i:i + 60], got))
    names = {a: (dn, demangled.get(n, n)) for a, (dn, n) in names.items()}

local = {}
if exports_tsv:
    for line in open(exports_tsv, encoding="utf-8"):
        p = line.rstrip("\n").split("\t")
        if len(p) >= 4 and p[1].startswith("0x"):
            local.setdefault(base + int(p[1], 16), p[3].strip())

for line in open(listing, encoding="utf-8", errors="replace"):
    line = line.rstrip("\n")
    notes = []
    for m in re.finditer(r"\b([0-9A-F]{8})h?\b", line[10:] if re.match(r"\s*[0-9A-F]{8}", line) else line):
        a = int(m.group(1), 16)
        if a in names:
            notes.append(f"{names[a][1]}")
        elif a in local:
            notes.append(local[a])
        else:
            so = off(a - base) if a > base else None
            s = cstr(so) if so is not None else None
            if s and len(s) >= 2:
                notes.append(repr(s))
    print(line + ("   ; " + " | ".join(notes) if notes else ""))
