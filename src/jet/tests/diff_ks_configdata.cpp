// Differential test of KernelStandard unit U7b1 (ConfigData.cpp). Each scenario is drawn once, then run on a tree
// whose root is made by our KernelStandard_7c.dll and on one made by KernelStandard_7c_orig.dll (both in this
// process, called through raw exports; since U14b each side's nodes, strings and files come from its own module):
// children added by every
// New / operator() / Replace / copy, values set by every setter and operator<<, nodes removed, extracted, moved,
// copied over and swapped, then every Find, getter, conversion and operator>> read back. After every step the whole
// tree (names, types, value bits, chunk counts) and its Crc are logged per side; at the end the reference counts of
// the test's strings are logged against their starting values. The logs must match.
//
// Cases the original answers with garbage (a vector conversion of a node with no array, a Range read of a missing
// one) are not drawn.
//
// usage: diff_ks_configdata.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

static int side;
static const bool trace = getenv("KSC_TRACE") != nullptr;
static std::vector<std::string> logs[2];
static void Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    std::string buf(static_cast<size_t>(n) + 1, '\0');
    va_start(ap, fmt);
    std::vsnprintf(&buf[0], buf.size(), fmt, ap);
    va_end(ap);
    buf.resize(static_cast<size_t>(n));
    logs[side].push_back(buf);
}

