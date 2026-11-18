/*
 * main.c - SWITCHER for Linux: control panel for HDS7105 / HDS7106 video
 * switchers, reimplemented in C from the Windows SWITCHER.exe.
 */

#include "app.h"
#include "hds7105.h"
#include "proto.h"
#include "testhook.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

app_t g_app;

static int win_w, win_h;

/* ------------------------------------------------------------------ */

void app_log(const char *fmt, ...)
{
    char *line = g_app.log[g_app.log_head];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof g_app.log[0], fmt, ap);
    va_end(ap);
    g_app.log_head = (g_app.log_head + 1) % APP_LOG_LINES;
    if (g_app.log_count < APP_LOG_LINES)
        g_app.log_count++;
    if (g_app.verbose)
        printf("%8.3f %s\n", GetTime(), line);
}

static void net_log(void *user, const char *dir, uint32_t ip, uint16_t port,
                    const uint8_t *buf, size_t n)
{
    char hex[100], ips[16];

    (void)user;
    sw_ip_str(ip, ips);
    sw_hex(hex, sizeof hex, buf, n);
    app_log("%-3s %s:%u [%zu] %s", dir, ips, port, n, hex);
}

void app_set_window(int w, int h)
{
    bool recenter = h != win_h;      /* screen change, not the side panel */

    if (w == win_w && h == win_h)
        return;
    win_w = w;
    win_h = h;
    SetWindowSize(w, h);
    if (recenter) {
        int m = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(m) - w) / 2,
                          (GetMonitorHeight(m) - h) / 2);
    }
}

void app_search(void)
{
    sw_devlist_clear(&g_app.devs);
    g_app.selected = -1;
    if (!g_app.no_broadcast)
        sw_net_search_broadcast(&g_app.net);
    if (g_app.probe_ip)
        sw_net_search_unicast(&g_app.net, g_app.probe_ip);
    g_app.search_at = GetTime();
    g_app.resend_at = g_app.no_broadcast ? 0 : g_app.search_at + 0.5;
}

void app_open_device(const sw_device_t *d)
{
    sw_session_from_device(&g_app.sess, &g_app.net, d);
    if (g_app.bcast_control) {
        char b[16];
        g_app.sess.bcast = sw_net_bcast_for(d->ip);
        sw_ip_str(g_app.sess.bcast, b);
        app_log("broadcast-control: commands go to %s", b);
    }
    if (sw_model_is_7106(d->model)) {
        g_app.screen = SCREEN_7106;
        h6_screen_enter();
    } else {
        g_app.screen = SCREEN_7105;
        h5_screen_enter();
    }
}

/* Return closes the control window, rescans and shows the list again
 * (showsearch::set7105UI / set7106UI after exec()). */
void app_back_to_search(void)
{
    g_app.screen = SCREEN_SEARCH;
    g_app.time_sent_at = 0;          /* search replies are not answers */
    app_search();
}

/* Set the switcher's clock to this PC's local time, as DeviceWell's
 * "Time synchronization" tool does.  The switcher answers with its
 * discovery reply (see on_frame). */
void app_sync_time(void)
{
    time_t now = time(NULL);
    struct tm t;
    char ts[32], ip[16];

    localtime_r(&now, &t);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &t);
    sw_ip_str(g_app.sess.ip, ip);
    if (sw_tx_time(&g_app.sess, &t) < 0) {
        ui_message("Sync time", "Could not send the time to %s", ip);
        return;
    }
    g_app.time_sent_at = GetTime();
    snprintf(g_app.time_sent, sizeof g_app.time_sent, "%s", ts);
    app_log("time sync: sent %s (local time) to %s", ts, ip);
    ui_toast("Time %s sent to %s", ts, ip);
}

/* ------------------------------------------------------------------ */

void app_draw_time_button(float x, float y)
{
    Rectangle r = { x, y, 108, 30 };

    if (ui_button(r, "Sync time", true))
        app_sync_time();
    ui_tooltip(r, "Set the switcher's date and time to this PC's local time");
}

