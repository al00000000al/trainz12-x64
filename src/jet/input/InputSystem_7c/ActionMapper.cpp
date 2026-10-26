// InputSystem_7c unit U7: InputAction (0x10008350..0x10008c5c) and InputActionMapper (0x10008d20..0x1000c8f8),
// transcribed from the assembly. An action is a named list of strokes; the mapper keeps the actions by name (a
// 97-bucket hash table), gives each device it is given copies of the actions the device can issue, and on every
// event of a device advances those copies, telling its observers when one completes. Action maps are read with the
// module's scanner (Lexer.cpp) from "actionmap" sections and written back by Save.
#include <jet/Config.h>
#include <jet/InputSystem.h>

#include "Lexer.h"

#include <cmath>
#include <cstring>
#include <new>

namespace Jet {

// the original's table is only the text "UNKNOWN" (0x100162ec), like Encrypt's in Kernel_7c
struct ResultTable {
    char text[8];
};
static const ResultTable kMapperResults = {"UNKNOWN"};
const ResultTable* const InputActionMapper::results = &kMapperResults;

namespace {

// float to int the way the original's fld / fistp does it: round to nearest even
int RoundToInt(float f) { return static_cast<int>(std::nearbyint(f)); }

unsigned long Ms(float time) { return static_cast<unsigned long>(RoundToInt(time * 1000.0f)); }

// 0x10008b60: appends `n` strokes, the block first growing to an even number of items
void Append(Array<StrokeDef, 2>& a, const StrokeDef* items, unsigned long n)
{
    a.Reserve(a.count + n);
    for (; n; --n) a.data[a.count++] = *items++;
}

// 0x10008bf0, EndAdd's: takes `n` (the count) off the capacity and moves the strokes that still fit to a new block.
// So a full array loses its block's worth, and an empty one with room gets count = capacity (kept).
void Shrink(Array<StrokeDef, 2>& a, unsigned long n)
{
    if (n >= a.capacity) {
        a.count = 0;
        return;
    }
    const unsigned long capacity = a.capacity - n;
    unsigned long last = a.count - 1;
    a.capacity = capacity;
    if (last >= capacity) last = capacity - 1;
    a.count = last + 1;
    auto* block = static_cast<StrokeDef*>(Alloc::New(static_cast<unsigned int>(capacity * sizeof(StrokeDef))));
    std::memcpy(block, a.data, a.count * sizeof(StrokeDef));
    if (a.data) Alloc::Delete(a.data);
    a.data = block;
}

// what the original does for `pstring = "text"`: drop the old node, intern the new text
void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

InputAction* FromLink(IListLink* l) { return reinterpret_cast<InputAction*>(reinterpret_cast<char*>(l) - sizeof(void*)); }

void LinkTail(IList<InputAction>& list, InputAction* a, IListLink InputAction::*link)
{
    IListLink& node = a->*link;
    node.next = &list.head;
    node.prev = list.head.prev;
    list.head.prev->next = &node;
    list.head.prev = &node;
    ++list.count;
}

void Unlink(IList<InputAction>& list, IListLink* node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
    --list.count;
}

// the index of a name among `count` names of the table (one node per text), or -1
int FindName(const PString* names, int count, const char* text)
{
    PString name(text);
    for (int i = 0; i < count; ++i)
        if (names[i] == name) return i;
    return -1;
}

}  // namespace

// ---- InputAction
InputAction::InputAction() : name("") , flush(false)  // 0x10008350
{
    link.next = link.prev = nullptr;
}

InputAction::InputAction(const InputAction& o) : flush(false)  // 0x10008390
{
    link.next = link.prev = nullptr;
    *this = o;
}

InputAction::~InputAction() {}  // 0x10008400: the strokes' block and the name go

// 0x10008490: the strokes are appended to the ones already there
InputAction& InputAction::operator=(const InputAction& o)
{
    if (this == &o) return *this;
    name = o.name;
    flush = o.flush;
    if (o.strokes.count) Append(strokes, o.strokes.data, o.strokes.count);
    return *this;
}

// 0x10008510: name, flush and this action's strokes (as many as it has) compared bytewise
bool InputAction::operator==(const InputAction& o) const
{
    if (name != o.name || flush != o.flush) return false;
    const auto* a = reinterpret_cast<const unsigned char*>(strokes.data);
    const auto* b = reinterpret_cast<const unsigned char*>(o.strokes.data);
    for (unsigned long i = 0; i < strokes.count * sizeof(StrokeDef); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

PString InputAction::GetName() const { return name; }      // 0x100085d0
void InputAction::SetName(const char* text) { SetText(name, text); }  // 0x10008620

void InputAction::Reset()  // 0x10008660 (the block is kept)
{
    flush = false;
    name = PString("");
    strokes.count = 0;
}

unsigned int InputAction::Count() { return strokes.count; }   // 0x10008740
void InputAction::BeginAdd() {}                                // 0x10002300 (shared empty body)
void InputAction::EndAdd() { Shrink(strokes, strokes.count); }  // 0x10008750
void InputAction::SetFlush(bool on) { flush = on; }            // 0x10008770

void InputAction::AddKey(KeyCode key, bool shift, bool ctrl, bool alt, bool pressed, float time)  // 0x10008780
{
    const StrokeDef d = {(static_cast<unsigned long>(key) & 0xff) << 16 | (pressed ? 0x80u : 0) | (shift ? 4u : 0) |
                             (ctrl ? 2u : 0) | (alt ? 1u : 0),
                         Ms(time)};
    Append(strokes, &d, 1);
}

void InputAction::AddButton(ButtonCode button, bool pressed, float time)  // 0x10008800
{
    const StrokeDef d = {(static_cast<unsigned long>(button) & 0xff | 0x100) << 16 | (pressed ? 0x80u : 0), Ms(time)};
    Append(strokes, &d, 1);
}

void InputAction::AddAxis(AxisCode axis, AxisModification mod, AxisDir dir, float time)  // 0x10008860
{
    const StrokeDef d = {(static_cast<unsigned long>(axis) & 0xff | 0x200) << 16 | (static_cast<unsigned long>(dir) & 3) << 10 |
                             (static_cast<unsigned long>(mod) & 3) << 8,
                         Ms(time)};
    Append(strokes, &d, 1);
}

void InputAction::AddPov(PovCode pov, PovDir dir, float time)  // 0x100088d0
{
    const StrokeDef d = {(static_cast<unsigned long>(pov) & 0xff | 0x300) << 16 | (static_cast<unsigned long>(dir) & 0xf) << 12,
                         Ms(time)};
    Append(strokes, &d, 1);
}

void InputAction::Add(const InputExtensionState& state, float time)  // 0x10008930
{
    const StrokeDef d = {state.code, Ms(time)};
    Append(strokes, &d, 1);
}

Array<StrokeDef, 2>* InputAction::GetStrokeList() const  // 0x10008980
{
    auto* list = static_cast<Array<StrokeDef, 2>*>(Alloc::New(sizeof(Array<StrokeDef, 2>)));
    if (list) new (list) Array<StrokeDef, 2>;
    if (strokes.count) Append(*list, strokes.data, strokes.count);
    return list;
}

void InputAction::SetStrokeList(const Array<StrokeDef, 2>& list)  // 0x100089c0
{
    strokes.count = 0;
    if (list.count) Append(strokes, list.data, list.count);
}

bool InputAction::TestDevice(const InputDevice* device) const  // 0x100089f0
{
    if (!device) return false;
    InputDeviceCaps caps;
    device->GetCaps(caps);
    return TestDevice(caps);
}

// 0x10008a90: a keyboard takes key strokes only; other devices need each stroke's button / axis / pov (no keys)
bool InputAction::TestDevice(const InputDeviceCaps& caps) const
{
    if (caps.type == DEVICE_KEYBOARD) {
        for (unsigned long i = 0; i < strokes.count; ++i)
            if (strokes.data[i].code >> 24) return false;
        return true;
    }
    for (unsigned long i = 0; i < strokes.count; ++i) {
        const unsigned long code = strokes.data[i].code;
        const unsigned long bit = 1u << ((code >> 16) & 31);
        switch (code >> 24) {
        case 1:
            if (!(caps.buttonMask & bit)) return false;
            break;
        case 2:
            if (!(caps.axisMask & bit)) return false;
            break;
        case 3:
            if (!(caps.unknown34 & bit)) return false;
            break;
        default: return false;
        }
    }
    return true;
}

// ---- the action table
InputActionMapper::ActionTable::ActionTable() : count(0), bucketCount(0x61)  // 0x1000c1a0
{
    auto* block = static_cast<unsigned long*>(Alloc::New(sizeof(unsigned long) + 0x61 * sizeof(Bucket)));
    if (!block) {
        buckets = nullptr;
        return;
    }
    *block = 0x61;  // new[]'s count
    buckets = reinterpret_cast<Bucket*>(block + 1);
    for (unsigned long i = 0; i < 0x61; ++i) new (&buckets[i]) Bucket;
}

InputActionMapper::ActionTable::~ActionTable()  // (in 0x10008d20) the lists themselves are not freed here
{
    if (!buckets) return;
    unsigned long* const block = reinterpret_cast<unsigned long*>(buckets) - 1;
    for (unsigned long i = *block; i; --i) buckets[i - 1].~Bucket();
    Alloc::Delete(block);
}

// 0x10008e90 + 0x1000c7b0: the last 16 characters, h = h * 8 ^ (signed char)c
unsigned long InputActionMapper::ActionTable::Index(PString name) const
{
    unsigned long h = 0;
    if (name.node) {
        const unsigned long length = name.node->length;
        unsigned long n = length > 16 ? 16 : length;
        for (const char* p = name.node->text + (length - n); n; --n, ++p) h = h * 8 ^ static_cast<unsigned long>(static_cast<long>(*p));
    }
    return h % bucketCount;
}

InputActionMapper::ActionTable::Iterator InputActionMapper::ActionTable::Find(PString name)
{
    Iterator i(*this);
    const unsigned long index = Index(name);
    Bucket::Chunk* const head = buckets[index].head;
    Bucket::Chunk* chunk = head;
    ActionList** node = head ? &head->data[0] : nullptr;
    while (node) {
        if ((*node)->name.node == name.node) {
            i.bucket = index;
            i.node = node;
            i.head = head;
            i.chunk = chunk;
            return i;
        }
        if (++node >= &chunk->data[chunk->count]) {
            chunk = chunk->next;
            if (chunk == head) break;
            node = &chunk->data[0];
        }
    }
    return i;
}

void InputActionMapper::ActionTable::Insert(ActionList* list)
{
    const unsigned long index = Index(list->name);
    ++count;
    buckets[index].Add(list);
}

void InputActionMapper::ActionTable::Clear()
{
    for (unsigned long i = 0; i < bucketCount; ++i) buckets[i].RemoveAll();
    count = 0;
}

void InputActionMapper::ActionTable::Iterator::Begin()  // 0x1000c4e0
{
    bucket = 0;
    head = chunk = table->buckets[0].head;
    node = head ? &head->data[0] : nullptr;
    while (!node && bucket < table->bucketCount - 1) {
        ++bucket;
        head = chunk = table->buckets[bucket].head;
        node = head ? &head->data[0] : nullptr;
    }
}

void InputActionMapper::ActionTable::Iterator::Next()  // 0x1000c000
{
    if (node && ++node >= &chunk->data[chunk->count]) {
        chunk = chunk->next;
        node = chunk == head ? nullptr : &chunk->data[0];
    }
    while (!node) {
        if (++bucket >= table->bucketCount) return;
        head = chunk = table->buckets[bucket].head;
        node = head ? &head->data[0] : nullptr;
    }
}

InputActionMapper::ActionList::~ActionList()  // 0x1000bd20: from the back
{
    while (list.count) {
        IListLink* const last = list.head.prev;
        Unlink(list, last);
        delete FromLink(last);
    }
}

// ---- the devices
InputActionMapper::Device::Device(InputDevice* d) : next(nullptr), prev(nullptr), device(d)  // 0x1000bdd0
{
    if (device) device->AddReference();
    UpdateZones();
}

InputActionMapper::Device::~Device()  // 0x1000be50: the actions from the back
{
    if (device) device->ReferenceCount::ReleaseReference();
    device = nullptr;
    while (actions.count) {
        IListLink* const last = actions.head.prev;
        Unlink(actions, last);
        delete FromLink(last);
    }
}

// 0x1000bf10: per axis, the range (property 3) and the threshold (property 4): {centre - threshold, centre +
// threshold} with centre = (max - min) / 2; left 0 when either is not answered
void InputActionMapper::Device::UpdateZones()
{
    std::memset(zones, 0, sizeof(zones));
    for (unsigned char i = 0; i < 8; ++i) {
        DeviceProperty p = {};
        p.id = PROPERTY_RANGE;
        p.index = i;
        if (!device->GetProperty(p)) continue;
        const int range = p.b - p.a;
        p.id = PROPERTY_THRESHOLD;
        p.index = i;
        if (!device->GetProperty(p)) continue;
        const int half = range >> 1;
        zones[i].lo = half - p.a;
        zones[i].hi = half + p.a;
    }
}

// ---- InputActionMapper
InputActionMapper::InputActionMapper()  // 0x10008f30
{
    devices.next = devices.prev = devices.Head();
    devices.count = 0;
}

InputActionMapper::~InputActionMapper()  // 0x10008d20: the devices from the front, their observers left as they are
{
    while (devices.count) {
        Device* const d = devices.next;
        d->prev->next = d->next;
        d->next->prev = d->prev;
        d->prev = d->next = nullptr;
        --devices.count;
        delete d;
    }
}

void InputActionMapper::Done()  // 0x10008fe0
{
    RemoveAllDevices();
    ActionTable::Iterator i(table);
    for (i.Begin(); i.node; i.Next())
        if (ActionList* const list = *i.node) delete list;
    table.Clear();
}

void InputActionMapper::Activate(bool on)  // 0x10009060 (no base call)
{
    active = on;
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        if (active)
            d->device->AddObserver(static_cast<InputObserver*>(this), 1);
        else
            d->device->RemoveObserver(static_cast<InputObserver*>(this));
    }
}

void InputActionMapper::Reset()  // 0x100090c0
{
    for (Device* d = devices.next; d != devices.Head(); d = d->next)
        for (IListLink* l = d->actions.head.next; l != &d->actions.head; l = l->next)
            static_cast<DeviceAction*>(FromLink(l))->progress = 0;
}

// 0x10009110: 2 no device; 4 none of the actions fits it; 3 some name has no action that fits; else 0
InputActionMapper::Result InputActionMapper::AddDevice(InputDevice* device)
{
    if (!device) return 2;
    Device* const d = new Device(device);
    d->next = devices.Head();
    d->prev = devices.prev;
    devices.prev->next = d;
    ++devices.count;
    devices.prev = d;
    if (active) d->device->AddObserver(static_cast<InputObserver*>(this), 1);
    InputDeviceCaps caps;
    device->GetCaps(caps);
    int result = 0;
    ActionTable::Iterator i(table);
    for (i.Begin(); i.node; i.Next()) {
        IList<InputAction>& list = (*i.node)->list;
        bool any = false;
        for (IListLink* l = list.head.next; l != &list.head; l = l->next) {
            InputAction* const a = FromLink(l);
            if (!a->TestDevice(caps)) continue;
            LinkTail(d->actions, new DeviceAction(*a), &InputAction::link);
            any = true;
        }
        if (!any) result = 3;
    }
    return d->actions.count ? result : 4;
}

// 0x10009380 / 0x10009410: the first device with that instance id; its observer goes whether active or not
void InputActionMapper::RemoveDevice(const UId& id)
{
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        if (std::memcmp(&d->device->instanceId, &id, sizeof(UId))) continue;
        d->prev->next = d->next;
        d->next->prev = d->prev;
        d->prev = d->next = nullptr;
        --devices.count;
        d->device->RemoveObserver(static_cast<InputObserver*>(this));
        delete d;
        return;
    }
}

void InputActionMapper::RemoveDevice(InputDevice* device)
{
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        if (std::memcmp(&d->device->instanceId, &device->instanceId, sizeof(UId))) continue;
        d->prev->next = d->next;
        d->next->prev = d->prev;
        d->prev = d->next = nullptr;
        --devices.count;
        d->device->RemoveObserver(static_cast<InputObserver*>(this));
        delete d;
        return;
    }
}

