#!/usr/bin/env python3
"""
fakedev.py - simulated HDS7105/HDS7106 switcher (+ optional streaming
"HIS" module) for testing the SWITCHER client without hardware.

Implemented from PROTOCOL.md, independently of the C sources.

  python3 tools/fakedev.py --ip 127.0.0.2 --his-ip 127.0.0.3
  ./build/switcher --probe 127.0.0.2         # unicast discovery

Use loopback addresses other than 127.0.0.1 so the device sockets can
share UDP port 7000 with the client on the same machine.  --broadcast
additionally answers broadcast searches (binds 255.255.255.255:7000).
"""
import argparse
import datetime
import json
import select
import socket
import sys
import time

PORT = 7000

NAMES = {
    0x05: "SET_IP", 0x23: "HIS_DNS", 0x24: "HIS_READ_IP", 0x49: "ALL_DATA",
    0x67: "HIS_SET_URL", 0x6A: "HIS_READ_URL", 0x80: "PGM", 0x81: "PVW",
    0x82: "SLIDER", 0x83: "CUT", 0x84: "AUTO", 0x85: "FTB", 0x86: "PIP",
    0x8D: "MIX_TRANS", 0x8E: "FADE", 0x8F: "SET_WIPE", 0x92: "WIPE_STYLE",
    0x9D: "PIP_BORDER_EN", 0x9E: "PIP_BORDER_COLOR", 0x9F: "PIP_BORDER_W",
    0xA0: "PIP_X", 0xA1: "PIP_Y", 0xA2: "PIP_SIZE", 0xA3: "PGM_OUT",
    0xA7: "POP", 0xA8: "POP_WINDOW", 0xA9: "POP_BORDER_EN",
    0xAA: "POP_BORDER_COLOR", 0xAB: "POP_BORDER_W", 0xAC: "POP_X", 0xAD: "POP_Y",
    0xB1: "RATE", 0xB8: "CH_VOL", 0xB9: "SRC1", 0xBA: "SRC2", 0xCB: "MIX_BTN",
    0xCC: "AFV_BTN", 0xCD: "STILL", 0xCE: "CH1VOL", 0xCF: "CH2VOL",
    0xD0: "F2_VIDEO1", 0xD4: "F3_MUTE", 0xD6: "F7_OSD", 0xFF: "SEARCH",
}


def frame(ftype, payload, model=0):
    """A5 6C len16 type FF 01 <9 zero> payload sum16 AE (byte 14 = model)."""
    length = 16 + len(payload) + 3
    hdr = bytearray([0xA5, 0x6C, length & 0xFF, length >> 8, ftype, 0xFF, 0x01] + [0] * 9)
    hdr[14] = model
    body = bytes(hdr) + bytes(payload)
    s = sum(body) & 0xFFFF
    return body + bytes([s & 0xFF, s >> 8, 0xAE])


def frames_in(data):
    i = 0
    while i + 3 < len(data):
        if data[i] == 0xA5 and data[i + 1] == 0x6C:
            n = data[i + 2] | data[i + 3] << 8
            if n >= 20 and i + n <= len(data) and data[i + n - 1] == 0xAE:
                f = data[i:i + n]
                if sum(f[:n - 3]) & 0xFFFF == f[n - 3] | f[n - 2] << 8:
                    yield f
                    i += n
                    continue
        i += 1


def bcd(b):
    return (b >> 4) * 10 + (b & 15)


def decode_time(f):
    """27-byte time-sync frame (81 FF, FF + YY MM DD WD hh mm ss in BCD):
    return (text, ok) where ok means valid BCD within 5 s of our clock."""
    yy, mo, dd, wd, hh, mi, ss = f[17:24]
    if any((b & 15) > 9 or (b >> 4) > 9 for b in f[17:24]):
        return "bad BCD " + f[17:24].hex(" "), False
    try:
        t = datetime.datetime(2000 + bcd(yy), bcd(mo), bcd(dd), bcd(hh), bcd(mi), bcd(ss))
    except ValueError:
        return "bad date " + f[17:24].hex(" "), False
    ok = (t.weekday() + 1) % 7 == bcd(wd) and \
        abs((datetime.datetime.now() - t).total_seconds()) < 5
    return t.strftime("%Y-%m-%d %a %H:%M:%S"), ok


def ip4(s):
    return bytes(int(x) for x in s.split("."))


