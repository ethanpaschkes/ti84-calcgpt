/* CALCGPT: a chat on the calculator. A language model runs on the Mac (mac/calcgptd.py, through Ollama); you type on the
 * calculator and the answer streams onto its screen. The messages are described at the top of calcgptd.py, and the Wi-Fi
 * setup messages for the bridge board in ../wireless/PROTOCOL.md (section 3).
 *
 * Keys: type your question (Alpha switches to letters, 2nd is a one-shot shift), Enter sends, Up/Down scroll,
 * Window/Zoom page up/down, Del erases, Graph starts a new chat, Y= stops an answer, Trace picks the AI model,
 * Mode opens the Wi-Fi setup, Clear erases the line then quits. */
#include <graphx.h>
#include <keypadc.h>
#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>
#include "chatmodel.h"
#include "typing.h"

#define SCREEN_W 320
#define HEAD_H 14
#define BODY_Y 18
#define ROW_H 10
#define MARGIN 8
#define FOOT_Y 228
#define WIFI_Y 40        /* the first row of the Wi-Fi list */
#define WIFI_ROW 14
#define WIFI_ROWS 12     /* rows of the list on screen */
#define WIFI_PASS_MAX 63 /* the longest WPA password */
#define SECOND 32768u    /* timer 1 ticks */

/* colours, as palette numbers */
enum { C_BLACK, C_LINK, C_GRAY, C_RED, C_GREEN, C_LINE, C_SEL, C_GBLUE, C_GRED, C_GYELLOW, C_GGREEN, C_WHITE = 255 };

/* keys: number = group * 8 + bit, as in typing_tables.h */
#define K(g, b) ((g) * 8 + (b))
#define K_GRAPH K(1, 0)
#define K_TRACE K(1, 1)
#define K_ZOOM K(1, 2)
#define K_WINDOW K(1, 3)
#define K_YEQU K(1, 4)
#define K_MODE K(1, 6)
#define K_DEL K(1, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_UP K(7, 3)

/* what is on screen: the chat, the Wi-Fi list, or typing a hidden network's name or a password */
enum { SCR_CHAT, SCR_WIFI, SCR_SSID, SCR_PASS };

static srl_device_t srl;
static bool has_srl;
static uint8_t srl_buf[4096];
static uint8_t in_buf[2048];

static struct chat chat;
static struct typing ty;
static char input[CHAT_INPUT_MAX + 1];
static int input_len;
static bool dirty = true;
static bool quit;
static int screen;
static bool menu_open;       /* the model menu is showing */
static int menu_sel;
static bool asked_models;    /* the list of models was asked for since the Mac link came up */
static uint32_t asked_at;    /* when it was last asked for */
static bool asked_conn;      /* the Wi-Fi board was asked for its connection line since the link came up */
static uint32_t scan_at;     /* when the last Wi-Fi scan was asked for */
static int wifi_sel, wifi_top;   /* the chosen row of the Wi-Fi list (the last row is "Other network") and the first on screen */
static char wifi_ssid[CHAT_SSID_MAX + 1];
static int wifi_ssid_len;
static char wifi_pass[WIFI_PASS_MAX + 1];
static int wifi_pass_len;

static usb_error_t handle_usb_event(usb_event_t event, void *event_data, usb_callback_data_t *callback_data)
{
    usb_error_t err = srl_UsbEventCallback(event, event_data, callback_data);
    if (err != USB_SUCCESS) return err;
    if (event == USB_DEVICE_CONNECTED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE)) usb_ResetDevice(event_data);
    if (event == USB_HOST_CONFIGURE_EVENT || (event == USB_DEVICE_ENABLED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE))) {
        usb_device_t device;
        if (has_srl) return USB_SUCCESS;
        device = event == USB_HOST_CONFIGURE_EVENT ? usb_FindDevice(NULL, NULL, USB_SKIP_HUBS) : event_data;
        if (!device) return USB_SUCCESS;
        if (srl_Open(&srl, device, srl_buf, sizeof(srl_buf), SRL_INTERFACE_ANY, 9600) == SRL_SUCCESS) {
            has_srl = true;
            dirty = true;
        }
    }
    if (event == USB_DEVICE_DISCONNECTED_EVENT && (usb_device_t)event_data == srl.dev) {
        srl_Close(&srl);
        has_srl = false;
        asked_models = false;
        asked_conn = false;
        chat.conn[0] = 0;
        chat.scanning = 0;
        dirty = true;
    }
    return USB_SUCCESS;
}

/* ---- requests to the Mac and the Wi-Fi board ---- */

