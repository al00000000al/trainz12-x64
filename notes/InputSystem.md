# InputSystem_7c reconstruction notes (private)

Units: U1 ObserverSubject (done), U2 devices/driver/ids (done), U3 ASCIICode/InputStrings (done),
U4 EmuDevice, U5 Input, U6 InputExtension/InputUIExtension, U7 InputAction/InputActionMapper,
U8 ForceEffect/ForceResource + JetGetPluginsInputSystem.

Annotated listing: %TEMP%\is_n.asm (dumpbin + annotate_asm.py, addresses without colon).

## U4 EmuDevice (0x360 bytes) : InputDevice, InputObserver (vptr +0x238)
- +0x23c AutoReference<InputDevice> source (the keyboard, from Input::GetDevice(UIdKeyboard))
- +0x240 ring buffer of InputEvent (0x14): {+0x240 read?, +0x244 write?, +0x248 lastIndex=0xff, +0x24c count,
  +0x250 data = Alloc::New(256*0x14)} (push helper 0x10001870)
- +0x254 FastDynamicArray<uint32 binding, ?> sorted insert helper 0x100048e0; chunks freed in dtor / Done
- +0x258 = 0, +0x25c byte = 1 (property 8 get/set; SetProperty 8 notifies 0x70)
- +0x260 axes[8] x 20 bytes: {+0 value int, +4 ?, +8 float, +0xc speed int, +0x10 byte flags (&~3)}
- +0x304 ranges[8] x 12 bytes {min, max, ?}: Init sets x/y/z = {0, 0xffff}
- binding dword: byte0 key, byte1 target, word flags: bit0 b1, bit1 b2, bit2 b3 (modifiers), bits3-4 type
  (0 button, 1 axis, 2 pov?), bits5.. direction/c.
- Bind (0x100021b0) BUG kept: packs the low WORD of the key argument slot, so target = byte1 of key's slot
  (`mov dh,target` is dead). Callers: LoadDefaults inlines constants (correct targets); joystick defaults call
  Bind with `push imm` -> target 0; Load calls with push ebx / push edi: check byte1.
- Init(Input*, UId&, OS*): Done(); needs input+0x28 (initialized flag); type 2 for UIdMouseEmu, 4 for
  UIdJoystickEmu; source = input->GetDevice(UIdKeyboard); none -> Done, false. Options "InputEmulation"
  (SetSeparators(100152C8 str)), GetNext loop -> Load(file, null) until one succeeds; else LoadDefaults.
  Then ranges; axes[0..2].value = type==2 ? 0 : 0x7fff; source->AddObserver(observer part, 0x7fffffff);
  valid = 1; return true.
- Done: source->RemoveObserver(this observer); release source; zero axes (0xa0); state.type=0; free bindings
  chunks; handle=0; valid=0.
- GetCaps: mouse emu "Mouse emulator" caps 0xCC002801, button/axis masks from bindings (type 0 -> buttonMask,
  type 1 -> axisMask &0xff), counts via popcount 0x10005390; joystick "Joystick emulator" + type 2 -> povMask &0xf;
  other: UIdUnknown, "" names, zeros.
- GetProperty: 3 range[i] (i<8), 5 position (mouse i<3 state.mouse.position, joy i<8), 8 byte +0x25c.
- SetProperty: 8 -> +0x25c = index byte, notify 0x70 (all); 5 clamp into range -> position + axis value,
  PropertyChangedEvent; 3 set range, mouse: clamp axis value, (i<3) position; joystick: value = (max-min)/2.

### U4 status (done, 2026-10-19)
- Emu.cpp: all EmuDevice exports but Load (Load + the flex scanner InputLex = unit U4b, still the original's,
  called on our object through InputSystem_7c_orig.lib). Ranges are 8 x {unused, min, max} at +0x300.
- Original bugs kept: Bind packs the key slot's low word (target 0 from Bind; joystick defaults collapse onto
  button / axis 0), BitCount uses `>` for `>>`, joystick AxisEvent reports the mouse fields, a lost event is never
  delivered (the state, type included, is zeroed first; memset 0x100 not 0x104).
- tests/diff_is_emu.cpp: 329 376 checks, 0 mismatches (Time::Ms patched in both IATs). Init is not covered yet
  (needs a real Input: U5).
