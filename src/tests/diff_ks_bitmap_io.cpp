// Differential test of KernelStandard unit U8, file formats (BitmapIO.cpp). Both modules load the same files:
//  - a sample of the install's .tga / .bmp / .jpg files (every one with KBI_ALL): GetInfo, and Load with and without
//    keepFormat;
//  - synthetic files written once per scenario: TGA (colour-mapped 8-bit, 16/24/32-bit, either row order, an image ID,
//    a right-to-left descriptor, wrong map sizes), BMP (8-bit with any colour count, 24/32-bit, either row order,
//    widths not a multiple of 4, other bit counts), PCX (8-bit RLE with the end palette), JPEG (written by the
//    original's SaveJPG), files cut short (headers compared only: the original leaves rows unwritten) and garbage;
//  - LoadAlpha / AddAlpha with an 8-bit BMP of the same size and of another size;
//  - Save to .tga / .bmp / .jpg (each side writes its own file; the bytes are compared) and loading them back.
// Each load logs the result, the header fields, the palette (INDEX) and the pixel rows.
//
// usage: diff_ks_bitmap_io.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
//        [corpus root] [corpus files per type]
// KBI_SWAP: the sides swapped; KBI_DUMP: every log line; KBI_ALL: the whole corpus.
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

#include <algorithm>
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
static std::vector<std::string> logs[2];
static void Log(const std::string& s) { logs[side].push_back(s); }
static std::string Num(long long v) { return std::to_string(v); }
static std::string Hex(const void* p, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<const unsigned char*>(p)[i];
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}

using Bm = unsigned char;
static int W(const Bm* b) { return *reinterpret_cast<const int*>(b); }
static int H(const Bm* b) { return *reinterpret_cast<const int*>(b + 4); }
static const unsigned char* Pal(const Bm* b) { return *reinterpret_cast<unsigned char* const*>(b + 8); }
static unsigned char* Data(const Bm* b) { return *reinterpret_cast<unsigned char* const*>(b + 0xc); }
static int Pitch(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x10); }
static int Bpp(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x14); }
static int Fmt(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x18); }
static unsigned NumBytes(const Bm* b) { return *reinterpret_cast<const unsigned*>(b + 0x40); }

struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void(__fastcall* dtor)(void*, void*);
    bool(__cdecl* load)(void*, void* const*, void* const*, bool);
    bool(__cdecl* loadAlpha)(void*, const char*, const char*);
    bool(__cdecl* addAlpha)(void*, const char*);
    bool(__cdecl* save)(const void*, const char*);
    bool(__cdecl* info)(const char*, int*, unsigned long*, unsigned long*);
};
static Api apis[2];
static void** pstringCache;
static void*(__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, void*);

static void Load(Api& a, HMODULE m)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0Bitmap@Jet@@QAE@XZ");
    a.dtor = Get<decltype(a.dtor)>(m, "??1Bitmap@Jet@@QAE@XZ");
    a.load = Get<decltype(a.load)>(m, "?Load@Bitmap@Jet@@QAA_NABVPString@2@0_N@Z");
    a.loadAlpha = Get<decltype(a.loadAlpha)>(m, "?LoadAlpha@Bitmap@Jet@@QAA_NPBD0@Z");
    a.addAlpha = Get<decltype(a.addAlpha)>(m, "?AddAlpha@Bitmap@Jet@@QAA_NPBD@Z");
    a.save = Get<decltype(a.save)>(m, "?Save@Bitmap@Jet@@QBA_NPBD@Z");
    a.info = Get<decltype(a.info)>(m, "?GetInfo@Bitmap@Jet@@SA_NPBDAAW4Format@12@AAK2@Z");
}

static std::string tmpDir;
static std::mt19937 rng;
static int R(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }

static void WriteFile(const std::string& path, const std::string& bytes)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}
static std::string ReadFile(const std::string& path)
{
    std::string s;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return s;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}

template <class T>
static void Put(std::string& s, T v)
{
    s.append(reinterpret_cast<const char*>(&v), sizeof(v));
}
static std::string Bytes(size_t n)
{
    std::string s(n, '\0');
    for (char& c : s) c = static_cast<char>(R(0, 3) ? R(0, 255) : (R(0, 1) ? 0 : 255));
    return s;
}

