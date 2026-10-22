// Unit U9c of KernelStandard_7c: JIRFFile and its size writers (0x1002c6c0..0x1002d4b0, 0x10001860, 0x10015600).
// The result table stays the original's export. Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U9c").
#include <jet/KernelStandard.h>

#include <cstring>
#include <malloc.h>

namespace Jet {

namespace {

const unsigned long kMagic = 0x4652494a;  // "JIRF"

File::Path& PathOf(File& f) { return *reinterpret_cast<File::Path*>(reinterpret_cast<unsigned char*>(&f) + 0x38); }
const File::Path& PathOf(const File& f)
{
    return *reinterpret_cast<const File::Path*>(reinterpret_cast<const unsigned char*>(&f) + 0x38);
}

// the PString of a C string (0x1000d180)
void AssignText(PString& p, const char* text)
{
    PString made;
    if (text)
        made.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text),
                                             static_cast<unsigned>(std::strlen(text)));
    p = made;
}

}  // namespace

// ---- JIRFFile

JIRFFile::JIRFFile() : slaveFile(nullptr), resource(nullptr) {}

// the File as MSVC8 copies it: the fields, two flag bits of +0x28, the path through Path::operator=
JIRFFile::JIRFFile(const JIRFFile& o) : File(o), slaveFile(o.slaveFile), resource(o.resource)
{
    PathOf(*this) = PathOf(o);
}

JIRFFile& JIRFFile::operator=(const JIRFFile& o)
{
    const unsigned char flags = opaque[0x28];
    std::memcpy(opaque, o.opaque, 0x38);
    opaque[0x28] = static_cast<unsigned char>((flags & ~3) | (o.opaque[0x28] & 3));
    PathOf(*this) = PathOf(o);
    slaveFile = o.slaveFile;
    resource = o.resource;
    return *this;
}

JIRFFile::~JIRFFile() { Close(); }

// File::Open, then a file slave and a resource slave over it (written when a mode bit of 0xf is set); an earlier
// pair is left as it is
JIRFFile::Result __cdecl JIRFFile::OpenJIRF(const char* path, const String& key, unsigned long mode)
{
    if (File::Open(path, mode, 0).code) return 4;
    slaveFile = new IOSlaveFile(this);
    resource = new IOSlaveResource(slaveFile, key, (mode & 0xf) != 0);
    return 0;
}

JIRFFile::Result __cdecl JIRFFile::OpenJIRF(const PString& base, const char* path, const String& key,
                                            unsigned long mode)
{
    if (File::Open(base, path, mode, 0).code) return 4;
    slaveFile = new IOSlaveFile(this);
    resource = new IOSlaveResource(slaveFile, key, (mode & 0xf) != 0);
    return 0;
}

void __cdecl JIRFFile::Close()
{
    if (resource) delete resource;
    resource = nullptr;
    if (slaveFile) delete slaveFile;
    slaveFile = nullptr;
    if (Handle() != -1) File::Close();
}

unsigned long __cdecl JIRFFile::Write(const void* src, unsigned long size) { return resource->WriteData(src, size); }
unsigned long __cdecl JIRFFile::Read(void* dst, unsigned long size) { return resource->ReadData(dst, size); }
bool __cdecl JIRFFile::Read32(unsigned int& v) { return resource->ReadData(&v, 4) != 0; }

unsigned long __cdecl JIRFFile::SeekCur(long offset)
{
    if (!resource->Seek(resource->Tell() + offset)) return 0xffffffffu;
    return resource->Tell();
}

unsigned long __cdecl JIRFFile::Seek(long position)
{
    if (!resource->Seek(static_cast<unsigned long>(position))) return 0xffffffffu;
    return resource->Tell();
}

unsigned long __cdecl JIRFFile::Tell() const { return resource->Tell(); }

unsigned int __cdecl JIRFFile::WriteHeader(unsigned long id)
{
    const unsigned long at = resource->Tell();
    unsigned long words[3] = {kMagic, 0, id};
    resource->WriteData(&words[0], 4);
    resource->WriteData(&words[1], 4);
    resource->WriteData(&words[2], 4);
    return at + 4;
}

unsigned int __cdecl JIRFFile::WriteBlock(unsigned long id, unsigned int size, void* data)
{
    const unsigned long at = resource->Tell();
    unsigned long words[2] = {id, size};
    resource->WriteData(&words[0], 4);
    resource->WriteData(&words[1], 4);
    if (data) resource->WriteData(data, size);
    return at + 4;
}

// (the fields are read whatever comes back: a short stream leaves them as they were on the stack)
JIRFFile::Result __cdecl JIRFFile::ReadBlock(unsigned long id, unsigned int& size, void* data)
{
    unsigned long got[2];
    resource->ReadData(&got[0], 4);
    resource->ReadData(&got[1], 4);
    if (got[0] != id) {
        size = got[1];
        return 3;
    }
    if (data) {
        const unsigned long n = got[1] > size ? size : got[1];
        const int skip = static_cast<int>(got[1] - resource->ReadData(data, n));
        if (skip > 0) SeekCur(skip);
    }
    size = got[1];
    return 0;
}

