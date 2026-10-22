import sys,collections,json
sys.path.insert(0,r"D:\trainz12-decomp\notes\data")
from model import fn,SUB
subs=collections.OrderedDict()
for f in fn: subs.setdefault(f["sub"],[]).append(f)
mods_all=["trainz.exe","Launcher.exe","ContentManager.exe","CCP.exe","TADDaemon.exe","TrainzUtil.exe","TrainzDiag.exe"]
print("sub\texports\tcode_exports\tdata\tsize_exp\timported_syms\tunused\tmodules")
for s,l in sorted(subs.items()):
    n=len(l); data=sum(1 for f in l if f["kind"]=="data"); code=n-data
    size=sum(f["size"] for f in l)
    used=[f for f in l if f["mods"]]
    mods=collections.Counter()
    for f in used:
        for m in f["mods"]: mods[m]+=1
    # jet modules vs exe
    jm=sorted(m.replace("_7c.dll","") for m in mods if m.endswith("_7c.dll"))
    ex=[m for m in mods if not m.endswith("_7c.dll")]
    print(f"{s}\t{n}\t{code}\t{data}\t{size}\t{len(used)}\t{n-len(used)}\tJet:{len(jm)} exe:{','.join(sorted(ex))}")
json.dump({s:[(f["m"],f["mods"]) for f in l] for s,l in subs.items()},open(r"D:\trainz12-decomp\notes\data\sub_imports.json","w"))
