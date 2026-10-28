/* CalcGPT: a ChatGPT-style chat screen for the TI-84 Plus CE.
 * There is no real language model in here: replies come from a built-in answer
 * list (kb.c), a small calculator (calc.c) and the classic Eliza rules.
 *
 *   Letters: press the key with the green letter above it
 *     0 = space   . = period   (-) = ?   + = apostrophe
 *   2nd switches to 123 mode (digits and + - * / ^ ( ) %); press it again for letters.
 *   Enter sends     Del erases     Clear empties the line
 *   Y=    choose the model        Zoom  start a new chat (also stops a solve)
 *   "solve boyle" (or just "solve") walks you through an equation: type a number
 *     for each value and ? (the (-) key, or VARS / X in 123 mode) for the unknown
 *   1, 2, 3 (with an empty line) pick one of the three follow-up buttons;
 *     press Alpha first to type a word starting with Y or Z
 *   Up/Down scroll the chat (or pick a suggestion on the new-chat screen)
 *   Mode  quits
 */
#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "eliza.h"
#include "kb.h"
#include "solver.h"
#include "tri.h"

/* palette (set in main) */
#define C_BG     0
#define C_TEXT   1
#define C_BUBBLE 2
#define C_MUTED  3
#define C_GREEN  4
#define C_LINE   5
#define C_SEL    6
#define C_TRANSP 255

#define SCREEN_W 320
#define VIEW_TOP 24
#define VIEW_BOTTOM 206
#define SUGG_TOP 165 /* three follow-up buttons fill 165..206 when shown */
#define LH   11
#define GAP  8
#define BOT_WRAP  33
#define USER_WRAP 26
#define MAX_LINES 70
#define INPUT_MAX 80

enum { R_USER, R_BOT, R_DIM };
enum { F_FIRST = 1, F_LAST = 2 };
enum { MODEL_20B, MODEL_120B, MODEL_ELIZA, NUM_MODELS };

struct line {
    char text[BOT_WRAP + 1];
    uint8_t role;
    uint8_t flags;
    uint8_t width; /* longest line of the message, for bubble width */
};

static struct line lines[MAX_LINES];
static int nlines;
static int scroll;

static int stream_first = -1; /* first line of the reply being typed out */
static int stream_chars;      /* characters of it shown so far */

static char input[INPUT_MAX + 1];
static int ilen;
static bool num_mode;
static int model = MODEL_20B;
static int chip_sel;
static unsigned long ticks;
static char sugg[3][40];     /* follow-up prompts shown as buttons 1-3 */
static int nsugg;
static const char *pending_next;
static bool alpha_next;      /* Alpha pressed: the next key types a letter */

static const char *const model_name[NUM_MODELS] = {"gpt-oss-20b", "gpt-oss-120b", "Eliza"};
static const char *const model_desc[NUM_MODELS] = {
    "Open-weight, fast", "Open-weight, more detail", "Classic 1966 chatbot",
};
static void set_palette(void)
{
    gfx_palette[C_BG]     = gfx_RGBTo1555(33, 33, 33);
    gfx_palette[C_TEXT]   = gfx_RGBTo1555(236, 236, 236);
    gfx_palette[C_BUBBLE] = gfx_RGBTo1555(52, 52, 52);
    gfx_palette[C_MUTED]  = gfx_RGBTo1555(150, 150, 150);
    gfx_palette[C_GREEN]  = gfx_RGBTo1555(16, 163, 127);
    gfx_palette[C_LINE]   = gfx_RGBTo1555(78, 78, 78);
    gfx_palette[C_SEL]    = gfx_RGBTo1555(70, 70, 70);
}

/* ---------- typing ---------- */

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

/* 123 mode: keys type what is printed on them, and the function keys type the
 * word (sin, cos, tan, log, ln) so you can ask "explain sin". */
static const char *number_text(uint8_t k)
{
    switch (k) {
    case sk_0: return "0";
    case sk_1: return "1";
    case sk_2: return "2";
    case sk_3: return "3";
    case sk_4: return "4";
    case sk_5: return "5";
    case sk_6: return "6";
    case sk_7: return "7";
    case sk_8: return "8";
    case sk_9: return "9";
    case sk_DecPnt: return ".";
    case sk_Add: return "+";
    case sk_Sub: return "-";
    case sk_Chs: return "-";
    case sk_Mul: return "*";
    case sk_Div: return "/";
    case sk_Power: return "^";
    case sk_LParen: return "(";
    case sk_RParen: return ")";
    case sk_Square: return "^2";
    case sk_Recip: return "^-1";
    case sk_Math: return "%";
    case sk_Comma: return ",";
    case sk_Store: return " ";
    case sk_Sin: return "sin ";
    case sk_Cos: return "cos ";
    case sk_Tan: return "tan ";
    case sk_Log: return "log ";
    case sk_Ln: return "ln ";
    case sk_Prgm: return "pi";
    case sk_Apps: return "=";
    case sk_Vars: return "?";
    case sk_GraphVar: return "x";
    default: return "";
    }
}

