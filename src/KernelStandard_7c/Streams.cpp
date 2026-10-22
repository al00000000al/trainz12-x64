// Unit U9a of KernelStandard_7c: DynamicBuffer (0x1002e120..0x1002ebaf, plus the getters it shares with other
// classes) and the reader / writer it hands to the strings' binary Read / Write. Notes:
// D:/trainz12-decomp/notes/KernelStandard.md ("U9a").
#include <jet/KernelStandard.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

// 0x1005590c / 0x10055918: {vftable, buffer}; the strings call slot 1
class BufferReader : public BinReader {
public:
    explicit BufferReader(const DynamicBuffer& b) : buffer(&b) {}
    unsigned long Read(void* dst, unsigned long size) override  // 0x1002e120 (= DynamicBuffer::Read(dst, size, 1))
    {
        return buffer->Read(dst, size, 1);
    }

private:
    const DynamicBuffer* buffer;
};

class BufferWriter : public BinWriter {
public:
    explicit BufferWriter(DynamicBuffer& b) : buffer(&b) {}
    unsigned long Write(const void* src, unsigned long size) override { return buffer->Write(src, size, 1); }

private:
    DynamicBuffer* buffer;
};

const unsigned long kIncrement = 0x200;

}  // namespace

// ---- construction

DynamicBuffer::DynamicBuffer(unsigned long capacity_, unsigned long increment_)
    : data(nullptr), position(0), capacity(capacity_), size(0), increment(increment_ ? increment_ : kIncrement),
      external(false)
{
    if (capacity) data = static_cast<unsigned char*>(Alloc::New(capacity));
}

// the data copied unless external
DynamicBuffer::DynamicBuffer(void* data_, unsigned long size_, bool external_)
    : data(nullptr), position(0), capacity(size_), size(size_), increment(kIncrement), external(external_)
{
    if (!external && data_ && size_) {
        data = static_cast<unsigned char*>(Alloc::New(size_));
        std::memcpy(data, data_, size_);
    } else {
        data = static_cast<unsigned char*>(data_);
    }
}

// (an owned buffer is copied by its capacity; an external one, or none, is shared)
DynamicBuffer::DynamicBuffer(const DynamicBuffer& o)
    : data(nullptr), position(o.position), capacity(o.capacity), size(o.size), increment(o.increment),
      external(o.external)
{
    if (!external && o.data && capacity) {
        data = static_cast<unsigned char*>(Alloc::New(capacity));
        std::memcpy(data, o.data, capacity);
    } else {
        data = o.data;
    }
}

DynamicBuffer& DynamicBuffer::operator=(const DynamicBuffer& o)
{
    if (this == &o) return *this;
    if (!external) {
        if (data) Alloc::Delete(data);
        data = nullptr;
    }
    position = o.position;
    capacity = o.capacity;
    size = o.size;
    increment = o.increment;
    external = o.external;
    if (!external && o.data && capacity) {
        data = static_cast<unsigned char*>(Alloc::New(capacity));
        std::memcpy(data, o.data, capacity);
    } else {
        data = o.data;
    }
    return *this;
}

DynamicBuffer::~DynamicBuffer() { Clean(); }

// (the increment and the external flag stay)
void DynamicBuffer::Clean()
{
    if (!external && data) Alloc::Delete(data);
    data = nullptr;
    capacity = 0;
    size = 0;
    position = 0;
}

void DynamicBuffer::Reset()
{
    size = 0;
    position = 0;
}

void DynamicBuffer::Rewind() { position = 0; }

void DynamicBuffer::ReInit(unsigned long capacity_, unsigned long increment_)
{
    if (!external && data) Alloc::Delete(data);
    data = nullptr;
    size = 0;
    position = 0;
    capacity = capacity_;
    if (capacity) data = static_cast<unsigned char*>(Alloc::New(capacity));
    external = false;
    increment = increment_ ? increment_ : kIncrement;
}

bool DynamicBuffer::Seek(unsigned long p)
{
    if (p > capacity) {
        position = capacity;
        return false;
    }
    position = p;
    return true;
}

bool DynamicBuffer::SetSize(unsigned long n)
{
    if (n > capacity) {
        size = capacity;
        return false;
    }
    size = n;
    return true;
}

void DynamicBuffer::SetData(void* data_, unsigned long size_, bool external_)
{
    if (!external) {
        if (data) Alloc::Delete(data);
        data = nullptr;
    }
    if (!external_ && data_ && size_) {
        data = static_cast<unsigned char*>(Alloc::New(size_));
        std::memcpy(data, data_, size_);
    } else {
        data = static_cast<unsigned char*>(data_);
    }
    capacity = size_;
    size = size_;
    external = external_;
    increment = kIncrement;
    position = 0;
}

bool DynamicBuffer::IsReadOnly() { return external; }
unsigned long DynamicBuffer::GetSize() const { return size; }
unsigned long DynamicBuffer::GetLength() const { return capacity; }
unsigned long DynamicBuffer::GetIncrement() const { return increment; }
unsigned long DynamicBuffer::GetCurrentPos() const { return position; }
void* DynamicBuffer::GetData() const { return data; }
void* DynamicBuffer::GetCurrentData() const { return data + position; }

