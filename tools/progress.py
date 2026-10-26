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

# (label, what it is, map dir, modules); the last group was not seen loaded (MODLOG "What actually loads")
GROUPS = [
    ("Engine core", "Jet: memory, strings, files, .ja archives, config, threads, logs, resources", "jet",
     ["Kernel_7c", "KernelStandard_7c", "KernelResources_7c", "KernelResource_7c"]),
    ("Input", "Jet: keyboard, mouse, joysticks, key bindings", "jet",
     ["InputWin32_7c", "InputSystem_7c", "InputDirectX_7c"]),
    ("Graphics", "Jet: 2D/3D scene, meshes, materials, water; OpenGL and DirectX 9 renderers (picked in the launcher)",
     "jet",
     ["Render2DCore_7c", "RenderSystem_7c", "Render3DCore_7c", "Render3DFoundation_7c", "Render3DExtension_7c",
      "RenderOpenGL_7c", "RenderDirectX9_7c", "WaterSystem_7c"]),
    ("User interface", "Jet: windows, buttons, menus, fonts", "jet",
     ["InterfaceSystem_7c", "InterfaceResources_7c"]),
    ("Debug", "Jet: debug console and overlays", "jet",
     ["DebugSupport_7c", "DebugInterface_7c"]),
    ("Game", "trainz.exe: simulation, trains, routes, Surveyor, TrainzScript, assets; TNI plugin API", "trainz",
     ["trainz.exe", "trainznativeinterface.dll"]),
    ("Not seen loaded", "Jet: Lexer (nothing references it), movie player (not loaded at the menu; re-check in a session)",
     "jet", ["Lexer_7c", "DSMoviePlayer_7c"]),
]
JET = [n for _, _, g, names in GROUPS if g == "jet" for n in names]


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
    rows, stats = [], []  # stats: (label, what, map dir, done bytes, bytes, modules done, modules)
    for label, what, group, names in GROUPS:
        g = [label, what, group, 0, 0, 0, len(names)]
        rows.append(f"| **{label}** | | | | |")
        for name in names:
            size, funcs = code_size(group, name)
            total = int(totals.get(name, 0))
            mine = owned(name)
            # a module with one or no export (a plugin entry only) is ours when its entry point is
            share = (mine / total if total > 1 else float(mine > 0)) if group == "jet" else 0.0
            g[3] += size * share
            g[4] += size
            g[5] += share >= 0.999
            status = ("done" if share >= 1 else "done*" if share >= 0.999 else
                      "partial" if share > 0 else "not started")
            exp = f"{mine}/{total}" if group == "jet" else "-"
            rows.append(f"| {name} | {size / 1024:,.0f} KB | {funcs:,} | {exp} | {status} |")
        stats.append(g)

    def line(label, done, size, mods_done=None, mods=None):
        mods_txt = f"  {mods_done}/{mods} modules" if mods else ""
        return (f"{label:<18}{bar(done / size)} {100 * done / size:5.1f} %  "
                f"{(size - done) / 1024:>6,.0f} KB left of {size / 1024:>5,.0f}{mods_txt}")

    def total(sel):
        return [sum(g[i] for g in sel) for i in (3, 4, 5, 6)]

    jet = [g for g in stats if g[2] == "jet" and g[0] != "Not seen loaded"]
    steps_done, steps = plan_steps()
    today = datetime.date.today().isoformat()
    out = [
        START,
        f"Measured by code size: a module counts in proportion to the exports that are already ours "
        f"(`tools/progress.py`, {today}).",
        "",
        "```",
        line("Whole game", *total(stats)[:2]),
        "",
        line("Jet engine", *total(jet)),
        *(line("  " + g[0], g[3], g[4], g[5], g[6]) for g in jet),
        "",
        *(line(g[0], g[3], g[4], g[5], g[6]) for g in stats if g not in jet),
        "```",
        "",
        "Jet is Auran's general engine, a set of DLLs: core services, input, graphics and UI. The train simulation "
        "itself (physics, routes, sessions, Surveyor, TrainzScript, the asset system) is the game, `trainz.exe`, "
        "the biggest part, not started yet.",
        "",
        "| Group | What it is |",
        "|---|---|",
        *(f"| {g[0]} | {g[1]} |" for g in stats),
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
