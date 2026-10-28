#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graphx.h"
#include "keypadc.h"

uint16_t gfx_palette[256];
volatile uint8_t kb_Data[8];
uint8_t fb[240][320];
uint32_t fake_ticks;
void (*swap_hook)(void);
void (*scan_hook)(void);
static uint8_t color, text_fg, sx = 1, sy = 1;

void gfx_Begin(void) {}
void gfx_End(void) {}
void gfx_SetDrawBuffer(void) {}
extern char text_log[];
void gfx_SwapDraw(void) { if (swap_hook) swap_hook(); text_log[0] = 0; }
void gfx_SetColor(uint8_t c) { color = c; }
void gfx_SetTextFGColor(uint8_t c) { text_fg = c; }
void gfx_SetTextBGColor(uint8_t c) { (void)c; }
void gfx_SetTextTransparentColor(uint8_t c) { (void)c; }
void gfx_SetTextScale(uint8_t w, uint8_t h) { sx = w; sy = h; }
static void px(int x, int y, uint8_t c) { if (x >= 0 && x < 320 && y >= 0 && y < 240) fb[y][x] = c; }
void gfx_FillRectangle(int x, int y, int w, int h) { int i, j; for (j = 0; j < h; j++) for (i = 0; i < w; i++) px(x + i, y + j, color); }
void gfx_Rectangle(int x, int y, int w, int h) { gfx_HorizLine(x, y, w); gfx_HorizLine(x, y + h - 1, w); gfx_VertLine(x, y, h); gfx_VertLine(x + w - 1, y, h); }
void gfx_HorizLine(int x, int y, int len) { gfx_FillRectangle(x, y, len, 1); }
void gfx_VertLine(int x, int y, int len) { gfx_FillRectangle(x, y, 1, len); }
void gfx_Line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sxn = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), syn = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        int e2;
        px(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sxn; }
        if (e2 <= dx) { err += dx; y0 += syn; }
    }
}
void gfx_FillCircle(int cx, int cy, int r) { int x, y; for (y = -r; y <= r; y++) for (x = -r; x <= r; x++) if (x * x + y * y <= r * r) px(cx + x, cy + y, color); }
void gfx_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2), x, y;
    for (y = miny; y <= maxy; y++) for (x = minx; x <= maxx; x++) {
        int d1 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1), d2 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2), d3 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
        if (!((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0))) px(x, y, color);
    }
}
void gfx_Sprite(const gfx_sprite_t *sp, int x, int y)
{
    int i, j;
    for (j = 0; j < sp->height; j++) for (i = 0; i < sp->width; i++) px(x + i, y + j, sp->data[j * sp->width + i]);
}
void gfx_ScaledSprite_NoClip(const gfx_sprite_t *sp, int x, int y, uint8_t w, uint8_t h)
{
    int i, j, a, b;
    for (j = 0; j < sp->height; j++) for (i = 0; i < sp->width; i++)
        for (b = 0; b < h; b++) for (a = 0; a < w; a++) px(x + i * w + a, y + j * h + b, sp->data[j * sp->width + i]);
}
/* text is only listed: x y scale red green blue string, one per line, for the picture being drawn */
char text_log[20000];
void gfx_PrintStringXY(const char *s, int x, int y)
{
    char line[200];
    unsigned p = gfx_palette[text_fg];
    snprintf(line, sizeof line, "%d %d %d %u %u %u %s\n", x, y, sx, ((p >> 10) & 31) << 3, ((p >> 5) & 31) << 3, (p & 31) << 3, s);
    if (strlen(text_log) + strlen(line) < sizeof text_log) strcat(text_log, line);
}
void kb_Scan(void) { if (scan_hook) scan_hook(); }
uint8_t boot_GetBatteryStatus(void) { return 3; }
void timer_Enable(int t, int a, int b, int c) { (void)t; (void)a; (void)b; (void)c; }
void timer_Set(int t, uint32_t v) { (void)t; fake_ticks = v; }
uint32_t timer_Get(int t) { (void)t; return fake_ticks; }
