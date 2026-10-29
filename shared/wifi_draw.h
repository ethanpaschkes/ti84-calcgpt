#ifndef WIFI_DRAW_H
#define WIFI_DRAW_H

#include <stdbool.h>
#include "wifi_screen.h"

/* Draws the Wi-Fi setup screen (wifi_screen.h) with graphx: title, the board's connection line, the list or the line being typed,
 * and the footer hint (or the board's status text, or "Scanning..."). The caller clears the screen first and swaps the buffer
 * after. `linked`: a USB link is up. */
void wifi_draw(struct wifi_ui *w, const struct chat *c, const struct typing *ty, bool linked);

#endif
