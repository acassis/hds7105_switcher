/*
 * ui.c - immediate-mode widgets for the SWITCHER control panel.
 */

#include "ui.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUND 0.18f
#define SEGS  8

static struct {
    Font font[4];
    float size[4];
    float spacing;
    bool loaded[4];

    int layer, top_layer;
    bool click_consumed;

    /* press-then-release button tracking */
    unsigned armed_key;
    int armed_layer;

    /* drop-down */
    int dd_id, dd_layer, dd_n, dd_changed_id;
    Rectangle dd_rect;
    const char *const *dd_items;
    int *dd_active;

    /* keyboard focus (text box / spinner edit) */
    int edit_id, edit_layer;
    Rectangle edit_rect;
    char num_buf[16];
    char commit_buf[16];
    int cursor;
    float scroll;
    int commit_id;               /* spinner edit finished by click-away */

    /* spinner auto-repeat */
    int rep_id, rep_dir;
    double rep_next;

    int drag_id;                 /* slider being dragged */

    double last_click;
    Rectangle last_click_rect;

    /* tooltip */
    const char *tip;
    Rectangle tip_rect;
    double tip_since;
    bool tip_seen;

    bool msg_open;
    char msg_title[64];
    char msg_text[600];

    char toast[200];
    double toast_until;
} U;

/* ---------------------------------------------------------------- */

static bool try_font(int slot, const char *const *paths, float size)
{
    const char *env = getenv(slot >= FONT_TITLE ? "SWITCHER_FONT_BOLD"
                                                : "SWITCHER_FONT");
    int i;

    for (i = -1;; i++) {
        const char *p = i < 0 ? env : paths[i];
        if (i >= 0 && !p)
            break;
        if (!p || !FileExists(p))
            continue;
        U.font[slot] = LoadFontEx(p, (int)size, NULL, 0);
        if (U.font[slot].texture.id) {
            SetTextureFilter(U.font[slot].texture, TEXTURE_FILTER_BILINEAR);
            U.loaded[slot] = true;
            U.size[slot] = size;
            return true;
        }
    }
    return false;
}

void ui_init(void)
{
    static const char *const regular[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        NULL
    };
    static const char *const bold[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        NULL
    };
    static const float sizes[4] = { 16, 13, 17, 24 };
    int i;

    memset(&U, 0, sizeof U);
    for (i = 0; i < 4; i++) {
        bool ok = try_font(i, i >= FONT_TITLE ? bold : regular, sizes[i]);
        if (!ok && i >= FONT_TITLE)
            ok = try_font(i, regular, sizes[i]);
        if (!ok) {
            U.font[i] = GetFontDefault();
            U.size[i] = sizes[i];
        }
    }
    U.spacing = U.loaded[FONT_UI] ? 0.0f : 1.0f;
}

void ui_shutdown(void)
{
    int i;
    for (i = 0; i < 4; i++)
        if (U.loaded[i])
            UnloadFont(U.font[i]);
}

float ui_font_size(ui_font_t f) { return U.size[f]; }

float ui_text_width(ui_font_t f, const char *s)
{
    return MeasureTextEx(U.font[f], s, U.size[f], U.spacing).x;
}

void ui_text(ui_font_t f, const char *s, float x, float y, Color c)
{
    DrawTextEx(U.font[f], s, (Vector2){ (float)(int)x, (float)(int)y },
               U.size[f], U.spacing, c);
}

void ui_text_center(ui_font_t f, const char *s, Rectangle r, Color c)
{
    Vector2 m = MeasureTextEx(U.font[f], s, U.size[f], U.spacing);
    ui_text(f, s, r.x + (r.width - m.x) / 2, r.y + (r.height - m.y) / 2, c);
}

void ui_text_right(ui_font_t f, const char *s, float right, float y, Color c)
{
    ui_text(f, s, right - ui_text_width(f, s), y, c);
}

/* ---------------------------------------------------------------- */

static bool can_input(void)
{
    return U.layer == U.top_layer && U.dd_id == 0 && !U.click_consumed;
}

bool ui_hovered(Rectangle r)
{
    return can_input() && CheckCollisionPointRec(GetMousePosition(), r);
}

static unsigned key_of(Rectangle r)
{
    return (unsigned)((int)r.x * 7919 + (int)r.y * 104729 +
                      (int)r.width * 31 + U.layer * 3 + 1);
}

