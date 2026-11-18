/*
 * test_proto.c - golden vectors for the frame builders and decoders.
 *
 * The expected bytes are derived by hand from the disassembly of
 * SWITCHER.exe (addresses in PROTOCOL.md), not from proto.c.
 */

#include "../src/proto.h"
#include "../src/devices.h"
#include "../src/hds7105.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int fails;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); fails++; } } while (0)

static void expect_bytes(const char *what, const uint8_t *got, size_t n,
                         const uint8_t *want, size_t wn)
{
    if (n != wn || memcmp(got, want, n) != 0) {
        char a[512], b[512];
        sw_hex(a, sizeof a, got, n);
        sw_hex(b, sizeof b, want, wn);
        printf("FAIL %s\n  got  %s\n  want %s\n", what, a, b);
        fails++;
    } else {
        printf("ok   %s\n", what);
    }
}

static void test_builders(void)
{
    uint8_t f[SW_URL_FRAME_LEN];
    size_t n;

    /* SendBroadcastBuf: A5 6C 14 00 81 FF 01 00x9 FF + sum 0x03A5 + AE */
    static const uint8_t search[] = {
        0xA5, 0x6C, 0x14, 0x00, 0x81, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0xA5, 0x03, 0xAE };
    n = sw_build_search(f);
    expect_bytes("search frame", f, n, search, sizeof search);

    /* CUT to a switcher (type A5, id FF): sum = A5+6C+14+A5+FF+01+83 = 0x034D */
    static const uint8_t cut[] = {
        0xA5, 0x6C, 0x14, 0x00, 0xA5, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x83, 0x4D, 0x03, 0xAE };
    n = sw_build_none(f, 0xA5, 0xFF, 0x83);
    expect_bytes("CUT (None_Char_Data)", f, n, cut, sizeof cut);

    /* PGM 2: len 0x15; sum = A5+6C+15+A5+FF+01+80+02 = 0x034D */
    static const uint8_t pgm2[] = {
        0xA5, 0x6C, 0x15, 0x00, 0xA5, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x80, 0x02, 0x4D, 0x03, 0xAE };
    n = sw_build_single(f, 0xA5, 0xFF, 0x80, 0x02);
    expect_bytes("PGM 2 (Single_Char_Data)", f, n, pgm2, sizeof pgm2);

    /* PIP H = 300 (0x012C): len 0x16; sum = A5+6C+16+A5+FF+01+A0+2C+01 = 0x0399 */
    static const uint8_t piph[] = {
        0xA5, 0x6C, 0x16, 0x00, 0xA5, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xA0, 0x2C, 0x01, 0x99, 0x03, 0xAE };
    n = sw_build_double(f, 0xA5, 0xFF, 0xA0, 0x2C, 0x01);
    expect_bytes("PIP H 300 (Double_Char_Data)", f, n, piph, sizeof piph);

    /* Set IP 192.168.1.50 to a switcher: byte 6 = 00 for type A5.
     * sum = A5+6C+20+A5+FF+05+C0+A8+01+32 = 0x0475 */
    {
        static const uint8_t ip[4] = { 192, 168, 1, 50 }, z[4];
        static const uint8_t want[] = {
            0xA5, 0x6C, 0x20, 0x00, 0xA5, 0xFF, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x05, 0xC0, 0xA8, 0x01, 0x32, 0, 0, 0, 0, 0, 0, 0, 0,
            0x75, 0x04, 0xAE };
        n = sw_build_ipcom(f, 0xA5, 0xFF, 0x05, ip, z, z);
        expect_bytes("Set_ipCom type A5", f, n, want, sizeof want);
    }

    /* Read HIS IP for an E0 module: byte 6 = 01; sum = A5+6C+20+E0+FF+01+24 = 0x0335 */
    {
        static const uint8_t z[4];
        static const uint8_t want[] = {
            0xA5, 0x6C, 0x20, 0x00, 0xE0, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x35, 0x03, 0xAE };
        n = sw_build_ipcom(f, 0xE0, 0xFF, 0x24, z, z, z);
        expect_bytes("Set_ipCom type E0", f, n, want, sizeof want);
    }

    /* E5 module: byte 6 omitted, trailer shifted by one, length still 32.
     * sum over 29 bytes = A5+6C+20+E5+FF+24 = 0x0339 */
    {
        static const uint8_t z[4];
        static const uint8_t want[] = {
            0xA5, 0x6C, 0x20, 0x00, 0xE5, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x39, 0x03, 0xAE, 0x00 };
        n = sw_build_ipcom(f, 0xE5, 0xFF, 0x24, z, z, z);
        expect_bytes("Set_ipCom type E5 (shifted quirk)", f, n, want, sizeof want);
    }

    /* DNS 8.8.4.4 to E0: len 0x1C; 23 08 08 04 04 02 50 3B 8C.
     * sum = A5+6C+1C+E0+FF+01+23+08+08+04+04+02+50+3B+8C = 0x0461 */
    {
        static const uint8_t dns[4] = { 8, 8, 4, 4 };
        static const uint8_t want[] = {
            0xA5, 0x6C, 0x1C, 0x00, 0xE0, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x23, 0x08, 0x08, 0x04, 0x04, 0x02, 0x50, 0x3B, 0x8C,
            0x61, 0x04, 0xAE };
        n = sw_build_dns(f, 0xE0, 0xFF, dns);
        expect_bytes("HIS DNS frame", f, n, want, sizeof want);
    }

    /* URL frame: fixed 809 bytes, field offsets from ST_RTMP_URL */
    n = sw_build_url(f, 0xE0, 0xFF, 3, "cam", 3, "rtmp://x/y", 10);
    CHECK(n == 809);
    CHECK(f[2] == 0x29 && f[3] == 0x03);
    CHECK(f[16] == 0x67 && f[17] == 0x00 && f[18] == 3 && f[19] == 3);
    CHECK(memcmp(f + 20, "cam", 3) == 0 && f[23] == 0);
    CHECK(f[36] == 10 && f[37] == 0);
    CHECK(memcmp(f + 38, "rtmp://x/y", 10) == 0);
    CHECK(f[808] == 0xAE);
    CHECK((f[806] | (f[807] << 8)) == sw_checksum(f, 806));
    printf("ok   URL frame layout\n");
}

