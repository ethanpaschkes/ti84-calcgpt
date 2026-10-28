/* SOLVER: a Desmos-style math solver for the TI-84 Plus CE.
 *
 *   Type like Desmos: "/" stacks a fraction, brackets close themselves.
 *   Enter puts an "=" on the line; Enter again solves it (or works it out).
 *   Without "=" you see the answer live as you type.
 *   Up/Down   pick an earlier line (Enter copies it down to edit)
 *   Graph     triangle solver form        Y=  help
 *   Mode      degrees / radians           2nd + Mode  quit
 *   Trace     show or hide the short steps
 */
#include <graphx.h>
#include "mreal.h"
#include <keypadc.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#include <ti/vars.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "edit.h"
#include "exact.h"
#include "expr.h"
#include "mathx.h"
#include "notebook.h"
#include "preset.h"
#include "proofs.h"
#include "solve.h"

#define BIG_STACK 32000
uint8_t big_stack[BIG_STACK];
unsigned saved_sp;

/* How much of the big stack has ever been used: it is filled with a marker byte at the start,
 * and the deepest byte that is no longer the marker shows how far down the stack reached. */
static size_t ram_free_at_start;

static int stack_deepest(void)
{
    int i = 0;
    while (i < BIG_STACK && big_stack[i] == 0xA5) i++;
    return BIG_STACK - i;
}


/* palette (set in main) */
#define C_LINE  5
#define C_SEL   6
#define C_WHITE 255

#define SCREEN_W 320
#define HEAD_H   14
#define VIEW_TOP 16
#define VIEW_BOT 184
#define ED_TOP   188
#define HIST_MAX 10

static struct hentry hist[HIST_MAX];
static int nhist;
static int sel = -1;       /* selected history entry, or -1 for the editor */
static struct editor ed;
static bool alt;
static bool show_steps = true;
static char preview[100];
static uint8_t preview_color = C_GRAY;
static bool preview_dirty = true;

