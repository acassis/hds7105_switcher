/*
 * hds7106.h - HDS7106 commands (port of HDS7106_workdata).
 *
 * The original 7106 UI never reads state back from the device
 * (Read7106State_Data @0x8d54d0 is a no-op), so there is no decoder.
 */

#ifndef SWITCHER_HDS7106_H
#define SWITCHER_HDS7106_H

#include "session.h"

/* Opcodes (vendor enum DW_SWITCHER_CMD) */
enum {
    H6_SET_PGM_CH      = 0x80,
    H6_SET_PVW_CH      = 0x81,
    H6_SET_SLIDER_POS  = 0x82,
    H6_SET_CUT         = 0x83,
    H6_SET_AUTO        = 0x84,
    H6_SET_PIP         = 0x86,
    H6_SHOW_MIX        = 0x8D,
    H6_SHOW_DIP        = 0x8E,
    H6_SHOW_WIPE       = 0x8F,
    H6_WIPE_STYLE      = 0x92,
    H6_PIP_BORDER_ENABLE = 0x9D,
    H6_PIP_BORDER_COLOR  = 0x9E,
    H6_PIP_BORDER_WIDTH  = 0x9F,
    H6_PIP_X           = 0xA0,
    H6_PIP_Y           = 0xA1,
    H6_PIP_SIZE        = 0xA2,
    H6_RESOLUTION      = 0xA3,
    H6_POP             = 0xA7,
    H6_POP_SOURCE      = 0xA8,
    H6_POP_BORDER_ENABLE = 0xA9,
    H6_POP_BORDER_COLOR  = 0xAA,
    H6_POP_BORDER_WIDTH  = 0xAB,
    H6_POP_X           = 0xAC,
    H6_POP_Y           = 0xAD,
    H6_AUDIO_CHX_VOLUME = 0xB8,
    H6_AUDIO_SRC1      = 0xB9,
    H6_AUDIO_SRC2      = 0xBA,
    H6_MIX_BTN         = 0xCB,
    H6_AFV_BTN         = 0xCC,
};

#define H6_TBAR_MAX 126

#define H6_NUM_SOURCES 8
extern const char *const h6_source_names[H6_NUM_SOURCES];
extern const uint8_t h6_source_codes[H6_NUM_SOURCES];   /* CH_NB = {0,1,3,4,5,6,7,8} */

#define H6_NUM_FORMATS 7
extern const char *const h6_format_names[H6_NUM_FORMATS];

int h6_pgm(sw_session_t *s, uint8_t ch);              /* 1..6 */
int h6_pvw(sw_session_t *s, uint8_t ch);
int h6_slider(sw_session_t *s, uint8_t dir, uint8_t pos);
int h6_cut(sw_session_t *s);
int h6_auto(sw_session_t *s);
int h6_transition(sw_session_t *s, int tr);           /* same ids as 7105 */
int h6_afv(sw_session_t *s);
int h6_mix(sw_session_t *s);
int h6_ch_source(sw_session_t *s, int ch, int index);
int h6_ch_volume(sw_session_t *s, int source_index, uint8_t vol);
int h6_pgm_out(sw_session_t *s, int format_index);
int h6_pip(sw_session_t *s, bool on);
int h6_pip_h(sw_session_t *s, uint16_t v);
int h6_pip_v(sw_session_t *s, uint16_t v);
int h6_pip_size(sw_session_t *s, uint8_t idx);
int h6_pip_border_enable(sw_session_t *s, uint8_t idx);
int h6_pip_border_width(sw_session_t *s, uint8_t w);
int h6_pip_border_color(sw_session_t *s, uint8_t idx);
int h6_pop(sw_session_t *s, bool on);
int h6_pop_window(sw_session_t *s, uint8_t w);        /* 1 or 2 */
int h6_pop_h(sw_session_t *s, uint16_t v);
int h6_pop_v(sw_session_t *s, uint16_t v);
int h6_pop_border_enable(sw_session_t *s, bool on);
int h6_pop_border_width(sw_session_t *s, uint8_t w);
int h6_pop_border_color(sw_session_t *s, uint8_t idx);
int h6_set_ip(sw_session_t *s, const uint8_t ip[4]);

/* T-bar state machine (HDS7106UI::setslider0 @0xd17c3c / setslider1). */
typedef struct {
    bool up, down;       /* sliderup / sliderdown                 */
    bool status;         /* sliderstatus: true = downward stroke  */
} h6_tbar_t;

/* Feed a new slider value; sends 0x82 frames.  Returns true when the
 * PGM/PVW display must be swapped (end of a stroke). */
bool h6_tbar_moved(h6_tbar_t *t, sw_session_t *s, int v);

#endif
