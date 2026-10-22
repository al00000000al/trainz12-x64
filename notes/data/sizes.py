import re,collections,sys
txt=open(r"D:\trainz12-decomp\decomp\KernelStandard_7c.dll.c",encoding="utf-8",errors="replace").read()
blocks=re.split(r"\n(?=// ==== )",txt)
mx=collections.defaultdict(int); alloc=collections.defaultdict(set)
for b in blocks:
    h=b.split("\n",1)[0]
    m=re.match(r"// ==== \w+\s+Jet::([\w:]+?)::([~\w`' ]+|operator\S*)\s+\[",h)
    if not m: continue
    c=m.group(1)
    for mm in re.finditer(r"\*\((\w+) ?\*+\)\(this \+ (0x[0-9a-f]+|\d+)\)",b):
        t=mm.group(1); off=int(mm.group(2),0)
        sz={"undefined4":4,"int":4,"uint":4,"float":4,"ulong":4,"undefined1":1,"char":1,"byte":1,"bool":1,"undefined2":2,"short":2,"ushort":2,"double":8,"undefined8":8,"longlong":8,"ulonglong":8}.get(t,4)
        mx[c]=max(mx[c],off+sz)
    for mm in re.finditer(r"this \+ (0x[0-9a-f]+)",b):
        mx[c]=max(mx[c],int(mm.group(1),0)+1)
for c,v in sorted(mx.items()): print(c,hex(v))
