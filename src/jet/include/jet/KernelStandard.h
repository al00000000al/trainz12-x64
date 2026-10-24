// Declarations of KernelStandard_7c classes that our modules call. Only what is used so far; members are
// declared exactly as exported so the mangled names match the import library.
#pragma once

#include <jet/PString.h>
#include <jet/Result.h>

#include <cstddef>

struct HWND__;

namespace Jet {

class UnicodeString;
class AnsiString;
class String;

// What the strings' binary Read / Write go through (DynamicBuffer::Read / Write of a string, Streams.cpp):
// {vftable, DynamicBuffer*}, slot 1 moves the bytes (a count is returned).
class BinReader {
public:
    virtual ~BinReader() {}
    virtual unsigned long Read(void* dst, unsigned long size) = 0;
};
class BinWriter {
public:
    virtual ~BinWriter() {}
    virtual unsigned long Write(const void* src, unsigned long size) = 0;
};

// A growable byte buffer with a cursor (KernelStandard unit U9a, Streams.cpp): {data, position, capacity, size
// (the bytes written), increment (the growth step, 0x200 when 0), external (the data is not owned: no writes, not
// freed)}, 0x18 bytes. Users build it inline.
class DynamicBuffer {
public:
    DynamicBuffer(unsigned long capacity = 0, unsigned long increment = 0);
    DynamicBuffer(void* data, unsigned long size, bool external);
    DynamicBuffer(const DynamicBuffer& o);
    DynamicBuffer& operator=(const DynamicBuffer& o);
    ~DynamicBuffer();
    void Clean();   // = the destructor: data freed, sizes and position 0
    void Reset();   // size and position 0
    void Rewind();  // position 0
    void ReInit(unsigned long capacity, unsigned long increment);
    bool Seek(unsigned long position);  // past the capacity: clipped, false
    bool SetSize(unsigned long size);   // past the capacity: clipped, false
    void SetData(void* data, unsigned long size, bool external);
    bool IsReadOnly();
    unsigned long GetSize() const;
    unsigned long GetLength() const;  // the capacity
    unsigned long GetIncrement() const;
    unsigned long GetCurrentPos() const;
    void* GetData() const;
    void* GetCurrentData() const;
    void AddLength(unsigned long n);  // room for n more bytes at the position

    bool Write(short v);
    bool Write(unsigned short v);
    bool Write(signed char v);
    bool Write(unsigned char v);
    bool Write32(int v);
    bool Write32(unsigned int v);
    bool Write(long v);
    bool Write(unsigned long v);
    bool Write(float v);
    bool Write(bool v);  // 4 bytes
    bool Write(const AnsiString& s);
    bool Write(const String& s);
    bool Write(const UnicodeString& s);
    bool Write(const PString& s);
    unsigned long Write(const void* src, unsigned long size, unsigned long count);  // `size` (not the bytes)

    // a read takes what is left: 1-3 bytes of a 4-byte value succeed and leave the rest as it was
    bool Read(short& v) const;
    bool Read(unsigned short& v) const;
    bool Read(signed char& v) const;
    bool Read(unsigned char& v) const;
    bool Read(long& v) const;
    bool Read(unsigned long& v) const;
    bool Read(float& v) const;
    bool Read(bool& v) const;
    bool Read32(int& v) const;  // (zeroed first)
    bool Read32(unsigned int& v) const;
    bool Read(PString& s) const;
    bool Read(AnsiString& s) const;
    bool Read(String& s) const;
    bool Read(UnicodeString& s) const;
    unsigned long Read(void* dst, unsigned long size, unsigned long count) const;  // the bytes read

    unsigned char* data;     // +0x00
    unsigned long position;  // +0x04
    unsigned long capacity;  // +0x08
    unsigned long size;      // +0x0c
    unsigned long increment; // +0x10
    bool external;           // +0x14
};
JET_LAYOUT32(sizeof(DynamicBuffer) == 0x18);

// ---- random numbers (KernelStandard unit U5, Rand.cpp)
//
// Rand: an R250-style generator: a table of `size` words (>= 250) XORed with the word `lag` places back, seeded
// through an auxiliary table of `auxSize` words (>= 100) and an LCRand-like step. Kernel_7c keeps one static
// instance at 0x10019cf0. Scaled results are computed in x87 double precision; float results come back
// unrounded in st(0), as in the original.
class Rand {
public:
    Rand();  // allocates the tables but does not seed them (call SetSeed or Randomise)
    Rand(int size);
    Rand(int size, int auxSize);
    ~Rand();
    Rand& operator=(const Rand& o);  // a shallow copy: both share the tables

    void Randomise();  // seed from timeGetTime (or the frame-lock time) + 10
    void SetSeed(unsigned int seed);
    unsigned int GetSeed() const;
    unsigned int GetRandom();

