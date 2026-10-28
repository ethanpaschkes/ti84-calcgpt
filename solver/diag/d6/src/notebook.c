/* Turns solver results into the lines the notebook shows. No drawing in here,
 * so it can be tested on a computer. */
#include "notebook.h"
#include "mathx.h"
#include "proofs.h"
#include <string.h>

bool nb_has_frac(const char *s)
{
    return strchr(s, '[') != NULL;
}

int nb_row_h(const char *s)
{
    return nb_has_frac(s) ? 26 : 14;
}

/* Width in pixels of a line drawn with 8-pixel characters and stacked fractions. */
int nb_row_width(const char *s)
{
    int x = 0, i = 0, n = (int)strlen(s);
    while (i < n) {
        if (s[i] == '[') {
            int bar = -1, end = -1, k;
            for (k = i + 1; k < n; k++) {
                if (s[k] == '|' && bar < 0) bar = k;
                else if (s[k] == ']') { end = k; break; }
            }
            if (bar > 0 && end > bar) {
                int nl = bar - i - 1, dl = end - bar - 1;
                int nw = nl ? nl * 8 : 8, dw = dl ? dl * 8 : 8;
                x += (nw > dw ? nw : dw) + 6;
                i = end + 1;
                continue;
            }
        }
        x += 8;
        i++;
    }
    return x;
}

/* "[a|b]" -> "(a)/(b)" for one-line previews */
void nb_plain_text(const char *src, char *dst, int cap)
{
    int n = 0;
    for (; *src && n < cap - 5; src++) {
        if (*src == '[') dst[n++] = '(';
        else if (*src == '|') { dst[n++] = ')'; dst[n++] = '/'; dst[n++] = '('; }
        else if (*src == ']') dst[n++] = ')';
        else dst[n++] = *src;
    }
    dst[n] = 0;
}

/* ---------- building results ---------- */

void nb_add_line(struct hentry *h, const char *text, uint8_t color, bool step)
{
    if (h->nlines >= LINES_MAX) return;
    strncpy(h->text[h->nlines], text, LINE_LEN - 1);
    h->text[h->nlines][LINE_LEN - 1] = 0;
    h->color[h->nlines] = color;
    h->is_step[h->nlines] = step;
    h->nlines++;
}

/* plain text split on newlines and wrapped to the screen width */
void nb_add_wrapped(struct hentry *h, const char *text, uint8_t color, bool step)
{
    const int wrap = 36;
    while (*text) {
        int n = 0, b;
        char buf[LINE_LEN];
        while (text[n] && text[n] != '\n' && n < wrap) n++;
        if (text[n] && text[n] != '\n') {
            for (b = n; b > 0 && text[b] != ' '; b--) {
            }
            if (b > 0) n = b;
        }
        memcpy(buf, text, n);
        buf[n] = 0;
        nb_add_line(h, buf, color, step);
        text += n;
        if (*text == '\n') text++;
        while (*text == ' ') text++;
    }
}

bool nb_is_whole(const char *exact)
{
    return !strchr(exact, '[') && !strstr(exact, "sqrt") && !strstr(exact, "pi") && !strchr(exact, '.');
}

void nb_build_solution(struct hentry *h, const char *lhs, const char *rhs, const struct mx_solution *s)
{
    int i;
    char line[LINE_LEN];

    if (s->kind == SK_ERROR) {
        nb_add_wrapped(h, s->note, C_RED, false);
        return;
    }
    if (s->kind == SK_ALL) {
        const char *steps = NULL;
        int k = mx_find_proof(lhs, rhs, &steps);
        nb_add_line(h, "True for every x (an identity)", C_GREEN, false);
        if (k) nb_add_wrapped(h, steps, C_GRAY, true);
        else nb_add_wrapped(h, "Checked at many values. To prove it, work on one side until it matches the other.", C_GRAY, true);
        return;
    }
    if (s->kind == SK_NONE) {
        nb_add_line(h, "No solution", C_RED, false);
        if (s->steps[0]) nb_add_wrapped(h, s->steps, C_GRAY, true);
        else if (s->note[0]) nb_add_wrapped(h, s->note, C_GRAY, true);
        return;
    }
    if (s->trig) nb_add_line(h, mx_degrees ? "Solutions from 0 to 360 degrees:" : "Solutions from 0 to 2pi:", C_GRAY, false);
    /* the answers: x = a   or   x = b, wrapped by width */
    line[0] = 0;
    for (i = 0; i < s->n; i++) {
        char piece[80], trial[LINE_LEN];
        strcpy(piece, "x = ");
        strcat(piece, s->exact[i]);
        if (line[0]) { strcpy(trial, line); strcat(trial, "  or  "); strcat(trial, piece); }
        else strcpy(trial, piece);
        if (line[0] && nb_row_width(trial) > 280) {
            nb_add_line(h, line, C_BLUE, false);
            strcpy(line, piece);
        } else {
            strcpy(line, trial);
        }
    }
    if (line[0]) nb_add_line(h, line, C_BLUE, false);
    if (s->more) nb_add_line(h, "(more solutions not shown)", C_GRAY, false);
    if (s->trig && s->general[0]) {
        nb_add_line(h, "General solution:", C_GRAY, false);
        /* one root per line so each fits */
        {
            const char *p = s->general;
            while (*p) {
                char one[LINE_LEN];
                const char *sep = strstr(p, "   or   ");
                int n = sep ? (int)(sep - p) : (int)strlen(p);
                if (n > LINE_LEN - 1) n = LINE_LEN - 1;
                memcpy(one, p, n);
                one[n] = 0;
                nb_add_line(h, one, C_BLUE, false);
                p += n;
                if (sep) p += 8;
            }
        }
    }
    {
        /* decimals, when an exact form hides them */
        bool show = false;
        char dl[LINE_LEN] = "x = ";
        for (i = 0; i < s->n; i++) if (s->is_exact[i] && !nb_is_whole(s->exact[i])) show = true;
        if (show && !(s->trig && mx_degrees)) {
            for (i = 0; i < s->n && strlen(dl) < 50; i++) {
                if (i) strcat(dl, ", ");
                strcat(dl, s->dec[i]);
            }
            nb_add_line(h, dl, C_GRAY, false);
        }
    }
    if (s->steps[0]) nb_add_wrapped(h, s->steps, C_GRAY, true);
}

void nb_build_eval(struct hentry *h, const struct mx_eval *e)
{
    char line[LINE_LEN];
    if (e->kind == EK_ERROR || e->kind == EK_OTHER) {
        nb_add_wrapped(h, e->note, e->kind == EK_ERROR ? C_RED : C_GRAY, false);
        return;
    }
    strcpy(line, "= ");
    strcat(line, e->exact);
    nb_add_line(h, line, C_BLUE, false);
    if (e->kind == EK_NUMBER && e->is_exact && !nb_is_whole(e->exact)) {
        strcpy(line, "~ ");
        strcat(line, e->dec);
        nb_add_line(h, line, C_GRAY, false);
    }
    if (e->steps[0]) nb_add_wrapped(h, e->steps, C_GRAY, true);
    if (e->note[0]) nb_add_wrapped(h, e->note, C_GRAY, true);
}

