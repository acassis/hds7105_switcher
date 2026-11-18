/*
 * screen_7106.c - HDS7106 control panel.
 *
 * Port of HDS7106UI with AUDIO, Transitions, PGM, PVW, PIP, POP and
 * IPDialog.  The original never reads state back from a 7106, so all
 * indicators reflect what this panel last sent.
 */

#include "app.h"
#include "hds7105.h"
#include "hds7106.h"
#include "proto.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define BASE_W  1000
#define RIGHT_W 280
#define WIN_H   640

static const char *const size_items[] = { "Small", "Medium", "Large" };
static const char *const onoff_items[] = { "OFF", "ON" };
static const char *const color_items[] = { "White", "Red", "Blue", "Green" };

static struct {
    int pgm, pvw;                    /* 1..6, 0 none (never initialised originally) */
    int audio;                       /* 0 none lit, 1 AFV, 2 MIX */
    int ch1, ch2, vol1, vol2, pgm_out;
    int trans;
    bool pip, pop;                   /* btnPIPstatus / btnPOPstatus */
    int tbar;
    h6_tbar_t tb;
    double cut_until, auto_at;
    /* PIP panel */
    int pip_h, pip_v, pip_size, pip_ben, pip_bw, pip_bcol;
    /* POP panel */
    int pop_window;                  /* 0 not chosen, 1 or 2 */
    bool pop_border;
    int pop_h, pop_v, pop_bw, pop_bcol;
    /* IP dialog */
    bool dlg_ip;
    char ip_text[32];
} S;

static sw_session_t *sess(void) { return &g_app.sess; }

void h6_screen_enter(void)
{
    memset(&S, 0, sizeof S);
    S.ch1 = S.ch2 = S.pgm_out = -1;        /* combos start empty */
    S.vol1 = S.vol2 = 60;                  /* range 0..100, value 60, step 10 */
    S.pip_size = S.pip_ben = S.pip_bcol = -1;
    S.pop_bcol = -1;
    S.pip_bw = S.pop_bw = 2;
}

bool h6_screen_modal(void) { return S.dlg_ip; }

static void swap_pgm_pvw(void)              /* HDS7106UI::setPGM_PVWCH */
{
    int t = S.pgm;
    S.pgm = S.pvw;
    S.pvw = t;
}

void h6_screen_tick(void)
{
    if (S.auto_at && GetTime() >= S.auto_at) {
        if (!S.pip)
            swap_pgm_pvw();
        S.auto_at = 0;
    }
}

/* ------------------------------------------------------------------ */

static void draw_audio(Rectangle g)
{
    bool srcs = S.audio != 1;
    float x = g.x;
    int old1 = S.vol1, old2 = S.vol2;

    ui_group(g, "AUDIO", UI_TITLE);
    if (ui_tally((Rectangle){ x + 18, g.y + 44, 62, 62 }, "AFV", "follow",
                 S.audio == 1, TONE_GREEN, true)) {
        S.audio = 1;
        h6_afv(sess());
    }
    if (ui_tally((Rectangle){ x + 18, g.y + 130, 62, 62 }, "MIX", "audio",
                 S.audio == 2, TONE_GREEN, true)) {
        S.audio = 2;
        h6_mix(sess());
    }
    ui_text(FONT_UI, "CH1", x + 98, g.y + 36, srcs ? UI_TEXT : UI_TEXT_DIM);
    ui_text(FONT_UI, "CH2", x + 98, g.y + 96, srcs ? UI_TEXT : UI_TEXT_DIM);
    ui_text(FONT_UI, "CH1_VOL", x + 232, g.y + 36, UI_TEXT);
    ui_text(FONT_UI, "CH2_VOL", x + 232, g.y + 96, UI_TEXT);
    if (ui_spinner(61, (Rectangle){ x + 232, g.y + 58, 112, 28 }, &S.vol1, 0,
                   100, 10, true)) {
        if (S.ch1 < 0) {
            S.vol1 = old1;                 /* nothing was sent */
            ui_message("Warning", "please choose CH1 source");
        }
        else
            h6_ch_volume(sess(), S.ch1, (uint8_t)S.vol1);
    }
    if (ui_spinner(62, (Rectangle){ x + 232, g.y + 118, 112, 28 }, &S.vol2, 0,
                   100, 10, true)) {
        if (S.ch2 < 0) {
            S.vol2 = old2;
            ui_message("Warning", "please choose CH2 source");
        }
        else
            h6_ch_volume(sess(), S.ch2, (uint8_t)S.vol2);
    }
    ui_text(FONT_UI, "Audio Mode", x + 98, g.y + 158, UI_TEXT);
    ui_text(FONT_UI, S.audio == 1 ? "Follow" : S.audio == 2 ? "Mixing" : "-",
            x + 98, g.y + 184, UI_ACCENT);
    ui_text(FONT_UI, "PGM Out", x + 232, g.y + 158, UI_TEXT);

    if (ui_dropdown(65, (Rectangle){ x + 232, g.y + 180, 112, 28 },
                    h6_format_names, H6_NUM_FORMATS, &S.pgm_out, true))
        h6_pgm_out(sess(), S.pgm_out);
    if (ui_dropdown(64, (Rectangle){ x + 98, g.y + 118, 120, 28 },
                    h6_source_names, H6_NUM_SOURCES, &S.ch2, srcs))
        h6_ch_source(sess(), 2, S.ch2);
    if (ui_dropdown(63, (Rectangle){ x + 98, g.y + 58, 120, 28 },
                    h6_source_names, H6_NUM_SOURCES, &S.ch1, srcs))
        h6_ch_source(sess(), 1, S.ch1);
}

