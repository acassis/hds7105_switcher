/*
 * hds7105.c - HDS7105 command set and reply decoding.
 * Addresses refer to the original SWITCHER.exe.
 */

#include "hds7105.h"
#include "proto.h"

#include <stdio.h>
#include <string.h>

const char *const h5_source_names[H5_NUM_SOURCES] = {
    "PHONE_IN", "IN1", "IN2", "IN3", "IN4", "MIC_IN1", "MIC_IN2"
};
/* SendCMDCH1 @0x8cf2e8 local table; inverse in set_CH1_Style (0x111c2c4) */
const uint8_t h5_source_codes[H5_NUM_SOURCES] = { 7, 3, 4, 5, 6, 8, 0 };

const char *const h5_format_names[H5_NUM_FORMATS] = {
    "1080P60", "1080P50", "1080I60", "1080I50", "1080P30", "1080P25", "1080P24"
};
/* SendCMDF1 @0x8d31c8 local table; inverse in set_F1_Style (0x111edc8) */
const uint8_t h5_format_codes[H5_NUM_FORMATS] = {
    0x07, 0x0B, 0x09, 0x01, 0x03, 0x0D, 0x05
};

int h5_source_index(uint8_t code)
{
    int i;
    for (i = 0; i < H5_NUM_SOURCES; i++)
        if (h5_source_codes[i] == code)
            return i;
    return -1;
}

int h5_format_index(uint8_t code)
{
    int i;
    for (i = 0; i < H5_NUM_FORMATS; i++)
        if (h5_format_codes[i] == code)
            return i;
    return -1;
}

int h5_read_state(sw_session_t *s) { return sw_tx_none(s, H5_ALL_DATA); }
int h5_pgm(sw_session_t *s, uint8_t ch) { return sw_tx_single(s, H5_PGM_CH, ch); }
int h5_pvw(sw_session_t *s, uint8_t ch) { return sw_tx_single(s, H5_PVW_CH, ch); }
int h5_cut(sw_session_t *s) { return sw_tx_none(s, H5_CUT); }
int h5_auto(sw_session_t *s) { return sw_tx_none(s, H5_AUTO); }
int h5_ftb(sw_session_t *s) { return sw_tx_none(s, H5_FTB); }
int h5_rate(sw_session_t *s, uint8_t r) { return sw_tx_single(s, H5_RATE, r); }
int h5_afv(sw_session_t *s) { return sw_tx_none(s, H5_AFV_BTN); }
int h5_mix(sw_session_t *s) { return sw_tx_none(s, H5_MIX_BTN); }
int h5_pip(sw_session_t *s, bool on) { return sw_tx_single(s, H5_PIP, on ? 1 : 0); }
int h5_still(sw_session_t *s) { return sw_tx_none(s, H5_STILL); }
int h5_pip_h(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H5_PIP_X, v); }
int h5_pip_v(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H5_PIP_Y, v); }
int h5_pip_size(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H5_PIP_SIZE, i); }
int h5_pip_border_enable(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H5_PIP_BORDER_ENABLE, i); }
int h5_pip_border_width(sw_session_t *s, uint8_t w) { return sw_tx_single(s, H5_PIP_BORDER_WIDTH, w); }
int h5_pip_border_color(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H5_PIP_BORDER_COLOR, i); }
int h5_f2(sw_session_t *s) { return sw_tx_none(s, H5_VIDEO1_SRC); }
int h5_f3(sw_session_t *s) { return sw_tx_none(s, H5_AUDIO_MUTE); }
int h5_f7(sw_session_t *s) { return sw_tx_none(s, H5_OSD_ROTATING); }
int h5_set_switcher_ip(sw_session_t *s, const uint8_t ip[4]) { return sw_tx_set_ip(s, ip); }

