// x64 stand-in for KernelStandard_7c, linked statically into the x64 harness (JET_KS_STATIC).
//
// This is NOT a reconstruction of KernelStandard: there is no x64 KernelStandard yet, and Kernel_7c needs ~95 of
// its functions to link. Each one does just what Kernel_7c and the probes rely on. Where a probe's output depends
// on it, the behaviour follows the 32-bit module (decompiled addresses given): PString interning and reference
// counts, Options parsing of plugins.ini, ReferenceCount, Mutex. Anything else is the simplest Win32 equivalent.
// Functions nothing in the harness reaches abort loudly.
#ifndef JET_KS_IMPLEMENTATION
#define JET_KS_IMPLEMENTATION
#endif
#include <jet/KernelStandard.h>

#include <windows.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <unordered_map>

namespace Jet {

[[noreturn]] static void NotInStub(const char* what)
{
    std::fprintf(stderr, "KernelStandardStub: %s is not part of the x64 stand-in\n", what);
    std::abort();
}

// ---------------------------------------------------------------------------------------------------------------
// Alloc, Time, Profile, Log, Thread, ExceptionHandler, Events, Rand

void* __cdecl Alloc::New(unsigned int size) { return std::malloc(size ? size : 1); }
void __cdecl Alloc::Delete(void* p) { std::free(p); }

unsigned long __cdecl Time::Ms() { return GetTickCount(); }
double __cdecl Time::GetSeconds()
{
    static const LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / static_cast<double>(freq.QuadPart);
}
void __cdecl Time::SetFrameLockTime(double) {}
void __cdecl Time::IncFrame() {}
void __cdecl Time::Delay(unsigned long ms) { Sleep(ms); }

int __cdecl Profile::RegisterTracePoint(const char*) { return 0; }
void __cdecl Profile::TracePoint(int) {}

void __cdecl Log::Init(Options*) {}

void __cdecl Thread::GetProcessName(char* out) { GetModuleFileNameA(nullptr, out, MAX_PATH); }

int __cdecl ExceptionHandler::Main(CallBack fn, void* arg, Thread*) { return fn(arg); }
void* __cdecl ExceptionHandler::AddCriticalShutdown(CallBack, void*)
{
    static int token;
    return &token;
}
void __cdecl ExceptionHandler::RemoveCriticalShutdown(void*) {}

Events::Events() : ownsEvents(true), lastId(0), removed{}, queue{}, requeue{} {}
Events::~Events() {}

// Rand/LCRand: the real unit U5 (src/jet/core/KernelStandard_7c/Rand.cpp) is linked; it reads these Time statics
bool Time::frameLock = false;
double Time::frameLockTime = 0.0;

// ---------------------------------------------------------------------------------------------------------------
// PStringCache / PString (0x10039cc0 Create, 0x1003a290 Destroy, 0x1003a3b0 Ins, 0x1003a810 Strip,
// 0x10039e80 GetANSI). Like the original, Destroy only drops the reference: nodes stay interned.

namespace {
struct CacheState {
    CRITICAL_SECTION lock;
    std::unordered_map<std::string, PStringCache::Node*> nodes;
    CacheState() { InitializeCriticalSection(&lock); }
};
CacheState& State()
{
    static CacheState s;
    return s;
}
}  // namespace

// the stand-in keeps its nodes in State(); the cache object only carries the counters (zeroed: the real
// constructor builds the hash table this stand-in does not use)
alignas(PStringCache) static unsigned char g_cache[sizeof(PStringCache)];
PStringCache* PString::cache = reinterpret_cast<PStringCache*>(g_cache);

void __cdecl PStringCache::Lock() { EnterCriticalSection(&State().lock); }
void __cdecl PStringCache::Unlock() { LeaveCriticalSection(&State().lock); }

static PStringCache::Node* Intern(PStringCache* c, const char* text, size_t length)
{
    c->Lock();
    auto& slot = State().nodes[std::string(text, length)];
    if (!slot) {
        auto* n = static_cast<PStringCache::Node*>(std::calloc(1, offsetof(PStringCache::Node, text) + length + 1));
        n->length = static_cast<unsigned int>(length);
        n->capacity = static_cast<unsigned int>(length);
        std::memcpy(n->text, text, length);
        slot = n;
    }
    PStringCache::Node* n = slot;
    ++n->refs;
    ++c->liveRefs;
    c->Unlock();
    return n;
}

PStringCache::Node* __cdecl PStringCache::Create(const unsigned char* text)
{
    if (!text) return nullptr;
    const char* t = reinterpret_cast<const char*>(text);
    return Intern(this, t, std::strlen(t));
}

void __cdecl PStringCache::Destroy(Node* node)
{
    Lock();
    --liveRefs;
    --node->refs;
    Unlock();
}

PStringCache::Node* __cdecl PStringCache::Ins(Node* node, const unsigned char* text, unsigned int pos)
{
    const char* t = reinterpret_cast<const char*>(text);
    if (!node) return t ? Intern(this, t, std::strlen(t)) : nullptr;
    if (!t || !*t || pos > node->length) return node;
    std::string s(node->text, node->length);
    s.insert(pos, t);
    Destroy(node);
    return Intern(this, s.data(), s.size());
}

PStringCache::Node* __cdecl PStringCache::Strip(Node* node, const unsigned char* chars)
{
    if (!node || !node->length) return node;
    std::string s;
    for (unsigned int i = 0; i < node->length; ++i)
        if (!std::strchr(reinterpret_cast<const char*>(chars), node->text[i])) s += node->text[i];
    if (s.size() == node->length) return node;
    Destroy(node);
    return Intern(this, s.data(), s.size());
}

int __cdecl PStringCache::GetANSI(Node* node, char* out, unsigned int size) const
{
    if (size < 2 || !node) {
        *out = 0;
        return 0;
    }
    const unsigned int n = node->length < size - 1 ? node->length : size - 1;
    std::memcpy(out, node->text, n);
    out[n] = 0;
    return static_cast<int>(n);
}

static std::string VFormat(const char* fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) std::vsnprintf(s.data(), s.size() + 1, fmt, args);
    return s;
}

