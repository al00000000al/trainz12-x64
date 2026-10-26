// Unit U2b2 of RenderSystem_7c: the shared scanner skeleton and the name list. See GfxParse.h.
#include "GfxParse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// GfxNameList

void GfxNameList::Append(const PString& name)
{
    Chunk* c;
    if (!head) {
        c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        c->count = 0;
        head = c;
        c->prev = c;
        c->next = c;
    } else {
        Chunk* const last = head->prev;
        c = last;
        if (last->count == 13) {
            c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
            c->count = 0;
            c->next = head;
            c->prev = last;
            head->prev = c;
            last->next = c;
        }
    }
    const int i = c->count++;
    PStringCache::Node* const n = name.node;
    if (n) {
        PStringCache* const cache = PString::Cache();
        cache->Lock();
        ++n->refs;
        ++cache->liveRefs;
        cache->Unlock();
    }
    c->names[i] = n;
}

void GfxNameList::Clear()
{
    Chunk* const first = head;
    if (!first) return;
    head = nullptr;
    auto release = [](Chunk* c) {
        for (int i = 0; i < c->count; ++i)
            if (c->names[i] && PString::Cache()) PString::Cache()->Destroy(c->names[i]);
        Alloc::Delete(c);
    };
    Chunk* c = first->next;
    while (c != first) {
        Chunk* const next = c->next;
        release(c);
        c = next;
    }
    release(first);
}

// ---------------------------------------------------------------------------------------------------------------
// GfxIntStack

void GfxIntStack::Push(int v)
{
    Chunk* c;
    if (!head) {
        c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        c->count = 0;
        head = c;
        c->prev = c;
        c->next = c;
    } else {
        Chunk* const last = head->prev;
        c = last;
        if (last->count == 13) {
            c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
            c->count = 0;
            c->next = head;
            c->prev = last;
            head->prev = c;
            last->next = c;
        }
    }
    c->items[c->count++] = v;
}

int GfxIntStack::Pop()
{
    Chunk* const last = head->prev;
    const int v = last->items[--last->count];
    if (last->count == 0) {
        if (last == head) {
            Alloc::Delete(last);
            head = nullptr;
        } else {
            last->prev->next = head;
            head->prev = last->prev;
            Alloc::Delete(last);
        }
    }
    return v;
}

void GfxIntStack::Clear()
{
    Chunk* const first = head;
    if (!first) return;
    Chunk* c = first->next;
    while (c != first) {
        Chunk* const next = c->next;
        Alloc::Delete(c);
        c = next;
    }
    Alloc::Delete(first);
    head = nullptr;
}

GfxLexStatics gfxLex;

// ---------------------------------------------------------------------------------------------------------------
// FlexScanner (the flex 2.5.4 C++ skeleton, REJECT variant)

namespace {
const int kBufSize = 0x4000;
int S(const unsigned short* t, int i) { return static_cast<short>(t[i]); }
}  // namespace

FlexScanner::FlexScanner(const FlexTables& tables, MemoryFile* in) : t(tables), yyin(in)
{
    stateBuf = static_cast<int*>(Alloc::New(0x10008));
}

FlexScanner::~FlexScanner()
{
    if (stateBuf) Alloc::Delete(stateBuf);
    if (Buffer* const b = current) {
        current = nullptr;
        if (b->ours) std::free(b->chars);
        std::free(b);
    }
}

int FlexScanner::Next(int state, int cls) const
{
    while (S(t.chk, S(t.base, state) + cls) != state) {
        state = S(t.def, state);
        if (state > t.jamState) cls = static_cast<signed char>(t.meta[cls]);
    }
    return S(t.nxt, S(t.base, state) + cls);
}

// LexerInput (0x1002e680): from the MemoryFile {current, start, size}
int FlexScanner::Input(char* buf, int max)
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

void FlexScanner::Output(const char* buf, int size)  // LexerOutput (0x10045f30)
{
    MemoryFile* const f = yyout;
    if (f && reinterpret_cast<unsigned>(f->cur) + size <= f->size + reinterpret_cast<unsigned>(f->start)) {
        std::memcpy(const_cast<char*>(f->cur), buf, static_cast<size_t>(size));
        f->cur += size;
    }
}

