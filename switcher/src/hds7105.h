/*
 * hds7105.h - HDS7105 (and 7105S / 7105(2021)) commands and replies.
 * Port of HDS7105_workdata + the status decoding in HDS7105UI.
 */

#ifndef SWITCHER_HDS7105_H
#define SWITCHER_HDS7105_H

#include "proto.h"
#include "session.h"

/* Opcodes (vendor enum DW_SWITCHER_7105CMD) */
enum {
    H5_SET_IP       = 0x05,
    H5_HIS_DNS      = 0x23,     /* streaming module: set DNS          */
    H5_HIS_READ_IP  = 0x24,     /* streaming module: read IP info     */
    H5_ALL_DATA     = 0x49,     /* read full state                    */
    H5_HIS_SET_URL  = 0x67,
    H5_HIS_READ_URL = 0x6A,
    H5_PGM_CH       = 0x80,
    H5_PVW_CH       = 0x81,
    H5_CUT          = 0x83,
    H5_AUTO         = 0x84,
    H5_FTB          = 0x85,
    H5_PIP          = 0x86,
    H5_SHOW_MIX     = 0x8D,
    H5_SHOW_DIP     = 0x8E,     /* "FADE" button                      */
    H5_SET_WIPE     = 0x8F,
    H5_SHOW_WIPE    = 0x92,
    H5_PIP_BORDER_ENABLE = 0x9D,
    H5_PIP_BORDER_COLOR  = 0x9E,
    H5_PIP_BORDER_WIDTH  = 0x9F,
    H5_PIP_X        = 0xA0,
    H5_PIP_Y        = 0xA1,
    H5_PIP_SIZE     = 0xA2,
    H5_PGM_OUT      = 0xA3,     /* F1                                 */
    H5_RATE         = 0xB1,
    H5_AUDIO_SRC1   = 0xB9,
    H5_AUDIO_SRC2   = 0xBA,
    H5_MIX_BTN      = 0xCB,     /* enum name "AFV"; sent by MIX button */
    H5_AFV_BTN      = 0xCC,     /* enum name "AUDIO"; sent by AFV      */
    H5_STILL        = 0xCD,
    H5_CH1VOL       = 0xCE,
    H5_CH2VOL       = 0xCF,
    H5_VIDEO1_SRC   = 0xD0,     /* F2                                 */
    H5_AUDIO_MUTE   = 0xD4,     /* F3                                 */
    H5_OSD_ROTATING = 0xD6,     /* F7                                 */
};

/* Transition ids as used by the button group / SendCMDTRANSITIONS */
enum { H5_TR_MIX = 1, H5_TR_FADE, H5_TR_UP, H5_TR_DOWN, H5_TR_LEFT, H5_TR_RIGHT };

/* Audio source combo: PHONE_IN, IN1..IN4, MIC_IN1, MIC_IN2 */
#define H5_NUM_SOURCES 7
extern const char *const h5_source_names[H5_NUM_SOURCES];
extern const uint8_t h5_source_codes[H5_NUM_SOURCES];   /* {7,3,4,5,6,8,0} */
int h5_source_index(uint8_t code);                      /* -1 if unknown   */

/* PGM out formats (F1) */
#define H5_NUM_FORMATS 7
extern const char *const h5_format_names[H5_NUM_FORMATS];
extern const uint8_t h5_format_codes[H5_NUM_FORMATS];   /* {07,0B,09,01,03,0D,05} */
int h5_format_index(uint8_t code);

/* Full state as decoded from the 0x49 reply */
typedef struct {
    uint8_t pgm, pvw;            /* 1..4, 9 = BAR                       */
    uint8_t audio_mode;          /* 0 AFV, 1 MIX                        */
    uint8_t ch1_src, ch1_vol, ch2_src, ch2_vol;
    uint8_t trans, wipe;         /* trans 1 MIX 2 FADE 4 WIPE; wipe 0 L 2 U 4 R 6 D */
    uint8_t ftb, pip;
    uint8_t pip_border_enable, pip_border_color, pip_border_width, pip_size;
    uint16_t pip_h, pip_v;
    uint8_t still, rate, pgm_out, mute;
} h5_status_t;

/* Commands --------------------------------------------------------- */
int h5_read_state(sw_session_t *s);
int h5_pgm(sw_session_t *s, uint8_t ch);
int h5_pvw(sw_session_t *s, uint8_t ch);
int h5_cut(sw_session_t *s);
int h5_auto(sw_session_t *s);
int h5_ftb(sw_session_t *s);
int h5_rate(sw_session_t *s, uint8_t rate);
int h5_transition(sw_session_t *s, int tr);      /* H5_TR_*              */
int h5_afv(sw_session_t *s);
int h5_mix(sw_session_t *s);
int h5_ch_source(sw_session_t *s, int ch, int index);   /* ch 1 or 2     */
int h5_ch_volume(sw_session_t *s, int ch, uint8_t vol);
int h5_pip(sw_session_t *s, bool on);
int h5_still(sw_session_t *s);
int h5_pip_h(sw_session_t *s, uint16_t v);
int h5_pip_v(sw_session_t *s, uint16_t v);
int h5_pip_size(sw_session_t *s, uint8_t idx);
int h5_pip_border_enable(sw_session_t *s, uint8_t idx);
int h5_pip_border_width(sw_session_t *s, uint8_t w);
int h5_pip_border_color(sw_session_t *s, uint8_t idx);
int h5_pgm_out(sw_session_t *s, int format_index);       /* F1 */
int h5_f2(sw_session_t *s);
int h5_f3(sw_session_t *s);
int h5_f7(sw_session_t *s);
int h5_set_switcher_ip(sw_session_t *s, const uint8_t ip[4]);

/* Streaming ("HIS") module; return -1 if no module is paired */
int h5_his_read_ip(sw_session_t *s);
int h5_his_set_ip(sw_session_t *s, const uint8_t ip[4], const uint8_t mask[4],
                  const uint8_t gw[4]);
int h5_his_set_dns(sw_session_t *s, const uint8_t dns[4]);
int h5_his_read_url(sw_session_t *s, uint8_t index);
int h5_his_set_url(sw_session_t *s, uint8_t index, const char *name,
                   const char *url);

/* PIP H/V maximum for a size index (Plane_Right::on_coboxIndexChanged_Size) */
void h5_pip_range(int size_idx, int *hmax, int *vmax);

/* Replies ------------------------------------------------------------ */
typedef struct {
    char ip[16], mask[16], gw[16], dns[16];
} h5_his_info_t;

typedef struct {
    int index;                   /* 0..7                                 */
    char name[SW_URL_NAME_MAX + 1];
    char url[SW_URL_MAX + 1];
} h5_url_info_t;

typedef enum { H5_RX_NONE, H5_RX_STATUS, H5_RX_HIS_INFO, H5_RX_URL } h5_rx_kind_t;

/* Decode a validated frame (HDS7105_workdata::AnalysisData @0x8d00c0). */
h5_rx_kind_t h5_decode(const uint8_t *f, size_t len, h5_status_t *st,
                       h5_his_info_t *his, h5_url_info_t *url);

#endif
