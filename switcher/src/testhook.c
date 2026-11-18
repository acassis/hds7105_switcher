/*
 * testhook.c - scripted UI input for automated tests.
 */

#include "testhook.h"
#include "raylib.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* AutomationEventType values from raylib 5.5 rcore.c (not public). */
#define EV_MOUSE_BUTTON_UP   5
#define EV_MOUSE_BUTTON_DOWN 6
#define EV_MOUSE_POSITION    7

#define MAX_LINES 512

static char *lines[MAX_LINES];
static int nlines, pc, wait_frames, phase, x0, y0, x1, y1;
static bool active, done;
static char shot_path[256];

int testhook_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[512];

    if (!f)
        return -1;
    while (nlines < MAX_LINES && fgets(buf, sizeof buf, f)) {
        buf[strcspn(buf, "\r\n")] = 0;
        if (!buf[0] || buf[0] == '#')
            continue;
        lines[nlines++] = strdup(buf);
    }
    fclose(f);
    active = true;
    return 0;
}

static void ev(unsigned type, int a, int b)
{
    AutomationEvent e = { 0 };
    e.type = type;
    e.params[0] = a;
    e.params[1] = b;
    PlayAutomationEvent(e);
}

void testhook_step(void)
{
    char cmd[32];

    if (!active || done)
        return;
    if (wait_frames > 0) {
        wait_frames--;
        return;
    }
    if (phase) {                         /* finishing a click / drag */
        if (phase == 1) {                /* click: release */
            ev(EV_MOUSE_BUTTON_UP, 0, 0);
            phase = 0;
            wait_frames = 2;
        } else {                         /* drag: phase counts steps */
            int steps = 24, k = phase - 2;
            if (k < steps) {
                ev(EV_MOUSE_POSITION, x0 + (x1 - x0) * (k + 1) / steps,
                   y0 + (y1 - y0) * (k + 1) / steps);
                phase++;
            } else {
                ev(EV_MOUSE_BUTTON_UP, 0, 0);
                phase = 0;
                wait_frames = 2;
            }
        }
        return;
    }
    if (pc >= nlines) {
        done = true;
        return;
    }
    cmd[0] = 0;
    sscanf(lines[pc], "%31s", cmd);
    if (!strcmp(cmd, "wait")) {
        wait_frames = atoi(lines[pc] + 4);
    } else if (!strcmp(cmd, "click")) {
        sscanf(lines[pc] + 5, "%d %d", &x0, &y0);
        ev(EV_MOUSE_POSITION, x0, y0);
        ev(EV_MOUSE_BUTTON_DOWN, 0, 0);
        phase = 1;
    } else if (!strcmp(cmd, "drag")) {
        sscanf(lines[pc] + 4, "%d %d %d %d", &x0, &y0, &x1, &y1);
        ev(EV_MOUSE_POSITION, x0, y0);
        ev(EV_MOUSE_BUTTON_DOWN, 0, 0);
        phase = 2;
    } else if (!strcmp(cmd, "shot")) {
        snprintf(shot_path, sizeof shot_path, "%s", lines[pc] + 5);
    } else if (!strcmp(cmd, "quit")) {
        done = true;
    } else {
        fprintf(stderr, "testhook: unknown command: %s\n", lines[pc]);
    }
    printf("[script] %s\n", lines[pc]);
    fflush(stdout);
    pc++;
}

void testhook_after_draw(void)
{
    if (shot_path[0]) {
        Image img;
        rlDrawRenderBatchActive();       /* flush pending draws first */
        img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
        shot_path[0] = 0;
    }
}

bool testhook_done(void)
{
    return active && done;
}
