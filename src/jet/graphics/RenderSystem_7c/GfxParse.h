// Unit U2b2 of RenderSystem_7c: the machinery shared by the four shader-language front ends (notes/RenderSystem.md):
// a flex 2.5.4 C++ scanner skeleton with REJECT (FlexScanner, 0x1002e680.. and 0x10045f70..0x10046240 shared by all
// four), an old-style Lemon parser engine (LemonParser<Traits>, one copy per language in the original) and the
// chunked list of names the parsers fill (0x1002cd80 / 0x1002ce20). All of it is internal to the module.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/PString.h>

#include <cstddef>
#include <cstdint>

namespace Jet {

// a circular list of chunks {next, prev, count, 13 names} (0x40 bytes each); the head is the first chunk
struct GfxNameList {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        PStringCache::Node* names[13];
    };
    Chunk* head = nullptr;

    void Append(const PString& name);  // 0x1002cd80: a reference to the name at the end
    void Clear();                      // 0x1002ce20: the second chunk first, the head last
};

// the same chunks holding ints, used as a stack (0x1000d530 push, 0x1002c1a0 pop)
struct GfxIntStack {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        int items[13];
    };
    Chunk* head = nullptr;

    void Push(int v);
    int Pop();     // from the last chunk, which goes when it empties (an empty stack would read address 0)
    void Clear();  // the second chunk first, the head last
};

// The rule actions' state: module statics in the original (0x100b7dc8..0x100b809f), shared by every scanner
// instance and kept between runs. Laid out the same way, so an overrun lands in the same neighbours. Names follow
// the addresses; what each flag does is in the actions.
struct GfxLexStatics {
    unsigned char pp7dc8[100];  // 0x100b7dc8 per #if level
    unsigned char pp7e2c;       // 0x100b7e2c
    unsigned char pad7e2d[3];
    int ppStack[20];            // 0x100b7e30 the pre-parser's start conditions
    unsigned char pp7e80[104];  // 0x100b7e80 per #if level
    unsigned char pp7ee8[100];  // 0x100b7ee8 per #if level
    int unknown7f4c;            // 0x100b7f4c
    int qkStack[20];            // 0x100b7f50 the quick parser's start conditions
    int tpStack[40];            // 0x100b7fa0 the texture program scanner's (one rule reads the entry below the top,
                                // so an empty stack reads qkStack[19])
    unsigned char unknown8040[3];
    unsigned char pp8043;       // 0x100b8043
    unsigned char unknown8044[12];
    int ppDepth;                // 0x100b8050
    int ppCurrent;              // 0x100b8054
    int pp8058;                 // 0x100b8058 code/native block depth
    int pp805c;                 // 0x100b805c brace depth
    unsigned char pp8060, pp8061, pp8062, pp8063, pp8064;
    unsigned char pad8065[2];
    unsigned char tpNative;     // 0x100b8067 the API section is native (its text is collected whole)
    int ppIfDepth;              // 0x100b8068
    int qkDepth;                // 0x100b806c
    int qkCurrent;              // 0x100b8070
    int qkBraces;               // 0x100b8074
    unsigned char unknown8078[0x18];
    int tpDepth;                // 0x100b8090
    int tpCurrent;              // 0x100b8094
    int tpBraces;               // 0x100b8098 brace depth inside an API section
    int tpBlock;                // 0x100b809c brace depth of a code or native block
};
static_assert(offsetof(GfxLexStatics, ppStack) == 0x68 && offsetof(GfxLexStatics, qkStack) == 0x188 &&
                  offsetof(GfxLexStatics, tpStack) == 0x1d8 && offsetof(GfxLexStatics, pp8043) == 0x27b &&
                  offsetof(GfxLexStatics, ppDepth) == 0x288 && offsetof(GfxLexStatics, ppIfDepth) == 0x2a0 &&
                  offsetof(GfxLexStatics, tpNative) == 0x29f && offsetof(GfxLexStatics, tpDepth) == 0x2c8 &&
                  sizeof(GfxLexStatics) == 0x2d8,
              "the original's layout");
extern GfxLexStatics gfxLex;

// ---------------------------------------------------------------------------------------------------------------
// flex

