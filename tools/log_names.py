"""Collect `Class::Method>` style log prefixes from a binary: each one names the function that prints it.

usage: python log_names.py <file> [out.txt]
"""
import re
import sys
from collections import Counter
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
# ASCII strings, then the prefix before '>' when it looks like a C++ qualified name
strings = re.findall(rb"[\x20-\x7e]{6,}", data)
pat = re.compile(rb"^([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)+)\s*>")
names = Counter()
for s in strings:
    m = pat.match(s)
    if m:
        names[m.group(1).decode()] += 1
classes = Counter(n.rsplit("::", 1)[0] for n in names)
print(f"strings: {len(strings)}  distinct Class::Method> names: {len(names)}  classes: {len(classes)}")
for c, k in classes.most_common(25):
    print(f"  {k:4} {c}")
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text("\n".join(sorted(names)) + "\n")
