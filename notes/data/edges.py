import csv,collections,json,sys
D=r"D:\trainz12-decomp\notes\data"
sys.path.insert(0,D)
owner={int(k,16):v for k,v in json.load(open(D+r"\owners.json")).items()}
calls=list(csv.DictReader(open(D+r"\calls.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
E=collections.defaultdict(collections.Counter)
EX=collections.defaultdict(collections.Counter)
for c in calls:
    if c["callee"].startswith("EXTERNAL") or not c["callee"]: 
        if c["kind"].endswith("ext"): EX[owner.get(int(c["caller"],16),"??")][c["callee_ns"]+"."+c["callee_name"]]+=1
        continue
    a=owner.get(int(c["caller"],16),"??"); b=owner.get(int(c["callee"],16),"??")
    if a!=b: E[a][b]+=1
for a in sorted(E):
    print(a.split(" ")[0], a)
    for b,n in E[a].most_common(): print("    ->",b[:60],n)
json.dump({a:dict(b) for a,b in E.items()},open(D+r"\edges.json","w"))
json.dump({a:dict(b) for a,b in EX.items()},open(D+r"\ext_imports.json","w"))