/* Qt-style click: press inside, release inside. */
static bool clicked(Rectangle r, bool *pressed_now)
{
    bool over = ui_hovered(r);
    unsigned k = key_of(r);

    *pressed_now = false;
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        U.armed_key = k;
        U.armed_layer = U.layer;
    }
    if (U.armed_key == k && U.armed_layer == U.layer) {
        *pressed_now = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            U.armed_key = 0;
            return over;
        }
    }
    return false;
}

static Rectangle dd_list_rect(void)
{
    float rowh = U.dd_rect.height;
    Rectangle l = { U.dd_rect.x, U.dd_rect.y + U.dd_rect.height,
                    U.dd_rect.width, rowh * (float)U.dd_n };
    if (l.y + l.height > (float)GetScreenHeight())
        l.y = U.dd_rect.y - l.height;
    if (l.y < 0)
        l.y = 0;
    return l;
}

void ui_begin_frame(bool modal_open)
{
    bool press = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 m = GetMousePosition();

    U.top_layer = U.msg_open ? 2 : (modal_open ? 1 : 0);
    U.click_consumed = false;
    U.tip = NULL;

    /* open drop-down list gets the click */
    if (U.dd_id) {
        bool close = IsKeyPressed(KEY_ESCAPE) || U.dd_layer < U.top_layer;
        if (press) {
            Rectangle l = dd_list_rect();
            if (CheckCollisionPointRec(m, l)) {
                int idx = (int)((m.y - l.y) / U.dd_rect.height);
                if (idx >= 0 && idx < U.dd_n && U.dd_active &&
                    *U.dd_active != idx) {
                    *U.dd_active = idx;
                    U.dd_changed_id = U.dd_id;
                }
            }
            close = true;
            U.click_consumed = true;
        }
        if (close)
            U.dd_id = 0;
    }

    /* clicking elsewhere ends text/number editing */
    if (U.edit_id && ((press && !CheckCollisionPointRec(m, U.edit_rect)) ||
                      U.edit_layer < U.top_layer)) {
        memcpy(U.commit_buf, U.num_buf, sizeof U.commit_buf);
        U.num_buf[0] = 0;
        U.commit_id = U.edit_id;
        U.edit_id = 0;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        U.drag_id = 0;
        U.rep_id = 0;
    }
}

void ui_layer(int layer)
{
    U.layer = layer;
}

void ui_end_layer(void)
{
    Rectangle l;
    int i;

    if (!U.dd_id || U.dd_layer != U.layer)
        return;
    l = dd_list_rect();
    DrawRectangleRec((Rectangle){ l.x + 3, l.y + 3, l.width, l.height },
                     (Color){ 0, 0, 0, 90 });
    DrawRectangleRec(l, UI_PANEL_DARK);
    for (i = 0; i < U.dd_n; i++) {
        Rectangle row = { l.x, l.y + U.dd_rect.height * (float)i, l.width,
                          U.dd_rect.height };
        bool hot = CheckCollisionPointRec(GetMousePosition(), row);
        bool cur = U.dd_active && *U.dd_active == i;
        if (hot)
            DrawRectangleRec(row, UI_ACCENT);
        else if (cur)
            DrawRectangleRec(row, UI_BTN);
        ui_text(FONT_UI, U.dd_items[i], row.x + 8,
                row.y + (row.height - U.size[FONT_UI]) / 2, UI_TEXT);
    }
    DrawRectangleLinesEx(l, 1, UI_BORDER);
}

static void draw_wrapped(const char *s, Rectangle r)
{
    char line[200];
    float y = r.y;
    const char *p = s;

    while (*p && y < r.y + r.height) {
        size_t n = 0, last_space = 0;
        while (p[n] && p[n] != '\n' && n < sizeof line - 1) {
            line[n] = p[n];
            if (p[n] == ' ')
                last_space = n;
            n++;
            line[n] = 0;
            if (ui_text_width(FONT_UI, line) > r.width && last_space) {
                n = last_space;
                break;
            }
        }
        line[n] = 0;
        ui_text(FONT_UI, line, r.x, y, UI_TEXT);
        y += U.size[FONT_UI] + 6;
        p += n;
        while (*p == ' ' || *p == '\n')
            p++;
    }
}