static int frames_seen;
static void count_cb(void *u, const uint8_t *f, size_t n)
{
    (void)u; (void)f; (void)n;
    frames_seen++;
}

static void test_parse(void)
{
    uint8_t f[64];
    size_t n = sw_build_single(f, 0xA5, 0xFF, 0x80, 1);

    frames_seen = 0;
    CHECK(sw_parse_datagram(f, n, count_cb, NULL) == 1 && frames_seen == 1);
    f[17] ^= 1;                                   /* corrupt payload */
    CHECK(sw_parse_datagram(f, n, count_cb, NULL) == 0);
    printf("ok   datagram validation\n");
}

/* Build a frame around an arbitrary payload, like a device reply. */
static size_t reply(uint8_t *out, uint8_t type, uint8_t model,
                    const uint8_t *payload, size_t pl)
{
    size_t len = SW_HDR_LEN + pl + 3;
    uint16_t sum;

    memset(out, 0, len);
    out[0] = 0xA5; out[1] = 0x6C;
    out[2] = (uint8_t)len; out[3] = (uint8_t)(len >> 8);
    out[4] = type; out[5] = 0xFF; out[6] = 1;
    out[14] = model;
    memcpy(out + SW_HDR_LEN, payload, pl);
    sum = sw_checksum(out, len - 3);
    out[len - 3] = (uint8_t)sum;
    out[len - 2] = (uint8_t)(sum >> 8);
    out[len - 1] = 0xAE;
    return len;
}

