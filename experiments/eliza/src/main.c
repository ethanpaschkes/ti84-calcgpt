/* ELIZA for the TI-84 Plus CE: a chat screen around the rule engine in eliza.c.
 *   ABC mode (default): press the key with the green letter above it
 *     0 = space   . = period   (-) = ?   + = apostrophe
 *   123 mode (press 2nd to switch, 2nd again to go back): keys type what is
 *     printed on them: digits . + - * / ^ ( ) and Math = %  (for calculator use)
 *   Enter sends, Del erases, Clear empties the line, Mode quits.
 *   Ask it anything, or type math: 12*3, what is 20% of 50, twenty plus seven.
 */
#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "eliza.h"

#define BLACK 0
#define USER  1
#define HINT  2
#define WHITE 255

#define LOG_LINES 19
#define LINE_LEN  37
#define INPUT_LEN 72
#define LINE_H    10

static char log_text[LOG_LINES][LINE_LEN + 1];
static uint8_t log_color[LOG_LINES];
static int nlog;

static char input[INPUT_LEN + 1];
static int ilen;
static bool num_mode;

static char letter_char(uint8_t k)
{
    switch (k) {
    case sk_Math: return 'a';
    case sk_Apps: return 'b';
    case sk_Prgm: return 'c';
    case sk_Recip: return 'd';
    case sk_Sin: return 'e';
    case sk_Cos: return 'f';
    case sk_Tan: return 'g';
    case sk_Power: return 'h';
    case sk_Square: return 'i';
    case sk_Comma: return 'j';
    case sk_LParen: return 'k';
    case sk_RParen: return 'l';
    case sk_Div: return 'm';
    case sk_Log: return 'n';
    case sk_7: return 'o';
    case sk_8: return 'p';
    case sk_9: return 'q';
    case sk_Mul: return 'r';
    case sk_Ln: return 's';
    case sk_4: return 't';
    case sk_5: return 'u';
    case sk_6: return 'v';
    case sk_Sub: return 'w';
    case sk_Store: return 'x';
    case sk_1: return 'y';
    case sk_2: return 'z';
    case sk_0: return ' ';
    case sk_DecPnt: return '.';
    case sk_Chs: return '?';
    case sk_Add: return '\'';
    default: return 0;
    }
}

static char number_char(uint8_t k)
{
    switch (k) {
    case sk_0: return '0';
    case sk_1: return '1';
    case sk_2: return '2';
    case sk_3: return '3';
    case sk_4: return '4';
    case sk_5: return '5';
    case sk_6: return '6';
    case sk_7: return '7';
    case sk_8: return '8';
    case sk_9: return '9';
    case sk_DecPnt: return '.';
    case sk_Add: return '+';
    case sk_Sub: return '-';
    case sk_Chs: return '-';
    case sk_Mul: return '*';
    case sk_Div: return '/';
    case sk_Power: return '^';
    case sk_LParen: return '(';
    case sk_RParen: return ')';
    case sk_Math: return '%';
    case sk_Comma: return ',';
    default: return 0;
    }
}

static void push_line(const char *s, int n, uint8_t color)
{
    if (nlog == LOG_LINES) {
        memmove(log_text[0], log_text[1], (LOG_LINES - 1) * (LINE_LEN + 1));
        memmove(log_color, log_color + 1, LOG_LINES - 1);
        nlog--;
    }
    memcpy(log_text[nlog], s, n);
    log_text[nlog][n] = 0;
    log_color[nlog] = color;
    nlog++;
}

/* Add text to the chat log, wrapping at spaces. */
static void add_log(const char *text, uint8_t color)
{
    while (*text) {
        int n = (int)strlen(text);
        if (n > LINE_LEN) {
            int b = LINE_LEN;
            n = LINE_LEN;
            while (b > 0 && text[b] != ' ') {
                b--;
            }
            if (b > 0) {
                n = b;
            }
        }
        push_line(text, n, color);
        text += n;
        while (*text == ' ') {
            text++;
        }
    }
}

static void redraw(bool typing)
{
    int i;
    int start = ilen > LINE_LEN - 2 ? ilen - (LINE_LEN - 2) : 0;

    gfx_FillScreen(WHITE);
    for (i = 0; i < nlog; i++) {
        gfx_SetTextFGColor(log_color[i]);
        gfx_PrintStringXY(log_text[i], 8, 4 + i * LINE_H);
    }
    if (typing) {
        gfx_SetTextFGColor(HINT);
        gfx_PrintStringXY("...", 8, 4 + nlog * LINE_H);
    }
    gfx_SetColor(HINT);
    gfx_HorizLine(0, 208, 320);
    gfx_SetTextFGColor(USER);
    gfx_PrintStringXY(">", 4, 214);
    gfx_PrintStringXY(input + start, 16, 214);
    gfx_SetColor(USER);
    gfx_VertLine(16 + (ilen - start) * 8, 213, 10);
    gfx_SetTextFGColor(HINT);
    gfx_PrintStringXY(num_mode ? "123 mode: digits + - * / ^ ( )  2nd=ABC"
                               : "ABC mode: 0=space (-)=?  2nd=123  Mode=quit", 4, 228);
    gfx_SwapDraw();
}

static void send_message(void)
{
    char shown[INPUT_LEN + 3];
    const char *reply;
    int len;
    uint16_t wait;

    strcpy(shown, "> ");
    strcat(shown, input);
    add_log(shown, USER);
    ilen = 0;
    input[0] = 0;
    redraw(true);

    reply = eliza_reply(shown + 2);
    len = (int)strlen(reply);
    wait = (uint16_t)(500 + len * 40);
    if (wait > 2800) {
        wait = 2800;
    }
    delay(wait);
    add_log(reply, BLACK);
}

int main(void)
{
    bool quit = false;

    gfx_Begin();
    gfx_palette[USER] = gfx_RGBTo1555(40, 90, 220);
    gfx_palette[HINT] = gfx_RGBTo1555(130, 130, 130);
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(WHITE);
    gfx_SetTextTransparentColor(WHITE);

    add_log("Hello. I'm Eliza. Talk to me, or give me math to do.", BLACK);

    while (!quit) {
        uint8_t k;

        redraw(false);
        do {
            k = os_GetCSC();
        } while (!k);

        if (k == sk_Mode) {
            quit = true;
        } else if (k == sk_Enter) {
            if (ilen) {
                send_message();
            }
        } else if (k == sk_Del) {
            if (ilen) {
                input[--ilen] = 0;
            }
        } else if (k == sk_Clear) {
            ilen = 0;
            input[0] = 0;
        } else if (k == sk_2nd) {
            num_mode = !num_mode;
        } else {
            char c = num_mode ? number_char(k) : letter_char(k);
            if (c && ilen < INPUT_LEN) {
                input[ilen++] = c;
                input[ilen] = 0;
            }
        }
    }

    gfx_End();
    return 0;
}
