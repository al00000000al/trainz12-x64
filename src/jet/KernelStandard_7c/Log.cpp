// Unit U12a of KernelStandard_7c: LogClient and Log (0x1002ee80..0x1002fd65). The state is the original's: the
// exported statics are imported (clientList, logFile, stderrFile, startTime, writeLock, initialized,
// displayModuleName, LogClient::disabled) and the one private flag, LogFlush (0x1007287b), is read and written in
// the original module by address. Its allocator and strings (U14) still log through the same clients and file.
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <windows.h>

#include <cstring>
#include <new>

#include <xmmintrin.h>

namespace Jet {

struct LogWork {
    static_assert(offsetof(LogClient, level) == 0x10 && offsetof(LogClient, busy) == 0x18 &&
                  offsetof(LogClient, outputs) == 0x1c && offsetof(LogClient, format) == 0x20);
    struct Ring {  // IList<LogClient>: the head stands in as the end of the ring
        LogClient* next;
        LogClient* prev;
        int count;
    };
    static Ring* List() { return reinterpret_cast<Ring*>(Log::clientList); }
    static LogClient* End() { return reinterpret_cast<LogClient*>(Log::clientList); }

    // the original's private "LogFlush" option (commit after every line)
    static bool& Flush()
    {
        static bool* const p =
            reinterpret_cast<bool*>(reinterpret_cast<char*>(GetModuleHandleA("KernelStandard_7c_orig.dll")) + 0x7287b);
        return *p;
    }

    // an AnsiString's buffer {capacity, length, text}: no text when the capacity is 0 (the shared empty buffer)
    static const int* Data(const AnsiString& s) { return static_cast<const int*>(s.data); }
    static const char* Text(const AnsiString& s)
    {
        const int* d = Data(s);
        return d[0] ? reinterpret_cast<const char*>(d + 2) : nullptr;
    }
    static unsigned int Length(const AnsiString& s) { return static_cast<unsigned int>(Data(s)[1]); }

    // the original's inline File delete (closed again if still open, an owned buffer freed)
    static void Destroy(File* f)
    {
        f->~File();
        Alloc::Delete(f);
    }