static void test_discovery(void)
{
    static sw_devlist_t l;
    uint8_t f[128];
    const uint8_t sw_pl[] = { 0xFF, 0x11, 0x22, 0x33 };
    const uint8_t his_pl[] = { 0xFF, 0x11, 0x22, 0x33 };
    size_t n;

    sw_devlist_clear(&l);
    /* HIS reply first, switcher second: still paired */
    n = reply(f, 0xE0, 0, his_pl, sizeof his_pl);
    sw_devlist_handle(&l, 0xC0A80165, f, n);
    n = reply(f, 0xA5, SW_MODEL_HDS7105, sw_pl, sizeof sw_pl);
    CHECK(sw_devlist_handle(&l, 0xC0A80164, f, n));
    CHECK(l.count == 1);
    CHECK(l.dev[0].model == 0x21 && l.dev[0].sign[0] == 0x11 &&
          l.dev[0].sign[2] == 0x33);
    CHECK(l.dev[0].has_his && l.dev[0].his_ip == 0xC0A80165 &&
          l.dev[0].his_type == 0xE0);
    /* duplicate reply does not add a second entry */
    sw_devlist_handle(&l, 0xC0A80164, f, n);
    CHECK(l.count == 1);
    printf("ok   discovery + HIS pairing\n");
}

static void test_status(void)
{
    uint8_t f[128], pl[64];
    h5_status_t st;
    h5_his_info_t his;
    h5_url_info_t url;
    size_t n, k = 0;

    /* 49 '#' <something> '#' <state...> */
    pl[k++] = 0x49; pl[k++] = '#'; pl[k++] = 'V'; pl[k++] = '1'; pl[k++] = '#';
    {
        const uint8_t s[25] = {
            2, 3, 1, 7, 50, 8, 60, 4, 6, 0, 1, 1, 2, 5, 1,
            0x23, 0x01,          /* H = 0x0123: low byte is '#' on purpose */
            0x2C, 0x01, 1, 2, 0x0B, 0, 0, 0xFF };
        memcpy(pl + k, s, sizeof s);
        k += sizeof s;
    }
    n = reply(f, 0xA5, 0, pl, k);
    CHECK(h5_decode(f, n, &st, &his, &url) == H5_RX_STATUS);
    CHECK(st.pgm == 2 && st.pvw == 3 && st.audio_mode == 1);
    CHECK(st.ch1_src == 7 && st.ch1_vol == 50 && st.ch2_src == 8 && st.ch2_vol == 60);
    CHECK(st.trans == 4 && st.wipe == 6 && st.pip == 1);
    CHECK(st.pip_border_enable == 1 && st.pip_border_color == 2);
    CHECK(st.pip_border_width == 5 && st.pip_size == 1);
    CHECK(st.pip_h == 0x0123 && st.pip_v == 300);
    CHECK(st.still == 1 && st.rate == 2 && st.pgm_out == 0x0B && st.mute == 0xFF);
    printf("ok   7105 status decode\n");

    /* HIS info: 24 00 | d[1] | ip | mask | gw | 6 bytes | dns */
    {
        const uint8_t p[] = { 0x24, 0x00, 0x00, 192, 168, 1, 36, 255, 255, 255, 0,
                              192, 168, 1, 1, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8 };
        n = reply(f, 0xE0, 0, p, sizeof p);
        CHECK(h5_decode(f, n, &st, &his, &url) == H5_RX_HIS_INFO);
        CHECK(strcmp(his.ip, "192.168.1.36") == 0);
        CHECK(strcmp(his.mask, "255.255.255.0") == 0);
        CHECK(strcmp(his.gw, "192.168.1.1") == 0);
        CHECK(strcmp(his.dns, "8.8.8.8") == 0);
        printf("ok   HIS info decode (octet 36 = 0x24 inside data)\n");
    }
}

static int real_frames;
static void keep_cb(void *u, const uint8_t *f, size_t n)
{
    (void)f; (void)n; (void)u;
    real_frames++;
}

