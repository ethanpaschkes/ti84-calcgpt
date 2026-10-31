/* CALC: a phone-like home screen for the TI-84 Plus CE, made for the Wi-Fi bridge box. Four tiles: Messages (coming soon),
 * Settings (the Wi-Fi setup screen shared with CALCGPT), ChatGPT (runs CALCGPT) and Math (runs MATH). The status bar shows the
 * time and Online / Offline: Online when the box is plugged in and its board says it has a server. Unplugged, Math and Settings
 * still work. It runs on top of the calculator's own operating system (it does not replace it).
 *
 * Keys: arrows move over the tiles, Enter opens, Clear leaves CALC. In a screen, Clear goes back.
 * Started from calcos/ (2026-10-24); the keys are in home.c, the link in ../../shared/link.c, the Wi-Fi screen in
 * ../../shared/wifi_screen.c + wifi_draw.c, the board's messages are parsed by ../calcgpt/src/chatmodel.c. */
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/power.h>
#include <sys/rtc.h>
#include <sys/timers.h>
#include <ti/vars.h>
#include "chatmodel.h"
#include "draw.h"
#include "home.h"
#include "keys.h"
#include "launch.h"
#include "link.h"
#include "timefmt.h"
#include "typing.h"
#include "wifi_draw.h"
#include "wifi_screen.h"

#define TILE_W 72
#define TILE_H 56
#define GRID_X 12
#define GRID_Y 72
#define SECOND 32768u

/* the home screen's palette numbers (the Wi-Fi screen uses the shared one from draw.h; CALC switches between them) */
enum {
    H_BLACK = 0, H_TEXT = 1, H_DIM = 2, H_BAR = 3, H_BLUE = 4, H_PURPLE = 5, H_GREEN = 6, H_ORANGE = 7, H_GRAY = 8,
    H_RED = 12, H_SEL = 14, H_LINE = 15, H_BG0 = 16 /* 16 steps of the background gradient: 16 to 31 */
};

enum { APP_HOME, APP_MESSAGES, APP_SETTINGS };

static struct chat chat;         /* parses the board's messages (connection line, Wi-Fi list, status) */
static struct wifi_ui wifi;
static struct typing ty;
static struct outbuf out;
static struct home home;
static int app = APP_HOME;
static uint8_t in_buf[512];
static bool asked_conn;          /* the board was asked for its connection line since the link came up */
static uint32_t asked_at;

/* ---------------------------------------------------------------- drawing */

