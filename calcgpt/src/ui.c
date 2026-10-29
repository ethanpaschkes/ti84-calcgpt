#include "ui.h"
#include <string.h>
#include "keys.h"

void ui_init(struct ui *u)
{
    memset(u, 0, sizeof(*u));
}

bool ui_repeats(int key)
{
    return key == K_UP || key == K_DOWN || key == K_DEL;
}

static void menu_key(struct ui *u, struct chat *c, int key)
{
    if (key == K_UP && u->menu_sel > 0) {
        u->menu_sel--;
    } else if (key == K_DOWN && u->menu_sel < c->nmodels - 1) {
        u->menu_sel++;
    } else if (key == K_ENTER) {
        if (c->nmodels > 0 && u->menu_sel != c->model_sel) {
            uint8_t m[2] = {'M', (uint8_t)u->menu_sel};
            out_bytes(&u->out, m, 2);
        }
        u->menu_open = false;
    } else if (key == K_CLEAR || key == K_TRACE) {
        u->menu_open = false;
    }
}

/* a key typed into the question: Del erases, other keys go through the typing layer */
static void type_key(struct ui *u, int key)
{
    char ch = 0;
    if (key == K_DEL) {
        if (u->input_len) u->input[--u->input_len] = 0;
        typing_used_other_key(&u->ty);
    } else if (typing_key(&u->ty, key, &ch) == TY_CHAR && u->input_len < CHAT_INPUT_MAX) {
        u->input[u->input_len++] = ch;
        u->input[u->input_len] = 0;
    }
}

void ui_key(struct ui *u, struct chat *c, int key, bool linked, uint32_t now)
{
    if (u->menu_open) {
        menu_key(u, c, key);
        return;
    }
    if (u->screen == SCR_WIFI) {
        if (wifi_ui_key(&u->wifi, c, &u->ty, key, &u->out, linked, now) == WIFI_BACK) u->screen = SCR_CHAT;
        return;
    }
    if (key == K_TRACE) {
        if (c->nmodels == 0) out_cmd(&u->out, 'R');
        u->menu_sel = c->model_sel;
        u->menu_open = true;
    } else if (key == K_MODE) {
        u->screen = SCR_WIFI;
        wifi_ui_open(&u->wifi, c, &u->out, linked, now);
    } else if (key == K_ENTER) {
        if (u->input_len && !c->busy && linked) {
            chat_user(c, u->input);
            out_msg(&u->out, 'Q', u->input, u->input_len);
            u->input_len = 0;
            u->input[0] = 0;
        }
        typing_used_other_key(&u->ty);
    } else if (key == K_CLEAR) {
        if (u->input_len) {
            u->input_len = 0;
            u->input[0] = 0;
        } else {
            u->quit = true;
        }
    } else if (key == K_GRAPH) {
        out_cmd(&u->out, 'N');
        chat_clear(c);
    } else if (key == K_YEQU) {
        out_cmd(&u->out, 'S');
    } else if (key == K_UP) {
        chat_scroll(c, -1, CHAT_VIEW_ROWS);
    } else if (key == K_DOWN) {
        chat_scroll(c, 1, CHAT_VIEW_ROWS);
    } else if (key == K_WINDOW) {
        chat_scroll(c, -(CHAT_VIEW_ROWS - 1), CHAT_VIEW_ROWS);
    } else if (key == K_ZOOM) {
        chat_scroll(c, CHAT_VIEW_ROWS - 1, CHAT_VIEW_ROWS);
    } else {
        type_key(u, key);
    }
}