void __cdecl PString::Setf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const std::string s = VFormat(fmt, args);
    va_end(args);
    Release();
    node = Intern(cache, s.data(), s.size());
}

int __cdecl PString::Addf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const std::string s = VFormat(fmt, args);
    va_end(args);
    node = cache->Ins(node, reinterpret_cast<const unsigned char*>(s.c_str()), Length());
    return static_cast<int>(s.size());
}

int __cdecl PString::Compare(const char* text) const { return std::strcmp(Text() ? Text() : "", text ? text : ""); }

// ---------------------------------------------------------------------------------------------------------------
// AnsiString / String: data is a malloc'd C string (or null for empty); copies are deep.

namespace {
struct AnsiAccess : AnsiString {
    static void*& Data(AnsiString& s) { return static_cast<AnsiAccess&>(s).data; }
    static const char* Text(const AnsiString& s)
    {
        const void* d = static_cast<const AnsiAccess&>(s).data;
        return d ? static_cast<const char*>(d) : "";
    }
};
void SetText(AnsiString& s, const char* text, size_t n)
{
    void*& d = AnsiAccess::Data(s);
    char* p = static_cast<char*>(std::malloc(n + 1));
    std::memcpy(p, text, n);
    p[n] = 0;
    std::free(d);
    d = p;
}
void Append(AnsiString& s, const char* text)
{
    const std::string t = std::string(AnsiAccess::Text(s)) + (text ? text : "");
    SetText(s, t.data(), t.size());
}
}  // namespace

