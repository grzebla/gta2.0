#!/usr/bin/env python3
"""Carves the files embedded in an InstallShield "Setup Player" single-file EXE (read-only;
the installer is never executed).

Entry layout observed in the Rockstar 2004 GTA2.exe (InstallShield 7-era):
    name\0 Disk1\<name>\0 version\0 decimal-size\0 <size raw bytes>
Output: <out>/Disk1/{data1.hdr,data1.cab,data2.cab,...} for `unshield`.

Usage: python3 -I tools/gta2-install/carve_isplayer.py <GTA2.exe> <out-dir>
"""
import mmap, os, re, sys, hashlib
src, out = sys.argv[1], sys.argv[2]
f = open(src, 'rb'); m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
pat = re.compile(rb'(Disk1\\([^\x00\\]{1,160}))\x00([0-9.]{1,24})\x00([0-9]{1,12})\x00')
pos, n = 0, 0
for mt in pat.finditer(m):
    if mt.start() < pos:
        continue
    path, name, ver, size = mt.group(1), mt.group(2), mt.group(3), int(mt.group(4))
    # the entry must be preceded by its own bare name + NUL
    if m[mt.start() - len(name) - 1:mt.start()] != name + b'\x00' or mt.end() + size > len(m):
        continue
    data = m[mt.end():mt.end() + size]
    rel = path.decode('latin1').replace('\\', '/')
    dst = os.path.join(out, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, 'wb') as o:
        o.write(data)
    print(f"{mt.end():#010x} {size:>11} {hashlib.sha256(data).hexdigest()[:16]} {rel}")
    pos, n = mt.end() + size, n + 1
print(f"{n} files, carved up to {pos:#x} of {len(m):#x}")
