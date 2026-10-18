"""Inventory of the TrainzScript native API (the methods add-on scripts call into the engine).

Parses every .gs header for `class X [isclass Y]` and `native` method declarations.
usage: python gs_natives.py <scripts dir> <out.tsv>
"""
import re
import sys
from collections import Counter
from pathlib import Path

CLASS = re.compile(r"^\s*(?:(?:final|static|game)\s+)*class\s+(\w+)(?:\s+isclass\s+([\w,\s]+))?", re.M)
NATIVE = re.compile(r"^\s*((?:(?:public|final|mandatory|obsolete|thread|static)\s+)*)native\s+([^;{]+?)\s*\(([^)]*)\)\s*;",
                    re.M)

rows = []
for path in sorted(Path(sys.argv[1]).rglob("*.gs")):
    text = path.read_text(encoding="latin-1")
    text = re.sub(r"//[^\n]*", "", text)  # drop line comments
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    classes = [(m.start(), m.group(1), (m.group(2) or "").strip()) for m in CLASS.finditer(text)]
    for m in NATIVE.finditer(text):
        owner = next((c for c in reversed(classes) if c[0] < m.start()), (0, "<global>", ""))
        ret_name = m.group(2).split()
        rows.append((path.name, owner[1], owner[2], ret_name[-1], " ".join(ret_name[:-1]),
                     " ".join(m.group(3).split()), m.group(1).strip()))

Path(sys.argv[2]).write_text(
    "file\tclass\tbase\tmethod\treturns\tparams\tmodifiers\n" + "".join("\t".join(r) + "\n" for r in rows),
    encoding="utf-8")
per_class = Counter(r[1] for r in rows)
print(f"native methods: {len(rows)} in {len(per_class)} classes")
for c, k in per_class.most_common(15):
    print(f"  {k:4} {c}")
