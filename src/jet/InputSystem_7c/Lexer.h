// InputSystem_7c's scanner for key binding texts ("InputLex", a flex 2.5 C++ scanner with the prefix icfg),
// used by EmuDevice::Load and InputActionMapper::LoadActionMap. The DFA tables are the original's, read from the
// installed game's module at build time (generated/InputLexTables.h); the driver below is flex's skeleton as the
// original was generated with (REJECT-style state buffer, yylineno, no buffer growth, 7-bit input).
#pragma once

namespace Jet {

class ConfigData;
class String;

// 0x10002550: an entry's values as one text, separated by spaces (what the loaders scan)
void JoinValues(const ConfigData& entry, String& out);

class InputLex {
public:
    enum Token {
        END = 0,
        INT = 1,       // yylval: decimal number
        HEX = 2,       // yylval: hexadecimal number
        FLOAT = 3,     // yylval: float
        TRUE_ = 4,
        FALSE_ = 5,
        INC = 6,
        DEC = 7,
        PRESSED = 8,
        RELEASED = 9,
        CHANGED = 10,
        POSITIVE = 11,
        NEGATIVE = 12,
        POV_CENTER = 13,  // 0xd..0x15: centre, then N, NE, E, SE, S, SW, W, NW
        POV_N = 14,
        SHIFT = 0x16,
        CTRL = 0x17,
        ALT = 0x18,
        KEY_NAME = 0x19,  // yylval: the text
        BUTTON_NAME = 0x1a,
        AXIS_NAME = 0x1b,
        POV_NAME = 0x1c,
    };

    // scans `size` bytes of `text` (the original's MemoryFile over a String or a PString's node)
    InputLex(const char* text, unsigned long size);
    ~InputLex();
    int Lex();  // yylex
    int IntValue() const { return value.i; }
    float FloatValue() const { return value.f; }
    const char* Text() const { return value.text; }

private:
    InputLex(const InputLex&) = delete;
    InputLex& operator=(const InputLex&) = delete;

    struct Buffer {  // yy_buffer_state (0x28 bytes, malloc'ed like the original's)
        void* inputFile;
        char* chBuf;
        char* bufPos;
        int bufSize;
        int nChars;
        int isOurBuffer;
        int isInteractive;
        int atBol;
        int fillBuffer;
        int bufferStatus;
    };
    struct Source {  // what the original's LexerInput reads from its MemoryFile: {cursor, base, size}
        const char* cur;
        const char* base;
        unsigned long size;
    };

    Buffer* CreateBuffer(void* file, int size);
    void DeleteBuffer(Buffer* b);
    void InitBuffer(Buffer* b, void* file);
    void FlushBuffer(Buffer* b);
    void SwitchToBuffer(Buffer* b);
    void LoadBufferState();
    int GetNextBuffer();
    int GetPreviousState();
    int TryNulTrans(int state);
    int LexerInput(char* buf, int maxSize);
    void LexerOutput(const char* buf, int size);
    [[noreturn]] void LexerError(const char* msg);

    // flex's scanner state
    char* yytext = nullptr;
    int yyleng = 0;
    int yylineno = 1;
    void* yyin = nullptr;
    void* yyout = nullptr;
    Buffer* current = nullptr;
    char holdChar = 0;
    int nChars = 0;
    char* cBufP = nullptr;
    int init = 1;
    int start = 0;
    int* stateBuf;
    int* statePtr = nullptr;
    char* fullMatch = nullptr;
    int lp = 0;
    int lineCount = 1;  // (InputLex's own: counted by the newline rule, never read)

    union {
        int i;
        float f;
        const char* text;
    } value{};
    char textBuffer[0x400];
    Source source;
};

}  // namespace Jet