/* ---------- chat log ---------- */

static void drop_first_message(void)
{
    int n = 1;
    while (n < nlines && !(lines[n].flags & F_FIRST)) {
        n++;
    }
    memmove(&lines[0], &lines[n], (nlines - n) * sizeof(lines[0]));
    nlines -= n;
    if (stream_first >= 0) {
        stream_first -= n;
    }
}

/* Length of the next line of t: at most `wrap` characters, ending at a newline
 * or at the last space that fits. */
static int line_len(const char *t, int wrap)
{
    int i, b;
    for (i = 0; t[i] && t[i] != '\n' && i < wrap; i++) {
    }
    if (!t[i] || t[i] == '\n' || t[i] == ' ') {
        return i;
    }
    for (b = wrap; b > 0 && t[b] != ' '; b--) {
    }
    return b > 0 ? b : wrap;
}

static const char *next_line(const char *t, int n)
{
    t += n;
    if (*t == '\n') {
        return t + 1;
    }
    while (*t == ' ') {
        t++;
    }
    return t;
}

/* Add a message, wrapped at spaces and at newlines. Returns the index of its
 * first line. */
static int add_message(const char *text, int role, bool gap_after)
{
    int wrap = role == R_USER ? USER_WRAP : BOT_WRAP;
    int first, count = 0, maxw = 0, i;
    const char *t;

    for (t = text; *t;) {
        int n = line_len(t, wrap);
        count++;
        if (n == 0 && *t != '\n') {
            break;
        }
        t = next_line(t, n);
    }
    while (nlines + count > MAX_LINES && nlines > 0) {
        drop_first_message();
    }

    first = nlines;
    for (t = text; *t && nlines < MAX_LINES;) {
        int n = line_len(t, wrap);
        memcpy(lines[nlines].text, t, n);
        lines[nlines].text[n] = 0;
        lines[nlines].role = (uint8_t)role;
        lines[nlines].flags = 0;
        if (n > maxw) {
            maxw = n;
        }
        nlines++;
        if (n == 0 && *t != '\n') {
            break;
        }
        t = next_line(t, n);
    }
    if (nlines == first) {
        lines[nlines].text[0] = 0;
        lines[nlines].role = (uint8_t)role;
        lines[nlines].flags = 0;
        nlines++;
    }
    lines[first].flags |= F_FIRST;
    if (gap_after) {
        lines[nlines - 1].flags |= F_LAST;
    }
    for (i = first; i < nlines; i++) {
        lines[i].width = (uint8_t)maxw;
    }
    scroll = 0;
    return first;
}

static int line_height(int i)
{
    const struct line *l = &lines[i];
    int h = LH;
    if (l->role == R_USER) {
        if (l->flags & F_FIRST) h += 4;
        if (l->flags & F_LAST) h += 4;
    }
    if (l->flags & F_LAST) h += GAP;
    return h;
}

/* ---------- drawing ---------- */

static void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

static void draw_header(void)
{
    int w = (int)gfx_GetStringWidth(model_name[model]);

    text_at(model_name[model], 10, 6, C_TEXT);
    gfx_SetColor(C_MUTED);
    gfx_FillTriangle(10 + w + 6, 9, 10 + w + 14, 9, 10 + w + 10, 14);
    text_at("Y=", 10 + w + 20, 6, C_MUTED);
    text_at("Zoom:new", SCREEN_W - 74, 6, C_MUTED);
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, 20, SCREEN_W);
}

static void draw_empty_state(void)
{
    int i;
    const char *title = "What can I help with?";
    int w = (int)gfx_GetStringWidth(title);

    text_at(title, (SCREEN_W - w) / 2, 34, C_TEXT);
    for (i = 0; i < app_chip_count; i++) {
        int y = 62 + i * 23;
        int cw = 176;
        int x = (SCREEN_W - cw) / 2;
        int tw = (int)gfx_GetStringWidth(app_chips[i]);
        if (i == chip_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(x, y, cw, 19);
        }
        gfx_SetColor(i == chip_sel ? C_MUTED : C_LINE);
        gfx_Rectangle(x, y, cw, 19);
        text_at(app_chips[i], x + (cw - tw) / 2, y + 6, i == chip_sel ? C_TEXT : C_MUTED);
    }
}