void InputActionMapper::RemoveAllDevices()  // 0x100094a0: from the back
{
    while (devices.count) {
        Device* const d = devices.prev;
        d->prev->next = d->next;
        d->next->prev = d->prev;
        d->prev = d->next = nullptr;
        --devices.count;
        d->device->RemoveObserver(static_cast<InputObserver*>(this));
        delete d;
    }
}

bool InputActionMapper::TestDevice(InputDevice* device) const  // 0x10009510: every name has an action that fits
{
    InputDeviceCaps caps;
    device->GetCaps(caps);
    ActionTable::Iterator i(const_cast<ActionTable&>(table));
    for (i.Begin(); i.node; i.Next())
        if (!TestDevice(caps, (*i.node)->list)) return false;
    return true;
}

bool InputActionMapper::TestDevice(InputDevice* device, const PString& action) const  // 0x10009600
{
    ActionTable::Iterator i = const_cast<ActionTable&>(table).Find(action);
    if (!i.node) return false;
    InputDeviceCaps caps;
    device->GetCaps(caps);
    return TestDevice(caps, (*i.node)->list);
}

bool InputActionMapper::TestDevice(const InputDeviceCaps& caps, IList<InputAction>& list) const  // 0x1000aad0
{
    for (IListLink* l = list.head.next; l != &list.head; l = l->next)
        if (FromLink(l)->TestDevice(caps)) return true;
    return false;
}