static uint32_t Bits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
static float Float(uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
using Node = unsigned char;  // a PStringCache node: refs at +0x10, text at +0x18
static std::string NText(const Node* n) { return n ? std::string("'") + reinterpret_cast<const char*>(n + 0x18) + "'" : "(null)"; }
static std::string PText(const void* p) { return NText(*static_cast<const Node* const*>(p)); }
static std::string StrText(const void* s)  // a String: one pointer to {capacity, length, chars}
{
    const auto* b = *static_cast<const unsigned* const*>(s);
    if (!b) return "(nullptr)";
    if (!b[0]) return "(null)";
    return std::string("'") + reinterpret_cast<const char*>(b + 2) + "' len" + std::to_string(b[1]);
}

// ---- the exports
enum F {
    kCtor, kCtorName, kCtorNameValue, kCopyCtor, kDtor, kAssign, kAppend, kNew, kNewName, kNewNameValue,
    kCallC, kCall, kCallCV, kCallV, kCallNC, kIndexC, kIndex, kFindC, kFind, kFindVC, kFindV, kFindIC, kFindI,
    kFindNC, kFindN, kCount, kReplace, kReplaceV, kRemoveChild, kRemoveName, kRemoveNameValue, kRemoveAll, kExtract,
    kListC, kList, kAssignNodes, kChildCount, kHasChildren, kHasDataOnly, kArrayCount, kGetName, kSetName, kEqName,
    kDataType, kIntData, kFloatData, kPStringData, kSetIntData, kSetFloatData, kSetPStringData, kInnerB, kInnerI,
    kInnerF, kInnerP, kByteArray, kIntArray, kFloatArray, kPStringArray, kSetBytes, kSetInts, kSetFloats, kSetPStrings,
    kSetList, kAddBytes, kAddInts, kAddFloats, kAddPStrings, kSetAnsi, kSetUnicode, kGetAnsi, kGetUnicode, kCompare,
    kCopyData, kCrc, kCrcSkip, kCalcCrc, kDataToPString, kClearData,
    kToBool, kToPString, kToText, kToInt, kToUint, kToFloat, kToV2, kToV3, kToV4, kToQ, kToColor, kToPoint, kToSize,
    kToRect,
    kInPString, kInString, kInChar, kInUchar, kInBool, kInShort, kInUshort, kInInt, kInUint, kInLong, kInUlong, kInFloat,
    kInRangeF, kInV2, kInV3, kInColor, kInQ, kInV4, kInRangeI, kInPoint, kInSize, kInRect,
    kOutPString, kOutString, kOutBool, kOutInt, kOutLong, kOutChar, kOutUchar, kOutShort, kOutUshort, kOutUint,
    kOutUlong, kOutFloat, kOutRangeI, kOutRangeF, kOutV2, kOutV3, kOutV4, kOutQ, kOutColor, kOutPoint, kOutSize,
    kOutRect, kOutConfig,
    kWriteFile, kWriteStr, kWriteW, kWriteBinW, kWriteBinFile, kWriteBinStr, kWriteBinPath, kWriteBinBase,
    kReadBinFile, kReadBinPath, kReadBinBase, kFormat, kWriteChildList, kWriteBlock, kWriteWrap, kWriteDataItem,
    kWriteDataBlock, kWriteTabs, kWriteChars, kWritePString, kWriteBinFileP, kReadBinFileP,
    kCount_
};
#define FDA "?$FastDynamicArray@PAVConfigData@Jet@@$0N@U?$Less@PAVConfigData@Jet@@PAV12@@2@@2@"
static const char* const kNames[kCount_] = {
    "??0ConfigData@Jet@@QAE@XZ",
    "??0ConfigData@Jet@@QAE@ABVPString@1@@Z",
    "??0ConfigData@Jet@@QAE@ABVPString@1@0@Z",
    "??0ConfigData@Jet@@QAE@ABV01@@Z",
    "??1ConfigData@Jet@@QAE@XZ",
    "??4ConfigData@Jet@@QAEAAV01@ABV01@@Z",
    "??6ConfigData@Jet@@QAAAAV01@PAV01@@Z",
    "?New@ConfigData@Jet@@QAAAAV12@XZ",
    "?New@ConfigData@Jet@@QAAAAV12@ABVPString@2@@Z",
    "?New@ConfigData@Jet@@QAAAAV12@ABVPString@2@0@Z",
    "??RConfigData@Jet@@QBAABV01@ABVPString@1@@Z",
    "??RConfigData@Jet@@QAAAAV01@ABVPString@1@@Z",
    "??RConfigData@Jet@@QBAABV01@ABVPString@1@0@Z",
    "??RConfigData@Jet@@QAAAAV01@ABVPString@1@0@Z",
    "??RConfigData@Jet@@QBAABV01@ABVPString@1@H@Z",
    "??AConfigData@Jet@@QBAABV01@H@Z",
    "??AConfigData@Jet@@QAAAAV01@H@Z",
    "?Find@ConfigData@Jet@@QBAABV12@ABVPString@2@@Z",
    "?Find@ConfigData@Jet@@QAAAAV12@ABVPString@2@@Z",
    "?Find@ConfigData@Jet@@QBAABV12@ABVPString@2@0@Z",
    "?Find@ConfigData@Jet@@QAAAAV12@ABVPString@2@0@Z",
    "?Find@ConfigData@Jet@@QBAABV12@H@Z",
    "?Find@ConfigData@Jet@@QAAAAV12@H@Z",
    "?Find@ConfigData@Jet@@QBAABV12@ABVPString@2@H@Z",
    "?Find@ConfigData@Jet@@QAAAAV12@ABVPString@2@H@Z",
    "?Count@ConfigData@Jet@@QBAHABVPString@2@@Z",
    "?Replace@ConfigData@Jet@@QAAAAV12@ABVPString@2@H@Z",
    "?Replace@ConfigData@Jet@@QAAAAV12@ABVPString@2@0H@Z",
    "?Remove@ConfigData@Jet@@QAE_NABV12@H@Z",
    "?Remove@ConfigData@Jet@@QAE_NABVPString@2@H@Z",
    "?Remove@ConfigData@Jet@@QAE_NABVPString@2@0H@Z",
    "?RemoveAll@ConfigData@Jet@@QAEXABVPString@2@@Z",
    "?Extract@ConfigData@Jet@@QAAPAV12@ABVPString@2@H@Z",
    "?GetDataList@ConfigData@Jet@@QBAPBV" FDA "XZ",
    "?GetDataList@ConfigData@Jet@@QAAPAV" FDA "XZ",
    "?AssignNodes@ConfigData@Jet@@QAAXPAV" FDA "@Z",
    "?GetChildCount@ConfigData@Jet@@QBAHXZ",
    "?HasChildren@ConfigData@Jet@@QBE_NXZ",
    "?HasDataItemOnly@ConfigData@Jet@@IBA_NXZ",
    "?GetArrayCount@ConfigData@Jet@@QBAHXZ",
    "?GetName@ConfigData@Jet@@QBAABVPString@2@XZ",
    "?SetName@ConfigData@Jet@@QAAXABVPString@2@@Z",
    "??8ConfigData@Jet@@QBA_NABVPString@1@@Z",
    "?GetDataType@ConfigData@Jet@@QBAHXZ",
    "?GetIntData@ConfigData@Jet@@QBAJXZ",
    "?GetFloatData@ConfigData@Jet@@QBAMXZ",
    "?GetPStringData@ConfigData@Jet@@QBAABVPString@2@XZ",
    "?SetIntData@ConfigData@Jet@@IAAXJ@Z",
    "?SetFloatData@ConfigData@Jet@@IAAXM@Z",
    "?SetPStringData@ConfigData@Jet@@IAAXABVPString@2@@Z",
    "?GetInnerByteArr@ConfigData@Jet@@IBAPAUByteArray@12@XZ",
    "?GetInnerIntArr@ConfigData@Jet@@IBAPAUIntArray@12@XZ",
    "?GetInnerFloatArr@ConfigData@Jet@@IBAPAUFloatArray@12@XZ",
    "?GetInnerPStringArr@ConfigData@Jet@@IBAPAUPStringArray@12@XZ",
    "?GetByteArray@ConfigData@Jet@@QBAPBEXZ",
    "?GetIntArray@ConfigData@Jet@@QBAPBHXZ",
    "?GetFloatArray@ConfigData@Jet@@QBAPBMXZ",
    "?GetPStringArray@ConfigData@Jet@@QBAPBVPString@2@XZ",
    "?SetByteArray@ConfigData@Jet@@QAAXPAEH@Z",
    "?SetIntArray@ConfigData@Jet@@QAAXPAHH@Z",
    "?SetFloatArray@ConfigData@Jet@@QAAXPAMH@Z",
    "?SetPStringArray@ConfigData@Jet@@QAAXPAVPString@2@H@Z",
    "?SetPStringList@ConfigData@Jet@@QAAXABV?$DynamicArray@VPString@Jet@@$0N@U?$Less@VPString@Jet@@V12@@2@@2@@Z",
    "?AddByteArray@ConfigData@Jet@@QAAXPAEH@Z",
    "?AddIntArray@ConfigData@Jet@@QAAXPAHH@Z",
    "?AddFloatArray@ConfigData@Jet@@QAAXPAMH@Z",
    "?AddPStringArray@ConfigData@Jet@@QAAXPAVPString@2@H@Z",
    "?SetAnsi@ConfigData@Jet@@QAA_NABVAnsiString@2@@Z",
    "?SetUnicode@ConfigData@Jet@@QAA_NABVUnicodeString@2@@Z",
    "?GetAnsi@ConfigData@Jet@@QBA_NAAVAnsiString@2@@Z",
    "?GetUnicode@ConfigData@Jet@@QBA_NAAVUnicodeString@2@@Z",
    "?CompareData@ConfigData@Jet@@QBA_NABV12@@Z",
    "?CopyData@ConfigData@Jet@@QAAXABV12@@Z",
    "?Crc@ConfigData@Jet@@QBAKXZ",
    "?Crc@ConfigData@Jet@@QBAKABVPString@2@@Z",
    "?CalcCRC@ConfigData@Jet@@ABAKKABVPString@2@@Z",
    "?DataToPString@ConfigData@Jet@@IBA?AVPString@2@XZ",
    "?ClearData@ConfigData@Jet@@IAAXXZ",
    "??BConfigData@Jet@@QBE_NXZ",
    "??BConfigData@Jet@@QBEABVPString@1@XZ",
    "??BConfigData@Jet@@QBEPBDXZ",
    "??BConfigData@Jet@@QBEHXZ",
    "??BConfigData@Jet@@QBEIXZ",
    "??BConfigData@Jet@@QBEMXZ",
    "??BConfigData@Jet@@QBE?AVVector2@1@XZ",
    "??BConfigData@Jet@@QBE?AVVector3@1@XZ",
    "??BConfigData@Jet@@QBE?AVVector4@1@XZ",
    "??BConfigData@Jet@@QBE?AVQuaternion@1@XZ",
    "??BConfigData@Jet@@QBE?AVColor@1@XZ",
    "??BConfigData@Jet@@QBE?AUPoint@1@XZ",
    "??BConfigData@Jet@@QBE?AUSize@1@XZ",
    "??BConfigData@Jet@@QBE?AURect@1@XZ",
    "??6ConfigData@Jet@@QAEXABVPString@1@@Z",
    "??6ConfigData@Jet@@QAEXABVString@1@@Z",
    "??6ConfigData@Jet@@QAEXD@Z",
    "??6ConfigData@Jet@@QAEXE@Z",
    "??6ConfigData@Jet@@QAEX_N@Z",
    "??6ConfigData@Jet@@QAEXF@Z",
    "??6ConfigData@Jet@@QAEXG@Z",
    "??6ConfigData@Jet@@QAEXH@Z",
    "??6ConfigData@Jet@@QAEXI@Z",
    "??6ConfigData@Jet@@QAEXJ@Z",
    "??6ConfigData@Jet@@QAEXK@Z",
    "??6ConfigData@Jet@@QAEXM@Z",
    "??6ConfigData@Jet@@QAEXABU?$Range@M@1@@Z",
    "??6ConfigData@Jet@@QAEXABVVector2@1@@Z",
    "??6ConfigData@Jet@@QAEXABVVector3@1@@Z",
    "??6ConfigData@Jet@@QAEXABVColor@1@@Z",
    "??6ConfigData@Jet@@QAEXABVQuaternion@1@@Z",
    "??6ConfigData@Jet@@QAEXABVVector4@1@@Z",
    "??6ConfigData@Jet@@QAEXABU?$Range@H@1@@Z",
    "??6ConfigData@Jet@@QAEXABUPoint@1@@Z",
    "??6ConfigData@Jet@@QAEXABUSize@1@@Z",
    "??6ConfigData@Jet@@QAEXABURect@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVPString@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVString@1@@Z",
    "??5ConfigData@Jet@@QBEXAA_N@Z",
    "??5ConfigData@Jet@@QBEXAAH@Z",
    "??5ConfigData@Jet@@QBEXAAJ@Z",
    "??5ConfigData@Jet@@QBEXAAD@Z",
    "??5ConfigData@Jet@@QBEXAAE@Z",
    "??5ConfigData@Jet@@QBEXAAF@Z",
    "??5ConfigData@Jet@@QBEXAAG@Z",
    "??5ConfigData@Jet@@QBEXAAI@Z",
    "??5ConfigData@Jet@@QBEXAAK@Z",
    "??5ConfigData@Jet@@QBEXAAM@Z",
    "??5ConfigData@Jet@@QBEXAAU?$Range@H@1@@Z",
    "??5ConfigData@Jet@@QBEXAAU?$Range@M@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVVector2@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVVector3@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVVector4@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVQuaternion@1@@Z",
    "??5ConfigData@Jet@@QBEXAAVColor@1@@Z",
    "??5ConfigData@Jet@@QBEXAAUPoint@1@@Z",
    "??5ConfigData@Jet@@QBEXAAUSize@1@@Z",
    "??5ConfigData@Jet@@QBEXAAURect@1@@Z",
    "??5ConfigData@Jet@@QBEXAAV01@@Z",
    "?Write@ConfigData@Jet@@QBA_NAAVFile@2@H@Z",
    "?Write@ConfigData@Jet@@QBA_NAAVPString@2@H@Z",
    "?Write@ConfigData@Jet@@QBA_NAAVWriter@12@H@Z",
    "?WriteBin@ConfigData@Jet@@QBA_NAAVWriter@12@H@Z",
    "?WriteBin@ConfigData@Jet@@QBA_NAAVFile@2@H@Z",
    "?WriteBin@ConfigData@Jet@@QBA_NAAVPString@2@H@Z",
    "?WriteBin@ConfigData@Jet@@QBA_NPBD_NH@Z",
    "?WriteBin@ConfigData@Jet@@QBA_NABVPString@2@PBD_NH@Z",
    "?ReadBin@ConfigData@Jet@@QAA_NAAVFile@2@@Z",
    "?ReadBin@ConfigData@Jet@@QAA_NPBD@Z",
    "?ReadBin@ConfigData@Jet@@QAA_NABVPString@2@PBD@Z",
    "?FormatDataString@ConfigData@Jet@@IBAXAAVString@2@@Z",
    "?WriteChildList@ConfigData@Jet@@IBA_NAAVWriter@12@HH@Z",
    "?WriteBlock@ConfigData@Jet@@IBA_NAAVWriter@12@HH@Z",
    "?WriteWrap@ConfigData@Jet@@IBE_NAAVWriter@12@PBDHAAHHH@Z",
    "?WriteDataItem@ConfigData@Jet@@IBA_NAAVWriter@12@HH@Z",
    "?WriteDataBlock@ConfigData@Jet@@IBA_NAAVWriter@12@HH@Z",
    "?WriteTabs@ConfigData@Jet@@IBA_NAAVWriter@12@HH@Z",
    "?WriteChars@ConfigData@Jet@@IBE_NAAVWriter@12@PBDHH@Z",
    "?WritePString@ConfigData@Jet@@IBE_NAAVWriter@12@ABVPString@2@H@Z",
    "?WriteBinFile@ConfigData@Jet@@IBA_NAAVWriter@12@H@Z",
    "?ReadBinFile@ConfigData@Jet@@IAA_NAAVFile@2@@Z",
};
static void* fns[2][kCount_];

// thiscall through __fastcall (the edx slot unused), and the __cdecl members (this first)
template <class R, class... A>
static R Th(int f, const void* self, A... a)
{
    return reinterpret_cast<R(__fastcall*)(const void*, void*, A...)>(fns[side][f])(self, nullptr, a...);
}
template <class R, class... A>
static R Cd(int f, const void* self, A... a)
{
    return reinterpret_cast<R(__cdecl*)(const void*, A...)>(fns[side][f])(self, a...);
}

// the original's own helpers (the same for both sides)
static void* (__cdecl* cdNew)(size_t);
static void(__cdecl* cdDelete)(void*);
static const unsigned char* unknownItem;
static void** pstringCache;
static Node* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__fastcall* stringCtor)(void*, void*, const char*);
static void* (__fastcall* stringDefault)(void*, void*);
static void(__fastcall* stringDtor)(void*, void*);
static void* (__fastcall* unicodeCtor)(void*, void*, const wchar_t*);
static void* (__fastcall* unicodeDefault)(void*, void*);
static void(__fastcall* unicodeDtor)(void*, void*);
static void* (__cdecl* allocNew)(unsigned);
static void(__cdecl* allocDelete)(void*);

