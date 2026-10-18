"""Make an x86 import library for an original Jet module from its export dump, so our code can call it.

Functions are imported by their mangled names; data exports (vftables, statics) are marked DATA and must be
declared __declspec(dllimport) on our side.

usage: python gen_implib.py <exports.tsv> <module name, e.g. KernelStandard_7c> <out dir> <path to lib.exe>
"""
import csv
import subprocess
import sys
from pathlib import Path

exports_tsv, module, out_dir, lib_exe = sys.argv[1:5]
with open(exports_tsv, encoding="utf-8") as f:
    exports = list(csv.DictReader(f, delimiter="\t"))

out = Path(out_dir)
out.mkdir(parents=True, exist_ok=True)
lines = [f"LIBRARY {module}.dll", "EXPORTS"]
for e in exports:
    name = e["mangled"]
    if not name:
        continue
    is_data = name.startswith("?") and "(" not in e["demangled"]
    lines.append(f"    {name}" + (" DATA" if is_data else ""))
def_path = out / f"{module}.imp.def"
def_path.write_text("\n".join(lines) + "\n", encoding="ascii")

r = subprocess.run([lib_exe, "/nologo", f"/def:{def_path}", "/machine:x86", f"/out:{out / (module + '.lib')}"],
                   capture_output=True, text=True)
if r.returncode:
    sys.exit(r.stdout + r.stderr)
print(f"{module}: {len(exports)} exports -> {out / (module + '.lib')}")