static void set_palette(void)
{
    gfx_palette[C_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[C_BLUE]  = gfx_RGBTo1555(25, 85, 200);
    gfx_palette[C_GRAY]  = gfx_RGBTo1555(125, 125, 125);
    gfx_palette[C_RED]   = gfx_RGBTo1555(200, 40, 40);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(20, 135, 70);
    gfx_palette[C_LINE]  = gfx_RGBTo1555(205, 205, 205);
    gfx_palette[C_SEL]   = gfx_RGBTo1555(226, 236, 255);
}

/* ---------- drawing a line with stacked fractions ---------- */

static bool has_frac(const char *s)
{
    return nb_has_frac(s);
}

static int row_h(const char *s)
{
    return nb_row_h(s);
}

static void put_char_at(char c, int x, int y)
{
    char b[2];
    b[0] = c;
    b[1] = 0;
    gfx_PrintStringXY(b, x, y);
}

static void put_range(const char *s, int from, int to, int x, int y)
{
    int k;
    for (k = from; k < to; k++) put_char_at(s[k], x + (k - from) * 8, y);
}

/* Draw (or just measure) s with its top at `top`. Returns the width in pixels. */
static int layout_row(const char *s, int x0, int top, int cursor, bool draw, uint8_t color)
{
    int x = x0, i = 0, n = (int)strlen(s);
    int ty = top + (has_frac(s) ? 9 : 3);

    if (draw) gfx_SetTextFGColor(color);
    for (;;) {
        if (draw && cursor == i) {
            gfx_SetColor(C_BLACK);
            gfx_VertLine(x - 1, ty - 1, 10);
        }
        if (i >= n) break;
        if (s[i] == '[') {
            int bar = -1, end = -1, k;
            for (k = i + 1; k < n; k++) {
                if (s[k] == '|' && bar < 0) bar = k;
                else if (s[k] == ']') { end = k; break; }
            }
            if (bar > 0 && end > bar) {
                int nl = bar - i - 1, dl = end - bar - 1;
                int nw = nl ? nl * 8 : 8, dw = dl ? dl * 8 : 8;
                int fw = (nw > dw ? nw : dw) + 6;
                int nx = x + (fw - nw) / 2, dx = x + (fw - dw) / 2;
                if (draw) {
                    gfx_SetColor(color);
                    gfx_HorizLine(x + 1, top + 13, fw - 2);
                    put_range(s, i + 1, bar, nx, top + 3);
                    put_range(s, bar + 1, end, dx, top + 15);
                    gfx_SetColor(C_GRAY);
                    if (!nl) gfx_Rectangle(nx, top + 3, 8, 9);
                    if (!dl) gfx_Rectangle(dx, top + 15, 8, 9);
                    gfx_SetColor(C_BLACK);
                    if (cursor > i && cursor <= bar) gfx_VertLine(nx + 8 * (cursor - (i + 1)) - 1, top + 2, 11);
                    else if (cursor > bar && cursor <= end) gfx_VertLine(dx + 8 * (cursor - (bar + 1)) - 1, top + 14, 11);
                }
                x += fw;
                i = end + 1;
                continue;
            }
        }
        if (draw) put_char_at(s[i], x, ty);
        x += 8;
        i++;
    }
    return x - x0;
}


static struct hentry *hist_new(const char *input)
{
    struct hentry *h;
    if (nhist == HIST_MAX) {
        memmove(&hist[0], &hist[1], (HIST_MAX - 1) * sizeof(hist[0]));
        nhist--;
    }
    h = &hist[nhist++];
    memset(h, 0, sizeof(*h));
    strncpy(h->input, input, ED_MAX);
    return h;
}

/* ---------- screen ---------- */

static int entry_height(const struct hentry *h)
{
    int i, t = row_h(h->input) + 2;
    for (i = 0; i < h->nlines; i++) {
        if (h->is_step[i] && !show_steps) continue;
        t += row_h(h->text[i]);
    }
    return t + 6;
}

/* the credit sits at the right of the top bar on every screen; titles must stay under 16 characters */
static void draw_credit(void)
{
    gfx_SetTextFGColor(C_GRAY);
    gfx_PrintStringXY("made by Ethan Paschkes", SCREEN_W - 6 - 22 * 8, 3);
}

static void draw_header(void)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    gfx_SetTextFGColor(C_BLACK);
    gfx_PrintStringXY("SOLVER", 6, 3);
    gfx_SetTextFGColor(C_BLUE);
    gfx_PrintStringXY(mx_degrees ? "DEG" : "RAD", 62, 3);
    if (alt) {
        gfx_SetTextFGColor(C_RED);
        gfx_PrintStringXY("2nd", 94, 3);
    }
    draw_credit();
}

static void draw_history(void)
{
    int i, total = 0, start = nhist, y;

    while (start > 0 && total + entry_height(&hist[start - 1]) <= (VIEW_BOT - VIEW_TOP) + 120) {
        start--;
        total += entry_height(&hist[start]);
    }
    /* keep the selected entry on screen */
    if (sel >= 0 && sel < start) start = sel;
    {
        int sum = 0;
        for (i = start; i < nhist; i++) sum += entry_height(&hist[i]);
        while (sum > VIEW_BOT - VIEW_TOP && start < nhist - 1 && (sel < 0 || start < sel)) {
            sum -= entry_height(&hist[start]);
            start++;
        }
        y = VIEW_BOT - sum;
        if (y < VIEW_TOP) y = VIEW_TOP;
    }
    for (i = start; i < nhist; i++) {
        const struct hentry *h = &hist[i];
        int k, hh = entry_height(h);
        if (y + hh < VIEW_TOP) { y += hh; continue; }
        if (i == sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(0, y - 1, SCREEN_W, hh);
        }
        layout_row(h->input, 8, y, -1, true, C_BLACK);
        y += row_h(h->input) + 2;
        for (k = 0; k < h->nlines; k++) {
            if (h->is_step[k] && !show_steps) continue;
            layout_row(h->text[k], 18, y, -1, true, h->color[k]);
            y += row_h(h->text[k]);
        }
        gfx_SetColor(C_LINE);
        gfx_HorizLine(8, y + 2, SCREEN_W - 16);
        y += 6;
    }
}

