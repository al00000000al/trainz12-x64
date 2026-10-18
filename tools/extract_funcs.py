"""Pull the functions whose name matches a regex out of a DecompileAll dump, dropping blank and offset lines.

usage: python extract_funcs.py <dump.c> <name regex | @start-end hex address range> [out file]
"""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
if sys.argv[2].startswith("@"):
    lo, hi = (int(x, 16) for x in sys.argv[2][1:].split("-"))
    pat = None
else:
    pat = re.compile(sys.argv[2])


def wanted(addr, name):
    return lo <= int(addr, 16) < hi if pat is None else bool(pat.search(name))
blocks, cur, keep = [], [], False
for line in text + ["// ==== END  END  [X, 0 bytes]"]:
    m = re.match(r"^// ==== (\w+)  (\S+)", line)
    if m:
        if keep:
            blocks.append(cur)
        cur, keep = [line], m.group(2) != "END" and wanted(m.group(1), m.group(2))
        continue
    if line.strip() and not re.match(r"^\s+/\* 0x[0-9a-f]+ ", line):
        cur.append(line)
out = "\n".join("\n".join(b) for b in blocks) + "\n"
if len(sys.argv) > 3:
    Path(sys.argv[3]).write_text(out, encoding="utf-8")
    print(f"{len(blocks)} functions, {out.count(chr(10))} lines -> {sys.argv[3]}")
else:
    sys.stdout.write(out)
