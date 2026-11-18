#!/usr/bin/env python3
"""Parse `objdump --dwarf=info` text output into a DIE tree and print
selected struct/class/enum types with their members (C-ish form).

usage: dwarf_types.py dwarf.txt 'name-regex' [--methods]
  dwarf.txt: objdump --dwarf=info SWITCHER.exe > dwarf.txt  (~250 MB)"""
import re, sys

die_re = re.compile(r'^\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: (\d+)(?: \((\w+)\))?')
at_re = re.compile(r'^\s*<([0-9a-f]+)>\s+(DW_AT_\w+)\s*:\s*(.*)$')

class Die:
    __slots__ = ('depth', 'off', 'tag', 'attrs', 'children', 'parent')
    def __init__(s, depth, off, tag):
        s.depth, s.off, s.tag, s.attrs, s.children, s.parent = depth, off, tag, {}, [], None

def parse(path):
    dies = {}
    stack = []
    roots = []
    cur = None
    with open(path, errors='replace') as f:
        for line in f:
            m = die_re.match(line)
            if m:
                depth, off, num, tag = int(m.group(1)), int(m.group(2), 16), int(m.group(3)), m.group(4)
                if num == 0:
                    cur = None
                    continue
                d = Die(depth, off, tag)
                dies[off] = d
                while stack and stack[-1].depth >= depth:
                    stack.pop()
                if stack:
                    d.parent = stack[-1]
                    stack[-1].children.append(d)
                else:
                    roots.append(d)
                stack.append(d)
                cur = d
                continue
            m = at_re.match(line)
            if m and cur is not None:
                cur.attrs[m.group(2)] = m.group(3).strip()
    return dies, roots

def name(d):
    n = d.attrs.get('DW_AT_name')
    if n is None:
        return None
    if n.startswith('(indirect string'):
        n = n.split('):', 1)[1].strip()
    return n

def ref(v):
    m = re.search(r'<0x([0-9a-f]+)>', v or '')
    return int(m.group(1), 16) if m else None

def tname(dies, off, depth=0):
    if off is None:
        return 'void'
    d = dies.get(off)
    if d is None or depth > 10:
        return '?'
    t = d.tag
    sub = lambda: tname(dies, ref(d.attrs.get('DW_AT_type')), depth + 1)
    if t in ('DW_TAG_base_type', 'DW_TAG_typedef', 'DW_TAG_structure_type', 'DW_TAG_class_type',
             'DW_TAG_enumeration_type', 'DW_TAG_union_type'):
        return name(d) or ('<anon %x>' % off)
    if t == 'DW_TAG_pointer_type':
        return sub() + '*'
    if t == 'DW_TAG_reference_type':
        return sub() + '&'
    if t == 'DW_TAG_const_type':
        return 'const ' + sub()
    if t == 'DW_TAG_volatile_type':
        return 'volatile ' + sub()
    if t == 'DW_TAG_array_type':
        dims = ''
        for c in d.children:
            if c.tag == 'DW_TAG_subrange_type':
                ub = c.attrs.get('DW_AT_upper_bound')
                cnt = c.attrs.get('DW_AT_count')
                if ub is not None:
                    dims += '[%d]' % (int(ub.split()[0], 0) + 1)
                elif cnt is not None:
                    dims += '[%s]' % cnt
                else:
                    dims += '[]'
        return sub() + dims
    return t

def dump(dies, d, indent=''):
    n = name(d) or '<anon>'
    kind = d.tag.replace('DW_TAG_', '').replace('_type', '')
    sz = d.attrs.get('DW_AT_byte_size', '?')
    print(f"{indent}{kind} {n}  /* size {sz}, die {d.off:#x}, decl_file {d.attrs.get('DW_AT_decl_file','?')} line {d.attrs.get('DW_AT_decl_line','?')} */ {{")
    for c in d.children:
        if c.tag == 'DW_TAG_member':
            loc = c.attrs.get('DW_AT_data_member_location', '')
            bits = ''
            if 'DW_AT_bit_size' in c.attrs:
                bits = ' : ' + c.attrs['DW_AT_bit_size']
            print(f"{indent}    {tname(dies, ref(c.attrs.get('DW_AT_type')))} {name(c)}{bits};  // off {loc}")
        elif c.tag == 'DW_TAG_enumerator':
            print(f"{indent}    {name(c)} = {c.attrs.get('DW_AT_const_value')},")
        elif c.tag == 'DW_TAG_inheritance':
            print(f"{indent}    /* base */ {tname(dies, ref(c.attrs.get('DW_AT_type')))};")
        elif c.tag == 'DW_TAG_subprogram' and '--methods' in sys.argv:
            print(f"{indent}    /* method */ {name(c)}();")
    print(f"{indent}}};")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path = sys.argv[1]
    wanted = re.compile(sys.argv[2])
    dies, roots = parse(path)
    seen = set()
    for d in dies.values():
        if d.tag in ('DW_TAG_structure_type', 'DW_TAG_class_type', 'DW_TAG_enumeration_type', 'DW_TAG_union_type'):
            n = name(d)
            if n and wanted.search(n) and 'DW_AT_declaration' not in d.attrs:
                key = (n, d.attrs.get('DW_AT_byte_size'))
                if key in seen:
                    continue
                seen.add(key)
                dump(dies, d)
                print()
