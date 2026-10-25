"""Find exports of a rebuilt module that are only import thunks (jmp dword ptr [__imp_...]).

A name listed in implemented.txt whose declaration mangles differently from the original's is not defined by our
code; the linker then resolves it from the original's import library, and the module silently re-exports the
original function under that name. Such exports start with FF 25 (jmp [mem]) into the import table.

usage: python check_thunks.py <our module.dll> [implemented.txt]
  With implemented.txt only those names are checked (forwarded exports are not code at all).
"""
import struct
import sys

data = open(sys.argv[1], 'rb').read()
names_wanted = None
if len(sys.argv) > 2:
    names_wanted = set()
    for line in open(sys.argv[2], encoding='utf-8'):
        line = line.split('#', 1)[0].strip()
        if line:
            names_wanted.add(line.split('=', 1)[0])
pe = struct.unpack_from('<I', data, 0x3C)[0]
base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt = struct.unpack_from('<H', data, pe + 20)[0]
secs = []
for i in range(nsec):
    s = pe + 24 + opt + i * 40
    vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', data, s + 8)
    secs.append((vaddr, max(vsize, rawsize), rawptr))


def off(rva):
    for vaddr, size, rawptr in secs:
        if vaddr <= rva < vaddr + size:
            return rawptr + rva - vaddr
    return None


def cstr(rva):
    o = off(rva)
    return data[o:data.index(b'\0', o)].decode('latin1')


exp_rva, exp_size = struct.unpack_from('<II', data, pe + 24 + 96)
imp_rva, imp_size = struct.unpack_from('<II', data, pe + 24 + 104)
iat_rva, iat_size = struct.unpack_from('<II', data, pe + 24 + 96 + 12 * 8)
o = off(exp_rva)
_, nfunc, nname, afunc, aname, aord = struct.unpack_from('<IIIIII', data, o + 16)
funcs = [struct.unpack_from('<I', data, off(afunc) + 4 * i)[0] for i in range(nfunc)]

# the DLL behind each import slot: a jmp into a system DLL (kernel32 GetCurrentThreadId, winmm timeGetTime) is
# how a one-line wrapper compiles, not a re-export
slot_dll = {}
d = off(imp_rva)
while True:
    ilt, _, _, dll_name, first = struct.unpack_from('<IIIII', data, d)
    if not dll_name:
        break
    k = 0
    while struct.unpack_from('<I', data, off(first) + 4 * k)[0]:
        slot_dll[first + 4 * k] = cstr(dll_name).lower()
        k += 1
    d += 20
system = ('kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'winmm.dll', 'version.dll', 'shell32.dll',
          'ole32.dll')
bad = 0
checked = 0
for i in range(nname):
    name = cstr(struct.unpack_from('<I', data, off(aname) + 4 * i)[0])
    if names_wanted is not None and name not in names_wanted:
        continue
    rva = funcs[struct.unpack_from('<H', data, off(aord) + 2 * i)[0]]
    if exp_rva <= rva < exp_rva + exp_size:
        print('FORWARDED', name)
        bad += 1
        continue
    checked += 1
    p = off(rva)
    if p is not None and data[p:p + 2] == b'\xff\x25':
        target = struct.unpack_from('<I', data, p + 2)[0] - base
        if slot_dll.get(target) in system:
            continue
        if iat_rva <= target < iat_rva + iat_size or imp_rva <= target < imp_rva + imp_size + 0x10000:
            print('THUNK', name, slot_dll.get(target, ''))
            bad += 1
print(f'{checked} checked, {bad} not ours')
sys.exit(1 if bad else 0)