AnsiString::AnsiString() : data(nullptr) {}
AnsiString::AnsiString(const AnsiString& o) : data(nullptr)
{
    if (o.data) SetText(*this, static_cast<const char*>(o.data), std::strlen(static_cast<const char*>(o.data)));
}
AnsiString::~AnsiString()
{
    std::free(data);
    data = nullptr;
}
__cdecl AnsiString::operator const char*() const { return AnsiAccess::Text(*this); }
AnsiString __cdecl AnsiString::Token(const char* delimiters)
{
    const std::string all = AnsiAccess::Text(*this);
    const size_t at = std::strcspn(all.c_str(), delimiters);
    AnsiString result(*this);
    SetText(result, all.data(), at);
    const std::string rest = at < all.size() ? all.substr(at + 1) : std::string();
    SetText(*this, rest.data(), rest.size());
    return result;
}
AnsiString& __cdecl AnsiString::operator+=(const AnsiString& o)
{
    Append(*this, AnsiAccess::Text(o));
    return *this;
}
AnsiString& __cdecl AnsiString::operator+=(const char* s)
{
    Append(*this, s);
    return *this;
}
int __cdecl AnsiString::Addf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const std::string s = VFormat(fmt, args);
    va_end(args);
    Append(*this, s.c_str());
    return static_cast<int>(s.size());
}
bool __cdecl AnsiString::EqualsIgnoreCase(const char* s) const { return _stricmp(AnsiAccess::Text(*this), s ? s : "") == 0; }

String::String() {}
String::String(const char* s)
{
    if (s) SetText(*this, s, std::strlen(s));
}
String::~String() {}
__cdecl String::operator const char*() const { return AnsiAccess::Text(*this); }

// ---------------------------------------------------------------------------------------------------------------
// Options (0x10035a40 ProcessRaw, 0x10035fc0 value end, 0x100358a0 ProcessFile, 0x10036140 SetSeparators,
// 0x10036180 GetNext). One entry per key {next, prev, key, value, separators, cursor} (0x18 bytes on 32-bit).
// "+Key=v" on a key with a non-empty value appends ",v". A value runs to the end of the text, or up to the
// whitespace before the next /, - or + switch, or between quotes.

namespace {
template <class N>
struct Entry {
    N node;
    const char* cursor;
};

template <class N>
Entry<N>* FindEntry(N* head, const char* key)
{
    for (N* n = head; n; n = n->next)
        if (_stricmp(n->key, key) == 0) return reinterpret_cast<Entry<N>*>(n);
    return nullptr;
}

bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }
bool IsSwitch(char c) { return c == '/' || c == '-' || c == '+'; }

const char* ValueEnd(const char* p)
{
    if (*p == '\'' || *p == '"') {
        ++p;
        while (*p && *p != '\'' && *p != '"') ++p;
        return p;
    }
    const char* firstSpace = nullptr;
    for (; *p; ++p) {
        if (!IsSpace(*p)) {
            firstSpace = nullptr;
            continue;
        }
        if (!firstSpace) firstSpace = p;
        if (p[1] && IsSwitch(p[1])) break;
    }
    return firstSpace ? firstSpace : p;
}
}  // namespace

Options::Options() : head(nullptr), tail(nullptr), count(0) {}

void __cdecl Options::ProcessRaw(const char* p)
{
    while (*p) {
        while (*p && IsSpace(*p)) ++p;
        char sign = 0;
        if (IsSwitch(*p)) sign = *p++;
        const char* keyStart = p;
        while (*p && !IsSpace(*p) && *p != '=' && *p != '-' && *p != '+') ++p;
        const std::string key(keyStart, p);
        const char* keyEnd = p;
        while (*p && IsSpace(*p)) ++p;
        std::string value;
        if (*p == '=') {
            ++p;
            while (*p && IsSpace(*p)) ++p;
            const char* end = ValueEnd(p);
            const char* v = (*p == '\'' || *p == '"') ? p + 1 : p;
            if (end > v) value.assign(v, end);
            p = end;
            if (*p == '\'' || *p == '"') ++p;
        } else if ((*p == '-' || *p == '+') && p == keyEnd) {
            value.assign(p, 1);
            ++p;
        }
        if (key.empty()) continue;
        auto* e = FindEntry(head, key.c_str());
        if (!e) {
            e = static_cast<decltype(e)>(Alloc::New(sizeof *e));
            std::memset(e, 0, sizeof *e);
            new (&e->node.key) String(key.c_str());
            new (&e->node.value) String();
            new (&e->node.separators) String();
            e->node.prev = tail;
            if (tail)
                tail->next = &e->node;
            else
                head = &e->node;
            tail = &e->node;
            ++count;
        }
        if (sign == '+' && std::strlen(e->node.value) != 0) {
            e->node.value += ",";
            e->node.value += value.c_str();
        } else {
            e->node.value.~String();
            new (&e->node.value) String(value.c_str());
        }
    }
}

