/* CALCGPT: a chat on the calculator. A language model answers from the Mac (mac/calcgptd.py) or from the CalcGPT server through
 * the Wi-Fi bridge board; you type on the calculator and the answer streams onto its screen. The messages are described at the
 * top of calcgptd.py and in ../wireless/PROTOCOL.md.
 *
 * This file is the drawing and the main loop. The keys and screens are in ui.c (tested on a computer), the Wi-Fi setup screen in
 * ../../shared/wifi_screen.c + wifi_draw.c (shared with CALC), the USB link in ../../shared/link.c.
 *
 * Keys: type your question (Alpha switches to letters, 2nd is a one-shot shift), Enter sends, Up/Down scroll,
 * Window/Zoom page up/down, Del erases, Graph starts a new chat, Y= stops an answer, Trace picks the AI model,
 * Mode opens the Wi-Fi setup, Clear erases the line then quits. */
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>
#include "chatmodel.h"
#include "draw.h"
#include "keys.h"
#include "link.h"
#include "ui.h"
#include "wifi_draw.h"

#define BODY_Y 18
#define ROW_H 10
#define MARGIN 8
#define SECOND 32768u    /* timer 1 ticks */

static struct chat chat;
static struct ui ui;
static uint8_t in_buf[2048];
static bool dirty = true;
static bool asked_models;    /* the list of models was asked for since the link came up */
static uint32_t asked_at;    /* when it was last asked for */
static bool asked_conn;      /* the Wi-Fi board was asked for its connection line since the link came up */

/* ---- drawing ---- */

/* "CalcGPT", the Wi-Fi connection line (when a bridge board sent one) and the model in use */
static void draw_header(void)
{
    int x = 6 + (int)gfx_GetStringWidth("CalcGPT") + 12, right = SCREEN_W - 6;
    draw_title("CalcGPT");
    if (chat.nmodels > 0) {
        const char *name = chat.models[chat.model_sel];
        right -= (int)gfx_GetStringWidth(name);
        text_at(name, right, 3, C_GRAY);
        right -= 10;
    }
    if (chat.conn[0] && right > x) text_at(fit(chat.conn, (unsigned int)(right - x)), x, 3, C_LINK);
}

static void draw_welcome(void)
{
    static const uint8_t letter_color[7] = {C_GBLUE, C_GRED, C_GYELLOW, C_GBLUE, C_GGREEN, C_GRED, C_GBLUE};
    static const char name[] = "CalcGPT";
    char one[2] = {0, 0};
    int i;
    gfx_SetTextScale(3, 3);
    for (i = 0; i < 7; i++) {
        one[0] = name[i];
        text_at(one, 76 + i * 24, 50, letter_color[i]);
    }
    gfx_SetTextScale(1, 1);
    text_at("Ask anything. Trace picks the AI model.", 24, 90, C_GRAY);
    text_at("Try:  what is sin 30?", 24, 112, C_BLACK);
    text_at("      explain the law of sines", 24, 124, C_BLACK);
    text_at("      plan my study week", 24, 136, C_BLACK);
    text_at("Alpha = letters, 2nd = capitals/symbols", 24, 156, C_GRAY);
    text_at("made by Ethan Paschkes", 24, 170, C_GRAY);
    if (!link_up()) text_at("Not connected: run calcgptd.py on the Mac", 18, 184, C_RED);
}

static void draw_chat(void)
{
    int i;
    for (i = 0; i < CHAT_VIEW_ROWS && chat.top + i < chat.nrows; i++) {
        const struct chat_row *r = &chat.rows[chat.top + i];
        char buf[CHAT_COLS + 1];
        int y = BODY_Y + i * ROW_H, x = MARGIN;
        memcpy(buf, chat.buf + r->start, r->len);
        buf[r->len] = 0;
        if (r->role == ROLE_USER) {
            if (r->first) text_at(">", MARGIN, y, C_GBLUE);
            x = MARGIN + 16;
            text_at(buf, x, y, C_LINK);
        } else {
            text_at(buf, x, y, C_BLACK);
        }
    }
    draw_scrollbar(BODY_Y, CHAT_VIEW_ROWS * ROW_H, chat.top, CHAT_VIEW_ROWS, chat.nrows);
}

