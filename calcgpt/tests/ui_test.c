/* CALCGPT's keys and screens (src/ui.c + ../../shared/wifi_screen.c) pressed on a computer: [mode] (key 14) opens the Wi-Fi
 * screen and asks for a scan, [trace] opens the model menu, [clear] goes back, typing and Enter send a question, and the Wi-Fi
 * screen joins networks the right way. Run by test_calcgpt.py. */
#include <stdio.h>
#include <string.h>
#include "../src/ui.h"
#include "../../shared/keys.h"

static int fails;
static struct ui u;
static struct chat c;

#define CHECK(what, got, want) do { long g_ = (long)(got), w_ = (long)(want); if (g_ != w_) { printf("FAIL %s: got %ld want %ld\n", what, g_, w_); fails++; } } while (0)

static void press(int key)
{
    ui_key(&u, &c, key, true, 0);
}

/* the bytes the keys sent so far, as text with non-printing bytes as \xNN, then emptied */
static const char *sent(void)
{
    static char text[2000];
    int i, n = 0;
    for (i = 0; i < u.out.len; i++) {
        uint8_t b = u.out.buf[i];
        if (b >= 32 && b < 127) text[n++] = (char)b;
        else n += sprintf(text + n, "\\x%02x", b);
    }
    text[n] = 0;
    u.out.len = 0;
    return text;
}

static void board_says(const char *bytes, int n)
{
    chat_feed(&c, (const uint8_t *)bytes, n);
}

static void fresh(void)
{
    chat_init(&c);
    ui_init(&u);
}

static void test_screens(void)
{
    fresh();
    CHECK("starts on the chat", u.screen, SCR_CHAT);
    press(14);   /* [mode], group 1 bit 6, by its raw number */
    CHECK("key 14 opens the Wi-Fi screen", u.screen, SCR_WIFI);
    CHECK("the Wi-Fi screen shows the list", u.wifi.screen, WIFI_LIST);
    CHECK("it asks the board for a scan", strcmp(sent(), "W"), 0);
    CHECK("and shows Scanning... while a link is up", c.scanning, 1);
    press(K_CLEAR);
    CHECK("[clear] goes back to the chat", u.screen, SCR_CHAT);
    CHECK("going back sends nothing", strcmp(sent(), ""), 0);
    CHECK("the program did not quit", u.quit, 0);

    press(K_TRACE);
    CHECK("[trace] opens the model menu", u.menu_open, 1);
    CHECK("with no models yet it asks for the list", strcmp(sent(), "R"), 0);
    press(K_CLEAR);
    CHECK("[clear] closes the menu", u.menu_open, 0);
    CHECK("the chat is still there", u.screen, SCR_CHAT);

    /* the menu with models: K, L "fast", L "smart", U 0 */
    board_says("KL\x04" "fastL\x05" "smartU\x00", 15);
    CHECK("two models arrived", c.nmodels, 2);
    press(K_TRACE);
    CHECK("the menu opens without asking again", strcmp(sent(), ""), 0);
    CHECK("the menu starts on the model in use", u.menu_sel, 0);
    press(K_DOWN);
    press(K_DOWN);
    CHECK("Down stops at the last model", u.menu_sel, 1);
    press(K_ENTER);
    CHECK("Enter picks model 1", strcmp(sent(), "M\\x01"), 0);
    CHECK("and closes the menu", u.menu_open, 0);

    /* [mode] with no link: the screen opens but "Scanning..." is not shown */
    ui_key(&u, &c, K_MODE, false, 0);
    CHECK("Wi-Fi screen without a link", u.screen, SCR_WIFI);
    CHECK("no Scanning... without a link", c.scanning, 0);
    CHECK("the W still leaves (harmless)", strcmp(sent(), "W"), 0);
    press(K_CLEAR);
}

static void test_typing_and_sending(void)
{
    fresh();
    press(K_ALPHA);          /* letters on (typing_tables.h: h is group 6 bit 5, i is group 2 bit 4) */
    press(K(6, 5));          /* h */
    press(K(2, 4));          /* i */
    CHECK("typed hi", strcmp(u.input, "hi"), 0);
    press(K_ENTER);
    CHECK("Enter sends Q len text", strcmp(sent(), "Q\\x02hi"), 0);
    CHECK("the line is empty again", u.input_len, 0);
    CHECK("the chat is waiting for the answer", c.busy, 1);
    press(K(6, 5));
    press(K_ENTER);
    CHECK("no second question while busy", strcmp(sent(), ""), 0);
    CHECK("the typed text stays", strcmp(u.input, "h"), 0);
    press(K_CLEAR);
    CHECK("[clear] erases the line first", u.input_len, 0);
    CHECK("and does not quit yet", u.quit, 0);
    press(K_CLEAR);
    CHECK("[clear] on an empty line quits", u.quit, 1);

    fresh();
    press(K_GRAPH);
    CHECK("Graph: new chat", strcmp(sent(), "N"), 0);
    press(K_YEQU);
    CHECK("Y=: stop", strcmp(sent(), "S"), 0);
    CHECK("Up/Down/Del repeat, Enter does not", ui_repeats(K_UP) && ui_repeats(K_DOWN) && ui_repeats(K_DEL) && !ui_repeats(K_ENTER), 1);
}

