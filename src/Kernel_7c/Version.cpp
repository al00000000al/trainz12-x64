// Jet::Version, reconstructed from Kernel_7c.dll 0x1000daf0..0x1000dba0.
#include <jet/Version.h>

#include <new>

namespace Jet {

Version::Version(unsigned char major_, unsigned char minor_, unsigned short build_)
    : build(build_), minor(minor_), major(major_) {}

Version::Version(unsigned long packed)
{
    *reinterpret_cast<unsigned long*>(this) = packed;
}

Version& Version::operator=(const Version& o)
{
    *reinterpret_cast<unsigned long*>(this) = o.Packed();
    return *this;
}

bool Version::operator==(const Version& o) const { return Packed() == o.Packed(); }
bool Version::operator!=(const Version& o) const { return Packed() != o.Packed(); }
bool Version::operator<(const Version& o) const { return Packed() < o.Packed(); }
bool Version::operator>(const Version& o) const { return Packed() > o.Packed(); }
bool Version::operator<=(const Version& o) const { return !(o.Packed() < Packed()); }
bool Version::operator>=(const Version& o) const { return !(Packed() < o.Packed()); }

}  // namespace Jet

// ??_FVersion@Jet@@QAEXXZ, the compiler's "default constructor closure" (0x1000dbb0), exported under that name
// through an alias in implemented.txt
extern "C" void __fastcall Jet_VersionDefaultCtorClosure(Jet::Version* v) { new (v) Jet::Version(); }