// 0x100096f0: every "actionmap" child of the file (the one named `mapName` when given); 5 when it cannot be read
InputActionMapper::Result InputActionMapper::Load(const char* file, const ResourceDatabase* db, const char* mapName)
{
    int result = 0;
    ConfigParser parser;
    bool failed;
    if (db) {
        PString name(file);
        PString path = db->GetFileName(name);
        failed = parser.Parse(path.Text()).code != 0;
    } else {
        failed = parser.Parse(file).code != 0;
    }
    if (failed || !parser.RootPtr()) return 5;
    const ConfigData& root = parser.Root();
    const int children = root.GetChildCount();
    PString map(mapName);
    PString actionmap("actionmap");
    for (int i = 0; i < children; ++i) {
        const ConfigData& child = root[i];
        if (child.Name() != actionmap) continue;
        if (mapName && child.Value() != map) continue;
        const int r = LoadActionMap(child).code;
        if (result == 0)
            result = r;
        else if (result == 3 && r == 4)
            result = 4;
    }
    return result;
}

// 0x10009940: the actions as an action map ("version 1.0", then actionmap("name") { action("name") { variant { key
// = ..., flush = ...; } } }), named `mapName` or a random UId; 2 no file, 5 not opened
InputActionMapper::Result InputActionMapper::Save(const char* file, const ResourceDatabase* db, const char* mapName)
{
    File out;
    String text;
    if (!file) return 2;
    bool failed;
    if (db) {
        PString name(file);
        PString path = db->GetFileName(name);
        failed = out.Open(path.Text(), File::CREATE_WRITE, 0).code != 0;
    } else {
        failed = out.Open(file, File::CREATE_WRITE, 0).code != 0;
    }
    if (failed) return 5;
    text.Setf("version 1.0\n\n");
    out.WriteLine(text, 0);
    if (mapName) {
        text.Setf("actionmap(\"%s\")\n{\n", mapName);
    } else {
        UId id;
        id.SetRnd();
        const PString s = id.GetString();
        text.Setf("actionmap(\"%s\")\n{\n", s.Text());
    }
    out.WriteLine(text, 0);
    const InputStrings& strings = InputStrings::Get();
    ActionTable::Iterator i(table);
    for (i.Begin(); i.node; i.Next()) {
        const ActionList* const list = *i.node;
        if (!list->list.count) continue;
        text.Setf("  action(\"%s\")\n  {\n", list->name.Text());
        out.WriteLine(text, 0);
        for (IListLink* l = list->list.head.next; l != &list->list.head; l = l->next) {
            const InputAction* const a = FromLink(l);
            text.Setf("    variant\n    {\n");
            out.WriteLine(text, 0);
            for (unsigned long s = 0; s < a->strokes.count; ++s) {
                const StrokeDef& d = a->strokes.data[s];
                const unsigned long code = (d.code >> 16) & 0xff;
                // (an unknown kind writes the previous line again)
                switch (d.code >> 24) {
                case 0: {
                    const char* const pressed = (d.code & 0x80) ? "pressed" : "released";
                    const PString key = strings.KeyToString(static_cast<KeyCode>(code));
                    text.Setf("      key    = %s%s%s%s, %s, %0d.%0d;\n", key.Text(), (d.code & 4) ? "+shift" : "",
                              (d.code & 2) ? "+ctrl" : "", (d.code & 1) ? "+alt" : "", pressed, d.time / 1000,
                              d.time % 1000);
                    break;
                }
                case 1: {
                    const PString button = strings.ButtonToString(static_cast<ButtonCode>(code), false);
                    text.Setf("      button = %s, %s, %0d.%0d;\n", button.Text(), (d.code & 0x80) ? "pressed" : "released",
                              d.time / 1000, d.time % 1000);
                    break;
                }
                case 2: {
                    const unsigned long dir = (d.code >> 10) & 3, mod = (d.code >> 8) & 3;
                    const PString axis = strings.AxisToString(static_cast<AxisCode>(code));
                    text.Setf("      axis   = %s, %s, %s %0d.%0d;\n", axis.Text(),
                              mod == 0 ? "changed" : mod == 1 ? "inc" : "dec",
                              dir == 1 ? "positive, " : dir == 2 ? "negative, " : "", d.time / 1000, d.time % 1000);
                    break;
                }
                case 3: {
                    const PString dir = strings.PovDirToString(static_cast<PovDir>((d.code >> 12) & 0xf));
                    const PString pov = strings.PovToString(static_cast<PovCode>(code));
                    text.Setf("      pov    = %s, %s, %0d.%0d;\n", pov.Text(), dir.Text(), d.time / 1000, d.time % 1000);
                    break;
                }
                }
                out.WriteLine(text, 0);
            }
            text.Setf("      flush  = %s;\n", a->flush ? "true" : "false");
            out.WriteLine(text, 0);
            text.Setf("    }\n\n");
            out.WriteLine(text, 0);
        }
        text.Setf("  }\n\n");
        out.WriteLine(text, 0);
    }
    text.Setf("}\n");
    out.WriteLine(text, 0);
    out.Close();
    return 0;
}

