import sys,collections,csv,json
sys.path.insert(0,r"D:\trainz12-decomp\notes\data")
from model import fn
nsz=collections.Counter(); nfn=collections.Counter()
for r in csv.DictReader(open(r"D:\trainz12-decomp\out\KernelStandard_7c.dll.functions.tsv",encoding="utf-8"),delimiter="\t"):
    ns=r["namespace"]; c=ns[5:] if ns.startswith("Jet::") else ns
    if r["thunk"]=="true": continue
    nsz[c]+=int(r["size"]); nfn[c]+=1
U=collections.OrderedDict([
("U1 Math-1 (Crc, StdMath, Vector2/3, int-log helpers, Eigen3, OptimalRects)",["Crc","StdMath","Vector2","Vector3","Jet","Eigen3","OptimalRects"]),
("U2 Math-2 (Matrix3, Matrix4, Quaternion)",["Matrix3","Matrix4","Quaternion"]),
("U3 Colour/image ops + small PODs (Color, HSVColor, PackedColor, ImageProcessing, Date, BuildVersion, list/tree nodes, ProtectedLong, DynamicPtr, MousePointer)",["Color","HSVColor","PackedColor","ImageProcessing","Date","BuildVersion","DLListNode","DLTreeNode","IListNode","ISingleListNode","RBTreeNode","ProtectedLong","DynamicPtr","MousePointer"]),
("U4 Sync primitives (SyncObject, Mutex, Semaphore, WEvent, CriticalSection, SingleLock, MultiLock, SWMRGuard)",["SyncObject","Mutex","Semaphore","WEvent","CriticalSection","SingleLock","MultiLock","SWMRGuard"]),
("U5 Rand, LCRand",["Rand","LCRand"]),
("U6 Small platform (DynamicLibrary, Events, SharedMemory, SharedScratchPad, Job, Jobs)",["DynamicLibrary","Events","Events::Event","SharedMemory","SharedScratchPad","Job","Jobs"]),
("U7 Config (ConfigData, ConfigParser(+flex core), Options, StringTables)",["ConfigData","ConfigParser","ConfigParserCore","scfgFlexLexer","FlexLexer","Options","StringTablesBase","StringTablesDefaultTraits"]),
("U8 Bitmap + embedded JPEG",["Bitmap"]),
("U9 JIRF + stream helpers (JIRF*, BlockReader, BPE, DynamicBuffer, IOSlave*)",["JIRFFile","JIRFWriteBlock","JIRFWriteHeader","JIRFWriteSize","BlockReader","BPE","DynamicBuffer","IOSlave","IOSlaveFile","IOSlaveMemory","IOSlaveMemoryDynamic","IOSlaveReadCursor","IOSlaveWriteCursor","IOSlaveResource"]),
("U10 File family (File, Path, Find, Entry, Time, FileEx, MemoryFile, FileSub*)",["File","File::Path","File::Find","File::Entry","File::Time","FileEx","MemoryFile","FileSub","FileSubArchive","FileSubPath","FileSubLink"]),
("U11 Platform core (OS, Window, WindowPlacement)",["OS","Window","WindowPlacement"]),
("U12 Diagnostics (Exception*, Log*, Console, Profile, QuickTimer, Debugger, Init)",["Exception","HardwareException","ExceptionHandler","Debugger","Log","LogClient","Console","Profile","QuickTimer","KernelStandardInit","AssertEx"]),
("U13 Threads/time (Thread, ThreadLocalStorage, Time)",["Thread","ThreadLocalStorage","Time"]),
("U14 Foundation (Alloc, PackedHeap*, PoolBlockAlloc, ReferenceCount, PString, PStringCache, strings, CRT glue)",["Alloc","PackedHeap","PackedHeapMP","PoolBlockAlloc","ReferenceCount","g_jetSTLAllocHeap","PString","PStringCache","AnsiString","UnicodeString","MultiByteString","String"]),
])
seen=set()
print("unit|exports|imported exports|class-attributed code bytes|code funcs")
tot=[0,0,0,0]
for u,cls in U.items():
    ex=[f for f in fn if f["cls"] in cls]
    imp=[f for f in ex if f["mods"]]
    b=sum(nsz[c] for c in cls); n=sum(nfn[c] for c in cls)
    for c in cls: seen.add(c)
    print(f"{u}|{len(ex)}|{len(imp)}|{b}|{n}")
    tot[0]+=len(ex);tot[1]+=len(imp);tot[2]+=b;tot[3]+=n
print("TOTAL",tot)
print("unassigned classes:",sorted({f['cls'] for f in fn}-seen))