/* SendCMDTRANSITIONS @0x8d1548 (jump table 0x112297c) */
int h5_transition(sw_session_t *s, int tr)
{
    static const uint8_t wipe[] = { 0x25, 0x29, 0x23, 0x27 }; /* U D L R */

    switch (tr) {
    case H5_TR_MIX:
        return sw_tx_none(s, H5_SHOW_MIX);
    case H5_TR_FADE:
        return sw_tx_none(s, H5_SHOW_DIP);
    case H5_TR_UP: case H5_TR_DOWN: case H5_TR_LEFT: case H5_TR_RIGHT:
        if (sw_tx_none(s, H5_SET_WIPE) != 0)          /* SET_WIPE @0x8d3044 */
            return -1;
        return sw_tx_single(s, H5_SHOW_WIPE, wipe[tr - H5_TR_UP]);
    default:
        return -1;
    }
}

int h5_ch_source(sw_session_t *s, int ch, int index)
{
    if (index < 0 || index >= H5_NUM_SOURCES)
        return -1;
    return sw_tx_single(s, ch == 2 ? H5_AUDIO_SRC2 : H5_AUDIO_SRC1,
                        h5_source_codes[index]);
}

int h5_ch_volume(sw_session_t *s, int ch, uint8_t vol)
{
    return sw_tx_single(s, ch == 2 ? H5_CH2VOL : H5_CH1VOL, vol);
}

int h5_pgm_out(sw_session_t *s, int i)
{
    if (i < 0 || i >= H5_NUM_FORMATS)
        return -1;
    return sw_tx_single(s, H5_PGM_OUT, h5_format_codes[i]);
}

void h5_pip_range(int size_idx, int *hmax, int *vmax)
{
    switch (size_idx) {
    case 0:  *hmax = 1440; *vmax = 810; break;  /* Small  */
    case 1:  *hmax = 1280; *vmax = 720; break;  /* Medium */
    case 2:  *hmax = 960;  *vmax = 540; break;  /* Large  */
    default: *hmax = 960;  *vmax = 960; break;  /* init_PIP: both 0..960 */
    }
}

/* ---- streaming module ------------------------------------------- */

static int his_send(sw_session_t *s, const uint8_t *f, size_t n)
{
    if (!s->has_his || s->his_ip == 0)
        return -1;
    return sw_session_send(s, s->his_ip, f, n);
}

/* SendCMD_Read_His_IP @0x8d1cf8 */
int h5_his_read_ip(sw_session_t *s)
{
    static const uint8_t z[4];
    uint8_t f[32];
    return his_send(s, f, sw_build_ipcom(f, s->his_type, s->his_id,
                                         H5_HIS_READ_IP, z, z, z));
}

/* SendCMD_Set_HIS_IP @0x8d18c4 */
int h5_his_set_ip(sw_session_t *s, const uint8_t ip[4], const uint8_t mask[4],
                  const uint8_t gw[4])
{
    uint8_t f[32];
    int r = his_send(s, f, sw_build_ipcom(f, s->his_type, s->his_id,
                                          H5_SET_IP, ip, mask, gw));
    if (r == 0)      /* original then talks to the new address */
        s->his_ip = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
                    ((uint32_t)ip[2] << 8) | ip[3];
    return r;
}

/* SendCMD_Set_HIS_DNS @0x8d1e78 */
int h5_his_set_dns(sw_session_t *s, const uint8_t dns[4])
{
    uint8_t f[32];
    return his_send(s, f, sw_build_dns(f, s->his_type, s->his_id, dns));
}

/* SendCMD_Read_Url @0x8d13e4 */
int h5_his_read_url(sw_session_t *s, uint8_t index)
{
    uint8_t f[32];
    return his_send(s, f, sw_build_single(f, s->his_type, s->his_id,
                                          H5_HIS_READ_URL, index));
}

/* SendCMD_Set_Url @0x8d1168 */
int h5_his_set_url(sw_session_t *s, uint8_t index, const char *name,
                   const char *url)
{
    static uint8_t f[SW_URL_FRAME_LEN];
    size_t n = sw_build_url(f, s->his_type, s->his_id, index,
                            name, strlen(name), url, strlen(url));
    return his_send(s, f, n);
}

/* ---- replies ------------------------------------------------------ */

