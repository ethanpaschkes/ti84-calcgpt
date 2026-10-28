#include "expr.h"
#include <stdint.h>
#include "exact.h"
#include "mathx.h"
#include "poly.h"
#include <math.h>
#include "mreal.h"
#include <string.h>

static real pfn(real x, void *c)
{
    return mx_run((const struct mx_prog *)c, x);
}

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) *(*o)++ = *s++;
}

static bool has_x(const char *s)
{
    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] == 'x' && !(i > 0 && s[i - 1] >= 'a' && s[i - 1] <= 'z')) return true;
    }
    return false;
}

/* If the whole text is one stacked fraction [N|D], split it. */
static bool split_fraction(const char *s, char *num, char *den, size_t cap)
{
    int depth = 0, bar = -1, i, n = (int)strlen(s);
    if (n < 5 || s[0] != '[' || s[n - 1] != ']') return false;
    for (i = 0; i < n; i++) {
        if (s[i] == '[') depth++;
        else if (s[i] == ']') {
            depth--;
            if (depth == 0 && i != n - 1) return false;
        } else if (s[i] == '|' && depth == 1) bar = i;
    }
    if (bar < 0 || depth != 0) return false;
    if ((size_t)(bar - 1) >= cap || (size_t)(n - bar - 2) >= cap) return false;
    memcpy(num, s + 1, bar - 1);
    num[bar - 1] = 0;
    memcpy(den, s + bar + 1, n - bar - 2);
    den[n - bar - 2] = 0;
    return true;
}

/* clear denominators and divide out any whole-number factor the top and bottom share */
static void content_reduce(struct poly *a, struct poly *b)
{
    int32_t L = 1, g = 0;
    struct poly *ps[2];
    int k, i;
    ps[0] = a;
    ps[1] = b;
    for (k = 0; k < 2; k++) {
        for (i = 0; i <= ps[k]->deg; i++) {
            int32_t gg = mx_gcd(L, ps[k]->c[i].d);
            L = L / gg * ps[k]->c[i].d;
        }
    }
    if (L > 5000) return;
    for (k = 0; k < 2; k++) {
        for (i = 0; i <= ps[k]->deg; i++) {
            ps[k]->c[i].n = ps[k]->c[i].n * (L / ps[k]->c[i].d);
            ps[k]->c[i].d = 1;
            g = mx_gcd(g, ps[k]->c[i].n);
        }
    }
    if (g > 1) {
        for (k = 0; k < 2; k++) for (i = 0; i <= ps[k]->deg; i++) ps[k]->c[i].n /= g;
    }
    /* a negative bottom leading coefficient moves its sign to the top */
    if (b->c[b->deg].n < 0) {
        for (k = 0; k < 2; k++) for (i = 0; i <= ps[k]->deg; i++) ps[k]->c[i].n = -ps[k]->c[i].n;
    }
}

void mx_evaluate(const char *text, struct mx_eval *out)
{
    struct mx_prog prog;
    char *o, *end;

    memset(out, 0, sizeof(*out));
    if (!mx_compile(text, 0, &prog)) {
        out->kind = EK_ERROR;
        strcpy(out->note, "I can't read that. Check the brackets and symbols.");
        return;
    }
    if (!has_x(text)) {
        real v = mx_run(&prog, 0.0L);
        if (v != v) {
            out->kind = EK_ERROR;
            strcpy(out->note, "That is undefined (dividing by zero, or a root of a negative).");
            return;
        }
        out->kind = EK_NUMBER;
        mx_decimal(v, 8, out->dec, sizeof(out->dec));
        out->is_exact = mx_exact(v, strstr(text, "pi") != NULL, out->exact, sizeof(out->exact));
        return;
    }

    /* has x */
    {
        char num[80], den[80];
        struct mx_prog pn, pd;
        struct poly a, b;
        o = out->steps;
        end = out->steps + sizeof(out->steps) - 1;

        if (split_fraction(text, num, den, sizeof(num)) && mx_compile(num, 0, &pn) && mx_compile(den, 0, &pd)
            && poly_fit(pfn, &pn, &a) && poly_fit(pfn, &pd, &b) && !(b.deg == 0 && b.c[0].n == 0)) {
            struct poly_roots ra, rb;
            char tn[100], td[100], list[100] = "";
            int i, k;
            bool cancelled = false;
            poly_solve(&a, &ra);
            poly_solve(&b, &rb);
            /* cancel shared rational roots */
            for (i = 0; i < ra.nrat; i++) {
                for (k = 0; k < rb.nrat; k++) {
                    if (ra.rat[i].n == rb.rat[k].n && ra.rat[i].d == rb.rat[k].d) {
                        int m = ra.mult[i] < rb.mult[k] ? ra.mult[i] : rb.mult[k], t;
                        char r[40];
                        real rv = (real)ra.rat[i].n / (real)ra.rat[i].d;
                        for (t = 0; t < m; t++) {
                            poly_deflate_root(&a, ra.rat[i]);
                            poly_deflate_root(&b, ra.rat[i]);
                        }
                        cancelled = true;
                        mx_decimal(rv, 6, r, sizeof(r));
                        if (list[0]) strcat(list, ", ");
                        strcat(list, r);
                    }
                }
            }
            if (b.deg > 0) content_reduce(&a, &b);
            poly_text(&a, tn, sizeof(tn));
            poly_text(&b, td, sizeof(td));
            out->kind = EK_FRACTION;
            if (b.deg == 0) {
                /* the denominator became a constant: show as a polynomial with fraction coefficients */
                struct poly q = a;
                int j;
                for (j = 0; j <= q.deg; j++) {
                    int32_t n = q.c[j].n, d = q.c[j].d * b.c[0].n;
                    int32_t g = mx_gcd(n, d);
                    if (d < 0) { n = -n; d = -d; }
                    if (g > 1) { n /= g; d /= g; }
                    q.c[j].n = n;
                    q.c[j].d = d ? d : 1;
                }
                poly_text(&q, out->exact, sizeof(out->exact));
            } else {
                strcpy(out->exact, "[");
                strcat(out->exact, tn);
                strcat(out->exact, "|");
                strcat(out->exact, td);
                strcat(out->exact, "]");
            }
            out->is_exact = true;
            if (cancelled) {
                strcpy(out->note, "Cancelled the common factor. Not defined at x = ");
                strcat(out->note, list);
                put(&o, end, "Factor the top and bottom, cancel what they share.");
            } else {
                strcpy(out->note, "Already in lowest terms.");
            }
            *o = 0;
            return;
        }
        if (poly_fit(pfn, &prog, &a) && a.deg >= 1) {
            struct poly_roots r;
            char fac[120];
            out->kind = EK_POLY;
            poly_text(&a, out->exact, sizeof(out->exact));
            out->is_exact = true;
            poly_solve(&a, &r);
            if (poly_factor_text(&a, &r, fac, sizeof(fac))) {
                put(&o, end, "Factored: ");
                put(&o, end, fac);
            }
            *o = 0;
            return;
        }
    }
    out->kind = EK_OTHER;
    strcpy(out->note, "Add = to solve it, like  x^2 = 4.");
}
