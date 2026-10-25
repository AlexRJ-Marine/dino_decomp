#!/usr/bin/env python3
"""Quick PE + embedded-archive triage."""
import hashlib
import sys
from pathlib import Path

import pefile

p = Path(sys.argv[1])
data = p.read_bytes()
print(f"file: {p}")
print(f"size: {len(data):,}")
print(f"sha256: {hashlib.sha256(data).hexdigest()}")
print(f"md5: {hashlib.md5(data).hexdigest()}")

pe = pefile.PE(str(p), fast_load=False)
print(f"machine: {hex(pe.FILE_HEADER.Machine)}")
print(f"timestamp: {pe.FILE_HEADER.TimeDateStamp}")
print(f"characteristics: {hex(pe.FILE_HEADER.Characteristics)}")
print(f"optional magic: {hex(pe.OPTIONAL_HEADER.Magic)}")
print(f"linker: {pe.OPTIONAL_HEADER.MajorLinkerVersion}.{pe.OPTIONAL_HEADER.MinorLinkerVersion}")
print(f"subsystem: {pe.OPTIONAL_HEADER.Subsystem}")
print(f"entrypoint: {hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint)}")
print("sections:")
for s in pe.sections:
    print(f"  {s.Name.decode(errors='replace').rstrip(chr(0)):8} vsize={s.Misc_VirtualSize:>10} raw={s.SizeOfRawData:>10} ptr={s.PointerToRawData:>10} flags={hex(s.Characteristics)}")

print("imports:")
if hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        funcs = [i.name.decode(errors="replace") for i in e.imports if i.name][:8]
        print(f"  {e.dll.decode(errors='replace')}: {len(e.imports)} ({', '.join(funcs)})")
print("resources:")
if hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        name = pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        n = len(t.directory.entries) if t.directory else 0
        print(f"  {name}: {n} entries")

# overlay / embedded signatures
overlay = pe.get_overlay_data_start_offset()
print(f"overlay offset: {overlay} ({overlay/len(data)*100:.1f}% of file)" if overlay else "overlay: none")
needles = {
    "MS CAB (MSCF)": b"MSCF",
    "InstallShield CAB (!IS)": b"ISc(",
    "InstallShield 6 CAB": b"ISc6",
    "Inno Setup": b"Inno Setup",
    "NSIS": b"NullsoftInst",
    "Wise": b"Wise Installation",
    "InstallShield text": b"InstallShield",
    "SafeDisc": b"SafeDisc",
    "SecuROM": b"SecuROM",
    "StarForce": b"StarForce",
    "Bink": b"Bink",
    "Miles": b"Miles Sound System",
    "DirectX": b"DirectX",
    "CABINET": b"CABINET",
    "PrintDlg": b"PrintDlg",
}
for label, needle in needles.items():
    idx = data.find(needle)
    print(f"  {label:28} {'found @ ' + hex(idx) if idx >= 0 else 'not found'}")

# scan for embedded PE files in the last part / whole file
hits = []
start = 0
while True:
    i = data.find(b"PE\0\0", start)
    if i < 0:
        break
    hits.append(i)
    start = i + 1
print(f"embedded 'PE\\0\\0' occurrences: {len(hits)} (first 10: {[hex(h) for h in hits[:10]]})")
