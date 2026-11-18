/*
 * proto.h - HDS7105/HDS7106 "SWITCHER" UDP frame format.
 *
 * Reverse engineered from SWITCHER.exe (see ../../PROTOCOL.md for the
 * per-field evidence).  Pure C, no GUI or socket dependencies.
 *
 * Frame layout (both directions):
 *
 *   0   A5 6C           magic
 *   2   len16 (LE)      total frame length, magic and trailer included
 *   4   type            device type (A5 switcher, E0/E5 streaming "HIS" module, 81 search)
 *   5   id              device id (FF in every reply seen by the original tool)
 *   6   01              (see sw_build_ipcom() for the one exception)
 *   7   00 x 9          reserved; replies put the model ("child type") at 14
 *   16  payload         opcode [+ args]
 *   L-3 sum16 (LE)      16-bit byte sum of frame[0 .. L-4]
 *   L-1 AE              end marker
 */

#ifndef SWITCHER_PROTO_H
#define SWITCHER_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct tm;

#define SW_PORT          7000
#define SW_MAGIC0        0xA5
#define SW_MAGIC1        0x6C
#define SW_END           0xAE
#define SW_HDR_LEN       16       /* payload (opcode) starts here           */
#define SW_MAX_FRAME     0x420    /* receive limit used by the original     */
#define SW_URL_FRAME_LEN 809      /* CreateMcuUdpDataBuf_url                */
#define SW_TIME_FRAME_LEN 27      /* "Time synchronization V1.1.exe" 0x401000 */

/* Device types (frame byte 4) */
#define SW_TYPE_SWITCHER 0xA5
#define SW_TYPE_HIS_E0   0xE0
#define SW_TYPE_HIS_E5   0xE5
#define SW_TYPE_SEARCH   0x81
#define SW_ID_ANY        0xFF

/* Model codes ("child type", discovery reply byte 14) */
#define SW_MODEL_HDS7105      0x21
#define SW_MODEL_HDS7106      0x22
#define SW_MODEL_HDS7105S     0x24
#define SW_MODEL_HDS7105_2021 0x25

/* Field limits of ST_RTMP_URL */
#define SW_URL_NAME_MAX 16
#define SW_URL_MAX      768

uint16_t sw_checksum(const uint8_t *buf, size_t n);

/* Frame builders.  `out` must hold at least the returned length
 * (SW_URL_FRAME_LEN for sw_build_url, 32 bytes for everything else).
 * Each returns the number of bytes to put on the wire. */
size_t sw_build_none(uint8_t *out, uint8_t type, uint8_t id, uint8_t op);
size_t sw_build_single(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                       uint8_t v);
size_t sw_build_double(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                       uint8_t a, uint8_t b);
size_t sw_build_search(uint8_t *out);
size_t sw_build_ipcom(uint8_t *out, uint8_t type, uint8_t id, uint8_t op,
                      const uint8_t ip[4], const uint8_t mask[4],
                      const uint8_t gw[4]);
size_t sw_build_dns(uint8_t *out, uint8_t type, uint8_t id,
                    const uint8_t dns[4]);
size_t sw_build_url(uint8_t *out, uint8_t type, uint8_t id, uint8_t index,
                    const char *name, size_t name_len,
                    const char *url, size_t url_len);

/* Set the device clock.  Not in SWITCHER.exe: this is the frame of
 * DeviceWell's separate "Time synchronization V1.1.exe".  It is the
 * discovery frame (type 81, id FF, opcode FF) followed by the date and
 * time as packed BCD: YY MM DD WD hh mm ss (YY = year % 100, WD = day of
 * the week, 0 = Sunday, as in struct tm).  The switcher answers with its
 * normal discovery reply.  `t` is taken as is; the original sends the PC's
 * local time (GetLocalTime). */
size_t sw_build_time(uint8_t *out, const struct tm *t);

/* Receive side ------------------------------------------------------- */

typedef void (*sw_frame_cb)(void *user, const uint8_t *frame, size_t len);

/* Scan one datagram for complete, checksum-valid frames and call `cb` for
 * each.  Returns the number of valid frames found. */
int sw_parse_datagram(const uint8_t *buf, size_t n, sw_frame_cb cb, void *user);

/* Payload slice that follows the n-th occurrence (1-based) of `sep`,
 * searching from frame offset `from`.  The slice ends before the checksum.
 * Returns NULL (and *out_len = 0) if not found.
 *
 * The original used QByteArray::split(sep)[n] over the whole frame, which
 * truncates at the next `sep` byte and can be thrown off by a separator in
 * the header; this is the same selection without those failure modes. */
const uint8_t *sw_slice_after(const uint8_t *frame, size_t len, uint8_t sep,
                              int n, size_t from, size_t *out_len);

/* Read byte i of a slice, 0 when out of range. */
static inline uint8_t sw_at(const uint8_t *s, size_t len, size_t i)
{
    return (s && i < len) ? s[i] : 0;
}

/* Hex dump helper for logs: writes at most `cap` chars incl. NUL. */
void sw_hex(char *dst, size_t cap, const uint8_t *buf, size_t n);

#endif
