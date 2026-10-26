"""Port progress: how much of the game's native code is ours, weighted by code size.

Code size and function counts come from ghidra_maps/*/<module>.functions.tsv (non-thunk functions),
exports we own from src/jet/<module>/implemented.txt, export totals from tools/progress_exports.tsv.
A module's share is owned/total exports, applied to its code size. Modules with no src/ directory count as 0.

usage: python -I tools/progress.py                 print the README block
       python -I tools/progress.py --write         put it between the progress markers in README.md
       python -I tools/progress.py --exports DIR   refresh tools/progress_exports.tsv from exports/*.tsv
"""
import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOTALS = ROOT / "tools" / "progress_exports.tsv"
README = ROOT / "README.md"
START, END = "<!-- progress:start -->", "<!-- progress:end -->"

# modules the game loads, in port order; the movie players and Lexer are not loaded (PORT_PLAN)
JET = ["Kernel_7c", "KernelStandard_7c", "KernelResources_7c", "KernelResource_7c", "Render2DCore_7c",
       "InputWin32_7c", "InputSystem_7c", "RenderSystem_7c", "Render3DCore_7c", "Render3DFoundation_7c",
       "Render3DExtension_7c", "RenderOpenGL_7c", "InterfaceSystem_7c", "InterfaceResources_7c",
       "WaterSystem_7c", "InputDirectX_7c", "RenderDirectX9_7c", "DebugSupport_7c", "DebugInterface_7c",
       "Lexer_7c", "DSMoviePlayer_7c"]
TRAINZ = ["trainz.exe", "trainznativeinterface.dll"]


def code_size(group, name):
    """(bytes, functions) of non-thunk functions in a Ghidra map."""
    fname = name if name.endswith((".exe", ".dll")) else name + ".dll"
    size = count = 0
    with open(ROOT / "ghidra_maps" / group / f"{fname}.functions.tsv", encoding="utf-8") as f:
        next(f)
        for line in f:
            col = line.rstrip("\n").split("\t")
            if col[5] != "true":
                size += int(col[1])
                count += 1
    return size, count


def owned(name):
    impl = ROOT / "src" / "jet" / name / "implemented.txt"
    if not impl.exists():
        return 0
    return sum(1 for line in impl.read_text(encoding="utf-8").splitlines()
               if line.strip() and not line.startswith("#"))


def refresh_totals(exports_dir):
    rows = []
    for name in JET:
        tsv = Path(exports_dir) / f"{name}.tsv"
        lines = tsv.read_text(encoding="utf-8").splitlines()[1:]
        rows.append(f"{name}\t{sum(1 for l in lines if l.strip())}")
    TOTALS.write_text("module\texports\n" + "\n".join(rows) + "\n", encoding="utf-8")


def plan_steps():
    text = (ROOT / "PORT_PLAN.md").read_text(encoding="utf-8")
    steps = re.findall(r"^(\d+b?)\. (.*)$", text.split("## Next steps", 1)[1], re.M)
    return sum(s.startswith("~~") for _, s in steps), len(steps)


def bar(frac, width=30):
    full = round(frac * width)
    return "█" * full + "░" * (width - full)


def block():
    totals = dict(l.split("\t") for l in TOTALS.read_text(encoding="utf-8").splitlines()[1:])
    rows, groups = [], {}
    for group, names in (("jet", JET), ("trainz", TRAINZ)):
        for name in names:
            size, funcs = code_size(group, name)
            total = int(totals.get(name, 0))
            mine = owned(name)
            # a module with one or no export (a plugin entry only) is ours when its entry point is
            share = (mine / total if total > 1 else float(mine > 0)) if group == "jet" else 0.0
            done = size * share
            g = groups.setdefault(group, [0, 0])
            g[0] += done
            g[1] += size
            status = ("done" if share >= 1 else "done*" if share >= 0.999 else
                      "partial" if share > 0 else "not started")
            exp = f"{mine}/{total}" if group == "jet" else "-"
            rows.append(f"| {name} | {size / 1024:,.0f} KB | {funcs:,} | {exp} | {status} |")
    done = sum(g[0] for g in groups.values())
    size = sum(g[1] for g in groups.values())
    steps_done, steps = plan_steps()
    jet, tz = groups["jet"], groups["trainz"]
    today = datetime.date.today().isoformat()
    out = [
        START,
        f"Measured by code size: a module counts in proportion to the exports that are already ours "
        f"(`tools/progress.py`, {today}).",
        "",
        "```",
        f"Whole game   {bar(done / size)} {100 * done / size:5.1f} %  ({done / 2**20:.2f} of {size / 2**20:.2f} MB)",
        f"Jet engine   {bar(jet[0] / jet[1])} {100 * jet[0] / jet[1]:5.1f} %",
        f"trainz.exe   {bar(tz[0] / tz[1])} {100 * tz[0] / tz[1]:5.1f} %",
        "```",
        "",
        f"Plan steps finished: {steps_done} of {steps} (`PORT_PLAN.md`). After the 32-bit replacement come the "
        f"x64 build and the add-on compatibility tests.",
        "",
        "<details><summary>Per module</summary>",
        "",
        "| Module | Code | Functions | Exports ours | Status |",
        "|---|---:|---:|---:|---|",
        *rows,
        "",
        "`done*`: KernelStandard_7c keeps one export, `HardwareException::GetCallStack` (an x86 stack walk), "
        "forwarded to the original until the x64 rewrite.",
        "",
        "</details>",
        END,
    ]
    return "\n".join(out)


def main(argv):
    if "--exports" in argv:
        refresh_totals(argv[argv.index("--exports") + 1])
    text = block()
    if "--write" in argv:
        readme = README.read_text(encoding="utf-8")
        if START not in readme or END not in readme:
            sys.exit(f"README.md has no {START} ... {END} markers")
        head, rest = readme.split(START, 1)
        README.write_text(head + text + rest.split(END, 1)[1], encoding="utf-8", newline="\n")
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        print(text)


if __name__ == "__main__":
    main(sys.argv[1:])
