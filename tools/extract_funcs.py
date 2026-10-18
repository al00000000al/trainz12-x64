"""Pull the functions whose name matches a regex out of a DecompileAll dump, dropping blank and offset lines.

usage: python extract_funcs.py <dump.c> <name regex> [out file]
"""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
pat = re.compile(sys.argv[2])
blocks, cur, keep = [], [], False
for line in text + ["// ==== END  END  [X, 0 bytes]"]:
    m = re.match(r"^// ==== (\w+)  (\S+)", line)
    if m:
        if keep:
            blocks.append(cur)
        cur, keep = [line], bool(pat.search(m.group(2)))
        continue
    if line.strip() and not re.match(r"^\s+/\* 0x[0-9a-f]+ ", line):
        cur.append(line)
out = "\n".join("\n".join(b) for b in blocks) + "\n"
if len(sys.argv) > 3:
    Path(sys.argv[3]).write_text(out, encoding="utf-8")
    print(f"{len(blocks)} functions, {out.count(chr(10))} lines -> {sys.argv[3]}")
else:
    sys.stdout.write(out)