static void draw_transitions(Rectangle g)
{
    static const struct { int id; const char *lbl, *sub; float x, y; } b[] = {
        { H5_TR_MIX, "MIX", NULL, 18, 44 },
        { H5_TR_FADE, "FADE", NULL, 18, 130 },
        { H5_TR_UP, "UP", "WIPE", 106, 44 },
        { H5_TR_DOWN, "DOWN", "WIPE", 106, 130 },
        { H5_TR_LEFT, "LEFT", "WIPE", 188, 44 },
        { H5_TR_RIGHT, "RIGHT", "WIPE", 188, 130 },
    };
    ui_font_t tf = ui_fit_font("RIGHT", 62, FONT_BIG);   /* one size for all */
    int i;

    ui_group(g, "TRANSITIONS", UI_TITLE);
    for (i = 0; i < 6; i++) {
        Rectangle r = { g.x + b[i].x, g.y + b[i].y, 62, 62 };
        if (ui_tally_font(r, b[i].lbl, b[i].sub, S.trans == b[i].id, TONE_GREEN,
                          true, tf)) {
            S.trans = b[i].id;
            h6_transition(sess(), b[i].id);
        }
    }
}

static void draw_bus(Rectangle g, const char *title, bool pgm)
{
    int i;

    ui_group(g, title, pgm ? UI_RED : UI_GREEN);
    for (i = 1; i <= 6; i++) {
        char l[4];
        Rectangle r = { g.x + 22 + 120.0f * (float)(i - 1), g.y + 36, 64, 64 };
        int *sel = pgm ? &S.pgm : &S.pvw;
        snprintf(l, sizeof l, "%d", i);
        if (ui_tally(r, l, NULL, *sel == i, pgm ? TONE_RED : TONE_GREEN, true)) {
            *sel = i;
            if (pgm)
                h6_pgm(sess(), (uint8_t)i);
            else
                h6_pvw(sess(), (uint8_t)i);
        }
    }
}

static void row_label(float x, float y, const char *s)
{
    ui_text(FONT_UI, s, x, y + 6, UI_TEXT);
}

static void draw_pip_panel(Rectangle g)
{
    float lx = g.x + 14, cx = g.x + 130, cw = g.width - 144, y = g.y + 40;
    int hmax, vmax;

    ui_group(g, "PIP", UI_TITLE);
    h5_pip_range(S.pip_size < 0 ? 2 : S.pip_size, &hmax, &vmax);
    row_label(lx, y, "Window");
    ui_button((Rectangle){ cx, y, cw, 30 }, "---", false);
    y += 44;
    row_label(lx, y, "H Position");
    if (ui_spinner(70, (Rectangle){ cx, y, cw, 30 }, &S.pip_h, 0, hmax, 10, true))
        h6_pip_h(sess(), (uint16_t)S.pip_h);
    y += 44;
    row_label(lx, y, "V Position");
    if (ui_spinner(71, (Rectangle){ cx, y, cw, 30 }, &S.pip_v, 0, vmax, 10, true))
        h6_pip_v(sess(), (uint16_t)S.pip_v);
    y += 44;
    row_label(lx, y, "Size");
    y += 44;
    row_label(lx, y, "Border Enable");
    y += 44;
    row_label(lx, y, "Border Width");
    if (ui_spinner(72, (Rectangle){ cx, y, cw, 30 }, &S.pip_bw, 2, 7, 1, true))
        h6_pip_border_width(sess(), (uint8_t)S.pip_bw);
    y += 44;
    row_label(lx, y, "Border Color");
    if (ui_dropdown(73, (Rectangle){ cx, y, cw, 30 }, color_items, 4, &S.pip_bcol, true))
        h6_pip_border_color(sess(), (uint8_t)S.pip_bcol);
    y -= 88;
    if (ui_dropdown(74, (Rectangle){ cx, y, cw, 30 }, onoff_items, 2, &S.pip_ben, true))
        h6_pip_border_enable(sess(), (uint8_t)S.pip_ben);
    y -= 44;
    if (ui_dropdown(75, (Rectangle){ cx, y, cw, 30 }, size_items, 3, &S.pip_size,
                    true)) {
        h5_pip_range(S.pip_size, &hmax, &vmax);     /* PIP::setCoboxSize */
        if (S.pip_h > hmax) S.pip_h = hmax;
        if (S.pip_v > vmax) S.pip_v = vmax;
        h6_pip_size(sess(), (uint8_t)S.pip_size);
    }
}

