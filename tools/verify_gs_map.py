"""Cross-check the GS native map from Ghidra against the .gs headers and the current function names.

usage: python verify_gs_map.py <gs_native_handlers.tsv> <gs_native_api.tsv> <trainz.exe.functions.tsv>
"""
import csv
import sys
from collections import defaultdict


def rows(p):
    with open(p, encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


gmap, api, funcs = rows(sys.argv[1]), rows(sys.argv[2]), rows(sys.argv[3])
natives = [r for r in gmap if r["kind"] == "native"]
callbacks = [r for r in gmap if r["kind"] == "callback"]

declared = {f"{r['class']}::{r['method']}" for r in api}
mapped = {r["class_method"] for r in natives}
print(f"declared natives in headers: {len(declared)}   mapped in exe: {len(mapped)}")
print(f"  declared and mapped: {len(declared & mapped)}")
print(f"  declared, not mapped: {len(declared - mapped)}  e.g. {sorted(declared - mapped)[:12]}")
print(f"  mapped, not declared: {len(mapped - declared)}  e.g. {sorted(mapped - declared)[:12]}")

# one handler must not serve two different methods (would mean the scan grabbed a neighbour's handler)
by_handler = defaultdict(set)
for r in natives:
    by_handler[r["handler"]].add(r["class_method"])
shared = {h: m for h, m in by_handler.items() if len(m) > 1}
print(f"handlers: {len(by_handler)}   shared by several methods: {len(shared)}")
for h, m in list(shared.items())[:8]:
    print(f"    {h}: {sorted(m)}")

# the name in Ghidra must match one of the methods mapped to that handler
name_at = {r["entry"].lower().lstrip("0"): r["name"] for r in funcs}
wrong = []
for h, methods in by_handler.items():
    name = name_at.get(h.lower().lstrip("0"))
    want = {"gs_" + m.replace("::", "_") for m in methods}
    if name is None or (name.startswith("gs_") and name not in want):
        wrong.append((h, name, sorted(want)))
print(f"handlers whose gs_ name does not match: {len(wrong)}")
for w in wrong[:10]:
    print("   ", w)
print(f"callbacks (engine -> script): {len({r['class_method'] for r in callbacks})} distinct")