- In game EmuDevice is built by Input (original code) until U5 is ours.

## U5 Input (0x44 bytes), analysis
- +0x00 const char* optionKeys[3] = {"InputKeyboard","InputMouse","InputJoystick"}
- +0x0c InputDriver* drivers[3] (created; refs), +0x18 InputDriver* typeDrivers[3] (keyboard, mouse, joystick;
  refs), +0x24 int driverCount, +0x28 bool initialized,
- +0x2c DynamicArray<AutoReference<InputDevice>,13> devices (Add 0x10007e30 at end, Push 0x10007eb0 at front,
  Remove 0x10007f90), +0x30 DynamicArray<InputDeviceCaps,6> caps, +0x34 FDA<InputExtension*,13> extensions
  (Find 0x100081e0, Add 0x10004c60, Remove 0x100080e0), +0x38 critical shutdown handle, +0x3c startup handle,
  +0x40 OS*. Static isInstance (ctor 1, dtor 0).
- ctor: zero 0x0c..0x40 (+0x28 byte), isInstance=1, keys. dtor: isInstance=0, Done, free ext chunks, caps
  RemoveAll (0x10004be0), devices RemoveAll (0x10005020).
- Init(os): if initialized return true; os && os->GetOptions() else false; AddCriticalShutdown(static
  CriticalShutdown, this) -> +0x38; AddCriticalStartup -> +0x3c; for type 0..2 LoadDriver(type, os,
  os->GetOptions()) while it succeeds (stops at the first failure); initialized=1; os=+0x40; true.
- Done: if !initialized return; extensions: for each: if refcount(+8)==1 ext->Done (slot5); ext->ReleaseReference
  (RC +4); it = Remove(it). devices: for each: dev->Done (slot 15); release; item=0; it = Remove(it).
  typeDrivers release+0; drivers[count-1..0] release+0 (count--). Remove critical handlers (if set) -> 0.
  initialized=0; os=0.
- Update(dt): Profile p(&info "Input::Update" file ".../Jet/InputSystem/Input.cpp", {0,0,-1,0x80000000,0,0}):
  drivers[i]->Update(dt) (slot1, non-null); devices: dev->Update(dt) (slot 14); extensions: if refcount==1:
  it = Remove(it); if (ext refcount == 1) ext->Done(); ext->Release; else ext->Update(dt) (slot 6), ++it.
  return true.
- EnumerateDevices(int typeMask, ulong capsMask): EnumerateAll; if caps empty return null; list = New(4) head=0;
  for caps c: if (c.type & typeMask) && (!capsMask || (c.caps & capsMask) == capsMask) list.Add(c) (0x10007db0);
  if list empty: RemoveAll, Delete, return null. (6 per chunk 0x15c)
- GetDevice(id): emu ids -> EmuDevice(New 0x360), Init(this, id, os); ok -> devices.Add; else delete -> null.
  else find in devices by instanceId (+0x14) -> AddRef, return. else type=FindDeviceType(id); 0 -> EnumerateAll,
  again. 1/2/4 -> typeDrivers[0/1/2]->CreateDevice(id) (slot 3); local=AddRef; devices.Push(local); then common:
  dev->Update(0) (slot 14), *ret = dev AddRef, local release. other type -> null. (CreateDevice null -> crash)
- RegisterExtension(ext): if !extensions.Find(ext): ext AddRef (+4), Add.
- LoadDriver(type, os, opts): static PString "Default"; SetSeparators(key, ","); while GetNext(key, name):
  "Default" -> "Win32" (type 0,1) / "DirectX" (2); factory = LookupPluginType(group F3117B18, name) (null ->
  return false!); same factory id as drivers[i] -> typeDrivers[type]=drivers[i], AddRef, true; else
  drv = factory->CreateInstance (slot 3); drivers[count]=drv; if drv: if drv->Init(os) (slot 0): typeDrivers[type]
  = drv; count++; AddRef; true; else drv release (slot left stale); factory->ReleaseReference; next. false.
