/* CALC's home screen keys (src/home.c) and its Settings = the shared Wi-Fi screen, pressed on a computer. Run by test_calc.py. */
#include <stdio.h>
#include <string.h>
#include "../src/home.h"
#include "../src/launch.h"
#include "../../shared/keys.h"
#include "../../shared/wifi_screen.h"

static int fails;

#define CHECK(what, got, want) do { long g_ = (long)(got), w_ = (long)(want); if (g_ != w_) { printf("FAIL %s: got %ld want %ld\n", what, g_, w_); fails++; } } while (0)

static void test_tiles(void)
{
    struct home h = {0};
    CHECK("four tiles", TILE_COUNT, 4);
    CHECK("tile 0 is Messages", strcmp(HOME_TILES[TILE_MESSAGES].title, "Messages"), 0);
    CHECK("tile 1 is Settings", strcmp(HOME_TILES[TILE_SETTINGS].title, "Settings"), 0);
    CHECK("tile 2 is ChatGPT", strcmp(HOME_TILES[TILE_CHATGPT].title, "ChatGPT"), 0);
    CHECK("tile 3 is Math", strcmp(HOME_TILES[TILE_MATH].title, "Math"), 0);
    CHECK("ChatGPT runs CALCGPT", strcmp(HOME_TILES[TILE_CHATGPT].program, "CALCGPT"), 0);
    CHECK("Math runs MATH", strcmp(HOME_TILES[TILE_MATH].program, "MATH"), 0);
    CHECK("Messages and Settings are screens inside CALC", HOME_TILES[TILE_MESSAGES].program[0] == 0 && HOME_TILES[TILE_SETTINGS].program[0] == 0, 1);

    CHECK("starts on Messages", h.sel, TILE_MESSAGES);
    CHECK("Left at the first tile stays", home_key(&h, K_LEFT), HOME_NONE);
    CHECK("still on the first tile", h.sel, 0);
    home_key(&h, K_RIGHT);
    CHECK("Right moves to Settings", h.sel, TILE_SETTINGS);
    home_key(&h, K_RIGHT);
    home_key(&h, K_RIGHT);
    CHECK("Right twice more: Math", h.sel, TILE_MATH);
    home_key(&h, K_RIGHT);
    CHECK("Right at the last tile stays", h.sel, TILE_MATH);
    home_key(&h, K_LEFT);
    CHECK("Left goes back to ChatGPT", h.sel, TILE_CHATGPT);
    CHECK("Enter opens", home_key(&h, K_ENTER), HOME_OPEN);
    CHECK("Enter does not move", h.sel, TILE_CHATGPT);
    CHECK("Clear quits", home_key(&h, K_CLEAR), HOME_QUIT);
    CHECK("a letter key does nothing", home_key(&h, K(3, 1)), HOME_NONE);
    CHECK("Mode does nothing on the home screen", home_key(&h, K_MODE), HOME_NONE);
}

/* Settings is the same Wi-Fi screen as CALCGPT's: opening it scans, Clear comes back */
static void test_settings(void)
{
    struct wifi_ui w;
    struct chat c;
    struct typing ty = {0};
    struct outbuf out = {.len = 0};
    chat_init(&c);
    wifi_ui_open(&w, &c, &out, true, 0);
    CHECK("opening Settings asks the board for a scan", out.len == 1 && out.buf[0] == 'W', 1);
    CHECK("it shows the list", w.screen, WIFI_LIST);
    out.len = 0;
    CHECK("Clear leaves Settings", wifi_ui_key(&w, &c, &ty, K_CLEAR, &out, true, 0), WIFI_BACK);
    CHECK("nothing else was sent", out.len, 0);
    wifi_ui_open(&w, &c, &out, false, 0);
    CHECK("with the box unplugged, no Scanning...", c.scanning, 0);
}

/* the TI-BASIC line CALC writes to start a program: Asm(prgmNAME) */
static void test_launch_tokens(void)
{
    uint8_t t[LAUNCH_TOKENS_MAX];
    static const uint8_t want[] = {0xBB, 0x6A, 0x5F, 'C', 'A', 'L', 'C', 'G', 'P', 'T'};
    int n = launch_tokens(t, "CALCGPT");
    CHECK("Asm(prgmCALCGPT) is 10 bytes", n, (int)sizeof(want));
    CHECK("the bytes", memcmp(t, want, sizeof(want)), 0);
    n = launch_tokens(t, "SOLVER");
    CHECK("Asm(prgmSOLVER) is 9 bytes", n, 9);
    CHECK("starts with Asm( prgm", t[0] == 0xBB && t[1] == 0x6A && t[2] == 0x5F, 1);
    CHECK("a lower-case or odd character is skipped", launch_tokens(t, "AB-c"), 5);
    CHECK("the longest name fits", launch_tokens(t, "ABCDEFGH1"), LAUNCH_TOKENS_MAX);
}

int main(void)
{
    test_tiles();
    test_settings();
    test_launch_tokens();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("calc (home screen, settings, launch): all passed\n");
    return 0;
}
