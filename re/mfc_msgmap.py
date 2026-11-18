#!/usr/bin/env python3
"""Find MFC message-map entries (AFX_MSGMAP_ENTRY: nMessage, nCode, nID,
nLastID, nSig, pfn) whose handler lies in [lo, hi): which function a
button (WM_COMMAND 0x111, id from pe_dialogs.py) or notification runs.

usage: mfc_msgmap.py <exe> <lo-hex> <hi-hex>"""
import struct
import subprocess
import sys

if len(sys.argv) < 4:
    sys.exit(__doc__)
exe, lo, hi = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
img = open(exe, 'rb').read()
for l in subprocess.run(['objdump', '-h', exe], capture_output=True, text=True).stdout.splitlines():
    f = l.split()
    if len(f) >= 7 and f[0].isdigit() and f[1] in ('.rdata', '.data'):
        vma, size, off = int(f[3], 16), int(f[2], 16), int(f[5], 16)
        for o in range(off, off + size - 24, 4):
            msg, code, i0, i1, sig, pfn = struct.unpack_from('<6I', img, o)
            if lo <= pfn < hi and msg < 0x10000 and i0 <= i1 < 0x10000 and sig < 0x100:
                print('%08x msg=%04x code=%-6d id=%d..%d sig=%d handler=%08x'
                      % (vma + o - off, msg, code if code < 0x8000 else code - 0x10000,
                         i0, i1, sig, pfn))