// ---- synthetic files
static std::string Tga(int type, int mapType, int mapBits, int bits, int w, int h, int desc, int idLen)
{
    std::string s;
    Put<uint8_t>(s, uint8_t(idLen));
    Put<uint8_t>(s, uint8_t(mapType));
    Put<uint8_t>(s, uint8_t(type));
    Put<uint16_t>(s, 0);
    Put<uint16_t>(s, uint16_t(mapType ? 256 : 0));
    Put<uint8_t>(s, uint8_t(mapBits));
    Put<uint16_t>(s, 0);
    Put<uint16_t>(s, 0);
    Put<uint16_t>(s, uint16_t(w));
    Put<uint16_t>(s, uint16_t(h));
    Put<uint8_t>(s, uint8_t(bits));
    Put<uint8_t>(s, uint8_t(desc));
    s += Bytes(size_t(idLen));
    if (type == 1) s += Bytes(0x300);
    s += Bytes(size_t(w) * h * ((bits + 7) / 8));
    return s;
}
static std::string Bmp(int bits, int w, int h, int clrUsed, int planes = 1, int compression = 0)
{
    const int bpp = bits / 8;
    const int colours = bits == 8 ? (clrUsed && clrUsed <= 256 ? clrUsed : 256) : 0;
    std::string body = Bytes(size_t(colours) * 4) + Bytes(size_t(w) * (h < 0 ? -h : h) * bpp);
    std::string s;
    Put<uint16_t>(s, 0x4d42);
    Put<uint32_t>(s, uint32_t(0x36 + body.size()));
    Put<uint32_t>(s, 0);
    Put<uint32_t>(s, uint32_t(0x36 + colours * 4));
    Put<uint32_t>(s, 0x28);
    Put<int32_t>(s, w);
    Put<int32_t>(s, h);
    Put<uint16_t>(s, uint16_t(planes));
    Put<uint16_t>(s, uint16_t(bits));
    Put<uint32_t>(s, uint32_t(compression));
    Put<uint32_t>(s, 0);
    Put<int32_t>(s, 0);
    Put<int32_t>(s, 0);
    Put<uint32_t>(s, uint32_t(clrUsed));
    Put<uint32_t>(s, 0);
    return s + body;
}
static std::string Pcx(int w, int h, bool goodMarker)
{
    std::string s;
    Put<uint8_t>(s, 10);
    Put<uint8_t>(s, 5);
    Put<uint8_t>(s, 1);
    Put<uint8_t>(s, 8);
    Put<int16_t>(s, 0);
    Put<int16_t>(s, 0);
    Put<int16_t>(s, int16_t(w - 1));
    Put<int16_t>(s, int16_t(h - 1));
    s += std::string(0x80 - s.size(), '\0');
    // plain bytes below 0xc0 and runs of 1..3. A run code is counted run pixels when the indices are kept and 0xc0 +
    // run pixels as colours (the original's bug): runs go only where neither reading writes past the bitmap
    const int n = w * h;
    int keepAt = 0, colourAt = 0;
    while (keepAt < n) {
        const int run = R(1, 3);
        const bool colourDone = colourAt >= n;
        if (R(0, 3) == 0 && n - keepAt >= run && (colourDone || n - colourAt >= 0xc0 + run)) {
            Put<uint8_t>(s, uint8_t(0xc0 + run));
            Put<uint8_t>(s, uint8_t(R(0, 255)));
            keepAt += run;
            if (!colourDone) colourAt += 0xc0 + run;
        } else {
            Put<uint8_t>(s, uint8_t(R(0, 0xbf)));
            ++keepAt;
            if (!colourDone) ++colourAt;
        }
    }
    s += std::string(256, '\0');  // room for the colour path's longer runs
    Put<uint8_t>(s, uint8_t(goodMarker ? 0x0c : 0x0b));
    s += Bytes(0x300);
    return s;
}

struct File {
    std::string name;  // under tmpDir
    bool compare;      // pixels compared (not for cut-short files)
};
struct Scenario {
    std::vector<File> files;
    int keep;
    std::string alphaGood, alphaBad;
    int saveFmt;
} sc;