static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}

// ---- the tree as raw memory: {name node, type, data, children: Chunk* head}, chunks {next, prev, count, items[13]}
struct Chunk {
    Chunk* next;
    Chunk* prev;
    uint32_t count;
    unsigned char* items[13];
};
static int Type(const unsigned char* c) { return *reinterpret_cast<const int*>(c + 4); }
static void* Data(const unsigned char* c) { return *reinterpret_cast<void* const*>(c + 8); }
static Chunk* Head(const unsigned char* c) { return *reinterpret_cast<Chunk* const*>(c + 0xc); }
static std::vector<unsigned char*> Children(const unsigned char* c)
{
    std::vector<unsigned char*> v;
    if (Chunk* const h = Head(c)) {
        Chunk* k = h;
        do {
            for (uint32_t i = 0; i < k->count; ++i) v.push_back(k->items[i]);
            k = k->next;
        } while (k != h);
    }
    return v;
}
static void Preorder(unsigned char* c, std::vector<unsigned char*>& out)
{
    out.push_back(c);
    for (unsigned char* k : Children(c)) Preorder(k, out);
}
static bool Within(unsigned char* root, const unsigned char* c)  // c in root's subtree
{
    std::vector<unsigned char*> v;
    Preorder(root, v);
    for (auto* x : v)
        if (x == c) return true;
    return false;
}

static std::string Value(const unsigned char* c)
{
    const int t = Type(c);
    const void* d = Data(c);
    char buf[64];
    switch (t) {
    case 0: return d ? "none!" : "none";
    case 1: std::snprintf(buf, sizeof(buf), "int %d", static_cast<int>(reinterpret_cast<intptr_t>(d))); return buf;
    case 2: std::snprintf(buf, sizeof(buf), "float %08x", static_cast<unsigned>(reinterpret_cast<uintptr_t>(d))); return buf;
    case 3: return "str " + NText(static_cast<const Node*>(d));
    default: break;
    }
    if (!d) return "type " + std::to_string(t) + " (no block)";
    const int n = *static_cast<const int*>(d);
    std::string s = "type " + std::to_string(t) + " n" + std::to_string(n) + " [";
    for (int i = 0; i < n && i < 64; ++i) {
        if (t == 4) std::snprintf(buf, sizeof(buf), "%02x", static_cast<const unsigned char*>(d)[4 + i]);
        else if (t == 7) std::snprintf(buf, sizeof(buf), "%s ", NText(static_cast<Node* const*>(d)[1 + i]).c_str());
        else std::snprintf(buf, sizeof(buf), "%08x ", static_cast<const unsigned*>(d)[1 + i]);
        s += buf;
    }
    return s + "]";
}
static void DumpTree(const unsigned char* c, int depth, std::string& s)
{
    s += "{" + std::to_string(depth) + " " + PText(c) + " " + Value(c);
    if (Chunk* const h = Head(c)) {
        s += " chunks";
        Chunk* k = h;
        do {
            s += " " + std::to_string(k->count);
            k = k->next;
        } while (k != h);
    }
    for (unsigned char* k : Children(c)) DumpTree(k, depth + 1, s);
    s += "}";
}

// ---- the generator
static std::mt19937 rng(7171);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static const char* const kTexts[] = {"a", "B", "b", "name", "x", "", "12", "-7", "0x1F", "true", "TRUE", "TrUe", "1",
                                     "2.5", "1e10", "abc", "\xc9t\xc9", " 5", "truex", "4294967295", "-1", "tru",
                                     "3.99", "-2.5e-3", "0", "kuid", "a b", "q\"x", "t\tab", "back\\slash", "{x}",
                                     "semi;", "#c", "/p", "`", "line\nfeed", "x=y", "(p)", "c,d", "\r\b"};
static const int kTextCount = sizeof(kTexts) / sizeof(kTexts[0]);
static Node* pools[2][kTextCount + 1];  // per side, from its own PString cache; [0] the null string
static Node** pool = pools[0];
static int poolRefs[kTextCount + 1];
static uint32_t FloatBits()
{
    static const uint32_t special[] = {0, 0x3f800000, 0xc0200000, 0x3dcccccd, 0x501502f9, 0x47f12065, 0x33d6bf95,
                                       0x7fc00000, 0x7f800001, 0x80000000, 0x7f800000, 0xff800000, 0x00000001,
                                       0x4b000000, 0x42c80000, 0x3eaaaaab};
    return R(4) ? special[R(16)] : static_cast<uint32_t>(rng());
}
static uint32_t IntBits()
{
    static const uint32_t special[] = {0, 1, 2, 0xffffffff, 0x7fffffff, 0x80000000, 0x80, 0xff, 0x7fff, 0x8000, 0xffff, 100};
    return R(3) ? special[R(12)] : static_cast<uint32_t>(rng());
}

