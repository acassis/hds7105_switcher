/*
 * hds7106.c - HDS7106 command set (HDS7106_workdata in the original).
 */

#include "hds7106.h"
#include "hds7105.h"

const char *const h6_source_names[H6_NUM_SOURCES] = {
    "Phone_IN", "RCA_IN", "IN1", "IN2", "IN3", "IN4", "IN5", "IN6"
};
/* global CH_NB @0x1544600 */
const uint8_t h6_source_codes[H6_NUM_SOURCES] = { 0, 1, 3, 4, 5, 6, 7, 8 };

const char *const h6_format_names[H6_NUM_FORMATS] = {
    "1080p60", "1080p50", "1080I60", "1080I50", "1080p30", "1080p25", "1080p24"
};

int h6_pgm(sw_session_t *s, uint8_t ch) { return sw_tx_single(s, H6_SET_PGM_CH, ch); }
int h6_pvw(sw_session_t *s, uint8_t ch) { return sw_tx_single(s, H6_SET_PVW_CH, ch); }
int h6_slider(sw_session_t *s, uint8_t dir, uint8_t pos) { return sw_tx_double(s, H6_SET_SLIDER_POS, dir, pos); }
int h6_cut(sw_session_t *s) { return sw_tx_none(s, H6_SET_CUT); }
int h6_auto(sw_session_t *s) { return sw_tx_none(s, H6_SET_AUTO); }
int h6_afv(sw_session_t *s) { return sw_tx_none(s, H6_AFV_BTN); }
int h6_mix(sw_session_t *s) { return sw_tx_none(s, H6_MIX_BTN); }
int h6_pip(sw_session_t *s, bool on) { return sw_tx_single(s, H6_SET_PIP, on ? 1 : 0); }
int h6_pip_h(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H6_PIP_X, v); }
int h6_pip_v(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H6_PIP_Y, v); }
int h6_pip_size(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H6_PIP_SIZE, i); }
int h6_pip_border_enable(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H6_PIP_BORDER_ENABLE, i); }
int h6_pip_border_width(sw_session_t *s, uint8_t w) { return sw_tx_single(s, H6_PIP_BORDER_WIDTH, w); }
int h6_pip_border_color(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H6_PIP_BORDER_COLOR, i); }
int h6_pop(sw_session_t *s, bool on) { return sw_tx_single(s, H6_POP, on ? 1 : 0); }
int h6_pop_window(sw_session_t *s, uint8_t w) { return sw_tx_single(s, H6_POP_SOURCE, w); }
int h6_pop_h(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H6_POP_X, v); }
int h6_pop_v(sw_session_t *s, uint16_t v) { return sw_tx_u16(s, H6_POP_Y, v); }
int h6_pop_border_enable(sw_session_t *s, bool on) { return sw_tx_single(s, H6_POP_BORDER_ENABLE, on ? 1 : 0); }
int h6_pop_border_width(sw_session_t *s, uint8_t w) { return sw_tx_single(s, H6_POP_BORDER_WIDTH, w); }
int h6_pop_border_color(sw_session_t *s, uint8_t i) { return sw_tx_single(s, H6_POP_BORDER_COLOR, i); }
int h6_set_ip(sw_session_t *s, const uint8_t ip[4]) { return sw_tx_set_ip(s, ip); }

/* Identical switch to the 7105 (jump table 0x1122acc) */
int h6_transition(sw_session_t *s, int tr) { return h5_transition(s, tr); }

int h6_ch_source(sw_session_t *s, int ch, int index)
{
    if (index < 0 || index >= H6_NUM_SOURCES)
        return -1;
    return sw_tx_single(s, ch == 2 ? H6_AUDIO_SRC2 : H6_AUDIO_SRC1,
                        h6_source_codes[index]);
}

/* SendCMDCH1_VOL/CH2_VOL @0x8d4d86/0x8d4f56: both use opcode 0xB8, keyed
 * by the *source* the channel is set to (per-input volume). */
int h6_ch_volume(sw_session_t *s, int source_index, uint8_t vol)
{
    if (source_index < 0 || source_index >= H6_NUM_SOURCES)
        return -1;
    return sw_tx_double(s, H6_AUDIO_CHX_VOLUME, h6_source_codes[source_index],
                        vol);
}

int h6_pgm_out(sw_session_t *s, int i)
{
    if (i < 0 || i >= H6_NUM_FORMATS)
        return -1;
    return sw_tx_single(s, H6_RESOLUTION, h5_format_codes[i]);
}

/*
 * Upward stroke (value rising) sends 82 00 v.  Reaching the top swaps the
 * PGM/PVW display and flips to the downward stroke, which sends
 * 82 01 (126-v) and flips back near the bottom.  The original tested
 * `v == 126` and `126 - v == 125` exactly, so a fast drag that skipped
 * those values left the bar stuck in the wrong direction; the port uses
 * >= / <= with the same end points.
 */
bool h6_tbar_moved(h6_tbar_t *t, sw_session_t *s, int v)
{
    bool swap = false;

    if (v >= H6_TBAR_MAX) {
        if (!t->up) {
            swap = true;
            t->up = true;
            t->down = false;
        }
        t->status = true;
    }
    if (!t->status && v < H6_TBAR_MAX)
        h6_slider(s, 0, (uint8_t)v);
    if (t->status) {                       /* setslider1 */
        int pos = H6_TBAR_MAX - v;
        h6_slider(s, 1, (uint8_t)pos);
        if (pos >= H6_TBAR_MAX - 1) {
            if (!t->down) {
                swap = true;
                t->up = false;
                t->down = true;
            }
            t->status = false;
        }
    }
    return swap;
}
