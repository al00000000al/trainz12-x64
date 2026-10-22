import sys,collections,json,csv,re
sys.path.insert(0,r"D:\trainz12-decomp\notes\data")
from model import fn,SUB
D=r"D:\trainz12-decomp\notes\data"
# attributed code size per class (namespace size incl. non-exported members)
nsz=collections.Counter(); nfn=collections.Counter()
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    ns=r["namespace"]
    c=ns[5:] if ns.startswith("Jet::") else ns
    if r["thunk"]=="true": continue
    nsz[c]+=int(r["size"]); nfn[c]+=1
# globals touched per class
refs=list(csv.DictReader(open(D+r"\datarefs.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
wr=collections.defaultdict(set)
for r in refs:
    if r["block"]!=".data": continue
    ns=r["func_ns"]; c=ns[5:] if ns.startswith("Jet::") else ns
    if "WRITE" in r["type"]: wr[c].add(r["data_addr"])
cl=collections.OrderedDict()
for f in fn:
    d=cl.setdefault(f["cls"],dict(exp=0,data=0,imp=0,mods=set(),sub=f["sub"]))
    d["exp"]+=1
    if f["kind"]=="data": d["data"]+=1
    if f["mods"]: d["imp"]+=1; d["mods"].update(f["mods"])
rows=[]
for c,d in cl.items():
    jm=sorted(m.replace("_7c.dll","") for m in d["mods"] if m.endswith("_7c.dll"))
    ex="E" if "trainz.exe" in d["mods"] else "-"
    tools=sum(1 for m in d["mods"] if m.endswith(".exe") and m!="trainz.exe")
    rows.append((d["sub"][:2],c,d["exp"],d["data"],d["imp"],nfn.get(c,0),nsz.get(c,0),len(jm),ex,tools,len(wr.get(c,[]))))
rows.sort()
print("sub|class|exports|data|imported|fns|bytes|jetmods|exe|tools|globals_written")
for r in rows: print("|".join(map(str,r)))
