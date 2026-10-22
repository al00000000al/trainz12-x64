import csv,collections,re
D=r"D:\trainz12-decomp\notes\data"
refs=list(csv.DictReader(open(D+r"\datarefs.tsv",encoding="utf-8"),delimiter="\t",quoting=csv.QUOTE_NONE))
blk=collections.Counter(r["block"] for r in refs); print(blk)
# data addresses written from code (.data block), per address: set of (ns)
wr=collections.defaultdict(set); rd=collections.defaultdict(set)
for r in refs:
    if r["block"]!=".data": continue
    t=r["type"]
    (wr if "WRITE" in t else rd)[r["data_addr"]].add(r["func_ns"])
print("data addrs touched",len(set(wr)|set(rd)))
# globals shared across >1 namespace
multi=[(a,wr[a],rd[a]) for a in set(wr)|set(rd) if len(wr[a]|rd[a])>1]
print("shared",len(multi))
import json
SP="C:/Users/Fedor/AppData/Local/Temp/claude/D--trainz12-decomp/909bbaac-fa0d-4dac-b844-25dfec91f8ce/scratchpad"
exp=json.load(open(SP+"/res.json"))
expname={0x10000000+rva:d for c,m,d,rva,size,th in exp if th=="?"}
rows=[]
for a in sorted(set(wr)|set(rd)):
    ai=int(a,16)
    ns=sorted(wr[a]|rd[a])
    name=expname.get(ai,"")
    name=re.sub(r"^(public|private|protected): (static )?","",name)
    rows.append((ai,a,sorted(wr[a]),sorted(rd[a]-wr[a]),name))
# merge consecutive anonymous addresses? just print
for ai,a,w,r,name in rows:
    print(a,"W:",",".join(x.replace("Jet::","") for x in w),"| R:",",".join(x.replace("Jet::","") for x in r),"|",name[:90])