void app_draw_header(const char *title, const char *subtitle)
{
    float w = (float)GetScreenWidth();

    DrawRectangleGradientV(0, 0, (int)w, 42, (Color){ 72, 72, 72, 255 },
                           (Color){ 52, 52, 52, 255 });
    DrawLine(0, 42, (int)w, 42, (Color){ 30, 30, 30, 255 });
    ui_text(FONT_BIG, title, 16, (42 - ui_font_size(FONT_BIG)) / 2, UI_TEXT);
    if (subtitle)
        ui_text(FONT_UI, subtitle, 30 + ui_text_width(FONT_BIG, title),
                (42 - ui_font_size(FONT_UI)) / 2 + 1, UI_TEXT_DIM);
}

void app_draw_footer(float y)
{
    float w = (float)GetScreenWidth();
    const char *last = g_app.log_count
        ? g_app.log[(g_app.log_head + APP_LOG_LINES - 1) % APP_LOG_LINES] : "";

    DrawRectangle(0, (int)y, (int)w, GetScreenHeight() - (int)y, UI_PANEL_DARK);
    BeginScissorMode(10, (int)y, (int)w - 110, GetScreenHeight() - (int)y);
    ui_text(FONT_SMALL, last, 12, y + 12, UI_TEXT_DIM);
    EndScissorMode();
    if (ui_button((Rectangle){ w - 92, y + 6, 80, 28 },
                  g_app.show_log ? "Hide log" : "Log", true) ||
        IsKeyPressed(KEY_F12))
        g_app.show_log = !g_app.show_log;
}

static void draw_log_overlay(void)
{
    float w = (float)GetScreenWidth(), h = 260;
    float y0 = (float)GetScreenHeight() - 40 - h;
    int i, n = (int)(h / 17) - 1;

    if (!g_app.show_log || g_app.screen == SCREEN_SEARCH)
        return;
    DrawRectangle(0, (int)y0, (int)w, (int)h, (Color){ 12, 12, 12, 235 });
    DrawLine(0, (int)y0, (int)w, (int)y0, UI_BORDER);
    if (n > g_app.log_count)
        n = g_app.log_count;
    for (i = 0; i < n; i++) {
        int idx = (g_app.log_head + APP_LOG_LINES - n + i) % APP_LOG_LINES;
        const char *l = g_app.log[idx];
        Color c = strncmp(l, "TX", 2) == 0 ? (Color){ 120, 200, 255, 255 }
                : strncmp(l, "RX", 2) == 0 ? (Color){ 140, 230, 140, 255 }
                : UI_TEXT;
        ui_text(FONT_SMALL, l, 10, y0 + 6 + 17.0f * (float)i, c);
    }
}

/* ------------------------------------------------------------------ */

static void on_frame(void *user, uint32_t ip, uint16_t port, bool discovery,
                     const uint8_t *f, size_t n)
{
    (void)user;
    (void)port;
    /* A time-sync frame is answered with the discovery reply. */
    if (g_app.time_sent_at > 0 && ip == g_app.sess.ip && n > SW_HDR_LEN &&
        f[4] == SW_TYPE_SWITCHER && f[SW_HDR_LEN] == 0xFF) {
        g_app.time_sent_at = 0;
        app_log("time sync: switcher answered");
        ui_toast("Time %s sent; the switcher answered", g_app.time_sent);
    }
    /* The original parsed discovery replies only on its (ephemeral-port)
     * search socket and state replies only on the :7000 socket.  Accept a
     * discovery reply on :7000 too, as long as it is not a state reply. */
    if (discovery || !sw_is_status_reply(f, n))
        sw_devlist_handle(&g_app.devs, ip, f, n);
    if (!discovery && g_app.screen == SCREEN_7105)
        h5_screen_rx(ip, f, n);
}

static void usage(const char *argv0)
{
    printf("usage: %s [options]\n"
           "  --connect IP      open the control panel for IP directly\n"
           "  --model 7105|7106 model for --connect (default 7105)\n"
           "  --probe IP        also send the discovery frame to IP (unicast)\n"
           "  --no-broadcast    never broadcast (only --probe / manual IPs)\n"
           "  --broadcast-control  send commands to the subnet broadcast\n"
           "                    address (for devices that do not answer ARP)\n"
           "  --port N          device UDP port (default 7000)\n"
           "  --verbose         print every frame to stdout\n"
           "  --script FILE     run a UI test script (see tests/)\n"
           "  --hidden          hidden window (with --script)\n",
           argv0);
}