void ui_end_frame(void)
{
    double now = GetTime();

    if (U.msg_open) {
        Rectangle c;
        ui_layer(2);
        c = ui_dialog(420, 190, U.msg_title);
        draw_wrapped(U.msg_text, (Rectangle){ c.x + 16, c.y + 14,
                                              c.width - 32, c.height - 70 });
        if (ui_button((Rectangle){ c.x + c.width - 106, c.y + c.height - 46,
                                   90, 32 }, "OK", true) ||
            IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
            IsKeyPressed(KEY_ESCAPE))
            U.msg_open = false;
    }

    if (U.toast[0] && now < U.toast_until) {
        float w = ui_text_width(FONT_UI, U.toast) + 28;
        Rectangle r = { ((float)GetScreenWidth() - w) / 2,
                        (float)GetScreenHeight() - 70, w, 34 };
        DrawRectangleRounded(r, 0.4f, SEGS, (Color){ 20, 20, 20, 235 });
        DrawRectangleRoundedLinesEx(r, 0.4f, SEGS, 1, UI_BORDER);
        ui_text_center(FONT_UI, U.toast, r, UI_TEXT);
    }

    if (U.tip && now - U.tip_since > 0.6) {
        Vector2 m = GetMousePosition();
        float w = ui_text_width(FONT_SMALL, U.tip) + 12;
        Rectangle r = { m.x + 14, m.y + 18, w, U.size[FONT_SMALL] + 8 };
        if (r.x + r.width > (float)GetScreenWidth())
            r.x = (float)GetScreenWidth() - r.width - 2;
        if (r.y + r.height > (float)GetScreenHeight())
            r.y = m.y - r.height - 4;
        DrawRectangleRec(r, (Color){ 250, 250, 210, 245 });
        ui_text(FONT_SMALL, U.tip, r.x + 6, r.y + 4, BLACK);
    }
    if (!U.tip)
        U.tip_seen = false;
}

/* ---------------------------------------------------------------- */

void ui_panel(Rectangle r, Color fill)
{
    DrawRectangleRounded(r, 0.04f, SEGS, fill);
}

void ui_group(Rectangle r, const char *title, Color title_color)
{
    float roundness = 24.0f / (r.width < r.height ? r.width : r.height);

    DrawRectangleRounded(r, roundness, SEGS, UI_PANEL);
    DrawRectangleRoundedLinesEx(r, roundness, SEGS, 2, UI_BORDER);
    if (title)
        ui_text_center(FONT_TITLE, title,
                       (Rectangle){ r.x, r.y + 4, r.width, 22 }, title_color);
}

void ui_dim(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 0, 0, 0, 150 });
}

Rectangle ui_dialog(float w, float h, const char *title)
{
    Rectangle r = { ((float)GetScreenWidth() - w) / 2,
                    ((float)GetScreenHeight() - h) / 2, w, h };
    Rectangle bar = { r.x, r.y, r.width, 34 };

    ui_dim();
    DrawRectangleRec((Rectangle){ r.x + 5, r.y + 5, w, h },
                     (Color){ 0, 0, 0, 120 });
    DrawRectangleRec(r, UI_PANEL);
    DrawRectangleRec(bar, (Color){ 64, 64, 64, 255 });
    DrawLineEx((Vector2){ r.x, r.y + 34 }, (Vector2){ r.x + w, r.y + 34 }, 1,
               UI_BORDER);
    DrawRectangleLinesEx(r, 1, UI_BORDER);
    if (title)
        ui_text(FONT_TITLE, title, r.x + 12, r.y + (34 - U.size[FONT_TITLE]) / 2,
                UI_TEXT);
    return (Rectangle){ r.x, r.y + 35, r.width, r.height - 35 };
}

void ui_tooltip(Rectangle r, const char *text)
{
    if (!ui_hovered(r))
        return;
    if (!U.tip_seen || U.tip_rect.x != r.x || U.tip_rect.y != r.y) {
        U.tip_since = GetTime();
        U.tip_rect = r;
        U.tip_seen = true;
    }
    U.tip = text;
}

/* ---------------------------------------------------------------- */

bool ui_button(Rectangle r, const char *label, bool enabled)
{
    bool down = false, hit = enabled && clicked(r, &down);
    bool hot = enabled && ui_hovered(r);
    Color fill = !enabled ? UI_PANEL_DARK : down ? UI_BTN_DOWN
               : hot ? UI_BTN_HOVER : UI_BTN;

    DrawRectangleRounded(r, ROUND, SEGS, fill);
    DrawRectangleRoundedLinesEx(r, ROUND, SEGS, 1,
                                hot ? (Color){ 189, 189, 189, 255 }
                                    : (Color){ 100, 100, 100, 255 });
    ui_text_center(FONT_UI, label, r, enabled ? UI_TEXT : UI_TEXT_DIM);
    return hit;
}