void __cdecl Options::ProcessFile(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return;
    std::string text;
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
    std::fclose(f);
    ProcessRaw(text.c_str());
}

void __cdecl Options::SetSeparators(const char* key, const char* separators) const
{
    if (auto* e = FindEntry(head, key)) {
        e->node.separators.~String();
        new (&e->node.separators) String(separators);
        const char* v = e->node.value;
        e->node.cursor = *v ? v : nullptr;
    }
}

bool __cdecl Options::GetNext(const char* key, String& value) const
{
    auto* e = FindEntry(head, key);
    if (!e || !e->node.cursor) return false;
    const char* seps = e->node.separators;
    const char* start = e->node.cursor;
    const char* end = start + std::strcspn(start, seps);
    if (end <= start) return false;
    const char* last = end;
    while (last > start + 1 && IsSpace(last[-1])) --last;
    value.~String();
    new (&value) String(std::string(start, last).c_str());
    while (*end && (IsSpace(*end) || std::strchr(seps, *end))) ++end;
    e->node.cursor = end;
    return true;
}

bool __cdecl Options::Get(const char* key) const { return FindEntry(head, key) != nullptr; }

bool __cdecl Options::Get(const char* key, int& value, int fallback) const
{
    auto* e = FindEntry(head, key);
    value = e ? std::atoi(e->node.value) : fallback;
    return e != nullptr;
}

bool __cdecl Options::Get(const char* key, float& value, float fallback) const
{
    auto* e = FindEntry(head, key);
    value = e ? static_cast<float>(std::atof(e->node.value)) : fallback;
    return e != nullptr;
}

