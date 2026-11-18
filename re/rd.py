#!/usr/bin/env python3
"""Read N dwords (d, default) or bytes (b) at a VMA of the PE image,
e.g. a switch jump table.

usage: rd.py <exe> <vma-hex> <count> [b|d]"""
import sys, struct, subprocess
if len(sys.argv) < 4:
    sys.exit(__doc__)
exe, va, n = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3])
kind = sys.argv[4] if len(sys.argv) > 4 else 'd'
S = []
for l in subprocess.run(['objdump', '-h', exe], capture_output=True, text=True).stdout.splitlines():
    f = l.split()
    if len(f) >= 7 and f[0].isdigit():
        S.append((int(f[3], 16), int(f[2], 16), int(f[5], 16)))
img = open(exe, 'rb').read()
for vma, size, off in S:
    if vma <= va < vma + size:
        o = off + va - vma
        if kind == 'd':
            print([hex(x) for x in struct.unpack('<%dI' % n, img[o:o + 4 * n])])
        else:
            print(img[o:o + n].hex(' '))
