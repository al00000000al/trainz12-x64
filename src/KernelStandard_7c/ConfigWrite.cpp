// Unit U7b2 of KernelStandard_7c: ConfigData as text and in the binary form (0x100118d0..0x10013ad0). Notes:
// notes/KernelStandard.md "U7b2". The text writers go through ConfigData::Writer; the binary reader reads a File.
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace Jet {

const char* ConfigFloatText(float f, unsigned& length);  // ConfigData.cpp (0x1000f960)

namespace {

const char* Text(const PString& p) { return p.node ? p.node->text : nullptr; }
const PString& AsPString(void* const& slot) { return reinterpret_cast<const PString&>(slot); }

// an AnsiString's buffer {capacity, length, chars}: "has text" is a buffer whose first word is not 0
struct Buf {
    unsigned capacity;
    unsigned length;
    char text[1];
};
const Buf* B(const AnsiString& s) { return *reinterpret_cast<const Buf* const*>(&s); }
bool HasText(const AnsiString& s) { return B(s)->capacity != 0; }
// (the original builds AnsiStrings, whose constructor makes a null text an empty buffer with Size() 0, and passes
// them as the String& FormatDataString takes)
String& AsString(AnsiString& s) { return reinterpret_cast<String&>(s); }

// 0x100118d0 / 0x10011930: the module's two writers ({vftable, target})
class FileWriter : public ConfigData::Writer {
public:
    explicit FileWriter(File& f) : file(&f) {}
    unsigned long Write(const void* data, unsigned long size, unsigned long count) override
    {
        return file->Write(data, size, count);
    }
    int Encoding() override { return file->EncodingField(); }

private:
    File* file;
};

// (Ins copies the text up to its NUL, whatever the size: the original's string writer does the same)
class StringWriter : public ConfigData::Writer {
public:
    explicit StringWriter(PString& s) : text(&s) {}
    unsigned long Write(const void* data, unsigned long, unsigned long) override
    {
        PStringCache::Node* const n = text->node;
        text->node = PString::Cache()->Ins(n, static_cast<const unsigned char*>(data), n ? n->length : 0);
        return 1;
    }
    int Encoding() override { return 1; }

private:
    PString* text;
};

const char kCrLf[] = "\r\n";
const char kSpaces[] = "                ";  // 16

}  // namespace

// ---- text (0x10011980..0x10012e40)
bool ConfigData::Write(File& file, int encoding) const
{
    FileWriter w(file);
    if (!encoding) encoding = w.Encoding();
    return WriteChildList(w, 0, encoding);
}

bool ConfigData::Write(PString& text, int encoding) const
{
    StringWriter w(text);
    if (!encoding) encoding = w.Encoding();
    return WriteChildList(w, 0, encoding);
}

bool ConfigData::Write(Writer& w, int encoding) const
{
    if (!encoding) encoding = w.Encoding();
    return WriteChildList(w, 0, encoding);
}

// 0x10011ab0: one line per child, a blank line before and after each block
bool ConfigData::WriteChildList(Writer& w, int depth, int encoding) const
{
    bool previousBlock = false;
    int i = 0;
    for (auto c = children.First(); c.node; ++c, ++i) {
        const ConfigData* const child = *c.node;
        const bool block = child->children.head != nullptr;
        if (i != 0 && (block || previousBlock)) WriteChars(w, kCrLf, 2, encoding);
        previousBlock = block;
        const bool ok = block ? child->WriteDataBlock(w, depth, encoding) : child->WriteDataItem(w, depth, encoding);
        if (!ok) return false;
        WriteChars(w, kCrLf, 2, encoding);
    }
    return true;
}

bool ConfigData::WriteBlock(Writer& w, int depth, int encoding) const  // 0x10011bb0
{
    return children.head ? WriteDataBlock(w, depth, encoding) : WriteDataItem(w, depth, encoding);
}

// 0x10011bf0: past column 100 (and 50 past the indent) the line breaks and continues at the indent
bool ConfigData::WriteWrap(Writer& w, const char* text, int length, int& column, int indent, int encoding) const
{
    column += length;
    const unsigned at = static_cast<unsigned>(column);
    if (at > 100 && indent >= 0 && at - static_cast<unsigned>(indent) > 50) {
        WriteChars(w, kCrLf, 2, encoding);
        std::string spaces(static_cast<size_t>(indent), ' ');
        WriteChars(w, spaces.c_str(), indent, encoding);
        column = length + indent;
    }
    return WriteChars(w, text, length, encoding);
}

// 0x10011ca0: "name = value;", arrays "name = a,b,c;" (one item in parentheses; ten or more get "//count")
bool ConfigData::WriteDataItem(Writer& w, int depth, int encoding) const
{
    AnsiString nameText(Text(name));
    FormatDataString(AsString(nameText));
    const bool hasName = HasText(nameText);
    if (!hasName && type == NONE) return true;
    if (hasName && B(nameText)->length == 0) nameText = "\"\"";
    if (!WriteTabs(w, depth, encoding)) return false;
    int column = depth * 2;
    if (hasName) {
        column += static_cast<int>(B(nameText)->length);
        if (!WriteChars(w, B(nameText)->text, static_cast<int>(B(nameText)->length), encoding)) return false;
    }
    if (hasName || type < BYTES) {
        column += 3;
        if (!WriteChars(w, " = ", 3, encoding)) return false;
    }
    int count = 1;
    if (type < BYTES) {
        const PString p = DataToPString();
        AnsiString value(Text(p));
        if (HasText(value)) {
            if (type == PSTRING) FormatDataString(AsString(value));
            if (HasText(value) && B(value)->length == 0) value = "\"\"";
            const bool has = HasText(value);
            if (!WriteChars(w, has ? B(value)->text : nullptr, has ? static_cast<int>(B(value)->length) : -1, encoding))
                return false;
        }
    } else {
        count = *static_cast<const int*>(data);
        if (count < 2) {
            if (!WriteChars(w, "(", 1, encoding)) return false;
            column += 1;
        }
        if (count > 0) {
            const int indent = column - 1;
            char buf[0x40];
            buf[0] = ',';
            switch (type) {
            case BYTES: {
                static const char hex[] = "0123456789ABCDEF";
                if (!WriteChars(w, ": ", 2, encoding)) return false;
                const unsigned char* const bytes = GetByteArray();
                for (int i = 0; i < count; ++i) {
                    if (i % 32 == 0) {
                        if (!WriteChars(w, kCrLf, 2, encoding)) return false;
                        if (!WriteChars(w, "  ", 2, encoding)) return false;
                        if (!WriteChars(w, "  ", 2, encoding)) return false;
                    }
                    const char two[3] = {hex[bytes[i] >> 4], hex[bytes[i] & 15], 0};
                    if (!WriteChars(w, two, 2, encoding)) return false;
                }
                break;
            }
            case INTS: {
                const int* const items = reinterpret_cast<const int*>(GetByteArray());
                _itoa(items[0], buf + 1, 10);
                int length = static_cast<int>(std::strlen(buf + 1));
                column += length;
                if (!WriteChars(w, buf + 1, length, encoding)) return false;
                for (int i = 1; i < count; ++i) {
                    _itoa(items[i], buf + 1, 10);
                    length = static_cast<int>(std::strlen(buf));
                    if (!WriteWrap(w, buf, length, column, indent, encoding)) return false;
                }
                break;
            }
            case FLOATS: {
                buf[0x31] = 0;
                const float* const items = GetFloatArray();
                unsigned unused;
                Crt80::strncpy(buf + 1, ConfigFloatText(items[0], unused), 0x30);
                int length = static_cast<int>(std::strlen(buf + 1));
                column += length;
                if (!WriteChars(w, buf + 1, length, encoding)) return false;
                for (int i = 1; i < count; ++i) {
                    Crt80::strncpy(buf + 1, ConfigFloatText(items[i], unused), 0x30);
                    length = static_cast<int>(std::strlen(buf + 1));
                    if (!WriteWrap(w, buf, length + 1, column, indent, encoding)) return false;
                }
                break;
            }
            case PSTRINGS: {
                const PString* const items = GetPStringArray();
                AnsiString first(Text(items[0]));
                FormatDataString(AsString(first));
                int length = static_cast<int>(first.Size()) - 1;
                column += length;
                if (!WriteChars(w, HasText(first) ? B(first)->text : nullptr, length, encoding)) return false;
                for (int i = 1; i < count; ++i) {
                    if (!WriteWrap(w, ",", 1, column, indent, encoding)) return false;
                    AnsiString item(Text(items[i]));
                    FormatDataString(AsString(item));
                    // (the item wraps to the column itself, the comma before it to one less: 0x10012429)
                    if (!WriteWrap(w, HasText(item) ? B(item)->text : nullptr, static_cast<int>(item.Size()) - 1,
                                   column, indent + 1, encoding))
                        return false;
                }
                break;
            }
            default:
                break;
            }
        }
        if (count < 2 && !WriteChars(w, ")", 1, encoding)) return false;
    }
    if (!WriteChars(w, ";", 1, encoding)) return false;
    if (count >= 10) {
        if (!WriteChars(w, "//", 2, encoding)) return false;
        char digits[16];
        _itoa(count, digits, 10);
        if (!WriteChars(w, digits, static_cast<int>(std::strlen(digits)), encoding)) return false;
    }
    return true;
}

// 0x100124e0: "name(value)" then the children in braces one level deeper
bool ConfigData::WriteDataBlock(Writer& w, int depth, int encoding) const
{
    WriteTabs(w, depth, encoding);
    AnsiString nameText(Text(name));
    FormatDataString(AsString(nameText));
    if (!HasText(nameText) || B(nameText)->length == 0) nameText = "\"\"";
    const PString p = DataToPString();
    AnsiString value(Text(p));
    FormatDataString(AsString(value));
    if (HasText(nameText) && !WriteChars(w, B(nameText)->text, static_cast<int>(B(nameText)->length), encoding))
        return false;
    if (B(value)->length > 0) {
        if (!WriteChars(w, "(", 1, encoding)) return false;
        const bool has = HasText(value);
        if (!WriteChars(w, has ? B(value)->text : nullptr, has ? static_cast<int>(B(value)->length) : -1, encoding))
            return false;
        if (!WriteChars(w, ")", 1, encoding)) return false;
    }
    if (!WriteChars(w, kCrLf, 2, encoding)) return false;
    WriteTabs(w, depth, encoding);
    if (!WriteChars(w, "{", 1, encoding)) return false;
    if (!WriteChars(w, kCrLf, 2, encoding)) return false;
    if (!WriteChildList(w, depth + 1, encoding)) return false;
    WriteTabs(w, depth, encoding);
    return WriteChars(w, "}", 1, encoding);
}

bool ConfigData::WriteTabs(Writer& w, int depth, int encoding) const  // 0x100127c0: two spaces a level
{
    int n = depth;
    while (n > 8) {
        if (!WriteChars(w, kSpaces, 16, encoding)) return false;
        n -= 8;
    }
    if (n > 0 && !WriteChars(w, kSpaces, n * 2, encoding)) return false;
    return true;
}

// 0x10012830: nothing written (and false) for an empty text; UTF-16 through msvcr80's mbstowcs
bool ConfigData::WriteChars(Writer& w, const char* text, int length, int encoding) const
{
    wchar_t local[0x200];
    const void* out = text;
    int bytes = length;
    if (encoding == 2) {
        const unsigned size = static_cast<unsigned>(length * 2 + 2);
        wchar_t* const wide = size <= sizeof(local) ? local : static_cast<wchar_t*>(Alloc::New(size));
        out = wide;
        bytes = static_cast<int>(Crt80::mbstowcs(wide, text, static_cast<size_t>(length))) * 2;
    }
    const bool ok = bytes > 0 && w.Write(out, static_cast<unsigned long>(bytes), 1) > 0;
    if (out != text && out != local && out) Alloc::Delete(const_cast<void*>(out));
    return ok;
}

// 0x100128f0: {length | 0x40000000 when UTF-16, the bytes}
bool ConfigData::WritePString(Writer& w, const PString& s, int encoding) const
{
    bool ok = true;
    const char* const text = Text(s);
    const void* out = text;
    unsigned length = 0;
    if (s.node) {
        length = s.node->length + 1;
        if (length >= 1) length -= 1;
    }
    wchar_t local[0x200];
    unsigned header = 0;
    if (encoding == 2) {
        const unsigned size = length * 2 + 2;
        wchar_t* const wide = size <= sizeof(local) ? local : static_cast<wchar_t*>(Alloc::New(size));
        out = wide;
        const int n = static_cast<int>(Crt80::mbstowcs(wide, text, length));
        if (n < 0) ok = false;
        else length = static_cast<unsigned>(n) * 2;
        header = 0x40000000;
    }
    if (ok) {
        header |= length;
        ok = w.Write(&header, 4, 1) > 0;
        if (ok && length) ok = w.Write(out, length, 1) > 0;
    }
    if (out != text && out != local && out) Alloc::Delete(const_cast<void*>(out));
    return ok;
}

// 0x10012a20: escapes \ " ` and control characters (then the whole is quoted), or quotes a text that starts with a
// digit or holds one of # / space , ; = ( ) { }
void ConfigData::FormatDataString(String& s) const
{
    if (!HasText(s)) return;
    const Buf* const b = B(s);
    std::string out;
    bool escaped = false;
    for (unsigned i = 0; i < b->length; ++i) {
        const char c = b->text[i];
        switch (c) {
        case '`':
        case '\\':
        case '\b':
        case '\t':
        case '\r':
        case '\n':
        case '"':
            escaped = true;
            out += '\\';
            out += c == '\b' ? 'b' : c == '\t' ? 't' : c == '\n' ? 'n' : c == '\r' ? 'r' : c;
            break;
        default:
            out += c;
            break;
        }
    }
    if (escaped) {
        s = ("\"" + out + "\"").c_str();
        return;
    }
    const char* const t = b->text;
    const unsigned first = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(t[0])));
    bool quote = first >= '0' && first <= '9';
    for (const char* special = "#/ ,;=(){}"; !quote && *special; ++special) quote = std::strchr(t, *special) != nullptr;
    if (quote) s = ("\"" + std::string(t, b->length) + "\"").c_str();
}