// 0x1000a140: the action goes into the table and to each device it fits; 0 duplicate or every device, 4 none, 3 some
InputActionMapper::Result InputActionMapper::AddAction(const InputAction& action)
{
    if (!AddToActionList(action)) return 0;
    unsigned long fitted = 0;
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        InputDeviceCaps caps;
        d->device->GetCaps(caps);
        if (!action.TestDevice(caps)) continue;
        LinkTail(d->actions, new DeviceAction(action), &InputAction::link);
        ++fitted;
    }
    if (fitted == devices.count) return 0;
    return fitted ? 3 : 4;
}

// 0x1000a2d0: the first equal action of its name, then the first equal copy on each device (only when the name is
// known; an emptied name list stays)
void InputActionMapper::RemoveAction(const InputAction& action)
{
    ActionTable::Iterator i = table.Find(action.GetName());
    if (!i.node) return;
    IList<InputAction>& list = (*i.node)->list;
    for (IListLink* l = list.head.next; l != &list.head; l = l->next) {
        if (!(*FromLink(l) == action)) continue;
        Unlink(list, l);
        delete FromLink(l);
        break;
    }
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        for (IListLink* l = d->actions.head.next; l != &d->actions.head; l = l->next) {
            if (!(*FromLink(l) == action)) continue;
            Unlink(d->actions, l);
            delete FromLink(l);
            break;
        }
    }
}