struct Op {
    int kind, sub;
    uint32_t x, y, z, w, n;
    std::vector<uint32_t> v;  // values (ints or float bits or pool indices or wide chars)
};

static unsigned char* NewNode(int f, const void* a = nullptr, const void* b = nullptr)
{
    void* const mem = cdNew(0x10);
    if (f == kCtor) Th<void*>(kCtor, mem);
    else if (f == kCtorName) Th<void*>(kCtorName, mem, a);
    else if (f == kCtorNameValue) Th<void*>(kCtorNameValue, mem, a, b);
    else Th<void*>(kCopyCtor, mem, a);
    return static_cast<unsigned char*>(mem);
}
static void DeleteNode(unsigned char* c)
{
    Th<void>(kDtor, c);
    cdDelete(c);
}

static std::string Where(const std::vector<unsigned char*>& nodes, const void* p)
{
    if (p == unknownItem) return "unknown";
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i] == p) return "#" + std::to_string(i);
    return p ? "elsewhere" : "null";
}
// a pointer the getters return, shown relative to the node
static std::string Rel(const unsigned char* c, const void* p)
{
    if (!p) return "null";
    if (p == c + 8) return "&data";
    if (Data(c) && p == static_cast<const unsigned char*>(Data(c)) + 4) return "data+4";
    if (p == Data(c)) return "data";
    return "other";
}

// how many items an array node holds (0 for the rest)
static int Items(const unsigned char* c)
{
    const int t = Type(c);
    return t >= 5 && t <= 7 && Data(c) ? *static_cast<const int*>(Data(c)) : 0;
}

static void Read(unsigned char* root, const unsigned char* c, const Op& op)
{
    std::vector<unsigned char*> nodes;
    Preorder(root, nodes);
    Node** const name = &pool[op.z % (kTextCount + 1)];
    Node** const value = &pool[op.w % (kTextCount + 1)];
    const int n = static_cast<int>(op.n % 5) - 1;
    const unsigned char* other = nodes[op.y % nodes.size()];
    std::string s;
    s += " callC " + Where(nodes, Cd<void*>(kCallC, c, name));
    s += " callCV " + Where(nodes, Cd<void*>(kCallCV, c, name, value));
    s += " callNC " + Where(nodes, Cd<void*>(kCallNC, c, name, n));
    s += " indexC " + Where(nodes, Cd<void*>(kIndexC, c, n));
    s += " findC " + Where(nodes, Cd<void*>(kFindC, c, name));
    s += " findVC " + Where(nodes, Cd<void*>(kFindVC, c, name, value));
    s += " findIC " + Where(nodes, Cd<void*>(kFindIC, c, n));
    s += " findNC " + Where(nodes, Cd<void*>(kFindNC, c, name, n));
    if (c != unknownItem) {
        s += " index " + Where(nodes, Cd<void*>(kIndex, c, n));
        s += " find " + Where(nodes, Cd<void*>(kFind, c, name));
        s += " findV " + Where(nodes, Cd<void*>(kFindV, c, name, value));
        s += " findI " + Where(nodes, Cd<void*>(kFindI, c, n));
        s += " findN " + Where(nodes, Cd<void*>(kFindN, c, name, n));
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), " count %d children %d has %d only %d array %d type %d int %ld float %08x",
                  Cd<int>(kCount, c, name), Cd<int>(kChildCount, c), Th<bool>(kHasChildren, c), Cd<bool>(kHasDataOnly, c),
                  Cd<int>(kArrayCount, c), Cd<int>(kDataType, c), Type(c) < 3 ? Cd<long>(kIntData, c) : 0L,
                  Type(c) < 3 ? Bits(Cd<float>(kFloatData, c)) : 0u);  // (an array's block address is not compared)
    s += buf;
    const bool isString = Type(c) == 3 || !Data(c);  // (the PString views read the data word as a node)
    s += " pstr " + (isString ? PText(Cd<void*>(kPStringData, c)) : Rel(c, Cd<void*>(kPStringData, c))) + " name " + PText(Cd<void*>(kGetName, c));
    s += " eq " + std::to_string(Cd<bool>(kEqName, c, name));
    s += " lists " + std::to_string(Cd<const unsigned char*>(kListC, c) == c + 0xc) +
         std::to_string(c == unknownItem || Cd<const unsigned char*>(kList, c) == c + 0xc);
    s += " inner " + Rel(c, Cd<void*>(kInnerB, c)) + Rel(c, Cd<void*>(kInnerI, c)) + Rel(c, Cd<void*>(kInnerF, c)) +
         Rel(c, Cd<void*>(kInnerP, c));
    s += " arrays " + Rel(c, Cd<void*>(kByteArray, c)) + " " + Rel(c, Cd<void*>(kIntArray, c)) + " " +
         Rel(c, Cd<void*>(kFloatArray, c)) + " " + Rel(c, Cd<void*>(kPStringArray, c));
    std::snprintf(buf, sizeof(buf), " bool %d text %s toInt %d toUint %u toFloat %08x", Th<bool>(kToBool, c),
                  Th<const char*>(kToText, c) ? "y" : "n", Th<int>(kToInt, c), Th<unsigned>(kToUint, c),
                  Bits(Th<float>(kToFloat, c)));
    s += buf;
    s += " toP " + (isString ? PText(Th<void*>(kToPString, c)) : Rel(c, Th<void*>(kToPString, c)));
    {
        Node* p = nullptr;
        Cd<void*>(kDataToPString, c, &p);
        s += " d2p " + NText(p);
        Release(p);
    }
    s += " cmp " + std::to_string(Cd<bool>(kCompare, c, other));
    std::snprintf(buf, sizeof(buf), " crc %08lx skip %08lx calc %08lx", Cd<unsigned long>(kCrc, c),
                  Cd<unsigned long>(kCrcSkip, c, name), Cd<unsigned long>(kCalcCrc, c, op.x, name));
    s += buf;
    {
        alignas(4) unsigned char str[4];
        stringDefault(str, nullptr);
        const bool ok = Cd<bool>(kGetAnsi, c, str);
        s += " ansi " + std::to_string(ok) + StrText(str);
        stringDtor(str, nullptr);
        alignas(4) unsigned char uni[12];
        unicodeCtor(uni, nullptr, L"old");
        const bool okU = Cd<bool>(kGetUnicode, c, uni);
        const wchar_t* const wt = *reinterpret_cast<wchar_t* const*>(uni);
        s += " uni " + std::to_string(okU) + " len" + std::to_string(reinterpret_cast<const unsigned*>(uni)[2]) + " [";
        if (wt)
            for (const wchar_t* q = wt; *q; ++q) {
                std::snprintf(buf, sizeof(buf), "%x ", *q);
                s += buf;
            }
        s += "]";
        unicodeDtor(uni, nullptr);
    }
    // operator>> into sentinels (unchanged for the missing-key node)
    {
        Node* p = nullptr;
        Th<void>(kOutPString, c, &p);
        s += " >p " + NText(p);
        Release(p);
        p = nullptr;
        alignas(4) unsigned char str[4];
        stringCtor(str, nullptr, "keep");
        Th<void>(kOutString, c, str);
        s += " >s " + StrText(str);
        stringDtor(str, nullptr);
        bool b = true;
        int i = 0x5a5a5a5a;
        long l = 0x5a5a5a5a;
        char ch = 0x5a;
        unsigned char uc = 0x5a;
        short sh = 0x5a5a;
        unsigned short us = 0x5a5a;
        unsigned u = 0x5a5a5a5a;
        unsigned long ul = 0x5a5a5a5a;
        float f = Float(0x5a5a5a5a);
        Th<void>(kOutBool, c, &b);
        Th<void>(kOutInt, c, &i);
        Th<void>(kOutLong, c, &l);
        Th<void>(kOutChar, c, &ch);
        Th<void>(kOutUchar, c, &uc);
        Th<void>(kOutShort, c, &sh);
        Th<void>(kOutUshort, c, &us);
        Th<void>(kOutUint, c, &u);
        Th<void>(kOutUlong, c, &ul);
        Th<void>(kOutFloat, c, &f);
        std::snprintf(buf, sizeof(buf), " >b %d >i %d >l %ld >c %d >uc %u >sh %d >us %u >u %u >ul %lu >f %08x", b, i, l, ch,
                      uc, sh, us, u, ul, Bits(f));
        s += buf;
    }
    // the vector forms, where the original reads a real array
    const int items = Items(c);
    auto words = [&](const void* p, int k) {
        std::string w;
        for (int j = 0; j < k; ++j) {
            std::snprintf(buf, sizeof(buf), "%08x ", static_cast<const unsigned*>(p)[j]);
            w += buf;
        }
        return w;
    };
    if (c != unknownItem && items >= 2 && Type(c) != 7) {
        uint32_t out[4];
        Th<void*>(kToV2, c, out);
        s += " v2 " + words(out, 2);
        Th<void*>(kToPoint, c, out);
        s += " pt " + words(out, 2);
        Th<void*>(kToSize, c, out);
        s += " sz " + words(out, 2);
        uint32_t o2[4] = {1, 2, 3, 4};
        Th<void>(kOutV2, c, o2);
        Th<void>(kOutPoint, c, o2 + 2);
        s += " >v2pt " + words(o2, 4);
        uint32_t r[2] = {9, 9};
        Th<void>(kOutRangeI, c, r);
        s += " >ri " + words(r, 2);
        Th<void>(kOutRangeF, c, r);
        s += " >rf " + words(r, 2);
        Th<void>(kOutSize, c, r);
        s += " >sz " + words(r, 2);
    }
    if (c != unknownItem && items >= 3 && Type(c) != 7) {
        uint32_t out[4] = {7, 7, 7, 7};
        Th<void*>(kToV3, c, out);
        s += " v3 " + words(out, 3);
        Th<void*>(kToColor, c, out);
        s += " col " + words(out, 4);
        uint32_t o[4] = {8, 8, 8, 8};
        Th<void>(kOutV3, c, o);
        s += " >v3 " + words(o, 4);
        Th<void>(kOutColor, c, o);
        s += " >col " + words(o, 4);
    }
    if (c != unknownItem && (items == 3 || items >= 4) && Type(c) != 7) {
        uint32_t out[4] = {6, 6, 6, 6};
        Th<void*>(kToQ, c, out);
        // (three items go through Quaternion::Set, unit U1, whose NaN payloads are not matched: see diff_ks_math2)
        auto nanless = [&](uint32_t* w) {
            if (items == 3)
                for (int j = 0; j < 4; ++j)
                    if ((w[j] & 0x7f800000u) == 0x7f800000u && (w[j] & 0x7fffffu)) w[j] = 0x7fffffffu;
        };
        nanless(out);
        s += " q " + words(out, 4);
        uint32_t o[4] = {5, 5, 5, 5};
        Th<void>(kOutQ, c, o);
        nanless(o);
        s += " >q " + words(o, 4);
    }
    if (c != unknownItem && items >= 4 && Type(c) != 7) {
        uint32_t out[4];
        Th<void*>(kToV4, c, out);
        s += " v4 " + words(out, 4);
        Th<void*>(kToRect, c, out);
        s += " rect " + words(out, 4);
        uint32_t o[4] = {4, 4, 4, 4};
        Th<void>(kOutV4, c, o);
        s += " >v4 " + words(o, 4);
        Th<void>(kOutRect, c, o);
        s += " >rect " + words(o, 4);
    }
    if (c == unknownItem) {  // nothing changes here either
        uint32_t o[4] = {3, 3, 3, 3};
        for (int f : {kOutRangeI, kOutRangeF, kOutV2, kOutV3, kOutV4, kOutQ, kOutColor, kOutPoint, kOutSize, kOutRect})
            Th<void>(f, c, o);
        s += " >all " + words(o, 4);
    }
    Log("read %s:%s", Where(nodes, c).c_str(), s.c_str());
}