// ---- binary (0x10012e60..0x10013ad0)
// 0x10012e60: name, one type byte, the value (strings as WritePString, arrays as their blocks), the child count,
// then each child
bool ConfigData::WriteBinFile(Writer& w, int encoding) const
{
    if (!WritePString(w, name, encoding)) return false;
    if (!w.Write(&type, 1, 1)) return false;
    switch (type) {
    case NONE:
        break;
    case INT:
    case FLOAT:
        if (!w.Write(&data, 4, 1)) return false;
        break;
    case PSTRING:
        if (!WritePString(w, AsPString(data), encoding)) return false;
        break;
    case PSTRINGS: {
        const PStringArray* const a = static_cast<const PStringArray*>(data);
        int count = a->count;
        if (!w.Write(&count, 4, 1)) return false;
        for (int i = 0; i < a->count; ++i)
            if (!WritePString(w, a->items[i], encoding)) return false;
        break;
    }
    case BYTES:
    case INTS:
    case FLOATS: {
        const int n = *static_cast<const int*>(data);
        const unsigned size = static_cast<unsigned>(type == BYTES ? n + 4 : n * 4 + 4);
        if (!size || !w.Write(data, size, 1)) return false;
        break;
    }
    default:
        return false;
    }
    int count = GetChildCount();
    if (!w.Write(&count, 4, 1)) return false;
    if (count > 0) {
        int i = 0;
        for (auto c = children.First(); c.node && i < count; ++c, ++i)
            if (!(*c.node)->WriteBinFile(w, encoding)) return false;
    }
    return true;
}

