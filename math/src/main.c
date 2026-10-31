/* MATH: a guided math app for the TI-84 Plus CE, opened from CALC's Math tile. Pick a problem type (triangle, Pythagorean,
 * quadratic, equation, unit circle, stats), fill a short form, and get "Answers are:" with a drawn diagram; Graph shows the
 * rule used with the numbers plugged in. Works with nothing plugged in. With the Wi-Fi box plugged in and online, Y= on the
 * answers sends the problem to ChatGPT through the same link CALCGPT uses.
 *
 * The keys and screens are in app.c (tested on a computer), the drawing in screens.c, the maths in tri.c and probs.c. */
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>
#include "app.h"
#include "chatmodel.h"
#include "draw.h"
#include "keys.h"
#include "link.h"
#include "outbuf.h"
#include "screens.h"

#define SECOND 32768u

static struct app app;
static struct chat chat;     /* the board's connection line, and ChatGPT's answer */
static struct outbuf out;
static uint8_t in_buf[512];
static bool asked_conn;
static uint32_t asked_at;

static bool online(void)
{
    return link_up() && strstr(chat.conn, "| online") != NULL;
}

/* the board is asked for its connection line ('I', which also tells it this program wants the ChatGPT server) once the link
 * is up, again every 2 s until it answers; forgotten when the link goes */
static void keep_in_touch(uint32_t now, bool *dirty)
{
    uint8_t cmd = 'I';
    if (link_lost()) {
        asked_conn = false;
        chat.conn[0] = 0;
        *dirty = true;
    }
    if (!link_up() || chat.conn[0]) return;
    if (!asked_conn || now - asked_at > 2 * SECOND) {
        link_send(&cmd, 1);
        asked_conn = true;
        asked_at = now;
    }
}

static uint8_t prev_keys[8];

/* the next key that went down since the last call, as group * 8 + bit, or -1 */
static int next_key(void)
{
    int g, b;
    kb_Scan();
    for (g = 1; g <= 7; g++) {
        uint8_t now = kb_Data[g], pressed = now & (uint8_t)~prev_keys[g];
        for (b = 0; b < 8; b++) {
            if (pressed & (1 << b)) {
                prev_keys[g] |= (uint8_t)(1 << b);
                return K(g, b);
            }
        }
        prev_keys[g] = now;
    }
    return -1;
}

static void forget_keys(void)
{
    int g;
    kb_Scan();
    for (g = 1; g <= 7; g++) prev_keys[g] = kb_Data[g];
}

/* a key on the ChatGPT answer screen: scrolling is here because the chat belongs to the main loop */
static bool chat_key(int key)
{
    if (key == K_UP) chat_scroll(&chat, -1, 13);
    else if (key == K_DOWN) chat_scroll(&chat, 1, 13);
    else return false;
    return true;
}

int main(void)
{
    bool dirty = true, usb;
    uint8_t cmd = 'Z';

    gfx_Begin();
    screens_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    chat_init(&chat);
    app_init(&app);
    usb = link_init();
    forget_keys();
    for (;;) {
        uint32_t now;
        int key, n;
        bool was_online = online();

        if (usb) link_poll();
        now = timer_Get(1);
        keep_in_touch(now, &dirty);
        n = link_read(in_buf, sizeof(in_buf));
        if (n > 0) chat_feed(&chat, in_buf, n);
        if (chat.changed) {
            chat.changed = 0;
            chat_layout(&chat, 13);
            dirty = true;
        }
        if (online() != was_online) dirty = true;
        key = next_key();
        if (key >= 0) {
            int did = app.screen == S_CHAT && chat_key(key) ? APP_REDRAW : app_key(&app, key, online());
            if (did == APP_QUIT) break;
            if (did == APP_ASK) {
                chat_clear(&chat);
                out_cmd(&out, 'N');
                chat_user(&chat, app.question);
                out_msg(&out, 'Q', app.question, (int)strlen(app.question));
                chat_layout(&chat, 13);
            }
            if (did != APP_NONE) dirty = true;
            link_send(out.buf, out.len);
            out.len = 0;
        }
        if (dirty) {
            dirty = false;
            screens_draw(&app, online(), &chat);
        }
    }
    if (usb) {
        link_send(&cmd, 1);   /* "quitting": the server closes the chat */
        link_close();
    }
    gfx_End();
    return 0;
}