JIRFFile::Result __cdecl JIRFFile::ReadBlockHeader(unsigned long& id, unsigned int& size)
{
    resource->ReadData(&id, 4);
    resource->ReadData(&size, 4);
    return 0;
}

JIRFFile::Result __cdecl JIRFFile::ReadHeader(unsigned long id)
{
    unsigned long w[3];
    resource->ReadData(&w[0], 4);
    resource->ReadData(&w[1], 4);
    resource->ReadData(&w[2], 4);
    if (w[0] != kMagic) return 2;
    return w[2] != id ? 3 : 0;
}

JIRFFile::Result __cdecl JIRFFile::ReadHeader(unsigned long id, unsigned int& size)
{
    unsigned long w[3];
    resource->ReadData(&w[0], 4);
    resource->ReadData(&w[1], 4);
    resource->ReadData(&w[2], 4);
    size = w[1] - 4;
    if (w[0] != kMagic) return 2;
    return w[2] != id ? 3 : 0;
}

void __cdecl JIRFFile::WriteAt(unsigned int position, unsigned long value)
{
    const unsigned long here = resource->Tell();
    if (resource->Seek(position)) resource->Tell();
    resource->WriteData(&value, 4);
    if (resource->Seek(here)) resource->Tell();
}

void __cdecl JIRFFile::WriteAt(unsigned int position) { WriteAt(position, resource->Tell() - 4 - position); }
void __cdecl JIRFFile::WriteSizeAt(unsigned int position) { WriteAt(position, resource->Tell() - 4 - position); }

// {length + 1, the text, zeros to 4 bytes}; an empty string is -1, none is 0
void __cdecl JIRFFile::WritePadded(const PString& s)
{
    unsigned long n = s.node ? s.node->length + 1 : 0;
    if (s.node && n == 1) {
        unsigned long none = 0xffffffffu;
        resource->WriteData(&none, 4);
        return;
    }
    resource->WriteData(&n, 4);
    if (n > 1) {
        --n;
        resource->WriteData(s.node ? s.node->text : nullptr, n);
        unsigned long zero = 0;
        resource->WriteData(&zero, (0u - n) & 3);
    }
}

void __cdecl JIRFFile::WritePadded(const String& s)
{
    const unsigned* const b = *reinterpret_cast<const unsigned* const*>(&s);
    unsigned long n = b[0] ? b[1] + 1 : 0;
    if (b[0] && n == 1) {
        unsigned long none = 0xffffffffu;
        resource->WriteData(&none, 4);
        return;
    }
    resource->WriteData(&n, 4);
    if (n > 1) {
        --n;
        resource->WriteData(b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr, n);
        unsigned long zero = 0;
        resource->WriteData(&zero, (0u - n) & 3);
    }
}

// (a short stream leaves the count or the text as they were on the stack, as in the original)
JIRFFile::Result __cdecl JIRFFile::ReadPadded(PString& s)
{
    unsigned long n;
    resource->ReadData(&n, 4);
    if (n == 0xffffffffu) {
        AssignText(s, "");
    } else if (n <= 1) {
        s = PString();
    } else {
        char* const text = static_cast<char*>(_alloca(n));
        --n;
        resource->ReadData(text, n);
        text[n] = 0;
        AssignText(s, text);
        if (resource->Seek(resource->Tell() + ((0u - n) & 3))) resource->Tell();
    }
    return 0;
}

JIRFFile::Result __cdecl JIRFFile::ReadPadded(String& s)
{
    unsigned long n;
    resource->ReadData(&n, 4);
    AnsiString& a = s;
    if (n == 0xffffffffu) {
        a = "";
    } else if (n <= 1) {
        a = AnsiString();
    } else {
        char* const text = static_cast<char*>(_alloca(n));
        --n;
        resource->ReadData(text, n);
        text[n] = 0;
        a = static_cast<const char*>(text);
        if (resource->Seek(resource->Tell() + ((0u - n) & 3))) resource->Tell();
    }
    return 0;
}

// ---- the size writers

JIRFWriteSize::JIRFWriteSize(JIRFFile& f) : file(&f) {}
JIRFWriteSize::~JIRFWriteSize() { file->WriteAt(position, file->Tell() - 4 - position); }
const JIRFWriteSize& __cdecl JIRFWriteSize::operator=(const JIRFWriteSize&) { return *this; }

JIRFWriteHeader::JIRFWriteHeader(JIRFFile& f, unsigned long id) : JIRFWriteSize(f) { position = f.WriteHeader(id); }
JIRFWriteHeader::~JIRFWriteHeader() {}

JIRFWriteBlock::JIRFWriteBlock(JIRFFile& f, unsigned long id) : JIRFWriteSize(f)
{
    const unsigned long at = f.Tell();
    unsigned long words[2] = {id, 0};
    f.Write(&words[0], 4);
    f.Write(&words[1], 4);
    position = at + 4;
}
JIRFWriteBlock::~JIRFWriteBlock() {}

}  // namespace Jet
