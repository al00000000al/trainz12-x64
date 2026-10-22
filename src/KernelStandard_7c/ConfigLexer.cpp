// ConfigParser's scanner. See ConfigLexer.h. The DFA tables are the original's (scfg_*.h, read from the installed
// module at build time); this file is the flex 2.5.4 skeleton and the rule actions.
#include "ConfigLexer.h"

#include "Crt80.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// generated from the installed original (CMakeLists.txt, tools/extract_data.py)
#include "scfg_accept.h"
#include "scfg_acclist.h"
#include "scfg_base.h"
#include "scfg_chk.h"
#include "scfg_def.h"
#include "scfg_ec.h"
#include "scfg_meta.h"
#include "scfg_nxt.h"

namespace Jet {

namespace {

const unsigned short accList[] = {SCFG_ACCLIST_DATA};
const unsigned short accept[] = {SCFG_ACCEPT_DATA};
const unsigned char ec[] = {SCFG_EC_DATA};
const unsigned int meta[] = {SCFG_META_DATA};
const unsigned short base[] = {SCFG_BASE_DATA};
const unsigned short def[] = {SCFG_DEF_DATA};
const unsigned short nxt[] = {SCFG_NXT_DATA};
const unsigned short chk[] = {SCFG_CHK_DATA};

const int kJamState = 0xcb;
const int kEndOfBufferRule = 0x52;  // YY_END_OF_BUFFER
const int kBufSize = 0x4000;
const char kEmpty[] = "";  // what rule 45 points yytext at (0x100515c4)

int S(const unsigned short* t, int i) { return static_cast<short>(t[i]); }

// one DFA step from `state` on character class `c`
int Next(int state, int c)
{
    int cls = c;
    while (S(chk, S(base, state) + cls) != state) {
        state = S(def, state);
        if (state > kJamState) cls = static_cast<signed char>(meta[cls]);
    }
    return S(nxt, S(base, state) + cls);
}

int ClassOf(unsigned char c)  // the main loop's: chars above 0x7e read as 0x7e
{
    return static_cast<signed char>(ec[c > 0x7e ? 0x7e : c]);
}

int Buf(const AnsiString& s, int field)  // an AnsiString's {capacity, length}
{
    return (*reinterpret_cast<const int* const*>(&s))[field];
}
const char* BufText(const AnsiString& s) { return reinterpret_cast<const char*>(*reinterpret_cast<const int* const*>(&s) + 2); }

}  // namespace

// 0x1001a500 / 0x10019190
ConfigLexer::ConfigLexer(MemoryFile* in, const PString& n) : name(n), yyin(in)
{
    stateBuf = static_cast<int*>(Alloc::New(0x10008));
}

ConfigLexer::~ConfigLexer()  // 0x1001a5d0 / 0x10019250
{
    if (stateBuf) Alloc::Delete(stateBuf);
    if (Buffer* const b = current) {
        current = nullptr;
        if (b->ours) std::free(b->chars);
        std::free(b);
    }
    if (stack) std::free(stack);
}

// 0x10019300: from the MemoryFile {current, start, size}
int ConfigLexer::Input(char* buf, int max)
{
    MemoryFile* const f = yyin;
    if (!f || static_cast<unsigned>(f->cur - f->start) >= f->size) return 0;
    int n = static_cast<int>(f->size + reinterpret_cast<unsigned>(f->start) - reinterpret_cast<unsigned>(f->cur));
    if (n > max) n = max;
    if (reinterpret_cast<unsigned>(f->cur) + n <= f->size + reinterpret_cast<unsigned>(f->start)) {
        std::memcpy(buf, f->cur, static_cast<size_t>(n));
        f->cur += n;
    }
    return n;
}

void ConfigLexer::Output(const char* buf, int size)  // 0x10019360 (ECHO; nobody sets yyout)
{
    MemoryFile* const f = yyout;
    if (f && reinterpret_cast<unsigned>(f->cur) + size <= f->size + reinterpret_cast<unsigned>(f->start)) {
        std::memcpy(const_cast<char*>(f->cur), buf, static_cast<size_t>(size));
        f->cur += size;
    }
}

void ConfigLexer::Error(const char* message)  // 0x10019880: to msvcr80's stderr, then exit(2)
{
    using IobFunc = FILE*(__cdecl*)();
    using Fprintf = int(__cdecl*)(FILE*, const char*, ...);
    using Exit = void(__cdecl*)(int);
    FILE* const iob = reinterpret_cast<IobFunc>(Crt80::Proc("__iob_func"))();
    reinterpret_cast<Fprintf>(Crt80::Proc("fprintf"))(reinterpret_cast<FILE*>(reinterpret_cast<char*>(iob) + 2 * 0x20),
                                                       "%s\n", message);
    reinterpret_cast<Exit>(Crt80::Proc("exit"))(2);
    std::exit(2);
}

ConfigLexer::Buffer* ConfigLexer::CreateBuffer(MemoryFile* in, int size)  // 0x10019770
{
    Buffer* const b = static_cast<Buffer*>(std::malloc(sizeof(Buffer)));
    if (!b) Error("out of dynamic memory in yy_create_buffer()");
    b->size = size;
    b->chars = static_cast<char*>(std::malloc(static_cast<size_t>(size + 2)));
    if (!b->chars) Error("out of dynamic memory in yy_create_buffer()");
    b->ours = 1;
    InitBuffer(b, in);
    return b;
}

void ConfigLexer::DeleteBuffer(Buffer* b)  // 0x100197e0
{
    if (!b) return;
    if (b == current) current = nullptr;
    if (b->ours) std::free(b->chars);
    std::free(b);
}

void ConfigLexer::FlushBuffer(Buffer* b)
{
    if (!b) return;
    b->count = 0;
    b->chars[0] = 0;
    b->chars[1] = 0;
    b->pos = b->chars;
    b->atBol = 1;
    b->status = 0;
    if (b == current) LoadBufferState();
}

void ConfigLexer::InitBuffer(Buffer* b, MemoryFile* in)  // 0x10019820
{
    FlushBuffer(b);
    b->input = in;
    b->fill = 1;
    b->interactive = 0;
}

void ConfigLexer::LoadBufferState()
{
    nChars = current->count;
    yytext = cBufP = current->pos;
    yyin = current->input;
    holdChar = *cBufP;
}

void ConfigLexer::Restart(MemoryFile* in)  // 0x100196c0
{
    if (!current) current = CreateBuffer(yyin, kBufSize);
    InitBuffer(current, in);
    LoadBufferState();
}

// 0x100193a0: 0 continue scanning, 1 end of file, 2 last match
int ConfigLexer::GetNextBuffer()
{
    char* dest = current->chars;
    char* source = yytext;
    if (cBufP > &current->chars[nChars + 1]) Error("fatal flex scanner internal error--end of buffer missed");
    if (current->fill == 0) return cBufP - yytext == 1 ? 1 : 2;
    const int move = static_cast<int>(cBufP - yytext) - 1;
    for (int i = 0; i < move; ++i) *dest++ = *source++;
    if (current->status == 2) {
        nChars = 0;
        current->count = 0;
    } else {
        int toRead = current->size - move - 1;
        while (toRead <= 0) Error("input buffer overflow, can't enlarge buffer because scanner uses REJECT");
        if (toRead > 0x2000) toRead = 0x2000;
        nChars = Input(&current->chars[move], toRead);
        if (nChars < 0) Error("input in flex scanner failed");
        current->count = nChars;
    }
    int result;
    if (nChars == 0) {
        if (move == 0) {
            result = 1;
            Restart(yyin);
        } else {
            result = 2;
            current->status = 2;
        }
    } else {
        result = 0;
    }
    nChars += move;
    current->chars[nChars] = 0;
    current->chars[nChars + 1] = 0;
    yytext = current->chars;
    return result;
}

int ConfigLexer::PreviousState()  // 0x100194d0
{
    int state = start;
    statePtr = stateBuf;
    *statePtr++ = state;
    for (const char* cp = yytext; cp < cBufP; ++cp) {
        const unsigned char c = static_cast<unsigned char>(*cp);
        const int cls = c == 0 || c >= 0x7f ? 1 : static_cast<signed char>(ec[c]);
        state = Next(state, cls);
        *statePtr++ = state;
    }
    return state;
}

int ConfigLexer::TryNulTransition(int state)  // 0x10019590
{
    state = Next(state, 1);
    if (state == kJamState) return 0;
    *statePtr++ = state;
    return state;
}

void ConfigLexer::Unput(int c, char* bp)  // 0x10019620
{
    char* cp = cBufP;
    *cp = holdChar;
    if (cp < current->chars + 2) {
        char* dest = &current->chars[current->size + 2];
        char* source = &current->chars[nChars + 2];
        while (source > current->chars) *--dest = *--source;
        cp += dest - source;
        bp += dest - source;
        nChars = current->size;
        current->count = current->size;
        if (cp < current->chars + 2) Error("flex scanner push-back overflow");
    }
    *--cp = static_cast<char>(c);
    if (c == '\n') --yylineno;
    yytext = bp;
    holdChar = *cp;
    cBufP = cp;
}

void ConfigLexer::PushState(int condition)
{
    if (stackPtr >= stackDepth) {
        stackDepth += 25;
        const size_t size = static_cast<size_t>(stackDepth) * sizeof(int);
        stack = static_cast<int*>(stack ? std::realloc(stack, size) : std::malloc(size));
        if (!stack) Error("out of memory expanding start-condition stack");
    }
    stack[stackPtr++] = (start - 1) / 2;
    start = 1 + 2 * condition;
}

void ConfigLexer::PopState()
{
    if (--stackPtr < 0) Error("start-condition stack underflow");
    start = 1 + 2 * stack[stackPtr];
}

char* ConfigLexer::CopyText(const char* s) const
{
    if (!s) return nullptr;
    const size_t n = std::strlen(s) + 1;
    char* const p = static_cast<char*>(Alloc::New(static_cast<unsigned>(n)));
    std::memcpy(p, s, n);
    return p;
}

char* ConfigLexer::TakeBuffer() { return Buf(buffer, 0) ? CopyText(BufText(buffer)) : nullptr; }

// 0x10017530 yylex
int ConfigLexer::Lex()
{
    if (init) {
        init = 0;
        if (!start) start = 1;
        if (!current) current = CreateBuffer(yyin, kBufSize);
        LoadBufferState();
    }
    for (;;) {
        char* cp = cBufP;
        *cp = holdChar;
        char* bp = cp;
        int state = start;
        statePtr = stateBuf;
        *statePtr++ = state;
        int act;
    match:
        do {
            state = Next(state, ClassOf(static_cast<unsigned char>(*cp)));
            ++cp;
            *statePtr++ = state;
        } while (state != kJamState);
    findAction:
        --statePtr;
        for (;;) {
            state = *statePtr;
            lp = S(accept, state);
            if (lp && lp < S(accept, state + 1)) break;
            --cp;
            --statePtr;
        }
        act = S(accList, lp);
        yytext = bp;
        yyleng = static_cast<int>(cp - bp);
        holdChar = *cp;
        *cp = 0;
        cBufP = cp;
        if (act != kEndOfBufferRule)
            for (int i = 0; i < yyleng; ++i)
                if (yytext[i] == '\n') ++yylineno;
    doAction:
        switch (act) {
        case 1: case 3: case 5: case 7: case 41: case 49: case 51: case 77: case 78: case 79:
            break;
        case 2: case 39: case 10: case 47:
            PushState(1);
            break;
        case 4:
            PushState(2);
            return VERSION;
        case 6: case 8: case 11: case 40: case 42: case 48: case 50: case 75: case 76:
            ++line;
            break;
        case 9: case 53:
            PopState();
            break;
        case 12: return START;
        case 13: return END;
        case 14: return OPEN;
        case 15: return CLOSE;
        case 16: return SC;
        case 17: return COMMA;
        case 18: return ASSIGN;
        case 19:
            buffer = kEmpty;
            PushState(5);
            return RAWASSIGN;
        case 20: value = CopyText(yytext); return FLOAT;
        case 21: value = CopyText(yytext); return FLOATQNAN;
        case 22: value = CopyText(yytext); return FLOATINF;
        case 23: value = CopyText(yytext); return FLOATIND;
        case 24: value = CopyText(yytext); return INT;
        case 25: {  // a quoted string: the quotes dropped
            char* const p = CopyText(yytext + 1);
            value = p;
            char* const end = p + std::strlen(p);
            end[-1] = 0;
            return STRING;
        }
        case 26: case 27: case 28:
            buffer = yytext;
            PushState(3);
            break;
        case 29: case 30:
            Unput(yytext[0], yytext);
            buffer = kEmpty;
            PushState(3);
            break;
        case 31: case 32: case 33: case 34: case 35: {  // the bare string ends at = ; { ( )
            static const char ends[] = "=;{()";
            PopState();
            Unput(ends[act - 31], yytext);
            value = TakeBuffer();
            return STRING;
        }
        case 36: case 37:
            ++line;
            PushState(4);
            break;
        case 38:
            PushState(4);
            break;
        case 43:
            Unput(yytext[0], yytext);
            PopState();
            value = TakeBuffer();
            return STRING;
        case 44:
            PopState();
            Unput(';', yytext);
            value = TakeBuffer();
            return HEXSTRING;
        case 45:
            yytext = const_cast<char*>(kEmpty);
            break;
        case 46: case 54: case 74:
            buffer.Ins(yytext, static_cast<unsigned>(Buf(buffer, 1)));
            break;
        case 52:
            PopState();
            buffer = kEmpty;
            if (value) Alloc::Delete(value);
            value = nullptr;
            return -1;
        case 55:
            buffer.Ins(yytext + 1, static_cast<unsigned>(Buf(buffer, 1)));
            ++line;
            break;
        case 56: case 57: case 58: case 59: case 60: {  // escapes: \" \n \r \t \b
            static const char chars[] = "\"\n\r\t\b";
            const char s[2] = {chars[act - 56], 0};
            buffer.Ins(s, static_cast<unsigned>(Buf(buffer, 1)));
            break;
        }
        case 61: case 62: case 63: case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72:
        case 73: {  // any other escaped character stands for itself
            const char s[2] = {yytext[1], 0};
            buffer.Ins(s, static_cast<unsigned>(Buf(buffer, 1)));
            break;
        }
        case 80:
            return -2;
        case 81:
            Output(yytext, yyleng);
            break;
        case kEndOfBufferRule: {
            const int matched = static_cast<int>(cp - yytext) - 1;
            *cp = holdChar;
            if (current->status == 0) {
                nChars = current->count;
                current->input = yyin;
                current->status = 1;
            }
            if (cBufP <= &current->chars[nChars]) {
                cBufP = yytext + matched;
                state = PreviousState();
                const int next = TryNulTransition(state);
                bp = yytext;
                if (next) {
                    cp = ++cBufP;
                    state = next;
                    goto match;
                }
                cp = cBufP;
                goto findAction;
            }
            switch (GetNextBuffer()) {
            case 1:
                switchOnEof = 0;
                cBufP = yytext;
                act = (start - 1) / 2 + 0x53;  // YY_STATE_EOF (yywrap() is always 1)
                goto doAction;
            case 0:
                cBufP = yytext + matched;
                state = PreviousState();
                cp = cBufP;
                bp = yytext;
                goto match;
            case 2:
                cBufP = &current->chars[nChars];
                state = PreviousState();
                cp = cBufP;
                bp = yytext;
                goto findAction;
            default:
                break;
            }
            break;
        }
        case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x58:
            return 0;
        default:
            Error("fatal flex scanner internal error--no action found");
        }
    }
}

}  // namespace Jet
