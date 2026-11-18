#!/usr/bin/env python3
"""Dump the DIALOGEX and STRINGTABLE resources of a PE file: control id,
class and text of every dialog item, and the string table entries.

usage: pe_dialogs.py <exe>"""
import struct
import subprocess
import sys

if len(sys.argv) < 2:
    sys.exit(__doc__)
exe = sys.argv[1]
img = open(exe, 'rb').read()
base_va = struct.unpack_from('<I', img, struct.unpack_from('<I', img, 0x3c)[0] + 0x34)[0]
S = []
for l in subprocess.run(['objdump', '-h', exe], capture_output=True, text=True).stdout.splitlines():
    f = l.split()
    if len(f) >= 7 and f[0].isdigit():
        S.append((f[1], int(f[3], 16) - base_va, int(f[2], 16), int(f[5], 16)))


def off(rva):
    for name, v, s, o in S:
        if v <= rva < v + s:
            return o + rva - v


rsrc = [x for x in S if x[0] == '.rsrc'][0][3]


def walk(o, path):
    nn, ni = struct.unpack_from('<HH', img, rsrc + o + 12)
    for i in range(nn + ni):
        name, ent = struct.unpack_from('<II', img, rsrc + o + 16 + 8 * i)
        if name & 0x80000000:
            no = rsrc + (name & 0x7fffffff)
            ln = struct.unpack_from('<H', img, no)[0]
            name = img[no + 2:no + 2 + 2 * ln].decode('utf-16le')
        if ent & 0x80000000:
            yield from walk(ent & 0x7fffffff, path + [name])
        else:
            rva, sz = struct.unpack_from('<II', img, rsrc + ent)
            yield path + [name], img[off(rva):off(rva) + sz]


def sz_or_ord(b, p):
    if struct.unpack_from('<H', b, p)[0] == 0xffff:
        return struct.unpack_from('<H', b, p + 2)[0], p + 4
    e = p
    while struct.unpack_from('<H', b, e)[0]:
        e += 2
    return b[p:e].decode('utf-16le'), e + 2


CLS = {0x80: 'Button', 0x81: 'Edit', 0x82: 'Static', 0x83: 'ListBox', 0x85: 'ComboBox'}
for path, b in walk(0, []):
    if path[0] == 6:
        p, first = 0, (path[1] - 1) * 16
        for i in range(16):
            ln = struct.unpack_from('<H', b, p)[0]
            s = b[p + 2:p + 2 + 2 * ln].decode('utf-16le')
            p += 2 + 2 * ln
            if s:
                print('STRING %5d %r' % (first + i, s))
    if path[0] != 5:
        continue
    if struct.unpack_from('<HH', b, 0) != (1, 0xffff):
        print('DIALOG %s (classic DLGTEMPLATE, not decoded)' % path[1])
        continue
    style, n, cx, cy = struct.unpack_from('<I', b, 12)[0], struct.unpack_from('<H', b, 16)[0], \
        *struct.unpack_from('<hh', b, 22)
    p = 26
    menu, p = sz_or_ord(b, p)
    cls, p = sz_or_ord(b, p)
    title, p = sz_or_ord(b, p)
    print('DIALOG %s %r %dx%d' % (path[1], title, cx, cy))
    if style & 0x48:                       # DS_SETFONT | DS_SHELLFONT
        p += 6
        face, p = sz_or_ord(b, p)
    for i in range(n):
        p = (p + 3) & ~3
        x, y, w, h, cid = struct.unpack_from('<hhhhI', b, p + 12)
        p += 24
        c, p = sz_or_ord(b, p)
        tx, p = sz_or_ord(b, p)
        p += 2 + struct.unpack_from('<H', b, p)[0]
        print('  id=%-6d %-18s %-40r @%d,%d %dx%d' % (
            cid if cid != 0xffffffff else -1, CLS.get(c, c), tx, x, y, w, h))