static void draw_user_line(int i, int y)
{
    const struct line *l = &lines[i];
    int bw = l->width * 8 + 16;
    int x = SCREEN_W - 10 - bw;
    int top = y;
    int h = LH + ((l->flags & F_FIRST) ? 4 : 0) + ((l->flags & F_LAST) ? 4 : 0);

    gfx_SetColor(C_BUBBLE);
    gfx_FillRectangle(x, top, bw, h);
    gfx_SetColor(C_BG);
    if (l->flags & F_FIRST) {
        gfx_SetPixel(x, top);
        gfx_SetPixel(x + 1, top);
        gfx_SetPixel(x, top + 1);
        gfx_SetPixel(x + bw - 1, top);
        gfx_SetPixel(x + bw - 2, top);
        gfx_SetPixel(x + bw - 1, top + 1);
    }
    if (l->flags & F_LAST) {
        gfx_SetPixel(x, top + h - 1);
        gfx_SetPixel(x + 1, top + h - 1);
        gfx_SetPixel(x, top + h - 2);
        gfx_SetPixel(x + bw - 1, top + h - 1);
        gfx_SetPixel(x + bw - 2, top + h - 1);
        gfx_SetPixel(x + bw - 1, top + h - 2);
    }
    text_at(l->text, x + 8, top + 1 + ((l->flags & F_FIRST) ? 4 : 0), C_TEXT);
}

static void draw_bot_line(int i, int y, const char *shown)
{
    const struct line *l = &lines[i];
    bool avatar = (l->flags & F_FIRST) && !(i > 0 && lines[i - 1].role == R_DIM);

    if (avatar) {
        gfx_SetColor(C_GREEN);
        gfx_FillCircle(13, y + 5, 5);
        gfx_SetColor(C_BG);
        gfx_FillCircle(13, y + 5, 2);
    }
    text_at(shown, 26, y + 1, l->role == R_DIM ? C_MUTED : C_TEXT);
}

static bool suggestions_visible(void)
{
    return nsugg > 0 && ilen == 0 && nlines > 0;
}

static void draw_chat(void)
{
    int bottom = nlines - 1 - scroll;
    int view_bottom = suggestions_visible() ? SUGG_TOP - 3 : VIEW_BOTTOM;
    int view_h = view_bottom - VIEW_TOP;
    int total = 0, start = bottom, i, y;
    char shown[BOT_WRAP + 1];

    if (bottom < 0) {
        return;
    }
    while (start >= 0 && total + line_height(start) <= view_h * 2) {
        total += line_height(start);
        start--;
    }
    start++;
    /* keep only what fits: drop lines from the top until it does */
    while (total > view_h && start < bottom) {
        total -= line_height(start);
        start++;
    }
    /* short chats start at the top; longer ones sit against the input box */
    y = (start == 0 && total < view_h) ? VIEW_TOP : view_bottom - total;

    for (i = start; i <= bottom; i++) {
        const struct line *l = &lines[i];
        int len = (int)strlen(l->text);
        int show = len;

        if (stream_first >= 0 && i >= stream_first) {
            int before = 0, k;
            for (k = stream_first; k < i; k++) {
                before += (int)strlen(lines[k].text);
            }
            show = stream_chars - before;
            if (show < 0) show = 0;
            if (show > len) show = len;
        }
        memcpy(shown, l->text, show);
        shown[show] = 0;

        if (l->role == R_USER) {
            draw_user_line(i, y);
        } else {
            draw_bot_line(i, y, shown);
        }
        y += line_height(i);
    }
}

static void draw_input(void)
{
    int start = ilen > 27 ? ilen - 27 : 0;
    bool active = ilen > 0;

    gfx_SetColor(C_BUBBLE);
    gfx_FillRectangle(20, 211, SCREEN_W - 40, 24);
    gfx_FillCircle(20, 223, 12);
    gfx_FillCircle(SCREEN_W - 20, 223, 12);

    if (active) {
        text_at(input + start, 22, 219, C_TEXT);
        gfx_SetColor(C_TEXT);
        gfx_VertLine(22 + (ilen - start) * 8, 217, 11);
    } else {
        text_at("Message", 22, 219, C_MUTED);
    }
    if (alpha_next) {
        text_at("ALPHA", SCREEN_W - 76, 219, C_GREEN);
    } else {
        text_at(num_mode ? "123" : "ABC", SCREEN_W - 64, 219, num_mode ? C_GREEN : C_MUTED);
    }

    gfx_SetColor(active ? C_TEXT : C_MUTED);
    gfx_FillCircle(SCREEN_W - 22, 223, 9);
    gfx_SetColor(C_BG);
    gfx_FillTriangle(SCREEN_W - 22, 217, SCREEN_W - 27, 223, SCREEN_W - 17, 223);
    gfx_FillRectangle(SCREEN_W - 23, 223, 3, 5);
}