struct FlexTables {
    const unsigned short* accList;
    const unsigned short* accept;
    const unsigned char* ec;
    const unsigned int* meta;
    const unsigned short* base;
    const unsigned short* def;
    const unsigned short* nxt;
    const unsigned short* chk;
    int jamState;
    int endOfBufferRule;   // YY_END_OF_BUFFER; the end-of-file rules follow, one per start condition
    bool highFromEc;       // yy_get_previous_state reads chars >= 0x7f as ec[0x7e] (else as class 1)
};

// The scanner state of yyFlexLexer (0x78 bytes in the original). Scan() runs the DFA and hands back the action
// of the next match (yytext/yyleng set, line counting done); the end-of-buffer action is handled inside.
class FlexScanner {
public:
    FlexScanner(const FlexTables& tables, MemoryFile* in);
    virtual ~FlexScanner();

protected:
    struct Buffer {  // yy_buffer_state (0x28)
        MemoryFile* input;
        char* chars;
        char* pos;
        int size;
        int count;
        int ours;
        int interactive;
        int atBol;
        int fill;
        int status;  // 0 new, 1 normal, 2 EOF pending
    };

    int Scan();
    void Unput(int c, char* bp);
    void Output(const char* buf, int size);  // ECHO (nobody sets yyout)
    [[noreturn]] static void Error(const char* message);
    char* CopyText(const char* s) const;  // strdup through Jet::Alloc (null stays null)

    char* yytext = nullptr;  // +4
    int yyleng = 0;          // +8
    int yylineno = 1;        // +0xc
    int start = 0;           // +0x3c

private:
    int Next(int state, int cls) const;
    int Input(char* buf, int max);
    Buffer* CreateBuffer(MemoryFile* in, int size);
    void InitBuffer(Buffer* b, MemoryFile* in);
    void LoadBufferState();
    void Restart(MemoryFile* in);
    int GetNextBuffer();
    int PreviousState();
    int TryNulTransition(int state);

    const FlexTables& t;
    MemoryFile* yyin;               // +0x20
    MemoryFile* yyout = nullptr;    // +0x24
    Buffer* current = nullptr;      // +0x28
    char holdChar = 0;              // +0x2c
    int nChars = 0;                 // +0x30
    char* cBufP = nullptr;          // +0x34
    int init = 1;                   // +0x38
    int switchOnEof = 0;            // +0x40
    int* stateBuf;                  // +0x4c (Jet::Alloc, 0x10008)
    int* statePtr = nullptr;        // +0x50
    int lp = 0;                     // +0x60
};

// ---------------------------------------------------------------------------------------------------------------
// Lemon

struct LemonAction {
    unsigned char lookahead, action;
    short next;  // index in the pool, -1 at the end of the chain
};
struct LemonState {
    short block;  // first slot in the pool, -1 none
    unsigned mask;
    unsigned char defaultAction;
};
struct LemonRule {
    unsigned char lhs, nrhs;
};

struct LemonEntry {
    int state;
    int major;
    void* minor;
};
// the parser's stack (0x4bc, Jet::Alloc): index of the top, error countdown, top, 100 entries
struct LemonStack {
    int index;
    int errorCount;
    LemonEntry* top;
    LemonEntry entries[100];
};
JET_LAYOUT32(sizeof(LemonStack) == 0x4bc);

// Traits: kNoCode, kStates, kRules, kErrorSymbol, kTerminals (minors of majors 1..kTerminals above 0x100 are
// freed), pool[], states[], rules[], Reduce(LemonStack*, int rule, Ctx*, void*& gotoMinor), Failed(Ctx*) and
// SyntaxError(Ctx*). Actions: below kStates shift, then the rules, then error, accept, and (lookups only) none.
template <class Traits, class Ctx>
struct LemonParser {
    static constexpr int kError = Traits::kStates + Traits::kRules;
    static constexpr int kAccept = kError + 1;
    static constexpr int kNone = kError + 2;