static Color tone_color(ui_tone_t t)
{
    switch (t) {
    case TONE_RED:   return UI_RED;
    case TONE_GREEN: return UI_GREEN;
    case TONE_AMBER: return UI_AMBER;
    default:         return UI_BTN_DOWN;
    }
}

static ui_font_t fit_font(const char *s, float maxw, ui_font_t start)
{
    static const ui_font_t order[] = { FONT_BIG, FONT_TITLE, FONT_UI, FONT_SMALL };
    int i = 0;

    while (i < 3 && order[i] != start)
        i++;
    for (; i < 3; i++)
        if (ui_text_width(order[i], s) <= maxw)
            break;
    return order[i];
}

ui_font_t ui_fit_font(const char *s, float width, ui_font_t start)
{
    return fit_font(s, width - 10, start);
}

bool ui_tally(Rectangle r, const char *label, const char *sub, bool on,
              ui_tone_t tone, bool enabled)
{
    return ui_tally_font(r, label, sub, on, tone, enabled,
                         r.height >= 50 ? FONT_BIG : FONT_TITLE);
}

bool ui_tally_font(Rectangle r, const char *label, const char *sub, bool on,
                   ui_tone_t tone, bool enabled, ui_font_t font)
{
    bool down = false, hit = enabled && clicked(r, &down);
    bool hot = enabled && ui_hovered(r);
    Color lit = tone_color(tone);
    Color fill = on ? lit : !enabled ? UI_PANEL_DARK : (Color){ 46, 46, 46, 255 };
    Color edge = hot ? (tone == TONE_RED ? (Color){ 229, 97, 97, 255 }
                      : tone == TONE_GREEN ? (Color){ 60, 200, 50, 255 }
                      : (Color){ 189, 189, 189, 255 })
                     : on ? ColorBrightness(lit, 0.35f) : (Color){ 96, 96, 96, 255 };
    Color txt = !enabled ? UI_TEXT_DIM : on ? WHITE : UI_TEXT;

    if (down)
        fill = ColorBrightness(on ? lit : UI_BTN, 0.25f);
    DrawRectangleRounded(r, ROUND, SEGS, fill);
    DrawRectangleRoundedLinesEx(r, ROUND, SEGS, hot ? 2.0f : 1.0f, edge);
    if (sub && *sub) {
        Rectangle top = { r.x, r.y + 4, r.width, r.height * 0.6f };
        Rectangle bot = { r.x, r.y + r.height * 0.58f, r.width, r.height * 0.36f };
        ui_text_center(fit_font(label, r.width - 10, font), label, top, txt);
        ui_text_center(FONT_SMALL, sub, bot, on ? WHITE : UI_TEXT_DIM);
    } else {
        ui_text_center(fit_font(label, r.width - 10, font), label, r, txt);
    }
    return hit;
}

bool ui_radio(Rectangle r, const char *label, bool selected)
{
    bool down = false, hit = clicked(r, &down);
    Vector2 c = { r.x + 9, r.y + r.height / 2 };

    DrawCircleV(c, 8, UI_FIELD);
    DrawCircleLines((int)c.x, (int)c.y, 8, ui_hovered(r) ? UI_TEXT : UI_BORDER);
    if (selected)
        DrawCircleV(c, 4.5f, UI_ACCENT);
    ui_text(FONT_UI, label, r.x + 24, r.y + (r.height - U.size[FONT_UI]) / 2,
            UI_TEXT);
    return hit;
}

int ui_list_row(Rectangle r, const char *left, const char *right, bool selected)
{
    bool down = false, hit = clicked(r, &down);
    bool hot = ui_hovered(r);
    int res = 0;

    if (selected)
        DrawRectangleRec(r, UI_ACCENT);
    else if (hot)
        DrawRectangleRec(r, UI_BTN);
    ui_text(FONT_UI, left, r.x + 10, r.y + (r.height - U.size[FONT_UI]) / 2,
            UI_TEXT);
    if (right)
        ui_text_right(FONT_SMALL, right, r.x + r.width - 10,
                      r.y + (r.height - U.size[FONT_SMALL]) / 2,
                      selected ? UI_TEXT : UI_TEXT_DIM);
    if (hit) {
        double now = GetTime();
        res = 1;
        if (now - U.last_click < 0.4 && U.last_click_rect.y == r.y &&
            U.last_click_rect.x == r.x)
            res = 2;
        U.last_click = now;
        U.last_click_rect = r;
    }
    return res;
}