/* Follow-up questions as numbered buttons, like ChatGPT's suggestions. */
static void draw_suggestions(void)
{
    int i;
    for (i = 0; i < nsugg; i++) {
        int y = SUGG_TOP + i * 14;
        int w = (int)gfx_GetStringWidth(sugg[i]) + 30;
        char digit[2];

        if (w > SCREEN_W - 20) {
            w = SCREEN_W - 20;
        }
        gfx_SetColor(C_LINE);
        gfx_Rectangle(10, y, w, 13);
        gfx_SetColor(C_GREEN);
        gfx_FillCircle(20, y + 6, 5);
        digit[0] = (char)('1' + i);
        digit[1] = 0;
        text_at(digit, 16, y + 3, C_BG);
        text_at(sugg[i], 30, y + 3, C_TEXT);
    }
}

static void draw_model_menu(int pick)
{
    int i;
    const int x = 30, y = 30, w = 260, h = 176;

    gfx_SetColor(C_BUBBLE);
    gfx_FillRectangle(x, y, w, h);
    gfx_SetColor(C_LINE);
    gfx_Rectangle(x, y, w, h);
    text_at("Model", x + 12, y + 8, C_MUTED);
    for (i = 0; i < NUM_MODELS; i++) {
        int ry = y + 26 + i * 32;
        if (i == pick) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(x + 6, ry, w - 12, 28);
        }
        text_at(model_name[i], x + 18, ry + 3, C_TEXT);
        text_at(model_desc[i], x + 18, ry + 15, C_MUTED);
        if (i == model) {
            gfx_SetColor(C_GREEN);
            gfx_FillCircle(x + w - 22, ry + 14, 5);
        }
    }
    text_at("Demo: built-in answers on this", x + 12, y + h - 30, C_MUTED);
    text_at("calculator, not the real model.", x + 12, y + h - 18, C_MUTED);
}

static void redraw(bool menu_open, int menu_pick)
{
    gfx_FillScreen(C_BG);
    draw_header();
    if (nlines == 0) {
        draw_empty_state();
    } else {
        draw_chat();
    }
    gfx_SetColor(C_BG);
    gfx_FillRectangle(0, 207, SCREEN_W, 33);
    draw_input();
    if (suggestions_visible() && !menu_open) {
        draw_suggestions();
    }
    if (menu_open) {
        draw_model_menu(menu_pick);
    }
    gfx_SwapDraw();
}

/* ---------- replies ---------- */

static char reply_buf[700];

static void set_suggestions(const char *next)
{
    nsugg = 0;
    while (next && *next && nsugg < 3) {
        int n = 0;
        while (next[n] && next[n] != '|') {
            n++;
        }
        if (n > 0 && n < (int)sizeof(sugg[0])) {
            memcpy(sugg[nsugg], next, n);
            sugg[nsugg][n] = 0;
            nsugg++;
        }
        next += n;
        if (*next == '|') {
            next++;
        }
    }
}

static const char *make_reply(const char *text)
{
    pending_next = model == MODEL_ELIZA ? NULL : app_default_next;
    if (model != MODEL_ELIZA && solver_handle(text, reply_buf, sizeof(reply_buf))) {
        pending_next = solver_next();
        return reply_buf;
    }
    if (tri_try(text, reply_buf, sizeof(reply_buf))) {
        return reply_buf;
    }
    if (calc_try(text, reply_buf, sizeof(reply_buf))) {
        if (reply_buf[0] == '=' && reply_buf[1] == ' ') {
            static char nice[320];
            strcpy(nice, "That's ");
            strncat(nice, reply_buf + 2, sizeof(nice) - 10);
            strcat(nice, ".");
            return nice;
        }
        return reply_buf;
    }
    if (model == MODEL_ELIZA) {
        return eliza_reply(text);
    }
    if (kb_answer(text, model == MODEL_120B, reply_buf, sizeof(reply_buf))) {
        if (kb_next()) {
            pending_next = kb_next();
        }
        return reply_buf;
    }
    return kb_fallback();
}

static void wait_for_release(void)
{
    while (kb_AnyKey()) {
    }
    while (os_GetCSC()) {
    }
}

