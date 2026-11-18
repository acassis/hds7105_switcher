#!/usr/bin/env python3
"""
timesync.py - set the clock of DeviceWell HDS7105/HDS7106 switchers from
the command line (e.g. from cron), without the GUI.

Sends the frame of DeviceWell's "Time synchronization V1.1.exe" (see
PROTOCOL.md, "Time sync") from UDP port 7000 and prints every reply; a
switcher answers with its discovery reply.

  A5 6C 1B 00 81 FF 01 00x9 | FF YY MM DD WD hh mm ss | sumL sumH AE
  (packed BCD, YY = year % 100, WD = day of week with 0 = Sunday)

usage: timesync.py [--at 'YYYY-MM-DD HH:MM:SS'] [--via IFNAME --src IP] DEST...
       timesync.py --selftest

  DEST     switcher IP, or a broadcast address (255.255.255.255 is what the
           Windows tool uses: every switcher on the LAN takes the time)
  --at     send this local time instead of the PC clock
  --via    send out of interface IFNAME with source address --src
           (IP_PKTINFO), e.g. a broadcast over a direct cable whose
           interface has another subnet; the switcher only answers
           sources in its own subnet
Exit status 0 if at least one switcher answered.
"""
import datetime
import socket
import struct
import sys
import time

PORT = 7000
IP_PKTINFO = 8


def bcd(v):
    return (v // 10) << 4 | (v % 10)


def frame(t):
    """Same bytes as the original's builder at 0x401000 (GetLocalTime)."""
    dow = (t.weekday() + 1) % 7                 # Monday=0 -> Sunday=0
    body = [0xA5, 0x6C, 0x1B, 0x00, 0x81, 0xFF, 0x01] + [0] * 9 + [
        0xFF, bcd(t.year % 100), bcd(t.month), bcd(t.day), bcd(dow),
        bcd(t.hour), bcd(t.minute), bcd(min(t.second, 59))]
    s = sum(body) & 0xFFFF
    return bytes(body + [s & 0xFF, s >> 8, 0xAE])


def selftest():
    # Accepted by a real HDS7105(2021) on 2026-11-18 (Wednesday)
    want = bytes.fromhex("a56c1b0081ff01000000000000000000"
                         "ff261118031516365f04ae")
    got = frame(datetime.datetime(2026, 11, 18, 15, 16, 36))
    assert got == want, got.hex(" ")
    print("selftest ok:", got.hex(" "))


def main(argv):
    if argv[:1] == ["--selftest"]:
        selftest()
        return 0
    at, via, src, dests = None, None, "0.0.0.0", []
    try:
        while argv:
            a = argv.pop(0)
            if a == "--at":
                at = datetime.datetime.strptime(argv.pop(0), "%Y-%m-%d %H:%M:%S")
            elif a == "--via":
                via = argv.pop(0)
            elif a == "--src":
                src = argv.pop(0)
            elif a.startswith("-"):
                raise ValueError(a)
            else:
                dests.append(a)
    except (IndexError, ValueError):
        dests = []
    if not dests:
        sys.exit(__doc__)

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("0.0.0.0", PORT))                   # like the original tool
    t = at or datetime.datetime.now()
    f = frame(t)
    for d in dests:
        if via:
            info = struct.pack("I4s4s", socket.if_nametoindex(via),
                               socket.inet_aton(src), b"\0\0\0\0")
            s.sendmsg([f], [(socket.IPPROTO_IP, IP_PKTINFO, info)], 0, (d, PORT))
        else:
            s.sendto(f, (d, PORT))
        print("TX %s:%d %s  %s" % (d, PORT, t.strftime("%Y-%m-%d %a %H:%M:%S"),
                                   f.hex(" ")))
    answered = 0
    end = time.time() + 2.0
    while time.time() < end:
        s.settimeout(max(0.05, end - time.time()))
        try:
            data, sa = s.recvfrom(4096)
        except socket.timeout:
            break
        if data == f:
            continue                            # our own broadcast
        if len(data) > 16 and data[:2] == b"\xa5\x6c" and data[16] == 0xFF:
            answered += 1
            name = data[18:-6].split(b"\0")[0].decode("ascii", "replace")
            print("RX %s:%d answered (%s)" % (sa[0], sa[1], name))
        else:
            print("RX %s:%d [%d] %s" % (sa[0], sa[1], len(data), data.hex(" ")))
    if not answered:
        print("no switcher answered", file=sys.stderr)
    return 0 if answered else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