static void draw_editor(void)
{
    gfx_SetColor(C_BLUE);
    gfx_HorizLine(0, ED_TOP - 3, SCREEN_W);
    layout_row(ed.s, 8, ED_TOP, sel < 0 ? ed.cur : -1, true, C_BLACK);
    gfx_SetTextFGColor(preview_color);
    gfx_PrintStringXY(preview, 8, ED_TOP + 28);
    gfx_SetTextFGColor(C_GRAY);
    gfx_PrintStringXY(ed.s[0] ? "Enter: = then solve   Window: presets   Y=: help"
                              : "Type a problem.  Window: presets   Y=: help", 4, 231);
}

static void redraw(void)
{
    gfx_FillScreen(C_WHITE);
    draw_history();
    gfx_SetColor(C_WHITE);
    gfx_FillRectangle(0, ED_TOP - 3, SCREEN_W, 240 - (ED_TOP - 3));
    gfx_FillRectangle(0, 0, SCREEN_W, HEAD_H);
    draw_header();
    draw_editor();
    gfx_SwapDraw();
}

static void update_preview(void)
{
    char lhs[ED_MAX + 1], rhs[ED_MAX + 1];
    struct mx_eval ev;

    preview[0] = 0;
    preview_color = C_GRAY;
    if (!ed.s[0]) return;
#ifdef NO_MATH
    return;
#endif
    ed_split(ed.s, lhs, rhs, ED_MAX + 1);
    if (ed_has_equals(&ed) && rhs[0]) {
        strcpy(preview, "Enter: find the x values that make it true");
        return;
    }
    mx_evaluate(lhs, &ev);
    if (ev.kind == EK_NUMBER || ev.kind == EK_POLY || ev.kind == EK_FRACTION) {
        char plain[100];
        nb_plain_text(ev.exact, plain, sizeof(plain));
        strcpy(preview, "= ");
        strncat(preview, plain, sizeof(preview) - 4);
        if (ev.kind == EK_NUMBER && ev.is_exact && !nb_is_whole(ev.exact) && strlen(preview) + strlen(ev.dec) < 90) {
            strcat(preview, "  ~ ");
            strcat(preview, ev.dec);
        }
        preview_color = C_BLUE;
    } else if (ev.kind == EK_OTHER) {
        strcpy(preview, "Press Enter to add =, then type the other side");
    }
}

/* ---------- keys ---------- */

static const char *key_text(uint8_t k)
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
    case sk_LParen: return "(";
    case sk_Power: return "^(";
    case sk_Square: return alt ? "sqrt(" : "^2";
    case sk_Recip: return "^(-1)";
    case sk_Sin: return alt ? "asin(" : "sin(";
    case sk_Cos: return alt ? "acos(" : "cos(";
    case sk_Tan: return alt ? "atan(" : "tan(";
    case sk_Log: return "log(";
    case sk_Ln: return alt ? "e^(" : "ln(";
    case sk_Math: return "abs(";
    case sk_Apps: return "csc(";
    case sk_Prgm: return "sec(";
    case sk_Vars: return "cot(";
    case sk_Stat: return "pi";
    case sk_GraphVar: return "x";
    default: return "";
    }
}

static bool stop_pressed(void)
{
    kb_Scan();
    return (kb_Data[6] & kb_Clear) != 0;
}

static void wait_for_release(void)
{
    while (kb_AnyKey()) {
    }
    while (os_GetCSC()) {
    }
}

static void commit(void)
{
    char lhs[ED_MAX + 1], rhs[ED_MAX + 1];
    struct hentry *h;

    ed_split(ed.s, lhs, rhs, ED_MAX + 1);
    h = hist_new(ed.s);
    strcpy(preview, "Working...");
    preview_color = C_GRAY;
    redraw();
    mx_stop_check = stop_pressed;
#ifdef NO_MATH
    nb_add_line(h, "(maths turned off for testing)", C_GRAY, false);
    (void)lhs;
#else
    if (rhs[0]) {
        static struct mx_solution sol;
        mx_solve(lhs, rhs, &sol);
        nb_build_solution(h, lhs, rhs, &sol);
    } else {
        struct mx_eval ev;
        mx_evaluate(lhs, &ev);
        nb_build_eval(h, &ev);
    }
#endif
    mx_stop_check = NULL;
    wait_for_release();
    ed_clear(&ed);
    sel = -1;
    preview_dirty = true;
}

