import csv,collections
calls=list(csv.DictReader(open(r"D:\trainz12-decomp\notes\data\calls.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
funcs={}
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    funcs[r["entry"]]=r
callers=collections.defaultdict(set); callees=collections.defaultdict(set)
for c in calls:
    callers[c["callee"]].add(c["caller"]); callees[c["caller"]].add(c["callee"])
glob=[(int(f["size"]),e,f) for e,f in funcs.items() if f["namespace"]=="Global" and f["thunk"]=="false"]
glob.sort(reverse=True)
for s,e,f in glob[:60]:
    cs=sorted({funcs[x]["namespace"] if x in funcs else "?" for x in callers.get("0x"+e,callers.get(e,[]))})
    print(e,s,f["name"],cs[:6], len(callers.get(e,[])))