    unsigned int operator()(unsigned int n);  // [0, n)
    int operator()(int lo, int hi);           // = NextInt(lo, hi)
    float operator()(float m);                // = NextFloat(m)
    float operator()(float lo, float hi);
    float operator()();
    int NextInt();  // 31 bits
    int NextInt(int n);
    int NextInt(int lo, int hi);  // 0 (not lo) when the generator gives 0
    float NextFloat();
    float NextFloat(float m);
    float NextFloat(float lo, float hi);
    double NextDouble();
    double NextDouble(double m);
    double NextDouble(double lo, double hi);
    bool NextBool();
    float Gaussian(float mean, float sd);  // polar method; the spare value is one static for all generators

protected:
    unsigned int RandLC();
    void Init(int size, int auxSize);

private:
    friend struct RandWork;
    int index;              // +0x00
    int lag;                // +0x04
    int size;               // +0x08
    int auxSize;            // +0x0c
    bool haveGauss;         // +0x10
    unsigned int state;     // +0x14: the LC state while seeding
    unsigned int* aux;      // +0x18
    unsigned int* table;    // +0x1c
};
JET_LAYOUT32(sizeof(Rand) == 0x20);

// LCRand: one 32-bit state, x = 16807 x mod (2^32 - 1) by Schrage's method (with the original's -1 for t <= 0)
class LCRand {
public:
    LCRand();  // seeded like Rand::Randomise
    LCRand(unsigned int seed);
    LCRand& operator=(const LCRand& o);
    void Randomise();
    void SetSeed(unsigned int seed);
    unsigned int GetSeed() const;
    unsigned int GetRandom();
    unsigned int operator()();
    int NextInt();
    int NextInt(int n);
    int NextInt(int lo, int hi);
    float NextFloat();
    float NextFloat(float m);
    float NextFloat(float lo, float hi);
    double NextDouble();
    double NextDouble(double m);
    double NextDouble(double lo, double hi);
    bool NextBool();

private:
    unsigned int state;
};

class Time {
public:
    static unsigned long __cdecl Ms();
    static double __cdecl GetSeconds();
    static void __cdecl SetFrameLockTime(double seconds);
    static void __cdecl IncFrame();
    static void __cdecl Delay(unsigned long ms);

private:
    friend struct RandWork;
    static JET_KS_FORWARDED bool frameLock;       // ?frameLock@Time@Jet@@0_NA
    static JET_KS_FORWARDED double frameLockTime;  // ?frameLockTime@Time@Jet@@0NA
};

// a scope timer on a function's static Info: while that Info's group (`mask`) is enabled, the time spent in the
// scope is added to it
class Profile {
public:
    struct Info {
        const char* name;  // "Input::Update"
        const char* file;
        unsigned long unknown08, time;
        int unknown10;       // -1 in the static initialisers
        unsigned long mask;  // 0x80000000 by default
        unsigned long unknown18, unknown1c;
    };
    Profile(Info* info);
    ~Profile();
    static int __cdecl RegisterTracePoint(const char* name);
    static void __cdecl TracePoint(int id);

private:
    Info* info;
    Profile* parent;
    unsigned long childTime, start;
};

class Alloc {
public:
    static void* __cdecl New(unsigned int size);
    static void __cdecl Delete(void* p);
};

// objects of classes using this live in Jet's heap, so any module can free them
#define JET_HEAP_OBJECT                                                    \
    static void* operator new(size_t n) { return Alloc::New(static_cast<unsigned int>(n)); } \
    static void operator delete(void* p) { Alloc::Delete(p); }

// ---- strings (KernelStandard). Both are one pointer to shared data; String derives from AnsiString.
class AnsiString {
public:
    AnsiString();  // the shared empty buffer
    AnsiString(const AnsiString& o);
    AnsiString(const char* s);
    ~AnsiString();
    AnsiString __cdecl Token(const char* delimiters);  // cuts and returns the first token
    __cdecl operator const char*() const;
    AnsiString& __cdecl operator+=(const AnsiString& o);
    AnsiString& __cdecl operator+=(const char* s);
    AnsiString& __cdecl operator+=(const PString& s);
    int __cdecl Addf(const char* fmt, ...);
    void __cdecl Setf(const char* fmt, ...);
    unsigned int __cdecl Size() const;
    bool __cdecl EqualsIgnoreCase(const char* s) const;
    void __cdecl ToLower();
    void __cdecl Setn(const char* s, unsigned int n);    // the first n characters (clipped to s)
    void __cdecl Ins(const char* s, unsigned int pos);  // s inserted at pos
    void __cdecl Ins(unsigned int ch, unsigned int pos, unsigned int count);  // count copies of ch at pos
    int __cdecl GetUnicode(__wchar_t* out, unsigned int size) const;  // characters written
    bool __cdecl Read(BinReader& r);
    bool __cdecl Write(BinWriter& w) const;
    const AnsiString& __cdecl operator=(const AnsiString& o);
    const AnsiString& __cdecl operator=(const PString& s);
    const AnsiString& __cdecl operator=(const char* s);

private:
    friend class Options;
    friend class ConfigData;
    friend class StringTablesBase;
    friend class IOSlaveResource;
    void MakeBufferUnique(unsigned int size, bool keep);

protected:
    void* data = nullptr;
};

class String : public AnsiString {
public:
    String();
    String(const char* s);
    ~String();
    __cdecl operator const char*() const;
    const String& __cdecl operator=(const char* s);
    const String& __cdecl operator=(const PString& s);
    String(const String& o);
    const String& __cdecl operator=(const String& o);
};

// UTF-16 string {text, capacity, length} (still the original's code)
class UnicodeString {
public:
    UnicodeString();
    ~UnicodeString();
    const UnicodeString& __cdecl operator=(const PString& s);
    bool __cdecl SetANSI(const char* s, int length);
    bool __cdecl Read(BinReader& r);
    bool __cdecl Write(BinWriter& w) const;

private:
    friend class ConfigData;
    friend class StringTablesBase;
    friend class IOSlaveResource;
    void __cdecl Set(const __wchar_t* s, int length, int capacity);

    wchar_t* text = nullptr;
    unsigned int capacity = 0;
    unsigned int length = 0;
};

// ---- options: a list of {key, value, ...} entries read from option files or raw lines
class Vector2;
class Vector3;
class Vector4;
class Quaternion;
class Color;
template <class T, unsigned int SIZ, class L>
class DynamicArray;
template <class A, class B>
struct Less;

// a pair of values written "a:b"
template <class T>
struct Range {
    T min;
    T max;
};

// The options of the command line, an options file, the environment: one entry per key (lower case), the value a
// string; typed reads parse it, GetNext walks it in separator-delimited runs.
class Options {
public:
    Options();
    // The original never exports a destructor: every user inlines it (Kernel_7c 0x1000c280).
    ~Options()
    {
        while (Node* n = head) {
            if (!n->prev)
                head = n->next;
            else
                n->prev->next = n->next;
            if (!n->next)
                tail = n->prev;
            else
                n->next->prev = n->prev;
            n->next = n->prev = nullptr;
            --count;
            n->separators.~String();
            n->value.~String();
            n->key.~String();
            Alloc::Delete(n);
        }
    }
    void __cdecl Init(const char* appName, int argc, char* argv[]);
    void __cdecl Init(const char* appName, const char* commandLine);
    void __cdecl ProcessCmdLine(int argc, char* argv[]);
    void __cdecl ProcessFile(const char* path);
    void __cdecl ProcessEnv(const char* variable);
    void __cdecl ProcessRaw(const char* line);
    void __cdecl Dump();
    void __cdecl SetSeparators(const char* key, const char* separators) const;
    bool __cdecl GetNext(const char* key, String& value) const;
    bool __cdecl GetNext(const char* key, PString& value) const;
    bool __cdecl GetNext(const char* key, bool& value) const;
    bool __cdecl GetNext(const char* key, unsigned int& value) const;
    bool __cdecl GetNext(const char* key, unsigned long& value) const;
    bool __cdecl GetNext(const char* key, int& value) const;
    bool __cdecl GetNext(const char* key, long& value) const;
    bool __cdecl GetNext(const char* key, float& value) const;
    bool __cdecl GetNext(const char* key, Vector2& value) const;
    bool __cdecl GetNext(const char* key, Vector3& value) const;
    bool __cdecl GetNext(const char* key, Vector4& value) const;
    bool __cdecl GetNext(const char* key, Quaternion& value) const;
    bool __cdecl GetNext(const char* key, Color& value) const;
    bool __cdecl Get(const char* key) const;
    bool __cdecl Get(const char* key, PString& value) const;
    bool __cdecl Get(const char* key, String& value) const;
    bool __cdecl Get(const char* key, PString& value, const PString& fallback) const;
    bool __cdecl Get(const char* key, String& value, const String& fallback) const;
    bool __cdecl Get(const char* key, bool& value, bool fallback) const;
    bool __cdecl Get(const char* key, int& value, int fallback) const;
    bool __cdecl Get(const char* key, unsigned int& value, unsigned int fallback) const;
    bool __cdecl Get(const char* key, long& value, long fallback) const;
    bool __cdecl Get(const char* key, unsigned long& value, unsigned long fallback) const;
    bool __cdecl Get(const char* key, float& value, float fallback) const;
    bool __cdecl Get(const char* key, Vector2& value, const Vector2& fallback) const;
    bool __cdecl Get(const char* key, Vector3& value, const Vector3& fallback) const;
    bool __cdecl Get(const char* key, Vector4& value, const Vector4& fallback) const;
    bool __cdecl Get(const char* key, Quaternion& value, const Quaternion& fallback) const;
    bool __cdecl Get(const char* key, Color& value, const Color& fallback) const;
    bool __cdecl Get(const char* key, DynamicArray<String, 41, Less<String, String>>& values, const char* separators) const;
    bool __cdecl Get(const char* key, Range<float>& value) const;
    bool __cdecl Get(const char* key, Range<int>& value) const;

private:
    // 0x18 bytes from Jet::Alloc
    struct Node {
        Node* next;
        Node* prev;
        String key;          // +0x08 lower case
        String value;        // +0x0c
        String separators;   // +0x10 for GetNext
        const char* cursor;  // +0x14 into value, where GetNext goes on
    };
    Node* Find(const char* key) const;
    Node* head;
    Node* tail;
    int count;
};

class DynamicLibrary;
class Window;

// The platform layer: options, window, application name. 0x18 bytes, built by App::Init in Jet's heap.
class OS {
public:
    OS(const char* appName, const char* commandLine, void* instance, HWND__* window);
    ~OS();
    bool Init();
    Options* __cdecl GetOptions() const;
    const char* __cdecl GetAppName() const;
    DynamicLibrary* __cdecl CreateDynamicLibrary() const;
    Window* __cdecl GetWindow() const;

private:
    unsigned char opaque[JET_OPAQUE_SIZE(0x18, 0x30)];
};

class Thread;  // (declared with the synchronisation objects)

// Runs a callback under Jet's crash handler; critical-shutdown callbacks run when it catches a crash.
class ExceptionHandler {
public:
    using CallBack = int(__cdecl*)(void*);
    static int __cdecl Main(CallBack fn, void* arg, Thread* thread);
    static void* __cdecl AddCriticalShutdown(CallBack fn, void* arg);
    static void __cdecl RemoveCriticalShutdown(void* handle);
    static void* __cdecl AddCriticalStartup(CallBack fn, void* arg);
    static void __cdecl RemoveCriticalStartup(void* handle);
};

// An intrusive list of T linked through T's ListLink at +4 (Events::Event, Job): {head, tail, count}. The links
// point at the neighbours' links, not at the objects.
template <class T>
struct ListOps;  // (KernelStandard_7c/Platform.cpp)
struct ListLink {
    ListLink* next;
    ListLink* prev;
};
template <class T>
struct LinkedList {
    T* head;
    T* tail;
    unsigned long count;
};

// Jet's timed event queue (0x2c bytes); the kernel keeps one as the TimeEventSystem. Events due by a time are
// updated in time order; a finished one is deleted when the queue owns its events, a repeating one is queued again.
class Events {
public:
    class Event {
    public:
        Event(unsigned long interval, unsigned long nextTime);
        Event(const Event& o);
        Event& operator=(const Event& o);
        int operator<(const Event& o) const;
        virtual bool Update();  // true: finished (the base moves its time on and finishes)
        virtual unsigned long GetId() const;
        unsigned long GetNextTime() const;
        void SetId(unsigned long id);
        JET_HEAP_OBJECT

