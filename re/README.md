# Reverse-engineering helpers

These scripts were used to recover the protocol in
[`../PROTOCOL.md`](../PROTOCOL.md) from DeviceWell's `SWITCHER.exe`. The
binary is a 32-bit MinGW Qt 5 **debug** build with full DWARF info, so
symbols, class layouts and the vendor's opcode enums are all available.
The binary itself is not in this repository.

They need only `objdump`, `c++filt` and Python 3.

```sh
objdump -t SWITCHER.exe > syms.txt
objdump --dwarf=info SWITCHER.exe > all.dwarf.txt      # ~250 MB

# disassemble every method of a class, with strings and slot targets resolved
re/dis.py SWITCHER.exe syms.txt '^HDS7105_workdata::' > h5.txt
re/slim.py h5.txt 'SendCMD'                             # just the interesting lines

# read a switch jump table (7 dwords) or raw bytes at a VMA
re/rd.py SWITCHER.exe 112297c 7
re/rd.py SWITCHER.exe 1544600 8 b

# class / struct / enum layouts from DWARF
re/dwarf_types.py all.dwarf.txt '^(SWITCHER_DATA|ST_RTMP_URL|DW_SWITCHER_7105CMD)$'
```

| Script | Purpose |
|---|---|
| `dis.py` | `objdump` disassembly of functions selected by demangled name. Each line is annotated with the C string or Qt `QStringLiteral` that an immediate points at, and with the function an immediate names. That resolves `QObject::connect()` slot targets. |
| `slim.py` | Reduces a `dis.py` dump to calls, compares, branches, constants and strings. |
| `rd.py` | Dumps dwords or bytes at a virtual address, for jump tables and constant tables. |
| `dwarf_types.py` | Parses `objdump --dwarf=info` output and prints structs, classes and enums with member offsets. |

Address arithmetic: `.text` starts at VMA 0x401000, and a COFF symbol's
value plus 0x401000 gives its VMA.
