/*
 * screen_search.c - device discovery window (showsearch + GroupBox_right).
 */

#include "app.h"
#include "proto.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define W 640
#define H 480

static const char *const model_items[] = { "HDS7105", "HDS7106" };

static void open_selected(void)
{
    if (g_app.selected < 0 || g_app.selected >= g_app.devs.count)
        return;
    app_open_device(&g_app.devs.dev[g_app.selected]);
}

void search_draw(void)
{
    Rectangle list = { 20, 64, 420, 300 };
    float bx = 460, by = 64;
    int i, shown = 0, model_dd = g_app.manual_model;
    char status[256];
    bool searching = GetTime() - g_app.search_at < 1.5;

    app_set_window(W, H);
    ui_layer(0);
    app_draw_header("SWITCHER", "HDS7105 / HDS7106 control");

    /* device list */
    DrawRectangleRec(list, (Color){ 100, 100, 100, 255 });   /* rgb(100,100,100) */
    DrawRectangleLinesEx(list, 2, (Color){ 83, 83, 83, 255 });
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (i = 0; i < g_app.devs.count; i++) {
        const sw_device_t *d = &g_app.devs.dev[i];
        char left[64], right[64], ip[16], hip[16];
        Rectangle row;
        int r;

        if (!sw_filter_match(g_app.filter, d->model))
            continue;
        row = (Rectangle){ list.x + 2, list.y + 2 + 34.0f * (float)shown,
                           list.width - 4, 32 };
        shown++;
        sw_ip_str(d->ip, ip);
        snprintf(left, sizeof left, "%s: %s", sw_model_name(d->model), ip);
        if (d->has_his) {
            sw_ip_str(d->his_ip, hip);
            snprintf(right, sizeof right, "stream %s", hip);
        } else {
            right[0] = 0;
        }
        r = ui_list_row(row, left, right, g_app.selected == i);
        if (r)
            g_app.selected = i;
        if (r == 2)
            open_selected();
    }
    EndScissorMode();
    if (!shown)
        ui_text_center(FONT_UI, searching ? "Searching..." : "No device found",
                       list, (Color){ 40, 40, 40, 255 });

    /* filter + actions (GroupBox_right) */
    ui_group((Rectangle){ bx, by, 160, 300 }, NULL, UI_TITLE);
    if (ui_radio((Rectangle){ bx + 20, by + 18, 120, 26 }, "all",
                 g_app.filter == SW_FILTER_ALL))
        g_app.filter = SW_FILTER_ALL;
    if (ui_radio((Rectangle){ bx + 20, by + 50, 120, 26 }, "HDS7105",
                 g_app.filter == SW_FILTER_7105))
        g_app.filter = SW_FILTER_7105;
    if (ui_radio((Rectangle){ bx + 20, by + 82, 120, 26 }, "HDS7106",
                 g_app.filter == SW_FILTER_7106))
        g_app.filter = SW_FILTER_7106;

    if (ui_button((Rectangle){ bx + 15, by + 130, 130, 36 }, "SEARCH", true))
        app_search();
    if (ui_button((Rectangle){ bx + 15, by + 176, 130, 36 }, "Open",
                  g_app.selected >= 0 &&
                  sw_filter_match(g_app.filter,
                                  g_app.devs.dev[g_app.selected].model)))
        open_selected();
    if (ui_button((Rectangle){ bx + 15, by + 246, 130, 36 }, "Exit", true))
        g_app.screen = (app_screen_t)-1;   /* main loop exits */

    /* manual connection, for devices behind a router */
    ui_text(FONT_SMALL, "Device IP (unicast search or direct connect):", 20, 378,
            UI_TEXT_DIM);
    ui_textbox(1, (Rectangle){ 20, 398, 170, 32 }, g_app.manual_ip,
               sizeof g_app.manual_ip, true);
    if (ui_button((Rectangle){ 200, 398, 80, 32 }, "Probe", true)) {
        uint32_t ip;
        if (sw_ip_parse(g_app.manual_ip, &ip)) {
            sw_net_search_unicast(&g_app.net, ip);
            g_app.search_at = GetTime();
            ui_toast("Search frame sent to %s", g_app.manual_ip);
        } else {
            ui_message("Warning", "Invalid IP address: \"%s\"", g_app.manual_ip);
        }
    }
    ui_dropdown(2, (Rectangle){ 290, 398, 110, 32 }, model_items, 2, &model_dd,
                true);
    g_app.manual_model = model_dd;
    if (ui_button((Rectangle){ 410, 398, 100, 32 }, "Connect", true)) {
        uint32_t ip;
        if (sw_ip_parse(g_app.manual_ip, &ip)) {
            sw_device_t *d = sw_devlist_add_manual(
                &g_app.devs, ip,
                model_dd ? SW_MODEL_HDS7106 : SW_MODEL_HDS7105);
            if (d)
                app_open_device(d);
        } else {
            ui_message("Warning", "Invalid IP address: \"%s\"", g_app.manual_ip);
        }
    }
    ui_tooltip((Rectangle){ 200, 398, 80, 32 },
               "Send the discovery frame straight to this IP");
    ui_tooltip((Rectangle){ 410, 398, 100, 32 },
               "Open the control panel without a discovery reply");

    snprintf(status, sizeof status, "%d device(s)%s   %s", g_app.devs.count,
             searching ? "  - searching" : "", g_app.net_msg);
    ui_text(FONT_SMALL, status, 20, H - 30, UI_TEXT_DIM);
    ui_end_layer();
}