    protected:
        void UpdateNext();

    private:
        friend class Events;
        template <class>
        friend struct ListOps;
        ListLink link;            // +0x04
        unsigned long interval;   // +0x0c
        unsigned long id;         // +0x10
        unsigned long nextTime;   // +0x14
    };

    Events();
    explicit Events(bool ownsEvents);
    ~Events();
    Events& operator=(const Events& o);
    void GenerateId(Event* e);
    void Add(Event* e);
    void Update(unsigned long time);
    bool RemoveEventSafely(Event* e);
    bool Remove(unsigned long id);
    void Remove(Event* e);

private:
    bool ownsEvents;               // +0x00
    unsigned long lastId;          // +0x04
    LinkedList<Event> removed;     // +0x08 removed by id, deleted on the next Update
    LinkedList<Event> queue;       // +0x14 by next time
    LinkedList<Event> requeue;     // +0x20 updated, not finished: back into the queue after the pass
};
JET_LAYOUT32(sizeof(Events) == 0x2c && sizeof(Events::Event) == 0x18);

class Log {
public:
    static void __cdecl Init(Options* options);
};

// ---- files
enum FileSeek { FILE_SEEK_SET = 0, FILE_SEEK_CUR = 1, FILE_SEEK_END = 2 };

class FileSubstitute;
class FileSub;

class File {
public:
    // a file time: seconds since 1970 (st_mtime), compared signed
    union Time {
        Time& operator=(const Time& o);
        bool __cdecl operator==(const Time& o) const;
        bool __cdecl operator<(const Time& o) const;
        bool __cdecl operator>(const Time& o) const;
        bool __cdecl operator<=(const Time& o) const;
        bool __cdecl operator>=(const Time& o) const;
        void __cdecl SetSeconds(long s);
        long seconds;
    };
    // one directory entry (0x120 bytes): a find result or an archive directory record
    struct Entry {
        Entry();
        Entry(const Entry& o);
        Entry& __cdecl operator=(const Entry& o);
        bool __cdecl IsDir();         // attrib bit 0x10
        bool __cdecl IsCompressed();  // attrib bit 0x80
        bool __cdecl IsHidden();      // attrib bit 0x02
        void __cdecl LittleEndian();  // does nothing
        char name[0x104];        // +0x000
        unsigned long size;      // +0x104 (an archive's stored size)
        unsigned long length;    // +0x108 (an archive's data length)
        unsigned long crc;       // +0x10c
        unsigned long u110;      // +0x110
        unsigned long u114;      // +0x114
        unsigned long time;      // +0x118
        unsigned short attrib;   // +0x11c _finddata attributes (0x80: compressed)
        unsigned short u11e;     // +0x11e
    };
    // a fixed MAX_PATH buffer with path helpers. Text past 0x102 characters leaves it empty and sets a
    // write-only error byte (0x100adb92 in the original).
    struct Path {
        Path();  // empty
        Path(const char* path);
        Path(const Path& o);
        void __cdecl operator=(const char* path);
        void __cdecl operator=(const Path& o);
        bool operator==(const Path& o) const;  // case-sensitive
        bool operator==(const char* o) const;
        bool operator!=(const Path& o) const;
        bool operator!=(const char* o) const;
        bool operator<(const Path& o) const;
        __cdecl operator const char*() const;
        void __cdecl Normalize();
        void __cdecl DelPath();
        void __cdecl DelFileName();
        void __cdecl Del(unsigned int pos, unsigned int count);
        void __cdecl DelDrive();
        void __cdecl DelSlash();
        void __cdecl AddSlash();
        void __cdecl ToLower();
        void __cdecl ToUpper();
        unsigned int __cdecl Length() const;
        unsigned int __cdecl Last() const;  // the last character
        unsigned int __cdecl GetFilePathHash() const;
        bool __cdecl HasValidData() const;  // NUL inside the buffer
        void __cdecl DelName(bool delSlash);  // true: the separator goes too
        void __cdecl AddPath(const char* path);
        void __cdecl AddPath(const char* path, int count);
        void __cdecl Add(const char* text);
        void __cdecl Add(const char* text, int count);
        bool __cdecl IsMatch(const char* other, const char** rest) const;
        static bool __cdecl IsMatch(const char* path, const char* pattern, const char** rest);
        static bool __cdecl IsEqual(const char* a, const char* b);
        bool __cdecl IsExt(const char* ext) const;
        void __cdecl SetExt(const char* ext);
        const char* __cdecl GetFileName() const;
        static const char* __cdecl GetFileName(const char* path);
        const char* __cdecl GetPath() const;  // past the drive colon
        static const char* __cdecl GetPath(const char* path);
        const char* __cdecl GetExt() const;
        static const char* __cdecl GetExt(const char* path);
        void __cdecl GetCwd();
        void __cdecl Make(const char* dir, const char* name);
        void __cdecl PrevDir();
        void __cdecl RootDir();
        void __cdecl ReplaceBackSlashes();
        void __cdecl ReplaceForwardSlashes();
        void __cdecl SplitPath(char* drive, char* dir, char* name, char* ext) const;
        bool __cdecl MakeRelativeTo(const char* base);
        Path __cdecl ExtractFirstPath();  // cuts and returns the first component
        FileSubstitute* __cdecl GetSubst() const;  // (still the original's: U10b)
        static FileSubstitute* __cdecl GetSubst(const char* path);
        char text[0x104];

