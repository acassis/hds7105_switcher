/*
 * screen_7105.c - HDS7105 control panel.
 *
 * Port of HDS7105UI with its panels: Upper_Widget (F-keys, FTB, DS),
 * Middle_widget (AUDIO, TRANSITIONS, FUNC), Below_Widget (PGM, PVW,
 * CUT/AUTO, RATE), Plane_Right (PIP settings) and Plane_Botton (network
 * settings dialog).
 */

#include "app.h"
#include "hds7105.h"
#include "proto.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define BASE_W 1000
#define PIP_W  230
#define WIN_H  640

static const char *const size_items[] = { "Small", "Medium", "Large" };
static const char *const onoff_items[] = { "OFF", "ON" };
static const char *const color_items[] = { "White", "Red", "Blue", "Green" };
static const char *const index_items[] = { "1", "2", "3", "4", "5", "6", "7", "8" };

typedef enum { DLG_NONE, DLG_SETTINGS, DLG_PGMOUT } dlg_t;

static struct {
    /* Below_Widget */
    int pgm, pvw;                 /* 1..4, 9 = BAR, 0 none */
    int rate;                     /* 0..2, -1 none         */
    double cut_until, auto_at;
    /* Middle_widget */
    int audio_mode;               /* -1 unknown, 0 AFV, 1 MIX */
    int ch1, ch2, vol1, vol2;
    int trans;                    /* lit transition button 1..6 */
    bool pip, still;
    /* Upper_Widget */
    bool f3, f7, ftb;
    double f2_until;
    int pgm_out;                  /* format index */
    /* Plane_Right */
    int pip_h, pip_v, pip_size, pip_ben, pip_bw, pip_bcol;
    /* status */
    double opened_at, last_status;
    char status_time[16];
    /* dialogs */
    dlg_t dlg;
    char sw_ip[32];
    char his_ip[32], his_mask[32], his_gw[32], his_dns[32];
    int url_idx;
    char url_name[SW_URL_NAME_MAX + 8];
    char url[SW_URL_MAX + 8];
} S;

static sw_session_t *sess(void) { return &g_app.sess; }

void h5_screen_enter(void)
{
    char ip[16];

    memset(&S, 0, sizeof S);
    S.rate = -1;
    S.audio_mode = -1;
    S.vol1 = S.vol2 = 50;          /* init_AUDIO: range 0..100, value 50 */
    S.pip_bw = 2;
    S.pip_size = -1;               /* H/V start at 0..960 (init_PIP) */
    S.opened_at = GetTime();
    sw_ip_str(sess()->ip, ip);
    snprintf(S.sw_ip, sizeof S.sw_ip, "%s", ip);
    if (sess()->has_his) {
        sw_ip_str(sess()->his_ip, ip);
        snprintf(S.his_ip, sizeof S.his_ip, "%s", ip);
    }
    /* HDS7105UI::DevTypeID -> SendCMDRead7105state */
    h5_read_state(sess());
}

bool h5_screen_modal(void) { return S.dlg != DLG_NONE; }

static void swap_pgm_pvw(void)
{
    int t = S.pgm;
    S.pgm = S.pvw;
    S.pvw = t;
}

void h5_screen_tick(void)
{
    double now = GetTime();

    if (S.auto_at && now >= S.auto_at) {        /* AUTO: Delay_MSec(900) */
        swap_pgm_pvw();
        S.auto_at = 0;
    }
}

