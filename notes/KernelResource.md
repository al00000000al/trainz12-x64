# KernelResource_7c (step 5, last small module): analysis notes

187 exports, ~30 KB code (0x10001000..0x100085c0). Listing: dumpbin /disasm + annotate_asm.py, compacted
(scratchpad kr2.asm: "ADDR mnemonic operands ; note", int 3 dropped).

Classes: Resource (0x5c), RedirResource (0x60), ResourceVariant (0xc), ResourceInstance (0xc), ResourceTypeBase
(0x20, a PluginType), ResourceDatabase (+Find 0x124, SearchHandle / RDBSearchHandle 0x444), ResourceNamelessDatabase,
ResourceBin (vftable 1000A544) / ResourceBinCache (0x40, 1000A6F0) / ResourceBinInstant (8, 1000A6D0),
ResourceSystem (0x104) + ResourceSystemTask (0x30, a Task, 1000A6B8) + the loader thread object (0x6c, 10007E80).

Plan: the whole module, then end-to-end diff tests with one ResourceSystem per module (each has its own statics),
single-threaded through Options "ResourceCaching=0" (Init reads it; the thread is made only when caching).

## Resource (0x5c), vftable 1000A274 (slots: see include/jet/Resource.h)
- +0x04 name PString, +0x08 owner db, +0x0c orig db, +0x10 creation time (float, Time::GetSeconds),
  +0x14 priority, +0x18 heap priority, +0x1c refs, +0x20 last-use time (= resourceSystem+0xdc), +0x24 internal refs
  (1 at birth), +0x28 status, +0x2c observers FDA<ResourceObserver*,13>, +0x30 observers notified (byte),
  +0x34 type (ResourceTypeBase*, AddReference / ReleaseReference = its vtable 0 / 1), +0x38 load name PString,
  +0x3c scramble key String, +0x40 hits, +0x44 misses, +0x48 locks, +0x4c redirs FDA<Resource*,13>,
  +0x50 size cache (-1), +0x54 data size cache (-1), +0x58 flags: 1 counted in the system's totals, 2 aliased,
  4 txt, 8 redir.
- status: 0 ready?, 1 (no owner at birth / reset), 2,3 queued/loading, 4 loaded, 6 available, 7 (loaded, inactive?),
  8 invalid. GetStatusString: table at 0x10010170.
- ctor(name, owner): as above; status = owner ? 0 : 1; ++totalResources. Reads resourceSystem->0xdc (must exist).
- copy ctor / operator=: field copy (floats through x87), FDAs chunk-copied (0x10001a70, AppendChunks), String
  copy, flags bits 0..3 copied one by one; operator= frees its FDAs first.
- dtor: --totalResources; status 6/7 --totalLoaded; 6 --totalActive; free redirs FDA, String, load name, observers
  FDA, name.
- AddReference ++refs. ReleaseReference: --refs; 0: owner ? (lastUse = rs->0xdc; vtbl[7] GetMustResetWhenFinished ->
  ResetToReady; true) : (--internal; 0 -> DeleteThis; true); else false. InternalAdd/Release on +0x24.
- Lock (0x10002190): lastUse = rs->0xdc; loop: status 2/3 and rs caching (+0x10 bit0) -> 0x10008310(thread
  +0x14, this) [ecx=this, edx=thread]; status != 6: 0x10007530(this); status 2/3 -> 0x10007610(this), loop.
  status 6 -> hits / totalHits (first hit) / locks; result 0.
  status 4 -> rs 0x10007720(this) (ecx=rs); then status 4: vtbl[0x30/4=12] LoadComplete -> 0: SetAvailableOnLoad,
  vtbl[13] OnLoadNotify; else status 8. status 6 -> hits, 0x10007610, 0. status 8 -> 0x10007610, result 7.
  else: ++misses (+0x44), ++totalMisses, TemplateLoad(loadName text): fail -> status 8, 0x10007610, result 1;
  ok -> status 4, ++totalLoaded, FinalizeLoad: ok -> hits..., else 0x10007610, result 1.
- Unlock: --locks. PreLoad: TemplateLoad -> fail status 8; ok 0x100074a0(this), ++totalLoaded.
- TemplateLoad(name): txt flag -> String(name) + ".txt" (0x10002d60 concat) -> vtbl[11] LoadTxt(String text);
  else vtbl[10] Load(name). ok = result 0.
- FinalizeLoad: vtbl[12] LoadComplete ok -> SetAvailableOnLoad, vtbl[13] OnLoadNotify, true; else status 8, false.
- ConvertResourceName(String& s): if s non-empty: ToLower; Path p(s); Normalize; Path q(loadName); DelName(false);
  q.AddPath(p); s = q.