static void Draw(int n)
{
    sc.files.clear();
    auto add = [&](const std::string& ext, const std::string& bytes, bool compare) {
        const std::string name = "kbi_" + Num(int(sc.files.size())) + "." + ext;
        WriteFile(tmpDir + name, bytes);
        sc.files.push_back(File{name, compare});
        if (R(0, 5) == 0 && bytes.size() > 4) {  // cut short
            const std::string cut = "kbi_" + Num(int(sc.files.size())) + "c." + ext;
            WriteFile(tmpDir + cut, bytes.substr(0, size_t(R(0, int(bytes.size()) - 1))));
            sc.files.push_back(File{cut, false});
        }
    };
    const int w = R(1, 40) * (R(0, 1) ? 4 : 1), h = R(1, 40);
    const int descs[] = {0, 0x20, 0x08, 0x28, 0x10};
    switch (n % 4) {
    case 0:
        add("tga", Tga(2, 0, 0, 24, w, h, descs[R(0, 4)], R(0, 1) ? 0 : R(1, 30)), true);
        add("tga", Tga(2, 0, 0, 32, w, h, descs[R(0, 4)], 0), true);
        add("tga", Tga(2, 0, 0, 16, w, h, descs[R(0, 4)], 0), true);
        add("tga", Tga(1, 1, R(0, 3) ? 24 : 16, 8, w, h, descs[R(0, 4)], 0), true);
        add("tga", Tga(R(0, 1) ? 3 : 10, 0, 0, 8, w, h, 0, 0), true);
        break;
    case 1:
        add("bmp", Bmp(24, w, R(0, 1) ? h : -h, 0), true);
        add("bmp", Bmp(32, w, R(0, 1) ? h : -h, 0), true);
        add("bmp", Bmp(8, w, R(0, 1) ? h : -h, R(0, 2) ? 0 : R(1, 300)), true);
        add("bmp", Bmp(R(0, 1) ? 16 : 4, w, h, 0), true);
        add("bmp", Bmp(24, w, h, 0, R(0, 1) ? 2 : 1, R(0, 1)), true);
        break;
    case 2:
        add("pcx", Pcx(w, h, R(0, 4) != 0), true);
        add("PCX", Pcx(R(1, 30), R(1, 30), true), true);
        break;
    default:
        add(R(0, 1) ? "jpg" : "jpeg", Bytes(size_t(R(0, 600))), true);  // garbage
        add("tga", Bytes(size_t(R(0, 300))), false);
        add("bmp", Bytes(size_t(R(0, 300))), false);
        add("xyz", Bytes(10), true);
        break;
    }
    sc.keep = R(0, 1);
    sc.alphaGood = "kbi_alpha.bmp";
    WriteFile(tmpDir + sc.alphaGood, Bmp(8, w, h, 0));
    sc.alphaBad = "kbi_alpha2.bmp";
    WriteFile(tmpDir + sc.alphaBad, Bmp(8, w + 4, h, 0));
    sc.saveFmt = R(0, 2);
}

static void* MakePString(const std::string& s) { return pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(s.c_str())); }

static void DumpBm(const char* tag, const Bm* b, bool pixels)
{
    Log(std::string(tag) + " " + Num(W(b)) + "x" + Num(H(b)) + " p" + Num(Pitch(b)) + " b" + Num(Bpp(b)) + " f" + Num(Fmt(b)) +
        " n" + Num(NumBytes(b)) + " d" + Num(Data(b) != nullptr) + " pal" + Num(Pal(b) != nullptr));
    if (!pixels) return;
    if (Pal(b)) Log("pal " + Hex(Pal(b), 0x400));
    if (!Data(b) || W(b) <= 0 || H(b) <= 0) return;
    const size_t row = size_t(W(b)) * Bpp(b);
    for (int y = 0; y < H(b); ++y) Log(Hex(Data(b) + size_t(y) * Pitch(b), row));
}