static void draw_pop_panel(Rectangle g)
{
    float lx = g.x + 14, cx = g.x + 130, cw = g.width - 144, y = g.y + 40;
    char wl[8];
    int oldh = S.pop_h, oldv = S.pop_v;

    ui_group(g, "POP", UI_TITLE);
    row_label(lx, y, "Window");
    snprintf(wl, sizeof wl, "%s", S.pop_window ? (S.pop_window == 1 ? "1" : "2") : "---");
    if (ui_button((Rectangle){ cx, y, cw, 30 }, wl, true)) {
        S.pop_window = S.pop_window == 1 ? 2 : 1;   /* POP::setlwindowindex */
        h6_pop_window(sess(), (uint8_t)S.pop_window);
    }
    y += 44;
    row_label(lx, y, "H Position");
    if (ui_spinner(80, (Rectangle){ cx, y, cw, 30 }, &S.pop_h, 0, 960, 10, true)) {
        if (!S.pop_window) {
            S.pop_h = oldh;
            ui_message("Warning", "please choose Window source");
        } else {
            h6_pop_h(sess(), (uint16_t)S.pop_h);
        }
    }
    y += 44;
    row_label(lx, y, "V Position");
    if (ui_spinner(81, (Rectangle){ cx, y, cw, 30 }, &S.pop_v, 0, 540, 10, true)) {
        if (!S.pop_window) {
            S.pop_v = oldv;
            ui_message("Warning", "please choose Window source");
        } else {
            h6_pop_v(sess(), (uint16_t)S.pop_v);
        }
    }
    y += 44;
    row_label(lx, y, "Size");
    ui_button((Rectangle){ cx, y, cw, 30 }, "Large", false);
    y += 44;
    row_label(lx, y, "Border Enable");
    if (ui_button((Rectangle){ cx, y, cw, 30 }, S.pop_border ? "On" : "Off", true)) {
        S.pop_border = !S.pop_border;               /* POP::setbtnBorder_Enable */
        h6_pop_border_enable(sess(), S.pop_border);
    }
    y += 44;
    row_label(lx, y, "Border Width");
    if (ui_spinner(82, (Rectangle){ cx, y, cw, 30 }, &S.pop_bw, 2, 7, 1, true))
        h6_pop_border_width(sess(), (uint8_t)S.pop_bw);
    y += 44;
    row_label(lx, y, "Border Color");
    if (ui_dropdown(83, (Rectangle){ cx, y, cw, 30 }, color_items, 4, &S.pop_bcol, true))
        h6_pop_border_color(sess(), (uint8_t)S.pop_bcol);
}

static void draw_ip_dialog(void)
{
    Rectangle c = ui_dialog(340, 150, "Device IP");
    uint32_t ip;

    ui_text(FONT_UI, "IP addr", c.x + 18, c.y + 24, UI_TEXT);
    ui_textbox(90, (Rectangle){ c.x + 100, c.y + 18, 220, 30 }, S.ip_text,
               sizeof S.ip_text, true);
    if (ui_button((Rectangle){ c.x + c.width - 200, c.y + c.height - 44, 86, 32 },
                  "close", true) || IsKeyPressed(KEY_ESCAPE))
        S.dlg_ip = false;
    if (ui_button((Rectangle){ c.x + c.width - 104, c.y + c.height - 44, 86, 32 },
                  "ok", true)) {
        /* IPDialog::on_btnip_clicked */
        if (!S.ip_text[0] || strcmp(S.ip_text, "0.0.0.0") == 0)
            ui_message("Warning", "Text cannot be empty");
        else if (!sw_ip_parse(S.ip_text, &ip))
            ui_message("Warning", "Invalid IP address: \"%s\"", S.ip_text);
        else {
            uint8_t b[4] = { (uint8_t)(ip >> 24), (uint8_t)(ip >> 16),
                             (uint8_t)(ip >> 8), (uint8_t)ip };
            h6_set_ip(sess(), b);              /* sent to the old address */
            sess()->ip = ip;
            S.dlg_ip = false;
            ui_toast("IP change sent; now talking to %s", S.ip_text);
        }
    }
}

