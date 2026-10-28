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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "edit.h"
#include "exact.h"
#include "expr.h"
#include "mathx.h"
#include "notebook.h"
#include "proofs.h"
#include "solve.h"
#include "triangle.h"

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

static void draw_header(void)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    gfx_SetTextFGColor(C_BLACK);
    gfx_PrintStringXY("SOLVER", 6, 3);
    gfx_SetTextFGColor(C_BLUE);
    gfx_PrintStringXY(mx_degrees ? "DEG" : "RAD", SCREEN_W - 32, 3);
    if (alt) {
        gfx_SetTextFGColor(C_RED);
        gfx_PrintStringXY("2nd", SCREEN_W - 64, 3);
    }
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
    gfx_PrintStringXY(ed.s[0] ? "Enter: = then solve   Graph: triangle   Y=: help"
                              : "Type a problem.  Y= for help, Graph for triangles", 4, 231);
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

/* ---------- triangle form ---------- */

static char tfield[6][10];
static int tcur;

static const char *const tlabel[6] = {"side a", "side b", "side c", "angle A", "angle B", "angle C"};

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

static void triangle_screen(void)
{
    bool running = true;

    wait_for_release();
    while (running) {
        int i;
        uint8_t k;

        gfx_FillScreen(C_WHITE);
        gfx_SetTextFGColor(C_BLACK);
        gfx_PrintStringXY("TRIANGLE", 6, 3);
        gfx_SetColor(C_LINE);
        gfx_HorizLine(0, HEAD_H, SCREEN_W);
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY("Fill any 3 values, 1 or more a side", 6, 18);
        for (i = 0; i < 6; i++) {
            int y = 34 + i * 24 + (i >= 3 ? 8 : 0);
            if (i == tcur) {
                gfx_SetColor(C_SEL);
                gfx_FillRectangle(0, y - 3, SCREEN_W, 22);
            }
            gfx_SetTextFGColor(C_BLACK);
            gfx_PrintStringXY(tlabel[i], 14, y + 3);
            gfx_SetColor(i == tcur ? C_BLUE : C_LINE);
            gfx_Rectangle(110, y - 1, 120, 18);
            gfx_SetTextFGColor(C_BLACK);
            gfx_PrintStringXY(tfield[i], 116, y + 3);
            if (i == tcur) {
                gfx_SetColor(C_BLACK);
                gfx_VertLine(116 + (int)strlen(tfield[i]) * 8, y + 1, 12);
            }
            if (i < 3) {
                gfx_SetTextFGColor(C_GRAY);
                gfx_PrintStringXY("units", 238, y + 3);
            } else {
                gfx_SetTextFGColor(C_GRAY);
                gfx_PrintStringXY("degrees", 238, y + 3);
            }
        }
        gfx_SetTextFGColor(C_GRAY);
        gfx_PrintStringXY("Enter: solve   Clear: erase / back", 4, 231);
        gfx_SwapDraw();
        do {
            k = os_GetCSC();
        } while (!k);

        if (k == sk_Up) tcur = (tcur + 5) % 6;
        else if (k == sk_Down) tcur = (tcur + 1) % 6;
        else if (k == sk_Graph) running = false;
        else if (k == sk_Clear) {
            if (tfield[tcur][0]) tfield[tcur][0] = 0;
            else running = false;
        } else if (k == sk_Del) {
            int n = (int)strlen(tfield[tcur]);
            if (n) tfield[tcur][n - 1] = 0;
        } else if (k == sk_Enter) {
            struct tri_in in;
            static char out[1800];
            memset(&in, 0, sizeof(in));
            for (i = 0; i < 6; i++) {
                real v = 0, sc = 0.1L;
                const char *p = tfield[i];
                bool any = *p != 0;
                while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
                if (*p == '.') {
                    p++;
                    while (*p >= '0' && *p <= '9') { v += (*p - '0') * sc; sc /= 10; p++; }
                }
                if (any) {
                    if (i < 3) { in.has_s[i] = true; in.s[i] = v; }
                    else { in.has_a[i - 3] = true; in.a[i - 3] = v; }
                }
            }
            tri_work(&in, out, sizeof(out));
            show_text("TRIANGLE", out);
        } else {
            const char *t = key_text(k);
            if ((*t >= '0' && *t <= '9' && t[1] == 0) || (*t == '.' && t[1] == 0)) {
                int n = (int)strlen(tfield[tcur]);
                if (n < 8) {
                    tfield[tcur][n] = *t;
                    tfield[tcur][n + 1] = 0;
                }
            }
        }
    }
}

static void help_screen(void)
{
    show_text("HELP",
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
        "Graph = triangle form (Law of Sines and Cosines)\n"
        "Mode = degrees / radians\n"
        "Trace = show or hide steps\n"
        "Clear = erase the line\n"
        "2nd + Mode = quit");
    wait_for_release();
}

int main(void)
{
    bool quit = false;

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
            triangle_screen();
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
