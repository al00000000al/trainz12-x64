import csv,collections
D=r"D:\trainz12-decomp\notes\data"
calls=list(csv.DictReader(open(D+r"\calls.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
lo,hi=0x100432e0,0x1004d571
inreg=lambda a: lo<=int(a,16)<hi
ext=collections.defaultdict(set); outc=collections.defaultdict(collections.Counter)
for c in calls:
    if c["kind"].endswith("ext"):
        if inreg(c["caller"]): outc["IMPORT"][c["callee_ns"]+"."+c["callee_name"]]+=1
        continue
    if not c["callee"] or c["callee"].startswith("EXTERNAL"): continue
    ci=inreg(c["caller"]); ei=inreg(c["callee"])
    if not ci and ei: ext[c["callee"]].add(c["caller_ns"]+"::"+c["caller_name"])
    if ci and not ei: outc["out"][c["callee_ns"]+"::"+c["callee_name"]]+=1
print("entries called from outside region:")
for k,v in sorted(ext.items()): print(k,sorted(v)[:6])
print(); print("calls out from region:")
for k,v in outc["out"].most_common(40): print(k,v)
print(outc["IMPORT"].most_common(40))