static void LoadOne(const Api& a, const std::string& path, bool keep, bool pixels)
{
    alignas(8) Bm b[0x44];
    std::memset(b, 0, sizeof(b));  // the constructor leaves the bytes per pixel as they were
    a.ctor(b, nullptr);
    void* name = MakePString(path);
    void* none = nullptr;
    const bool ok = a.load(b, &name, &none, keep);
    std::string shown = path.substr(path.find_last_of('\\') + 1);
    if (shown.rfind("kbi_out", 0) == 0) shown = "kbi_out" + shown.substr(8);  // the side's own copy
    Log("load " + shown + " k" + Num(keep) + " " + Num(ok));
    // what the original leaves unwritten: a TGA whose rows were cut short, a bottom-up colour-mapped TGA kept as
    // indices (all rows over row 0), the 16-bit row buffer before its first full row
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (char& ch : ext) ch = char(tolower(static_cast<unsigned char>(ch)));
    if (keep && ext == "pcx" && ok) {
        // the indices go in one run of width * height bytes, whatever the pitch
        DumpBm("bm", b, false);
        if (pixels && Pal(b)) Log("pal " + Hex(Pal(b), 0x400));
        if (pixels && Data(b)) Log(Hex(Data(b), size_t(W(b)) * H(b)));
    } else {
        DumpBm("bm", b, pixels && ok);
    }
    a.dtor(b, nullptr);
    if (name) pstringDestroy(*pstringCache, name);
}

static void Info(const Api& a, const std::string& path)
{
    int f = -7;
    unsigned long w = 7, h = 7;
    const bool ok = a.info(path.c_str(), &f, &w, &h);
    Log("info " + Num(ok) + " " + Num(f) + " " + Num(static_cast<long long>(w)) + " " + Num(static_cast<long long>(h)));
}

static std::vector<std::string> corpus;

