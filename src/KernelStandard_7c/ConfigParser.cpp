// Unit U7c1 of KernelStandard_7c: ConfigParser (0x10015b20..0x10017140, 0x100198b0..0x1001a7eb) and its Lemon
// parser. The parser's tables are the original's (scfg_*.h, read from the installed module at build time); the
// driver is old Lemon's (hash-table states) and the 34 rule actions are ported by stack position. Notes:
// notes/KernelStandard.md "U7c1".
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "ConfigLexer.h"
#include "Crt80.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "scfg_actions.h"
#include "scfg_hexhi.h"
#include "scfg_hexlo.h"
#include "scfg_rules.h"
#include "scfg_states.h"

namespace Jet {

namespace {

// {lookahead | action << 8, next entry address}; the states {first entry address, mask, default action}
const unsigned int actions[] = {SCFG_ACTIONS_DATA};
const unsigned int states[] = {SCFG_STATES_DATA};
const unsigned char rules[] = {SCFG_RULES_DATA};  // {lhs, rhs count} per rule
const unsigned char hexLo[] = {SCFG_HEXLO_DATA};  // indexed by the character
const unsigned char hexHi[] = {SCFG_HEXHI_DATA};
const unsigned int kActionsAt = 0x100735a0;  // where the entries live in the original (their links point there)

const int kStates = 65;          // YYNSTATE
const int kErrorAction = 99;     // YY_ERROR_ACTION (YYNSTATE + YYNRULE)
const int kAcceptAction = 100;   // YY_ACCEPT_ACTION
const int kNoAction = 101;       // YY_NO_ACTION
const int kNoCode = 29;          // YYNOCODE
const int kErrorSymbol = 20;     // YYERRORSYMBOL
const int kStackDepth = 500;

struct StackEntry {
    int state;
    int major;
    void* minor;
};
struct Parser {  // 0x177c bytes in the original
    int idx;
    int errcnt;
    StackEntry* top;
    StackEntry stack[kStackDepth];
};
struct Extra {  // the parser's argument: {scanner, error, the ConfigParser}
    ConfigLexer* core;
    bool error;
    ConfigParser* parser;
};

int Entry(unsigned int address) { return address ? static_cast<int>((address - kActionsAt) / 8) : -1; }

// yy_find_parser_action / yy_find_reduce_action (0x10016130): the same lookup
int FindAction(int state, int lookahead)
{
    const unsigned int* const s = &states[state * 3];
    const int first = Entry(s[0]);
    const unsigned int mask = s[1];
    const int fallback = static_cast<int>(s[2] & 0xff);
    if (lookahead == kNoCode) {
        if (mask != 0 || static_cast<int>(actions[first * 2] & 0xff) != kNoCode) return kNoAction;
        return fallback;
    }
    for (int e = first + static_cast<int>(mask & static_cast<unsigned>(lookahead)); e >= 0; e = Entry(actions[e * 2 + 1]))
        if (static_cast<int>(actions[e * 2] & 0xff) == lookahead) return static_cast<int>((actions[e * 2] >> 8) & 0xff);
    return fallback;
}

void Destroy(int major, void*& minor)  // the terminals' values are their texts (Jet::Alloc)
{
    if (static_cast<unsigned char>(major - 1) < 0x10 && minor) {
        Alloc::Delete(minor);
        minor = nullptr;
    }
}

void PopStack(Parser* p)
{
    if (p->idx < 0) return;
    Destroy(p->top->major, p->top->minor);
    --p->idx;
    --p->top;
}

void PopAll(Parser* p)  // 0x10017100 (also the accept action)
{
    while (p->idx >= 0) PopStack(p);
}

void Shift(Parser* p, int state, int major, void* minor)  // 0x10016190 (an overflow empties the stack)
{
    ++p->idx;
    ++p->top;
    if (p->idx >= kStackDepth) {
        --p->idx;
        --p->top;
        PopAll(p);
        return;
    }
    p->top->state = state;
    p->top->major = major;
    p->top->minor = minor;
}

// ---- the values the rules build
struct Item {  // a value of a data array (0x10, Jet::Alloc): kind 0xa int, 5 float, 0xf string
    int kind;
    PString text;
    float f;
    int i;
};
const int kInt = 0xa, kFloat = 5, kString = 0xf;

struct ItemChunk {  // DynamicArray<Item*, 63>
    ItemChunk* next;
    ItemChunk* prev;
    int count;
    Item* items[63];
};
using ItemList = ItemChunk*;  // the list object itself (Jet::Alloc, 4 bytes) holds the head

void Append(ItemList* list, Item* item)  // 0x100173a0
{
    ItemChunk* c;
    if (!*list) {
        c = static_cast<ItemChunk*>(Alloc::New(sizeof(ItemChunk)));
        c->count = 0;
        *list = c;
        c->prev = c;
        c->next = c;
    } else {
        ItemChunk* const last = (*list)->prev;
        c = last;
        if (last->count == 63) {
            c = static_cast<ItemChunk*>(Alloc::New(sizeof(ItemChunk)));
            c->count = 0;
            c->next = *list;
            c->prev = last;
            (*list)->prev = c;
            last->next = c;
        }
    }
    c->items[c->count++] = item;
}

void FreeChunks(ItemList* list)
{
    if (ItemChunk* const head = *list) {
        ItemChunk* c = head->next;
        while (c != head) {
            ItemChunk* const n = c->next;
            Alloc::Delete(c);
            c = n;
        }
        Alloc::Delete(head);
        *list = nullptr;
    }
}

void FreeItems(ItemList* list)  // 0x10017420: the items (and their strings), then the chunks
{
    if (ItemChunk* const head = *list) {
        ItemChunk* c = head;
        do {
            for (int k = 0; k < c->count; ++k)
                if (Item* const item = c->items[k]) {
                    item->text.~PString();
                    Alloc::Delete(item);
                }
            c = c->next;
        } while (c != head);
    }
    FreeChunks(list);
}

Item* NewItem(int kind)  // (only the string is cleared; the other number is left as allocated)
{
    Item* const item = static_cast<Item*>(Alloc::New(sizeof(Item)));
    if (item) new (&item->text) PString();
    item->kind = kind;
    return item;
}

float Bits(unsigned int u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// the special float tokens: a non-negative text gives the first, a negative one (or a NaN) the second
float Special(const char* text, unsigned int positive, unsigned int negative)
{
    return Crt80::atof(text) >= 0.0 ? Bits(positive) : Bits(negative);
}

PString FromText(const char* text)  // 0x1000c4d0
{
    PString p;
    if (text) p.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
    return p;
}

void Free(void*& minor)
{
    if (minor) Alloc::Delete(minor);
    minor = nullptr;
}

ConfigData* NewNode(const PString& name)
{
    void* const mem = ConfigData::operator new(sizeof(ConfigData));
    return mem ? ::new (mem) ConfigData(name) : nullptr;
}

ConfigData* NewNode(const PString& name, const PString& value)
{
    void* const mem = ConfigData::operator new(sizeof(ConfigData));
    return mem ? ::new (mem) ConfigData(name, value) : nullptr;
}

void FreeNodeList(ConfigData::Children* list)  // 0x10016fc0: the chunks and the list object (not the nodes)
{
    using Chunk = ConfigData::Children::Chunk;
    if (Chunk* const head = list->head) {
        Chunk* c = head->next;
        while (c != head) {
            Chunk* const n = c->next;
            Alloc::Delete(c);
            c = n;
        }
        Alloc::Delete(head);
        list->head = nullptr;
    }
    Alloc::Delete(list);
}

ConfigData::Children* NewNodeList()
{
    ConfigData::Children* const list = static_cast<ConfigData::Children*>(Alloc::New(sizeof(ConfigData::Children)));
    if (list) list->head = nullptr;
    return list;
}

}  // namespace

// the grammar's actions (a friend of ConfigData and ConfigParser)
struct ConfigGrammar {
    // 0x10013bd0: the two lists swap contents (chunk copies, as ConfigData::AssignNodes)
    static void Swap(ConfigData::Children& a, ConfigData::Children& b)
    {
        ConfigData::Children saved;
        Copy(saved, a);
        Clear(a);
        Copy(a, b);
        Clear(b);
        Copy(b, saved);
        Clear(saved);
    }
    static void Copy(ConfigData::Children& to, const ConfigData::Children& from)
    {
        using Chunk = ConfigData::Children::Chunk;
        Chunk* const first = from.head;
        if (!first) return;
        Chunk* c = first;
        do {
            Chunk* const n = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
            if (n) n->count = 0;
            std::memcpy(&n->count, &c->count, c->count * sizeof(ConfigData*) + 4);
            if (to.head) {
                Chunk* const last = to.head->prev;
                n->prev = last;
                n->next = to.head;
                to.head->prev = n;
                last->next = n;
            } else {
                to.head = n;
                n->prev = n;
                n->next = n;
            }
            c = c->next;
        } while (c != first);
    }
    static void Clear(ConfigData::Children& a)
    {
        using Chunk = ConfigData::Children::Chunk;
        if (Chunk* const first = a.head) {
            Chunk* c = first->next;
            while (c != first) {
                Chunk* const n = c->next;
                Alloc::Delete(c);
                c = n;
            }
            Alloc::Delete(first);
            a.head = nullptr;
        }
    }

