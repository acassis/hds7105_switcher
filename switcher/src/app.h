/*
 * app.h - application state shared by the screens.
 */

#ifndef SWITCHER_APP_H
#define SWITCHER_APP_H

#include "devices.h"
#include "net.h"
#include "session.h"

#define APP_LOG_LINES 200

typedef enum { SCREEN_SEARCH, SCREEN_7105, SCREEN_7106 } app_screen_t;

typedef struct {
    sw_net_t net;
    char net_msg[160];

    sw_devlist_t devs;
    sw_filter_t filter;
    int selected;                    /* index into devs.dev, -1 none */
    double search_at;                /* last broadcast time          */
    double resend_at;                /* second broadcast, 0 = none   */
    char manual_ip[32];
    int manual_model;                /* 0 = 7105, 1 = 7106           */

    app_screen_t screen;
    sw_session_t sess;

    bool verbose;                    /* --verbose: frames to stdout  */
    bool no_broadcast;               /* --no-broadcast (testing)     */
    bool bcast_control;              /* --broadcast-control          */
    uint32_t probe_ip;               /* --probe: re-sent on SEARCH   */
    double time_sent_at;             /* Sync time: waiting for reply */
    char time_sent[32];              /* the local time that was sent */
    bool show_log;
    char log[APP_LOG_LINES][128];
    int log_head, log_count;
} app_t;

extern app_t g_app;

void app_log(const char *fmt, ...);
void app_search(void);               /* clear list + broadcast       */
void app_open_device(const sw_device_t *d);
void app_back_to_search(void);
void app_sync_time(void);            /* PC local time -> switcher    */
void app_set_window(int w, int h);

/* Screens */
void search_draw(void);

void h5_screen_enter(void);
bool h5_screen_modal(void);
void h5_screen_tick(void);
void h5_screen_draw(void);
void h5_screen_rx(uint32_t ip, const uint8_t *f, size_t n);

void h6_screen_enter(void);
bool h6_screen_modal(void);
void h6_screen_tick(void);
void h6_screen_draw(void);

/* Shared bits of chrome */
void app_draw_header(const char *title, const char *subtitle);
void app_draw_footer(float y);
void app_draw_time_button(float x, float y);   /* "Sync time", 108x30 */

#endif