/* ---------- scrolling text page ---------- */

static void show_text(const char *title, const char *text)
{
    /* a scrollable page of plain text */
    char lines[60][40];
    int n = 0, top = 0, k;
    const char *p = text;
    uint8_t key;

    while (*p && n < 60) {
        int len = 0, b;
        while (p[len] && p[len] != '\n' && len < 38) len++;
        if (p[len] && p[len] != '\n') {
            for (b = len; b > 0 && p[b] != ' '; b--) {
            }
            if (b > 0) len = b;
        }
        memcpy(lines[n], p, len);
        lines[n][len] = 0;
        n++;
        p += len;
        if (*p == '\n') p++;
        while (*p == ' ') p++;
    }
    for (;;) {
        gfx_FillScreen(C_WHITE);
        gfx_SetTextFGColor(C_BLACK);
        gfx_PrintStringXY(title, 6, 3);
        draw_credit();
        gfx_SetColor(C_LINE);
        gfx_HorizLine(0, HEAD_H, SCREEN_W);
        for (k = 0; k < 20 && top + k < n; k++) {
            gfx_SetTextFGColor(strstr(lines[top + k], " = ") || lines[top + k][0] == 'A' ? C_BLACK : C_GRAY);
            gfx_PrintStringXY(lines[top + k], 6, 20 + k * 10);
        }
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY(n > 20 ? "Up/Down scroll   any other key: back" : "Any key: back", 4, 231);
        gfx_SwapDraw();
        do {
            key = os_GetCSC();
        } while (!key);
        if (key == sk_Down && top + 20 < n) top++;
        else if (key == sk_Up && top > 0) top--;
        else if (key != sk_Down && key != sk_Up) return;
    }
}

/* ---------- preset forms ---------- */

static char fvalue[PRESET_COUNT][PRESET_FIELDS][FIELD_LEN];
static int fchoice[PRESET_COUNT][PRESET_FIELDS];
static int fcur[PRESET_COUNT];

#define BOX_X 104
#define BOX_W 132

static void draw_form(int pi)
{
    const struct preset *p = &presets[pi];
    int i;

    gfx_FillScreen(C_WHITE);
    gfx_SetTextFGColor(C_BLACK);
    gfx_PrintStringXY(p->name, 6, 3);
    draw_credit();
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    gfx_SetTextFGColor(C_GRAY);
    gfx_PrintStringXY(p->hint, 6, 20);
    for (i = 0; i < p->nfields; i++) {
        const struct preset_field *fl = &p->field[i];
        int y = 36 + i * 24;
        if (i == fcur[pi]) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(0, y - 3, SCREEN_W, 23);
        }
        gfx_SetTextFGColor(C_BLACK);
        gfx_PrintStringXY(fl->label, 8, y + 3);
        gfx_SetColor(i == fcur[pi] ? C_BLUE : C_LINE);
        gfx_Rectangle(BOX_X, y - 1, BOX_W, 18);
        gfx_SetTextFGColor(C_BLACK);
        if (fl->choices) {
            gfx_PrintStringXY("<", BOX_X + 4, y + 3);
            gfx_PrintStringXY(fl->choices[fchoice[pi][i]], BOX_X + 18, y + 3);
            gfx_PrintStringXY(">", BOX_X + BOX_W - 12, y + 3);
        } else {
            gfx_PrintStringXY(fvalue[pi][i], BOX_X + 6, y + 3);
            if (i == fcur[pi]) {
                gfx_SetColor(C_BLACK);
                gfx_VertLine(BOX_X + 6 + (int)strlen(fvalue[pi][i]) * 8, y + 1, 12);
            }
        }
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY(fl->unit, BOX_X + BOX_W + 6, y + 3);
    }
    gfx_SetTextFGColor(C_GRAY);
    gfx_PrintStringXY("Enter: solve    Clear: erase / back", 4, 231);
    gfx_SwapDraw();
}

