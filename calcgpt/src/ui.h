#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>
#include "chatmodel.h"
#include "outbuf.h"
#include "typing.h"
#include "wifi_screen.h"

/* CALCGPT's keys and screens, with no drawing and no USB in it so tests/ui_test.c can press keys on a computer: which screen is
 * showing (the chat or the Wi-Fi setup), the model menu, the line being typed, and the bytes each key sends (collected in `out`;
 * main.c sends them). Key numbers are in ../../shared/keys.h.
 *
 * Keys on the chat: Enter sends the question, Up/Down scroll, Window/Zoom page, Del erases, Graph starts a new chat, Y= stops an
 * answer, Trace opens the model menu, Mode opens the Wi-Fi setup, Clear erases the line and then quits. */

enum { SCR_CHAT, SCR_WIFI };

struct ui {
    int screen;
    bool menu_open;         /* the model menu is showing over the chat */
    int menu_sel;
    char input[CHAT_INPUT_MAX + 1];
    int input_len;
    bool quit;
    struct typing ty;
    struct wifi_ui wifi;
    struct outbuf out;
};

void ui_init(struct ui *u);

/* a key went down. linked: a USB link is up; now: timer ticks (32768 Hz), for the Wi-Fi scan timeout. */
void ui_key(struct ui *u, struct chat *c, int key, bool linked, uint32_t now);

/* keys that repeat while held */
bool ui_repeats(int key);

#endif
