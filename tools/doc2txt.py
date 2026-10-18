"""Extract plain text from a Word 97-2003 .doc via its piece table (no Word needed).

usage: uv run --with olefile python doc2txt.py <in.doc> <out.txt>
"""
import struct
import sys

import olefile

ole = olefile.OleFileIO(sys.argv[1])
word = ole.openstream("WordDocument").read()
flags = struct.unpack_from("<H", word, 0x0A)[0]
table = ole.openstream("1Table" if flags & 0x0200 else "0Table").read()
fc_clx, lcb_clx = struct.unpack_from("<II", word, 0x01A2)
clx = table[fc_clx:fc_clx + lcb_clx]

# skip Prc entries (0x01), then Pcdt (0x02): lcb, PlcPcd = (n+1) CPs + n Pcd(8 bytes)
i = 0
while clx[i] == 0x01:
    i += 3 + struct.unpack_from("<H", clx, i + 1)[0]
assert clx[i] == 0x02, "no piece table"
lcb = struct.unpack_from("<I", clx, i + 1)[0]
plc = clx[i + 5:i + 5 + lcb]
n = (lcb - 4) // 12
cps = struct.unpack_from(f"<{n + 1}I", plc, 0)
out = []
for k in range(n):
    fc = struct.unpack_from("<I", plc, (n + 1) * 4 + k * 8 + 2)[0]
    count = cps[k + 1] - cps[k]
    if fc & 0x40000000:  # compressed: 8-bit cp1252 at fc/2
        off = (fc & 0x3FFFFFFF) // 2
        out.append(word[off:off + count].decode("cp1252", "replace"))
    else:
        out.append(word[fc:fc + 2 * count].decode("utf-16-le", "replace"))
text = "".join(out).replace("\r", "\n").replace("\x07", "\t")
text = "".join(ch for ch in text if ch in "\n\t" or ord(ch) >= 32)
open(sys.argv[2], "w", encoding="utf-8").write(text)
print(f"{len(text)} chars -> {sys.argv[2]}")
