import sys,collections,json,csv
sys.path.insert(0,r"D:\trainz12-decomp\notes\data")
from model import fn,SUB,imps
D=r"D:\trainz12-decomp\notes\data"
out={}
# T1: per subsystem
owners_sz=collections.defaultdict(lambda:[0,0])
owner={int(k,16):v for k,v in json.load(open(D+r"\owners.json")).items()}
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    if r["thunk"]=="true": continue
    o=owner.get(int(r["entry"],16),"?? unowned")
    owners_sz[o][0]+=1; owners_sz[o][1]+=int(r["size"])
subs=collections.OrderedDict()
for f in fn: subs.setdefault(f["sub"],[]).append(f)
def short(m): return m.replace("_7c.dll","").replace(".dll","").replace(".exe","")
L=["| Group | exports (code/data) | imported by anyone | code fns / bytes (incl. unexported + attributed helpers) | Jet modules importing | exes importing |","|---|---|---|---|---|---|"]
for s,l in sorted(subs.items()):
    data=sum(1 for f in l if f["kind"]=="data"); used=[f for f in l if f["mods"]]
    mods=collections.Counter(m for f in used for m in f["mods"])
    jm=sorted(short(m) for m in mods if m.endswith("_7c.dll")); ex=sorted(short(m) for m in mods if not m.endswith("_7c.dll"))
    n,b=owners_sz.get(s,(0,0))
    L.append(f"| {s} | {len(l)} ({len(l)-data}/{data}) | {len(used)} | {n} / {b:,} | {len(jm)} | {len(ex)}: {', '.join(ex) if ex else '-'} |")
for s in sorted(owners_sz):
    if s[:2] in("15","99") or s.startswith("??"):
        n,b=owners_sz[s]; L.append(f"| {s} | - | - | {n} / {b:,} | - | - |")
out["T1"]="\n".join(L)
# T2: module -> imported symbol count
L=["| Importer | KernelStandard exports used |","|---|---|"]
for m,v in sorted(imps.items(),key=lambda x:-len(x[1])): L.append(f"| {m} | {len(v)} |")
out["T2"]="\n".join(L)
# T3: matrix subsystem x importer group (Jet modules count, exe)
# T4 class table
rows=[l.split("|") for l in open(D+r"\classtab.txt").read().strip().split("\n")[1:]]
L=["| grp | class | exports (data) | imported | fns | bytes | Jet mods | trainz.exe | tool exes | globals written |","|---|---|---|---|---|---|---|---|---|---|"]
for r in rows:
    L.append(f"| {r[0]} | {r[1]} | {r[2]} ({r[3]}) | {r[4]} | {r[5]} | {int(r[6]):,} | {r[7]} | {'yes' if r[8]=='E' else '-'} | {r[9]} | {r[10]} |")
out["T4"]="\n".join(L)
json.dump(out,open(D+r"\tables.json","w"))
print(out["T1"]); print(out["T2"])