static void apply_status(const h5_status_t *st)
{
    int i, hmax, vmax;
    time_t t = time(NULL);

    S.pgm = st->pgm;
    S.pvw = st->pvw;
    S.audio_mode = st->audio_mode <= 1 ? st->audio_mode : -1;
    if ((i = h5_source_index(st->ch1_src)) >= 0) S.ch1 = i;
    if ((i = h5_source_index(st->ch2_src)) >= 0) S.ch2 = i;
    S.vol1 = st->ch1_vol > 100 ? 100 : st->ch1_vol;
    S.vol2 = st->ch2_vol > 100 ? 100 : st->ch2_vol;
    switch (st->trans) {
    case 1: S.trans = H5_TR_MIX; break;
    case 2: S.trans = H5_TR_FADE; break;
    case 4:
        S.trans = st->wipe == 0 ? H5_TR_LEFT : st->wipe == 2 ? H5_TR_UP
                : st->wipe == 4 ? H5_TR_RIGHT : st->wipe == 6 ? H5_TR_DOWN : 0;
        break;
    default: S.trans = 0;
    }
    S.ftb = st->ftb == 1;
    S.pip = st->pip == 1;
    S.pip_ben = st->pip_border_enable == 1;
    if (st->pip_border_color <= 3) S.pip_bcol = st->pip_border_color;
    if (st->pip_border_width >= 2 && st->pip_border_width <= 7)
        S.pip_bw = st->pip_border_width;
    if (st->pip_size <= 2) S.pip_size = st->pip_size;
    h5_pip_range(S.pip_size, &hmax, &vmax);
    S.pip_h = st->pip_h > hmax ? hmax : st->pip_h;
    S.pip_v = st->pip_v > vmax ? vmax : st->pip_v;
    S.still = st->still == 1;
    S.rate = st->rate <= 2 ? st->rate : -1;
    if ((i = h5_format_index(st->pgm_out)) >= 0) S.pgm_out = i;
    if (st->mute == 0) S.f3 = false;
    if (st->mute == 0xFF) S.f3 = true;

    S.last_status = GetTime();
    strftime(S.status_time, sizeof S.status_time, "%H:%M:%S", localtime(&t));
}

