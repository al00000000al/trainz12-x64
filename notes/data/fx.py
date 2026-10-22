# usage: fx.py <substring in header line> ... prints decompiled function blocks whose header matches
import sys,re
txt=open(r"D:\trainz12-decomp\decomp\KernelStandard_7c.dll.c",encoding="utf-8",errors="replace").read()
blocks=re.split(r"\n(?=// ==== )",txt)
pats=sys.argv[1:]
n=0
for b in blocks:
    h=b.split("\n",1)[0]
    if any(p in h for p in pats):
        print(b.rstrip()); print(); n+=1
print("// blocks:",n,file=sys.stderr)