// a DynamicArray<PString, 13> of the given pool strings (the test's own chunks: SetPStringList only reads them)
struct List {
    Chunk* head = nullptr;
    Chunk chunks[3];
};
static void MakeList(List& l, const std::vector<uint32_t>& v)
{
    l.head = nullptr;
    int used = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i % 13 == 0) {
            Chunk* const c = &l.chunks[used++];
            c->count = 0;
            if (!l.head) {
                l.head = c;
                c->next = c->prev = c;
            } else {
                Chunk* const last = l.head->prev;
                c->prev = last;
                c->next = l.head;
                last->next = c;
                l.head->prev = c;
            }
        }
        Chunk* const c = l.head->prev;
        c->items[c->count++] = reinterpret_cast<unsigned char*>(pool[v[i] % (kTextCount + 1)]);
    }
}

static void Set(unsigned char* c, const Op& op)
{
    const std::vector<uint32_t>& v = op.v;
    const uint32_t a = v.empty() ? 0 : v[0];
    float fl[4];
    for (int i = 0; i < 4; ++i) fl[i] = Float(i < static_cast<int>(v.size()) ? v[i] : 0);
    int in[4];
    for (int i = 0; i < 4; ++i) in[i] = static_cast<int>(i < static_cast<int>(v.size()) ? v[i] : 0);
    Node** const p = &pool[a % (kTextCount + 1)];
    switch (op.sub) {
    case 0: Th<void>(kInPString, c, p); break;
    case 1: {
        alignas(4) unsigned char str[4];
        if (a % 5 == 0) stringDefault(str, nullptr);
        else stringCtor(str, nullptr, kTexts[a % kTextCount]);
        Th<void>(kInString, c, str);
        stringDtor(str, nullptr);
        break;
    }
    case 2: Th<void>(kInChar, c, static_cast<char>(a)); break;
    case 3: Th<void>(kInUchar, c, static_cast<unsigned char>(a)); break;
    case 4: Th<void>(kInBool, c, (a & 1) != 0); break;
    case 5: Th<void>(kInShort, c, static_cast<short>(a)); break;
    case 6: Th<void>(kInUshort, c, static_cast<unsigned short>(a)); break;
    case 7: Th<void>(kInInt, c, static_cast<int>(a)); break;
    case 8: Th<void>(kInUint, c, static_cast<unsigned>(a)); break;
    case 9: Th<void>(kInLong, c, static_cast<long>(a)); break;
    case 10: Th<void>(kInUlong, c, static_cast<unsigned long>(a)); break;
    case 11: Th<void>(kInFloat, c, fl[0]); break;
    case 12: Th<void>(kInRangeF, c, fl); break;
    case 13: Th<void>(kInV2, c, fl); break;
    case 14: Th<void>(kInV3, c, fl); break;
    case 15: Th<void>(kInColor, c, fl); break;
    case 16: Th<void>(kInQ, c, fl); break;
    case 17: Th<void>(kInV4, c, fl); break;
    case 18: Th<void>(kInRangeI, c, in); break;
    case 19: Th<void>(kInPoint, c, in); break;
    case 20: Th<void>(kInSize, c, in); break;
    case 21: Th<void>(kInRect, c, in); break;
    case 22: Cd<void>(kSetIntData, c, static_cast<long>(a)); break;
    case 23: Cd<void>(kSetFloatData, c, fl[0]); break;
    case 24: Cd<void>(kSetPStringData, c, p); break;
    case 25:
    case 29: {
        unsigned char bytes[40];
        for (size_t i = 0; i < v.size() && i < 40; ++i) bytes[i] = static_cast<unsigned char>(v[i]);
        Cd<void>(op.sub == 25 ? kSetBytes : kAddBytes, c, bytes, static_cast<int>(v.size()));
        break;
    }
    case 26:
    case 30: {
        std::vector<int> x(v.begin(), v.end());
        x.push_back(0);
        Cd<void>(op.sub == 26 ? kSetInts : kAddInts, c, x.data(), static_cast<int>(v.size()));
        break;
    }
    case 27:
    case 31: {
        std::vector<float> x;
        for (uint32_t u : v) x.push_back(Float(u));
        x.push_back(0);
        Cd<void>(op.sub == 27 ? kSetFloats : kAddFloats, c, x.data(), static_cast<int>(v.size()));
        break;
    }
    case 28:
    case 32: {
        std::vector<Node*> x;
        for (uint32_t u : v) x.push_back(pool[u % (kTextCount + 1)]);
        x.push_back(nullptr);
        Cd<void>(op.sub == 28 ? kSetPStrings : kAddPStrings, c, x.data(), static_cast<int>(v.size()));
        break;
    }
    case 33: {
        List l;
        MakeList(l, v);
        Cd<void>(kSetList, c, &l);
        break;
    }
    case 34: {
        alignas(4) unsigned char str[4];
        if (a % 5 == 0) stringDefault(str, nullptr);
        else stringCtor(str, nullptr, kTexts[a % kTextCount]);
        Log("setAnsi %d", Cd<bool>(kSetAnsi, c, str));
        stringDtor(str, nullptr);
        break;
    }
    case 35: {
        alignas(4) unsigned char uni[12];
        if (a % 5 == 0) {
            unicodeDefault(uni, nullptr);
        } else {
            std::vector<wchar_t> w;
            for (size_t i = 1; i < v.size(); ++i) w.push_back(static_cast<wchar_t>(v[i]));
            w.push_back(0);
            unicodeCtor(uni, nullptr, w.data());
        }
        Log("setUnicode %d", Cd<bool>(kSetUnicode, c, uni));
        unicodeDtor(uni, nullptr);
        break;
    }
    case 36: Cd<void>(kSetName, c, p); break;
    case 37: Cd<void>(kClearData, c); break;
    default: break;
    }
}

