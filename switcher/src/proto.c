/*
 * proto.c - frame builders/parsers for the SWITCHER UDP protocol.
 * Addresses in comments refer to functions in the original SWITCHER.exe.
 */

#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* SWITCHER_DATA::CalcCheckSum @0x8231c8 */
uint16_t sw_checksum(const uint8_t *buf, size_t n)
{
    uint16_t s = 0;
    while (n--)
        s = (uint16_t)(s + *buf++);
    return s;
}

/* Common 16-byte header.  Returns pointer to the payload. */
static uint8_t *put_header(uint8_t *p, uint16_t len, uint8_t type, uint8_t id)
{
    *p++ = SW_MAGIC0;
    *p++ = SW_MAGIC1;
    *p++ = (uint8_t)(len & 0xFF);
    *p++ = (uint8_t)(len >> 8);
    *p++ = type;
    *p++ = id;
    *p++ = 0x01;
    memset(p, 0, 9);
    return p + 9;
}

/* Trailer: checksum over everything before it, then AE.
 * The original always sums `len - 3` bytes from the frame start. */
static size_t put_trailer(uint8_t *frame, uint8_t *p, uint16_t len)
{
    uint16_t sum = sw_checksum(frame, (size_t)len - 3);
    *p++ = (uint8_t)(sum & 0xFF);
    *p++ = (uint8_t)(sum >> 8);
    *p++ = SW_END;
    (void)p;
    return len;
}

/* SWITCHER_DATA::None_Char_Data @0x8237c0 (len 0x14) */
size_t sw_build_none(uint8_t *out, uint8_t type, uint8_t id, uint8_t op)
{
    uint8_t *p = put_header(out, 20, type, id);
    *p++ = op;
    return put_trailer(out, p, 20);
}

/* SWITCHER_DATA::Single_Char_Data @0x823da8 (len 0x15) */
size_t sw_build_single(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                       uint8_t v)
{
    uint8_t *p = put_header(out, 21, type, id);
    *p++ = op;
    *p++ = v;
    return put_trailer(out, p, 21);
}

/* SWITCHER_DATA::Double_Char_Data @0x823af4 (len 0x16) */
size_t sw_build_double(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                       uint8_t a, uint8_t b)
{
    uint8_t *p = put_header(out, 22, type, id);
    *p++ = op;
    *p++ = a;
    *p++ = b;
    return put_trailer(out, p, 22);
}

/* SWITCHER_DATA::SendBroadcastBuf @0x823c74: type 81, id FF, payload FF */
size_t sw_build_search(uint8_t *out)
{
    uint8_t *p = put_header(out, 20, SW_TYPE_SEARCH, SW_ID_ANY);
    *p++ = 0xFF;
    return put_trailer(out, p, 20);
}

/* Two decimal digits as packed BCD (0..99). */
static uint8_t bcd2(int v)
{
    v = ((v % 100) + 100) % 100;
    return (uint8_t)((v / 10) << 4 | (v % 10));
}

/* "Time synchronization V1.1.exe" @0x401000, called from its SYNC handler
 * @0x4040d0: GetLocalTime(), then type 81 / id FF / FF + 7 BCD bytes. */
size_t sw_build_time(uint8_t *out, const struct tm *t)
{
    uint8_t *p = put_header(out, SW_TIME_FRAME_LEN, SW_TYPE_SEARCH, SW_ID_ANY);

    *p++ = 0xFF;
    *p++ = bcd2(t->tm_year + 1900);
    *p++ = bcd2(t->tm_mon + 1);
    *p++ = bcd2(t->tm_mday);
    *p++ = bcd2(t->tm_wday);
    *p++ = bcd2(t->tm_hour);
    *p++ = bcd2(t->tm_min);
    *p++ = bcd2(t->tm_sec > 59 ? 59 : t->tm_sec);   /* SYSTEMTIME has no :60 */
    return put_trailer(out, p, SW_TIME_FRAME_LEN);
}

/* SWITCHER_DATA::Set_ipCom @0x82412c (len 0x20).
 *
 * Byte 6 is not the usual 0x01: it is 0x00 for type A5 and 0x01 for type
 * E0.  For any other type (e.g. an E5 streaming module) the original wrote
 * no byte at all, so everything after shifts down by one while the length
 * field still says 32 and the last byte on the wire is a leftover 0x00.
 * Ported literally: the device firmware was built against this tool. */