static void send_bytes(const uint8_t *data, int n)
{
    if (has_srl) srl_Write(&srl, data, (size_t)n);
}

static void send_cmd(char cmd)
{
    uint8_t m = (uint8_t)cmd;
    send_bytes(&m, 1);
}

static void send_question(const char *s)
{
    uint8_t msg[2 + CHAT_INPUT_MAX];
    int n = (int)strlen(s);
    msg[0] = 'Q';
    msg[1] = (uint8_t)n;
    memcpy(msg + 2, s, (size_t)n);
    send_bytes(msg, 2 + n);
}

static void start_scan(void)
{
    send_cmd('W');
    chat.scanning = has_srl;
    scan_at = timer_Get(1);
}

/* 'J' len ssid len password (length 0 = an open network) */
static void send_join(void)
{
    uint8_t msg[3 + CHAT_SSID_MAX + WIFI_PASS_MAX];
    int n = 0;
    msg[n++] = 'J';
    msg[n++] = (uint8_t)wifi_ssid_len;
    memcpy(msg + n, wifi_ssid, (size_t)wifi_ssid_len);
    n += wifi_ssid_len;
    msg[n++] = (uint8_t)wifi_pass_len;
    memcpy(msg + n, wifi_pass, (size_t)wifi_pass_len);
    n += wifi_pass_len;
    send_bytes(msg, n);
}

/* ---- drawing ---- */

