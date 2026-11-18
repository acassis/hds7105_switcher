/*
 * ui.h - tiny immediate-mode widget kit on top of raylib.
 *
 * Layers: 0 = screen, 1 = dialog, 2 = message box.  Only widgets drawn on
 * the top-most open layer receive input, and an open drop-down list takes
 * all clicks until it closes (like a Qt popup).
 */

#ifndef SWITCHER_UI_H
#define SWITCHER_UI_H

#include "raylib.h"
#include <stdbool.h>

/* Palette (taken from the original style sheets where possible) */
#define UI_BG          (Color){ 24, 24, 24, 255 }
#define UI_PANEL       (Color){ 58, 58, 58, 255 }     /* rgb(58,58,58)   */
#define UI_PANEL_DARK  (Color){ 44, 44, 44, 255 }
#define UI_BORDER      (Color){ 137, 137, 137, 255 }  /* group border    */
#define UI_TITLE       (Color){ 160, 160, 160, 255 }
#define UI_TEXT        (Color){ 220, 220, 220, 255 }
#define UI_TEXT_DIM    (Color){ 120, 120, 120, 255 }
#define UI_ACCENT      (Color){ 59, 115, 160, 255 }   /* #3b73a0         */
#define UI_BTN         (Color){ 72, 72, 72, 255 }
#define UI_BTN_HOVER   (Color){ 92, 92, 92, 255 }
#define UI_BTN_DOWN    (Color){ 120, 120, 120, 255 }
#define UI_RED         (Color){ 205, 5, 5, 255 }      /* rgb(205,5,5)    */
#define UI_GREEN       (Color){ 19, 160, 0, 255 }
#define UI_AMBER       (Color){ 214, 140, 0, 255 }
#define UI_FIELD       (Color){ 32, 32, 32, 255 }

typedef enum { TONE_NONE, TONE_RED, TONE_GREEN, TONE_AMBER } ui_tone_t;
typedef enum { FONT_UI, FONT_SMALL, FONT_TITLE, FONT_BIG } ui_font_t;

void ui_init(void);            /* after InitWindow */
void ui_shutdown(void);

/* Frame: call ui_begin_frame() first; it settles last frame's drop-down
 * clicks.  `modal_open` says whether the screen will draw a dialog. */
void ui_begin_frame(bool modal_open);
void ui_layer(int layer);      /* select the layer the next widgets are on */
void ui_end_layer(void);       /* draw this layer's open drop-down list    */
void ui_end_frame(void);       /* message box + toast                      */

/* Text */
void ui_text(ui_font_t f, const char *s, float x, float y, Color c);
void ui_text_center(ui_font_t f, const char *s, Rectangle r, Color c);
void ui_text_right(ui_font_t f, const char *s, float right, float y, Color c);
float ui_text_width(ui_font_t f, const char *s);
float ui_font_size(ui_font_t f);

/* Containers */
void ui_group(Rectangle r, const char *title, Color title_color);
void ui_panel(Rectangle r, Color fill);
void ui_dim(void);                       /* darken whole window (behind a dialog) */
Rectangle ui_dialog(float w, float h, const char *title);  /* centred frame */

/* Widgets.  `enabled == false` draws greyed and ignores input. */
bool ui_button(Rectangle r, const char *label, bool enabled);
/* Tally/function button: lit with `tone` when `on`.  The label uses the
 * largest font that fits; ui_tally_font() caps it at `font` so a group of
 * buttons can share one size (see ui_fit_font). */
bool ui_tally(Rectangle r, const char *label, const char *sub, bool on,
              ui_tone_t tone, bool enabled);
bool ui_tally_font(Rectangle r, const char *label, const char *sub, bool on,
                   ui_tone_t tone, bool enabled, ui_font_t font);
/* Largest font (FONT_BIG down to FONT_SMALL, starting at `start`) whose
 * rendering of `s` fits in a button `width` pixels wide. */
ui_font_t ui_fit_font(const char *s, float width, ui_font_t start);
bool ui_dropdown(int id, Rectangle r, const char *const *items, int n,
                 int *active, bool enabled);
bool ui_spinner(int id, Rectangle r, int *value, int min, int max, int step,
                bool enabled);
/* Returns true when the user presses Enter in the box. */
bool ui_textbox(int id, Rectangle r, char *buf, int cap, bool enabled);
bool ui_radio(Rectangle r, const char *label, bool selected);
/* Vertical fader; returns true while the value changes. */
bool ui_vslider(int id, Rectangle r, int *value, int min, int max, bool enabled);

/* Row inside a list; returns 1 on click, 2 on double click. */
int  ui_list_row(Rectangle r, const char *left, const char *right,
                 bool selected);

bool ui_hovered(Rectangle r);            /* respects layer gating */
void ui_tooltip(Rectangle r, const char *text);

/* Non-blocking notifications */
void ui_message(const char *title, const char *fmt, ...);
bool ui_message_open(void);
void ui_toast(const char *fmt, ...);

#endif
