#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>
#include <stdint.h>

/* Drawing helpers shared by CALCGPT and CALC's Settings screen (graphx; calculator only). The colours are palette numbers set by
 * ui_palette(); CALC has a palette of its own for its home screen and switches to this one for the Wi-Fi screen. */

#define SCREEN_W 320
#define HEAD_H 14
#define FOOT_Y 228

enum { C_BLACK, C_LINK, C_GRAY, C_RED, C_GREEN, C_LINE, C_SEL, C_GBLUE, C_GRED, C_GYELLOW, C_GGREEN, C_WHITE = 255 };

void ui_palette(void);
void text_at(const char *s, int x, int y, uint8_t color);
/* a copy of s at most w pixels wide, with bytes the font cannot show (a network's name may have any) drawn as '?' */
const char *fit(const char *s, unsigned int w);
/* the title at the top left and the line under the title bar */
void draw_title(const char *title);
/* a rounded box */
void pill(int x0, int y0, int x1, int h, uint8_t border);
/* a line being typed (a question, a network name or a password), its end shown when it is long, and the typing layer's state */
struct typing;
void draw_field(const char *s, int len, int y, bool active, const struct typing *ty);
/* a scroll bar at the right edge, when there are more rows than fit */
void draw_scrollbar(int y, int track, int top, int view, int total);
/* the longest of these hints that fits the footer */
const char *hint(const char *long_hint, const char *short_hint);
/* the footer line and a text in it */
void draw_footer_text(const char *text, uint8_t color);

#endif
