import csv,collections,json,sys
D=r"D:\trainz12-decomp\notes\data"
owner={int(k,16):v for k,v in json.load(open(D+r"\owners.json")).items()}
calls=list(csv.DictReader(open(D+r"\calls.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
want=sys.argv[1:]
seen=set()
for c in calls:
    if c["callee"].startswith("EXTERNAL") or not c["callee"]: continue
    a=owner.get(int(c["caller"],16),"??"); b=owner.get(int(c["callee"],16),"??")
    if a[:2]+">"+b[:2] in want:
        k=(c["caller_ns"],c["caller_name"],c["callee_ns"],c["callee_name"])
        if k in seen: continue
        seen.add(k); print(a[:2],b[:2],f'{c["caller_ns"]}::{c["caller_name"]}@{c["caller"]} -> {c["callee_ns"]}::{c["callee_name"]}@{c["callee"]}')