static void home_palette(void)
{
    int i;
    gfx_palette[H_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[H_TEXT] = gfx_RGBTo1555(245, 248, 255);
    gfx_palette[H_DIM] = gfx_RGBTo1555(190, 205, 235);
    gfx_palette[H_BAR] = gfx_RGBTo1555(10, 18, 45);
    gfx_palette[H_BLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[H_PURPLE] = gfx_RGBTo1555(150, 88, 210);
    gfx_palette[H_GREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[H_ORANGE] = gfx_RGBTo1555(245, 130, 20);
    gfx_palette[H_GRAY] = gfx_RGBTo1555(105, 118, 140);
    gfx_palette[H_RED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[H_SEL] = gfx_RGBTo1555(255, 255, 255);
    gfx_palette[H_LINE] = gfx_RGBTo1555(60, 80, 130);
    for (i = 0; i < 16; i++) gfx_palette[H_BG0 + i] = gfx_RGBTo1555(18 + i * 3, 36 + i * 4, 88 + i * 6);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

static void text_centered(const char *s, int x, int w, int y, uint8_t color)
{
    text_at(s, x + (w - 8 * (int)strlen(s)) / 2, y, color);
}

/* a vertical gradient wallpaper */
static void wallpaper(void)
{
    int y;
    for (y = 0; y < 240; y++) {
        gfx_SetColor((uint8_t)(H_BG0 + y * 16 / 240));
        gfx_HorizLine(0, y, SCREEN_W);
    }
}

static void round_rect(int x, int y, int w, int h, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillRectangle(x + 4, y, w - 8, h);
    gfx_FillRectangle(x, y + 4, w, h - 8);
    gfx_FillCircle(x + 4, y + 4, 4);
    gfx_FillCircle(x + w - 5, y + 4, 4);
    gfx_FillCircle(x + 4, y + h - 5, 4);
    gfx_FillCircle(x + w - 5, y + h - 5, 4);
}

/* the box is plugged in and its board has a server */
static bool online(void)
{
    return link_up() && strstr(chat.conn, "| online") != NULL;
}

static void battery_icon(int x, int y)
{
    int level = boot_GetBatteryStatus(), i;
    if (level > 4) level = 4;
    gfx_SetColor(H_DIM);
    gfx_Rectangle(x, y, 22, 10);
    gfx_FillRectangle(x + 22, y + 3, 2, 4);
    for (i = 0; i < level; i++) {
        gfx_SetColor(level <= 1 ? H_RED : H_GREEN);
        gfx_FillRectangle(x + 2 + i * 5, y + 2, 4, 6);
    }
}

/* the status bar: time, Online / Offline, battery */
static void status_bar(const char *title)
{
    char buf[20];
    uint8_t hh, mm, ss;
    gfx_SetColor(H_BAR);
    gfx_FillRectangle(0, 0, SCREEN_W, HEAD_H + 4);
    text_at(title, 6, 5, H_TEXT);
    boot_GetTime(&ss, &mm, &hh);
    fmt_clock(hh, mm, 0, buf);
    text_centered(buf, 0, SCREEN_W, 5, H_TEXT);
    if (online()) text_at("Online", 214, 5, H_GREEN);
    else text_at("Offline", 206, 5, H_DIM);
    battery_icon(286, 4);
}

/* white icons inside the coloured tiles */
static void icon(int which, int cx, int cy, uint8_t tile)
{
    int i;
    gfx_SetColor(C_WHITE);
    switch (which) {
    case TILE_MESSAGES:   /* a speech bubble */
        gfx_FillRectangle(cx - 14, cy - 12, 28, 18);
        gfx_FillTriangle(cx - 8, cy + 6, cx - 2, cy + 6, cx - 10, cy + 13);
        gfx_SetColor(tile);
        gfx_FillRectangle(cx - 9, cy - 7, 18, 2);
        gfx_FillRectangle(cx - 9, cy - 2, 12, 2);
        break;
    case TILE_SETTINGS:   /* a gear */
        gfx_FillCircle(cx, cy, 11);
        for (i = 0; i < 4; i++) {
            gfx_FillRectangle(cx - 2 + (i == 1 ? 10 : i == 3 ? -10 : 0), cy - 2 + (i == 0 ? -12 : i == 2 ? 12 : 0), 4, 4);
        }
        gfx_SetColor(tile);
        gfx_FillCircle(cx, cy, 4);
        break;
    case TILE_CHATGPT:    /* a speech bubble with three dots */
        gfx_FillRectangle(cx - 14, cy - 12, 28, 18);
        gfx_FillTriangle(cx - 8, cy + 6, cx - 2, cy + 6, cx - 10, cy + 13);
        gfx_SetColor(tile);
        for (i = -1; i <= 1; i++) gfx_FillCircle(cx + i * 7, cy - 3, 2);
        break;
    default:              /* a calculator */
        gfx_FillRectangle(cx - 10, cy - 14, 20, 28);
        gfx_SetColor(tile);
        gfx_FillRectangle(cx - 7, cy - 11, 14, 6);
        for (i = 0; i < 9; i++) gfx_FillRectangle(cx - 7 + (i % 3) * 5, cy - 2 + (i / 3) * 5, 4, 3);
        break;
    }
}

static uint8_t tile_color(int which)
{
    switch (which) {
    case TILE_MESSAGES: return H_GREEN;
    case TILE_SETTINGS: return H_GRAY;
    case TILE_CHATGPT: return H_PURPLE;
    default: return H_BLUE;
    }
}

static void draw_home(void)
{
    int i;
    wallpaper();
    status_bar("CALC");
    text_centered(online() ? "Connected to the box" : link_up() ? "Box plugged in, no internet yet" : "Offline: plug in the box for ChatGPT",
                  0, SCREEN_W, 40, H_DIM);
    for (i = 0; i < TILE_COUNT; i++) {
        int x = GRID_X + i * (TILE_W + 4), y = GRID_Y;
        uint8_t color = tile_color(i);
        if (i == home.sel) round_rect(x - 3, y - 3, TILE_W + 6, TILE_H + 6, H_SEL);
        round_rect(x, y, TILE_W, TILE_H, color);
        icon(i, x + TILE_W / 2, y + 22, color);
        text_centered(HOME_TILES[i].title, x, TILE_W, y + TILE_H - 12, H_TEXT);
    }
    text_centered("made by Ethan Paschkes", 0, SCREEN_W, 170, H_DIM);
    text_centered("Enter: open    Clear: leave CALC", 0, SCREEN_W, 226, H_DIM);
    gfx_SwapDraw();
}

static void draw_messages(void)
{
    wallpaper();
    status_bar("Messages");
    gfx_SetTextScale(2, 2);
    text_at("Coming soon", 72, 90, H_TEXT);
    gfx_SetTextScale(1, 1);
    text_centered("Messages between calculators, through the box.", 0, SCREEN_W, 130, H_DIM);
    text_centered("Clear: back", 0, SCREEN_W, 226, H_DIM);
    gfx_SwapDraw();
}

static void draw_settings(void)
{
    gfx_FillScreen(C_WHITE);
    wifi_draw(&wifi, &chat, &ty, link_up());
    gfx_SwapDraw();
}

static void redraw(void)
{
    if (app == APP_HOME) draw_home();
    else if (app == APP_MESSAGES) draw_messages();
    else draw_settings();
}

/* ---------------------------------------------------------------- the link */

/* the board is asked for its connection line ('V') once the link is up, again every 2 s until it answers (it may still be
 * starting: the calculator powers it); forgotten when the link goes */
static void keep_in_touch(uint32_t now, bool *dirty)
{
    uint8_t cmd = 'I';
    if (link_lost()) {
        asked_conn = false;
        chat.conn[0] = 0;
        chat.scanning = 0;
        *dirty = true;
    }
    if (!link_up() || chat.conn[0]) return;
    if (!asked_conn || now - asked_at > 2 * SECOND) {
        link_send(&cmd, 1);
        asked_conn = true;
        asked_at = now;
    }
}

/* ---------------------------------------------------------------- keys */

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
                prev_keys[g] |= (uint8_t)(1 << b);   /* any other new key is returned by the next call */
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

static int launcher(int sel);

/* writes the TI-BASIC program `Asm(prgmNAME)`; false if the calculator refused (no room) */
static bool write_launch_prgm(const char *program)
{
    uint8_t tokens[LAUNCH_TOKENS_MAX];
    int n = launch_tokens(tokens, program);
    uint8_t h = ti_OpenVar(LAUNCH_PRGM, "w", OS_TYPE_PRGM);
    bool ok;
    if (!h) return false;
    ok = ti_Write(tokens, 1, (size_t)n, h) == (size_t)n;
    ti_Close(h);
    return ok;
}

/* called by the calculator when a program started from here has finished: start over and show the home screen again */
static int came_back(void *data, int retval)
{
    int sel = data ? *(int *)data : 0;
    (void)retval;
    return launcher(sel);
}

static void open_tile(uint32_t now)
{
    if (home.sel == TILE_MESSAGES) {
        app = APP_MESSAGES;
    } else if (home.sel == TILE_SETTINGS) {
        app = APP_SETTINGS;
        ui_palette();
        memset(&ty, 0, sizeof(ty));
        wifi_ui_open(&wifi, &chat, &out, link_up(), now);
    }
}

/* a key on the home screen or in a screen; returns true when CALC should end */
static bool handle_key(int key, uint32_t now)
{
    if (app == APP_HOME) {
        int did = home_key(&home, key);
        if (did == HOME_QUIT) return true;
        if (did == HOME_OPEN) open_tile(now);
    } else if (app == APP_MESSAGES) {
        if (key == K_CLEAR) app = APP_HOME;
    } else if (wifi_ui_key(&wifi, &chat, &ty, key, &out, link_up(), now) == WIFI_BACK) {
        app = APP_HOME;
        home_palette();
    }
    link_send(out.buf, out.len);
    out.len = 0;
    return false;
}

static int launcher(int sel)
{
    bool dirty = true, usb = false;
    int last_minute = -1;

    gfx_Begin();
    home_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    chat_init(&chat);
    out.len = 0;
    asked_conn = false;
    app = APP_HOME;
    home.sel = sel >= 0 && sel < TILE_COUNT ? sel : 0;
    usb = link_init();
    forget_keys();
    for (;;) {
        uint8_t hh, mm, ss;
        uint32_t now;
        int key, n;

        if (usb) link_poll();
        now = timer_Get(1);
        keep_in_touch(now, &dirty);
        if (wifi_ui_tick(&wifi, &chat, now)) dirty = true;
        n = link_read(in_buf, sizeof(in_buf));
        if (n > 0) chat_feed(&chat, in_buf, n);
        if (chat.changed) {
            chat.changed = 0;
            dirty = true;
        }
        key = next_key();
        if (key >= 0) {
            if (handle_key(key, now)) break;
            dirty = true;
            if (app == APP_HOME && key == K_ENTER && HOME_TILES[home.sel].program[0]) {
                int chosen = home.sel;
                if (write_launch_prgm(HOME_TILES[chosen].program)) {
                    if (usb) link_close();   /* the box loses power here and starts again when the program turns USB on */
                    gfx_End();
                    os_RunPrgm(LAUNCH_PRGM, &chosen, sizeof(chosen), came_back);
                    return 1;   /* only reached if CALCRUN could not be started */
                }
            }
        }
        boot_GetTime(&ss, &mm, &hh);
        if (mm != last_minute) {   /* the clock moves */
            last_minute = mm;
            dirty = true;
        }
        if (dirty) {
            dirty = false;
            redraw();
        }
    }
    if (usb) link_close();
    gfx_End();
    return 0;
}

int main(void)
{
    return launcher(0);
}
