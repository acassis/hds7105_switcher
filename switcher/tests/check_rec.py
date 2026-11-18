#!/usr/bin/env python3
"""
check_rec.py - compare frames recorded by tools/fakedev.py (--record)
with the expected sequence for an end-to-end script.

usage: check_rec.py rec.jsonl expected.txt

expected.txt: one frame per line, "<to> <opcode hex> [arg hex ...]",
e.g. "SW 80 03" or "HIS 24".  Args are a prefix match.  Lines starting
with '#' are comments.
"""
import json
import sys


def main():
    rec = [json.loads(line) for line in open(sys.argv[1])]
    exp = []
    for line in open(sys.argv[2]):
        line = line.split("#")[0].strip()
        if line:
            p = line.split()
            exp.append((p[0], int(p[1], 16), [int(x, 16) for x in p[2:]]))

    ok = True
    n = max(len(rec), len(exp))
    for i in range(n):
        r = rec[i] if i < len(rec) else None
        e = exp[i] if i < len(exp) else None
        got = "%s %02X %s" % (r["to"], r["op"], " ".join("%02X" % x for x in r["args"][:len(e[2]) if e else 3])) if r else "-"
        want = "%s %02X %s" % (e[0], e[1], " ".join("%02X" % x for x in e[2])) if e else "-"
        match = r and e and r["to"] == e[0] and r["op"] == e[1] and r["args"][:len(e[2])] == e[2]
        mark = "ok  " if match else "FAIL"
        if not match:
            ok = False
        print("%s %2d  got %-22s want %-22s %s" % (mark, i, got.strip(), want.strip(),
                                                   r["name"] if r else ""))
    print("\nPASS" if ok else "\nFAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
