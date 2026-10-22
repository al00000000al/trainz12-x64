// InputSystem_7c's key binding scanner (InputLex: 0x100025e0..0x100027ef, the flex skeleton 0x100101a0..0x10010858,
// yylex 0x1000fc80). See Lexer.h.
#include "Lexer.h"

#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "InputLexTables.h"  // generated from the installed original at build time (CMakeLists.txt)

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Jet {

namespace {

// the original's tables (yy_acclist .. yy_chk); shorts are read signed like the original does
const unsigned short accList[] = {ICFG_ACCLIST_DATA};
const unsigned short accept[] = {ICFG_ACCEPT_DATA};
const unsigned char ec[] = {ICFG_EC_DATA};
const unsigned int meta[] = {ICFG_META_DATA};
const unsigned short base[] = {ICFG_BASE_DATA};
const unsigned short def[] = {ICFG_DEF_DATA};
const unsigned short nxt[] = {ICFG_NXT_DATA};
const unsigned short chk[] = {ICFG_CHK_DATA};

int S(const unsigned short* t, int i) { return static_cast<short>(t[i]); }
int Meta(int c) { return static_cast<signed char>(meta[c]); }

enum { JAM_STATE = 0x83, LAST_DFA = 0x83, END_OF_BUFFER = 0x24, BUFFER_SIZE = 0x4000, READ_SIZE = 0x2000 };
enum { BUFFER_NEW = 0, BUFFER_NORMAL = 1, BUFFER_EOF_PENDING = 2 };
enum { EOB_CONTINUE_SCAN = 0, EOB_END_OF_FILE = 1, EOB_LAST_MATCH = 2 };

// one transition (the main loop's form)
int Step(int state, int c)
{
    while (S(chk, S(base, state) + c) != state) {
        state = S(def, state);
        if (state > LAST_DFA) c = Meta(c);
    }
    return S(nxt, S(base, state) + c);
}

}  // namespace

// a list (kind >= 4) has its count in the first dword of its data; a count of 0, or kind 0, takes the first value
void JoinValues(const ConfigData& entry, String& out)
{
    out = "";
    const PString* values = entry.GetPStringArray();
    if (!values) return;
    int count = 0;
    const int kind = entry.Kind();
    if (kind != 0) count = kind < 4 ? 1 : entry.Data()[0];
    if (count == 0) {
        out = values[0];
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (i) out += " ";
        out += values[i];
    }
}

InputLex::InputLex(const char* text, unsigned long size)  // 0x100025e0 / 0x10002730
{
    stateBuf = static_cast<int*>(Alloc::New((BUFFER_SIZE + 2) * sizeof(int)));
    source.cur = source.base = text;
    source.size = size;
    DeleteBuffer(current);
    SwitchToBuffer(CreateBuffer(&source, BUFFER_SIZE));
}

InputLex::~InputLex()  // 0x10010260
{
    if (stateBuf) Alloc::Delete(stateBuf);
    if (Buffer* b = current) {
        current = nullptr;
        if (b->isOurBuffer) std::free(b->chBuf);
        std::free(b);
    }
}

// ---- buffers
InputLex::Buffer* InputLex::CreateBuffer(void* file, int size)  // 0x10010720
{
    auto* b = static_cast<Buffer*>(std::malloc(sizeof(Buffer)));
    if (!b) LexerError("out of dynamic memory in yy_create_buffer()");
    b->bufSize = size;
    b->chBuf = static_cast<char*>(std::malloc(static_cast<size_t>(size) + 2));
    if (!b->chBuf) LexerError("out of dynamic memory in yy_create_buffer()");
    b->isOurBuffer = 1;
    InitBuffer(b, file);
    return b;
}

void InputLex::DeleteBuffer(Buffer* b)  // 0x10010790
{
    if (!b) return;
    if (b == current) current = nullptr;
    if (b->isOurBuffer) std::free(b->chBuf);
    std::free(b);
}

void InputLex::FlushBuffer(Buffer* b)
{
    if (!b) return;
    b->nChars = 0;
    b->chBuf[0] = 0;
    b->chBuf[1] = 0;
    b->bufPos = b->chBuf;
    b->atBol = 1;
    b->bufferStatus = BUFFER_NEW;
    if (b == current) LoadBufferState();
}

void InputLex::InitBuffer(Buffer* b, void* file)  // 0x100107d0
{
    FlushBuffer(b);
    b->isInteractive = 0;
    b->inputFile = file;
    b->fillBuffer = 1;
}

void InputLex::LoadBufferState()
{
    nChars = current->nChars;
    yytext = cBufP = current->bufPos;
    yyin = current->inputFile;
    holdChar = *cBufP;
}

void InputLex::SwitchToBuffer(Buffer* b)  // 0x100106c0
{
    if (current == b) return;
    if (current) {
        *cBufP = holdChar;
        current->bufPos = cBufP;
        current->nChars = nChars;
    }
    current = b;
    LoadBufferState();
}

// ---- input
// 0x10010350: from the {cursor, base, size} block
int InputLex::LexerInput(char* buf, int maxSize)
{
    auto* src = static_cast<Source*>(yyin);
    if (!src || static_cast<unsigned long>(src->cur - src->base) >= src->size) return 0;
    const int left = static_cast<int>(src->base + src->size - src->cur);
    if (left < maxSize) maxSize = left;
    if (src->cur + maxSize <= src->base + src->size) {
        std::memcpy(buf, src->cur, static_cast<size_t>(maxSize));
        src->cur += maxSize;
    }
    return maxSize;
}

void InputLex::LexerOutput(const char*, int) {}  // 0x100103b0: to yyout, which is never set

void InputLex::LexerError(const char* msg)  // 0x10010830
{
    std::fprintf(stderr, "%s\n", msg);
    std::exit(2);
}

int InputLex::GetNextBuffer()  // 0x100103f0
{
    char* dest = current->chBuf;
    char* src = yytext;
    if (cBufP > current->chBuf + nChars + 1) LexerError("fatal flex scanner internal error--end of buffer missed");
    if (!current->fillBuffer) return cBufP - yytext == 1 ? EOB_END_OF_FILE : EOB_LAST_MATCH;
    const int toMove = static_cast<int>(cBufP - yytext) - 1;
    for (int i = 0; i < toMove; ++i) *dest++ = *src++;
    if (current->bufferStatus == BUFFER_EOF_PENDING) {
        nChars = 0;
        current->nChars = 0;
    } else {
        int toRead = current->bufSize - toMove - 1;
        if (toRead <= 0)
            for (;;) LexerError("input buffer overflow, can't enlarge buffer because scanner uses REJECT");
        if (toRead > READ_SIZE) toRead = READ_SIZE;
        nChars = LexerInput(&current->chBuf[toMove], toRead);
        if (nChars < 0) LexerError("input in flex scanner failed");
        current->nChars = nChars;
    }
    int result;
    if (nChars == 0) {
        if (toMove == 0) {
            result = EOB_END_OF_FILE;
            // yyrestart(yyin)
            if (!current) current = CreateBuffer(yyin, BUFFER_SIZE);
            InitBuffer(current, yyin);
            LoadBufferState();
        } else {
            result = EOB_LAST_MATCH;
            current->bufferStatus = BUFFER_EOF_PENDING;
        }
    } else {
        result = EOB_CONTINUE_SCAN;
    }
    nChars += toMove;
    current->chBuf[nChars] = 0;
    current->chBuf[nChars + 1] = 0;
    yytext = current->chBuf;
    return result;
}

// 0x10010520: the states for the text read so far (a NUL or a byte past 0x7b counts as class 1 here)
int InputLex::GetPreviousState()
{
    int state = start;
    statePtr = stateBuf;
    *statePtr++ = state;
    for (const char* cp = yytext; cp < cBufP; ++cp) {
        const unsigned c0 = static_cast<unsigned char>(*cp);
        const int c = c0 < 0x7c ? (c0 ? static_cast<signed char>(ec[c0]) : 1) : 1;
        state = Step(state, c);
        *statePtr++ = state;
    }
    return state;
}

int InputLex::TryNulTrans(int state)  // 0x100105e0
{
    state = Step(state, 1);
    if (state != JAM_STATE) *statePtr++ = state;
    return state == JAM_STATE ? 0 : state;
}

// ---- yylex (0x1000fc80)
int InputLex::Lex()
{
    int state, act;
    char *cp, *bp;
    if (init) {
        init = 0;
        if (!start) start = 1;
        if (!current) current = CreateBuffer(yyin, BUFFER_SIZE);
        LoadBufferState();
    }
    for (;;) {
        cp = cBufP;
        *cp = holdChar;
        bp = cp;
        state = start;
        statePtr = stateBuf;
        *statePtr++ = state;
    match:
        do {
            unsigned c0 = static_cast<unsigned char>(*cp);
            ++cp;
            if (c0 > 0x7b) c0 = 0x7b;
            state = Step(state, static_cast<signed char>(ec[c0]));
            *statePtr++ = state;
        } while (state != JAM_STATE);
    findAction:
        state = *--statePtr;
        lp = S(accept, state);
        for (;;) {
            if (lp && lp < S(accept, state + 1)) {
                act = S(accList, lp);
                fullMatch = cp;
                break;
            }
            --cp;
            state = *--statePtr;
            lp = S(accept, state);
        }
        yytext = bp;
        yyleng = static_cast<int>(cp - bp);
        holdChar = *cp;
        *cp = 0;
        cBufP = cp;
        if (act != END_OF_BUFFER)
            for (int i = 0; i < yyleng; ++i)
                if (yytext[i] == '\n') ++yylineno;
    doAction:
        switch (act) {
        case 1: value.i = static_cast<int>(std::strtoul(yytext, nullptr, 16)); return HEX;
        case 2: value.i = std::atoi(yytext); return INT;
        case 3: value.f = static_cast<float>(std::atof(yytext)); return FLOAT;
        case 4: return TRUE_;
        case 5: return FALSE_;
        case 6: case 7: return INC;
        case 8: case 9: return DEC;
        case 10: return PRESSED;
        case 11: return RELEASED;
        case 12: case 13: return POSITIVE;
        case 14: case 15: return NEGATIVE;
        case 16: return CHANGED;
        case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
        case 0x1a: case 0x1b: case 0x1c:
            return act - 0x11 + POV_CENTER;  // the pov directions, then SHIFT, CTRL, ALT
        case 0x1d: case 0x1e: case 0x1f: case 0x20:
            // the name is copied (the original's strcpy into a 0x400 buffer; longer names are cut here)
            std::strncpy(textBuffer, yytext, sizeof(textBuffer) - 1);
            textBuffer[sizeof(textBuffer) - 1] = 0;
            value.text = textBuffer;
            return act - 0x1d + KEY_NAME;
        case 0x21: ++lineCount; break;
        case 0x22: break;
        case 0x23: LexerOutput(yytext, yyleng); break;
        case END_OF_BUFFER: {
            const int amount = static_cast<int>(cp - yytext) - 1;
            *cp = holdChar;
            if (current->bufferStatus == BUFFER_NEW) {
                nChars = current->nChars;
                current->inputFile = yyin;
                current->bufferStatus = BUFFER_NORMAL;
            }
            if (cBufP <= current->chBuf + nChars) {  // a NUL in the input
                cBufP = yytext + amount;
                state = GetPreviousState();
                const int next = TryNulTrans(state);
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
            case EOB_END_OF_FILE:
                cBufP = yytext;
                act = (start - 1) / 2 + 0x25;  // yywrap() is 1
                goto doAction;
            case EOB_CONTINUE_SCAN:
                cBufP = yytext + amount;
                state = GetPreviousState();
                cp = cBufP;
                bp = yytext;
                goto match;
            case EOB_LAST_MATCH:
                cBufP = current->chBuf + nChars;
                state = GetPreviousState();
                cp = cBufP;
                bp = yytext;
                goto findAction;
            }
            break;
        }
        case 0x25: return END;
        default: LexerError("fatal flex scanner internal error--no action found");
        }
    }
}

}  // namespace Jet