void InputActionMapper::RemoveAllActions()  // 0x1000a400
{
    ActionTable::Iterator i(table);
    for (i.Begin(); i.node; i.Next())
        if (ActionList* const list = *i.node) delete list;
    table.Clear();
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        while (d->actions.count) {
            IListLink* const last = d->actions.head.prev;
            Unlink(d->actions, last);
            delete FromLink(last);
        }
    }
}

// 0x1000a4c0: each device of that id advances its actions; a completed one is told to the observers (a STOP stops
// the event only when the mapper is exclusive) and, when it flushes, resets the device's other actions
InputEvent::Propagation InputActionMapper::OnInputEvent(const UId& id, const InputEvent& e)
{
    bool stopped = false;
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        if (std::memcmp(&d->device->instanceId, &id, sizeof(UId))) continue;
        bool flushed = false;
        for (IListLink* l = d->actions.head.next; l != &d->actions.head; l = l->next) {
            DeviceAction* const a = static_cast<DeviceAction*>(FromLink(l));
            const int kind = e.type & 0xf0;
            if (kind == InputEvent::EVENT_LOST || kind == InputEvent::EVENT_GET) {
                a->progress = 0;
                continue;
            }
            if (kind == InputEvent::EVENT_PROPERTY && (e.id == PROPERTY_RANGE || e.id == PROPERTY_THRESHOLD))
                d->UpdateZones();
            // too late for the next stroke: start again
            if (a->progress && a->strokes.data[a->progress].time + a->lastTime < e.time) a->progress = 0;
            const unsigned long before = a->progress;
            const unsigned long code = a->strokes.data[a->progress].code;
            const unsigned char target = static_cast<unsigned char>(code >> 16);
            const unsigned long t = static_cast<unsigned long>(e.type);
            switch (code >> 24) {
            case 0:  // a key: pressed (not a repeat) or released, with at least the stroke's modifiers
                if ((t & 0xf) != DEVICE_KEYBOARD || kind != InputEvent::EVENT_KEY || e.code != target) break;
                if (!((code & 0x80) && (t & INPUT_DOWN) && !(t & INPUT_REPEAT))) {
                    if ((code & 0x80) || !(t & INPUT_UP)) break;
                }
                if ((code & 4) && !(t & 0x8000000)) break;
                if ((code & 2) && !(t & 0x10000000)) break;
                if ((code & 1) && !(t & 0x20000000)) break;
                ++a->progress;
                break;
            case 1:  // a button of any device
                if (kind != InputEvent::EVENT_KEY || e.code != target) break;
                if (!((code & 0x80) && (t & INPUT_DOWN))) {
                    if ((code & 0x80) || !(t & INPUT_UP)) break;
                }
                ++a->progress;
                break;
            case 2: {  // an axis: changed / inc / dec (moving away from the centre on the negative side: inc wants a
                       // falling value, dec a rising one), anywhere or past the dead zone
                if (kind != InputEvent::EVENT_AXIS || e.code != target) break;
                const unsigned long dir = (code >> 10) & 3;
                // the zones are read through the event's code, past the eight for other codes (kept)
                const char* const zone = reinterpret_cast<const char*>(d) + 0x18 + 8 * e.code;
                auto below = [&] { return e.value < *reinterpret_cast<const int*>(zone); };
                auto above = [&] { return e.value > *reinterpret_cast<const int*>(zone + 4); };
                switch ((code >> 8) & 3) {
                case 0:
                    if (dir == 0 || (dir == 1 && above()) || (dir == 2 && below())) ++a->progress;
                    break;
                case 1:
                    if ((dir == 0 && e.delta > 0) || (dir == 1 && above() && e.delta > 0) ||
                        (dir == 2 && below() && e.delta < 0))
                        ++a->progress;
                    break;
                case 2:
                    if ((dir == 0 && e.delta < 0) || (dir == 1 && above() && e.delta < 0) ||
                        (dir == 2 && below() && e.delta > 0))
                        ++a->progress;
                    break;
                }
                break;
            }
            case 3: {  // a pov at the direction (hundredths of a degree, -1 centred)
                if (kind != InputEvent::EVENT_POV || e.code != target) break;
                const unsigned long dir = (code >> 12) & 0xf;
                if (dir > 8) break;
                if (e.value == (dir == 8 ? -1 : static_cast<int>(4500 * dir))) ++a->progress;
                break;
            }
            default: a->progress = 0; break;
            }
            if (a->progress == before || a->progress == 0) continue;
            a->lastTime = e.time;
            if (a->progress != a->strokes.count) continue;
            a->progress = 0;
            if (a->flush) flushed = true;
            LockObservers();
            for (ObserverIterator o = FirstObserver(); o.observer.node; ++o) {
                if ((*o)->OnInputAction(id, a->GetName(), e) == InputEvent::STOP) {
                    stopped = exclusive;
                    break;
                }
            }
            UnlockObservers();
            if (flushed) {
                for (IListLink* r = d->actions.head.next; r != &d->actions.head; r = r->next)
                    static_cast<DeviceAction*>(FromLink(r))->progress = 0;
                break;
            }
        }
    }
    return stopped ? InputEvent::STOP : InputEvent::PROPAGATE;
}