static void ip_from(char out[16], const uint8_t *d, size_t len, size_t o)
{
    snprintf(out, 16, "%u.%u.%u.%u", sw_at(d, len, o), sw_at(d, len, o + 1),
             sw_at(d, len, o + 2), sw_at(d, len, o + 3));
}

/* Copy n bytes starting at d[o], skipping NULs (on_ToShow_HisUrl). */
static void text_from(char *out, size_t cap, const uint8_t *d, size_t len,
                      size_t o, size_t n)
{
    size_t i, k = 0;
    for (i = 0; i < n && k + 1 < cap; i++) {
        uint8_t c = sw_at(d, len, o + i);
        if (c)
            out[k++] = (char)c;
    }
    out[k] = 0;
}

h5_rx_kind_t h5_decode(const uint8_t *f, size_t len, h5_status_t *st,
                       h5_his_info_t *his, h5_url_info_t *url)
{
    const uint8_t *d;
    size_t n;

    if (len < SW_HDR_LEN + 2)
        return H5_RX_NONE;

    /* Switcher state: A5 FF .. 49 '#' .. '#' <state> */
    if (f[4] == SW_TYPE_SWITCHER && f[5] == SW_ID_ANY &&
        f[SW_HDR_LEN] == H5_ALL_DATA) {
        d = sw_slice_after(f, len, '#', 2, SW_HDR_LEN + 1, &n);
        if (!d)
            return H5_RX_NONE;
        memset(st, 0, sizeof *st);
        st->pgm = sw_at(d, n, 0);
        st->pvw = sw_at(d, n, 1);
        st->audio_mode = sw_at(d, n, 2);
        st->ch1_src = sw_at(d, n, 3);
        st->ch1_vol = sw_at(d, n, 4);
        st->ch2_src = sw_at(d, n, 5);
        st->ch2_vol = sw_at(d, n, 6);
        st->trans = sw_at(d, n, 7);
        st->wipe = sw_at(d, n, 8);
        st->ftb = sw_at(d, n, 9);
        st->pip = sw_at(d, n, 10);
        st->pip_border_enable = sw_at(d, n, 11);
        st->pip_border_color = sw_at(d, n, 12);
        st->pip_border_width = sw_at(d, n, 13);
        st->pip_size = sw_at(d, n, 14);
        st->pip_h = (uint16_t)(sw_at(d, n, 15) | (sw_at(d, n, 16) << 8));
        st->pip_v = (uint16_t)(sw_at(d, n, 17) | (sw_at(d, n, 18) << 8));
        st->still = sw_at(d, n, 19);
        st->rate = sw_at(d, n, 20);
        st->pgm_out = sw_at(d, n, 21);
        st->mute = sw_at(d, n, 24);
        return H5_RX_STATUS;
    }

    if ((f[4] == SW_TYPE_HIS_E0 || f[4] == SW_TYPE_HIS_E5) &&
        f[5] == SW_ID_ANY && f[SW_HDR_LEN + 1] == 0) {
        if (f[SW_HDR_LEN] == H5_HIS_READ_URL) {         /* on_ToShow_HisUrl */
            size_t ul;
            d = sw_slice_after(f, len, H5_HIS_READ_URL, 1, SW_HDR_LEN, &n);
            if (!d)
                return H5_RX_NONE;
            url->index = sw_at(d, n, 1);
            text_from(url->name, sizeof url->name, d, n, 3, sw_at(d, n, 2));
            ul = (size_t)sw_at(d, n, 19) | ((size_t)sw_at(d, n, 20) << 8);
            text_from(url->url, sizeof url->url, d, n, 21, ul);
            return H5_RX_URL;
        }
        if (f[SW_HDR_LEN] == H5_HIS_READ_IP) {          /* on_ToShow_HisInfo */
            d = sw_slice_after(f, len, H5_HIS_READ_IP, 1, SW_HDR_LEN, &n);
            if (!d)
                return H5_RX_NONE;
            ip_from(his->ip, d, n, 2);
            ip_from(his->mask, d, n, 6);
            ip_from(his->gw, d, n, 10);
            ip_from(his->dns, d, n, 20);
            return H5_RX_HIS_INFO;
        }
    }
    return H5_RX_NONE;
}
