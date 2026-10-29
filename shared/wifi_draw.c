#include "wifi_draw.h"
#include <graphx.h>
#include "draw.h"

#define WIFI_Y 40        /* the first row of the list */
#define WIFI_ROW 14

/* signal strength as four bars of growing height, the first `level` of them dark */
static void draw_bars(int x, int y, int level)
{
    int i;
    for (i = 0; i < 4; i++) {
        gfx_SetColor(i < level ? C_BLACK : C_LINE);
        gfx_FillRectangle(x + i * 4, y + 6 - i * 2, 3, 2 + i * 2);
    }
}

static void draw_list(struct wifi_ui *w, const struct chat *c)
{
    int n = c->nwifi + 1, i;   /* the networks, then "Other network" */
    wifi_ui_keep_sel(w, c);
    for (i = w->top; i < n && i < w->top + WIFI_ROWS; i++) {
        int y = WIFI_Y + (i - w->top) * WIFI_ROW;
        if (i == w->sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(4, y - 3, SCREEN_W - 12, WIFI_ROW);
        }
        if (i == c->nwifi) {
            text_at("Other network...", 48, y, C_LINK);
        } else {
            const struct chat_wifi *net = &c->wifi[i];
            draw_bars(10, y, net->signal);
            if (net->locked) text_at("*", 30, y, C_GRAY);
            if (net->known) text_at("+", 38, y, C_GREEN);
            text_at(net->ssid[0] ? fit(net->ssid, SCREEN_W - 60) : "(no name)", 48, y, C_BLACK);
        }
    }
    if (c->nwifi == 0) text_at(c->scanning ? "Looking for networks..." : "No networks found. Mode: look again", 48, WIFI_Y + 2 * WIFI_ROW, C_GRAY);
    draw_scrollbar(WIFI_Y - 3, WIFI_ROWS * WIFI_ROW, w->top, WIFI_ROWS, n);
    text_at("*  needs a password    +  remembered", 10, 212, C_GRAY);
}

/* typing a hidden network's name, or the password of the chosen network */
static void draw_entry(const struct wifi_ui *w, const struct typing *ty)
{
    if (w->screen == WIFI_SSID) {
        text_at("Name of the network:", 10, 52, C_BLACK);
        draw_field(w->ssid, w->ssid_len, 66, true, ty);
    } else {
        text_at("Password for", 10, 46, C_BLACK);
        text_at(fit(w->ssid, SCREEN_W - 20), 10, 58, C_LINK);
        draw_field(w->pass, w->pass_len, 72, true, ty);
        text_at("Leave it empty for an open network.", 10, 122, C_GRAY);
    }
    text_at("Alpha = letters, 2nd = capitals/symbols", 10, 108, C_GRAY);
}

void wifi_draw(struct wifi_ui *w, const struct chat *c, const struct typing *ty, bool linked)
{
    const char *h;
    draw_title("Wi-Fi");
    if (c->scanning) text_at("Scanning...", SCREEN_W - 6 - (int)gfx_GetStringWidth("Scanning..."), 3, C_RED);
    if (!linked) text_at("Not connected to the Wi-Fi board", 6, 20, C_RED);
    else if (c->conn[0]) text_at(fit(c->conn, SCREEN_W - 12), 6, 20, C_LINK);
    else text_at("No answer from a Wi-Fi board yet", 6, 20, C_GRAY);
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, 32, SCREEN_W);
    if (w->screen == WIFI_LIST) draw_list(w, c);
    else draw_entry(w, ty);
    if (c->status[0]) {
        draw_footer_text(c->status, C_RED);
        return;
    }
    if (w->screen == WIFI_LIST) h = hint("Enter: join  Mode: scan again  Clear: back", "Enter: join  Mode: scan  Clear: back");
    else h = w->screen == WIFI_SSID ? "Enter: next  Del: erase  Clear: back" : "Enter: join  Del: erase  Clear: back";
    draw_footer_text(h, C_GRAY);
}