    private:
        operator bool() const;  // exported, private: true
        bool operator!() const;  // exported, private: false
    };
    class Find {
    public:
        Find();
        ~Find();
        bool __cdecl Open(const char* pattern);
        bool __cdecl Open(const char* pattern, const PString& base);
        bool __cdecl Next();
        void __cdecl Close();
        Path __cdecl GetNameAndPath() const;

    private:
        unsigned char opaque[JET_OPAQUE_SIZE(0x440, 0x460)];
    };
    // File::Exists returns a ResultClass by value; bound at run time (see KernelStandardRuntime.cpp)
    static bool Exists(const char* path);
    static bool __cdecl Md(const char* path);     // makes the directory (and its parents)
    static void __cdecl Subst(const PString& drive, const char* path, bool add);
    static bool __cdecl Prune(const char* path);  // deletes the directory tree; the path must not be null

    // A buffered file over the CRT's low-level I/O (msvcr80's file descriptors: the original's FileSub code
    // opens files into these objects). Mode bits: 0x0f write (2: BPE-compressed "CFIL" output), 0xf0 read
    // (0x10 plain or compressed, 0x20 plain, 0x30 compressed), 0xf00 create (0x100 truncate, 0x200 append,
    // 0x300 new only), 0x1000 rejected. Unit U10a (File.cpp).
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;
    enum Mode { READ = 0x10, CREATE_WRITE = 0x101 };  // the modes InputSystem uses
    File();
    ~File();
    Result __cdecl Open(const char* path, unsigned long mode, int encoding);  // "x:rest": x is a subst name
    Result __cdecl Open(const PString& base, const char* path, unsigned long mode, int encoding);
    unsigned long __cdecl Read(void* data, unsigned long size, unsigned long count);  // items read
    bool __cdecl Read(signed char& v);
    bool __cdecl Read(unsigned char& v);
    bool __cdecl Read(short& v);
    bool __cdecl Read(unsigned short& v);
    bool __cdecl Read(long& v);
    bool __cdecl Read(unsigned long& v);
    bool __cdecl Read(float& v);
    bool __cdecl Read(__int64& v);
    bool __cdecl Read(unsigned __int64& v);
    bool __cdecl Read32(int& v);
    bool __cdecl Read32(unsigned int& v);
    bool __cdecl Read(PString& s);
    bool __cdecl Read(AnsiString& s);
    bool __cdecl Read(String& s);
    bool __cdecl Read(UnicodeString& s);
    unsigned long __cdecl Write(const void* data, unsigned long size, unsigned long count);
    bool __cdecl Write(signed char v);
    bool __cdecl Write(unsigned char v);
    bool __cdecl Write(short v);
    bool __cdecl Write(unsigned short v);
    bool __cdecl Write(long v);
    bool __cdecl Write(unsigned long value);
    bool __cdecl Write(float v);
    bool __cdecl Write(__int64 v);
    bool __cdecl Write(unsigned __int64 v);
    bool __cdecl Write32(int v);
    bool __cdecl Write32(unsigned int v);
    bool __cdecl Write(const PString& s);
    bool __cdecl Write(const AnsiString& s);
    bool __cdecl Write(const String& s);
    bool __cdecl Write(const UnicodeString& s);
    void __cdecl SafeRead(void* data, unsigned long size, unsigned long count);         // throws FileEx short
    void __cdecl SafeWrite(const void* data, unsigned long size, unsigned long count);  // throws FileEx short
    unsigned long __cdecl WriteLine(const char* text, int encoding);
    unsigned long __cdecl ReadLine(void* buffer, unsigned long size, int encoding);  // 0 at the end
    unsigned long __cdecl ReadBlock(const char* id, void* data, unsigned long size);
    unsigned long __cdecl WriteBlock(const char* id, const void* data, unsigned long size);
    unsigned long __cdecl Seek(long offset, FileSeek whence);
    unsigned long __cdecl SeekSet(long offset);
    unsigned long __cdecl SeekCur(long offset);
    unsigned long __cdecl SeekEnd(long offset);
    unsigned long __cdecl Tell() const;
    bool __cdecl Eof();
    unsigned long __cdecl Close();
    void __cdecl Commit();
    unsigned long __cdecl Size();
    void __cdecl SetBuffer(void* buffer);  // a 16 KB buffer the file does not own
    bool __cdecl IsOpen();
    bool __cdecl IsCompressed();
    const char* __cdecl Name() const;
    bool __cdecl GetStamp(Time& t);
    bool __cdecl SetStamp(Time& t);
    void __cdecl DetectEncoding();
    int __cdecl GetEncoding() const;  // 1 ANSI, 2 UTF-16
    void __cdecl SetEncoding(int encoding);
    static int __cdecl GetDefaultEncoding();  // 1
    static void __cdecl SetCallBack(bool(__cdecl* progress)(unsigned long done, unsigned long written));
    static void __cdecl SetLog(bool on);  // does nothing
    static bool __cdecl Cd(const char* path);
    static bool __cdecl GetCwd(Path& path);
    static bool __cdecl Rd(const char* path);
    static void __cdecl SetStamp(Time& t, const char* path);  // throws FileEx
    static bool __cdecl Remove(const char* path);
    static bool __cdecl Remove(const PString& base, const char* path);
    // the current position, read inline by the original's callers (+0x1c; x64 offset provisional)
    long Position() const { return static_cast<long>(position); }
    int Handle() const { return fd; }  // -1 when closed
    // the text encoding field (+0x34: 1 ANSI, 2 UTF-16), read inline by ConfigData's file writer
    int EncodingField() const { return encoding; }

    static const PString Sep;

private:
    friend class JIRFFile;
    friend class FileImpl;
    static bool(__cdecl* callback)(unsigned long, unsigned long);

