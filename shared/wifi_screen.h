#ifndef WIFI_SCREEN_H
#define WIFI_SCREEN_H

#include <stdbool.h>
#include <stdint.h>
#include "chatmodel.h"
#include "outbuf.h"
#include "typing.h"

/* The Wi-Fi setup screen shared by CALCGPT ([mode]) and CALC (the Settings tile): the list of networks the bridge board found,
 * joining one (typing its password, or a hidden network's name), and scanning again. This file is the keys and state only, with no
 * drawing and no USB, so it runs in tests on a computer; wifi_draw.c draws it. The networks, the connection line and the status
 * text live in the struct chat that parses the board's messages (V, X, Y, T: ../wireless/PROTOCOL.md section 3).
 *
 * Keys: Up/Down pick, Enter joins (an open network at once; a locked one asks for its password), Mode or Graph scan again,
 * Clear goes back (first erasing what is typed). "Other network..." at the end of the list joins a hidden network by name. */

#define WIFI_ROWS 12          /* rows of the list on screen */
#define WIFI_PASS_MAX 63      /* the longest WPA password */
#define WIFI_SCAN_WAIT (10u * 32768u)   /* no list after 10 s (no Wi-Fi board, only the Mac): stop saying "Scanning..." */

enum { WIFI_LIST, WIFI_SSID, WIFI_PASS };   /* what the screen shows: the list, typing a hidden network's name, typing a password */
enum { WIFI_STAY, WIFI_BACK };              /* what a key did: nothing the caller cares about, or "leave the Wi-Fi screen" */

struct wifi_ui {
    int screen;
    int sel, top;              /* the chosen row (the last row is "Other network") and the first row on screen */
    char ssid[CHAT_SSID_MAX + 1];
    int ssid_len;
    char pass[WIFI_PASS_MAX + 1];
    int pass_len;
    uint32_t scan_at;          /* when the last scan was asked for (32768 Hz ticks) */
};

/* the screen opens: list from the top, and a scan is asked for */
void wifi_ui_open(struct wifi_ui *w, struct chat *c, struct outbuf *out, bool linked, uint32_t now);

/* asks the board for the list of networks ('W'); "Scanning..." shows only when a link is up */
void wifi_ui_scan(struct wifi_ui *w, struct chat *c, struct outbuf *out, bool linked, uint32_t now);

/* a key went down (linked: a USB link is up). Returns WIFI_BACK when the user left the screen. */
int wifi_ui_key(struct wifi_ui *w, struct chat *c, struct typing *ty, int key, struct outbuf *out, bool linked, uint32_t now);

/* the chosen row stays inside the list (it can shrink after a new scan) and on screen; call before drawing */
void wifi_ui_keep_sel(struct wifi_ui *w, const struct chat *c);

/* time passes: returns true when the screen should be redrawn (a scan that got no answer stopped) */
bool wifi_ui_tick(struct wifi_ui *w, struct chat *c, uint32_t now);

#endif