void FlexScanner::Error(const char* message)  // LexerError (0x1002e830): stderr, exit(2)
{
    std::fprintf(stderr, "%s\n", message);
    std::exit(2);
}

char* FlexScanner::CopyText(const char* s) const
{
    if (!s) return nullptr;
    const size_t n = std::strlen(s) + 1;
    char* const p = static_cast<char*>(Alloc::New(static_cast<unsigned>(n)));
    std::memcpy(p, s, n);
    return p;
}

FlexScanner::Buffer* FlexScanner::CreateBuffer(MemoryFile* in, int size)  // 0x1002f780
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

void FlexScanner::InitBuffer(Buffer* b, MemoryFile* in)  // 0x10046240 (yy_flush_buffer inlined)
{
    b->count = 0;
    b->chars[0] = 0;
    b->chars[1] = 0;
    b->pos = b->chars;
    b->atBol = 1;
    b->status = 0;
    if (b == current) LoadBufferState();
    b->interactive = 0;
    b->input = in;
    b->fill = 1;
}

void FlexScanner::LoadBufferState()
{
    nChars = current->count;
    yytext = cBufP = current->pos;
    yyin = current->input;
    holdChar = *cBufP;
}

void FlexScanner::Restart(MemoryFile* in)  // yyrestart (0x100461f0)
{
    if (!current) current = CreateBuffer(yyin, kBufSize);
    InitBuffer(current, in);
    LoadBufferState();
}

// 0x10045f70: 0 continue scanning, 1 end of file, 2 last match
int FlexScanner::GetNextBuffer()
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

int FlexScanner::PreviousState()
{
    int state = start;
    statePtr = stateBuf;
    *statePtr++ = state;
    for (const char* cp = yytext; cp < cBufP; ++cp) {
        const unsigned char c = static_cast<unsigned char>(*cp);
        int cls;
        if (c >= 0x7f)
            cls = t.highFromEc ? static_cast<signed char>(t.ec[0x7e]) : 1;
        else
            cls = c == 0 ? 1 : static_cast<signed char>(t.ec[c]);
        state = Next(state, cls);
        *statePtr++ = state;
    }
    return state;
}

int FlexScanner::TryNulTransition(int state)
{
    state = Next(state, 1);
    if (state == t.jamState) return 0;
    *statePtr++ = state;
    return state;
}

void FlexScanner::Unput(int c, char* bp)
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

// the matching half of yylex: the action of the next match
int FlexScanner::Scan()
{
    if (init) {
        init = 0;
        if (!start) start = 1;
        if (!current) current = CreateBuffer(yyin, kBufSize);
        LoadBufferState();
    }
    char* cp = cBufP;
    *cp = holdChar;
    char* bp = cp;
    int state = start;
    statePtr = stateBuf;
    *statePtr++ = state;
    for (;;) {
    match:
        do {
            const unsigned char c = static_cast<unsigned char>(*cp);
            state = Next(state, static_cast<signed char>(t.ec[c > 0x7e ? 0x7e : c]));
            ++cp;
            *statePtr++ = state;
        } while (state != t.jamState);
    findAction:
        --statePtr;
        for (;;) {
            state = *statePtr;
            lp = S(t.accept, state);
            if (lp && lp < S(t.accept, state + 1)) break;
            --cp;
            --statePtr;
        }
        const int act = S(t.accList, lp);
        yytext = bp;
        yyleng = static_cast<int>(cp - bp);
        holdChar = *cp;
        *cp = 0;
        cBufP = cp;
        if (act != t.endOfBufferRule) {
            for (int i = 0; i < yyleng; ++i)
                if (yytext[i] == '\n') ++yylineno;
            return act;
        }
        // end of buffer
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
            cBufP = yytext;
            switchOnEof = 0;
            return (start - 1) / 2 + t.endOfBufferRule + 1;  // YY_STATE_EOF (yywrap() is always 1)
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
            return 0;  // (not reached)
        }
    }
}

}  // namespace Jet