static void form_screen(int pi)
{
    const struct preset *p = &presets[pi];
    bool running = true;

    wait_for_release();
    while (running) {
        uint8_t k;

        draw_form(pi);
        do {
            k = os_GetCSC();
        } while (!k);
        if (k == sk_Up) fcur[pi] = (fcur[pi] + p->nfields - 1) % p->nfields;
        else if (k == sk_Down) fcur[pi] = (fcur[pi] + 1) % p->nfields;
        else if (k == sk_Window || k == sk_Graph || k == sk_Mode) running = false;
        else if (k == sk_Left || k == sk_Right) {
            const struct preset_field *fl = &p->field[fcur[pi]];
            if (fl->choices) {
                int d = k == sk_Right ? 1 : fl->nchoices - 1;
                fchoice[pi][fcur[pi]] = (fchoice[pi][fcur[pi]] + d) % fl->nchoices;
            }
        } else if (k == sk_Clear) {
            char *v = fvalue[pi][fcur[pi]];
            if (v[0]) v[0] = 0;
            else running = false;
        } else if (k == sk_Del) {
            char *v = fvalue[pi][fcur[pi]];
            int n = (int)strlen(v);
            if (n) v[n - 1] = 0;
        } else if (k == sk_Enter) {
            const char *f[PRESET_FIELDS];
            static char out[1800];
            int i;
            for (i = 0; i < p->nfields; i++) {
                f[i] = p->field[i].choices ? p->field[i].choices[fchoice[pi][i]] : fvalue[pi][i];
            }
            strcpy(out, "");
            mx_stop_check = stop_pressed;
            p->run(f, out, sizeof(out));
            mx_stop_check = NULL;
            show_text(p->name, out);
        } else if (!p->field[fcur[pi]].choices) {
            const char *t = k == sk_Div ? "/" : k == sk_RParen ? ")" : key_text(k);
            char *v = fvalue[pi][fcur[pi]];
            if (*t && (int)(strlen(v) + strlen(t)) < FIELD_LEN) strcat(v, t);
            alt = false;
        }
    }
}

static void preset_menu(void)
{
    int cur = 0;
    bool running = true;

    wait_for_release();
    while (running) {
        int i;
        uint8_t k;

        gfx_FillScreen(C_WHITE);
        gfx_SetTextFGColor(C_BLACK);
        gfx_PrintStringXY("PRESETS", 6, 3);
        draw_credit();
        gfx_SetColor(C_LINE);
        gfx_HorizLine(0, HEAD_H, SCREEN_W);
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY("Pick a problem type. It asks for the values.", 6, 20);
        for (i = 0; i < PRESET_COUNT; i++) {
            int y = 34 + i * 19;
            if (i == cur) {
                gfx_SetColor(C_SEL);
                gfx_FillRectangle(0, y - 3, SCREEN_W, 18);
            }
            gfx_SetTextFGColor(C_BLUE);
            gfx_SetTextXY(10, y + 2);
            gfx_PrintInt((i + 1) % 10, 1);
            gfx_SetTextFGColor(C_BLACK);
            gfx_PrintStringXY(presets[i].name, 28, y + 2);
        }
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY("Up/Down + Enter, or press 1-9, 0    Clear: back", 4, 231);
        gfx_SwapDraw();
        do {
            k = os_GetCSC();
        } while (!k);
        if (k == sk_Up) cur = (cur + PRESET_COUNT - 1) % PRESET_COUNT;
        else if (k == sk_Down) cur = (cur + 1) % PRESET_COUNT;
        else if (k == sk_Clear || k == sk_Window || k == sk_Mode) running = false;
        else if (k == sk_Enter) { form_screen(cur); }
        else {
            const char *t = key_text(k);
            if (t[0] >= '0' && t[0] <= '9' && t[1] == 0) {
                cur = t[0] == '0' ? 9 : t[0] - '1';
                form_screen(cur);
            }
            alt = false;
        }
    }
}