static void stream_reply(const char *reply)
{
    int first = add_message(reply, R_BOT, true);
    int total = 0, i, step;

    for (i = first; i < nlines; i++) {
        total += (int)strlen(lines[i].text);
    }
    stream_first = first;
    stream_chars = 0;
    step = total > 120 ? total / 40 : 3;
    while (stream_chars < total) {
        stream_chars += step;
        redraw(false, 0);
        if (kb_AnyKey()) {
            break;
        }
        delay(20);
    }
    stream_first = -1;
    set_suggestions(pending_next);
    redraw(false, 0);
    wait_for_release();
}

static void send_text(const char *text)
{
    const char *reply;
    int idx = -1;
    int k;

    nsugg = 0;
    add_message(text, R_USER, true);
    ilen = 0;
    input[0] = 0;
    srand((unsigned)ticks);

    if (model != MODEL_ELIZA) {
        static const char *const dots[3] = {"Thinking.", "Thinking..", "Thinking..."};
        idx = add_message(dots[0], R_DIM, false);
        for (k = 0; k < 4; k++) {
            strcpy(lines[idx].text, dots[k % 3]);
            redraw(false, 0);
            delay(220);
        }
        strcpy(lines[idx].text, "Thought for 1s");
    } else {
        redraw(false, 0);
        delay(400);
    }
    {
        bool was_solving = solver_active();
        reply = make_reply(text);
        if (solver_active()) {
            num_mode = true;
        } else if (was_solving) {
            num_mode = false;
        }
    }
    stream_reply(reply);
}

static void new_chat(void)
{
    solver_cancel();
    nlines = 0;
    nsugg = 0;
    scroll = 0;
    chip_sel = 0;
    ilen = 0;
    input[0] = 0;
}

static void model_menu(void)
{
    int pick = model;
    for (;;) {
        uint8_t k;
        redraw(true, pick);
        do {
            k = os_GetCSC();
        } while (!k);
        if (k == sk_Up) {
            pick = (pick + NUM_MODELS - 1) % NUM_MODELS;
        } else if (k == sk_Down) {
            pick = (pick + 1) % NUM_MODELS;
        } else if (k == sk_Enter) {
            model = pick;
            return;
        } else if (k == sk_Yequ || k == sk_Clear || k == sk_Mode) {
            return;
        }
    }
}

int main(void)
{
    bool quit = false;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_TRANSP);
    gfx_SetTextTransparentColor(C_TRANSP);

    while (!quit) {
        uint8_t k;

        redraw(false, 0);
        do {
            k = os_GetCSC();
            ticks++;
        } while (!k);

        if (k == sk_Mode) {
            quit = true;
        } else if (k == sk_Yequ) {
            model_menu();
        } else if (k == sk_Zoom) {
            new_chat();
        } else if (k == sk_2nd) {
            num_mode = !num_mode;
            alpha_next = false;
        } else if (k == sk_Alpha) {
            alpha_next = !alpha_next;
        } else if (!num_mode && !alpha_next && suggestions_visible()
                   && (k == sk_1 || k == sk_2 || k == sk_3)) {
            int pick = k == sk_1 ? 0 : k == sk_2 ? 1 : 2;
            if (pick < nsugg) {
                char chosen[40];
                strcpy(chosen, sugg[pick]);
                send_text(chosen);
            }
        } else if (k == sk_Enter) {
            if (ilen) {
                send_text(input);
            } else if (nlines == 0) {
                send_text(app_chips[chip_sel]);
            }
        } else if (k == sk_Del) {
            if (ilen) {
                input[--ilen] = 0;
            }
        } else if (k == sk_Clear) {
            ilen = 0;
            input[0] = 0;
        } else if (k == sk_Up) {
            if (nlines == 0) {
                chip_sel = (chip_sel + app_chip_count - 1) % app_chip_count;
            } else if (scroll < nlines - 1) {
                scroll++;
            }
        } else if (k == sk_Down) {
            if (nlines == 0) {
                chip_sel = (chip_sel + 1) % app_chip_count;
            } else if (scroll > 0) {
                scroll--;
            }
        } else {
            const char *t;
            char one[2];
            if (num_mode && !alpha_next) {
                t = number_text(k);
            } else {
                one[0] = letter_char(k);
                one[1] = 0;
                t = one;
            }
            if (*t && ilen + (int)strlen(t) <= INPUT_MAX) {
                strcpy(input + ilen, t);
                ilen += (int)strlen(t);
            }
            alpha_next = false;
        }
    }

    gfx_End();
    return 0;
}