bool ConfigData::WriteBin(Writer& w, int encoding) const  // 0x10013410
{
    if (!encoding) encoding = w.Encoding();
    unsigned short word = 0x4443;  // "CD"
    if (!w.Write(&word, 2, 1)) return false;
    word = 0x101;
    if (!w.Write(&word, 2, 1)) return false;
    return WriteBinFile(w, encoding);
}

bool ConfigData::WriteBin(File& file, int encoding) const
{
    FileWriter w(file);
    return WriteBin(w, encoding);
}

bool ConfigData::WriteBin(PString& text, int encoding) const
{
    StringWriter w(text);
    return WriteBin(w, encoding);
}

// 0x10013560: the file made (flag: mode 2, else 1, with 0x100)
bool ConfigData::WriteBin(const char* path, bool flag, int encoding) const
{
    File f;
    if (f.Open(path, (flag ? 2u : 1u) | 0x100u, 0).code != 0) return false;
    const bool ok = WriteBin(f, encoding);
    f.Close();
    return ok;
}

bool ConfigData::WriteBin(const PString& base, const char* path, bool flag, int encoding) const
{
    File f;
    if (f.Open(base, path, (flag ? 2u : 1u) | 0x100u, 0).code != 0) return false;
    const bool ok = WriteBin(f, encoding);
    f.Close();
    return ok;
}