/* Frames captured from a real HDS7105(2021) at 192.168.1.7. */
static void test_real_device(void)
{
    static const uint8_t disc[] = {
        0xa5, 0x6c, 0x29, 0x00, 0xa5, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x25, 0x00, 0xff, 0x00, 'H', 'D', 'S', '7', '1', '0', '5',
        '(', '2', '0', '2', '1', ')', '-', '9', '-', 'C', 0x17, 0x60, 0x55,
        0x67, 0x08, 0xae };
    static const uint8_t state[] = {
        0xa5, 0x6c, 0x3e, 0x00, 0xa5, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x25, 0x00, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x39,
        0x42, 0x24, 0x24, 0x24, 0x24, 0x00, 0x03, 0x11, 0x23, 0x02, 0x01, 0x01,
        0x03, 0x64, 0x04, 0x64, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0xa6,
        0x05, 0xae };
    static sw_devlist_t l;
    h5_status_t st;
    h5_his_info_t his;
    h5_url_info_t url;

    real_frames = 0;
    CHECK(sw_parse_datagram(disc, sizeof disc, keep_cb, NULL) == 1);
    CHECK(sw_parse_datagram(state, sizeof state, keep_cb, NULL) == 1);
    CHECK(real_frames == 2);

    sw_devlist_clear(&l);
    CHECK(sw_devlist_handle(&l, 0xC0A80107, disc, sizeof disc));
    CHECK(l.count == 1 && l.dev[0].model == SW_MODEL_HDS7105_2021);
    CHECK(sw_model_is_7105(l.dev[0].model));
    CHECK(l.dev[0].sign[0] == 0x17 && l.dev[0].sign[1] == 0x60 &&
          l.dev[0].sign[2] == 0x55);
    CHECK(!sw_is_status_reply(disc, sizeof disc));
    CHECK(sw_is_status_reply(state, sizeof state));

    CHECK(h5_decode(state, sizeof state, &st, &his, &url) == H5_RX_STATUS);
    CHECK(st.pgm == 2 && st.pvw == 1 && st.audio_mode == 1);
    CHECK(st.ch1_src == 3 && st.ch1_vol == 100);     /* IN1 */
    CHECK(st.ch2_src == 4 && st.ch2_vol == 100);     /* IN2 */
    CHECK(st.trans == 1 && st.ftb == 0 && st.pip == 0);
    CHECK(st.pip_border_enable == 1 && st.pip_border_color == 0);
    CHECK(st.pip_border_width == 2 && st.pip_size == 0);
    CHECK(st.pip_h == 0 && st.pip_v == 0 && st.still == 0 && st.rate == 0);
    CHECK(st.pgm_out == 0x03 && h5_format_index(st.pgm_out) == 4); /* 1080P30 */
    CHECK(st.mute == 0);
    printf("ok   real HDS7105(2021) discovery + state frames\n");
}

/* Time sync (Time synchronization V1.1.exe @0x401000).  The first vector
 * is the frame an HDS7105(2021) accepted on 2026-11-18 (the user saw its
 * date and time update); the second checks BCD digits, Sunday = 0 and :60. */
static void test_time(void)
{
    static const uint8_t real[] = {
        0xA5, 0x6C, 0x1B, 0x00, 0x81, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0x26, 0x11, 0x18, 0x03, 0x15, 0x16, 0x36, 0x5F, 0x04, 0xAE };
    static const uint8_t sun[] = {
        0xA5, 0x6C, 0x1B, 0x00, 0x81, 0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0x00, 0x01, 0x02, 0x00, 0x09, 0x08, 0x59, 0x19, 0x04, 0xAE };
    struct tm t;
    uint8_t f[32];
    size_t n;

    memset(&t, 0, sizeof t);              /* Wed 2026-11-18 15:16:36 */
    t.tm_year = 126; t.tm_mon = 10; t.tm_mday = 18; t.tm_wday = 3;
    t.tm_hour = 15; t.tm_min = 16; t.tm_sec = 36;
    n = sw_build_time(f, &t);
    expect_bytes("time sync 2026-11-18 Wed 15:16:36 (real device)", f, n,
                 real, sizeof real);

    memset(&t, 0, sizeof t);              /* Sun 2000-01-02 09:08:60 */
    t.tm_year = 100; t.tm_mon = 0; t.tm_mday = 2; t.tm_wday = 0;
    t.tm_hour = 9; t.tm_min = 8; t.tm_sec = 60;
    n = sw_build_time(f, &t);
    expect_bytes("time sync 2000-01-02 Sun 09:08:60 -> :59", f, n,
                 sun, sizeof sun);
}

int main(void)
{
    test_builders();
    test_parse();
    test_discovery();
    test_status();
    test_real_device();
    test_time();
    printf(fails ? "\n%d FAILURE(S)\n" : "\nall tests passed\n", fails);
    return fails ? 1 : 0;
}
