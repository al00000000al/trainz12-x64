// Jet::Compress / Jet::Encrypt plugin bases, their kernel implementations (LZSSCompress, BFEncrypt), the kernel's
// plugin table JetGetPluginsKernel and the CreateInstance helpers. The codecs themselves are JetLzss and
// AuranBlowfish, verified byte for byte against the original.
#include <jet/EditItem.h>
#include <jet/Plugin.h>

#include "AuranBlowfish.h"
#include "JetLzss.h"

#include <vector>

namespace Jet {

const UId Compress::uid("8DBDFEB0-52F4-4c1b-BA91-6A7B1CCC6D3E");
const UId Encrypt::uid("AAF296E0-17A7-11d5-A9C2-00E01898A4A9");

// Encrypt::results points at the text "UNKNOWN" in the original (0x100106ec); there is no fuller table.
struct ResultTable {
    char text[8];
};
static const ResultTable kEncryptResults = {"UNKNOWN"};
const ResultTable* const Encrypt::results = &kEncryptResults;

// ---------------------------------------------------------------------------------------------------------------
// Compress

Compress::Compress() {}
Compress::~Compress() {}
bool Compress::Init() { return true; }

unsigned long Compress::CompressBlock(void* dst, unsigned long dstSize, const void* src, unsigned long srcSize,
                                      unsigned int ratio)  // 0x10002960
{
    IOSlaveMemory in(const_cast<void*>(src), srcSize);
    IOSlaveMemory out(dst, dstSize);
    if (!CompressStream(out.AsSlave(), in.AsSlave(), srcSize, ratio)) return 0;
    return out.AsSlave()->Tell();
}

unsigned long Compress::DecompressBlock(void* dst, unsigned long dstSize, const void* src, unsigned long srcSize)
{
    IOSlaveMemory in(const_cast<void*>(src), srcSize);
    IOSlaveMemory out(dst, dstSize);
    if (!DecompressStream(out.AsSlave(), in.AsSlave(), srcSize)) return 0;
    return out.AsSlave()->Tell();
}

// ---------------------------------------------------------------------------------------------------------------
// What MSVC emits only for complete objects, which these abstract classes never are: their vbtable (vbptr at +4,
// ReferenceCount at +0x10; the original folds both into one at 0x1051c) and their vbase destructors (0x100026a0,
// 0x10001ce0: the class destructor, then ~ReferenceCount). Exported under the original names (implemented.txt).

extern const int CompressEncryptVbtable[2];
const int CompressEncryptVbtable[2] = {0, 0xc};

void __fastcall Jet_CompressVbaseDestructor(Compress* self)
{
    self->Compress::~Compress();
}

void __fastcall Jet_EncryptVbaseDestructor(Encrypt* self)
{
    self->Encrypt::~Encrypt();
}

// ---------------------------------------------------------------------------------------------------------------
// LZSSCompress

LZSSCompress::LZSSCompress() {}
LZSSCompress::~LZSSCompress() { Reset(); }
void LZSSCompress::Reset() {}

static bool WriteAll(IOSlave* dst, const std::vector<uint8_t>& data)
{
    return data.empty() || dst->WriteData(data.data(), static_cast<unsigned long>(data.size())) == data.size();
}

// 0x10006ec0: read `srcSize` bytes from the source's position, write the stream at the destination's position;
// if that fails, go back and store the data uncompressed (header 0).
bool LZSSCompress::CompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize, unsigned int level)
{
    const unsigned long dstStart = dst->Tell(), srcStart = src->Tell();
    std::vector<uint8_t> input(srcSize);
    const unsigned long got = srcSize ? src->ReadData(input.data(), srcSize) : 0;
    if (got == srcSize) {
        const std::vector<uint8_t> packed = JetLzss::Compress(input.data(), srcSize, level);
        if (WriteAll(dst, packed)) return true;
    }
    if (!dst->Seek(dstStart) || !src->Seek(srcStart)) return false;
    const uint8_t stored = 0;
    if (dst->WriteData(&stored, 1) != 1) return false;
    if (src->ReadData(input.data(), srcSize) != srcSize) return false;
    return WriteAll(dst, input);
}

// 0x10007150
bool LZSSCompress::DecompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize)
{
    if (srcSize == 0) return false;
    std::vector<uint8_t> input(srcSize);
    if (src->ReadData(input.data(), srcSize) != srcSize) return false;
    std::vector<uint8_t> output;
    const bool ok = JetLzss::Decompress(input.data(), srcSize, output);
    return WriteAll(dst, output) && ok;
}

// ---------------------------------------------------------------------------------------------------------------
// Encrypt / BFEncrypt

Encrypt::Encrypt() {}
Encrypt::~Encrypt() {}

class BFEncrypt : public Encrypt {
public:
    EResult GetCaps(Caps& caps) override  // 0x100021f0
    {
        caps.keyMinSize = 1;
        caps.keyMaxSize = 0x100;
        caps.bufferMinSize = 8;
        caps.bufferMaxSize = 0xffffffff;
        caps.bufferAlignment = 0x40;
        return EResult(0);
    }
    EResult SetKey(const void* key, unsigned long size) override { return EResult(AuranBlowfish::SetKey(state, key, size)); }
    unsigned long GetOptimalSize(unsigned long size) override { return AuranBlowfish::OptimalSize(size); }
    EResult EncryptData(void* data, unsigned long size) override { return EResult(AuranBlowfish::Encrypt(state, data, size)); }
    EResult DecryptData(void* data, unsigned long size) override { return EResult(AuranBlowfish::Decrypt(state, data, size)); }

private:
    AuranBlowfish::State state;  // +0x0c, as in the original (P at +0x0c, S at +0x54)
};