/* ---------------------------------------------------------------- */

bool ui_dropdown(int id, Rectangle r, const char *const *items, int n,
                 int *active, bool enabled)
{
    bool down = false;
    bool open = U.dd_id == id;
    bool hot = enabled && ui_hovered(r);
    bool changed = U.dd_changed_id == id;
    const char *txt = (*active >= 0 && *active < n) ? items[*active] : "---";
    Vector2 a;

    if (changed)
        U.dd_changed_id = 0;
    if (enabled && clicked(r, &down) && !open) {
        U.dd_id = id;
        U.dd_layer = U.layer;
        U.dd_rect = r;
        U.dd_items = items;
        U.dd_n = n;
        U.dd_active = active;
    }
    DrawRectangleRounded(r, 0.15f, SEGS, enabled ? UI_FIELD : UI_PANEL_DARK);
    DrawRectangleRoundedLinesEx(r, 0.15f, SEGS, 1,
                                open || hot ? UI_TEXT : (Color){ 100, 100, 100, 255 });
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width - 22, (int)r.height);
    ui_text(FONT_UI, txt, r.x + 8, r.y + (r.height - U.size[FONT_UI]) / 2,
            enabled ? UI_TEXT : UI_TEXT_DIM);
    EndScissorMode();
    a = (Vector2){ r.x + r.width - 14, r.y + r.height / 2 };
    DrawTriangle((Vector2){ a.x - 5, a.y - 3 }, (Vector2){ a.x, a.y + 3 },
                 (Vector2){ a.x + 5, a.y - 3 }, enabled ? UI_TEXT : UI_TEXT_DIM);
    return changed;
}

static bool focused(int id) { return U.edit_id == id; }

static void focus(int id, Rectangle r)
{
    U.edit_id = id;
    U.edit_layer = U.layer;
    U.edit_rect = r;
    U.commit_id = 0;
}

static bool commit_number(char *buf, int *value, int min, int max)
{
    long v;
    char *end;

    if (!buf[0])
        return false;
    v = strtol(buf, &end, 10);
    buf[0] = 0;
    if (v < min) v = min;
    if (v > max) v = max;
    if ((int)v == *value)
        return false;
    *value = (int)v;
    return true;
}