// 0x1000ab20: a copy goes to the list of its name (made when new); false when an equal action is there
bool InputActionMapper::AddToActionList(const InputAction& action)
{
    ActionTable::Iterator i = table.Find(action.GetName());
    ActionList* list;
    if (!i.node) {
        list = new ActionList;
        list->name = action.GetName();
        table.Insert(list);
    } else {
        list = *i.node;
        for (IListLink* l = list->list.head.next; l != &list->list.head; l = l->next)
            if (*FromLink(l) == action) return false;
    }
    LinkTail(list->list, new InputAction(action), &InputAction::link);
    return true;
}

// 0x1000ad00: each action("name") { variant { key / button / axis / pov / flush = ...; } } of the map. A variant
// with strokes becomes an action (added to the devices when there are some: the results combine as in Load).
InputActionMapper::Result InputActionMapper::LoadActionMap(const ConfigData& map)
{
    int result = 0;
    PString actionKey("action"), variantKey("variant"), keyKey("key"), buttonKey("button"), axisKey("axis"),
        povKey("pov"), flushKey("flush"), empty("");
    PString current;
    const PString* const names = InputStrings::Get().names;
    const int actions = map.GetChildCount();
    for (int i = 0; i < actions; ++i) {
        const ConfigData& entry = map[i];
        if (entry.Name() != actionKey) continue;
        current = entry.Value();
        if (current == empty || !current.node) continue;
        const int variants = entry.GetChildCount();
        for (int j = 0; j < variants; ++j) {
            const ConfigData& variant = entry[j];
            if (variant.Name() != variantKey) continue;
            InputAction action;
            const int lines = variant.GetChildCount();
            for (int k = 0; k < lines; ++k) {
                String text;
                JoinValues(variant[k], text);
                InputLex lex(static_cast<const char*>(text), text.Size());
                float time = 0.5f;
                const PString& what = variant[k].Name();
                if (what == keyKey) {
                    int key = 0;
                    unsigned long flags = 0x80;
                    for (int t = lex.Lex(); t > 0; t = lex.Lex()) {
                        switch (t) {
                        case InputLex::FLOAT: time = lex.FloatValue(); break;
                        case InputLex::PRESSED: flags |= 0x80; break;
                        case InputLex::RELEASED: flags &= ~0x80ul; break;
                        case InputLex::SHIFT: flags |= 4; break;
                        case InputLex::CTRL: flags |= 2; break;
                        case InputLex::ALT: flags |= 1; break;
                        case InputLex::KEY_NAME: {
                            const int found = FindName(names, 0x100, lex.Text());
                            key = found < 0 ? 0 : found;
                            break;
                        }
                        }
                    }
                    if (!key) continue;
                    const StrokeDef d = {(flags & ~0xff0000ul) | (static_cast<unsigned long>(key) << 16 & 0xff0000), Ms(time)};
                    Append(action.strokes, &d, 1);
                } else if (what == buttonKey) {
                    int button = -1;
                    unsigned long flags = 0x1000080;
                    for (int t = lex.Lex(); t > 0; t = lex.Lex()) {
                        switch (t) {
                        case InputLex::FLOAT: time = lex.FloatValue(); break;
                        case InputLex::PRESSED: flags |= 0x80; break;
                        case InputLex::RELEASED: flags &= ~0x80ul; break;
                        case InputLex::BUTTON_NAME: {
                            const int found = FindName(names + InputStrings::BUTTONS, 0x25, lex.Text());
                            button = found < 0 ? -1 : found < 0x20 ? found : found - 0x20;  // the mouse buttons
                            break;
                        }
                        }
                    }
                    if (button == -1) continue;
                    const StrokeDef d = {(flags & ~0xff0000ul) | (static_cast<unsigned long>(button) << 16 & 0xff0000), Ms(time)};
                    Append(action.strokes, &d, 1);
                } else if (what == axisKey) {
                    int axis = -1;
                    unsigned long flags = 0x2000000;
                    for (int t = lex.Lex(); t > 0; t = lex.Lex()) {
                        switch (t) {
                        case InputLex::FLOAT: time = lex.FloatValue(); break;
                        case InputLex::INC: flags = (flags & ~0x200ul) | 0x100; break;
                        case InputLex::DEC: flags = (flags & ~0x100ul) | 0x200; break;
                        case InputLex::CHANGED: flags &= ~0x300ul; break;
                        case InputLex::POSITIVE: flags = (flags & ~0x800ul) | 0x400; break;
                        case InputLex::NEGATIVE: flags = (flags & ~0x400ul) | 0x800; break;
                        case InputLex::AXIS_NAME: axis = FindName(names + InputStrings::AXES, 8, lex.Text()); break;
                        }
                    }
                    if (axis == -1) continue;
                    const StrokeDef d = {(flags & ~0xff0000ul) | (static_cast<unsigned long>(axis) << 16 & 0xff0000), Ms(time)};
                    Append(action.strokes, &d, 1);
                } else if (what == povKey) {
                    int pov = -1;
                    unsigned long dir = 0xf;
                    for (int t = lex.Lex(); t > 0; t = lex.Lex()) {
                        if (t == InputLex::FLOAT)
                            time = lex.FloatValue();
                        else if (t == InputLex::POV_CENTER)
                            dir = 8;
                        else if (t >= InputLex::POV_N && t <= InputLex::POV_N + 7)
                            dir = static_cast<unsigned long>(t - InputLex::POV_N);
                        else if (t == InputLex::POV_NAME)
                            pov = FindName(names + InputStrings::POVS, 4, lex.Text());
                    }
                    if (pov == -1 || dir == 0xf) continue;
                    InputExtensionState state;
                    state.code = ((static_cast<unsigned long>(pov) & 0xff | 0x300) << 4 | dir) << 12;
                    action.Add(state, time);
                } else if (what == flushKey) {
                    for (int t = lex.Lex(); t > 0; t = lex.Lex()) {
                        if (t == InputLex::TRUE_)
                            action.flush = true;
                        else if (t == InputLex::FALSE_)
                            action.flush = false;
                    }
                }
            }
            if (!action.strokes.count) continue;
            SetText(action.name, current.Text());
            if (!devices.count) {
                AddToActionList(action);
                continue;
            }
            const int r = AddAction(action).code;
            if (result == 0)
                result = r;
            else if (result == 3 && r == 4)
                result = 4;
        }
    }
    return result;
}

InputDeviceCapsList* InputActionMapper::EnumerateDevices()  // 0x1000baa0
{
    if (!devices.count) return nullptr;
    auto* list = static_cast<InputDeviceCapsList*>(Alloc::New(sizeof(InputDeviceCapsList)));
    if (list) new (list) InputDeviceCapsList;
    for (Device* d = devices.next; d != devices.Head(); d = d->next) {
        InputDeviceCaps caps;
        d->device->GetCaps(caps);
        list->Add(caps);
    }
    return list;
}

IList<InputAction>* InputActionMapper::EnumerateActions()  // 0x1000bb70
{
    if (!table.count) return nullptr;
    auto* list = static_cast<IList<InputAction>*>(Alloc::New(sizeof(IList<InputAction>)));
    if (list) new (list) IList<InputAction>;
    ActionTable::Iterator i(table);
    for (i.Begin(); i.node; i.Next()) {
        IList<InputAction>& from = (*i.node)->list;
        for (IListLink* l = from.head.next; l != &from.head; l = l->next)
            LinkTail(*list, new InputAction(*FromLink(l)), &InputAction::link);
    }
    return list;
}

}  // namespace Jet