bool __cdecl Options::Get(const char* key, String& value) const
{
    auto* e = FindEntry(head, key);
    if (!e) return false;
    const std::string v = static_cast<const char*>(e->node.value);
    value.~String();
    new (&value) String(v.c_str());
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// OS: {Options*, appName, commandLine} in the opaque block

namespace {
struct OSState {
    Options* options;
    char* appName;
    char* commandLine;
};
}  // namespace

OS::OS(const char* appName, const char* commandLine, void*, HWND__*)
{
    static_assert(sizeof(OSState) <= sizeof opaque);
    auto* s = reinterpret_cast<OSState*>(opaque);
    s->options = new (Alloc::New(sizeof(Options))) Options;
    s->appName = _strdup(appName ? appName : "");
    s->commandLine = _strdup(commandLine ? commandLine : "");
}
OS::~OS()
{
    auto* s = reinterpret_cast<OSState*>(opaque);
    s->options->~Options();
    Alloc::Delete(s->options);
    std::free(s->appName);
    std::free(s->commandLine);
}
bool OS::Init()
{
    auto* s = reinterpret_cast<OSState*>(opaque);
    s->options->ProcessRaw(s->commandLine);
    return true;
}
Options* __cdecl OS::GetOptions() const { return reinterpret_cast<const OSState*>(opaque)->options; }
const char* __cdecl OS::GetAppName() const { return reinterpret_cast<const OSState*>(opaque)->appName; }
DynamicLibrary* __cdecl OS::CreateDynamicLibrary() const { return new DynamicLibrary; }

// ---------------------------------------------------------------------------------------------------------------
// DynamicLibrary

bool __cdecl DynamicLibrary::Open(const char* path)
{
    if (handle) Close();
    handle = LoadLibraryA(path);
    return handle != nullptr;
}
bool __cdecl DynamicLibrary::Import(void** proc, const char* name)
{
    *proc = handle ? reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name)) : nullptr;
    return *proc != nullptr;
}
void __cdecl DynamicLibrary::Close()
{
    if (handle) FreeLibrary(static_cast<HMODULE>(handle));
    handle = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// File

static bool IsSlash(char c) { return c == '\\' || c == '/'; }

File::Path::Path(const char* path)
{
    text[0] = 0;
    if (path) strncpy_s(text, path, _TRUNCATE);
}
void __cdecl File::Path::DelName(bool keepSlash)
{
    char* cut = nullptr;
    for (char* p = text; *p; ++p)
        if (IsSlash(*p)) cut = p;
    if (!cut)
        text[0] = 0;
    else
        cut[keepSlash ? 1 : 0] = 0;
}
void __cdecl File::Path::AddPath(const char* path)
{
    const size_t n = std::strlen(text);
    if (n && !IsSlash(text[n - 1]) && path && !IsSlash(path[0])) strcat_s(text, "\\");
    if (path) strcat_s(text, path);
}
void __cdecl File::Path::Add(const char* t)
{
    if (t) strcat_s(text, t);
}
bool __cdecl File::Path::IsMatch(const char* other, const char** rest) const
{
    if (rest) *rest = nullptr;
    return _stricmp(text, other ? other : "") == 0;
}
const char* __cdecl File::Path::GetFileName() const
{
    const char* name = text;
    for (const char* p = text; *p; ++p)
        if (IsSlash(*p)) name = p + 1;
    return name;
}
bool __cdecl File::Path::IsExt(const char* ext) const
{
    const char* dot = std::strrchr(GetFileName(), '.');
    return dot && _stricmp(dot + 1, ext ? ext : "") == 0;
}
void __cdecl File::Path::SetExt(const char* ext)
{
    if (char* dot = const_cast<char*>(std::strrchr(GetFileName(), '.'))) *dot = 0;
    if (ext && *ext) {
        strcat_s(text, ".");
        strcat_s(text, ext);
    }
}

namespace {
struct FindState {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    char dir[MAX_PATH];
};
}  // namespace

File::Entry::Entry() { std::memset(this, 0, sizeof *this); }
File::Path::Path() { text[0] = 0; }

// the stand-in's search state hangs off the subs field (Find has its real members since U10b)
File::Find::Find() : handle(-1), subs(nullptr), names(nullptr), nameCount(0), archive(nullptr)
{
    auto* s = new FindState{};
    s->handle = INVALID_HANDLE_VALUE;
    subs = reinterpret_cast<SubChunk*>(s);
}
File::Find::~Find()
{
    Close();
    delete reinterpret_cast<FindState*>(subs);
}
bool __cdecl File::Find::Open(const char* pattern)
{
    Close();
    auto* s = reinterpret_cast<FindState*>(subs);
    Path dir(pattern);
    dir.DelName(true);
    strncpy_s(s->dir, dir.text, _TRUNCATE);
    s->handle = FindFirstFileA(pattern, &s->data);
    return s->handle != INVALID_HANDLE_VALUE;
}
bool __cdecl File::Find::Next()
{
    auto* s = reinterpret_cast<FindState*>(subs);
    return s->handle != INVALID_HANDLE_VALUE && FindNextFileA(s->handle, &s->data);
}
void __cdecl File::Find::Close()
{
    auto* s = reinterpret_cast<FindState*>(subs);
    if (s->handle != INVALID_HANDLE_VALUE) FindClose(s->handle);
    s->handle = INVALID_HANDLE_VALUE;
}
File::Path __cdecl File::Find::GetNameAndPath() const
{
    const auto* s = reinterpret_cast<const FindState*>(subs);
    Path p(s->dir);
    p.Add(s->data.cFileName);
    return p;
}

// Kernel_7c sees `bool File::Exists(const char*)` (KernelStandardRuntime.cpp); this header build declares the
// Result-returning one, so the bool symbol is supplied under its mangled name
static bool StubFileExists(const char* path) { return path && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
extern "C" bool StubFileExistsC(const char* path) { return StubFileExists(path); }
#pragma comment(linker, "/alternatename:?Exists@File@Jet@@SA_NPEBD@Z=StubFileExistsC")

bool __cdecl File::Md(const char* path)
{
    const std::string p(path);
    for (size_t i = 1; i < p.size(); ++i)
        if (IsSlash(p[i]) && p[i - 1] != ':') CreateDirectoryA(p.substr(0, i).c_str(), nullptr);
    return CreateDirectoryA(p.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void PruneTree(const std::string& dir)
{
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::string name = d.cFileName;
            if (name == "." || name == "..") continue;
            const std::string full = dir + "\\" + name;
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                PruneTree(full);
            else
                DeleteFileA(full.c_str());
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

bool __cdecl File::Prune(const char* path)
{
    PruneTree(path);
    return true;
}

// Sync objects: the real unit U4 (src/jet/core/KernelStandard_7c/SyncObjects.cpp) is linked instead

// ---------------------------------------------------------------------------------------------------------------
// ReferenceCount (0x1003d3a0..0x1003d3e0)

void ReferenceCount::AddReference() const { InterlockedIncrement(reinterpret_cast<volatile LONG*>(&referenceCount)); }
bool ReferenceCount::ReleaseReference() const
{
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&referenceCount)) == 0) {
        DeleteThis();
        return true;
    }
    return false;
}
void __cdecl ReferenceCount::DeleteThis() const { delete this; }
ReferenceCount::~ReferenceCount() {}

// ---------------------------------------------------------------------------------------------------------------
// I/O: IOSlave and IOSlaveMemory (Kernel_7c reads its blocks through them); the rest of the family is not needed

IOSlave::IOSlave() {}
IOSlave::IOSlave(const IOSlave&) {}
IOSlave& IOSlave::operator=(const IOSlave&) { return *this; }
IOSlave::~IOSlave() {}
unsigned long IOSlave::GetSize() { return 0; }
unsigned long IOSlave::ReadData(void*, unsigned long) { return 0; }
unsigned long IOSlave::WriteData(const void*, unsigned long) { return 0; }
bool IOSlave::WriteByte(unsigned char v) { return WriteData(&v, 1) == 1; }
bool IOSlave::WriteUINT32(unsigned long v) { return WriteData(&v, 4) == 4; }
bool IOSlave::ReadUINT32(unsigned long& v) { return ReadData(&v, 4) == 4; }
unsigned long IOSlave::Write(IOSlaveReadCursor&, unsigned long) { NotInStub("IOSlave::Write"); }

IOSlaveMemory::IOSlaveMemory(void* d, unsigned long n)
    : data(static_cast<unsigned char*>(d)), position(0), size(d ? n : 0)
{
}
IOSlaveMemory::~IOSlaveMemory() {}
bool IOSlaveMemory::Seek(unsigned long p)
{
    if (p > size) return false;
    position = p;
    return true;
}
unsigned long IOSlaveMemory::Tell() { return position; }
unsigned long IOSlaveMemory::GetSize() { return size; }
unsigned long IOSlaveMemory::ReadData(void* dst, unsigned long n)
{
    if (n >= size - position) n = size - position;
    if (dst) std::memcpy(dst, data + position, n);
    position += n;
    return n;
}
unsigned long IOSlaveMemory::WriteData(const void* src, unsigned long n)
{
    if (n >= size - position) n = size - position;
    std::memcpy(data + position, src, n);
    position += n;
    return n;
}

// ---------------------------------------------------------------------------------------------------------------
// DynamicBuffer: only UId::Read/Write use it, and the harness never makes one

bool DynamicBuffer::Read(unsigned long&) const { NotInStub("DynamicBuffer::Read"); }
bool DynamicBuffer::Read(unsigned short&) const { NotInStub("DynamicBuffer::Read"); }
unsigned long DynamicBuffer::Read(void*, unsigned long, unsigned long) const { NotInStub("DynamicBuffer::Read"); }
bool DynamicBuffer::Write(unsigned long) { NotInStub("DynamicBuffer::Write"); }
bool DynamicBuffer::Write(unsigned short) { NotInStub("DynamicBuffer::Write"); }
unsigned long DynamicBuffer::Write(const void*, unsigned long, unsigned long) { NotInStub("DynamicBuffer::Write"); }

}  // namespace Jet