    static LemonStack* New()
    {
        auto* p = static_cast<LemonStack*>(Alloc::New(sizeof(LemonStack)));
        if (p) p->index = -1;
        return p;
    }
    static void Destroy(LemonEntry* e)
    {
        if (static_cast<unsigned char>(e->major - 1) < Traits::kTerminals &&
            reinterpret_cast<uintptr_t>(e->minor) > 0x100) {
            if (e->minor) Alloc::Delete(e->minor);
            e->minor = nullptr;
        }
    }
    static void PopAll(LemonStack* p)
    {
        while (p->index >= 0) {
            Destroy(p->top);
            --p->index;
            --p->top;
        }
    }
    static void Free(LemonStack* p)  // the stack and what is left on it
    {
        if (!p) return;
        PopAll(p);
        Alloc::Delete(p);
    }
    static void Push(LemonStack* p, int state, int major, void* const* minor)
    {
        ++p->index;
        ++p->top;
        if (p->index >= 100) {  // overflow: everything is dropped
            --p->index;
            --p->top;
            PopAll(p);
            return;
        }
        p->top->state = state;
        p->top->major = major;
        p->top->minor = *minor;
    }
    // the action of `state` on `la`; false for the "no code" lookahead on a state that does not end with it
    static bool Find(int state, int la, int& action)
    {
        const LemonState& s = Traits::states[state];
        if (la == Traits::kNoCode) {
            // (a state without actions would make the original read address 0)
            if (s.mask != 0 || s.block < 0 || Traits::pool[s.block].lookahead != Traits::kNoCode) return false;
        } else if (s.block >= 0) {
            for (int e = s.block + static_cast<int>(s.mask & static_cast<unsigned>(la)); e >= 0;
                 e = Traits::pool[e].next)
                if (Traits::pool[e].lookahead == la) {
                    action = Traits::pool[e].action;
                    return true;
                }
        }
        action = s.defaultAction;
        return true;
    }
    static int FindGoto(LemonStack* p, int la)
    {
        int action;
        return Find(p->top->state, la, action) ? action : kNone;
    }
    static void Reduce(LemonStack* p, int rule, Ctx* ctx)
    {
        void* gotoMinor = p;  // left as it is by rules without a value (the original's stack slot)
        Traits::Reduce(p, rule, ctx, gotoMinor);
        const int lhs = Traits::rules[rule].lhs;
        const int n = Traits::rules[rule].nrhs;
        p->index -= n;
        p->top -= n;
        const int action = FindGoto(p, lhs);
        if (action < Traits::kStates)
            Push(p, action, lhs, &gotoMinor);
        else if (action == kAccept)
            PopAll(p);
    }
    static void Parse(LemonStack* p, int major, void* minor, Ctx* ctx)
    {
        int errorHit = 0;
        if (p->index < 0) {
            if (major == 0) return;
            p->top = p->entries;
            p->index = 0;
            p->errorCount = -1;
            p->top->state = 0;
            p->top->major = 0;
        }
        const bool endOfInput = major == 0;
        void* value = minor;
        do {
            int action;
            if (!Find(p->top->state, major, action)) {
                PopAll(p);
                return;
            }
            if (action < Traits::kStates) {
                Push(p, action, major, &value);
                --p->errorCount;
                if (!endOfInput || p->index < 0) return;
                major = 0;
            } else if (action < kError) {
                Reduce(p, action - Traits::kStates, ctx);
                if (major == Traits::kNoCode) return;
            } else if (action == kError) {
                if (p->errorCount < 0) Traits::SyntaxError(ctx);  // "Syntax error at %s." (formatted, never shown)
                if (p->top->major == Traits::kErrorSymbol || errorHit) {
                    DropToken(major, value);
                    major = Traits::kNoCode;
                } else {
                    int recover = kError;
                    while (p->index >= 0) {
                        LemonEntry* e = p->top;
                        if (e->major == Traits::kErrorSymbol) break;
                        recover = FindGoto(p, Traits::kErrorSymbol);
                        if (recover < Traits::kStates) break;
                        Destroy(e);
                        --p->index;
                        --p->top;
                    }
                    if (p->index < 0 || major == 0) {
                        DropToken(major, value);
                        PopAll(p);
                        Traits::Failed(ctx);
                        major = Traits::kNoCode;
                    } else if (p->top->major != Traits::kErrorSymbol) {
                        void* none = nullptr;
                        Push(p, recover, Traits::kErrorSymbol, &none);
                    }
                }
                p->errorCount = 3;
                errorHit = 1;
                if (major == Traits::kNoCode) return;
            } else {
                PopAll(p);
                return;
            }
        } while (p->index >= 0);
    }

private:
    static void DropToken(int major, void*& value)
    {
        const unsigned char m = static_cast<unsigned char>(major);
        if (m != 0 && m <= Traits::kTerminals && reinterpret_cast<uintptr_t>(value) > 0x100) {
            if (value) Alloc::Delete(value);
            value = nullptr;
        }
    }
};

}  // namespace Jet
