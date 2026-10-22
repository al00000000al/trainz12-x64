# builds the per-export model: class, subsystem, size, importers
import json,re,csv,collections
SP="C:/Users/Fedor/AppData/Local/Temp/claude/D--trainz12-decomp/909bbaac-fa0d-4dac-b844-25dfec91f8ce/scratchpad"
exp=json.load(open(SP+"/exp.json"))
imps=json.load(open(SP+"/imports.json"))
SUB={}
def S(name,*classes):
    for c in classes: SUB[c]=name
S("01 Memory (Alloc, PackedHeap, PoolBlockAlloc)","Alloc","PackedHeap","PackedHeapMP","PoolBlockAlloc","g_jetSTLAllocHeap","DynamicPtr","ReferenceCount")
S("02 String core (PString/PStringCache)","PString","PStringCache")
S("03 AnsiString/UnicodeString/MultiByteString/String","String","AnsiString","UnicodeString","MultiByteString")
S("04 StringTables (localisation tables)","StringTablesBase","StringTablesDefaultTraits")
S("05 Threads and sync","Thread","ThreadLocalStorage","CriticalSection","Mutex","Semaphore","WEvent","SyncObject","MultiLock","SingleLock","SWMRGuard","ProtectedLong","Job","Jobs")
S("06 Time, Rand, Crc, Date","Time","QuickTimer","Rand","LCRand","Crc","Date")
S("07 Math (Vector/Matrix/Quaternion/Eigen3/StdMath)","Matrix3","Matrix4","Quaternion","Vector2","Vector3","Eigen3","StdMath","OptimalRects","Jet")
S("08 Intrusive lists/trees","IListNode","ISingleListNode","DLListNode","DLTreeNode","RBTreeNode")
S("09 File and IOSlave streams","File","File::Path","File::Find","File::Entry","File::Time","FileEx","FileSubArchive","FileSubPath","FileSubLink","FileSub","IOSlave","IOSlaveMemory","IOSlaveMemoryDynamic","IOSlaveFile","IOSlaveReadCursor","IOSlaveWriteCursor","IOSlaveResource","MemoryFile","DynamicBuffer","BlockReader","BinReader","BinWriter","BPE","SharedMemory")
S("10 JIRF chunk files","JIRFFile","JIRFWriteBlock","JIRFWriteHeader","JIRFWriteSize")
S("11 ConfigData/ConfigParser/Options","ConfigData","ConfigParser","ConfigParserCore","Options","scfgFlexLexer","FlexLexer")
S("12 Diagnostics (Log, Console, Profile, Exceptions)","Log","LogClient","Console","Profile","ExceptionHandler","HardwareException","Exception","Debugger","AssertEx","BuildVersion","KernelStandardInit")
S("13 Platform (OS, Window, DynamicLibrary, Events)","OS","Window","WindowPlacement","DynamicLibrary","Events","Events::Event","MousePointer","SharedScratchPad")
S("14 Bitmap/Color/ImageProcessing (+JPEG)","Bitmap","Color","HSVColor","PackedColor","ImageProcessing")
def cls(m,d):
    s=re.sub(r"^(public|private|protected): (static |virtual )?","",d)
    if m.startswith("?g_jetSTL"): return "g_jetSTLAllocHeap"
    if "Jet::File::callback" in s: return "File"
    if "Bitmap::GetBlitter" in s: return "Bitmap"
    if "Profile::sort_func" in s: return "Profile"
    if "default constructor closure" in s or "::operator" in s:
        pre=s.split("::operator")[0] if "::operator" in s else s.split("::`default")[0]
        pre=pre.split(" ")[-1]
        c=pre.replace("Jet::","",1)
        return c
    # other: general
    pre=s.split("(")[0]
    # strip templates
    depth=0;buf=""
    for ch in pre:
        if ch=="<": depth+=1
        elif ch==">": depth-=1
        elif depth==0: buf+=ch
    q=buf.split(" ")[-1]
    parts=q.split("::")
    if len(parts)==1: return "Jet(free)"
    c="::".join(parts[:-1]).replace("Jet::","",1)
    if c=="Jet": return "Jet"
    return c
fn=[]
for m,d,rva,size,th in exp:
    c=cls(m,d)
    if c.startswith("Jet::"): c=c[5:]
    fn.append(dict(m=m,d=d,rva=rva,size=size,th=th,cls=c,sub=SUB.get(c,"??"),kind="data" if th=="?" else "code"))
mods=collections.defaultdict(set)
for mod,names in imps.items():
    for n in names: mods[n].add(mod)
for f in fn: f["mods"]=sorted(mods.get(f["m"],[]))
if __name__=="__main__":
    print(collections.Counter(f["sub"] for f in fn))
    print([ (f["cls"],f["d"][:70]) for f in fn if f["sub"]=="??"][:60])
