import csv
fs=[]
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    fs.append((int(r["entry"],16),int(r["size"]),r["namespace"],r["name"],r["thunk"]))
fs.sort()
runs=[]
for a,s,ns,n,t in fs:
    if ns=="Global" or ns.startswith("MSVCR80") or ns in("KERNEL32.DLL",) : key="_"
    else: key=ns
    if runs and runs[-1][0]==key:
        runs[-1][2]=a+s; runs[-1][3]+=s; runs[-1][4]+=1
    else: runs.append([key,a,a+s,s,1])
import sys
for k,a,e,sz,n in runs: print(f"{a:08x}-{e:08x} {e-a:6d} {n:4d} {k}")