class Device:
    def __init__(self, a):
        self.a = a
        self.sign = bytes([0x5A, 0x12, 0x34])
        self.model = a.model
        self.st = dict(pgm=1, pvw=2, audio=1, ch1=7, vol1=50, ch2=3, vol2=40,
                       trans=1, wipe=0, ftb=0, pip=0, ben=0, bcol=0, bw=2,
                       size=0, h=0, v=0, still=0, rate=1, pgmout=0x07, mute=0)
        self.his = dict(ip=a.his_ip or "0.0.0.0", mask="255.255.255.0",
                        gw="192.168.1.1", dns="8.8.8.8")
        self.urls = {i: ("", "") for i in range(8)}
        self.urls[0] = ("main", "rtmp://live.example.com/app/streamkey")
        self.rec = open(a.record, "a") if a.record else None
        self.wipe_pending = False

    def log(self, who, f, src):
        op = f[16] if len(f) > 16 else None
        if f[4] == 0x81:
            op = 0xFF
        args = list(f[17:len(f) - 3])
        entry = dict(t=round(time.time(), 3), to=who, src="%s:%d" % src,
                     type=f[4], op=op, name=NAMES.get(op, "?"), args=args[:8],
                     len=len(f), hex=f.hex(" "))
        extra = ""
        if f[4] == 0x81 and len(f) == 27:
            entry["name"] = "TIME_SYNC"
            entry["time"], entry["time_ok"] = decode_time(f)
            extra = "  -> %s%s" % (entry["time"], "" if entry["time_ok"] else " (WRONG)")
        print("%-4s %-15s %-16s args=%s%s" % (who, entry["src"], entry["name"],
                                              " ".join("%02X" % x for x in args[:8]),
                                              extra),
              flush=True)
        if self.rec:
            self.rec.write(json.dumps(entry) + "\n")
            self.rec.flush()

    def status_bytes(self):
        s = self.st
        b = [s["pgm"], s["pvw"], s["audio"], s["ch1"], s["vol1"], s["ch2"],
             s["vol2"], s["trans"], s["wipe"], s["ftb"], s["pip"], s["ben"],
             s["bcol"], s["bw"], s["size"], s["h"] & 255, s["h"] >> 8,
             s["v"] & 255, s["v"] >> 8, s["still"], s["rate"], s["pgmout"],
             0, 0, s["mute"]]
        return bytes(b)

    def on_switcher(self, f):
        """Apply a command; return a reply frame or None."""
        s, op = self.st, f[16]
        a = f[17] if len(f) > 20 else 0
        b = f[18] if len(f) > 21 else 0
        if op == 0x49:
            return frame(0xA5, b"I#HDS#" + self.status_bytes())
        if op == 0x80: s["pgm"] = a
        elif op == 0x81: s["pvw"] = a
        elif op in (0x83, 0x84): s["pgm"], s["pvw"] = s["pvw"], s["pgm"]
        elif op == 0x85: s["ftb"] ^= 1
        elif op == 0x86: s["pip"] = a
        elif op == 0x8D: s["trans"] = 1
        elif op == 0x8E: s["trans"] = 2
        elif op == 0x8F: self.wipe_pending = True
        elif op == 0x92: s["trans"], s["wipe"] = 4, a - 0x23
        elif op == 0x9D: s["ben"] = a
        elif op == 0x9E: s["bcol"] = a
        elif op == 0x9F: s["bw"] = a
        elif op == 0xA0: s["h"] = a | b << 8
        elif op == 0xA1: s["v"] = a | b << 8
        elif op == 0xA2: s["size"] = a
        elif op == 0xA3: s["pgmout"] = a
        elif op == 0xB1: s["rate"] = a
        elif op == 0xB9: s["ch1"] = a
        elif op == 0xBA: s["ch2"] = a
        elif op == 0xCB: s["audio"] = 1      # MIX button
        elif op == 0xCC: s["audio"] = 0      # AFV button
        elif op == 0xCD: s["still"] ^= 1
        elif op == 0xCE: s["vol1"] = a
        elif op == 0xCF: s["vol2"] = a
        elif op == 0xD4: s["mute"] ^= 0xFF
        return None

    def on_his(self, f):
        op = f[16] if f[6] in (0, 1) else f[15]   # E5 frames lack byte 6
        h = self.his
        if op == 0x24:
            body = bytes([0x24, 0, 0]) + ip4(h["ip"]) + ip4(h["mask"]) + \
                ip4(h["gw"]) + bytes(6) + ip4(h["dns"])
            return frame(0xE0, body)
        if op == 0x6A:
            idx = f[17] & 7
            name, url = self.urls[idx]
            nb = name.encode()[:16]
            ub = url.encode()[:768]
            body = bytes([0x6A, 0, idx, len(nb)]) + nb.ljust(16, b"\0") + \
                bytes([len(ub) & 255, len(ub) >> 8]) + ub
            return frame(0xE0, body)
        if op == 0x67:
            idx, nl = f[18], f[19]
            name = f[20:20 + nl].decode(errors="replace")
            ul = f[36] | f[37] << 8
            url = f[38:38 + ul].decode(errors="replace")
            self.urls[idx & 7] = (name, url)
        if op == 0x23:
            h["dns"] = ".".join(str(x) for x in f[17:21])
        if op == 0x05:
            h["ip"] = ".".join(str(x) for x in f[17:21])
            h["mask"] = ".".join(str(x) for x in f[21:25])
            h["gw"] = ".".join(str(x) for x in f[25:29])
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", default="127.0.0.2")
    ap.add_argument("--his-ip", default=None)
    ap.add_argument("--model", type=lambda x: int(x, 0), default=0x21)
    ap.add_argument("--broadcast", action="store_true")
    ap.add_argument("--record", help="append received frames as JSON lines")
    a = ap.parse_args()

    dev = Device(a)

    def sock(addr):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.bind((addr, PORT))
        return s

    sw = sock(a.ip)
    his = sock(a.his_ip) if a.his_ip else None
    bc = sock("255.255.255.255") if a.broadcast else None
    socks = [x for x in (sw, his, bc) if x]
    print("fake model 0x%02X on %s%s%s" % (a.model, a.ip,
          (" + HIS on " + a.his_ip) if his else "",
          " + broadcast" if bc else ""), flush=True)

    while True:
        r, _, _ = select.select(socks, [], [])
        for s in r:
            data, src = s.recvfrom(4096)
            for f in frames_in(data):
                who = "SW" if s is sw else "HIS" if s is his else "BC"
                dev.log(who, f, src)
                if f[4] == 0x81:                       # discovery / time sync
                    sw.sendto(frame(0xA5, b"\xff" + dev.sign, dev.model), src)
                    if his:
                        his.sendto(frame(0xE0, b"\xff" + dev.sign), src)
                    continue
                reply = dev.on_his(f) if s is his else dev.on_switcher(f)
                if reply:
                    s.sendto(reply, src)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
