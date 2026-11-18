#!/usr/bin/env python3
"""Disassemble functions from SWITCHER.exe by demangled-name regex.

usage: dis.py <exe> <syms.txt> <regex> [--lines]

  syms.txt  output of `objdump -t SWITCHER.exe`
  regex     matched against demangled names, e.g. '^HDS7105_workdata::'

Each instruction line is annotated with the C string or Qt 5 QStringLiteral
an immediate points at, and with the function an immediate names (so
connect() slot targets are resolved).  Function bounds are taken from the
next symbol, which is exact for this unoptimised debug build.
"""
import bisect
import re
import struct
import subprocess
import sys

if len(sys.argv) < 4:
    sys.exit(__doc__)
exe, symf, pat = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
TEXT_BASE = 0x401000          # COFF symbol value + .text VMA

funcs = []  # (addr, mangled)
sym_re = re.compile(r'\(sec\s+1\)\(fl 0x00\)\(ty\s+20\)\(scl\s+\d+\) \(nx \d\) 0x([0-9a-f]+) (\S+)')
with open(symf) as f:
    for line in f:
        m = sym_re.search(line)
        if m:
            funcs.append((TEXT_BASE + int(m.group(1), 16), m.group(2)))
funcs.sort()
addrs = sorted(set(a for a, _ in funcs))

names = [n[1:] if n.startswith('__Z') else n for _, n in funcs]
dem = subprocess.run(['c++filt'], input='\n'.join(names),
                     capture_output=True, text=True).stdout.split('\n')

sel = []
for (a, n), d in zip(funcs, dem):
    if pat.search(d):
        i = bisect.bisect_right(addrs, a)
        end = addrs[i] if i < len(addrs) else a + 0x1000
        sel.append((a, end, d))

SECTS = []
for l in subprocess.run(['objdump', '-h', exe], capture_output=True, text=True).stdout.splitlines():
    f = l.split()
    if len(f) >= 7 and f[0].isdigit():
        SECTS.append((int(f[3], 16), int(f[2], 16), int(f[5], 16)))
IMG = open(exe, 'rb').read()
def rd(va, n):
    for vma, size, off in SECTS:
        if vma <= va < vma + size:
            o = off + (va - vma)
            return IMG[o:o + n]
    return None
def strat(va):
    b = rd(va, 400)
    if not b:
        return None
    # Qt5 static QArrayData header: ref=-1, size, alloc, offset
    if len(b) >= 16:
        ref, size, alloc, off = struct.unpack('<iiIi', b[:16])
        if ref == -1 and 0 <= size < 180 and off in (12, 16, 20, 24):
            raw = rd(va + off, size * 2)
            if raw and len(raw) == size * 2:
                try:
                    return 'QStr:"' + raw.decode('utf-16-le') + '"'
                except Exception:
                    pass
    end = b.find(b'\0')
    if end >= 1:
        s = b[:end]
        try:
            t = s.decode('utf-8')
            if all(c.isprintable() or c in '\n\t' for c in t):
                return '"' + t + '"'
        except Exception:
            pass
    return None
imm_re = re.compile(r'0x([0-9a-f]{6,7})\b')
FSTART = {}
for (fa, fn), fd in zip(funcs, dem):
    FSTART.setdefault(fa, fd)
def annotate(line):
    notes = []
    for m in imm_re.finditer(line):
        v = int(m.group(1), 16)
        if v in FSTART and 'call' not in line and 'j' != line.split('\t')[-1][:1]:
            notes.append('&' + FSTART[v][:110])
        elif 0x110e000 <= v < 0x15eb000:
            s = strat(v)
            if s:
                notes.append(s)
    return line + ('    ; ' + ' | '.join(notes) if notes else '')

seen = set()
for a, end, d in sel:
    if a in seen:
        continue
    seen.add(a)
    print(f"\n==================== {d}  [{a:#x}-{end:#x}]")
    args = ['objdump', '-d', '-C', '-M', 'intel', '--no-show-raw-insn',
            f'--start-address={a:#x}', f'--stop-address={end:#x}', exe]
    if '--lines' in sys.argv:
        args.insert(2, '-l')
    out = subprocess.run(args, capture_output=True, text=True).stdout
    body = out.split('>:\n', 1)[-1] if '>:\n' in out else out
    for line in body.splitlines():
        line = line.rstrip()
        if not line or line.startswith('Disassembly'):
            continue
        print(annotate(line))