static void set_palette(void)
{
    gfx_palette[C_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[C_LINK] = gfx_RGBTo1555(25, 85, 200);
    gfx_palette[C_GRAY] = gfx_RGBTo1555(115, 115, 115);
    gfx_palette[C_RED] = gfx_RGBTo1555(200, 40, 40);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(20, 135, 70);
    gfx_palette[C_LINE] = gfx_RGBTo1555(205, 205, 205);
    gfx_palette[C_SEL] = gfx_RGBTo1555(226, 236, 255);
    gfx_palette[C_GBLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[C_GRED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[C_GYELLOW] = gfx_RGBTo1555(251, 188, 5);
    gfx_palette[C_GGREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

static void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

/* a copy of s that is at most w pixels wide, with bytes the font cannot show (a network's name may have any) drawn as '?' */
static const char *fit(const char *s, unsigned int w)
{
    static char out[CHAT_COLS + 1];
    int n = 0;
    while (s[n] && n < CHAT_COLS) {
        out[n] = s[n] >= 32 && s[n] < 127 ? s[n] : '?';
        n++;
    }
    out[n] = 0;
    while (n > 0 && gfx_GetStringWidth(out) > w) out[--n] = 0;
    return out;
}

static void draw_title(const char *title)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    text_at(title, 6, 3, C_BLACK);
}

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
    if (!has_srl) text_at("Not connected: run calcgptd.py on the Mac", 18, 184, C_RED);
}

/* a scroll bar at the right edge, when there are more rows than fit */
static void draw_scrollbar(int y, int track, int top, int view, int total)
{
    int thumb, pos;
    if (total <= view) return;
    thumb = track * view / total;
    pos = (track - thumb) * top / (total - view);
    gfx_SetColor(C_LINE);
    gfx_FillRectangle(SCREEN_W - 5, y, 3, track);
    gfx_SetColor(C_LINK);
    gfx_FillRectangle(SCREEN_W - 5, y + pos, 3, thumb < 4 ? 4 : thumb);
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

/* a rounded box for the line being typed */
static void pill(int x0, int y0, int x1, int h, uint8_t border)
{
    int r = h / 2;
    gfx_SetColor(border);
    gfx_FillCircle(x0 + r, y0 + r, r);
    gfx_FillCircle(x1 - r, y0 + r, r);
    gfx_FillRectangle(x0 + r, y0, x1 - x0 - 2 * r, h);
    gfx_SetColor(C_WHITE);
    gfx_FillCircle(x0 + r, y0 + r, r - 1);
    gfx_FillCircle(x1 - r, y0 + r, r - 1);
    gfx_FillRectangle(x0 + r, y0 + 1, x1 - x0 - 2 * r, h - 2);
}

/* a line being typed (the question, a network name or a password), its end shown when it is long, and the typing layer */
static void draw_field(const char *s, int len, int y, bool active)
{
    const char *shown = s + (len > 30 ? len - 30 : 0);
    pill(6, y, 314, 20, active ? C_GBLUE : C_LINE);
    text_at(shown, 20, y + 6, C_BLACK);
    if (active) {
        gfx_SetColor(C_BLACK);
        gfx_VertLine(20 + (int)gfx_GetStringWidth(shown), y + 3, 11);
    }
    text_at(ty.alpha ? (ty.shift ? "ABC" : "abc") : (ty.shift ? "^" : "123"), 284, y + 6, C_GRAY);
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
    if (chat.nmodels == 0) {
        text_at("none yet: is calcgptd.py on?", x + 8, y + 24, C_RED);
    }
    for (i = 0; i < chat.nmodels; i++) {
        int ry = y + 20 + i * row;
        if (i == menu_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(x + 4, ry - 2, w - 8, row);
        }
        text_at(i == menu_sel ? ">" : " ", x + 8, ry, C_GBLUE);
        text_at(chat.models[i], x + 24, ry, C_BLACK);
        if (i == chat.model_sel) text_at("(in use)", x + w - 8 - 8 * 8, ry, C_GREEN);
    }
    text_at("Up/Down  Enter: use  Clear: back", x + 8, y + h - 14, C_GRAY);
}

/* signal strength as four bars of growing height, the first `level` of them dark */
static void draw_bars(int x, int y, int level)
{
    int i;
    for (i = 0; i < 4; i++) {
        gfx_SetColor(i < level ? C_BLACK : C_LINE);
        gfx_FillRectangle(x + i * 4, y + 6 - i * 2, 3, 2 + i * 2);
    }
}

/* the chosen row stays inside the list (it can shrink after a new scan) and on screen */
static void wifi_keep_sel(void)
{
    if (wifi_sel > chat.nwifi) wifi_sel = chat.nwifi;
    if (wifi_sel < wifi_top) wifi_top = wifi_sel;
    if (wifi_sel >= wifi_top + WIFI_ROWS) wifi_top = wifi_sel - WIFI_ROWS + 1;
}

static void draw_wifi_list(void)
{
    int n = chat.nwifi + 1, i;   /* the networks, then "Other network" */
    wifi_keep_sel();
    for (i = wifi_top; i < n && i < wifi_top + WIFI_ROWS; i++) {
        int y = WIFI_Y + (i - wifi_top) * WIFI_ROW;
        if (i == wifi_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(4, y - 3, SCREEN_W - 12, WIFI_ROW);
        }
        if (i == chat.nwifi) {
            text_at("Other network...", 48, y, C_LINK);
        } else {
            const struct chat_wifi *w = &chat.wifi[i];
            draw_bars(10, y, w->signal);
            if (w->locked) text_at("*", 30, y, C_GRAY);
            if (w->known) text_at("+", 38, y, C_GREEN);
            text_at(w->ssid[0] ? fit(w->ssid, SCREEN_W - 60) : "(no name)", 48, y, C_BLACK);
        }
    }
    if (chat.nwifi == 0) text_at(chat.scanning ? "Looking for networks..." : "No networks found. Mode: look again",
                                 48, WIFI_Y + 2 * WIFI_ROW, C_GRAY);
    draw_scrollbar(WIFI_Y - 3, WIFI_ROWS * WIFI_ROW, wifi_top, WIFI_ROWS, n);
    text_at("*  needs a password    +  remembered", 10, 212, C_GRAY);
}

/* typing a hidden network's name, or the password of the chosen network */
static void draw_wifi_entry(void)
{
    if (screen == SCR_SSID) {
        text_at("Name of the network:", 10, 52, C_BLACK);
        draw_field(wifi_ssid, wifi_ssid_len, 66, true);
    } else {
        text_at("Password for", 10, 46, C_BLACK);
        text_at(fit(wifi_ssid, SCREEN_W - 20), 10, 58, C_LINK);
        draw_field(wifi_pass, wifi_pass_len, 72, true);
        text_at("Leave it empty for an open network.", 10, 122, C_GRAY);
    }
    text_at("Alpha = letters, 2nd = capitals/symbols", 10, 108, C_GRAY);
}

static void draw_wifi(void)
{
    draw_title("Wi-Fi");
    if (chat.scanning) text_at("Scanning...", SCREEN_W - 6 - (int)gfx_GetStringWidth("Scanning..."), 3, C_RED);
    if (!has_srl) text_at("Not connected to the Wi-Fi board", 6, 20, C_RED);
    else if (chat.conn[0]) text_at(fit(chat.conn, SCREEN_W - 12), 6, 20, C_LINK);
    else text_at("No answer from a Wi-Fi board yet", 6, 20, C_GRAY);
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, 32, SCREEN_W);
    if (screen == SCR_WIFI) draw_wifi_list();
    else draw_wifi_entry();
}

/* the longest of these hints that fits the footer */
static const char *hint(const char *long_hint, const char *short_hint)
{
    return gfx_GetStringWidth(long_hint) <= SCREEN_W - 8 ? long_hint : short_hint;
}

static void draw_footer(void)
{
    const char *h;
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, FOOT_Y - 3, SCREEN_W);
    if (chat.status[0]) {
        text_at(chat.status, 4, FOOT_Y, C_RED);
        return;
    }
    if (screen == SCR_CHAT && chat.busy) {
        text_at("Answering...   Y=: stop", 4, FOOT_Y, C_RED);
        return;
    }
    if (screen == SCR_CHAT) h = hint("Trace: model  Mode: Wi-Fi  Graph: new  Clear: quit", "Trace: model  Mode: Wi-Fi  Graph: new");
    else if (screen == SCR_WIFI) h = hint("Enter: join  Mode: scan again  Clear: back", "Enter: join  Mode: scan  Clear: back");
    else h = screen == SCR_SSID ? "Enter: next  Del: erase  Clear: back" : "Enter: join  Del: erase  Clear: back";
    text_at(h, 4, FOOT_Y, C_GRAY);
}

static void redraw(void)
{
    gfx_FillScreen(C_WHITE);
    if (screen == SCR_CHAT) {
        draw_header();
        if (chat.nrows == 0 && !chat.busy) draw_welcome();
        else draw_chat();
        draw_field(input, input_len, 201, !chat.busy);
    } else {
        draw_wifi();
    }
    draw_footer();
    if (menu_open) draw_menu();
    gfx_SwapDraw();
}

/* ---- keys ---- */

/* a key typed into a line of text: Del erases, other keys go through the typing layer */
static void type_into(char *line, int *len, int max, int key)
{
    char ch = 0;
    if (key == K_DEL) {
        if (*len) line[--*len] = 0;
        typing_used_other_key(&ty);
    } else if (typing_key(&ty, key, &ch) == TY_CHAR && *len < max) {
        line[(*len)++] = ch;
        line[*len] = 0;
    }
}

static void handle_menu_key(int key)
{
    if (key == K_UP && menu_sel > 0) {
        menu_sel--;
    } else if (key == K_DOWN && menu_sel < chat.nmodels - 1) {
        menu_sel++;
    } else if (key == K_ENTER) {
        if (chat.nmodels > 0 && menu_sel != chat.model_sel) {
            uint8_t m[2] = {'M', (uint8_t)menu_sel};
            send_bytes(m, 2);
        }
        menu_open = false;
    } else if (key == K_CLEAR || key == K_TRACE) {
        menu_open = false;
    }
}

/* Enter on the Wi-Fi list: an open network is joined at once, a locked one asks for its password, "Other network" for a name.
 * Nothing is sent until a Wi-Fi board has answered: over the cable the Mac would read the name's letters as commands. */
static void pick_network(void)
{
    const struct chat_wifi *w = &chat.wifi[wifi_sel];
    if (!chat.conn[0]) return;
    wifi_pass_len = 0;
    wifi_pass[0] = 0;
    if (wifi_sel == chat.nwifi) {
        wifi_ssid_len = 0;
        wifi_ssid[0] = 0;
        screen = SCR_SSID;
        return;
    }
    strcpy(wifi_ssid, w->ssid);
    wifi_ssid_len = (int)strlen(wifi_ssid);
    if (w->locked) screen = SCR_PASS;
    else send_join();
}

static void handle_wifi_key(int key)
{
    char *line = screen == SCR_SSID ? wifi_ssid : wifi_pass;
    int *len = screen == SCR_SSID ? &wifi_ssid_len : &wifi_pass_len;
    if (screen == SCR_WIFI) {
        if (key == K_UP && wifi_sel > 0) wifi_sel--;
        else if (key == K_DOWN && wifi_sel < chat.nwifi) wifi_sel++;
        else if (key == K_ENTER) pick_network();
        else if (key == K_MODE || key == K_GRAPH) start_scan();
        else if (key == K_CLEAR) screen = SCR_CHAT;
        wifi_keep_sel();
    } else if (key == K_ENTER) {
        typing_used_other_key(&ty);
        if (screen == SCR_PASS) {
            send_join();
            screen = SCR_WIFI;
        } else if (wifi_ssid_len) {
            screen = SCR_PASS;
        }
    } else if (key == K_CLEAR) {
        if (*len) {
            *len = 0;
            line[0] = 0;
        } else {
            screen = SCR_WIFI;
        }
    } else {
        type_into(line, len, screen == SCR_SSID ? CHAT_SSID_MAX : WIFI_PASS_MAX, key);
    }
}

static void handle_key(int key)
{
    dirty = true;
    if (menu_open) {
        handle_menu_key(key);
        return;
    }
    if (screen != SCR_CHAT) {
        handle_wifi_key(key);
        return;
    }
    if (key == K_TRACE) {
        if (chat.nmodels == 0) send_cmd('R');
        menu_sel = chat.model_sel;
        menu_open = true;
    } else if (key == K_MODE) {
        screen = SCR_WIFI;
        wifi_sel = 0;
        wifi_top = 0;
        start_scan();
    } else if (key == K_ENTER) {
        if (input_len && !chat.busy && has_srl) {
            chat_user(&chat, input);
            send_question(input);
            input_len = 0;
            input[0] = 0;
        }
        typing_used_other_key(&ty);
    } else if (key == K_CLEAR) {
        if (input_len) {
            input_len = 0;
            input[0] = 0;
        } else {
            quit = true;
        }
    } else if (key == K_GRAPH) {
        send_cmd('N');
        chat_clear(&chat);
    } else if (key == K_YEQU) {
        send_cmd('S');
    } else if (key == K_UP) {
        chat_scroll(&chat, -1, CHAT_VIEW_ROWS);
    } else if (key == K_DOWN) {
        chat_scroll(&chat, 1, CHAT_VIEW_ROWS);
    } else if (key == K_WINDOW) {
        chat_scroll(&chat, -(CHAT_VIEW_ROWS - 1), CHAT_VIEW_ROWS);
    } else if (key == K_ZOOM) {
        chat_scroll(&chat, CHAT_VIEW_ROWS - 1, CHAT_VIEW_ROWS);
    } else {
        type_into(input, &input_len, CHAT_INPUT_MAX, key);
    }
}

static bool repeats(int key)
{
    return key == K_UP || key == K_DOWN || key == K_DEL;
}

int main(void)
{
    uint8_t prev[8] = {0}, cur[8] = {0};
    int held = -1;
    uint32_t held_since = 0, next_repeat = 0;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    chat_init(&chat);
    if (usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS) != USB_SUCCESS) {
        usb_Cleanup();
        gfx_End();
        return 1;
    }

    while (!quit) {
        int g, b, n;
        uint32_t now;

        usb_HandleEvents();
        /* the Mac answers with its list of models. Asked again each second until it does: over the wireless bridge the first
         * request can be lost while the board is still starting up, and the Mac also uses it to tell CALCGPT from other programs */
        if (has_srl && chat.nmodels == 0 && (!asked_models || timer_Get(1) - asked_at > SECOND)) {
            send_cmd('R');
            asked_models = true;
            asked_at = timer_Get(1);
        }
        /* a Wi-Fi bridge board answers with its connection line ('V'); the Mac ignores this */
        if (has_srl && !asked_conn) {
            send_cmd('I');
            asked_conn = true;
        }
        /* no list within 10 seconds (no Wi-Fi board, only the Mac): stop showing "Scanning..." */
        if (chat.scanning && timer_Get(1) - scan_at > 10 * SECOND) {
            chat.scanning = 0;
            dirty = true;
        }
        if (has_srl) {
            n = srl_Read(&srl, in_buf, sizeof(in_buf));
            if (n > 0) chat_feed(&chat, in_buf, n);
        }
        if (chat.changed) {
            chat.changed = 0;
            dirty = true;
        }
        kb_Scan();
        for (g = 1; g <= 7; g++) cur[g] = kb_Data[g];
        now = timer_Get(1);
        for (g = 1; g <= 7; g++) {
            uint8_t pressed = cur[g] & (uint8_t)~prev[g];
            for (b = 0; b < 8; b++) {
                if (pressed & (1 << b)) {
                    handle_key(K(g, b));
                    held = repeats(K(g, b)) ? K(g, b) : -1;
                    held_since = now;
                    next_repeat = now + 13107;  /* about 0.4 s before a held key repeats */
                }
            }
            prev[g] = cur[g];
        }
        if (held >= 0) {
            if (!(cur[held / 8] & (1 << (held % 8)))) held = -1;
            else if (now >= next_repeat && now - held_since > 13107) {
                handle_key(held);
                next_repeat = now + 2300;      /* then about 14 times a second */
            }
        }
        if (dirty) {
            dirty = false;
            chat_layout(&chat, CHAT_VIEW_ROWS);
            redraw();
        }
    }
    send_cmd('Z');
    {
        int i;
        for (i = 0; i < 400; i++) usb_HandleEvents();  /* let the last message leave before the USB link is shut down */
    }
    usb_Cleanup();
    gfx_End();
    return 0;
}