// ---- U7b2: text and binary output, binary input
// a writer with ConfigData::Writer's slots (deleting destructor, Write, Encoding); it fails from call `failAt` on
struct TestWriter {
    virtual ~TestWriter() {}
    virtual unsigned long Write(const void* p, unsigned long size, unsigned long count)
    {
        ++calls;
        if (failAt >= 0 && calls > failAt) return 0;
        bytes.append(static_cast<const char*>(p), static_cast<size_t>(size) * count);
        return count;
    }
    virtual int Encoding() { return encoding; }
    std::string bytes;
    int calls = 0, failAt = -1, encoding = 0;
};

static std::string Hex(const std::string& b)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (unsigned char c : b) {
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}
static std::string FileBytes(const std::string& path)
{
    std::string b;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) b.append(buf, n);
        std::fclose(f);
    } else {
        b = "(no file)";
    }
    return b;
}
static void PutFile(const std::string& path, const std::string& b)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(b.data(), 1, b.size(), f);
    std::fclose(f);
}

static std::string tmpDir;  // with the trailing backslash
// each side its own current directory: since U10b part 2 the modules count temp names separately, and a directory
// substitute's compressed write leaves its hidden temp file behind (original bug), so one side's leftovers would
// collide with the other side's next names (the original then writes to fd -1 and msvcr80 ends the process)
static void SideCwd(int s) { SetCurrentDirectoryA((tmpDir + "c" + std::to_string(s)).c_str()); }
static void* (__fastcall* fileCtor)(void*, void*);
static void(__fastcall* fileDtor)(void*, void*);
static void* (__cdecl* fileOpen)(void*, int*, const char*, unsigned long, int);
static unsigned long(__cdecl* fileClose)(void*);