// pixel rows are compared unless the original leaves some unwritten
static bool PixelsDefined(const std::string& path, bool keep)
{
    const std::string bytes = ReadFile(path);
    if (bytes.size() < 18) return false;
    const std::string ext = path.substr(path.find_last_of('.') + 1);
    auto lower = [](std::string s) {
        for (char& c : s) c = char(tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string e = lower(ext);
    if (e == "tga") {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
        const int type = p[2], bits = p[16], desc = p[17], w = p[12] | p[13] << 8, h = p[14] | p[15] << 8;
        size_t need = 18 + p[0] + size_t(w) * h * ((bits + 7) / 8);
        if (type == 1) need += 0x300;
        if (bytes.size() < need) return false;
        if (type == 1 && keep && !(desc & 0x20)) return h <= 1;
        return true;
    }
    return true;
}

static void Run(int scenario)
{
    const Api& a = apis[side];
    for (const File& f : sc.files) {
        const std::string path = tmpDir + f.name;
        // GetInfo reads the header with File::Read into an uninitialised buffer: a file shorter than the header
        // leaves stack bytes in the result
        if (f.compare && ReadFile(path).size() >= 0x80) Info(a, path);
        LoadOne(a, path, sc.keep != 0, f.compare && PixelsDefined(path, sc.keep != 0));
    }
    // alpha
    {
        alignas(8) Bm b[0x44];
        std::memset(b, 0, sizeof(b));
        a.ctor(b, nullptr);
        const std::string& base = sc.files[0].name;
        const bool ok = a.loadAlpha(b, (tmpDir + base).c_str(), (tmpDir + sc.alphaGood).c_str());
        Log("loadAlpha " + Num(ok));
        DumpBm("la", b, ok && PixelsDefined(tmpDir + base, false));
        Log("addAlpha bad " + Num(a.addAlpha(b, (tmpDir + sc.alphaBad).c_str())));
        Log("addAlpha missing " + Num(a.addAlpha(b, (tmpDir + "kbi_none.bmp").c_str())));
        // save, then load back
        if (Data(b) && PixelsDefined(tmpDir + base, false)) {
            static const char* exts[] = {"tga", "bmp", "jpg"};
            const std::string out = "kbi_out" + Num(side) + "." + exts[sc.saveFmt];
            const bool saved = a.save(b, (tmpDir + out).c_str());
            const std::string bytes = ReadFile(tmpDir + out);
            Log("save " + std::string(exts[sc.saveFmt]) + " " + Num(saved) + " " + Num(static_cast<long long>(bytes.size())) + " " +
                Hex(bytes.data(), bytes.size()));
            LoadOne(a, tmpDir + out, false, true);
            DeleteFileA((tmpDir + out).c_str());
        }
        a.dtor(b, nullptr);
    }
    // corpus share of this scenario
    for (size_t i = size_t(scenario); i < corpus.size(); i += 1000000) {
        if (getenv("KBI_START") && i < size_t(std::atoi(getenv("KBI_START")))) continue;
        if (getenv("KBI_TRACE")) std::printf("side %d corpus %zu %s\n", side, i, corpus[i].c_str());
        Info(a, corpus[i]);
        // a 4096 x 4096 image needs 64 MB in one block: in this 32-bit process (both modules at fixed bases) that
        // fails now and then on either side (Alloc throws), so the biggest images are compared by GetInfo only
        {
            int f = 0;
            unsigned long w = 0, h = 0;
            if (a.info(corpus[i].c_str(), &f, &w, &h) && double(w) * double(h) > 2048.0 * 4096.0) continue;
        }
        LoadOne(a, corpus[i], false, true);
        LoadOne(a, corpus[i], true, PixelsDefined(corpus[i], true));
    }
}

static void Walk(const std::string& dir, std::vector<std::string> (&out)[3])
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        const std::string p = dir + "\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Walk(p, out);
            continue;
        }
        const size_t dot = n.find_last_of('.');
        if (dot == std::string::npos) continue;
        std::string e = n.substr(dot + 1);
        for (char& c : e) c = char(tolower(static_cast<unsigned char>(c)));
        if (e == "tga") out[0].push_back(p);
        else if (e == "bmp") out[1].push_back(p);
        else if (e == "jpg" || e == "jpeg") out[2].push_back(p);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    std::printf("crash %08lx at %p side %d\n", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress, side);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_bitmap_io <dir> [scenarios] [corpus root] [per type]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 400;
    const std::string root = argc > 3 ? argv[3] : "";
    const int perType = argc > 4 ? std::atoi(argv[4]) : 300;
    rng.seed(4321u);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KBI_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        SetCurrentDirectoryA(tmpDir.c_str());
    }
    if (const char* leak = getenv("KBI_LEAK")) {  // loads one file many times per side and prints the memory in use
        for (side = 0; side < 2; ++side) {
            for (int k = 0; k < 6; ++k) {
                for (int r = 0; r < 40; ++r) {
                    Info(apis[side], leak);
                    LoadOne(apis[side], leak, false, false);
                    LoadOne(apis[side], leak, true, false);
                    logs[side].clear();
                }
                PROCESS_MEMORY_COUNTERS pmc{};
                GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
                std::printf("%s side %d after %d: %zu KB\n", swap ? "swapped" : "", side, (k + 1) * 40, pmc.PagefileUsage / 1024);
            }
        }
        return 0;
    }
    if (!root.empty()) {
        std::vector<std::string> found[3];
        Walk(root, found);
        std::printf("corpus: %zu tga, %zu bmp, %zu jpg\n", found[0].size(), found[1].size(), found[2].size());
        const bool all = getenv("KBI_ALL") != nullptr;
        for (auto& list : found) {
            std::sort(list.begin(), list.end());
            const size_t step = all || int(list.size()) <= perType ? 1 : list.size() / size_t(perType);
            for (size_t i = 0; i < list.size(); i += step) corpus.push_back(list[i]);
        }
    }
    // the corpus is spread over the scenarios: scenario k loads corpus[k], corpus[k + 1000000], ...
    const int total = std::max(scenarios, int(corpus.size()));
    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < total; ++n) {
        Draw(n);
        for (side = 0; side < 2; ++side) Run(n);
        const size_t lines = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < lines; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 10) {
                    std::string ctx;
                    for (size_t k = std::min(i + 1, logs[0].size()); k-- > 0;)
                        if (logs[0][k].rfind("load ", 0) == 0 || logs[0][k].rfind("info ", 0) == 0 || logs[0][k].rfind("save ", 0) == 0) {
                            ctx = logs[0][k].substr(0, 120);
                            break;
                        }
                    std::printf("scenario %d line %zu after '%s':\n  ours %s\n  orig %s\n", n, i, ctx.c_str(), x.substr(0, 300).c_str(),
                                y.substr(0, 300).c_str());
                }
                break;
            }
        }
        if (getenv("KBI_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 200).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