    static Options::Node* Find(const Options* o, const char* key) { return o->Find(key); }
    static void ResetLogEntry(Options::Node* n)
    {
        static_cast<AnsiString&>(n->separators) = ",";
        n->cursor = Text(n->value);
    }
};

// ---- LogClient

LogClient::LogClient(const char* name_) : next(nullptr), prev(nullptr)
{
    busy = false;
    name = name_;
    level = 2;
    outputs = static_cast<unsigned char>((outputs & 0xf3) | 3);
    format = static_cast<unsigned char>((((Log::displayModuleName * 2) ^ format) & 2 ^ format) | 1);
}

void __cdecl LogClient::OutputString(const char* fmt, ...)
{
    OutputStringV(fmt, reinterpret_cast<char*>(&fmt + 1));
}

// "<kind> <minutes>:<seconds> : " (when the time prefix is on), the text, CRLF (when line breaks are on); then the
// file (under the write lock, once at a time per client), the debugger, the stdout/stderr stream
void __cdecl LogClient::OutputStringV(const char* fmt, char* args)
{
    AnsiString text;
    float elapsed = 0.0f;
    if (Log::startTime > 0.0) elapsed = static_cast<float>(Time::GetSeconds() - Log::startTime);
    const float m = elapsed * 0.016666668f - 0.5f;
    const int minutes = _mm_cvt_ss2si(_mm_set_ss(m));  // fistp, round to nearest
    const float minutesF = static_cast<float>(minutes);
    const float seconds = elapsed - minutesF * 60.0f;
    if (format & 2)
        text.Setf("%c %3.0f:%04.1f : ", static_cast<int>(" !?+-"[kind]), static_cast<double>(minutesF),
                  static_cast<double>(seconds));
    text.Add(fmt, args);
    if (format & 1) text.Ins("\r\n", LogWork::Length(text));
    if ((outputs & 1) && Log::logFile) {
        if (Log::writeLock) Log::writeLock->Lock(INFINITE);
        if (Log::logFile && Log::logFile->Handle() != -1 && !busy) {
            busy = true;
            Log::logFile->WriteLine(LogWork::Text(text), 0);
            if (LogWork::Flush()) Log::logFile->Commit();
            busy = false;
        }
        if (Log::writeLock) Log::writeLock->Unlock();
    }
    if (outputs & 2) OutputDebugStringA(LogWork::Text(text));
    if (Log::stderrFile) {
        const char* t = LogWork::Text(text);
        Crt80::vfprintf(Log::stderrFile, "%s", reinterpret_cast<va_list>(&t));
    }
}

// ---- Log

Log& Log::operator=(const Log&) { return *this; }

LogClient* __cdecl Log::GetClient(const char* name)
{
    if (!clientList) {
        auto* r = static_cast<LogWork::Ring*>(Alloc::New(sizeof(LogWork::Ring)));
        if (r) {
            r->prev = reinterpret_cast<LogClient*>(r);
            r->next = reinterpret_cast<LogClient*>(r);
            r->count = 0;
        }
        clientList = reinterpret_cast<IList<LogClient>*>(r);
    }
    for (LogClient* c = LogWork::List()->next; c != LogWork::End(); c = c->next)
        if (Crt80::_stricmp(LogWork::Text(c->name), name) == 0) return c;
    return CreateClient(name);
}

// the name's own length must match: the ':' that ends it in an option line is not part of the comparison
LogClient* __cdecl Log::SearchClientList(const char* name)
{
    const size_t n = std::strlen(name);
    for (LogClient* c = LogWork::List()->next; c != LogWork::End(); c = c->next)
        if (LogWork::Length(c->name) == n && Crt80::_strnicmp(LogWork::Text(c->name), name, n) == 0) return c;
    return CreateClient(name);
}

LogClient* __cdecl Log::CreateClient(const char* name)
{
    void* p = Alloc::New(sizeof(LogClient));
    LogClient* c = p ? ::new (p) LogClient(name) : nullptr;
    LogWork::Ring* r = LogWork::List();
    c->next = LogWork::End();
    c->prev = r->prev;
    r->prev->next = c;
    ++r->count;
    r->prev = c;
    return c;
}

// "<client>:<setting>:<setting>...": Err / Warn / Diag / Prog / None set the level, "<output>[=on|=off]" switches
// mono (the debugger), window, newlines, displaymodulename, file
void __cdecl Log::Process(String& line)
{
    AnsiString client = line.Token(":");
    if (!LogWork::Length(client)) return;
    LogClient* c = SearchClientList(LogWork::Text(client));
    AnsiString tok = line.Token(":");
    while (LogWork::Length(tok)) {
        if (LogWork::Data(tok)[0] && Crt80::_stricmp(LogWork::Text(tok), "Err") == 0)
            c->level = 1;
        else if (tok.EqualsIgnoreCase("Warn"))
            c->level = 2;
        else if (tok.EqualsIgnoreCase("Diag"))
            c->level = 3;
        else if (tok.EqualsIgnoreCase("Prog"))
            c->level = 4;
        else if (tok.EqualsIgnoreCase("None"))
            c->level = 0;
        else {
            unsigned char on = 1;
            AnsiString key = tok.Token("=");
            AnsiString value = tok.Token(":");
            if (LogWork::Length(value) && !value.EqualsIgnoreCase("=on") && value.EqualsIgnoreCase("=off")) on = 0;
            if (key.EqualsIgnoreCase("mono"))
                c->outputs ^= ((on * 2) ^ c->outputs) & 2;
            else if (key.EqualsIgnoreCase("window"))
                c->outputs ^= ((on * 8) ^ c->outputs) & 8;
            else if (key.EqualsIgnoreCase("newlines"))
                c->format ^= (c->format ^ on) & 1;
            else if (key.EqualsIgnoreCase("displaymodulename"))
                c->format ^= ((on * 2) ^ c->format) & 2;
            else if (key.EqualsIgnoreCase("file"))
                c->outputs ^= (c->outputs ^ on) & 1;
        }
        tok = line.Token(":");
    }
}

void __cdecl Log::Init(Options* options)
{
    if (initialized) return;
    void* p = Alloc::New(sizeof(CriticalSection));
    writeLock = p ? ::new (p) CriticalSection("Log") : nullptr;
    startTime = Time::GetSeconds();
    String fileName;
    options->Get("LogFlush", LogWork::Flush(), LogWork::Flush());
    options->Get("LogDisplayModuleName", displayModuleName, displayModuleName);
    const int ansi = LogWork::Find(options, "LogAnsi") ? 1 : 0;
    p = Alloc::New(sizeof(File));
    logFile = p ? ::new (p) File() : nullptr;
    {
        AnsiString fallback("JetLog.txt");
        options->Get("LogFile", fileName, static_cast<const String&>(fallback));
    }
    bool keep = false;
    if (!LogWork::Length(fileName)) {
        if (logFile) LogWork::Destroy(logFile);
    } else {
        if (logFile->Open(LogWork::Text(fileName), 0x101, ansi).code == 0) {
            if (logFile->EncodingField() == 2) {
                unsigned int bom = 0xfeff;
                logFile->Write(&bom, 1, 2);
            }
            if (!LogWork::Find(options, "NoLog")) {
                logFile->WriteLine("Begin:\r\n", 0);
                logFile->Commit();
                keep = true;
            } else {
                logFile->WriteLine("Log Disabled.\r\n", 0);
                logFile->Close();
            }
        }
        if (!keep) {
            if (logFile) LogWork::Destroy(logFile);
            LogClient::disabled = true;
        }
    }
    if (!keep) logFile = nullptr;

    if (LogWork::Find(options, "LogStdOut")) {
        stderrFile = reinterpret_cast<_iobuf*>(Crt80::__iob_func() + 0x20);
        if (stderrFile) LogClient::disabled = false;
    }
    if (LogWork::Find(options, "LogStdErr")) {
        stderrFile = reinterpret_cast<_iobuf*>(Crt80::__iob_func() + 0x40);
        if (stderrFile) LogClient::disabled = false;
    }
    for (LogClient* c = LogWork::List()->next; c != LogWork::End(); c = c->next)
        c->format ^= ((displayModuleName * 2) ^ c->format) & 2;
    if (auto* n = LogWork::Find(options, "Log")) LogWork::ResetLogEntry(n);
    String setting;
    while (options->GetNext("Log", setting)) Process(setting);
    initialized = true;
}

void __cdecl Log::SetLogToFile(_iobuf* file)
{
    stderrFile = file;
    if (file) LogClient::disabled = false;
}

_iobuf* __cdecl Log::GetLogToFile() { return stderrFile; }

void __cdecl Log::Done()
{
    initialized = false;
    if (writeLock) delete writeLock;
    writeLock = nullptr;
    if (logFile) {
        logFile->WriteLine("End.\r\n", 0);
        logFile->Close();
        if (logFile) LogWork::Destroy(logFile);
        logFile = nullptr;
    }
    if (LogWork::Ring* r = LogWork::List()) {
        while (r->count) {  // 0x1002fcc0: the clients are unlinked from the front, their strings released
            LogClient* c = r->next;
            c->prev->next = c->next;
            c->next->prev = c->prev;
            c->prev = nullptr;
            c->next = nullptr;
            --r->count;
            c->unused.~AnsiString();
            c->name.~AnsiString();
            Alloc::Delete(c);
        }
        Alloc::Delete(r);
    }
    clientList = nullptr;
}

void __cdecl Log::Shutdown(bool quiet)
{
    if (!logFile || logFile->Handle() == -1) return;
    if (writeLock) writeLock->Lock(INFINITE);
    if (!quiet) {
        logFile->WriteLine("Shutdown.\r\n", 0);
        logFile->Close();
    }
    if (logFile) LogWork::Destroy(logFile);
    logFile = nullptr;
    if (writeLock) writeLock->Unlock();
}

}  // namespace Jet
