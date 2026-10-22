// Jet::UId, reconstructed from Kernel_7c.dll 0x10001530..0x10001920 and 0x1000d790..0x1000d9c0.
#include <jet/KernelStandard.h>
#include <jet/UId.h>

#include <objbase.h>

#include <cstdlib>
#include <cstring>

namespace Jet {

const UId UIdNull(0);

UId::UId() {}

UId::UId(int)
{
    Zero();
}

UId::UId(unsigned long d1, unsigned short d2, unsigned short d3) : data1(d1), data2(d2), data3(d3)
{
    std::memset(data4, 0, sizeof data4);
}

UId::UId(unsigned long d1, unsigned short d2, unsigned short d3, const unsigned char* const d4)
    : data1(d1), data2(d2), data3(d3)
{
    if (d4)
        std::memcpy(data4, d4, sizeof data4);
    else
        std::memset(data4, 0, sizeof data4);
}

// "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", any case. Like the original: copies at most 37 chars, upper-cases,
// cuts the first three fields in place, then reads the 8 trailing bytes two hex digits at a time.
UId::UId(const char* text)
{
    char buf[37];
    std::strncpy(buf, text, 37);
    buf[36] = 0;  // the original also overwrites the 37th char
    _strupr(buf);
    buf[8] = 0;
    buf[13] = 0;
    buf[18] = 0;
    data1 = std::strtoul(buf, nullptr, 16);
    data2 = static_cast<unsigned short>(std::strtoul(buf + 9, nullptr, 16));
    data3 = static_cast<unsigned short>(std::strtoul(buf + 14, nullptr, 16));

    const char* p = buf + 19;  // two bytes, then '-' at 23, then six bytes
    for (int i = 0; i < 8; ++i) {
        if (i == 2) ++p;
        char pair[3] = {p[0], p[1], 0};
        data4[i] = static_cast<unsigned char>(std::strtoul(pair, nullptr, 16));
        p += 2;
    }
}

UId& UId::operator=(const UId& o)
{
    std::memcpy(this, &o, sizeof(UId));
    return *this;
}

void UId::Zero()
{
    std::memset(this, 0, sizeof(UId));
}

bool __cdecl UId::operator==(const UId& o) const { return std::memcmp(this, &o, sizeof(UId)) == 0; }
bool __cdecl UId::operator!=(const UId& o) const { return !(*this == o); }

// field by field, unsigned; data4 as bytes (memcmp)
bool __cdecl UId::operator<(const UId& o) const
{
    if (data1 != o.data1) return data1 < o.data1;
    if (data2 != o.data2) return data2 < o.data2;
    if (data3 != o.data3) return data3 < o.data3;
    return std::memcmp(data4, o.data4, sizeof data4) < 0;
}

bool __cdecl UId::operator>(const UId& o) const
{
    if (data1 != o.data1) return data1 > o.data1;
    if (data2 != o.data2) return data2 > o.data2;
    if (data3 != o.data3) return data3 > o.data3;
    return std::memcmp(data4, o.data4, sizeof data4) > 0;
}

bool __cdecl UId::operator<=(const UId& o) const { return !(*this > o); }
bool __cdecl UId::operator>=(const UId& o) const { return !(*this < o); }

__cdecl UId::operator bool() const
{
    const unsigned int* w = reinterpret_cast<const unsigned int*>(this);
    return (w[0] | w[1] | w[2] | w[3]) != 0;
}

bool __cdecl UId::operator!() const { return !static_cast<bool>(*this); }

unsigned int __cdecl UId::GetHashID() const
{
    const unsigned int* w = reinterpret_cast<const unsigned int*>(this);
    return w[0] ^ w[1] ^ w[2] ^ w[3];
}

// Binary form: data1, data2, data3 through the typed writers, then the 8 bytes as one block.
bool __cdecl UId::Write(DynamicBuffer& buf)
{
    return buf.Write(data1) && buf.Write(data2) && buf.Write(data3) && buf.Write(data4, 8, 1) != 0;
}

bool __cdecl UId::Read(DynamicBuffer& buf)
{
    return buf.Read(data1) && buf.Read(data2) && buf.Read(data3) && buf.Read(data4, 8, 1) != 0;
}

PString __cdecl UId::GetString() const
{
    PString s;
    s.Setf("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", data1, unsigned(data2), unsigned(data3),
           unsigned(data4[0]), unsigned(data4[1]), unsigned(data4[2]), unsigned(data4[3]), unsigned(data4[4]),
           unsigned(data4[5]), unsigned(data4[6]), unsigned(data4[7]));
    return s;
}

void __cdecl UId::SetRnd()
{
    // the original keeps a lazily constructed static generator, randomised on first use
    static Rand rng;
    static bool randomised = false;
    if (!randomised) {
        rng.Randomise();
        randomised = true;
    }
    GUID g;
    if (CoCreateGuid(&g) == S_OK) {
        static_assert(sizeof(GUID) == sizeof(UId));
        std::memcpy(this, &g, sizeof g);
        return;
    }
    data1 = Time::Ms();
    data2 = static_cast<unsigned short>(rng.NextInt(0xffff));
    data3 = static_cast<unsigned short>(rng.NextInt(0xffff));
    for (int i = 0; i < 8; ++i)
        data4[i] = static_cast<unsigned char>(rng.NextInt(0xff));
}

}  // namespace Jet
