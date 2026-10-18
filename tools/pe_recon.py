"""PE recon for Trainz 2012 binaries: arch, flags, sections, imports, RTTI class count, packer hints.

usage: uv run --with pefile python pe_recon.py <file.exe|dll> [...]
"""
import re
import sys

import pefile

RTTI = re.compile(rb"\.\?A[VU][A-Za-z0-9_@?$]{2,200}@@")


def recon(path):
    pe = pefile.PE(path, fast_load=False)
    fh, oh = pe.FILE_HEADER, pe.OPTIONAL_HEADER
    print(f"=== {path}")
    print(f"machine=0x{fh.Machine:04x} laa={bool(fh.Characteristics & 0x20)} "
          f"dynbase={bool(oh.DllCharacteristics & 0x40)} nx={bool(oh.DllCharacteristics & 0x100)} "
          f"subsystem={oh.Subsystem} linker={oh.MajorLinkerVersion}.{oh.MinorLinkerVersion} "
          f"imagebase=0x{oh.ImageBase:x} entry=0x{oh.AddressOfEntryPoint:x}")
    for s in pe.sections:
        name = s.Name.rstrip(b"\0").decode(errors="replace")
        print(f"  section {name:8} vsize={s.Misc_VirtualSize:>9} rawsize={s.SizeOfRawData:>9} "
              f"entropy={s.get_entropy():.2f}")
    rich = pe.parse_rich_header()
    if rich:
        ids = sorted({(v >> 16, v & 0xFFFF) for v in rich["values"][::2]})
        print(f"  rich: {len(ids)} tool ids, max build {max(b for _, b in ids)}")
    if hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        for imp in pe.DIRECTORY_ENTRY_IMPORT:
            print(f"  import {imp.dll.decode():32} {len(imp.imports)} funcs")
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        print(f"  exports: {len(pe.DIRECTORY_ENTRY_EXPORT.symbols)}")
    data = open(path, "rb").read()
    classes = sorted(set(RTTI.findall(data)))
    print(f"  RTTI type descriptors: {len(classes)}")
    for c in classes[:15]:
        print(f"    {c.decode(errors='replace')}")


for p in sys.argv[1:]:
    try:
        recon(p)
    except Exception as e:  # keep going over the whole list
        print(f"=== {p}: {e}")