/* the list of AI models, drawn over the chat */
static void draw_menu(void)
{
    const int x = 30, w = 260, row = 16;
    int n = chat.nmodels > 0 ? chat.nmodels : 1, h = 22 + n * row + 18, y = 34, i;
    gfx_SetColor(C_BLACK);
    gfx_FillRectangle(x - 1, y - 1, w + 2, h + 2);
    gfx_SetColor(C_WHITE);
    gfx_FillRectangle(x, y, w, h);
    text_at("Choose the AI model", x + 8, y + 6, C_BLACK);
    if (chat.nmodels == 0) text_at("none yet: is calcgptd.py on?", x + 8, y + 24, C_RED);
    for (i = 0; i < chat.nmodels; i++) {
        int ry = y + 20 + i * row;
        if (i == ui.menu_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(x + 4, ry - 2, w - 8, row);
        }
        text_at(i == ui.menu_sel ? ">" : " ", x + 8, ry, C_GBLUE);
        text_at(chat.models[i], x + 24, ry, C_BLACK);
        if (i == chat.model_sel) text_at("(in use)", x + w - 8 - 8 * 8, ry, C_GREEN);
    }
    text_at("Up/Down  Enter: use  Clear: back", x + 8, y + h - 14, C_GRAY);
}

static void draw_chat_footer(void)
{
    if (chat.status[0]) draw_footer_text(chat.status, C_RED);
    else if (chat.busy) draw_footer_text("Answering...   Y=: stop", C_RED);
    else draw_footer_text(hint("Trace: model  Mode: Wi-Fi  Graph: new  Clear: quit", "Trace: model  Mode: Wi-Fi  Graph: new"), C_GRAY);
}

static void redraw(void)
{
    gfx_FillScreen(C_WHITE);
    if (ui.screen == SCR_CHAT) {
        draw_header();
        if (chat.nrows == 0 && !chat.busy) draw_welcome();
        else draw_chat();
        draw_field(ui.input, ui.input_len, 201, !chat.busy, &ui.ty);
        draw_chat_footer();
        if (ui.menu_open) draw_menu();
    } else {
        wifi_draw(&ui.wifi, &chat, &ui.ty, link_up());
    }
    gfx_SwapDraw();
}

/* ---- the link ---- */

static void press(int key, uint32_t now)
{
    ui_key(&ui, &chat, key, link_up(), now);
    link_send(ui.out.buf, ui.out.len);
    ui.out.len = 0;
    dirty = true;
}

/* the other side is asked for its model list and connection line once the link is up; forgotten when it goes */
static void keep_in_touch(uint32_t now)
{
    uint8_t cmd;
    if (link_lost()) {
        asked_models = false;
        asked_conn = false;
        chat.conn[0] = 0;
        chat.scanning = 0;
        dirty = true;
    }
    if (!link_up()) return;
    /* the Mac answers with its list of models. Asked again each second until it does: over the wireless bridge the first
     * request can be lost while the board is still starting up, and the Mac also uses it to tell CALCGPT from other programs */
    if (chat.nmodels == 0 && (!asked_models || now - asked_at > SECOND)) {
        cmd = 'R';
        link_send(&cmd, 1);
        asked_models = true;
        asked_at = now;
    }
    /* a Wi-Fi bridge board answers with its connection line ('V'); the Mac ignores this */
    if (!asked_conn) {
        cmd = 'I';
        link_send(&cmd, 1);
        asked_conn = true;
    }
}

int main(void)
{
    uint8_t prev[8] = {0}, cur[8] = {0}, cmd;
    int held = -1;
    uint32_t held_since = 0, next_repeat = 0;

    gfx_Begin();
    ui_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    chat_init(&chat);
    ui_init(&ui);
    if (!link_init()) {
        gfx_End();
        return 1;
    }

    while (!ui.quit) {
        int g, b, n;
        uint32_t now;

        link_poll();
        now = timer_Get(1);
        keep_in_touch(now);
        if (wifi_ui_tick(&ui.wifi, &chat, now)) dirty = true;
        n = link_read(in_buf, sizeof(in_buf));
        if (n > 0) chat_feed(&chat, in_buf, n);
        if (chat.changed) {
            chat.changed = 0;
            dirty = true;
        }
        kb_Scan();
        for (g = 1; g <= 7; g++) cur[g] = kb_Data[g];
        for (g = 1; g <= 7; g++) {
            uint8_t pressed = cur[g] & (uint8_t)~prev[g];
            for (b = 0; b < 8; b++) {
                if (pressed & (1 << b)) {
                    press(K(g, b), now);
                    held = ui_repeats(K(g, b)) ? K(g, b) : -1;
                    held_since = now;
                    next_repeat = now + 13107;  /* about 0.4 s before a held key repeats */
                }
            }
            prev[g] = cur[g];
        }
        if (held >= 0) {
            if (!(cur[K_GROUP(held)] & K_MASK(held))) held = -1;
            else if (now >= next_repeat && now - held_since > 13107) {
                press(held, now);
                next_repeat = now + 2300;      /* then about 14 times a second */
            }
        }
        if (dirty) {
            dirty = false;
            chat_layout(&chat, CHAT_VIEW_ROWS);
            redraw();
        }
    }
    cmd = 'Z';
    link_send(&cmd, 1);   /* "quitting": the Mac side closes the chat */
    link_close();
    gfx_End();
    return 0;
}