bool ui_spinner(int id, Rectangle r, int *value, int min, int max, int step,
                bool enabled)
{
    Rectangle minus = { r.x, r.y, r.height, r.height };
    Rectangle plus = { r.x + r.width - r.height, r.y, r.height, r.height };
    Rectangle mid = { r.x + r.height + 2, r.y, r.width - 2 * r.height - 4,
                      r.height };
    bool changed = false, d = false;
    int old = *value;
    char txt[24];
    int dir = 0;

    if (U.commit_id == id) {                  /* clicked away while editing */
        U.commit_id = 0;
        changed |= commit_number(U.commit_buf, value, min, max);
    }
    if (!enabled && focused(id))
        U.edit_id = 0;

    if (enabled) {
        if (clicked(minus, &d)) {}
        if (d && U.rep_id != id * 2) {
            U.rep_id = id * 2; U.rep_next = GetTime() + 0.4; dir = -1;
        } else if (d && GetTime() >= U.rep_next) {
            U.rep_next = GetTime() + 0.06; dir = -1;
        }
        if (clicked(plus, &d)) {}
        if (d && U.rep_id != id * 2 + 1) {
            U.rep_id = id * 2 + 1; U.rep_next = GetTime() + 0.4; dir = 1;
        } else if (d && GetTime() >= U.rep_next) {
            U.rep_next = GetTime() + 0.06; dir = 1;
        }
        if (!focused(id) && ui_hovered(r)) {
            float w = GetMouseWheelMove();
            if (w > 0) dir = 1;
            if (w < 0) dir = -1;
        }
        if (dir) {
            long v = (long)*value + dir * step;
            if (v < min) v = min;
            if (v > max) v = max;
            *value = (int)v;
            if (focused(id)) {
                U.edit_id = 0;
                U.num_buf[0] = 0;
            }
        }
        if (!focused(id) && clicked(mid, &d)) {
            focus(id, r);
            snprintf(U.num_buf, sizeof U.num_buf, "%d", *value);
        }
        if (focused(id)) {
            int ch;
            size_t len = strlen(U.num_buf);
            while ((ch = GetCharPressed()) != 0) {
                if (((ch >= '0' && ch <= '9') || (ch == '-' && len == 0 && min < 0))
                    && len < 7) {
                    U.num_buf[len++] = (char)ch;
                    U.num_buf[len] = 0;
                }
            }
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))
                && len)
                U.num_buf[--len] = 0;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                IsKeyPressed(KEY_TAB)) {
                changed |= commit_number(U.num_buf, value, min, max);
                U.edit_id = 0;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                U.num_buf[0] = 0;
                U.edit_id = 0;
            }
        }
    }
    changed |= *value != old;

    /* draw */
    DrawRectangleRounded(mid, 0.15f, SEGS,
                         enabled ? UI_FIELD : UI_PANEL_DARK);
    DrawRectangleRoundedLinesEx(mid, 0.15f, SEGS, 1,
                                focused(id) ? UI_ACCENT : (Color){ 100, 100, 100, 255 });
    if (focused(id))
        snprintf(txt, sizeof txt, "%s%s", U.num_buf,
                 ((int)(GetTime() * 2) & 1) ? "|" : "");
    else
        snprintf(txt, sizeof txt, "%d", *value);
    ui_text_center(FONT_UI, txt, mid, enabled ? UI_TEXT : UI_TEXT_DIM);
    {
        bool hm = enabled && ui_hovered(minus), hp = enabled && ui_hovered(plus);
        DrawRectangleRounded(minus, 0.2f, SEGS, hm ? UI_BTN_HOVER : UI_BTN);
        DrawRectangleRounded(plus, 0.2f, SEGS, hp ? UI_BTN_HOVER : UI_BTN);
        ui_text_center(FONT_TITLE, "-", minus, enabled ? UI_TEXT : UI_TEXT_DIM);
        ui_text_center(FONT_TITLE, "+", plus, enabled ? UI_TEXT : UI_TEXT_DIM);
    }
    return changed;
}

bool ui_textbox(int id, Rectangle r, char *buf, int cap, bool enabled)
{
    bool d = false, enter = false;
    int len = (int)strlen(buf);
    float tx, cx;
    char tmp;

    if (!enabled && focused(id))
        U.edit_id = 0;
    if (U.commit_id == id)
        U.commit_id = 0;
    if (enabled && !focused(id) && clicked(r, &d)) {
        focus(id, r);
        U.cursor = len;
        U.scroll = 0;
        U.num_buf[0] = 0;
    }
    if (enabled && focused(id)) {
        int ch;
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (U.cursor > len) U.cursor = len;
        while ((ch = GetCharPressed()) != 0) {
            if (ch >= 32 && ch < 127 && len < cap - 1) {
                memmove(buf + U.cursor + 1, buf + U.cursor, (size_t)(len - U.cursor + 1));
                buf[U.cursor++] = (char)ch;
                len++;
            }
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
            U.cursor > 0) {
            memmove(buf + U.cursor - 1, buf + U.cursor, (size_t)(len - U.cursor + 1));
            U.cursor--;
            len--;
        }
        if ((IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) &&
            U.cursor < len) {
            memmove(buf + U.cursor, buf + U.cursor + 1, (size_t)(len - U.cursor));
            len--;
        }
        if ((IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) && U.cursor > 0)
            U.cursor--;
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) && U.cursor < len)
            U.cursor++;
        if (IsKeyPressed(KEY_HOME)) U.cursor = 0;
        if (IsKeyPressed(KEY_END)) U.cursor = len;
        if (ctrl && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            for (; clip && *clip && len < cap - 1; clip++) {
                if (*clip < 32 || *clip > 126)
                    continue;
                memmove(buf + U.cursor + 1, buf + U.cursor, (size_t)(len - U.cursor + 1));
                buf[U.cursor++] = *clip;
                len++;
            }
        }
        if (ctrl && IsKeyPressed(KEY_C))
            SetClipboardText(buf);
        if (ctrl && IsKeyPressed(KEY_U)) {
            buf[0] = 0;
            len = U.cursor = 0;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            enter = true;
            U.edit_id = 0;
        } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
            U.edit_id = 0;
        }
    }

    DrawRectangleRounded(r, 0.15f, SEGS, enabled ? UI_FIELD : UI_PANEL_DARK);
    DrawRectangleRoundedLinesEx(r, 0.15f, SEGS, 1,
                                focused(id) ? UI_ACCENT : (Color){ 100, 100, 100, 255 });

    /* horizontal scroll keeps the cursor visible */
    tx = r.x + 7;
    if (focused(id)) {
        tmp = buf[U.cursor];
        buf[U.cursor] = 0;
        cx = ui_text_width(FONT_UI, buf);
        buf[U.cursor] = tmp;
        if (cx - U.scroll > r.width - 16) U.scroll = cx - (r.width - 16);
        if (cx - U.scroll < 0) U.scroll = cx;
        tx -= U.scroll;
    } else {
        cx = 0;
    }
    BeginScissorMode((int)r.x + 3, (int)r.y, (int)r.width - 6, (int)r.height);
    ui_text(FONT_UI, buf, tx, r.y + (r.height - U.size[FONT_UI]) / 2,
            enabled ? UI_TEXT : UI_TEXT_DIM);
    if (focused(id) && ((int)(GetTime() * 2) & 1) == 0)
        DrawRectangle((int)(tx + cx), (int)r.y + 5, 1, (int)r.height - 10, UI_TEXT);
    EndScissorMode();
    return enter;
}