    int fd;                    // +0x00 msvcr80 descriptor, -1 when closed
    char* cur;                 // +0x04 the next byte in the buffer
    char* buffer;              // +0x08 16 KB
    unsigned long count;       // +0x0c valid (read) or pending (write) bytes in the buffer
    unsigned long mode;        // +0x10
    unsigned long size;        // +0x14 the data size, -1 until known (0 after a failed read)
    unsigned long dataStart;   // +0x18 the "CFIL" header's length
    unsigned long position;    // +0x1c in the data
    unsigned long rawPos;      // +0x20 the next raw file offset
    unsigned long dirty;       // +0x24 written since the last flush
    unsigned char flags;       // +0x28 bit 0 owns the buffer, bit 1 the descriptor belongs to a FileSub
    FileSubstitute* subst;     // +0x2c the substitute it was opened through
    FileSub* sub;              // +0x30 the FileSub object holding it
    int encoding;              // +0x34
    Path path;                 // +0x38
};
JET_LAYOUT32(sizeof(File) == 0x13c);  // (the offsets are checked in File.cpp)

// A file's contents in memory: {current, start, size, owned, AnsiString name}. Unit U10a (MemoryFile.cpp).
class MemoryFile {
public:
    MemoryFile();
    MemoryFile(const char* name, unsigned int size);  // an owned empty block of size bytes
    MemoryFile(const void* data, unsigned int size, bool unicode, int encoding);
    MemoryFile(File& file, unsigned int size, bool unicode, int encoding);
    MemoryFile(File& file, bool unicode, int encoding);  // the rest of the file
    MemoryFile(File& file, const String& key);
    MemoryFile(const MemoryFile& o);  // shallow
    MemoryFile& operator=(const MemoryFile& o);
    ~MemoryFile();
    File::Result Open(const char* path, unsigned long mode, bool unicode, int encoding);
    File::Result Open(const PString& base, const char* path, unsigned long mode, bool unicode, int encoding);
    bool Open(File& file, unsigned int size, bool unicode, int encoding);
    bool Open(File& file, bool unicode, int encoding);
    bool Open(File& file, const String& key);
    bool Open(const void* data, unsigned int size, bool unicode, int encoding);
    void Open(unsigned int size);
    void Close();  // frees owned data, empties
    unsigned int Read(void* dst, unsigned int size, unsigned int count) const;  // short reads read nothing
    void Read(signed char& v) const;
    void Read(unsigned char& v) const;
    void Read(short& v) const;
    void Read(unsigned short& v) const;
    void Read(long& v) const;
    void Read(unsigned long& v) const;
    void Read(float& v) const;
    void Read(PString& s, bool unused) const;  // u32 length (bit 30: UTF-16), then the text
    signed char Readsbyte() const;
    unsigned char Readubyte() const;
    short Readsword() const;
    unsigned short Readuword() const;
    long Readsdword() const;
    unsigned long Readudword() const;
    float Readfloat() const;
    unsigned char Peek() const;
    void Write(const void* src, unsigned int size);
    unsigned int Seek(int offset, FileSeek whence) const;
    unsigned int SeekSet(int offset) const;
    unsigned int SeekCur(int offset) const;
    unsigned int SeekEnd(int offset) const;
    unsigned int Tell() const;
    unsigned int Size() const;
    bool Eof() const;
    const void* Data() const;
    const void* CurrentPosition() const;
    const char* Name() const;
    void SetName(const char* name);
    void SetName(const String& name);

    mutable const char* cur;  // +0x00
    const char* start;        // +0x04
    unsigned int size;        // +0x08
    bool owned;               // +0x0c
    AnsiString name;          // +0x10

protected:
    bool __cdecl ReadData(File& file, unsigned int size, bool unicode, int encoding);
    bool __cdecl ReadData(File& file, const String& key);
};
JET_LAYOUT32(sizeof(MemoryFile) == 0x14);

// ---- synchronisation objects (KernelStandard unit U4, SyncObjects.cpp). SyncObject {vptr, HANDLE}; vftable slots:
// deleting dtor, operator void*, Lock(timeout), Unlock(count), Unlock() (MSVC puts overloads in reverse order).
// Other modules import the constructors, so their objects carry KernelStandard's vftables. All are allocated
// through Jet::Alloc (their deleting destructors free with Alloc::Delete).
#define JET_HEAP_ARRAYS                                                                     \
    static void* operator new[](size_t n) { return Alloc::New(static_cast<unsigned int>(n)); } \
    static void operator delete[](void* p) { Alloc::Delete(p); }

class SyncObject {
public:
    JET_KS_IMPORT virtual ~SyncObject();
    JET_KS_IMPORT virtual __cdecl operator void*();
    JET_KS_IMPORT virtual bool __cdecl Lock(unsigned long timeout) const;  // true only for WAIT_OBJECT_0
    JET_KS_IMPORT virtual bool __cdecl Unlock() const;                     // base: false
    JET_KS_IMPORT virtual bool __cdecl Unlock(unsigned long count) const;  // base: false
    // tries Lock(0): true when that fails (a mutex owned by this thread counts as not locked)
    JET_KS_IMPORT bool __cdecl IsLocked();
    JET_KS_IMPORT bool __cdecl Signal();                       // Unlock()
    JET_KS_IMPORT bool __cdecl Wait(unsigned long timeout) const;  // Lock(timeout)
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
#if defined(JET_KS_IMPLEMENTATION)  // only KernelStandard itself (or the x64 stand-in) builds these objects
    JET_KS_IMPORT SyncObject();
#else
    SyncObject() = delete;
#endif
    void* handle;

private:
    JET_KS_IMPORT SyncObject(const SyncObject&);         // sets the vftable only; the handle is not copied
    JET_KS_IMPORT void operator=(const SyncObject&);     // does nothing
};

class Mutex : public SyncObject {
public:
    JET_KS_IMPORT Mutex(bool initiallyOwned = false, const char* name = nullptr);
    JET_KS_IMPORT virtual ~Mutex();
    JET_KS_IMPORT bool __cdecl Unlock() const override;                     // ReleaseMutex
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long count) const override;  // the same
};

class Semaphore : public SyncObject {
public:
    JET_KS_IMPORT Semaphore(int initialCount = 0, int maxCount = 1, const char* name = nullptr);
    JET_KS_IMPORT virtual ~Semaphore();
    JET_KS_IMPORT bool __cdecl Unlock() const override;                     // release 1
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long count) const override;  // release count
};

class WEvent : public SyncObject {
public:
    JET_KS_IMPORT WEvent(bool manualReset = false, bool initialState = false, const char* name = nullptr);
    JET_KS_IMPORT virtual ~WEvent();
    JET_KS_IMPORT bool __cdecl Unlock() const override;  // true, does nothing
    JET_KS_IMPORT void __cdecl Set();
    JET_KS_IMPORT void __cdecl Pulse();
    JET_KS_IMPORT void __cdecl Reset();
};

// {vptr, unused handle, CRITICAL_SECTION* from Jet::Alloc}; the name is ignored
class CriticalSection : public SyncObject {
public:
    JET_KS_IMPORT CriticalSection(const char* name = nullptr);
    JET_KS_IMPORT virtual ~CriticalSection();
    JET_KS_IMPORT bool __cdecl Lock(unsigned long timeout) const override;  // INFINITE: Enter, else TryEnter
    JET_KS_IMPORT bool __cdecl Unlock() const override;

private:
    void* section;  // +8
};

JET_LAYOUT32(sizeof(Mutex) == 8);
JET_LAYOUT32(sizeof(CriticalSection) == 0xc);

// locks a SyncObject (Lock(INFINITE)) for its lifetime: {SyncObject*, bool locked}
class SingleLock {
public:
    JET_KS_IMPORT SingleLock(const SyncObject* object, bool lockNow);
    JET_KS_IMPORT ~SingleLock();
    JET_KS_IMPORT SingleLock& operator=(const SingleLock& o);
    JET_KS_IMPORT bool __cdecl Lock(unsigned long timeout) const;
    JET_KS_IMPORT bool __cdecl Unlock() const;
    JET_KS_IMPORT bool __cdecl IsLocked() const;

private:
    const SyncObject* object;
    mutable bool locked;
};