void h6_screen_draw(void)
{
    char ip[16], sub[96];
    bool right = S.pip || S.pop;
    double now = GetTime();
    bool auto_lit;

    app_set_window(right ? BASE_W + RIGHT_W : BASE_W, WIN_H);
    ui_layer(0);
    sw_ip_str(sess()->ip, ip);
    snprintf(sub, sizeof sub, "%s%s   (the HDS7106 does not report its state)", ip,
             sess()->bcast ? " [broadcast]" : "");
    app_draw_header("HDS7106", sub);
    if (ui_button((Rectangle){ (float)GetScreenWidth() - 120, 6, 108, 30 },
                  "Set IP", true)) {
        snprintf(S.ip_text, sizeof S.ip_text, "%s", ip);
        S.dlg_ip = true;
    }
    app_draw_time_button((float)GetScreenWidth() - 240, 6);

    if (ui_tally((Rectangle){ 20, 52, 58, 58 }, "<", "RETURN", false, TONE_NONE, true))
        app_back_to_search();

    draw_transitions((Rectangle){ 462, 52, 270, 236 });
    draw_bus((Rectangle){ 20, 300, 712, 116 }, "PGM", true);
    draw_bus((Rectangle){ 20, 432, 712, 116 }, "PVW", false);

    ui_text_center(FONT_BIG, "HD VIDEO", (Rectangle){ 745, 50, 250, 30 },
                   (Color){ 255, 255, 251, 255 });
    ui_text_center(FONT_BIG, "SWITCHER", (Rectangle){ 745, 78, 250, 30 },
                   (Color){ 255, 255, 251, 255 });

    if (ui_tally((Rectangle){ 770, 122, 70, 70 }, "PIP", NULL, S.pip, TONE_GREEN, true)) {
        if (!S.pip) {                          /* setBtnPIP */
            h6_pip(sess(), true);
            S.pip = true;
        } else {
            h6_pip(sess(), false);
            S.pip = false;
        }
        S.pop = false;
    }
    if (ui_tally((Rectangle){ 890, 122, 70, 70 }, "POP", NULL, S.pop, TONE_GREEN, true)) {
        if (!S.pop) {                          /* setBtnPOP */
            h6_pop(sess(), true);
            S.pop = true;
        } else {
            h6_pop(sess(), false);
            S.pop = false;
        }
        S.pip = false;
    }

    /* T-bar */
    if (ui_vslider(66, (Rectangle){ 762, 210, 96, 366 }, &S.tbar, 0,
                   H6_TBAR_MAX, true)) {
        if (h6_tbar_moved(&S.tb, sess(), S.tbar))
            swap_pgm_pvw();
    }
    ui_text_center(FONT_SMALL, "T-BAR", (Rectangle){ 762, 578, 96, 16 }, UI_TEXT_DIM);

    auto_lit = S.auto_at != 0 || (S.tbar > 0 && S.tbar < H6_TBAR_MAX);
    if (ui_tally((Rectangle){ 888, 300, 80, 72 }, "AUTO", NULL, auto_lit, TONE_RED,
                 S.auto_at == 0)) {
        h6_auto(sess());                       /* setBtnAUTO */
        S.auto_at = now + 0.9;
    }
    if (ui_tally((Rectangle){ 888, 440, 80, 72 }, "CUT", NULL, now < S.cut_until,
                 TONE_RED, true)) {
        h6_cut(sess());                        /* setBtncut */
        if (!S.pip)
            swap_pgm_pvw();
        S.cut_until = now + 0.3;
    }

    if (S.pip)
        draw_pip_panel((Rectangle){ BASE_W + 10, 120, RIGHT_W - 20, 380 });
    else if (S.pop)
        draw_pop_panel((Rectangle){ BASE_W + 10, 120, RIGHT_W - 20, 380 });

    draw_audio((Rectangle){ 90, 52, 360, 236 });
    app_draw_footer(WIN_H - 40);
    ui_end_layer();

    if (S.dlg_ip) {
        ui_layer(1);
        draw_ip_dialog();
        ui_end_layer();
    }
}