- SetType: old type vtbl[1] release; set; new vtbl[0] addref. AttachType: release old, set (no addref).
- GetOwnerName: owner ? owner+0xc : static PString "" (0x1001067c, lazily made from 0x1000a50d).
- CreateInstance(name): r = type->vtbl[3] Create(name, owner); r.loadName = loadName; r.SetType-like (release old,
  set, addref); r.flags bit 2 = this bit 2; return r.
- DeleteThis: type = +0x34; RemoveRedirs; if flags&1: --rs+0xe4, rs+0xf0 -= GetDataSize (cached +0x54);
  owner -> owner->Remove(this) (0x10005010); --rs+0xe0; rs+0xec -= GetSize (+0x50); vtbl[8](1) delete; type
  vtbl[1] release.
- Inactive: status 6: --totalActive; db = owner+0x3c (a SyncObject: vtbl[2] Lock(-1)); refs 0 -> rs(DB)->+8
  (bin) vtbl[4](this); db vtbl[4] Unlock().
- SetPriority: +0x14. AddObserver: notified ? observer vtbl[0](observer, this) : observers.Add. RemoveObserver:
  observers.Find + Remove. LoadComplete: 0. Load / LoadTxt: 6. Cache(bool force): force -> status 1; status 1 and
  caching -> 0x10008090(thread +0x14, this) [ecx=thread]. SetScrambleKey: String::operator=. AddRedir:
  redirs.Add; RemoveRedir: Find + Remove. RemoveRedirs: each redir --internal, 0 -> DeleteThis; free FDA.
- SetAvailableOnLoad: flags&1 unset -> set, ++rs+0xe4, rs+0xf0 += GetDataSize; status 4 -> 6, ++totalActive,
  refs 0 -> Inactive.
- OnLoadNotify: each observer (from the last chunk's last item backwards) vtbl[0](observer, this); notified = 1.
- ResetToReady: status 6/7: caching -> 0x10008310(thread, this); rs 0x10007720(this); status 1.
- Touch: lastUse = rs+0xdc. LoadAll: vtbl[0] Lock ok -> vtbl[1] Unlock, true. IsAllLoaded: 6 -> 0, 8 -> 1, else 5.
- GetSize / GetDataSize: cached from vtbl[15] / vtbl[16]. CalculateSize -1, CalculateDataSize 0.
- Accessors: IsAvailable status==6, IsInvalid ==8, SetInvalid status=8, SetStatus (thiscall!), IsLocked locks>0,
  GetDB orig ?: owner, IsRedir/SetRedir bit 3, IsAliased bit 1, IsTxt bit 2, SetLoadName.
- InitRS(rs): Resource::resourceSystem.
- FDA<T*,13> helpers: 0x10002bd0 Add (edi = array), 0x10002c50 Remove(iterator) (memmove inside the chunk),
  0x10007b00 Find.
- RedirResource (0x60): Resource + target +0x5c; ctor(name, owner, target): Resource, flags |= 8. dtor: target ->
  its redirs Find(this)+Remove; ~Resource. GetRedirection: target.
- ResourceVariant (0xc): {+0, +4, name PString +8}; ctor(name) zeroes then name.
- ResourceDatabase::Find (0x124): File::Entry + int +0x120.

## Quirks of the original found by diff_kr_system (kept in the port)
- Two resources of one name can sit in one table (e.g. two invalid ";g" made by Get). Remove(const Resource*)
  finds by name, so deleting the second-listed one takes the first one's slot; the first one's own Remove then
  finds the second by the name still in its freed memory (the original's ~PString does not zero the node). Our
  ~PString leaves the node too (PString.h), or the table keeps a freed entry.
- ResourceBinCache::Add: priority = NextFloat(spread) + oldest + time summed on the x87 stack (53-bit) and rounded
  once; NextFloat itself is a double function in KernelStandard (st0 unrounded). Resource.cpp calls it through a
  double-returning pointer. (x64: no x87; revisit if bin priorities must stay bit-exact there.)
- nextSizeCheck (0x10010678) is a static: a new ResourceSystem with a smaller clock skips CheckSize until it
  passes the old one.
- LCRand in the bin is seeded from the clock (LCRand()), so bin priorities differ run to run.
- With the loader in use: Remove(name) of a queued resource drops the table's reference but leaves it listed (the
  queue's keeps it alive); the claim that later unqueues it deletes it under the table. A resource queued twice
  (Cache(true), or reset to Ready on release, while queued) keeps a heap entry after the database destructor
  deletes it (that loop drops internal references until the resource goes), and the loader's exit deletes it
  again. Loader Unqueue of the last heap element sifts it up from the slot past the end, so when its heap
  priority beats its parent's it goes back in and the parent drops out (heap priorities are only set by TakeNext).
  The game avoids these by letting the loader drain; the test claims queued resources first.