// WaitForMultipleObjects over SyncObjects: {HANDLE* handles, bool* locked, count, SyncObject** objects}
class MultiLock {
public:
    // what Lock returns: 1 timeout, 0x10000000 + index acquired, 0x20000000 + index abandoned, 0 otherwise
    enum Result { FAILED = 0, TIMEOUT = 1, ACQUIRED = 0x10000000, ABANDONED = 0x20000000 };
    JET_KS_IMPORT MultiLock(SyncObject** objects, unsigned long count, bool lockNow);
    JET_KS_IMPORT ~MultiLock();
    JET_KS_IMPORT MultiLock& operator=(const MultiLock& o);
    JET_KS_IMPORT Result __cdecl Lock(unsigned long timeout, bool waitAll) const;
    JET_KS_IMPORT bool __cdecl Unlock() const;
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long index) const;  // false, does nothing
    JET_KS_IMPORT bool __cdecl IsLocked(unsigned long index) const;
    JET_HEAP_OBJECT

private:
    void** handles;
    bool* locked;
    unsigned long count;
    SyncObject** objects;
};

// single writer, many readers: {WEvent noReaders, Mutex writer, SyncObject* both[2], MultiLock*, int readers}
class SWMRGuard {
public:
    JET_KS_IMPORT SWMRGuard();
    JET_KS_IMPORT ~SWMRGuard();
    JET_KS_IMPORT bool __cdecl WaitToWrite(unsigned long timeout);
    JET_KS_IMPORT void DoneWriting();
    JET_KS_IMPORT bool __cdecl WaitToRead(unsigned long timeout);
    JET_KS_IMPORT void DoneReading();

private:
    WEvent noReaders;         // +0x00
    Mutex writer;             // +0x08
    SyncObject* both[2];      // +0x10: writer, noReaders
    MultiLock* lock;          // +0x18
    int readers;              // +0x1c
};
JET_LAYOUT32(sizeof(SWMRGuard) == 0x20);

// a Win32 thread (a SyncObject: waiting on it waits for the thread to end)
class Thread : public SyncObject {
public:
    enum Priority { PRIORITY_NORMAL = 0, PRIORITY_1 = 1, PRIORITY_2 = 2 };  // only the values used are named
    using Function = unsigned int(__stdcall*)(void*);
    JET_KS_IMPORT Thread(Function fn, void* arg, bool start);
    JET_KS_IMPORT virtual ~Thread();
    JET_KS_IMPORT virtual __cdecl operator void*();
    JET_KS_IMPORT bool __cdecl Resume();
    JET_KS_IMPORT void __cdecl SetPriority(Priority p);
    JET_KS_IMPORT bool __cdecl IsDone();
    JET_KS_IMPORT static unsigned long __cdecl GetCurrentId();
    JET_KS_IMPORT static unsigned long __cdecl GetProcessId();
    static void __cdecl GetProcessName(char* out);

private:
    friend class ResourceSystem;  // compares the caller with the main thread
    static JET_KS_IMPORT unsigned int mainThreadId;
    unsigned char opaque[JET_OPAQUE_SIZE(0x18, 0x28)];
};

// A dynamically loaded module: {vptr, handle}. Users construct and destroy it inline (Kernel_7c 0x1000afd0:
// handle = 0; at the end "if (handle) Close()"), so only the virtual members are imported; we call them by
// qualified name and keep our own vftable copy, like the original.
// Slot order: deleting destructor, Open, Close, Import.
class DynamicLibrary {
public:
#if defined(JET_KS_BUILD)
    DynamicLibrary();
    DynamicLibrary(const DynamicLibrary& o);
    DynamicLibrary& operator=(const DynamicLibrary& o);
    virtual ~DynamicLibrary();
#else
    DynamicLibrary() : handle(nullptr) {}
    virtual ~DynamicLibrary()
    {
        if (handle) DynamicLibrary::Close();
    }
#endif
    JET_KS_IMPORT virtual bool __cdecl Open(const char* path);
    JET_KS_IMPORT virtual void __cdecl Close();
    JET_KS_IMPORT virtual bool __cdecl Import(void** proc, const char* name);
    static bool __cdecl GetModuleNameAndAddress(void* address, String& name, void*& base);  // always false
    JET_HEAP_OBJECT

private:
    void* handle;
};
JET_LAYOUT32(sizeof(DynamicLibrary) == 8);

// A named page-file mapping shared between processes (0x10 bytes): {data, name, size, mapping}.
class SharedMemory {
public:
    SharedMemory(const char* name, unsigned int size);
    ~SharedMemory();
    SharedMemory& operator=(const SharedMemory& o);
    bool ProcessAttach();  // creates or opens the mapping and maps it
    bool ProcessDetach();
    void* Data();
    bool TryExclusiveLock();  // always false
    void Unlock();            // does nothing

private:
    void* data;
    const char* name;
    unsigned int size;
    void* mapping;
};
JET_LAYOUT32(sizeof(SharedMemory) == 0x10);

// Jet's text console (0x1c bytes); only what Jobs uses.
class Console {
public:
    void __cdecl Clear();
    void __cdecl Printf(const char* format, ...);
    void __cdecl BlitToMono44(int x, int y, unsigned char* screen) const;

    int width;               // +0x00
    int height;              // +0x04
    unsigned char* buffer;   // +0x08
    int unknown0c;
    int unknown10;
    int unknown14;
    int unknown18;
};
JET_LAYOUT32(sizeof(Console) == 0x1c);

class Jobs;

// A unit of work for the Jobs thread (0x28 bytes). Process runs on that thread, Dispatch on the thread calling
// Jobs::Update once it is done. A job made with a parent holds the parent until its own Dispatch.
class Job {
public:
    explicit Job(Job* parent = nullptr);
    Job(const Job& o);
    Job& operator=(const Job& o);
    virtual ~Job();
    virtual const char* GetName() const;  // "Unknown"
    float GetProgress() const;
    JET_HEAP_OBJECT

protected:
    virtual void __cdecl Process();
    virtual void __cdecl Dispatch();

private:
    friend class Jobs;
    template <class>
    friend struct ListOps;
    void SetProgress(float p);
    void Lock();
    void Unlock();
    bool IsDone() const;
    void __cdecl StartProcessing();
    void __cdecl EndProcessing();

    ListLink link;              // +0x04
    float progress;             // +0x0c
    Job* parent;                // +0x10
    unsigned long locks;        // +0x14 children not dispatched yet
    unsigned long created;      // +0x18 timeGetTime
    unsigned long started;      // +0x1c
    unsigned long ended;        // +0x20
    unsigned long unknown24;    // +0x24 (set with the others, never read)
};
JET_LAYOUT32(sizeof(Job) == 0x28);

// The job thread (0xa8 bytes). Add queues a job; the thread processes jobs in order; a job with children still
// pending waits until they are dispatched; Update (main thread) dispatches and deletes the finished ones.
class Jobs {
public:
    Jobs();
    void __cdecl Init(Options* options);  // option "JobsConsole": text consoles drawn into 0xb0000
    void __cdecl Done();
    void __cdecl Update();
    void __cdecl Add(Job* job);
    bool IsActive() const;

private:
    static unsigned int __stdcall Main(void* jobs);
    void Run();
    void Finish(Job* job);
    void ShowFinished();
    void ShowQueue();

    unsigned long unknown00;          // +0x00
    unsigned long unknown04;          // +0x04
    LinkedList<Job> queue;            // +0x08
    Mutex queueLock;                  // +0x14
    Semaphore queued;                 // +0x1c
    LinkedList<Job> finished;         // +0x24
    Mutex finishedLock;               // +0x30
    Semaphore finishedCount;          // +0x38
    LinkedList<Job> pending;          // +0x40 processed, waiting for children (thread only)
    Thread* thread;                   // +0x4c
    Console console;                  // +0x50 status
    Console queueConsole;             // +0x6c
    Console finishedConsole;          // +0x88
    bool consoleOn;                   // +0xa4
    bool active;                      // +0xa5
};
JET_LAYOUT32(sizeof(Jobs) == 0xa8);

