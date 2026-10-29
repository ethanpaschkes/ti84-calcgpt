#include "wifi_screen.h"
#include <string.h>
#include "keys.h"

void wifi_ui_scan(struct wifi_ui *w, struct chat *c, struct outbuf *out, bool linked, uint32_t now)
{
    out_cmd(out, 'W');
    c->scanning = linked;
    w->scan_at = now;
}

void wifi_ui_open(struct wifi_ui *w, struct chat *c, struct outbuf *out, bool linked, uint32_t now)
{
    w->screen = WIFI_LIST;
    w->sel = 0;
    w->top = 0;
    wifi_ui_scan(w, c, out, linked, now);
}

void wifi_ui_keep_sel(struct wifi_ui *w, const struct chat *c)
{
    if (w->sel > c->nwifi) w->sel = c->nwifi;
    if (w->sel < w->top) w->top = w->sel;
    if (w->sel >= w->top + WIFI_ROWS) w->top = w->sel - WIFI_ROWS + 1;
}

bool wifi_ui_tick(struct wifi_ui *w, struct chat *c, uint32_t now)
{
    if (c->scanning && now - w->scan_at > WIFI_SCAN_WAIT) {
        c->scanning = 0;
        return true;
    }
    return false;
}

/* 'J' len ssid len password (length 0 = an open network) */
static void send_join(const struct wifi_ui *w, struct outbuf *out)
{
    uint8_t msg[3 + CHAT_SSID_MAX + WIFI_PASS_MAX];
    int n = 0;
    msg[n++] = 'J';
    msg[n++] = (uint8_t)w->ssid_len;
    memcpy(msg + n, w->ssid, (size_t)w->ssid_len);
    n += w->ssid_len;
    msg[n++] = (uint8_t)w->pass_len;
    memcpy(msg + n, w->pass, (size_t)w->pass_len);
    n += w->pass_len;
    out_bytes(out, msg, n);
}

/* Enter on the list: an open network or one the board remembers is joined at once (the board uses its saved password), a locked
 * new one asks for its password, "Other network" for a name.
 * Nothing is sent until a Wi-Fi board has answered: over the cable the Mac would read the name's letters as commands. */
static void pick_network(struct wifi_ui *w, const struct chat *c, struct outbuf *out)
{
    const struct chat_wifi *net = &c->wifi[w->sel];
    if (!c->conn[0]) return;
    w->pass_len = 0;
    w->pass[0] = 0;
    if (w->sel == c->nwifi) {
        w->ssid_len = 0;
        w->ssid[0] = 0;
        w->screen = WIFI_SSID;
        return;
    }
    strcpy(w->ssid, net->ssid);
    w->ssid_len = (int)strlen(w->ssid);
    if (net->locked && !net->known) w->screen = WIFI_PASS;
    else send_join(w, out);
}

/* a key typed into a line of text: Del erases, other keys go through the typing layer */
static void type_into(struct typing *ty, char *line, int *len, int max, int key)
{
    char ch = 0;
    if (key == K_DEL) {
        if (*len) line[--*len] = 0;
        typing_used_other_key(ty);
    } else if (typing_key(ty, key, &ch) == TY_CHAR && *len < max) {
        line[(*len)++] = ch;
        line[*len] = 0;
    }
}

int wifi_ui_key(struct wifi_ui *w, struct chat *c, struct typing *ty, int key, struct outbuf *out, bool linked, uint32_t now)
{
    char *line = w->screen == WIFI_SSID ? w->ssid : w->pass;
    int *len = w->screen == WIFI_SSID ? &w->ssid_len : &w->pass_len;
    if (w->screen == WIFI_LIST) {
        if (key == K_UP && w->sel > 0) w->sel--;
        else if (key == K_DOWN && w->sel < c->nwifi) w->sel++;
        else if (key == K_ENTER) pick_network(w, c, out);
        else if (key == K_MODE || key == K_GRAPH) wifi_ui_scan(w, c, out, linked, now);
        else if (key == K_CLEAR) return WIFI_BACK;
        wifi_ui_keep_sel(w, c);
    } else if (key == K_ENTER) {
        typing_used_other_key(ty);
        if (w->screen == WIFI_PASS) {
            send_join(w, out);
            w->screen = WIFI_LIST;
        } else if (w->ssid_len) {
            w->screen = WIFI_PASS;
        }
    } else if (key == K_CLEAR) {
        if (*len) {
            *len = 0;
            line[0] = 0;
        } else {
            w->screen = WIFI_LIST;
        }
    } else {
        type_into(ty, line, len, w->screen == WIFI_SSID ? CHAT_SSID_MAX : WIFI_PASS_MAX, key);
    }
    return WIFI_STAY;
}