    // 0x10015ca0: a data array from the items. Its kind is the first item's, unless the first item of another
    // kind is a string (then strings) or a float (then floats); the scan stops there.
    static void FillArray(ConfigData* node, ItemList* list)
    {
        if (ItemChunk* const head = *list) {
            int kind = head->items[0]->kind;
            int count = 0;
            {
                bool decided = false;
                ItemChunk* c = head;
                int k = 1;
                do {
                    for (; k < c->count && !decided; ++k) {
                        const int other = c->items[k]->kind;
                        if (other == kind) continue;
                        if (other == kString) kind = kString;
                        else if (kind != kString && other == kFloat) kind = kFloat;
                        decided = true;
                    }
                    k = 0;
                    c = c->next;
                } while (c != head && !decided);
                c = head;
                do {
                    count += c->count;
                    c = c->next;
                } while (c != head);
            }
            auto each = [&](auto&& f) {
                int n = 0;
                ItemChunk* c = head;
                do {
                    for (int k = 0; k < c->count; ++k) f(c->items[k], n++);
                    c = c->next;
                } while (c != head);
            };
            if (kind == kInt) {
                int* const ints = static_cast<int*>(Alloc::New(static_cast<unsigned>(count * 4)));
                each([&](Item* item, int n) {
                    if (item->kind == kInt) ints[n] = item->i;
                    else if (item->kind == kString) ints[n] = Crt80::strtol(item->text.Text(), nullptr, 0);
                    else if (item->kind == kFloat) ints[n] = static_cast<int>(item->f);
                });
                node->ClearData();
                int* const block = static_cast<int*>(Alloc::New(static_cast<unsigned>(count * 4 + 4)));
                node->type = ConfigData::INTS;
                node->data = block;
                block[0] = count;
                if (count) std::memcpy(block + 1, ints, static_cast<size_t>(count) * 4);
                if (ints) Alloc::Delete(ints);
            } else if (kind == kFloat) {
                float* const floats = static_cast<float*>(Alloc::New(static_cast<unsigned>(count * 4)));
                each([&](Item* item, int n) {
                    if (item->kind == kFloat) floats[n] = static_cast<float>(static_cast<double>(item->f));
                    else if (item->kind == kInt) floats[n] = static_cast<float>(item->i);
                    else floats[n] = static_cast<float>(Crt80::atof(item->text.Text()));
                });
                node->ClearData();
                int* const block = static_cast<int*>(Alloc::New(static_cast<unsigned>(count * 4 + 4)));
                node->type = ConfigData::FLOATS;
                node->data = block;
                block[0] = count;
                if (count) std::memcpy(block + 1, floats, static_cast<size_t>(count) * 4);
                Alloc::Delete(floats);
            } else if (kind == kString) {
                int* const arr = static_cast<int*>(Alloc::New(static_cast<unsigned>(count * 4 + 4)));
                PString* items = nullptr;
                if (arr) {
                    arr[0] = count;
                    items = reinterpret_cast<PString*>(arr + 1);
                    for (int n = 0; n < count; ++n) new (&items[n]) PString();
                }
                each([&](Item* item, int n) {
                    if (item->kind == kString) items[n] = item->text;
                    else if (item->kind == kInt) items[n].Setf("%d", item->i);
                    else items[n].Setf("%f", static_cast<double>(item->f));
                });
                node->SetPStringArray(items, count);
                if (items) {
                    for (int n = count - 1; n >= 0; --n) items[n].~PString();
                    Alloc::Delete(arr);
                }
            }
        }
        FreeItems(list);
        FreeChunks(list);
        Alloc::Delete(list);
    }