// ---- I/O slaves (byte streams; KernelStandard unit U9b, IOSlave.cpp). vftable: deleting dtor, Seek, Tell, GetSize,
// ReadData, WriteData. Deleting destructors free with Jet::Alloc.
class IOSlaveReadCursor;
class IOSlave {
public:
    JET_KS_IMPORT IOSlave();
    JET_KS_IMPORT IOSlave(const IOSlave& o);
    JET_KS_IMPORT IOSlave& operator=(const IOSlave& o);
    JET_KS_IMPORT virtual ~IOSlave();
    virtual bool Seek(unsigned long pos) = 0;
    virtual unsigned long Tell() = 0;
    JET_KS_IMPORT virtual unsigned long GetSize();                                   // 0
    JET_KS_IMPORT virtual unsigned long ReadData(void* dst, unsigned long size);         // 0
    JET_KS_IMPORT virtual unsigned long WriteData(const void* src, unsigned long size);  // 0 (= ReadData's code)
    JET_KS_IMPORT bool WriteByte(unsigned char v);
    JET_KS_IMPORT bool WriteUINT32(unsigned long v);
    JET_KS_IMPORT bool ReadUINT32(unsigned long& v);
    JET_KS_IMPORT unsigned long Write(IOSlaveReadCursor& src, unsigned long size);  // in 4 KB pieces
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS
};

// a fixed memory block {data, position, size} (no data: size 0)
class IOSlaveMemory : public IOSlave {
public:
    JET_KS_IMPORT IOSlaveMemory(void* data, unsigned long size);
    JET_KS_IMPORT IOSlaveMemory(const IOSlaveMemory& o);
    JET_KS_IMPORT IOSlaveMemory& operator=(const IOSlaveMemory& o);
    JET_KS_IMPORT virtual ~IOSlaveMemory();
    JET_KS_IMPORT bool Seek(unsigned long pos) override;
    JET_KS_IMPORT unsigned long Tell() override;
    JET_KS_IMPORT unsigned long GetSize() override;
    JET_KS_IMPORT unsigned long ReadData(void* dst, unsigned long size) override;  // a null dst skips
    JET_KS_IMPORT unsigned long WriteData(const void* src, unsigned long size) override;
    IOSlave* AsSlave() { return this; }

private:
    unsigned char* data;     // +0x04
    unsigned long position;  // +0x08
    unsigned long size;      // +0x0c
};
JET_LAYOUT32(sizeof(IOSlaveMemory) == 0x10);

// a growing memory stream over an MSVC8 std::vector<char> {allocator, first, last, end} and a position
class IOSlaveMemoryDynamic : public IOSlave {
public:
    IOSlaveMemoryDynamic();
    IOSlaveMemoryDynamic(const IOSlaveMemoryDynamic& o);
    IOSlaveMemoryDynamic& operator=(const IOSlaveMemoryDynamic& o);
    virtual ~IOSlaveMemoryDynamic();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;
    unsigned long WriteData(const void* src, unsigned long size) override;
    const char* GetData() const;  // null when empty

    struct Vector {
        unsigned char allocator[4];  // (empty std::allocator<char>)
        char* first;
        char* last;
        char* end;
    };

private:
    Vector bytes;            // +0x04
    unsigned long position;  // +0x14
};
JET_LAYOUT32(sizeof(IOSlaveMemoryDynamic) == 0x18);

class File;
// an open File (a closed one counts as none) and its size when made (grown by writes)
class IOSlaveFile : public IOSlave {
public:
    explicit IOSlaveFile(File* file);
    IOSlaveFile(const IOSlaveFile& o);
    IOSlaveFile& operator=(const IOSlaveFile& o);
    virtual ~IOSlaveFile();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;  // a null dst skips
    unsigned long WriteData(const void* src, unsigned long size) override;

private:
    File* file;          // +0x04
    unsigned long size;  // +0x08
};
JET_LAYOUT32(sizeof(IOSlaveFile) == 0xc);

// Reads a slave through a buffer {slave, stream position of the buffer, buffer, offset in it, bytes in it,
// capacity}; Access(offset, n) makes n bytes at the cursor + offset contiguous (null at the end, which also lets
// the slave go).
class IOSlaveReadCursor {
public:
    IOSlaveReadCursor();
    explicit IOSlaveReadCursor(IOSlave* slave);
    IOSlaveReadCursor(const IOSlaveReadCursor& o);  // the same slave at the same place, no buffer
    ~IOSlaveReadCursor();
    void operator=(IOSlave* slave);
    void operator=(const IOSlaveReadCursor& o);
    void operator++();
    void operator++(int);
    void operator+=(unsigned long n);
    void Inc(unsigned long n);
    unsigned char operator*();
    unsigned char operator[](unsigned long offset);
    unsigned char AccessByte(unsigned long offset);
    unsigned long AccessUInt32(unsigned long offset);
    const void* Access(unsigned long offset, unsigned long n);
    int GetIndex(const void* p);  // the offset of a pointer into the buffer from the cursor

    IOSlave* slave;           // +0x00
    unsigned long position;   // +0x04
    unsigned char* buffer;    // +0x08
    unsigned long offset;     // +0x0c
    unsigned long filled;     // +0x10
    unsigned long capacity;   // +0x14
};
JET_LAYOUT32(sizeof(IOSlaveReadCursor) == 0x18);

// Writes through to a slave, keeping the last 2 x chunk bytes written in two buffers for WriteCopy (an LZ copy
// from distance -offset).
class IOSlaveWriteCursor : public IOSlave {
public:
    IOSlaveWriteCursor(IOSlave* slave, unsigned long chunk);
    virtual ~IOSlaveWriteCursor();
    unsigned long WriteData(const void* src, unsigned long size) override;
    bool ReadData(void* dst, int offset, unsigned long size);  // from the history, else from the slave
    unsigned long WriteCopy(int offset, unsigned long size);

protected:
    IOSlaveWriteCursor(const IOSlaveWriteCursor& o);  // (the vftable only)
    void operator=(const IOSlaveWriteCursor& o);    // (nothing)
    bool Seek(unsigned long pos) override;            // false
    unsigned long Tell() override;                    // 0
    unsigned long ReadData(void* dst, unsigned long size) override;  // 0

private:
    IOSlave* slave;           // +0x04
    unsigned char* buffers[2];  // +0x08
    unsigned long chunk;      // +0x10
    unsigned long fill[2];    // +0x14
    unsigned long current;    // +0x1c
};
JET_LAYOUT32(sizeof(IOSlaveWriteCursor) == 0x20);

