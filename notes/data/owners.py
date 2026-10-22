import csv,collections,json,re,sys
sys.path.insert(0,r"D:\trainz12-decomp\notes\data")
from model import SUB,fn
D=r"D:\trainz12-decomp\notes\data"
funcs={}
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    funcs[r["entry"]]=dict(a=int(r["entry"],16),size=int(r["size"]),ns=r["namespace"],name=r["name"],thunk=r["thunk"]=="true")
calls=list(csv.DictReader(open(D+r"\calls.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
def sub_of_ns(ns):
    c=ns.replace("Jet::","",1) if ns.startswith("Jet::") else ns
    if c=="Jet": return SUB.get("Jet")
    return SUB.get(c)
FIRST=0x10001000; JPEG_LO,JPEG_HI=0x100432e0,0x1004d571; CRT_LO=0x1004d572
order=sorted(funcs.values(),key=lambda f:f["a"])
owner={}
for f in order:
    if f["ns"] not in("Global",) and not f["ns"].endswith(".DLL") and "::type_info" not in f["ns"]:
        s=sub_of_ns(f["ns"])
        if s: owner[f["a"]]=s
        elif f["ns"].startswith("std::"): owner[f["a"]]="99 STL/CRT glue"
        elif f["ns"] in("MSVCR80.DLL","KERNEL32.DLL","DBGHELP.DLL","VERSION.DLL"): owner[f["a"]]="99 STL/CRT glue"
# region labels
for f in order:
    if f["a"] in owner: continue
    if JPEG_LO<=f["a"]<JPEG_HI: owner[f["a"]]="15 Embedded JPEG encoder (IJG-style, SaveJPG only)"
    elif f["a"]>=CRT_LO: owner[f["a"]]="99 STL/CRT glue"
# callers propagation then adjacency
callers=collections.defaultdict(set)
for c in calls:
    if c["kind"].endswith("ext") or c["callee"].startswith("EXTERNAL") or not c["callee"]: continue
    callers[int(c["callee"],16)].add(int(c["caller"],16))
for it in range(6):
    ch=0
    for f in order:
        if f["a"] in owner: continue
        cs={owner.get(x) for x in callers.get(f["a"],[])}
        cs.discard(None)
        if len(cs)==1: owner[f["a"]]=next(iter(cs)); ch+=1
    # adjacency
    for i,f in enumerate(order):
        if f["a"] in owner: continue
        p=next((owner[order[j]["a"]] for j in range(i-1,-1,-1) if order[j]["a"] in owner),None)
        n=next((owner[order[j]["a"]] for j in range(i+1,len(order)) if order[j]["a"] in owner),None)
        if p and p==n: owner[f["a"]]=p; ch+=1
    if not ch: break
unk=[f for f in order if f["a"] not in owner]
print("unowned",len(unk),sum(f["size"] for f in unk))
json.dump({hex(k):v for k,v in owner.items()},open(D+r"\owners.json","w"))
agg=collections.defaultdict(lambda:[0,0])
for f in order:
    if f["thunk"]: continue
    o=owner.get(f["a"],"?? unowned"); agg[o][0]+=1; agg[o][1]+=f["size"]
for k,v in sorted(agg.items()): print(k,*v,sep="\t")