- EnumerateAll: typeDrivers[i]->Enumerate(caps, 1<<i). FindDeviceType: caps with id -> type, else 0.
- CriticalShutdown: devices slot 16; drivers[0..count) slot 4. Startup: drivers slot 5; devices slot 17.
  statics return 0. IsActiveWnd: os && os->GetWindow() ? GetWindow()->IsActiveWnd() : false.
- InputExtension (U6) 0x10: InputObserver +0, ReferenceCount +4 (count +8), +0xc active=1, +0xd exclusive=0.
  vtable: 0 OnInputEvent, 1 Init(Input&) (= RegisterExtension inline, true), 2 Activate, 3 SetExclusive,
  4 Reset, 5 Done, 6 Update(float).

## U5/U6/U8 status (2026-10-20): done, see MODLOG. Tests diff_is_input, diff_is_uiext, diff_is_force.

## U7 InputActionMapper (0x34) analysis
- +0 ObserverSubject<InputActionObserver>, +0xc InputExtension (active +0x18), +0x1c ActionTable
  {FDA<ActionList*,4>* buckets (new[] of 97 heads, eh vector ctor 1000C8B0/dtor 1000C8C0), count +0x20,
  bucketCount +0x24 = 0x61}, +0x28 device list (intrusive {next,prev} head + count +0x30).
- hash 0x10008e90(PString by value): last <=16 chars, h = h*8 ^ (signed char)c; bucket = h % 97 (0x1000c7b0).
- ActionList (0x10): {PString name +0, IList<InputAction> {next +4, prev +8, count +0xc}}; InputAction's IList
  node is +4/+8 (node ptr = action+4). Freed by 0x1000bd20 (pops from the back, deleting dtor slot0(1)).
- table iterator (0x1000c4e0 begin / 0x1000c000 next): {table*, bucket, node*, chunkHead, chunk} = 0x14 bytes.
  find 0x1000c370(name) -> iterator (node null if absent); insert 0x1000c280(name, list*) adds to bucket
  (FDA<ptr,4> Add 0x1000c900), count++; clear 0x1000c300 frees all bucket chunks, count=0.
- DeviceAction (0x58, 0x1000bdd0 ctor): {next +0, prev +4, InputDevice* +8 (AddRef), IList<DeviceAction'> +0xc
  {next,prev,count +0x14}, dead zones 8 x {lo,hi} +0x18 (0x1000bf10: memset 0x40; per axis GetProperty(3,i)
  -> range=b-a; GetProperty(4,i) -> half=range>>1, lo=half-a, hi=half+a)}. Dtor 0x1000be50: release device,
  null, delete the bound actions (vtable 10016524 = InputAction subclass 0x28 bytes: +0x20,+0x24 state).
- AddDevice(dev): null -> 2. new DeviceAction; link at tail of +0x28 list; if active dev->AddObserver(this+0xc,1)
  (slot1, prio 1); caps = dev->GetCaps; for each action list in the table: for each action (from list head):
  if action.TestDevice(caps): copy (0x28 object: InputAction copy + vtable 10016524, +0x20=+0x24=0) linked at
  tail of the DeviceAction's list; any=true. Per list: if none matched -> result 3. End: list empty -> 4 else
  result (0 or 3).
- RemoveDevice(UId)/(InputDevice*): find by device instanceId (+0x14); unlink, count--, dev->RemoveObserver
  (this+0xc) (slot 2), 0x1000be50, Delete. RemoveAllDevices: pop from the back (+0x2c) the same way.
- Done: RemoveAllDevices; for each table list: 0x1000bd20 + Delete; clear table.
- Activate(on): active=on; for each device: on ? AddObserver(obs, 1) : RemoveObserver(obs).
- Reset: for each device, for each bound action: +0x24 = 0.
- TestDevice(dev): caps; for each list: TestDevice(caps, list) (0x1000aad0) false -> false; true.
- TestDevice(dev, name): find list; none -> false; caps; TestDevice(caps, list).
- Load(file, db, mapName): ConfigParser; db ? Parse(db->GetFileName(file)) : Parse(file); fail -> 5.
  root (+0xc of parser?) null -> 5. n = root.GetChildCount; for i: child = root[i]; if child.name ==
  "actionmap" && (!mapName || child.value(+8) == mapName): r = LoadActionMap(child); r==0 -> result = ...