// A scrambled stream inside another slave: a 12-byte header {version (1), key word, size} then the bytes, each
// scrambled by its position, the key word and the key text (CheckRead / CheckWrite). Written ones get their header
// and 4 KB padding when destroyed. An empty key passes everything through.
class IOSlaveResource : public IOSlave {
public:
    IOSlaveResource(IOSlave* slave, const String& key, bool write);
    IOSlaveResource(const IOSlaveResource& o);
    IOSlaveResource& operator=(const IOSlaveResource& o);
    virtual ~IOSlaveResource();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;
    unsigned long WriteData(const void* src, unsigned long size) override;

protected:
    unsigned char CheckWrite(unsigned long pos, unsigned char v);
    unsigned char CheckRead(unsigned long pos, unsigned char v);

private:
    IOSlave* slave;           // +0x04 (null when the header failed)
    unsigned long base;       // +0x08 the slave position after the header
    AnsiString key;           // +0x0c
    bool write;               // +0x10
    unsigned long version;    // +0x14
    unsigned long keyWord;    // +0x18
    unsigned long position;   // +0x1c
    unsigned long size;       // +0x20
};
JET_LAYOUT32(sizeof(IOSlaveResource) == 0x24);

// A reader of "name { ... }" block text files, line by line through File::ReadLine {file, line buffer, token buffer,
// cursor, buffer size, PString name, parent, child, end, block pending}; children share the parent's buffers.
class BlockReader {
public:
    BlockReader();
    explicit BlockReader(File& file);
    BlockReader(const BlockReader& o);
    BlockReader& operator=(const BlockReader& o);
    ~BlockReader();
    void __cdecl Init(File& file);
    BlockReader* __cdecl Begin();
    void __cdecl End();
    bool __cdecl Next(PString& name);
    bool __cdecl ReadString(char* out, int size);
    bool __cdecl ReadString(PString& out);
    int __cdecl ReadStringv(PString* out, int count);
    bool __cdecl ReadInt(int& v);
    int __cdecl ReadIntv(int* out, int count);
    bool __cdecl ReadFloat(float& v);
    int __cdecl ReadFloatv(float* out, int count);
    bool __cdecl ReadBool(bool& v);

protected:
    explicit BlockReader(BlockReader* parent);
    bool __cdecl SkipBlock();
    bool __cdecl ParseEntry();
    bool __cdecl ParseString();
    bool __cdecl IsBlockStart();
    static bool __cdecl SkipToToken(const char*& p);
    static bool __cdecl SkipWhiteSpace(const char*& p);
    static bool __cdecl SkipNonWhiteSpace(const char*& p);
    static bool __cdecl IsEol(const char* p);
    static bool __cdecl IsComment(const char* p);

private:
    File* file;            // +0x00
    char* line;            // +0x04
    char* token;           // +0x08
    const char* cursor;    // +0x0c
    unsigned long bufferSize;  // +0x10
    PString name;          // +0x14
    BlockReader* parent;   // +0x18
    BlockReader* child;    // +0x1c
    bool ended;            // +0x20
    bool pending;          // +0x21
};
JET_LAYOUT32(sizeof(BlockReader) == 0x24);

// Byte-pair encoding (Philip Gage's, blocks of up to 2 KB / 210 distinct bytes); Comp falls back to a copy when
// the output reaches the input's size.
class BPE {
public:
    static unsigned long __cdecl Comp(void* dst, const void* src, unsigned long size);
    static unsigned long __cdecl Decomp(void* dst, const void* src, unsigned long size);
};

// JIRF chunk files (KernelStandard unit U9c, JIRF.cpp): a File read and written through an IOSlaveFile and an
// IOSlaveResource (scrambled when a key is given). Header {"JIRF", size, id}, blocks {id, size, data}; sizes are
// patched in when the writer objects go.
class JIRFFile : public File {
public:
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;
    JIRFFile();
    JIRFFile(const JIRFFile& o);  // (shares the slaves)
    JIRFFile& operator=(const JIRFFile& o);
    ~JIRFFile();
    Result __cdecl OpenJIRF(const char* path, const String& key, unsigned long mode);
    Result __cdecl OpenJIRF(const PString& base, const char* path, const String& key, unsigned long mode);
    void __cdecl Close();
    unsigned long __cdecl Write(const void* src, unsigned long size);
    unsigned long __cdecl Read(void* dst, unsigned long size);
    bool __cdecl Read32(unsigned int& v);  // (any byte read counts)
    unsigned long __cdecl SeekCur(long offset);  // -1 on failure
    unsigned long __cdecl Seek(long position);
    unsigned long __cdecl Tell() const;
    unsigned int __cdecl WriteHeader(unsigned long id);  // the size field's position
    unsigned int __cdecl WriteBlock(unsigned long id, unsigned int size, void* data);
    Result __cdecl ReadBlock(unsigned long id, unsigned int& size, void* data);
    Result __cdecl ReadBlockHeader(unsigned long& id, unsigned int& size);
    Result __cdecl ReadHeader(unsigned long id);
    Result __cdecl ReadHeader(unsigned long id, unsigned int& size);
    void __cdecl WriteAt(unsigned int position, unsigned long value);
    void __cdecl WriteAt(unsigned int position);  // the size from there to here
    void __cdecl WriteSizeAt(unsigned int position);
    void __cdecl WritePadded(const PString& s);
    void __cdecl WritePadded(const String& s);
    Result __cdecl ReadPadded(PString& s);
    Result __cdecl ReadPadded(String& s);

private:
    IOSlaveFile* slaveFile;     // +0x13c
    IOSlaveResource* resource;  // +0x140
};
JET_LAYOUT32(sizeof(JIRFFile) == 0x144);

// writes the chunk size at `position` when it goes
class JIRFWriteSize {
protected:
    explicit JIRFWriteSize(JIRFFile& file);
    ~JIRFWriteSize();
    JIRFFile* file;
    unsigned int position;

private:
    const JIRFWriteSize& __cdecl operator=(const JIRFWriteSize& o);  // (nothing)
};
class JIRFWriteHeader : public JIRFWriteSize {
public:
    JIRFWriteHeader(JIRFFile& file, unsigned long id);
    ~JIRFWriteHeader();
};
class JIRFWriteBlock : public JIRFWriteSize {
public:
    JIRFWriteBlock(JIRFFile& file, unsigned long id);
    ~JIRFWriteBlock();
};

// ---- reference counting (KernelStandard). Only the out-of-line members are imported; like the original
// Kernel_7c, our module keeps its own copy of the vftable.
class ReferenceCount {
public:
    ReferenceCount() : referenceCount(1) {}
    JET_KS_IMPORT void AddReference() const;
    JET_KS_IMPORT bool ReleaseReference() const;
    int GetReferenceCount() const { return referenceCount; }

protected:
    JET_KS_IMPORT virtual void __cdecl DeleteThis() const;
    JET_KS_IMPORT virtual ~ReferenceCount();

private:
    mutable int referenceCount;
};

// (FastDynamicArray.h includes this header)
template <class A, class B>
struct Less;
template <class T, unsigned int SIZ, class L>
class FastDynamicArray;

// A per-thread scratch buffer (0x14 bytes) that grows to the largest size asked; GetAccess hands out the calling
// thread's (with a reference), making it on first use.
class SharedScratchPad : public ReferenceCount {
public:
    static SharedScratchPad* __cdecl GetAccess();
    unsigned char* __cdecl GetBuffer(unsigned int size);
    JET_HEAP_OBJECT

protected:
    explicit SharedScratchPad(unsigned long threadId);
    ~SharedScratchPad() override;

private:
    unsigned long threadId;  // +0x08
    unsigned char* buffer;   // +0x0c
    unsigned int capacity;   // +0x10

protected:
    static CriticalSection lock;
    static FastDynamicArray<SharedScratchPad*, 13, Less<SharedScratchPad*, SharedScratchPad*>> scratchPadArray;
};

}  // namespace Jet