// 0x10013030: into this node as it is: the old value is not freed, children are appended
bool ConfigData::ReadBinFile(File& file)
{
    if (!file.Read(name)) return false;
    unsigned char t;
    if (file.Read(&t, 1, 1) == 0) return false;
    if (t >= 8) return false;
    if (t != NONE) {
        if (t < BYTES) {
            if (t == PSTRING) {
                PString s;
                if (!file.Read(s)) return false;
                if (!Text(s)) s = PString("");
                SetPStringData(s);
            } else if (file.Read(&data, 4, 1) == 0) {
                return false;
            }
        } else {
            long count;
            if (!file.Read(count)) return false;
            if (t == PSTRINGS) {
                PStringArray* const a = static_cast<PStringArray*>(Alloc::New(static_cast<unsigned>(count * 4 + 4)));
                std::memset(a->items, 0, static_cast<size_t>(count * 4));
                a->count = count;
                data = a;
                for (int i = 0; i < count; ++i) {
                    if (!file.Read(a->items[i])) {
                        for (int j = i - 1; j >= 0; --j) a->items[j].~PString();
                        if (data) Alloc::Delete(data);
                        data = nullptr;
                        return false;
                    }
                }
            } else {
                const unsigned bytes = static_cast<unsigned>(t == BYTES ? count : count * 4);
                int* const a = static_cast<int*>(Alloc::New(bytes + 4));
                data = a;
                a[0] = count;
                if (file.Read(a + 1, bytes, 1) == 0) {
                    if (data) Alloc::Delete(data);
                    data = nullptr;
                    return false;
                }
            }
        }
    }
    type = t;
    int count;
    if (!(file.Read(&count, 1, 4) > 0)) return false;
    if (count > 0) {
        const PString none;
        for (int i = 0; i < count; ++i) {
            void* const mem = operator new(sizeof(ConfigData));
            ConfigData* const c = mem ? ::new (mem) ConfigData(none) : nullptr;
            if (!c->ReadBinFile(file)) {
                if (c) delete c;
                return false;
            }
            children.Add(c);
        }
    }
    return true;
}

bool ConfigData::ReadBin(File& file)  // 0x10013800: "CD", version 0x101, then the value cleared and read
{
    unsigned short word = 0;
    if (file.Read(&word, 2, 1) == 0 || word != 0x4443) return false;
    if (file.Read(&word, 2, 1) == 0 || word != 0x101) return false;
    ClearData();
    return ReadBinFile(file);
}

bool ConfigData::ReadBin(const char* path)
{
    File f;
    if (f.Open(path, 0x10, 0).code != 0) return false;
    const bool ok = ReadBin(f);
    f.Close();
    return ok;
}

bool ConfigData::ReadBin(const PString& base, const char* path)
{
    File f;
    if (f.Open(base, path, 0x10, 0).code != 0) return false;
    const bool ok = ReadBin(f);
    f.Close();
    return ok;
}

}  // namespace Jet
