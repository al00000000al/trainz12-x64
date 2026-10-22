// Jet::Version (Kernel_7c). 4 bytes: build in the low word, then minor, then major, so comparing versions is
// comparing the packed value. Layout and calling conventions must match the original exports exactly.
#pragma once

namespace Jet {

struct Version {
    unsigned short build;
    unsigned char minor;
    unsigned char major;

    Version(unsigned char major_ = 0, unsigned char minor_ = 0, unsigned short build_ = 0);  // ??0Version@Jet@@QAE@EEG@Z
    Version(unsigned long packed);                                                            // ??0Version@Jet@@QAE@K@Z

    Version& operator=(const Version& o);
    bool operator==(const Version& o) const;
    bool operator!=(const Version& o) const;
    bool operator<(const Version& o) const;
    bool operator>(const Version& o) const;
    bool operator<=(const Version& o) const;
    bool operator>=(const Version& o) const;

    unsigned long Packed() const { return *reinterpret_cast<const unsigned long*>(this); }
};

static_assert(sizeof(Version) == 4);

}  // namespace Jet
