#ifndef SCREENS_H
#define SCREENS_H

#include <stdbool.h>
#include "app.h"

/* The Math app's screens drawn with graphx (calculator only), in CALC's look: the dark gradient wallpaper, the status bar, white
 * cards. The diagrams (a to-scale triangle, a parabola, two lines, the unit circle, a dot plot) are drawn from the numbers in
 * struct app. */

struct chat;

void screens_palette(void);
/* draws the screen the app is on; online = the box has a server; chat = the ChatGPT answer (S_CHAT only) */
void screens_draw(const struct app *a, bool online, const struct chat *chat);

#endif