    // 0x10016210: rule `rule` with the stack top at `msp` (msp[-k] the k'th symbol from the right)
    static void* Reduce(int rule, StackEntry* msp, Extra* extra)
    {
        auto m = [msp](int k) -> void*& { return msp[k].minor; };
        auto text = [msp](int k) { return static_cast<const char*>(msp[k].minor); };
        void* result = nullptr;
        switch (rule) {
        case 0: {  // the tree's top nodes go to the root
            auto* const list = static_cast<ConfigData::Children*>(m(0));
            Swap(*list, extra->parser->root->children);
            FreeNodeList(list);
            m(0) = nullptr;
            break;
        }
        case 1:  // the version
            extra->parser->version = static_cast<float>(Crt80::atof(text(0)));
            Free(m(0));
            Free(m(-1));
            break;
        case 2:
        case 16:
            result = m(0);
            break;
        case 3:
            result = m(0);
            Free(m(-1));
            break;
        case 4: {  // a named data node
            ConfigData* const node = static_cast<ConfigData*>(m(0));
            result = node;
            node->name = FromText(text(-2));
            Free(m(-2));
            Free(m(-1));
            break;
        }
        case 5:
            result = NewNode(FromText(text(-2)));
            Free(m(-2));
            Free(m(-1));
            Free(m(0));
            break;
        case 6: {
            ConfigData* const node = NewNode(FromText(text(-1)));
            result = node;
            auto* const list = static_cast<ConfigData::Children*>(m(0));
            Swap(*list, node->children);
            FreeNodeList(list);
            m(0) = nullptr;
            Free(m(-1));
            break;
        }
        case 7: {
            ConfigData* const node = NewNode(FromText(text(-4)), FromText(text(-2)));
            result = node;
            auto* const list = static_cast<ConfigData::Children*>(m(0));
            Swap(*list, node->children);
            FreeNodeList(list);
            m(0) = nullptr;
            Free(m(-4));
            Free(m(-2));
            Free(m(-3));
            Free(m(-1));
            break;
        }
        case 8:
        case 9:
        case 10:
        case 11:
        case 12: {  // a single number
            ConfigData* const node = NewNode(PString());
            result = node;
            if (rule == 8) *node << static_cast<int>(Crt80::strtol(text(-1), nullptr, 0));
            else if (rule == 9) *node << static_cast<float>(Crt80::atof(text(-1)));
            else if (rule == 10) *node << Special(text(-1), 0x7fe00000, 0xffe00000);
            else if (rule == 11) *node << Special(text(-1), 0x7f800000, 0xff800000);
            else *node << Special(text(-1), 0x7fc00000, 0xffc00000);
            Free(m(-1));
            Free(m(0));
            break;
        }
        case 13: {
            ConfigData* const node = static_cast<ConfigData*>(m(0));
            result = node;
            node->name = FromText(text(-3));
            Free(m(-3));
            Free(m(-2));
            Free(m(-1));
            break;
        }
        case 14: {  // bytes in hex
            const char* const s = text(-1);
            const unsigned length = static_cast<unsigned>(std::strlen(s)) >> 1;
            unsigned char* const bytes = static_cast<unsigned char*>(Alloc::New(length));
            auto hex = [](const unsigned char* table, char c) {
                const int i = static_cast<signed char>(c);
                return i >= 0 && i < 103 ? table[i] : static_cast<unsigned char>(0);
            };
            for (unsigned i = 0; i < length; ++i)
                bytes[i] = static_cast<unsigned char>(hex(hexLo, s[i * 2 + 1]) + hex(hexHi, s[i * 2]));
            ConfigData* const node = NewNode(PString());
            result = node;
            node->SetByteArray(bytes, static_cast<int>(length));
            if (bytes) Alloc::Delete(bytes);
            Free(m(-1));
            Free(m(0));
            break;
        }
        case 15: {  // a string piece
            void* const mem = Alloc::New(sizeof(String));
            result = mem ? ::new (mem) String(text(0)) : nullptr;
            Free(m(0));
            break;
        }
        case 17: {  // string pieces joined
            String* const s = static_cast<String*>(m(-1));
            result = s;
            String* const more = static_cast<String*>(m(0));
            *s += *more;
            if (more) {
                more->~String();
                Alloc::Delete(more);
            }
            m(0) = nullptr;
            break;
        }
        case 18: {
            String* const s = static_cast<String*>(m(-1));
            result = NewNode(PString(), FromText(static_cast<const char*>(*s)));
            s->~String();
            Alloc::Delete(s);
            m(-1) = nullptr;
            Free(m(0));
            break;
        }
        case 19: {
            ConfigData* const node = NewNode(PString());
            result = node;
            FillArray(node, static_cast<ItemList*>(m(-2)));
            Free(m(-3));
            Free(m(-1));
            Free(m(0));
            break;
        }
        case 20: {
            ConfigData* const node = NewNode(PString());
            result = node;
            FillArray(node, static_cast<ItemList*>(m(-1)));
            Free(m(0));
            break;
        }
        case 21: {
            ItemList* const list = static_cast<ItemList*>(m(-2));
            Append(list, static_cast<Item*>(m(0)));
            result = list;
            Free(m(-1));
            break;
        }
        case 22: {
            ItemList* const list = static_cast<ItemList*>(Alloc::New(sizeof(ItemList)));
            if (list) *list = nullptr;
            Append(list, static_cast<Item*>(m(-2)));
            Append(list, static_cast<Item*>(m(0)));
            result = list;
            Free(m(-1));
            break;
        }
        case 23: {
            ItemList* const list = static_cast<ItemList*>(Alloc::New(sizeof(ItemList)));
            if (list) *list = nullptr;
            Append(list, static_cast<Item*>(m(-1)));
            result = list;
            Free(m(-2));
            Free(m(0));
            break;
        }
        case 24:
            result = m(-1);
            Free(m(-2));
            Free(m(0));
            break;
        case 25:
            result = NewNodeList();
            Free(m(-1));
            Free(m(0));
            break;
        case 26: {
            ConfigData::Children* const list = NewNodeList();
            list->Add(static_cast<ConfigData*>(m(0)));
            result = list;
            break;
        }
        case 27: {
            ConfigData::Children* const list = static_cast<ConfigData::Children*>(m(-1));
            list->Add(static_cast<ConfigData*>(m(0)));
            result = list;
            break;
        }
        case 28: {
            Item* const item = NewItem(kInt);
            item->i = Crt80::strtol(text(0), nullptr, 0);
            result = item;
            Free(m(0));
            break;
        }
        case 29:
        case 30:
        case 31: {
            Item* const item = NewItem(kFloat);
            if (rule == 29) item->f = static_cast<float>(Crt80::atof(text(0)));
            else if (rule == 30) item->f = Special(text(0), 0x7fe00000, 0xffe00000);
            else item->f = Special(text(0), 0x7f800000, 0xff800000);
            result = item;
            Free(m(0));
            break;
        }
        case 32: {
            Item* const item = NewItem(kFloat);
            item->f = Special(text(-1), 0x7fc00000, 0xffc00000);
            result = item;
            Free(m(-1));
            Free(m(0));
            break;
        }
        case 33: {
            Item* const item = NewItem(kString);
            item->text = FromText(text(0));
            result = item;
            Free(m(0));
            break;
        }
        default:
            break;
        }
        return result;
    }
};

namespace {

void Reduce(Parser* p, int rule, Extra* extra)
{
    void* const result = ConfigGrammar::Reduce(rule, p->top, extra);
    const int lhs = rules[rule * 2];
    const int count = rules[rule * 2 + 1];
    p->idx -= count;
    p->top -= count;
    const int act = FindAction(p->top->state, lhs);
    if (act < kStates) Shift(p, act, lhs, result);
    else if (act == kAcceptAction) PopAll(p);
}

// 0x10017140 (old lempar.c's Parse); the error messages are formatted into a buffer nobody reads, so they are
// left out
void Parse(Parser* p, int major, void* minor, Extra* extra)
{
    bool errorHit = false;
    if (p->idx < 0) {
        if (major == 0) return;
        p->top = &p->stack[0];
        p->idx = 0;
        p->errcnt = -1;
        p->top->state = 0;
        p->top->major = 0;
    }
    const bool endOfInput = major == 0;
    do {
        int act = FindAction(p->top->state, major);
        if (act < kStates) {
            Shift(p, act, major, minor);
            --p->errcnt;
            if (endOfInput && p->idx >= 0) major = 0;
            else major = kNoCode;
        } else if (act < kErrorAction) {
            Reduce(p, act - kStates, extra);
        } else if (act == kErrorAction) {
            if (p->errcnt < 0) extra->error = true;  // yy_syntax_error
            if (p->top->major == kErrorSymbol || errorHit) {
                Destroy(major, minor);
                major = kNoCode;
            } else {
                while (p->idx >= 0 && p->top->major != kErrorSymbol && (act = FindAction(p->top->state, kErrorSymbol)) >= kStates)
                    PopStack(p);
                if (p->idx < 0 || major == 0) {
                    Destroy(major, minor);
                    PopAll(p);  // yy_parse_failed
                    extra->error = true;
                    major = kNoCode;
                } else if (p->top->major != kErrorSymbol) {
                    Shift(p, act, kErrorSymbol, nullptr);
                }
            }
            p->errcnt = 3;
            errorHit = true;
        } else {  // accept, or no action
            PopAll(p);
            major = kNoCode;
        }
    } while (major != kNoCode && p->idx >= 0);
}

// 0x1001a670: 0, or 3 on any error
int Run(ConfigLexer* core, ConfigParser* owner)
{
    Extra extra = {core, false, owner};
    Parser* const p = static_cast<Parser*>(Alloc::New(sizeof(Parser)));
    if (p) p->idx = -1;
    int token = core->Lex();
    while (token > 0 && !extra.error) {
        Parse(p, token, core->value, &extra);
        core->value = nullptr;
        token = core->Lex();
    }
    Parse(p, 0, core->value, &extra);  // ("Syntax error." for -1 goes to the same unread buffer)
    if (p) {  // 0x100160e0
        PopAll(p);
        Alloc::Delete(p);
    }
    return !extra.error && token >= 0 ? 0 : 3;
}

AnsiString& Key(void*& slot) { return reinterpret_cast<AnsiString&>(slot); }
const AnsiString& Key(void* const& slot) { return reinterpret_cast<const AnsiString&>(slot); }

void Assign(PString& p, const char* text)  // 0x1000d180: the old node released, the text interned
{
    p = FromText(text);
}

}  // namespace

// ---- the class (0x100198b0..)
void* ConfigParser::operator new(size_t size) { return Alloc::New(static_cast<unsigned>(size)); }
void ConfigParser::operator delete(void* p) { Alloc::Delete(p); }

ConfigParser::ConfigParser() : core(nullptr), fileName(), root(nullptr) { ::new (&key) AnsiString(); }

ConfigParser::ConfigParser(ConfigData* config) : core(nullptr), fileName(), root(config), version(1.0f)
{
    ::new (&key) AnsiString();
}

ConfigParser::ConfigParser(const ConfigParser& o) : core(o.core), fileName(o.fileName), root(o.root)
{
    version = static_cast<float>(static_cast<double>(o.version));
    ::new (&key) AnsiString(Key(o.key));
}

ConfigParser& ConfigParser::operator=(const ConfigParser& o)  // 0x10015b90 (shallow)
{
    core = o.core;
    fileName = o.fileName;
    root = o.root;
    version = static_cast<float>(static_cast<double>(o.version));
    Key(key) = Key(o.key);
    return *this;
}

ConfigParser::~ConfigParser()  // 0x10019980
{
    if (core) {
        core->~ConfigLexer();
        Alloc::Delete(core);
    }
    core = nullptr;
    if (root) delete root;
    root = nullptr;
    Key(key).~AnsiString();
}

float ConfigParser::GetVersion() const { return version; }
void ConfigParser::SetVersion(float v) { version = v; }
void ConfigParser::SetConfig(ConfigData* config) { root = config; }
ConfigData* ConfigParser::GetConfig() { return root; }
const ConfigData* ConfigParser::GetConfig() const { return root; }
const ConfigData* ConfigParser::GetConfigRoot() const { return root; }
const char* ConfigParser::GetFileName() const { return fileName.Text(); }
void ConfigParser::SetFileName(const char* name) { Assign(fileName, name); }
void ConfigParser::SetResourceKey(const String& k) { Key(key) = k; }

// 0x10019ee0: a new scanner over the file, a new root named after the file, then the parse
ConfigParser::Result ConfigParser::Parse(const char*, const char* name, MemoryFile& file)
{
    if (core) {
        core->~ConfigLexer();
        Alloc::Delete(core);
    }
    core = nullptr;
    if (void* const mem = Alloc::New(sizeof(ConfigLexer))) core = ::new (mem) ConfigLexer(&file, FromText(name));
    if (root) delete root;
    root = nullptr;
    root = NewNode(fileName);
    return Result(Run(core, this));
}

ConfigParser::Result ConfigParser::Parse(MemoryFile& file)  // 0x10019c90
{
    const int* const b = *reinterpret_cast<const int* const*>(&file.name);  // the name's text, null without one
    Assign(fileName, b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr);
    const char* const name = fileName.Text();
    return Parse(name, name, file);
}

ConfigParser::Result ConfigParser::Parse(const char* path)  // 0x10019cf0
{
    Assign(fileName, path);
    File f;
    if (f.Open(path, 0x10, 0).code != 0) return Result(2);
    MemoryFile mf;
    const Result r = mf.Open(f, reinterpret_cast<const String&>(Key(key))) ? Parse(path, path, mf) : Result(3);
    return r;
}

ConfigParser::Result ConfigParser::Parse(const char* name, void* data, unsigned int size)  // 0x10019e40
{
    Assign(fileName, name);
    MemoryFile mf(data, size, false, 0);
    mf.name = name;
    return Parse(mf);
}

// 0x10019a70: the file through the subst drive `base`; the scanner is named "base:path"
ConfigParser::Result ConfigParser::Parse(const PString& base, const char* path)
{
    Assign(fileName, path);
    File f;
    if (f.Open(base, path, 0x10, 0).code != 0) return Result(2);
    MemoryFile mf;
    File::Path full(base.Text());
    std::strcat(full.text, ":");
    std::strcat(full.text, path);
    const Result r = mf.Open(f, reinterpret_cast<const String&>(Key(key))) ? Parse(path, full.text, mf) : Result(3);
    return r;
}

bool ConfigParser::Save(const char* path, bool flag, int encoding) const { return Save(nullptr, path, flag, encoding); }

bool ConfigParser::Save(const char* base, const char* path, bool flag, int encoding) const  // 0x1001a070
{
    File f;
    if (!root || !path) return false;
    const unsigned long mode = (flag ? 2u : 1u) | 0x100u;
    const bool failed = base ? f.Open(FromText(base), path, mode, 0).code != 0 : f.Open(path, mode, 0).code != 0;
    if (failed) return false;
    const bool ok = WriteData(f, encoding);
    f.Close();
    return ok;
}

bool ConfigParser::SaveString(PString& text, int encoding) const  // 0x1001a240 (no version line)
{
    File f;
    if (!root) return false;
    return root->Write(text, encoding);
}

// 0x1001a350: "version x.xx" (UTF-16 with a BOM for encoding 2), then the tree as text
bool ConfigParser::WriteData(File& f, int encoding) const
{
    if (!root) return false;
    AnsiString header;
    header.Setf("version %1.2f\r\n", static_cast<double>(version));
    if (encoding == 2) {
        unsigned int bom = 0xfeff;
        if (!(f.Write(&bom, 2, 1) > 0)) return false;
        __wchar_t wide[64];
        const int bytes = header.GetUnicode(wide, 0x3f) * 2;
        if (bytes <= 0) return false;
        if (!(f.Write(wide, static_cast<unsigned long>(bytes), 1) > 0)) return false;
    } else {
        const int* const b = *reinterpret_cast<const int* const*>(&header);
        const int length = b[0] ? b[1] : -1;
        const char* const t = b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr;
        if (!(f.Write(t, static_cast<unsigned long>(length), 1) > 0)) return false;
    }
    return root->Write(f, encoding);
}

}  // namespace Jet
