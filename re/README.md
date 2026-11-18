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

## Binaries without symbols (Time synchronization V1.1.exe)

DeviceWell's time-sync tool is a stripped MFC program, so `dis.py`
cannot select functions by name. Three helpers handle that case:

| Script | Purpose |
|---|---|
| `pe_dialogs.py` | Dumps the dialog and string-table resources. This gives each control's id, for example **SYNC** = 1000. |
| `mfc_msgmap.py` | Finds MFC message-map entries whose handler is in a code range. A `WM_COMMAND` (0x111) entry with the control id names the button's handler. |
| `pe_annot.py` | Prints an address range of an `objdump -d` listing, annotating immediates that point at C or UTF-16 strings |

The time-sync frame was found as follows:

```sh
objdump -d -M intel tsync.exe > tsync.dis
re/pe_dialogs.py tsync.exe                       # SYNC button: id 1000
re/mfc_msgmap.py tsync.exe 401000 406000         # id 1000 -> handler 0x4040d0
re/pe_annot.py tsync.exe tsync.dis 4040d0 404730 # calls 0x401000, then sendto
re/pe_annot.py tsync.exe tsync.dis 401000 4010ea # the frame builder
```

The Winsock calls are imported by ordinal. To find them, list the import
directory with `objdump -p` and match the `WS2_32` IAT slots against
`call DWORD PTR ds:<slot>`. For example, `sendto` is ordinal 20.