// sizes of the original objects (other modules allocate LZSSCompress and call the exported constructor)
JET_LAYOUT32(sizeof(PluginType) == 0x20);
JET_LAYOUT32(sizeof(PluginInstance) == 0x14);  // vbptr, factory, vtordisp, ReferenceCount {vptr, count}
JET_LAYOUT32(sizeof(LZSSCompress) == 0x115c);
JET_LAYOUT32(sizeof(BFEncrypt) == 0x1060);

// ---------------------------------------------------------------------------------------------------------------
// the kernel's plugin table (0x10005230)

extern "C" void __cdecl JetGetPluginsKernel()
{
    const UId compressGroup("8DBDFEB0-52F4-4c1b-BA91-6A7B1CCC6D3E");
    new PluginFactoryOf<LZSSCompress, Compress>(PString("LZSS"), UId("8DBDFEB1-52F4-4c1b-BA91-6A7B1CCC6D3E"), compressGroup);
    const UId encryptGroup("AAF296E0-17A7-11d5-A9C2-00E01898A4A9");
    new PluginFactoryOf<BFEncrypt, Encrypt>(PString("BlowFish"), UId("AAF296E1-17A7-11d5-A9C2-00E01898A4A9"), encryptGroup);
}

// ---------------------------------------------------------------------------------------------------------------
// CreateInstance helpers. A factory's CreateInstance is vtable slot 3 (__cdecl); found factories hold a reference
// that is dropped at the end.

// lets these functions reach the protected lookups and call the factory slot generically
struct PluginAccess : PluginBase {
    using PluginBase::LookupPluginType;
    static void* Create(PluginType* factory)
    {
        using Fn = void*(__cdecl*)(PluginType*);
        return (*reinterpret_cast<Fn**>(factory))[3](factory);
    }
};

template <class T>
static T* ReleaseIfInitFails(T* obj, bool ok)
{
    if (!ok && obj) {
        static_cast<const ReferenceCount*>(obj)->ReleaseReference();
        return nullptr;
    }
    return obj;
}

Compress* __cdecl Compress::CreateInstance(const UId& id)  // 0x10002780
{
    PluginType* factory = nullptr;
    PluginAccess::LookupPluginType(&factory, id);
    Compress* c = nullptr;
    if (factory) {
        c = static_cast<Compress*>(PluginAccess::Create(factory));
        if (c) c = ReleaseIfInitFails(c, c->Init());
    }
    if (factory) factory->ReleaseReference();
    return c;
}

Compress* __cdecl Compress::CreateInstance(const PString& name)  // 0x10002820
{
    PluginType* factory = nullptr;
    PluginAccess::LookupPluginType(&factory, Compress::uid, name);
    Compress* c = nullptr;
    if (factory) {
        c = static_cast<Compress*>(PluginAccess::Create(factory));
        if (c) c = ReleaseIfInitFails(c, c->Init());
    }
    if (factory) factory->ReleaseReference();
    return c;
}

Compress* __cdecl Compress::CreateInstance()  // 0x100028c0
{
    Compress* c = new LZSSCompress;
    return ReleaseIfInitFails(c, c->Init());
}

Encrypt* __cdecl Encrypt::CreateInstance(const UId& id)  // 0x10004a20
{
    PluginType* factory = nullptr;
    PluginAccess::LookupPluginType(&factory, id);
    Encrypt* e = factory ? static_cast<Encrypt*>(PluginAccess::Create(factory)) : nullptr;
    if (factory) factory->ReleaseReference();
    return e;
}

Encrypt* __cdecl Encrypt::CreateInstance(const PString& name)  // 0x10004aa0
{
    PluginType* factory = nullptr;
    PluginAccess::LookupPluginType(&factory, Encrypt::uid, name);
    Encrypt* e = factory ? static_cast<Encrypt*>(PluginAccess::Create(factory)) : nullptr;
    if (factory) factory->ReleaseReference();
    return e;
}

Encrypt* __cdecl Encrypt::CreateInstance() { return new BFEncrypt; }  // 0x10004b20

// EditItemManager::CreateInstance: a manager plugin by name, initialised with that name (EditItem.cpp has the rest).
EditItemManager* __cdecl EditItemManager::CreateInstance(const PString& name)  // 0x10003fa0
{
    PluginType* factory = nullptr;
    PluginAccess::LookupPluginType(&factory, EditItemManager::uid, name);
    EditItemManager* m = nullptr;
    if (factory) {
        m = static_cast<EditItemManager*>(PluginAccess::Create(factory));
        if (m && !m->Initialize(name)) {
            static_cast<const ReferenceCount*>(m)->ReleaseReference();
            m = nullptr;
        }
    }
    if (factory) factory->ReleaseReference();
    return m;
}

}  // namespace Jet
