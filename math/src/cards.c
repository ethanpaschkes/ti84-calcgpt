#include "cards.h"
#include <graphx.h>
#include <math.h>
#include <string.h>
#include <sys/power.h>
#include <sys/rtc.h>
#include "draw.h"
#include "num.h"
#include "timefmt.h"

#define PI_F 3.14159265f
#define HELP_Y 226

/* palette numbers (the same look as CALC's home screen) */
enum {
    M_BG = 0, M_TEXT = 1, M_DIM = 2, M_BAR = 3, M_BLUE = 4, M_PURPLE = 5, M_GREEN = 6, M_ORANGE = 7, M_GRAY = 8,
    M_INK = 9, M_FILL = 10, M_LIGHT = 11, M_RED = 12, M_SEL = 14, M_LINE = 15, M_BG0 = 16 /* 16 to 31: the wallpaper */
};

void cards_palette(void)
{
    int i;
    gfx_palette[M_BG] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[M_TEXT] = gfx_RGBTo1555(245, 248, 255);
    gfx_palette[M_DIM] = gfx_RGBTo1555(190, 205, 235);
    gfx_palette[M_BAR] = gfx_RGBTo1555(10, 18, 45);
    gfx_palette[M_BLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[M_PURPLE] = gfx_RGBTo1555(150, 88, 210);
    gfx_palette[M_GREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[M_ORANGE] = gfx_RGBTo1555(245, 130, 20);
    gfx_palette[M_GRAY] = gfx_RGBTo1555(105, 118, 140);
    gfx_palette[M_INK] = gfx_RGBTo1555(30, 34, 50);
    gfx_palette[M_FILL] = gfx_RGBTo1555(200, 222, 255);
    gfx_palette[M_LIGHT] = gfx_RGBTo1555(236, 240, 250);
    gfx_palette[M_RED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[M_SEL] = gfx_RGBTo1555(255, 255, 255);
    gfx_palette[M_LINE] = gfx_RGBTo1555(205, 210, 225);
    for (i = 0; i < 16; i++) gfx_palette[M_BG0 + i] = gfx_RGBTo1555(18 + i * 3, 36 + i * 4, 88 + i * 6);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

/* ---------------------------------------------------------------- pieces */

static void text_centered(const char *s, int x, int w, int y, uint8_t color)
{
    text_at(s, x + (w - 8 * (int)strlen(s)) / 2, y, color);
}

static void round_rect(int x, int y, int w, int h, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillRectangle(x + 4, y, w - 8, h);
    gfx_FillRectangle(x, y + 4, w, h - 8);
    gfx_FillCircle(x + 4, y + 4, 4);
    gfx_FillCircle(x + w - 5, y + 4, 4);
    gfx_FillCircle(x + 4, y + h - 5, 4);
    gfx_FillCircle(x + w - 5, y + h - 5, 4);
}

static void battery_icon(int x, int y)
{
    int level = boot_GetBatteryStatus(), i;
    if (level > 4) level = 4;
    gfx_SetColor(M_DIM);
    gfx_Rectangle(x, y, 22, 10);
    gfx_FillRectangle(x + 22, y + 3, 2, 4);
    for (i = 0; i < level; i++) {
        gfx_SetColor(level <= 1 ? M_RED : M_GREEN);
        gfx_FillRectangle(x + 2 + i * 5, y + 2, 4, 6);
    }
}

void cards_background(const char *title, bool online)
{
    char buf[20];
    uint8_t hh, mm, ss;
    int y;
    for (y = 0; y < 240; y++) {
        gfx_SetColor((uint8_t)(M_BG0 + y * 16 / 240));
        gfx_HorizLine(0, y, SCREEN_W);
    }
    gfx_SetColor(M_BAR);
    gfx_FillRectangle(0, 0, SCREEN_W, HEAD_H + 4);
    text_at(title, 6, 5, M_TEXT);
    boot_GetTime(&ss, &mm, &hh);
    fmt_clock(hh, mm, 0, buf);
    text_centered(buf, 0, SCREEN_W, 5, M_TEXT);
    if (online) text_at("Online", 214, 5, M_GREEN);
    else text_at("Offline", 206, 5, M_DIM);
    battery_icon(286, 4);
}

void cards_help(const char *text)
{
    text_centered(text, 0, SCREEN_W, HELP_Y, M_DIM);
}

void cards_error(const char *text)
{
    char lines[2][WRAP_W];
    int n = wrap_text(text, 38, lines, 2), i;
    gfx_SetColor(M_RED);
    gfx_FillRectangle(0, 210, SCREEN_W, 30);
    for (i = 0; i < n; i++) text_centered(lines[i], 0, SCREEN_W, 213 + i * 11, M_SEL);
}

void cards_card(void)
{
    round_rect(CARD_X, CARD_Y, CARD_W, CARD_H, M_SEL);
}

/* a number with at most `dec` decimals, for short labels in a diagram */
static const char *label_num(float v, int dec)
{
    static char buf[NUM_CHARS];
    fmt_num(v, num_decimals < dec ? num_decimals : dec, buf);
    return buf;
}

static void dot(int x, int y, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillCircle(x, y, 2);
}

/* ---------------------------------------------------------------- the cards */

void card_form(const struct form *f, const char *button)
{
    int rows = form_rows(f), i, by, bw = 8 * (int)strlen(button) + 30;
    cards_card();
    text_at(f->title, 20, CARD_Y + 8, M_INK);
    text_at(f->hint, 20, CARD_Y + 20, M_GRAY);
    for (i = 0; i < f->n; i++) {
        int col = i / rows, row = i % rows;
        int x = 20 + col * 148, y = CARD_Y + 38 + row * 21;
        int x0 = x + 32, x1 = x + (f->cols == 2 ? 136 : 160);
        bool active = i == f->cur;
        text_at(f->f[i].label, x, y + 4, M_INK);
        pill(x0, y, x1, 17, active ? M_BLUE : M_LINE);
        text_at(f->f[i].text, x0 + 8, y + 5, M_INK);
        if (active) {
            gfx_SetColor(M_INK);
            gfx_VertLine(x0 + 8 + 8 * f->f[i].len, y + 3, 11);
        }
    }
    by = CARD_Y + CARD_H - 30;
    if (f->cur == f->n) round_rect(160 - bw / 2 - 3, by - 3, bw + 6, 24, M_INK);
    round_rect(160 - bw / 2, by, bw, 18, M_BLUE);
    text_centered(button, 160 - bw / 2, bw, by + 5, M_SEL);
}

void card_choice(const char *title, const char *subtitle, const char *const *options, int n, int sel)
{
    int i, h = 44 + n * 22 + (subtitle ? 12 : 0), y0 = 120 - h / 2, y;
    if (y0 < CARD_Y) y0 = CARD_Y;
    round_rect(44, y0, 232, h, M_SEL);
    text_centered(title, 44, 232, y0 + 10, M_INK);
    y = y0 + 26;
    if (subtitle) {
        text_centered(subtitle, 44, 232, y, M_GRAY);
        y += 12;
    }
    for (i = 0; i < n; i++, y += 22) {
        if (i == sel) round_rect(52, y - 4, 216, 20, M_FILL);
        text_at(i == sel ? ">" : " ", 60, y + 2, M_BLUE);
        text_at(options[i], 76, y + 2, M_INK);
    }
}

void card_answers(const char *title, const char *const *lines, int n, const char *rule, int highlight)
{
    int i;
    cards_card();
    text_at(title, 20, CARD_Y + 8, M_INK);
    for (i = 0; i < n && i < 10; i++) text_at(lines[i], 20, CARD_Y + 26 + i * 14, i == highlight ? M_BLUE : M_INK);
    gfx_SetColor(M_LINE);
    gfx_VertLine(172, CARD_Y + 26, CARD_H - 40);
    if (rule) text_at(rule, 20, CARD_Y + CARD_H - 14, M_GRAY);
}

void card_steps(const char *title, const char *text)
{
    static char lines[13][WRAP_W];
    int n = wrap_text(text, 36, lines, 13), i;
    cards_card();
    text_at(title, 20, CARD_Y + 8, M_INK);
    gfx_SetColor(M_LINE);
    gfx_HorizLine(20, CARD_Y + 20, CARD_W - 24);
    for (i = 0; i < n; i++) text_at(lines[i], 20, CARD_Y + 26 + i * 11, M_INK);
}

/* ---------------------------------------------------------------- diagrams */

/* a label kept inside the diagram's part of the card */
static void label_at(const char *s, int x, int y, uint8_t color)
{
    int w = 8 * (int)strlen(s);
    if (x < 176) x = 176;
    if (x + w > 310) x = 310 - w;
    if (y > CARD_Y + CARD_H - 14) y = CARD_Y + CARD_H - 14;
    if (y < CARD_Y + 4) y = CARD_Y + 4;
    text_at(s, x, y, color);
}

void diagram_triangle(const struct tri_sol *t)
{
    int px[3], py[3], i, cx = 0, cy = 0, big = 0;
    static const char *const SN[3] = {"a", "b", "c"}, *const AN[3] = {"A", "B", "C"};
    tri_points(t, 194, CARD_Y + 30, 90, 104, px, py);
    for (i = 0; i < 3; i++) {
        cx += px[i];
        cy += py[i];
    }
    cx /= 3;
    cy /= 3;
    gfx_SetColor(M_FILL);
    gfx_FillTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
    gfx_SetColor(M_BLUE);
    for (i = 0; i < 3; i++) gfx_Line(px[i], py[i], px[(i + 1) % 3], py[(i + 1) % 3]);
    for (i = 1; i < 3; i++) if (t->a[i] > t->a[big]) big = i;
    if (fabsf(t->a[big] - 90.0f) < 0.05f) {   /* the little square of a right angle */
        int p = (big + 1) % 3, q = (big + 2) % 3;
        float dx1 = (float)(px[p] - px[big]), dy1 = (float)(py[p] - py[big]), dx2 = (float)(px[q] - px[big]), dy2 = (float)(py[q] - py[big]);
        float l1 = sqrtf(dx1 * dx1 + dy1 * dy1), l2 = sqrtf(dx2 * dx2 + dy2 * dy2);
        if (l1 > 1 && l2 > 1) {
            int ux = (int)(dx1 * 7 / l1), uy = (int)(dy1 * 7 / l1), vx = (int)(dx2 * 7 / l2), vy = (int)(dy2 * 7 / l2);
            gfx_SetColor(M_INK);
            gfx_Line(px[big] + ux, py[big] + uy, px[big] + ux + vx, py[big] + uy + vy);
            gfx_Line(px[big] + vx, py[big] + vy, px[big] + ux + vx, py[big] + uy + vy);
        }
    }
    for (i = 0; i < 3; i++) {
        /* the corner's letter and angle, pushed away from the middle; the side's letter and length at its midpoint */
        char buf[24];
        int dx = px[i] - cx, dy = py[i] - cy, mx, my;
        int j = (i + 1) % 3, k = (i + 2) % 3;
        strcpy(buf, AN[i]);
        strcat(buf, " ");
        strcat(buf, label_num(t->a[i], 1));
        label_at(buf, px[i] + (dx > 4 ? 6 : dx < -4 ? -6 - 8 * (int)strlen(buf) : -4 * (int)strlen(buf)),
                 py[i] + (dy > 4 ? 4 : dy < -4 ? -12 : -4), M_INK);
        mx = (px[j] + px[k]) / 2;
        my = (py[j] + py[k]) / 2;
        dx = mx - cx;
        dy = my - cy;
        strcpy(buf, SN[i]);
        strcat(buf, "=");
        strcat(buf, label_num(t->s[i], 1));
        label_at(buf, mx + (dx > 4 ? 5 : dx < -4 ? -5 - 8 * (int)strlen(buf) : -4 * (int)strlen(buf)),
                 my + (dy > 4 ? 3 : dy < -4 ? -11 : -4), M_GRAY);
    }
}

/* a small graph box: x from xlo to xhi, y from ylo to yhi, with axes where they fall inside */
struct graph {
    int x0, y0, w, h;
    float xlo, xhi, ylo, yhi;
};

static int gx(const struct graph *g, float x) { return g->x0 + (int)((x - g->xlo) / (g->xhi - g->xlo) * (float)g->w); }
static int gy(const struct graph *g, float y) { return g->y0 + g->h - (int)((y - g->ylo) / (g->yhi - g->ylo) * (float)g->h); }

static void graph_axes(const struct graph *g)
{
    gfx_SetColor(M_LINE);
    gfx_Rectangle(g->x0 - 1, g->y0 - 1, g->w + 2, g->h + 2);
    gfx_SetColor(M_GRAY);
    if (g->ylo < 0 && g->yhi > 0) gfx_HorizLine(g->x0, gy(g, 0), g->w);
    if (g->xlo < 0 && g->xhi > 0) gfx_VertLine(gx(g, 0), g->y0, g->h);
}

/* y = a x^2 + b x + c, or a straight line when a is 0 */
static void graph_curve(const struct graph *g, float a, float b, float c, uint8_t color)
{
    int i, lx = 0, ly = 0;
    gfx_SetColor(color);
    for (i = 0; i <= 40; i++) {
        float x = g->xlo + (g->xhi - g->xlo) * (float)i / 40.0f, y = a * x * x + b * x + c;
        int sx = gx(g, x), sy;
        if (y > g->yhi) y = g->yhi;
        if (y < g->ylo) y = g->ylo;
        sy = gy(g, y);
        if (i) gfx_Line(lx, ly, sx, sy);
        lx = sx;
        ly = sy;
    }
}

static void graph_fit_y(struct graph *g, float a, float b, float c, bool first)
{
    int i;
    for (i = 0; i <= 20; i++) {
        float x = g->xlo + (g->xhi - g->xlo) * (float)i / 20.0f, y = a * x * x + b * x + c;
        if (first && i == 0) g->ylo = g->yhi = y;
        if (y < g->ylo) g->ylo = y;
        if (y > g->yhi) g->yhi = y;
    }
}

static void finish_y(struct graph *g)
{
    float pad;
    if (g->ylo > 0) g->ylo = 0;
    if (g->yhi < 0) g->yhi = 0;
    pad = (g->yhi - g->ylo) * 0.15f;
    if (pad < 0.5f) pad = 0.5f;
    g->ylo -= pad;
    g->yhi += pad;
}

void diagram_quadratic(float a, float b, float c, const struct quad_out *q)
{
    struct graph g = {180, CARD_Y + 26, 120, 112, 0, 0, 0, 0};
    float mid = q->vx, span = 3;
    if (q->nreal == 2) span = (q->x2 - q->x1) * 0.9f + 1.0f;
    if (a == 0) {
        mid = q->x1;
        span = 4;
    }
    g.xlo = mid - span;
    g.xhi = mid + span;
    graph_fit_y(&g, a, b, c, true);
    finish_y(&g);
    graph_axes(&g);
    graph_curve(&g, a, b, c, M_BLUE);
    if (q->nreal >= 1) dot(gx(&g, q->x1), gy(&g, 0), M_RED);
    if (q->nreal == 2) dot(gx(&g, q->x2), gy(&g, 0), M_RED);
    if (a != 0) dot(gx(&g, q->vx), gy(&g, q->vy), M_GREEN);
    text_at("roots", 182, CARD_Y + 142, M_RED);
    text_at("vertex", 244, CARD_Y + 142, M_GREEN);
}

void diagram_lines(float a, float b, float c, float d, int kind, float x)
{
    struct graph g = {180, CARD_Y + 26, 120, 112, 0, 0, 0, 0};
    float mid = kind == 1 ? x : 0;
    g.xlo = mid - 5;
    g.xhi = mid + 5;
    graph_fit_y(&g, 0, a, b, true);
    graph_fit_y(&g, 0, c, d, false);
    finish_y(&g);
    graph_axes(&g);
    graph_curve(&g, 0, a, b, M_BLUE);
    graph_curve(&g, 0, c, d, M_ORANGE);
    if (kind == 1) dot(gx(&g, x), gy(&g, a * x + b), M_RED);
    text_at("ax+b", 182, CARD_Y + 142, M_BLUE);
    text_at("cx+d", 240, CARD_Y + 142, M_ORANGE);
}

void diagram_unit_circle(const struct trig_out *t)
{
    const int cx = 240, cy = CARD_Y + 84, r = 54;
    float d = t->deg, step;
    int px = cx + (int)(t->c * (float)r), py = cy - (int)(t->s * (float)r), lx = cx + r - 12, ly = cy;
    char buf[32];
    gfx_SetColor(M_LINE);
    gfx_HorizLine(cx - r - 8, cy, 2 * r + 16);
    gfx_VertLine(cx, cy - r - 8, 2 * r + 16);
    gfx_SetColor(M_FILL);
    for (step = 6; step <= d + 5.99f; step += 6) {   /* the angle swept, as a fan of small triangles */
        float e = step > d ? d : step;
        int ex = cx + (int)(cosf(e * (PI_F / 180.0f)) * (float)(r - 12)), ey = cy - (int)(sinf(e * (PI_F / 180.0f)) * (float)(r - 12));
        gfx_FillTriangle(cx, cy, lx, ly, ex, ey);
        lx = ex;
        ly = ey;
    }
    gfx_SetColor(M_GRAY);
    gfx_Circle(cx, cy, r);
    gfx_SetColor(M_ORANGE);
    gfx_Line(px, py, px, cy);   /* the triangle: its height is sin, its base is cos */
    gfx_Line(cx, cy, px, cy);
    gfx_SetColor(M_BLUE);
    gfx_Line(cx, cy, px, py);
    dot(px, py, M_RED);
    strcpy(buf, "(");
    strcat(buf, label_num(t->c, 2));
    strcat(buf, ", ");
    strcat(buf, label_num(t->s, 2));
    strcat(buf, ")");
    text_centered(buf, 172, 140, CARD_Y + CARD_H - 16, M_INK);
    text_at("1", cx + r + 2, cy + 2, M_GRAY);
}

void diagram_dots(const struct stats_out *s)
{
    const int x0 = 184, x1 = 296, y = CARD_Y + 110;
    int i, stack = 0, mx;
    float span = s->hi - s->lo;
    if (span < 1e-6f) span = 1;
    gfx_SetColor(M_GRAY);
    gfx_HorizLine(x0 - 4, y, x1 - x0 + 8);
    for (i = 0; i < s->n; i++) {
        int x = x0 + (int)((s->sorted[i] - s->lo) / span * (float)(x1 - x0));
        stack = i > 0 && s->sorted[i] == s->sorted[i - 1] ? stack + 1 : 0;
        dot(x, y - 5 - stack * 7, M_BLUE);
    }
    mx = x0 + (int)((s->mean - s->lo) / span * (float)(x1 - x0));
    gfx_SetColor(M_RED);
    gfx_FillTriangle(mx, y + 2, mx - 4, y + 9, mx + 4, y + 9);
    label_at(label_num(s->lo, 1), x0 - 8, y + 12, M_GRAY);
    {
        const char *hi = label_num(s->hi, 1);
        label_at(hi, x1 - 8 * (int)strlen(hi) + 8, y + 12, M_GRAY);
    }
    label_at("mean", mx - 16, y + 24, M_RED);
}