int main(int argc, char **argv)
{
    const char *connect_ip = NULL, *probe_ip = NULL, *script = NULL;
    int model = SW_MODEL_HDS7105, port = SW_PORT, i;
    bool hidden = false;
    char err[160];

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--connect") && v) { connect_ip = v; i++; }
        else if (!strcmp(a, "--model") && v) {
            model = !strcmp(v, "7106") ? SW_MODEL_HDS7106 : SW_MODEL_HDS7105;
            i++;
        }
        else if (!strcmp(a, "--probe") && v) { probe_ip = v; i++; }
        else if (!strcmp(a, "--port") && v) { port = atoi(v); i++; }
        else if (!strcmp(a, "--verbose")) g_app.verbose = true;
        else if (!strcmp(a, "--no-broadcast")) g_app.no_broadcast = true;
        else if (!strcmp(a, "--broadcast-control")) g_app.bcast_control = true;
        else if (!strcmp(a, "--script") && v) { script = v; i++; }
        else if (!strcmp(a, "--hidden")) hidden = true;
        else { usage(argv[0]); return strcmp(a, "--help") ? 2 : 0; }
    }

    g_app.selected = -1;
    if (sw_net_open(&g_app.net, err, sizeof err) != 0) {
        fprintf(stderr, "switcher: network: %s\n", err);
        return 1;
    }
    g_app.net.dev_port = (uint16_t)port;
    g_app.net.log = net_log;
    snprintf(g_app.net_msg, sizeof g_app.net_msg, "%s",
             err[0] ? err : "UDP port 7000 ready");
    if (err[0])
        app_log("%s", err);

    if (!g_app.verbose)
        SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT |
                   (hidden ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(640, 480, "SWITCHER - HDS7105 / HDS7106");
    win_w = 640;
    win_h = 480;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    ui_init();
    if (script && testhook_load(script) != 0) {
        fprintf(stderr, "switcher: cannot read script %s\n", script);
        return 1;
    }

    if (probe_ip) {
        if (!sw_ip_parse(probe_ip, &g_app.probe_ip)) {
            fprintf(stderr, "switcher: bad IP %s\n", probe_ip);
            return 2;
        }
        snprintf(g_app.manual_ip, sizeof g_app.manual_ip, "%s", probe_ip);
    }
    app_search();                    /* SWITCHER_DATA ctor broadcasts */
    if (connect_ip) {
        uint32_t ip;
        sw_device_t *d;
        if (!sw_ip_parse(connect_ip, &ip)) {
            fprintf(stderr, "switcher: bad IP %s\n", connect_ip);
            return 2;
        }
        d = sw_devlist_add_manual(&g_app.devs, ip, (uint8_t)model);
        if (d)
            app_open_device(d);
    }

    while (!WindowShouldClose() && (int)g_app.screen >= 0) {
        bool modal;

        testhook_step();
        sw_net_poll(&g_app.net, on_frame, NULL);
        if (g_app.resend_at && GetTime() >= g_app.resend_at) {
            sw_net_search_broadcast(&g_app.net);
            g_app.resend_at = 0;
        }
        if (g_app.time_sent_at > 0 && GetTime() - g_app.time_sent_at > 3.0) {
            g_app.time_sent_at = 0;
            app_log("time sync: no answer within 3 s");
            ui_toast("No answer to the time sync; check the connection (Log)");
        }
        if (g_app.screen == SCREEN_7105) h5_screen_tick();
        if (g_app.screen == SCREEN_7106) h6_screen_tick();

        modal = (g_app.screen == SCREEN_7105 && h5_screen_modal()) ||
                (g_app.screen == SCREEN_7106 && h6_screen_modal());

        BeginDrawing();
        ClearBackground(UI_BG);
        ui_begin_frame(modal);
        switch (g_app.screen) {
        case SCREEN_SEARCH: search_draw(); break;
        case SCREEN_7105:   h5_screen_draw(); break;
        case SCREEN_7106:   h6_screen_draw(); break;
        }
        ui_layer(modal ? 1 : 0);
        draw_log_overlay();
        ui_end_frame();
        testhook_after_draw();
        EndDrawing();
        if (testhook_done())
            break;
    }

    ui_shutdown();
    CloseWindow();
    sw_net_close(&g_app.net);
    return 0;
}