// the capacity rounded up to the increment past position + n; the old capacity's bytes move over
void DynamicBuffer::AddLength(unsigned long n)
{
    if (external || n <= capacity - position) return;
    const unsigned long grown = (position + increment + n - 1) / increment * increment;
    unsigned char* const p = static_cast<unsigned char*>(Alloc::New(grown));
    if (data) {
        std::memcpy(p, data, capacity);
        if (data) Alloc::Delete(data);
        data = nullptr;
    }
    capacity = grown;
    data = p;
}

// ---- writes: at the position, which then moves on (the size follows it)

namespace {

template <class T>
bool Put(DynamicBuffer& b, T v)
{
    if (b.external) return false;
    b.AddLength(sizeof(T));
    if (!b.data) return false;
    std::memcpy(b.data + b.position, &v, sizeof(T));
    b.position += sizeof(T);
    if (b.position > b.size) b.size = b.position;
    return true;
}

}  // namespace

bool DynamicBuffer::Write(short v) { return Put(*this, v); }
bool DynamicBuffer::Write(unsigned short v) { return Put(*this, v); }
bool DynamicBuffer::Write(signed char v) { return Put(*this, v); }
bool DynamicBuffer::Write(unsigned char v) { return Put(*this, v); }
bool DynamicBuffer::Write32(int v) { return Put(*this, v); }
bool DynamicBuffer::Write32(unsigned int v) { return Put(*this, v); }
bool DynamicBuffer::Write(long v) { return Put(*this, v); }
bool DynamicBuffer::Write(unsigned long v) { return Put(*this, v); }
bool DynamicBuffer::Write(float v) { return Put(*this, v); }
bool DynamicBuffer::Write(bool v) { return Put(*this, static_cast<int>(v != 0)); }

bool DynamicBuffer::Write(const AnsiString& s)
{
    BufferWriter w(*this);
    return s.Write(w);
}
bool DynamicBuffer::Write(const String& s)
{
    BufferWriter w(*this);
    return static_cast<const AnsiString&>(s).Write(w);
}
bool DynamicBuffer::Write(const UnicodeString& s)
{
    BufferWriter w(*this);
    return s.Write(w);
}
bool DynamicBuffer::Write(const PString& s)
{
    BufferWriter w(*this);
    return s.Write(w);
}

unsigned long DynamicBuffer::Write(const void* src, unsigned long size_, unsigned long count)
{
    const unsigned long n = size_ * count;
    if (!src || !n || external) return 0;
    AddLength(n);
    if (!data) return 0;
    std::memcpy(data + position, src, n);
    position += n;
    if (position > size) size = position;
    return size_;
}

// ---- reads: what is left of the bytes written, from the position

namespace {

// true when any byte is left (the copy is skipped without data)
bool Take(const DynamicBuffer& cb, void* dst, unsigned long want)
{
    DynamicBuffer& b = const_cast<DynamicBuffer&>(cb);
    if (!dst || b.position >= b.size) return false;
    const unsigned long n = b.size - b.position < want ? b.size - b.position : want;
    if (b.data && n) {
        std::memcpy(dst, b.data + b.position, n);
        b.position += n;
    }
    return n != 0;
}

}  // namespace

bool DynamicBuffer::Read(short& v) const { return Take(*this, &v, 2); }
bool DynamicBuffer::Read(unsigned short& v) const { return Take(*this, &v, 2); }
bool DynamicBuffer::Read(signed char& v) const { return Take(*this, &v, 1); }
bool DynamicBuffer::Read(unsigned char& v) const { return Take(*this, &v, 1); }
bool DynamicBuffer::Read(long& v) const { return Take(*this, &v, 4); }
bool DynamicBuffer::Read(unsigned long& v) const { return Take(*this, &v, 4); }
bool DynamicBuffer::Read(float& v) const { return Take(*this, &v, 4); }

bool DynamicBuffer::Read(bool& v) const
{
    long word = 0;
    const bool ok = Read(word);
    v = word != 0;
    return ok;
}

bool DynamicBuffer::Read32(int& v) const
{
    v = 0;
    return Take(*this, &v, 4);
}
bool DynamicBuffer::Read32(unsigned int& v) const
{
    v = 0;
    return Take(*this, &v, 4);
}

bool DynamicBuffer::Read(PString& s) const
{
    BufferReader r(*this);
    return s.Read(r);
}
bool DynamicBuffer::Read(AnsiString& s) const
{
    BufferReader r(*this);
    return s.Read(r);
}
bool DynamicBuffer::Read(String& s) const
{
    BufferReader r(*this);
    return static_cast<AnsiString&>(s).Read(r);
}
bool DynamicBuffer::Read(UnicodeString& s) const
{
    BufferReader r(*this);
    return s.Read(r);
}

unsigned long DynamicBuffer::Read(void* dst, unsigned long size_, unsigned long count) const
{
    DynamicBuffer& b = const_cast<DynamicBuffer&>(*this);
    unsigned long n = size_ * count;
    if (!dst || !n || b.position >= b.size) return 0;
    if (n > b.size - b.position) n = b.size - b.position;
    if (b.data && n) {
        std::memcpy(dst, b.data + b.position, n);
        b.position += n;
    }
    return n;
}

// ??_F (the default constructor closure, exported under that name by implemented.txt)
void __fastcall Jet_DynamicBufferDefaultCtor(DynamicBuffer* p) { ::new (p) DynamicBuffer(); }

}  // namespace Jet
