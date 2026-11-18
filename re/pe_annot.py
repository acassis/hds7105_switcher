#!/usr/bin/env python3
"""Print the objdump disassembly lines whose address is in [lo, hi) and
annotate every immediate that points at a C string or UTF-16 string in a
data section.  For binaries without symbols (dis.py needs them).

usage: pe_annot.py <exe> <objdump-d.txt> <lo-hex> <hi-hex>
  objdump -d -M intel tool.exe > dis.txt
  re/pe_annot.py tool.exe dis.txt 401000 406000"""
import re
import subprocess
import sys

if len(sys.argv) < 5:
    sys.exit(__doc__)
exe, dis, lo, hi = sys.argv[1], sys.argv[2], int(sys.argv[3], 16), int(sys.argv[4], 16)
S = []
for l in subprocess.run(['objdump', '-h', exe], capture_output=True, text=True).stdout.splitlines():
    f = l.split()
    if len(f) >= 7 and f[0].isdigit():
        S.append((f[1], int(f[3], 16), int(f[2], 16), int(f[5], 16)))
DATA = [(v, v + n) for name, v, n, o in S if name in ('.rdata', '.data')]
img = open(exe, 'rb').read()


def rd(va, n):
    for name, vma, size, off in S:
        if vma <= va < vma + size:
            o = off + va - vma
            return img[o:o + n]
    return b''


def sval(va):
    b = rd(va, 200)
    a = b.split(b'\0')[0]
    if len(a) >= 3 and all(32 <= c < 127 or c in (9, 10, 13) for c in a):
        return repr(a.decode())
    e = 0
    while e + 1 < len(b) and (b[e] or b[e + 1]):
        e += 2
    try:
        t = b[:e].decode('utf-16le')
    except UnicodeDecodeError:
        return None
    if len(t) >= 2 and t.isprintable() and all(ord(c) < 0x3000 or 0x4e00 <= ord(c) < 0xa000 for c in t):
        return 'L' + repr(t)
    return None


for l in open(dis):
    m = re.match(r'\s*([0-9a-f]+):\t[0-9a-f ]+\t(.*)', l)
    if not m or not lo <= int(m.group(1), 16) < hi:
        continue
    ann = [s for v in (int(h, 16) for h in re.findall(r'0x([0-9a-f]{6,8})', m.group(2)))
           if any(a <= v < b for a, b in DATA) for s in [sval(v)] if s]
    print('%8s: %s%s' % (m.group(1), m.group(2), ('   ; ' + ' '.join(ann)) if ann else ''))