// every helper from the side's own module (its pool, cache, strings and files)
static HMODULE sideModule[2];
static void UseSide(int sd)
{
    const HMODULE m = sideModule[sd];
    pool = pools[sd];
    cdNew = Get<decltype(cdNew)>(m, "??2ConfigData@Jet@@SAPAXI@Z");
    cdDelete = Get<decltype(cdDelete)>(m, "??3ConfigData@Jet@@SAXPAX@Z");
    unknownItem = Get<const unsigned char*>(m, "?unknownItem@ConfigData@Jet@@2V12@A");
    pstringCache = Get<void**>(m, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(m, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(m, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    stringCtor = Get<decltype(stringCtor)>(m, "??0String@Jet@@QAE@PBD@Z");
    stringDefault = Get<decltype(stringDefault)>(m, "??0String@Jet@@QAE@XZ");
    stringDtor = Get<decltype(stringDtor)>(m, "??1String@Jet@@QAE@XZ");
    unicodeCtor = Get<decltype(unicodeCtor)>(m, "??0UnicodeString@Jet@@QAE@PB_W@Z");
    unicodeDefault = Get<decltype(unicodeDefault)>(m, "??0UnicodeString@Jet@@QAE@XZ");
    unicodeDtor = Get<decltype(unicodeDtor)>(m, "??1UnicodeString@Jet@@QAE@XZ");
    fileCtor = Get<decltype(fileCtor)>(m, "??0File@Jet@@QAE@XZ");
    fileDtor = Get<decltype(fileDtor)>(m, "??1File@Jet@@QAE@XZ");
    fileOpen = Get<decltype(fileOpen)>(
        m, "?Open@File@Jet@@QAA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBDKH@Z");
    fileClose = Get<decltype(fileClose)>(m, "?Close@File@Jet@@QAAKXZ");
}
static std::string TreeOf(const unsigned char* c)
{
    std::string t;
    DumpTree(c, 0, t);
    return t;
}

// a random text for FormatDataString / WriteChars
static std::string RandomText(const Op& op)
{
    static const char chars[] = "aZ09 #/,;=(){}`\\\"\t\r\n\b\x0b\x0c.-_x\xe9\xff";
    std::string t;
    const uint32_t n = op.n % 13;
    uint32_t r = op.w;
    for (uint32_t i = 0; i < n; ++i) {
        t += chars[r % (sizeof(chars) - 1)];
        r = r * 1103515245u + 12345u;
    }
    return t;
}

// The places a binary may be cut at: the starts of its fields. File::Read (the original's, in both modules) takes
// a partly read field as read, so a cut inside one leaves heap or stack bytes in the value (a count or string
// length then overruns the heap); a cut where a string's text starts makes File::Read(PString&) overrun it too.
static bool CutsString(const std::string& b, size_t& at, std::vector<size_t>& cuts)
{
    cuts.push_back(at);
    if (at + 4 > b.size()) return false;
    uint32_t h;
    std::memcpy(&h, b.data() + at, 4);
    at += 4 + (h & 0x3fffffffu);
    return at <= b.size();
}
static bool CutsNode(const std::string& b, size_t& at, std::vector<size_t>& cuts)
{
    if (!CutsString(b, at, cuts)) return false;
    cuts.push_back(at);
    if (at + 1 > b.size()) return false;
    const unsigned char t = static_cast<unsigned char>(b[at++]);
    int32_t n = 0;
    cuts.push_back(at);
    if (t >= 4) {
        if (at + 4 > b.size()) return false;
        std::memcpy(&n, b.data() + at, 4);
        at += 4;
        if (t != 7) cuts.push_back(at);
    }
    switch (t) {
    case 0: break;
    case 1:
    case 2: at += 4; break;
    case 3:
        cuts.pop_back();
        if (!CutsString(b, at, cuts)) return false;
        break;
    case 4: at += n; break;
    case 5:
    case 6: at += 4 * static_cast<size_t>(n); break;
    case 7:
        for (int32_t i = 0; i < n; ++i)
            if (!CutsString(b, at, cuts)) return false;
        break;
    default: return false;
    }
    cuts.push_back(at);
    if (at + 4 > b.size()) return false;
    int32_t children;
    std::memcpy(&children, b.data() + at, 4);
    at += 4;
    for (int32_t i = 0; i < children; ++i)
        if (!CutsNode(b, at, cuts)) return false;
    return true;
}
static std::vector<size_t> SafeCuts(const std::string& b)
{
    std::vector<size_t> cuts = {0, 2};
    size_t at = 4;
    if (b.size() >= 4) CutsNode(b, at, cuts);
    std::vector<size_t> inside;
    for (size_t c : cuts)
        if (c < b.size()) inside.push_back(c);
    return inside;
}

static void Output(unsigned char* root, unsigned char* c, const Op& op)
{
    // A PString array with a null item, written as UTF-16 text, makes both modules call mbstowcs(NULL, ..., -1),
    // which ends the process through the CRT's invalid-parameter handler: such trees are written as ANSI.
    std::vector<unsigned char*> sub;
    Preorder(c, sub);
    bool nullItem = false;
    for (unsigned char* x : sub)
        if (Type(x) == 7 && Data(x))
            for (int i = 0; i < *static_cast<const int*>(Data(x)); ++i)
                if (!static_cast<Node* const*>(Data(x))[1 + i]) nullItem = true;
    const int enc = nullItem ? 1 : static_cast<int>(op.w % 3);
    const std::string path = tmpDir + "diff_ksc.bin";
    std::string s = "out " + std::to_string(op.sub % 13) + " enc " + std::to_string(enc);
    switch (op.sub % 13) {
    case 0: {  // to a PString (the writer copies each piece up to its NUL)
        Node* p = nullptr;
        const bool ok = Cd<bool>(kWriteStr, c, &p, enc);
        s += " ok " + std::to_string(ok) + " " + (p ? Hex(reinterpret_cast<const char*>(p + 0x18)) : "(null)");
        Release(p);
        break;
    }
    case 1:
    case 3: {  // to a writer, which may fail part way
        TestWriter w;
        w.encoding = nullItem ? 1 : static_cast<int>(op.y % 3);
        w.failAt = op.n % 3 == 0 ? static_cast<int>(op.n % 40) : -1;
        const bool ok = Cd<bool>(op.sub % 13 == 1 ? kWriteW : kWriteBinW, c, &w, enc);
        s += " ok " + std::to_string(ok) + " calls " + std::to_string(w.calls) + " " + Hex(w.bytes);
        break;
    }
    case 2:
    case 6: {  // through a File (the original's)
        alignas(8) unsigned char f[0x140];
        fileCtor(f, nullptr);
        int r = 0;
        fileOpen(f, &r, path.c_str(), 0x101, 0);
        bool ok = false;
        if (r == 0) ok = Cd<bool>(op.sub % 13 == 2 ? kWriteFile : kWriteBinFile, c, f, enc);
        fileClose(f);
        fileDtor(f, nullptr);
        s += " open " + std::to_string(r) + " ok " + std::to_string(ok) + " " + Hex(FileBytes(path));
        if (op.sub % 13 == 6) {  // and back through ReadBin(File&)
            unsigned char* const n = NewNode(kCtor);
            fileCtor(f, nullptr);
            fileOpen(f, &r, path.c_str(), 0x10, 0);
            const bool back = r == 0 && Cd<bool>(kReadBinFile, n, f);
            fileClose(f);
            fileDtor(f, nullptr);
            s += " read " + std::to_string(back) + " " + TreeOf(n);
            DeleteNode(n);
        }
        break;
    }
    case 4: {
        Node* p = nullptr;
        // (the string writer copies up to a NUL: binary fields have none after them, so it reads on into the stack,
        // whose contents differ between builds; only the result is compared)
        const bool ok = Cd<bool>(kWriteBinStr, c, &p, enc);
        s += " ok " + std::to_string(ok);
        Release(p);
        break;
    }
    case 5:
    case 12: {  // to a file by name, then read back into a new node or into this one
        bool ok;
        Node* base = nullptr;
        if (op.sub % 13 == 5) {
            ok = Cd<bool>(kWriteBinPath, c, path.c_str(), (op.y & 1) != 0, enc);
        } else {
            base = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>("ksc"));  // the subst drive
            ok = Cd<bool>(kWriteBinBase, c, &base, "diff_ksc.bin", (op.y & 1) != 0, enc);
        }
        s += " ok " + std::to_string(ok) + " " + Hex(FileBytes(path));
        const bool into = op.x % 4 == 0 && c != root;
        unsigned char* const n = into ? c : NewNode(kCtor);
        const bool back = base ? Cd<bool>(kReadBinBase, n, &base, "diff_ksc.bin") : Cd<bool>(kReadBinPath, n, path.c_str());
        s += " read " + std::to_string(back) + " " + TreeOf(n);
        if (!into) DeleteNode(n);
        Release(base);
        break;
    }
    case 7: {  // a cut or damaged binary read back
        TestWriter w;
        Cd<bool>(kWriteBinW, c, &w, enc);
        std::string b = w.bytes;
        // (only cut: a changed count or length makes the original overrun the heap; see SafeCuts)
        const std::vector<size_t> cuts = SafeCuts(b);
        if (op.z % 5 == 0 && b.size() >= 4) b[op.z % 4] ^= static_cast<char>(1 + op.x % 255);  // the magic or version
        else if (!cuts.empty()) b.resize(cuts[op.n % cuts.size()]);
        if (trace) std::printf("cut %zu of %s\n", b.size(), Hex(w.bytes).c_str());
        PutFile(path, b);
        const bool into = op.x % 4 == 0 && c != root;
        unsigned char* const n = into ? c : NewNode(kCtor);
        const bool back = Cd<bool>(kReadBinPath, n, path.c_str());
        s += " read " + std::to_string(back) + " " + TreeOf(n);
        if (!into) DeleteNode(n);
        break;
    }
    case 8: {  // FormatDataString
        const std::string t = RandomText(op);
        alignas(4) unsigned char str[4];
        stringCtor(str, nullptr, t.c_str());
        Cd<void>(kFormat, c, str);
        s += " [" + Hex(t) + "] -> " + StrText(str);
        stringDtor(str, nullptr);
        break;
    }
    case 9: {  // the item / block writers at a depth
        TestWriter w;
        const int depth = static_cast<int>(op.n % 12);
        const int f = op.y % 4 == 0 ? kWriteChildList : op.y % 4 == 1 ? kWriteBlock : op.y % 4 == 2 ? kWriteDataItem : kWriteDataBlock;
        const bool ok = Cd<bool>(f, c, &w, depth, enc);
        s += " f " + std::to_string(op.y % 4) + " ok " + std::to_string(ok) + " " + Hex(w.bytes);
        if (op.y % 4 == 0) {
            TestWriter t;
            s += " tabs " + std::to_string(Cd<bool>(kWriteTabs, c, &t, depth, enc)) + " " + Hex(t.bytes);
        }
        break;
    }
    case 10: {  // WriteChars and WriteWrap
        const std::string t = RandomText(op);
        TestWriter w;
        const int length = t.empty() ? 0 : static_cast<int>(op.x % (t.size() + 1));
        s += " chars " + std::to_string(Th<bool>(kWriteChars, c, &w, t.c_str(), length, enc)) + " " + Hex(w.bytes);
        TestWriter v;
        int column = static_cast<int>(op.y % 160);
        const int indent = static_cast<int>(op.x % 90) - 5;
        const bool ok = Th<bool>(kWriteWrap, c, &v, t.c_str(), length, &column, indent, enc);
        s += " wrap " + std::to_string(ok) + " col " + std::to_string(column) + " " + Hex(v.bytes);
        break;
    }
    default: {  // WritePString and WriteBinFile
        TestWriter w;
        Node* const* p = &pool[op.z % (kTextCount + 1)];
        s += " pstr " + std::to_string(Th<bool>(kWritePString, c, &w, p, enc)) + " " + Hex(w.bytes);
        TestWriter v;
        s += " node " + std::to_string(Cd<bool>(kWriteBinFileP, c, &v, enc)) + " " + Hex(v.bytes);
        break;
    }
    }
    Log("%s", s.c_str());
}

static void Run(const std::vector<Op>& ops)
{
    // the strings' reference counts before
    for (int i = 1; i <= kTextCount; ++i) poolRefs[i] = *reinterpret_cast<int*>(pool[i] + 0x10);
    if (trace) std::printf("root...\n");
    unsigned char* const root = NewNode(kCtorName, &pool[4]);
    for (const Op& op : ops) {
        std::vector<unsigned char*> nodes;
        Preorder(root, nodes);
        unsigned char* const c = nodes[op.x % nodes.size()];
        unsigned char* const other = nodes[op.y % nodes.size()];
        Node** const name = &pool[op.z % (kTextCount + 1)];
        Node** const value = &pool[op.w % (kTextCount + 1)];
        const int n = static_cast<int>(op.n % 4);
        if (trace) std::printf("side %d op %d.%d on #%u of %zu\n", side, op.kind, op.sub, op.x % nodes.size(), nodes.size());
        switch (op.kind) {
        case 0: {  // a child
            void* r = nullptr;
            switch (op.sub % 9) {
            case 0: r = Cd<void*>(kNew, c); break;
            case 1: r = Cd<void*>(kNewName, c, name); break;
            case 2: r = Cd<void*>(kNewNameValue, c, name, value); break;
            case 3: r = Cd<void*>(kCall, c, name); break;
            case 4: r = Cd<void*>(kCallV, c, name, value); break;
            case 5: r = Cd<void*>(kReplace, c, name, n); break;
            case 6: r = Cd<void*>(kReplaceV, c, name, value, n); break;
            case 7: r = Cd<void*>(kAppend, c, NewNode(kCopyCtor, other)); break;
            default: r = Cd<void*>(kAppend, c, NewNode(op.w & 1 ? kCtorNameValue : kCtor, name, value)); break;
            }
            std::vector<unsigned char*> now;
            Preorder(root, now);
            Log("add %d -> %s", op.sub % 9, Where(now, r).c_str());
            break;
        }
        case 1:
            Set(c, op);
            break;
        case 2: {  // removal
            bool r = false;
            switch (op.sub % 6) {
            case 0: {
                const auto kids = Children(c);
                // (the match is compared before it is deleted, so any node may serve as the pattern)
                const unsigned char* const child = op.w % 7 == 0 ? unknownItem
                                                   : kids.empty() || op.w % 5 == 0 ? other
                                                                                   : kids[op.w % kids.size()];
                r = Th<bool>(kRemoveChild, c, child, n);
                break;
            }
            case 1: r = Th<bool>(kRemoveName, c, name, n); break;
            case 2: r = Th<bool>(kRemoveNameValue, c, name, value, n); break;
            case 3: Th<void>(kRemoveAll, c, name); break;
            default: {
                unsigned char* const x = Cd<unsigned char*>(kExtract, c, name, n);
                r = x != nullptr;
                if (x) {
                    if (op.sub % 6 == 4) {
                        DeleteNode(x);
                    } else {
                        std::vector<unsigned char*> now;
                        Preorder(root, now);
                        Cd<void*>(kAppend, now[op.y % now.size()], x);
                    }
                }
                break;
            }
            }
            Log("remove %d -> %d", op.sub % 6, r);
            break;
        }
        case 3: {  // copies between unrelated nodes
            const bool related = Within(c, other) || Within(other, c);
            switch (op.sub % 3) {
            case 0:
                if (!related || c == other) Th<void*>(kAssign, c, other);
                break;
            case 1: Cd<void>(kCopyData, c, other); break;
            default:
                if (!related) Cd<void>(kAssignNodes, c, other + 0xc);
                break;
            }
            break;
        }
        case 5:
            Output(root, c, op);
            break;
        default:
            Read(root, op.sub % 7 == 0 ? unknownItem : c, op);
            break;
        }
        std::string t;
        DumpTree(root, 0, t);
        Log("tree %s crc %08lx", t.c_str(), Cd<unsigned long>(kCrc, root));
    }
    DeleteNode(root);
    std::string refs;
    for (int i = 1; i <= kTextCount; ++i) {
        const int d = *reinterpret_cast<int*>(pool[i] + 0x10) - poolRefs[i];
        if (d) refs += " " + std::to_string(i) + ":" + std::to_string(d);
    }
    Log("refs%s", refs.c_str());
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    const void* at = e->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<const char*>(at), &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::printf("crash %08lx at %p (%s +%tx) data %p side %d\n", e->ExceptionRecord->ExceptionCode, at, name,
                static_cast<const char*>(at) - reinterpret_cast<const char*>(m),
                reinterpret_cast<void*>(e->ExceptionRecord->ExceptionInformation[1]), side);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_configdata <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 3000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KSC_SWAP") != nullptr;
    for (int i = 0; i < kCount_; ++i) {
        if (!kNames[i]) {
            std::printf("name table short at %d\n", i);
            return 2;
        }
        fns[0][i] = Get<void*>(swap ? orig : ours, kNames[i]);
        fns[1][i] = Get<void*>(swap ? ours : orig, kNames[i]);
    }
    sideModule[0] = swap ? orig : ours;
    sideModule[1] = swap ? ours : orig;
    UseSide(0);
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        CreateDirectoryA((tmpDir + "c0").c_str(), nullptr);
        CreateDirectoryA((tmpDir + "c1").c_str(), nullptr);
    }
    for (int sd = 0; sd < 2; ++sd) {
        UseSide(sd);
        if (!*pstringCache) {
            std::printf("no PString cache\n");
            return 2;
        }
        // "ksc" names the temp directory for Open(base, path); each module keeps its own substitute list (U10b)
        Node* drive = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>("ksc"));
        Get<void(__cdecl*)(Node* const*, const char*, bool)>(sideModule[sd], "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z")(
            &drive, tmpDir.c_str(), true);
        for (int i = 0; i < kTextCount; ++i)
            pool[i + 1] = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(kTexts[i]));
    }

    unsigned long long checks = 0, bad = 0;
    for (int sc = 0; sc < scenarios; ++sc) {
        std::vector<Op> ops(1 + R(60));
        for (Op& op : ops) {
            const uint32_t k = R(100);
            op.kind = k < 22 ? 0 : k < 48 ? 1 : k < 58 ? 2 : k < 65 ? 3 : k < 80 ? 5 : 4;
            op.sub = static_cast<int>(R(op.kind == 1 ? 38 : 1000));
            op.x = static_cast<uint32_t>(rng());
            op.y = static_cast<uint32_t>(rng());
            op.z = R(3) ? R(6) : static_cast<uint32_t>(rng());  // names: mostly a few
            op.w = static_cast<uint32_t>(rng());
            op.n = static_cast<uint32_t>(rng());
            const int count = static_cast<int>(R(R(4) ? 6 : 30));
            for (int i = 0; i < count; ++i) {
                const int s = op.sub;
                if (s == 11 || s == 12 || s == 13 || s == 14 || s == 15 || s == 16 || s == 17 || s == 23 || s == 27 || s == 31)
                    op.v.push_back(FloatBits());
                else if (s == 35)
                    op.v.push_back(R(4) ? 0x20 + R(0x5f) : R(2) ? 0xa0 + R(0x60) : 0x400 + R(0x100));
                else
                    op.v.push_back(R(2) ? IntBits() : R(kTextCount + 1));
            }
            if (op.kind == 1 && op.sub >= 12 && op.sub <= 21)
                while (op.v.size() < 4) op.v.push_back(op.sub <= 17 ? FloatBits() : IntBits());
        }
        for (side = 0; side < 2; ++side) {
            UseSide(side);
            SideCwd(side);
            Run(ops);
        }
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 10) std::printf("scenario %d line %zu:\n  ours %s\n  orig %s\n", sc, i, x.c_str(), y.c_str());
                break;
            }
        }
        if (getenv("KSC_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", sc, l.c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
