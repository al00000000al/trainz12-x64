// ConfigParser's scanner (KernelStandard_7c 0x10017530..0x10019880, 0x1001a500..0x1001a667): a flex 2.5.4 C++
// scanner (scfgFlexLexer, with REJECT-style state buffer, a start-condition stack and yylineno) and the
// ConfigParserCore around it (line, token value, file name, the string being built). Internal to the module:
// only ConfigParser holds one. Notes: notes/KernelStandard.md "U7c1".
#pragma once

#include <jet/KernelStandard.h>
#include <jet/PString.h>

namespace Jet {

class ConfigLexer {
public:
    // the scanner's tokens (the parser's terminals)
    enum Token {
        ASSIGN = 1, CLOSE, COMMA, END, FLOAT, FLOATIND, FLOATINF, FLOATQNAN, HEXSTRING, INT, OPEN, RAWASSIGN, SC,
        START, STRING, VERSION
    };

    ConfigLexer(MemoryFile* in, const PString& name);
    ~ConfigLexer();

    int Lex();  // a token (0 at the end, -1 a bad string, -2 any other character); the value in `value`

    int line = 1;              // +0x78 counted by the rules
    char* value = nullptr;     // +0x7c the token's text (Jet::Alloc), owned by whoever takes it
    PString name;              // +0x80 the file's name

private:
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

    int Input(char* buf, int max);
    void Output(const char* buf, int size);
    [[noreturn]] void Error(const char* message);
    Buffer* CreateBuffer(MemoryFile* in, int size);
    void DeleteBuffer(Buffer* b);
    void InitBuffer(Buffer* b, MemoryFile* in);
    void FlushBuffer(Buffer* b);
    void LoadBufferState();
    void Restart(MemoryFile* in);
    int GetNextBuffer();
    int PreviousState();
    int TryNulTransition(int state);
    void Unput(int c, char* bp);
    void PushState(int condition);
    void PopState();
    char* CopyText(const char* s) const;  // strdup through Jet::Alloc (null stays null)
    char* TakeBuffer();                   // a copy of `buffer`, or null when it holds no text

    // yyFlexLexer (0x78 bytes in the original)
    char* yytext = nullptr;
    int yyleng = 0;
    int yylineno = 1;
    int stackPtr = 0;
    int stackDepth = 0;
    int* stack = nullptr;
    MemoryFile* yyin;
    MemoryFile* yyout = nullptr;
    Buffer* current = nullptr;
    char holdChar = 0;
    int nChars = 0;
    char* cBufP = nullptr;
    int init = 1;
    int start = 0;
    int switchOnEof = 0;
    int* stateBuf;
    int* statePtr = nullptr;
    int lp = 0;

    AnsiString buffer;  // +0x84
};

}  // namespace Jet