bool ui_vslider(int id, Rectangle r, int *value, int min, int max, bool enabled)
{
    float span = r.height - 40;
    float t = max > min ? (float)(*value - min) / (float)(max - min) : 0;
    Rectangle handle = { r.x + 4, r.y + 20 + (1 - t) * span - 18,
                         r.width - 8, 36 };
    Rectangle track = { r.x + r.width / 2 - 4, r.y + 20, 8, span };
    int old = *value;

    if (enabled && ui_hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        U.drag_id = id;
    if (enabled && U.drag_id == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float y = GetMousePosition().y;
        float tt = 1 - (y - (r.y + 20)) / span;
        if (tt < 0) tt = 0;
        if (tt > 1) tt = 1;
        *value = min + (int)(tt * (float)(max - min) + 0.5f);
    }
    if (enabled && U.drag_id != id && ui_hovered(r)) {
        float w = GetMouseWheelMove();
        if (w != 0) {
            int v = *value + (w > 0 ? 3 : -3);
            *value = v < min ? min : v > max ? max : v;
        }
    }

    DrawRectangleRounded(r, 0.1f, SEGS, UI_PANEL_DARK);
    DrawRectangleRec(track, UI_FIELD);
    DrawRectangleLinesEx(track, 1, UI_BORDER);
    {
        int i;
        for (i = 0; i <= 10; i++) {
            float y = r.y + 20 + span * (float)i / 10;
            DrawLine((int)(r.x + 10), (int)y, (int)(r.x + 22), (int)y, UI_TEXT_DIM);
            DrawLine((int)(r.x + r.width - 22), (int)y, (int)(r.x + r.width - 10),
                     (int)y, UI_TEXT_DIM);
        }
    }
    handle.y = r.y + 20 + (1 - (max > min ? (float)(*value - min) / (float)(max - min) : 0)) * span - 18;
    DrawRectangleRounded(handle, 0.2f, SEGS,
                         U.drag_id == id ? UI_BTN_DOWN : (Color){ 170, 170, 170, 255 });
    DrawRectangle((int)handle.x + 6, (int)(handle.y + handle.height / 2) - 1,
                  (int)handle.width - 12, 3, UI_RED);
    return *value != old;
}

/* ---------------------------------------------------------------- */

void ui_message(const char *title, const char *fmt, ...)
{
    va_list ap;

    snprintf(U.msg_title, sizeof U.msg_title, "%s", title);
    va_start(ap, fmt);
    vsnprintf(U.msg_text, sizeof U.msg_text, fmt, ap);
    va_end(ap);
    U.msg_open = true;
}

bool ui_message_open(void) { return U.msg_open; }

void ui_toast(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(U.toast, sizeof U.toast, fmt, ap);
    va_end(ap);
    U.toast_until = GetTime() + 2.5;
}