static void test_wifi_screen(void)
{
    fresh();
    press(K_MODE);
    sent();
    /* the board answers: V "Wi-Fi: none", then X 2, Y entries: open "Cafe" (3 bars), locked and new "Home" (4 bars) */
    board_says("V\x0b" "Wi-Fi: none", 13);
    board_says("X\x02" "Y\x07" "\x03  CafeY\x07" "\x04* Home", 20);
    CHECK("two networks listed", c.nwifi, 2);
    CHECK("the list arriving ends Scanning...", c.scanning, 0);
    CHECK("Cafe is open", c.wifi[0].locked, 0);
    CHECK("Home is locked", c.wifi[1].locked, 1);

    press(K_ENTER);
    CHECK("Enter on an open network joins it at once: J len name len 0", strcmp(sent(), "J\\x04Cafe\\x00"), 0);
    CHECK("stays on the list", u.wifi.screen, WIFI_LIST);

    press(K_DOWN);
    press(K_ENTER);
    CHECK("Enter on a locked network asks for the password", u.wifi.screen, WIFI_PASS);
    CHECK("nothing sent yet", strcmp(sent(), ""), 0);
    press(K(3, 1));   /* 1 */
    press(K(3, 2));   /* 4 */
    CHECK("password typed", strcmp(u.wifi.pass, "14"), 0);
    press(K_ENTER);
    CHECK("Enter joins with the password", strcmp(sent(), "J\\x04Home\\x02" "14"), 0);
    CHECK("back on the list", u.wifi.screen, WIFI_LIST);

    press(K_DOWN);    /* "Other network..." */
    CHECK("the last row is Other network", u.wifi.sel, 2);
    press(K_DOWN);
    CHECK("Down stops there", u.wifi.sel, 2);
    press(K_ENTER);
    CHECK("Other network asks for a name", u.wifi.screen, WIFI_SSID);
    press(K_ENTER);
    CHECK("an empty name is not accepted", u.wifi.screen, WIFI_SSID);
    press(K(3, 1));
    press(K_ENTER);
    CHECK("then the password", u.wifi.screen, WIFI_PASS);
    press(K_CLEAR);
    CHECK("[clear] with nothing typed leaves the entry", u.wifi.screen, WIFI_LIST);
    CHECK("nothing was sent", strcmp(sent(), ""), 0);
    press(K_MODE);
    CHECK("[mode] on the list scans again", strcmp(sent(), "W"), 0);
    press(K_CLEAR);
    CHECK("[clear] on the list goes back to the chat", u.screen, SCR_CHAT);

    /* a locked network the board remembers ('+') joins at once with an empty password: the board uses the one it saved */
    fresh();
    press(K_MODE);
    sent();
    board_says("V\x0b" "Wi-Fi: none", 13);
    board_says("X\x01" "Y\x07" "\x04*+Home", 11);
    CHECK("Home is locked and remembered", c.wifi[0].locked && c.wifi[0].known, 1);
    press(K_ENTER);
    CHECK("Enter on a remembered network joins without asking", strcmp(sent(), "J\\x04Home\\x00"), 0);
    CHECK("no password screen", u.wifi.screen, WIFI_LIST);

    /* no board has answered (conn empty): Enter sends nothing, so the Mac over the cable never reads a name as commands */
    fresh();
    press(K_MODE);
    sent();
    board_says("X\x01" "Y\x05" "\x03  Lab", 9);
    press(K_ENTER);
    CHECK("no join before a board has spoken", strcmp(sent(), ""), 0);

    /* the scan timeout: 10 s with no list and Scanning... goes away */
    fresh();
    ui_key(&u, &c, K_MODE, true, 1000);
    CHECK("scanning", c.scanning, 1);
    CHECK("not yet", wifi_ui_tick(&u.wifi, &c, 1000 + 9 * 32768), 0);
    CHECK("after 10 s the screen changes", wifi_ui_tick(&u.wifi, &c, 1000 + 11 * 32768), 1);
    CHECK("no longer scanning", c.scanning, 0);
}

int main(void)
{
    test_screens();
    test_typing_and_sending();
    test_wifi_screen();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("calcgpt keys and screens: all passed\n");
    return 0;
}
