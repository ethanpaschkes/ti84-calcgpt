#include "draw.h"
#include <graphx.h>
#include "chatmodel.h"
#include "typing.h"

void ui_palette(void)
{
    gfx_palette[C_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[C_LINK] = gfx_RGBTo1555(25, 85, 200);
    gfx_palette[C_GRAY] = gfx_RGBTo1555(115, 115, 115);
    gfx_palette[C_RED] = gfx_RGBTo1555(200, 40, 40);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(20, 135, 70);
    gfx_palette[C_LINE] = gfx_RGBTo1555(205, 205, 205);
    gfx_palette[C_SEL] = gfx_RGBTo1555(226, 236, 255);
    gfx_palette[C_GBLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[C_GRED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[C_GYELLOW] = gfx_RGBTo1555(251, 188, 5);
    gfx_palette[C_GGREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

const char *fit(const char *s, unsigned int w)
{
    static char out[CHAT_COLS + 1];
    int n = 0;
    while (s[n] && n < CHAT_COLS) {
        out[n] = s[n] >= 32 && s[n] < 127 ? s[n] : '?';
        n++;
    }
    out[n] = 0;
    while (n > 0 && gfx_GetStringWidth(out) > w) out[--n] = 0;
    return out;
}

void draw_title(const char *title)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    text_at(title, 6, 3, C_BLACK);
}

void pill(int x0, int y0, int x1, int h, uint8_t border)
{
    int r = h / 2;
    gfx_SetColor(border);
    gfx_FillCircle(x0 + r, y0 + r, r);
    gfx_FillCircle(x1 - r, y0 + r, r);
    gfx_FillRectangle(x0 + r, y0, x1 - x0 - 2 * r, h);
    gfx_SetColor(C_WHITE);
    gfx_FillCircle(x0 + r, y0 + r, r - 1);
    gfx_FillCircle(x1 - r, y0 + r, r - 1);
    gfx_FillRectangle(x0 + r, y0 + 1, x1 - x0 - 2 * r, h - 2);
}

void draw_field(const char *s, int len, int y, bool active, const struct typing *ty)
{
    const char *shown = s + (len > 30 ? len - 30 : 0);
    pill(6, y, 314, 20, active ? C_GBLUE : C_LINE);
    text_at(shown, 20, y + 6, C_BLACK);
    if (active) {
        gfx_SetColor(C_BLACK);
        gfx_VertLine(20 + (int)gfx_GetStringWidth(shown), y + 3, 11);
    }
    text_at(ty->alpha ? (ty->shift ? "ABC" : "abc") : (ty->shift ? "^" : "123"), 284, y + 6, C_GRAY);
}

void draw_scrollbar(int y, int track, int top, int view, int total)
{
    int thumb, pos;
    if (total <= view) return;
    thumb = track * view / total;
    pos = (track - thumb) * top / (total - view);
    gfx_SetColor(C_LINE);
    gfx_FillRectangle(SCREEN_W - 5, y, 3, track);
    gfx_SetColor(C_LINK);
    gfx_FillRectangle(SCREEN_W - 5, y + pos, 3, thumb < 4 ? 4 : thumb);
}

const char *hint(const char *long_hint, const char *short_hint)
{
    return gfx_GetStringWidth(long_hint) <= SCREEN_W - 8 ? long_hint : short_hint;
}

void draw_footer_text(const char *text, uint8_t color)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, FOOT_Y - 3, SCREEN_W);
    text_at(text, 4, FOOT_Y, color);
}
