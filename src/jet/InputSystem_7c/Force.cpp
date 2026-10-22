// InputSystem_7c unit U8: force feedback (ForceEffect 0x1000f370..0x1000fa5c, ForceResource 0x10005450..0x10006e55),
// the null driver (0x100108c0, 0x10010b60..0x10010d03, 0x100129e0..0x10012c28) and the plugin table
// JetGetPluginsInputSystem (0x10010aa0).
#include <jet/InputSystem.h>

#include <cstdio>
#include <cstring>
#include <new>

namespace Jet {

namespace {

// DirectInput's effect GUIDs (GUID_ConstantForce .. GUID_Friction) and the .ffe file's
const UId effectRamp("13541C21-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectConstant("13541C20-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectSquare("13541C22-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectSine("13541C23-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectTriangle("13541C24-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectSawtoothUp("13541C25-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectSawtoothDown("13541C26-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectSpring("13541C27-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectDamper("13541C28-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectInertia("13541C29-8E33-11D0-9AD0-00A0C9A06E35");
const UId effectFriction("13541C2A-8E33-11D0-9AD0-00A0C9A06E35");
const UId ffeFile("197E775C-34BA-11D3-ABD5-00C04F8EC627");

struct TypeGuid {
    unsigned long type;
    const UId* guid;
};
// in the order the original compares them
const TypeGuid typeGuids[] = {
    {ForceInfo::CONSTANT, &effectConstant}, {ForceInfo::RAMP, &effectRamp},
    {ForceInfo::SQUARE, &effectSquare},     {ForceInfo::SINE, &effectSine},
    {ForceInfo::TRIANGLE, &effectTriangle}, {ForceInfo::SAWTOOTH_UP, &effectSawtoothUp},
    {ForceInfo::SAWTOOTH_DOWN, &effectSawtoothDown}, {ForceInfo::SPRING, &effectSpring},
    {ForceInfo::DAMPER, &effectDamper},     {ForceInfo::INERTIA, &effectInertia},
    {ForceInfo::FRICTION, &effectFriction},
};

bool SameId(const void* a, const UId& b) { return std::memcmp(a, &b, sizeof(UId)) == 0; }
bool IsTag(const unsigned char* t, const char* tag) { return std::memcmp(t, tag, 4) == 0; }

// the size of a type's parameter block; 0 for the types never written (custom force, unknown)
unsigned long ParamSize(unsigned long type)
{
    switch (type) {
    case ForceInfo::CONSTANT: return 4;
    case ForceInfo::RAMP: return 8;
    case ForceInfo::SQUARE: case ForceInfo::SINE: case ForceInfo::TRIANGLE: case ForceInfo::SAWTOOTH_UP:
    case ForceInfo::SAWTOOTH_DOWN: return 0x10;
    case ForceInfo::SPRING: case ForceInfo::DAMPER: case ForceInfo::INERTIA: case ForceInfo::FRICTION: return 0x30;
    default: return 0;
    }
}

// a database's base directory (its PString at +0xc; x64 offset provisional)
const PString& BasePath(const ResourceDatabase* db)
{
    return *reinterpret_cast<const PString*>(reinterpret_cast<const char*>(db) + 0xc);
}

void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

}  // namespace

// ---- ForceEffect
ForceEffect::ForceEffect(InputDevice* d) : device(d), valid(true)  // 0x1000f370
{
    if (device) device->AddReference();
}

ForceEffect::~ForceEffect()  // 0x1000f420
{
    for (Children::Iterator it = effects.First(); it.node;) {
        ForceEffect* const e = *it;
        it = effects.Remove(it);
        e->ReferenceCount::ReleaseReference();
    }
    if (device) {
        device->ReleaseReference();
        device = nullptr;
    }
    effects.RemoveAll();
}

const ForceEffect::EffectList& ForceEffect::EnumerateAttachedEffects()  // 0x10001910
{
    return reinterpret_cast<const EffectList&>(effects);
}

ForceInfo& ForceEffect::GetInfo() { return info; }  // 0x10001920
bool ForceEffect::IsLoaded() { return false; }      // 0x10001a30
bool ForceEffect::Create(const ForceInfo&, unsigned long) { return false; }  // 0x10001a50

// valid while it and all its children are (the first invalid one ends the walk)
bool ForceEffect::IsValid() const  // 0x1000f520
{
    bool r = valid;
    for (Children::ConstIterator it = effects.First(); r && it.node; ++it) r = (*it)->IsValid();
    return r;
}

void ForceEffect::Invalidate()  // 0x1000f590
{
    valid = false;
    for (Children::Iterator it = effects.First(); it.node; ++it) (*it)->Invalidate();
    CriticalShutdown();
}

void ForceEffect::CriticalShutdown()  // 0x1000f5f0
{
    if (device) {
        device->ReleaseReference();
        device = nullptr;
    }
}

bool ForceEffect::IsPlaying()  // 0x1000f610: any child
{
    bool r = false;
    for (Children::Iterator it = effects.First(); it.node; ++it)
        if (!r) r = (*it)->IsPlaying();
    return r;
}

// the rest: every child until one fails (no children: true)
bool ForceEffect::Start(unsigned long iterations, bool solo)  // 0x1000f670
{
    bool r = true;
    for (Children::Iterator it = effects.First(); it.node; ++it) r = r && (*it)->Start(iterations, solo);
    return r;
}

bool ForceEffect::Stop()  // 0x1000f6e0
{
    bool r = true;
    for (Children::Iterator it = effects.First(); it.node; ++it) r = r && (*it)->Stop();
    return r;
}

bool ForceEffect::Update()  // 0x1000f740
{
    bool r = true;
    for (Children::Iterator it = effects.First(); it.node; ++it) r = r && (*it)->Update();
    return r;
}

bool ForceEffect::Load()  // 0x1000f7a0
{
    bool r = true;
    for (Children::Iterator it = effects.First(); it.node; ++it) r = r && (*it)->Load();
    return r;
}

bool ForceEffect::Unload()  // 0x1000f800
{
    bool r = true;
    for (Children::Iterator it = effects.First(); it.node; ++it) r = r && (*it)->Unload();
    return r;
}

// takes a reference to `e` even when it is attached already (kept)
bool ForceEffect::Attach(ForceEffect* e)  // 0x1000f860
{
    if (!device || !e) return false;
    e->AddReference();
    if (!effects.Find(e).node) effects.Add(e);
    return true;
}

bool ForceEffect::Detach(ForceEffect* e)  // 0x1000f8c0
{
    if (!e) return false;
    Children::Iterator it = effects.Find(e);
    if (!it.node) return false;
    effects.Remove(it);
    e->ReferenceCount::ReleaseReference();
    return true;
}

// from a .ffe resource: the first effect the derived class creates is this one, the others are created by the
// device and attached
bool ForceEffect::Create(const PString& name, const ResourceDatabase* db, unsigned long flags)  // 0x1000f920
{
    bool ok = false;
    if (!db) return false;
    const Resource* const res = db->Get(name, 0);
    if (!res) return false;
    if (res->Lock().code == 0) {
        const Array<ForceInfo, 1>& list = static_cast<const ForceResource*>(res)->GetEffectList();
        bool first = true;
        for (unsigned long i = 0; i < list.count; ++i) {
            const ForceInfo& inf = list.data[i];
            if (first) {
                if (Create(inf, flags)) {
                    ok = true;
                    first = false;
                }
            } else if (ForceEffect* const e = device->CreateEffect(inf, flags)) {
                if (Attach(e)) ok = true;
                e->ReferenceCount::ReleaseReference();
            }
        }
        res->Unlock();
    }
    res->ReleaseReference();
    return ok;
}

// ---- ForceResource
ForceResource::ForceResource(const PString& name, const ResourceDatabase* owner) : Resource(name, owner) {}  // 0x100055c0

ForceResource::ForceResource(const ForceResource& o) : Resource(o)  // 0x100054c0
{
    Assign(o.effects);
}

ForceResource::~ForceResource() {}  // 0x10005450 (the array frees its block)

ForceResource& ForceResource::operator=(const ForceResource& o)  // 0x10005530
{
    Resource::operator=(o);
    Assign(o.effects);
    return *this;
}

const Array<ForceInfo, 1>& ForceResource::GetEffectList() const { return effects; }  // 0x100055f0

// 0x10006d70: the block is replaced only when the other's capacity is larger
void ForceResource::Assign(const Array<ForceInfo, 1>& o)
{
    effects.count = 0;
    if (!o.data) return;
    if (o.capacity > effects.capacity) {
        if (effects.data) Alloc::Delete(effects.data);
        effects.data = static_cast<ForceInfo*>(Alloc::New(static_cast<unsigned int>(o.capacity * sizeof(ForceInfo))));
        effects.capacity = o.capacity;
    }
    effects.count = o.count;
    for (unsigned long i = 0; i < effects.count; ++i) std::memcpy(&effects.data[i], &o.data[i], sizeof(ForceInfo));
}

void ForceResource::Append(const ForceInfo& info)  // 0x10006df0
{
    const unsigned long n = effects.count + 1;
    if (n > effects.capacity) {
        auto* block = static_cast<ForceInfo*>(Alloc::New(static_cast<unsigned int>(n * sizeof(ForceInfo))));
        if (effects.data) {
            std::memcpy(block, effects.data, effects.count * sizeof(ForceInfo));
            if (effects.data) Alloc::Delete(effects.data);
        }
        effects.data = block;
        effects.capacity = n;
    }
    std::memcpy(&effects.data[effects.count], &info, sizeof(ForceInfo));
    ++effects.count;
}

// the effects of the file are added to the list (which is not emptied first); one ForceInfo is reused for all, so
// fields an effect does not set keep the previous effect's values
ForceResource::Result ForceResource::Load(const char* name)  // 0x100058c0
{
    File file;
    bool failed;
    {
        PString n(name);
        PString path = Owner()->GetFileName(n);
        failed = file.Open(path.Text(), File::READ, 0).code != 0;
    }
    if (failed) return Result(2);
    ForceInfo info;
    unsigned char tag[4];
    unsigned long guid[4];
    if (file.Read(tag, 4, 1) && IsTag(tag, "RIFF") && file.Read(tag, 4, 1) && file.Read(tag, 4, 1) &&
        IsTag(tag, "FORC") && file.Read(guid, 0x10, 1) && SameId(guid, ffeFile)) {
        for (int r = LoadEffect(file, info); r != 0; r = LoadEffect(file, info))
            if (r > 0) Append(info);
    }
    file.Close();
    return Result(effects.count ? 0 : 7);
}

// one LIST. A list this code does not take (no object offsets, an unknown coordinate system, trigger button or
// type) is skipped: the original seeks to the list's start plus its size, 8 bytes short of its end (kept).
int ForceResource::LoadEffect(File& f, ForceInfo& info) const  // 0x10005c00
{
    const long start = f.Position();
    unsigned long v;
    unsigned char tag[4];
    if (!f.Read(tag, 4, 1) || !IsTag(tag, "LIST")) return 0;
    if (!f.Read(&v, 4, 1)) return 0;
    const long listEnd = static_cast<long>(v) + start;
    unsigned char nameBuffer[0x44];
    if (!f.Read(nameBuffer, 0x44, 1)) return 0;
    if (!f.Read(&v, 4, 1)) return 0;  // the DIEFFECT size
    if (!f.Read(&v, 4, 1)) return 0;  // its flags
    if (v & 0x20) {
        info.cartesian = 0;
    } else if (v & 0x10) {
        info.cartesian = 1;
    } else {
        f.SeekSet(listEnd);
        return -1;
    }
    if (!(v & 2)) {
        f.SeekSet(listEnd);
        return -1;
    }
    if (!f.Read(&v, 4, 1)) return 0;
    info.duration = static_cast<long>(v);
    if (!f.Read(&v, 4, 1)) return 0;
    info.samplePeriod = v;
    if (!f.Read(&v, 4, 1)) return 0;
    info.gain = v;
    if (!f.Read(&v, 4, 1)) return 0;
    info.triggerButton = v == 0xffffffff ? -1 : static_cast<long>(v - 0x30);
    if (info.triggerButton < -1 || info.triggerButton >= 0x20) {
        f.SeekSet(listEnd);
        return -1;
    }
    if (!f.Read(&v, 4, 1)) return 0;
    info.triggerRepeat = v;
    if (!f.Read(&v, 4, 1)) return 0;
    const unsigned long axes = v;
    if (!f.Read(&v, 4, 1)) return 0;
    const bool axisBlock = (v & 0x20) != 0;
    if (!f.Read(&v, 4, 1)) return 0;
    const bool directionBlock = (v & 0x40) != 0;
    if (!f.Read(&v, 4, 1)) return 0;
    const bool envelopeBlock = (v & 0x80) != 0;
    if (!f.Read(&v, 4, 1)) return 0;  // the parameter size
    if (!f.Read(&v, 4, 1)) return 0;
    const bool paramBlock = (v & 0x100) != 0;
    if (!f.Read(&v, 4, 1)) return 0;
    info.startDelay = v;
    unsigned long guid[4];
    if (!f.Read(guid, 0x10, 1)) return 0;
    bool known = false;
    for (const TypeGuid& t : typeGuids) {
        if (SameId(guid, *t.guid)) {
            info.type = t.type;
            known = true;
            break;
        }
    }
    if (!known) {
        f.SeekSet(listEnd);
        return -1;
    }
    if (!f.Read(&v, 4, 1)) return 0;
    if (axisBlock) {  // the axes: skipped
        if (!f.Read(tag, 4, 1) || !IsTag(tag, "data") || !f.Read(&v, 4, 1)) return 0;
        if (f.SeekCur(static_cast<long>(v)) == 0xffffffff) return 0;
    }
    if (directionBlock) {
        if (!f.Read(tag, 4, 1) || !IsTag(tag, "data") || !f.Read(&v, 4, 1)) return 0;
        const unsigned long n = v >> 2;
        info.direction[1] = 0;
        info.direction[0] = 0;
        for (unsigned long i = 0; i < n; ++i) {
            if (!f.Read(&v, 4, 1)) return 0;
            if (i < 2) info.direction[i] = static_cast<long>(v);
        }
    }
    if (envelopeBlock) {
        if (!f.Read(tag, 4, 1) || !IsTag(tag, "data") || !f.Read(&v, 4, 1) || !f.Read(&v, 4, 1)) return 0;
        if (!f.Read(&v, 4, 1)) return 0;
        info.attackLevel = v;
        if (!f.Read(&v, 4, 1)) return 0;
        info.attackTime = v;
        if (!f.Read(&v, 4, 1)) return 0;
        info.fadeLevel = v;
        if (!f.Read(&v, 4, 1)) return 0;
        info.fadeTime = v;
        info.hasEnvelope = true;
    } else {
        info.hasEnvelope = false;
        info.fadeLevel = 10000;
        info.attackLevel = 10000;
        info.fadeTime = 0;
        info.attackTime = 0;
    }
    if (!paramBlock) return 1;
    if (!f.Read(tag, 4, 1) || !IsTag(tag, "data") || !f.Read(&v, 4, 1)) return 0;
    int words = 0;
    switch (info.type) {
    case ForceInfo::CONSTANT: words = 1; break;
    case ForceInfo::RAMP: words = 2; break;
    case ForceInfo::SQUARE: case ForceInfo::SINE: case ForceInfo::TRIANGLE: case ForceInfo::SAWTOOTH_UP:
    case ForceInfo::SAWTOOTH_DOWN: words = 4; break;
    case ForceInfo::SPRING: case ForceInfo::DAMPER: case ForceInfo::INERTIA: case ForceInfo::FRICTION:
        // one condition of six values per axis; only two are kept
        for (unsigned long a = 0; a < axes; ++a) {
            long* const dst = a < 2 ? &info.params[6 * a] : nullptr;
            for (int k = 0; k < 6; ++k) {
                if (!f.Read(&v, 4, 1)) return 0;
                if (dst) dst[k] = static_cast<long>(v);
            }
        }
        return 1;
    default: return 1;
    }
    for (int k = 0; k < words; ++k) {
        if (!f.Read(&v, 4, 1)) return 0;
        info.params[k] = static_cast<long>(v);
    }
    return 1;
}

// the effect and its children as a .ffe file; a failed file is removed. (The RIFF size is left 0.)
bool ForceResource::Save(const ForceEffect& effect, const char* name, const ResourceDatabase* db)  // 0x10005600
{
    File file;
    if (!name) return false;
    bool failed;
    if (db) {
        PString n(name);
        PString path = db->GetFileName(n);
        failed = file.Open(path.Text(), File::CREATE_WRITE, 0).code != 0;
    } else {
        failed = file.Open(name, File::CREATE_WRITE, 0).code != 0;
    }
    if (failed) return false;
    bool ok = false;
    ForceEffect& e = const_cast<ForceEffect&>(effect);
    if (file.Write("RIFF", 4, 1) && file.Write(0ul) && file.Write("FORC", 4, 1) && file.Write(&ffeFile, 0x10, 1) &&
        SaveEffect(file, e.GetInfo(), 1)) {
        ok = true;
        unsigned long index = 2;
        const ForceEffect::EffectList& children = e.EnumerateAttachedEffects();
        for (ForceEffect::EffectList::ConstIterator it = children.First(); it.node; ++it) {
            if (!SaveEffect(file, (*it)->GetInfo(), index++)) {
                ok = false;
                break;
            }
        }
    }
    file.Close();
    if (!ok) {
        if (db)
            File::Remove(BasePath(db), name);
        else
            File::Remove(name);
    }
    return ok;
}

bool ForceResource::SaveEffect(File& f, const ForceInfo& info, unsigned long index)  // 0x10006640
{
    const unsigned long size = ParamSize(info.type);
    if (!size || info.type > ForceInfo::FRICTION) return false;
    if (!f.Write("LIST", 4, 1)) return false;
    const long sizeAt = f.Position();
    if (!f.Write(0ul)) return false;
    char buffer[0x80];
    std::memset(buffer, 0, sizeof(buffer));
    std::sprintf(buffer, "efctHere goes effect #%d name for DirectX (tm) ForceEditor.", static_cast<int>(index));
    buffer[0x43] = 0;  // the name field is 0x44 bytes ("efctHere goes effect #1 name for DirectX (tm) Fo")
    if (!f.Write(buffer, 0x44, 1)) return false;
    const unsigned long guidType = info.type;
    const UId* guid = nullptr;
    for (const TypeGuid& t : typeGuids)
        if (t.type == guidType) guid = t.guid;
    if (!f.Write(0x38ul) || !f.Write(info.cartesian ? 0x12ul : 0x22ul) ||
        !f.Write(static_cast<unsigned long>(info.duration)) || !f.Write(info.samplePeriod) || !f.Write(info.gain) ||
        !f.Write(info.triggerButton == -1 ? 0xfffffffful : static_cast<unsigned long>(info.triggerButton + 0x30)) ||
        !f.Write(info.triggerRepeat) || !f.Write(2ul) || !f.Write(0x20ul) || !f.Write(0x40ul) ||
        !f.Write(info.hasEnvelope ? 0x80ul : 0ul) || !f.Write(size) || !f.Write(0x100ul) || !f.Write(info.startDelay) ||
        !f.Write(guid, 0x10, 1) || !f.Write(1ul))
        return false;
    if (!f.Write("data", 4, 1) || !f.Write(8ul) || !f.Write(0ul) || !f.Write(4ul)) return false;  // the axes X, Y
    if (!f.Write("data", 4, 1) || !f.Write(8ul) || !f.Write(static_cast<unsigned long>(info.direction[0])) ||
        !f.Write(static_cast<unsigned long>(info.direction[1])))
        return false;
    if (info.hasEnvelope) {
        if (!f.Write("data", 4, 1) || !f.Write(0x14ul) || !f.Write(0x14ul) || !f.Write(info.attackLevel) ||
            !f.Write(info.attackTime) || !f.Write(info.fadeLevel) || !f.Write(info.fadeTime))
            return false;
    }
    if (!f.Write("data", 4, 1) || !f.Write(size)) return false;
    for (unsigned long k = 0; k < size / 4; ++k)
        if (!f.Write(static_cast<unsigned long>(info.params[k]))) return false;
    const long end = f.Position();
    f.SeekSet(sizeAt);
    if (!f.Write(static_cast<unsigned long>(end - sizeAt - 4))) return false;
    f.SeekSet(end);
    return true;
}

// ---- the null driver: one stub device, always there (in Jet itself: the RTTI names must match the original's)
namespace {

// 0x10012a30
void NullCaps(InputDeviceCaps& c)
{
    c.type = 8;
    c.id = UIdNullDev;
    SetText(c.name, "Null");
    SetText(c.description, "System stub");
    c.numButtons = c.buttonMask = c.numAxes = c.axisMask = c.numPovs = c.unknown34 = 0;
    c.caps = 0xFF000001;
}

}  // namespace

class NullInputDevice : public InputDevice {
public:
    NullInputDevice() : InputDevice(nullptr)  // (inlined in 0x10012b60)
    {
        valid = false;
        instanceId = UIdNullDev;
    }
    ~NullInputDevice() override {}
    void __cdecl GetCaps(InputDeviceCaps& c) const override { NullCaps(c); }  // 0x100129f0
};

class NullInputDriver : public InputDriver {
public:
    bool __cdecl Init(OS*) override { return true; }  // 0x100108c0
    void __cdecl Enumerate(InputDeviceCapsList& list, int) override  // 0x10012af0 (any type)
    {
        InputDeviceCaps c;
        NullCaps(c);
        list.Add(c);
    }
    AutoReference<InputDevice> __cdecl CreateDevice(const UId&) override  // 0x10012b60
    {
        AutoReference<InputDevice> r;
        r = new NullInputDevice;
        return r;
    }
};

extern "C" void __cdecl JetGetPluginsInputSystem()  // 0x10010aa0
{
    const UId resources("4C414E80-BBDA-11D4-9AD7-0001025364B8");
    new ResourceType<ForceResource>(PString("ffe"), UId("4915B7B0-2C79-40d6-9EBD-D52DA27A3518"), resources);
    new PluginFactoryOf<NullInputDriver, InputDriver>(PString("None"), UId("60F6F2D8-C67C-4522-ACBF-BDE86DACB36F"),
                                                     UId("F3117B18-F364-41a1-8993-966236A35B5E"));
}

}  // namespace Jet
