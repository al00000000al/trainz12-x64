"""Compare the exported vftables (??_7...) of two builds of a 32-bit module, slot by slot.

Each slot is classified as an import (a `jmp [IAT]` thunk, shown as the imported name) or as a function of the
module itself. A rebuilt module must have the same number of slots and the same import in every slot where the
original has one; slots with local code must be local in both (their bodies are checked by the module's tests).

usage: python compare_vtables.py <original.dll> <rebuilt.dll> [slots to read, default 64]
"""
import struct
import sys


class PE:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        self.secs = [struct.unpack_from("<8sIIII", d, opt + optsz + 40 * i) for i in range(nsec)]
        self.code = [(va, va + vs) for name, vs, va, rs, ro in self.secs if b".text" in name]
        exp_rva = struct.unpack_from("<I", d, opt + 96)[0]
        o = self.off(exp_rva)
        n_names, f_rva, n_rva, o_rva = struct.unpack_from("<IIII", d, o + 24)[0], *struct.unpack_from("<III", d, o + 28)
        self.exports = {}
        for i in range(n_names):
            name = self.cstr(self.off(struct.unpack_from("<I", d, self.off(n_rva) + 4 * i)[0]))
            ordinal = struct.unpack_from("<H", d, self.off(o_rva) + 2 * i)[0]
            self.exports[name] = struct.unpack_from("<I", d, self.off(f_rva) + 4 * ordinal)[0]
        self.imports = {}
        o = self.off(struct.unpack_from("<I", d, opt + 96 + 8)[0])
        while True:
            oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", d, o)
            if not name_rva:
                break
            lookup = self.off(oft or ft)
            k = 0
            while True:
                e = struct.unpack_from("<I", d, lookup + 4 * k)[0]
                if not e:
                    break
                self.imports[self.base + ft + 4 * k] = f"#{e & 0xFFFF}" if e & 0x80000000 else self.cstr(self.off(e) + 2)
                k += 1
            o += 20

    def off(self, rva):
        for _, vs, va, rs, ro in self.secs:
            if va <= rva < va + max(vs, rs):
                return ro + rva - va
        return None

    def cstr(self, o):
        return self.d[o:self.d.find(b"\0", o)].decode("latin1")

    def is_code(self, rva):
        return any(a <= rva < b for a, b in self.code)

    def slots(self, vt_rva, limit):
        out = []
        for i in range(limit):
            o = self.off(vt_rva + 4 * i)
            if o is None:
                break
            target = struct.unpack_from("<I", self.d, o)[0] - self.base
            if not self.is_code(target):
                break
            to = self.off(target)
            if self.d[to:to + 2] == b"\xff\x25":  # jmp dword ptr [iat]
                out.append(self.imports.get(struct.unpack_from("<I", self.d, to + 2)[0], "?import"))
            else:
                out.append("local")
        return out


orig, ours = PE(sys.argv[1]), PE(sys.argv[2])
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 64
bad = 0
for name in sorted(n for n in orig.exports if n.startswith("??_7")):
    a = orig.slots(orig.exports[name], limit)
    b = ours.slots(ours.exports[name], limit) if name in ours.exports else []
    diffs = [i for i in range(max(len(a), len(b))) if (a[i] if i < len(a) else None) != (b[i] if i < len(b) else None)]
    print(f"{name}: {len(a)} slots, {'same' if not diffs else f'{len(diffs)} differ'}")
    for i in diffs:
        bad += 1
        print(f"  [{i}] orig {a[i] if i < len(a) else '-'}  ours {b[i] if i < len(b) else '-'}")
sys.exit(1 if bad else 0)