size_t sw_build_ipcom(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                      const uint8_t ip[4], const uint8_t mask[4],
                      const uint8_t gw[4])
{
    const uint16_t len = 32;
    uint8_t *p = out;

    memset(out, 0, len);
    *p++ = SW_MAGIC0;
    *p++ = SW_MAGIC1;
    *p++ = (uint8_t)(len & 0xFF);
    *p++ = (uint8_t)(len >> 8);
    *p++ = type;
    *p++ = id;
    if (type == SW_TYPE_SWITCHER)
        *p++ = 0x00;
    else if (type == SW_TYPE_HIS_E0)
        *p++ = 0x01;
    p += 9;                         /* nine zero bytes */
    *p++ = op;
    memcpy(p, ip, 4);   p += 4;
    memcpy(p, mask, 4); p += 4;
    memcpy(p, gw, 4);   p += 4;
    put_trailer(out, p, len);
    return len;
}

/* HDS7105_workdata::CreateMcuUdpDataBuf_ipDns @0x8d2cfc (len 0x1C).
 * The trailing 02 50 3B 8C is a constant in the original. */
size_t sw_build_dns(uint8_t *out, uint8_t type, uint8_t id,
                    const uint8_t dns[4])
{
    uint8_t *p = put_header(out, 28, type, id);
    *p++ = 0x23;
    memcpy(p, dns, 4);
    p += 4;
    *p++ = 0x02;
    *p++ = 0x50;
    *p++ = 0x3B;
    *p++ = 0x8C;
    return put_trailer(out, p, 28);
}

/* HDS7105_workdata::SendCMD_Set_Url @0x8d1168 +
 * CreateMcuUdpDataBuf_url @0x8d26ac (len 0x329): serialised ST_RTMP_URL
 *   67 00 index name_len name[16] url_lenL url_lenH url[768]
 * Lengths are clamped to the struct fields (the original did not). */
size_t sw_build_url(uint8_t *out, uint8_t type, uint8_t id, uint8_t index,
                    const char *name, size_t name_len,
                    const char *url, size_t url_len)
{
    const uint16_t len = SW_URL_FRAME_LEN;
    uint8_t *p = put_header(out, len, type, id);

    if (name_len > SW_URL_NAME_MAX)
        name_len = SW_URL_NAME_MAX;
    if (url_len > SW_URL_MAX)
        url_len = SW_URL_MAX;

    *p++ = 0x67;
    *p++ = 0x00;
    *p++ = index;
    *p++ = (uint8_t)name_len;
    memset(p, 0, SW_URL_NAME_MAX);
    if (name_len)
        memcpy(p, name, name_len);
    p += SW_URL_NAME_MAX;
    *p++ = (uint8_t)(url_len & 0xFF);
    *p++ = (uint8_t)(url_len >> 8);
    memset(p, 0, SW_URL_MAX);
    if (url_len)
        memcpy(p, url, url_len);
    p += SW_URL_MAX;
    return put_trailer(out, p, len);
}

/* Receive: SWITCHER_DATA::MYanalysis @0x822e40 and
 * HDS7105_workdata::Receive7105Data @0x8d0edc, per datagram.
 *
 * The original appended datagrams to a persistent buffer that it never
 * cleared once it exceeded 0x420 bytes (latching dead), and validated the
 * checksum of all frames concatenated.  UDP delivers whole datagrams, so
 * each one is parsed on its own and every frame is checked individually. */
int sw_parse_datagram(const uint8_t *buf, size_t n, sw_frame_cb cb, void *user)
{
    int found = 0;
    size_t i = 0;

    if (n < 20 || n > SW_MAX_FRAME)
        return 0;

    while (i + 3 < n) {
        if (buf[i] == SW_MAGIC0 && buf[i + 1] == SW_MAGIC1) {
            size_t len = (size_t)buf[i + 2] | ((size_t)buf[i + 3] << 8);
            if (len >= 20 && i + len <= n && buf[i + len - 1] == SW_END) {
                const uint8_t *f = buf + i;
                uint16_t want = (uint16_t)(f[len - 3] | (f[len - 2] << 8));
                if (sw_checksum(f, len - 3) == want) {
                    if (cb)
                        cb(user, f, len);
                    found++;
                    i += len;
                    continue;
                }
            }
        }
        i++;
    }
    return found;
}

const uint8_t *sw_slice_after(const uint8_t *frame, size_t len, uint8_t sep,
                              int n, size_t from, size_t *out_len)
{
    size_t end = len >= 3 ? len - 3 : 0;   /* stop before checksum */
    size_t i;

    *out_len = 0;
    for (i = from; i < end; i++) {
        if (frame[i] == sep && --n == 0) {
            *out_len = end - (i + 1);
            return frame + i + 1;
        }
    }
    return NULL;
}

void sw_hex(char *dst, size_t cap, const uint8_t *buf, size_t n)
{
    size_t o = 0, i;

    if (!cap)
        return;
    dst[0] = 0;
    for (i = 0; i < n && o + 4 < cap; i++)
        o += (size_t)snprintf(dst + o, cap - o, i ? " %02X" : "%02X", buf[i]);
    if (i < n && o + 4 < cap)
        snprintf(dst + o, cap - o, " ..");
}