static void help_screen(void)
{
    static char text[1500];
    char num[16];

    strcpy(text,
        "TYPING\n"
        "/ stacks a fraction. Brackets close themselves; the right arrow steps out.\n"
        "X key = x   STAT = pi   ^ and x^2 for powers\n"
        "sin cos tan keys (2nd = inverse); csc sec cot on APPS PRGM VARS; MATH = abs; LOG LN\n"
        "\nSOLVING\n"
        "Enter puts = on the line, Enter again solves it. With no = you see the answer as you type.\n"
        "x^2 = 4x - 3  lists every x that works.\n"
        "sin x = 1/2   gives the angles and the general form.\n"
        "Type an identity, like sin^2 x + cos^2 x = 1, and it says if it is true for every x.\n"
        "\nOTHER KEYS\n"
        "Up/Down pick an old line, Enter copies it down\n"
        "Window = presets: triangle, right triangle, trig equation, angle values, other trig values, convert, arc length, polar, amplitude and period, quadratic\nGraph = the triangle form straight away\n"
        "Mode = degrees / radians\n"
        "Trace = show or hide steps\n"
        "Clear = erase the line\n"
        "2nd + Mode = quit");
    strcat(text, "\n\nMEMORY (for checking the program)\nDeepest stack use: ");
    mx_decimal((real)stack_deepest(), 7, num, sizeof(num));
    strcat(text, num);
    strcat(text, " of 32000 bytes\nFree RAM when this run started: ");
    mx_decimal((real)(ram_free_at_start / 1024), 7, num, sizeof(num));
    strcat(text, num);
    strcat(text, "k");
    show_text("HELP", text);
    wait_for_release();
}

static int run(void)
{
    bool quit = false;
    void *free_start;

    ram_free_at_start = os_MemChk(&free_start);
    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    mx_degrees = true;
    ed_clear(&ed);

    while (!quit) {
        uint8_t k;

        if (preview_dirty) {
            update_preview();
            preview_dirty = false;
        }
        redraw();
        do {
            k = os_GetCSC();
        } while (!k);

        if (k == sk_2nd) {
            alt = !alt;
        } else if (k == sk_Mode) {
            if (alt) quit = true;
            else mx_degrees = !mx_degrees;
            alt = false;
            preview_dirty = true;
        } else if (k == sk_Graph) {
            alt = false;
            form_screen(0);
        } else if (k == sk_Window) {
            alt = false;
            preset_menu();
        } else if (k == sk_Yequ) {
            alt = false;
            help_screen();
        } else if (k == sk_Trace) {
            show_steps = !show_steps;
        } else if (k == sk_Up) {
            if (nhist) {
                if (sel < 0) sel = nhist - 1;
                else if (sel > 0) sel--;
            }
        } else if (k == sk_Down) {
            if (sel >= 0) sel = sel + 1 >= nhist ? -1 : sel + 1;
        } else if (k == sk_Clear) {
            if (sel >= 0) sel = -1;
            else ed_clear(&ed);
            preview_dirty = true;
        } else if (k == sk_Enter) {
            if (sel >= 0) {
                ed_set(&ed, hist[sel].input);
                sel = -1;
            } else if (ed.s[0]) {
                if (!ed_has_equals(&ed)) ed_append_equals(&ed);
                else commit();
            }
            preview_dirty = true;
        } else if (sel >= 0) {
            sel = -1;
        } else if (k == sk_Left) {
            ed_left(&ed);
        } else if (k == sk_Right) {
            ed_right(&ed);
        } else if (k == sk_Del) {
            ed_backspace(&ed);
            preview_dirty = true;
        } else if (k == sk_Div) {
            ed_fraction(&ed);
            preview_dirty = true;
        } else if (k == sk_RParen) {
            ed_close_paren(&ed);
            preview_dirty = true;
        } else {
            const char *t = key_text(k);
            if (*t) {
                ed_insert(&ed, t);
                preview_dirty = true;
            }
            alt = false;
        }
        if (k != sk_2nd && k != sk_Mode) alt = false;
    }

    gfx_End();
    return 0;
}

/* The calculator's own stack is only about 4 KB, far too small for the solver, so the whole
 * program runs on a bigger stack kept in ordinary memory and puts the old one back on exit. */
int main(void)
{
    int r;
    __asm__ volatile("ld (_saved_sp), sp");
    memset(big_stack, 0xA5, BIG_STACK);
    __asm__ volatile("ld hl, _big_stack + 32000\n\tld sp, hl" ::: "hl");
    r = run();
    __asm__ volatile("ld sp, (_saved_sp)");
    return r;
}