void h5_screen_rx(uint32_t ip, const uint8_t *f, size_t n)
{
    static h5_status_t st;
    static h5_his_info_t his;
    static h5_url_info_t url;
    h5_rx_kind_t k;

    if (ip != sess()->ip && !(sess()->has_his && ip == sess()->his_ip))
        return;
    k = h5_decode(f, n, &st, &his, &url);
    switch (k) {
    case H5_RX_STATUS:
        if (ip == sess()->ip) {
            apply_status(&st);
            app_log("state: PGM %d PVW %d %s", st.pgm, st.pvw,
                    st.audio_mode ? "MIX" : "AFV");
        }
        break;
    case H5_RX_HIS_INFO:
        snprintf(S.his_ip, sizeof S.his_ip, "%s", his.ip);
        snprintf(S.his_mask, sizeof S.his_mask, "%s", his.mask);
        snprintf(S.his_gw, sizeof S.his_gw, "%s", his.gw);
        snprintf(S.his_dns, sizeof S.his_dns, "%s", his.dns);
        ui_toast("Streaming module network settings received");
        break;
    case H5_RX_URL:
        S.url_idx = url.index <= 7 ? url.index : 0;
        snprintf(S.url_name, sizeof S.url_name, "%s", url.name);
        snprintf(S.url, sizeof S.url, "%s", url.url);
        ui_toast("Stream URL %d received", S.url_idx + 1);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */

static bool parse_ip4(const char *s, uint8_t out[4], const char *what)
{
    uint32_t ip;

    if (!s[0]) {
        ui_message("Warning", "Text cannot be empty");
        return false;
    }
    if (!sw_ip_parse(s, &ip)) {
        ui_message("Warning", "%s is not a valid IPv4 address: \"%s\"", what, s);
        return false;
    }
    out[0] = (uint8_t)(ip >> 24);
    out[1] = (uint8_t)(ip >> 16);
    out[2] = (uint8_t)(ip >> 8);
    out[3] = (uint8_t)ip;
    return true;
}

static void draw_settings(void)
{
    Rectangle c = ui_dialog(700, 500, "Settings");
    Rectangle g1 = { c.x + 16, c.y + 14, c.width - 32, 74 };
    Rectangle g2 = { c.x + 16, g1.y + g1.height + 12, c.width - 32, 150 };
    Rectangle g3 = { c.x + 16, g2.y + g2.height + 12, c.width - 32, 150 };
    bool his = sess()->has_his;
    uint8_t a[4], b[4], d[4];
    float x, y;

    /* SWITCHER IP */
    ui_group(g1, NULL, UI_TITLE);
    ui_text(FONT_TITLE, "SWITCHER IP", g1.x + 14, g1.y + 10, UI_ACCENT);
    ui_text(FONT_UI, "IP ADDR", g1.x + 14, g1.y + 44, UI_TEXT);
    ui_textbox(10, (Rectangle){ g1.x + 100, g1.y + 36, 180, 30 }, S.sw_ip,
               sizeof S.sw_ip, true);
    if (ui_button((Rectangle){ g1.x + 296, g1.y + 36, 110, 30 }, "Set IP", true) &&
        parse_ip4(S.sw_ip, a, "IP ADDR")) {
        h5_set_switcher_ip(sess(), a);         /* SendCMD_SetIP */
        sess()->ip = ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) |
                     ((uint32_t)a[2] << 8) | a[3];
        ui_toast("Switcher IP change sent; now talking to %s", S.sw_ip);
    }
    ui_text(FONT_SMALL, "Only the address is sent (mask/gateway 0)",
            g1.x + 420, g1.y + 44, UI_TEXT_DIM);

    /* HIS INFO SET */
    ui_group(g2, NULL, UI_TITLE);
    ui_text(FONT_TITLE, "HIS INFO SET", g2.x + 14, g2.y + 10, UI_ACCENT);
    if (!his)
        ui_text(FONT_SMALL, "No streaming module was paired with this switcher "
                "during discovery", g2.x + 160, g2.y + 13, UI_AMBER);
    x = g2.x + 14;
    y = g2.y + 44;
    ui_text(FONT_UI, "IP ADDR", x, y + 6, UI_TEXT);
    ui_textbox(11, (Rectangle){ x + 86, y, 170, 30 }, S.his_ip, sizeof S.his_ip, his);
    ui_text(FONT_UI, "SUBNET", x + 280, y + 6, UI_TEXT);
    ui_textbox(12, (Rectangle){ x + 366, y, 170, 30 }, S.his_mask, sizeof S.his_mask, his);
    ui_text(FONT_UI, "GATEWAY", x, y + 46, UI_TEXT);
    ui_textbox(13, (Rectangle){ x + 86, y + 40, 170, 30 }, S.his_gw, sizeof S.his_gw, his);
    ui_text(FONT_UI, "DNS", x + 280, y + 46, UI_TEXT);
    ui_textbox(14, (Rectangle){ x + 366, y + 40, 170, 30 }, S.his_dns, sizeof S.his_dns, his);
    if (ui_button((Rectangle){ g2.x + g2.width - 130, y + 80, 116, 30 }, "Read Status", his))
        h5_his_read_ip(sess());
    if (ui_button((Rectangle){ x + 86, y + 80, 120, 30 }, "Set IP", his) &&
        parse_ip4(S.his_ip, a, "IP ADDR") && parse_ip4(S.his_mask, b, "SUBNET") &&
        parse_ip4(S.his_gw, d, "GATEWAY")) {
        h5_his_set_ip(sess(), a, b, d);
        ui_toast("Streaming module IP change sent");
    }
    if (ui_button((Rectangle){ x + 366, y + 80, 120, 30 }, "Set DNS", his) &&
        parse_ip4(S.his_dns, a, "DNS")) {
        h5_his_set_dns(sess(), a);
        ui_toast("DNS change sent");
    }

    /* URL SET */
    ui_group(g3, NULL, UI_TITLE);
    ui_text(FONT_TITLE, "URL SET", g3.x + 14, g3.y + 10, UI_ACCENT);
    x = g3.x + 14;
    y = g3.y + 44;
    ui_text(FONT_UI, "Index", x, y + 6, UI_TEXT);
    ui_dropdown(15, (Rectangle){ x + 86, y, 70, 30 }, index_items, 8, &S.url_idx, his);
    ui_text(FONT_UI, "Other Name", x + 180, y + 6, UI_TEXT);
    ui_textbox(16, (Rectangle){ x + 280, y, 200, 30 }, S.url_name,
               SW_URL_NAME_MAX + 1, his);
    ui_text(FONT_UI, "Url", x, y + 46, UI_TEXT);
    ui_textbox(17, (Rectangle){ x + 86, y + 40, g3.width - 114, 30 }, S.url,
               SW_URL_MAX + 1, his);
    if (ui_button((Rectangle){ g3.x + g3.width - 130, y + 80, 116, 30 }, "Read Status", his))
        h5_his_read_url(sess(), (uint8_t)S.url_idx);
    if (ui_button((Rectangle){ x + 86, y + 80, 120, 30 }, "Set Url", his)) {
        if (strlen(S.url_name) > SW_URL_NAME_MAX)
            ui_message("Warning", "OtherName Text Errors");
        else if (strlen(S.url) > SW_URL_MAX)
            ui_message("Warning", "Url Text Errors");
        else {
            h5_his_set_url(sess(), (uint8_t)S.url_idx, S.url_name, S.url);
            ui_toast("Stream URL %d sent", S.url_idx + 1);
        }
    }
    ui_text(FONT_SMALL, "Ctrl+V pastes, Ctrl+U clears", x + 220, y + 88, UI_TEXT_DIM);

    if (ui_button((Rectangle){ c.x + c.width - 120, c.y + c.height - 44, 104, 32 },
                  "Close", true) || IsKeyPressed(KEY_ESCAPE))
        S.dlg = DLG_NONE;
}

static void draw_pgmout(void)
{
    Rectangle c = ui_dialog(300, 160, "F1 - PGM Out");

    ui_text(FONT_UI, "PGM Out", c.x + 20, c.y + 26, UI_TEXT);
    if (ui_dropdown(20, (Rectangle){ c.x + 110, c.y + 20, 160, 30 },
                    h5_format_names, H5_NUM_FORMATS, &S.pgm_out, true))
        h5_pgm_out(sess(), S.pgm_out);         /* on_coboxIndexChanged_PGMOUT */
    if (ui_button((Rectangle){ c.x + c.width - 110, c.y + c.height - 44, 94, 32 },
                  "Close", true) || IsKeyPressed(KEY_ESCAPE))
        S.dlg = DLG_NONE;                      /* F1 style off */
}

/* ------------------------------------------------------------------ */

static void draw_upper(float y)
{
    static const char *fsub[8] = { "PGM OUT", "VIDEO1", "MUTE", "", "", "", "OSD", "" };
    static const char *ftip[8] = {
        "Program output resolution (SET_SWITCHER_RESOLUTION 0xA3)",
        "VIDEO1_SRC (0xD0)", "Audio mute toggle (0xD4)",
        "Not connected in the original tool", "Not connected in the original tool",
        "Not connected in the original tool", "OSD rotating toggle (0xD6)",
        "Not connected in the original tool" };
    double now = GetTime();
    float x = 112;
    int i;

    ui_group((Rectangle){ 10, y, 980, 86 }, NULL, UI_TITLE);
    if (ui_tally((Rectangle){ 22, y + 11, 64, 64 }, "<", "RETURN", false,
                 TONE_NONE, true))
        app_back_to_search();

    for (i = 0; i < 8; i++) {
        char lbl[4];
        Rectangle r = { x + 74.0f * (float)i, y + 11, 64, 64 };
        bool on = false, en = (i == 0 || i == 1 || i == 2 || i == 6);

        snprintf(lbl, sizeof lbl, "F%d", i + 1);
        if (i == 0) on = S.dlg == DLG_PGMOUT;
        if (i == 1) on = now < S.f2_until;
        if (i == 2) on = S.f3;
        if (i == 6) on = S.f7;
        if (ui_tally(r, lbl, fsub[i], on, TONE_GREEN, en)) {
            switch (i) {
            case 0: S.dlg = DLG_PGMOUT; break;
            case 1: S.f2_until = now + 0.2; h5_f2(sess()); break;
            case 2: S.f3 = !S.f3; h5_f3(sess()); break;
            case 6: S.f7 = !S.f7; h5_f7(sess()); break;
            }
        }
        ui_tooltip(r, ftip[i]);
    }
    {
        Rectangle r = { x + 74.0f * 8 + 12, y + 11, 64, 64 };
        bool blink = S.ftb && ((int)(now / 0.4) & 1) == 0;   /* 400 ms timer */
        if (ui_tally(r, "FTB", "BLACK", blink, TONE_RED, true)) {
            S.ftb = !S.ftb;
            h5_ftb(sess());
        }
        ui_tooltip(r, "Fade to black (0x85)");
        r.x += 84;
        if (ui_tally(r, "DS", "SYNC", false, TONE_NONE, true))
            h5_read_state(sess());
        ui_tooltip(r, "Sync: read the full device state (0x49); needed after "
                      "using the switcher's own panel");
    }
}

static void draw_middle(float y)
{
    Rectangle ga = { 10, y, 430, 230 };
    Rectangle gt = { 450, y, 330, 230 };
    Rectangle gf = { 790, y, 200, 230 };
    bool srcs = S.audio_mode != 0;            /* AFV disables source combos */
    float cx;
    ui_font_t tf;
    static const struct { int id; const char *lbl, *sub; float dx, dy; } tr[] = {
        { H5_TR_UP,    "UP",    "WIPE", 0, -58 },
        { H5_TR_LEFT,  "LEFT",  "WIPE", -66, 0 },
        { H5_TR_RIGHT, "RIGHT", "WIPE", 66, 0 },
        { H5_TR_DOWN,  "DOWN",  "WIPE", 0, 58 },
    };
    int i;

    /* AUDIO */
    ui_group(ga, "AUDIO", UI_TITLE);
    if (ui_tally((Rectangle){ ga.x + 18, ga.y + 40, 64, 64 }, "AFV", "follow",
                 S.audio_mode == 0, TONE_GREEN, true)) {
        S.audio_mode = 0;
        h5_afv(sess());
    }
    if (ui_tally((Rectangle){ ga.x + 18, ga.y + 120, 64, 64 }, "MIX", "audio",
                 S.audio_mode == 1, TONE_GREEN, true)) {
        S.audio_mode = 1;
        h5_mix(sess());
    }
    ui_text(FONT_UI, "CH1", ga.x + 104, ga.y + 44, UI_ACCENT);
    ui_text(FONT_UI, "CH1_VOL", ga.x + 280, ga.y + 44, UI_ACCENT);
    ui_text(FONT_UI, "CH2", ga.x + 104, ga.y + 124, UI_ACCENT);
    ui_text(FONT_UI, "CH2_VOL", ga.x + 280, ga.y + 124, UI_ACCENT);
    if (ui_spinner(31, (Rectangle){ ga.x + 280, ga.y + 66, 132, 30 }, &S.vol1,
                   0, 100, 5, true))
        h5_ch_volume(sess(), 1, (uint8_t)S.vol1);
    if (ui_spinner(32, (Rectangle){ ga.x + 280, ga.y + 146, 132, 30 }, &S.vol2,
                   0, 100, 5, true))
        h5_ch_volume(sess(), 2, (uint8_t)S.vol2);
    ui_text(FONT_SMALL,
            S.audio_mode == 0 ? "Audio follows video (sources locked)"
            : S.audio_mode == 1 ? "Audio mixing" : "Audio mode unknown",
            ga.x + 104, ga.y + 196, UI_TEXT_DIM);

    /* TRANSITIONS: one label size for the whole group, set by the longest
     * label in the smallest button */
    tf = ui_fit_font("RIGHT", 62, FONT_BIG);
    ui_group(gt, "TRANSITIONS", UI_TITLE);
    if (ui_tally_font((Rectangle){ gt.x + 18, gt.y + 40, 64, 64 }, "MIX", NULL,
                      S.trans == H5_TR_MIX, TONE_GREEN, true, tf)) {
        S.trans = H5_TR_MIX;
        h5_transition(sess(), H5_TR_MIX);
    }
    if (ui_tally_font((Rectangle){ gt.x + 18, gt.y + 120, 64, 64 }, "FADE", NULL,
                      S.trans == H5_TR_FADE, TONE_GREEN, true, tf)) {
        S.trans = H5_TR_FADE;
        h5_transition(sess(), H5_TR_FADE);
    }
    /* wipe directions as a cross, centred below the group title */
    cx = gt.x + 210;
    for (i = 0; i < 4; i++) {
        Rectangle r = { cx - 31 + tr[i].dx, gt.y + 130 - 27 + tr[i].dy, 62, 54 };
        if (ui_tally_font(r, tr[i].lbl, tr[i].sub, S.trans == tr[i].id,
                          TONE_GREEN, true, tf)) {
            S.trans = tr[i].id;
            h5_transition(sess(), tr[i].id);
        }
    }

    /* FUNC */
    tf = ui_fit_font("STILL", 70, FONT_BIG);
    ui_group(gf, "FUNC", UI_TITLE);
    if (ui_tally_font((Rectangle){ gf.x + 22, gf.y + 40, 70, 70 }, "PIP", NULL,
                      S.pip, TONE_GREEN, true, tf)) {
        S.pip = !S.pip;                        /* on_btnclicked_PIP */
        h5_pip(sess(), S.pip);
    }
    if (ui_tally_font((Rectangle){ gf.x + 108, gf.y + 40, 70, 70 }, "STILL", NULL,
                      S.still, TONE_GREEN, true, tf)) {
        S.still = !S.still;                    /* on_btnclicked_STILL */
        h5_still(sess());
    }
    ui_text(FONT_SMALL, "PIP opens the PIP", gf.x + 22, gf.y + 130, UI_TEXT_DIM);
    ui_text(FONT_SMALL, "settings panel", gf.x + 22, gf.y + 148, UI_TEXT_DIM);

    if (ui_dropdown(33, (Rectangle){ ga.x + 104, ga.y + 66, 160, 30 },
                    h5_source_names, H5_NUM_SOURCES, &S.ch1, srcs))
        h5_ch_source(sess(), 1, S.ch1);
    if (ui_dropdown(34, (Rectangle){ ga.x + 104, ga.y + 146, 160, 30 },
                    h5_source_names, H5_NUM_SOURCES, &S.ch2, srcs))
        h5_ch_source(sess(), 2, S.ch2);
}

static void draw_below(float y)
{
    static const int ids[5] = { 1, 2, 3, 4, 9 };
    static const char *lbl[5] = { "1", "2", "3", "4", "BAR" };
    Rectangle gp = { 10, y, 400, 120 };
    Rectangle gv = { 420, y, 400, 120 };
    Rectangle gc = { 830, y, 160, 160 };
    double now = GetTime();
    int i;

    ui_group(gp, "PGM", UI_RED);
    ui_group(gv, "PVW", UI_GREEN);
    for (i = 0; i < 5; i++) {
        Rectangle rp = { gp.x + 20 + 74.0f * (float)i, gp.y + 38, 64, 64 };
        Rectangle rv = { gv.x + 20 + 74.0f * (float)i, gv.y + 38, 64, 64 };
        if (ui_tally(rp, lbl[i], NULL, S.pgm == ids[i], TONE_RED, true)) {
            S.pgm = ids[i];
            h5_pgm(sess(), (uint8_t)ids[i]);
        }
        if (ui_tally(rv, lbl[i], NULL, S.pvw == ids[i], TONE_GREEN, true)) {
            S.pvw = ids[i];
            h5_pvw(sess(), (uint8_t)ids[i]);
        }
    }

    ui_group(gc, NULL, UI_TITLE);
    if (ui_tally((Rectangle){ gc.x + 10, gc.y + 12, 66, 66 }, "CUT", NULL,
                 now < S.cut_until, TONE_RED, true)) {
        S.cut_until = now + 0.3;               /* timercut 300 ms */
        h5_cut(sess());
        swap_pgm_pvw();
    }
    if (ui_tally((Rectangle){ gc.x + 84, gc.y + 12, 66, 66 }, "AUTO", NULL,
                 S.auto_at != 0, TONE_RED, S.auto_at == 0)) {
        h5_auto(sess());
        S.auto_at = now + 0.9;                 /* swap after 900 ms */
    }
    for (i = 0; i < 3; i++) {
        char l[8];
        Rectangle r = { gc.x + 10 + 48.0f * (float)i, gc.y + 96, 44, 50 };
        snprintf(l, sizeof l, "R%d", i + 1);
        if (ui_tally(r, l, "RATE", S.rate == i, TONE_GREEN, true)) {
            S.rate = i;
            h5_rate(sess(), (uint8_t)i);
        }
        ui_tooltip(r, "Transition rate (0xB1)");
    }
}

static void draw_pip_panel(void)
{
    Rectangle g = { BASE_W, 50, PIP_W - 10, 540 };
    float x = g.x + 16, y = g.y + 44;
    int hmax, vmax;

    ui_group(g, "PIP", UI_TITLE);
    h5_pip_range(S.pip_size, &hmax, &vmax);

    ui_text(FONT_UI, "H_Position", x, y, UI_TEXT);
    if (ui_spinner(40, (Rectangle){ x, y + 22, g.width - 32, 30 }, &S.pip_h, 0,
                   hmax, 10, true))
        h5_pip_h(sess(), (uint16_t)S.pip_h);
    y += 72;
    ui_text(FONT_UI, "V_Position", x, y, UI_TEXT);
    if (ui_spinner(41, (Rectangle){ x, y + 22, g.width - 32, 30 }, &S.pip_v, 0,
                   vmax, 10, true))
        h5_pip_v(sess(), (uint16_t)S.pip_v);
    y += 72;
    ui_text(FONT_UI, "Size", x, y, UI_TEXT);
    y += 72;
    ui_text(FONT_UI, "Border_Enable", x, y, UI_TEXT);
    y += 72;
    ui_text(FONT_UI, "Border_Width", x, y, UI_TEXT);
    if (ui_spinner(42, (Rectangle){ x, y + 22, g.width - 32, 30 }, &S.pip_bw, 2,
                   7, 1, true))
        h5_pip_border_width(sess(), (uint8_t)S.pip_bw);
    y += 72;
    ui_text(FONT_UI, "Border_Color", x, y, UI_TEXT);

    if (ui_dropdown(45, (Rectangle){ x, y + 22, g.width - 32, 30 }, color_items,
                    4, &S.pip_bcol, true))
        h5_pip_border_color(sess(), (uint8_t)S.pip_bcol);
    y -= 144;
    if (ui_dropdown(44, (Rectangle){ x, y + 22, g.width - 32, 30 }, onoff_items,
                    2, &S.pip_ben, true))
        h5_pip_border_enable(sess(), (uint8_t)S.pip_ben);
    y -= 72;
    if (ui_dropdown(43, (Rectangle){ x, y + 22, g.width - 32, 30 }, size_items,
                    3, &S.pip_size, true)) {
        /* Plane_Right::on_coboxIndexChanged_Size: re-range, then 0xA2 */
        h5_pip_range(S.pip_size, &hmax, &vmax);
        if (S.pip_h > hmax) S.pip_h = hmax;
        if (S.pip_v > vmax) S.pip_v = vmax;
        h5_pip_size(sess(), (uint8_t)S.pip_size);
    }
}

void h5_screen_draw(void)
{
    char sub[96], ip[16];
    const char *model = sw_model_name(sess()->model);

    app_set_window(S.pip ? BASE_W + PIP_W : BASE_W, WIN_H);

    ui_layer(0);
    sw_ip_str(sess()->ip, ip);
    if (S.last_status > 0)
        snprintf(sub, sizeof sub, "%s%s   state read %s", ip,
                 sess()->bcast ? " [broadcast]" : "", S.status_time);
    else if (GetTime() - S.opened_at > 2.0 && !sess()->bcast)
        snprintf(sub, sizeof sub, "%s   no reply - if ARP fails, try --broadcast-control", ip);
    else
        snprintf(sub, sizeof sub, "%s%s   waiting for device state...", ip,
                 sess()->bcast ? " [broadcast]" : "");
    app_draw_header(model ? model : "HDS7105", sub);
    if (ui_button((Rectangle){ (float)GetScreenWidth() - 120, 6, 108, 30 },
                  "Settings", true))
        S.dlg = DLG_SETTINGS;

    draw_upper(48);
    draw_below(394);
    if (S.pip)
        draw_pip_panel();
    draw_middle(150);
    app_draw_footer(WIN_H - 40);
    ui_end_layer();

    if (S.dlg != DLG_NONE) {
        ui_layer(1);
        if (S.dlg == DLG_SETTINGS)
            draw_settings();
        else
            draw_pgmout();
        ui_end_layer();
    }
}
