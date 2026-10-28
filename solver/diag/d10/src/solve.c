#include "solve.h"
#include "exact.h"
#include "mathx.h"
#include "poly.h"
#include <math.h>
#include "mreal.h"
#include <string.h>


#define PI_D 3.14159265358979323846f


struct eq {
    struct mx_prog l, r;
};

static real fdiff(real x, void *c)
{
    struct eq *e = (struct eq *)c;
    return mx_run(&e->l, x) - mx_run(&e->r, x);
}

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) *(*o)++ = *s++;
}

static void say(struct mx_solution *s, const char *line)
{
    char *o = s->steps + strlen(s->steps), *end = s->steps + sizeof(s->steps) - 1;
    if (s->steps[0]) put(&o, end, "\n");
    put(&o, end, line);
    *o = 0;
}

static bool finite(real v)
{
    return v == v && fabs(v) < 1e30f;
}

/* ---------- identity test ---------- */

static bool is_identity(struct eq *e, real *cx, real *cl, real *cr, bool *found_counter)
{
    static const real pts[] = {3.7f, 9.3f, 1.3f, 2.9f, 4.1f, 5.7f, 7.9f, 11.3f, 17.9f, 23.7f, 37.1f, 53.3f, 71.9f,
                                 101.3f, 143.7f, 211.1f, -17.3f, -47.9f, -3.1f, 13.1f, 8.6f, 29.5f, 61.7f, -0.9f};
    int i, valid = 0;
    bool ok = true;
    /* float trig of large radian values is only good to about 1e-5 */
    real same_tol = mx_degrees ? 2e-5f : 3e-4f;
    *found_counter = false;
    for (i = 0; i < (int)(sizeof(pts) / sizeof(pts[0])); i++) {
        real a = mx_run(&e->l, pts[i]), b = mx_run(&e->r, pts[i]);
        if (!finite(a) || !finite(b)) continue;
        valid++;
        if (fabs(a - b) > same_tol * (1.0f + fabs(a) + fabs(b))) {
            ok = false;
            if (!*found_counter) {
                *found_counter = true;
                *cx = pts[i];
                *cl = a;
                *cr = b;
            }
        }
    }
    return valid >= 8 && ok;
}

/* ---------- numeric roots ---------- */

static real bisect(struct eq *e, real a, real b)
{
    real fa = fdiff(a, e);
    int i;
    for (i = 0; i < 32; i++) {
        real m = 0.5f * (a + b), fm = fdiff(m, e);
        if (fm == 0.0f) return m;
        if ((fa < 0) == (fm < 0)) { a = m; fa = fm; } else b = m;
    }
    return 0.5f * (a + b);
}

/* is f really ~0 here (not a pole)? */
static bool is_root(struct eq *e, real x)
{
    real a = mx_run(&e->l, x), b = mx_run(&e->r, x);
    if (!finite(a) || !finite(b)) return false;
    return fabs(a - b) <= 2e-5f * (1.0f + fabs(a) + fabs(b));
}

/* roots closer together than float can tell apart are the same root */
static int add_root(real *roots, int n, real x)
{
    real tol = 3e-4f * (1.0f + fabs(x));
    int i;
    if (n >= 40) return n;
    for (i = 0; i < n; i++) if (fabs(roots[i] - x) < tol) return n;
    roots[n++] = x;
    return n;
}

/* all roots of f in [lo, hi) on a grid of N intervals */
static int scan_roots(struct eq *e, real lo, real hi, int N, real *roots, int n)
{
    real step = (hi - lo) / N, px = lo, pf = fdiff(lo, e);
    real ppf = NAN;
    int i;

    for (i = 1; i <= N; i++) {
        real x = lo + step * i, fx;
        if ((i & 15) == 0 && mx_stop_check && mx_stop_check()) break;
        fx = fdiff(x, e);
        if (finite(pf) && finite(fx)) {
            if (pf == 0.0f) {
                if (is_root(e, px)) n = add_root(roots, n, px);
            } else if ((pf < 0) != (fx < 0) && fx != 0.0f) {
                real r = bisect(e, px, x);
                if (is_root(e, r)) n = add_root(roots, n, r);
            }
            /* a touching root: |f| has a local minimum near zero */
            if (finite(ppf) && fabs(pf) < fabs(ppf) && fabs(pf) <= fabs(fx) && fabs(pf) < 0.05f * (1.0f + fabs(pf))) {
                /* golden section search for the minimum of |f| on [px - step, px + step] */
                real a = px - step, b = px + step;
                int k;
                for (k = 0; k < 25; k++) {
                    real m1 = a + (b - a) / 3.0f, m2 = b - (b - a) / 3.0f;
                    real f1 = fabs(fdiff(m1, e)), f2 = fabs(fdiff(m2, e));
                    if (!(f1 == f1) || !(f2 == f2)) break;
                    if (f1 < f2) b = m2; else a = m1;
                }
                if (is_root(e, 0.5f * (a + b))) n = add_root(roots, n, 0.5f * (a + b));
            }
        }
        ppf = pf;
        px = x;
        pf = fx;
    }
    if (finite(pf) && pf == 0.0f && is_root(e, px)) n = add_root(roots, n, px);
    return n;
}

static void sort_roots(real *r, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        real t = r[i];
        for (j = i - 1; j >= 0 && r[j] > t; j--) r[j + 1] = r[j];
        r[j + 1] = t;
    }
}

/* ---------- filling in the answer ---------- */

static void set_value(struct mx_solution *s, int i, real v, bool angle)
{
    bool pi_ok = angle && !mx_degrees;
    if (fabs(v) < 2e-6f) v = 0.0f;
    s->val[i] = v;
    mx_decimal(v, 6, s->dec[i], sizeof(s->dec[i]));
    if (angle && mx_degrees) {
        /* degrees: whole numbers read best */
        real rv = floor(v + 0.5f);
        if (fabs(v - rv) < 1e-3f) {
            mx_decimal(rv, 6, s->exact[i], sizeof(s->exact[i]));
            s->is_exact[i] = true;
            return;
        }
        strcpy(s->exact[i], s->dec[i]);
        s->is_exact[i] = false;
        return;
    }
    s->is_exact[i] = mx_exact(v, pi_ok, s->exact[i], sizeof(s->exact[i]));
}

/* ---------- periodic trig ---------- */

static bool detect_period(struct eq *e, real *period)
{
    static const real cand_deg[] = {10, 12, 15, 20, 24, 30, 36, 45, 60, 72, 90, 120, 144, 180, 240, 360, 540, 720};
    static const real pts[] = {3.3f, 17.9f, 41.7f, 66.1f, 103.7f, 159.3f, 23.9f, 87.4f};
    int c, i;
    for (c = 0; c < (int)(sizeof(cand_deg) / sizeof(cand_deg[0])); c++) {
        real P = mx_degrees ? cand_deg[c] : cand_deg[c] * PI_D / 180.0f;
        bool ok = true;
        int valid = 0;
        for (i = 0; i < 8 && ok; i++) {
            real x = mx_degrees ? pts[i] : pts[i] * PI_D / 180.0f * 1.0f;
            real a = fdiff(x, e), b = fdiff(x + P, e);
            if (!finite(a) || !finite(b)) continue;
            valid++;
            if (fabs(a - b) > 5e-5f * (1.0f + fabs(a) + fabs(b))) ok = false;
        }
        if (ok && valid >= 5) { *period = P; return true; }
    }
    return false;
}

/* build "x = 30 + 360n  or  x = 150 + 360n" */
static void build_general(struct mx_solution *s, const real *base, int nbase)
{
    char *o = s->general, *end = s->general + sizeof(s->general) - 1;
    char per[40];
    int i;

    if (mx_degrees) mx_decimal(s->period, 6, per, sizeof(per));
    else mx_exact(s->period, true, per, sizeof(per));
    s->general[0] = 0;
    for (i = 0; i < nbase; i++) {
        char v[40];
        if (mx_degrees) mx_decimal(base[i], 6, v, sizeof(v));
        else mx_exact(base[i], true, v, sizeof(v));
        if (i) put(&o, end, "   or   ");
        put(&o, end, "x = ");
        if (fabs(base[i]) > 5e-5f * (1.0f + s->period)) {
            put(&o, end, v);
            put(&o, end, " + ");
        }
        put(&o, end, per);
        put(&o, end, mx_degrees ? "n" : " n");
    }
    *o = 0;
}

static void solve_trig(struct eq *e, real period, struct mx_solution *s)
{
    real base[40], all[60];
    int nb = 0, na = 0, i;
    real full = mx_degrees ? 360.0f : 2.0f * PI_D;
    char buf[80], per[40];

    nb = scan_roots(e, 0.0f, period, 360, base, 0);
    /* a root at exactly one period is the same as one at 0 */
    {
        int keep = 0;
        for (i = 0; i < nb; i++) {
            if (fabs(base[i] - period) < 5e-5f * (1.0f + period)) continue;
            base[keep++] = base[i];
        }
        nb = keep;
    }
    sort_roots(base, nb);
    s->trig = true;
    s->period = period;
    if (mx_degrees) mx_decimal(period, 6, per, sizeof(per)); else mx_exact(period, true, per, sizeof(per));
    strcpy(buf, "Period: ");
    strcat(buf, per);
    say(s, buf);
    if (nb == 0) {
        s->kind = SK_NONE;
        say(s, "No solutions: the two sides never meet.");
        return;
    }
    s->nbase = nb;
    /* all solutions in one full turn */
    for (i = 0; i < nb && na < 58; i++) {
        real v;
        for (v = base[i]; v < full - 5e-5f * (1.0f + full) && na < 58; v += period) all[na++] = v;
    }
    sort_roots(all, na);
    s->kind = SK_LIST;
    s->n = na > SOL_MAX ? SOL_MAX : na;
    s->more = na > SOL_MAX;
    for (i = 0; i < s->n; i++) set_value(s, i, all[i], true);
    build_general(s, base, nb);
    (void)full;
}

/* ---------- main entry ---------- */

void mx_solve(const char *lhs, const char *rhs, struct mx_solution *s)
{
    struct eq e;
    real roots[40], cx = 0, cl = 0, cr = 0;
    bool counter = false;
    struct poly po;
    int nroots = 0, i;
    bool trig;

    memset(s, 0, sizeof(*s));
    if (!mx_compile(lhs, 0, &e.l) || !mx_compile(rhs, 0, &e.r)) {
        s->kind = SK_ERROR;
        strcpy(s->note, "I can't read that. Check the brackets and symbols.");
        return;
    }
    if (is_identity(&e, &cx, &cl, &cr, &counter)) {
        s->kind = SK_ALL;
        strcpy(s->note, "True for every x where both sides are defined.");
        return;
    }
    trig = mx_has_trig(lhs) || mx_has_trig(rhs);

    /* polynomial equations: exact */
    if (!trig && poly_from_progs(&e.l, &e.r, &po)) {
        struct poly_roots pr;
        char text[120], line[200];
        real lo, hi;

        if (po.deg == 0) {
            s->kind = SK_NONE;
            strcpy(s->note, "No x makes this true.");
            return;
        }
        poly_text(&po, text, sizeof(text));
        strcpy(line, "Move everything to one side:\n");
        strcat(line, text);
        strcat(line, " = 0");
        say(s, line);
        poly_solve(&po, &pr);
        {
            char fac[120];
            if (poly_factor_text(&po, &pr, fac, sizeof(fac))) {
                strcpy(line, "Factor: ");
                strcat(line, fac);
                strcat(line, " = 0");
                say(s, line);
            }
        }
        for (i = 0; i < pr.nrat && nroots < 40; i++) roots[nroots++] = (real)pr.rat[i].n / (real)pr.rat[i].d;
        if (pr.rest.deg == 2 && pr.nquad >= 0) {
            char qline[100];
            if (pr.nquad == 2) {
                char abc[100] = "Quadratic formula: a = ";
                char tmp[24];
                mx_decimal((real)pr.qa, 6, tmp, sizeof(tmp)); strcat(abc, tmp); strcat(abc, ", b = ");
                mx_decimal((real)pr.qb, 6, tmp, sizeof(tmp)); strcat(abc, tmp); strcat(abc, ", c = ");
                mx_decimal((real)pr.qc, 6, tmp, sizeof(tmp)); strcat(abc, tmp);
                say(s, abc);
                strcpy(qline, "Discriminant b^2 - 4ac = ");
                mx_decimal((real)pr.disc, 6, tmp, sizeof(tmp));
                strcat(qline, tmp);
                say(s, qline);
            }
            for (i = 0; i < pr.nquad; i++) roots[nroots++] = pr.quad_val[i];
        } else if (pr.rest.deg == 2 && pr.nquad < 0) {
            say(s, "The leftover quadratic has no real roots (discriminant < 0).");
        } else if (pr.rest.deg >= 3) {
            /* no more rational roots: find the rest numerically */
            struct eq inner = e;
            real bound = 1.0f;
            for (i = 0; i <= po.deg; i++) {
                real c = fabs((real)po.c[i].n / (real)po.c[i].d / ((real)po.c[po.deg].n / (real)po.c[po.deg].d));
                if (c + 1.0f > bound) bound = c + 1.0f;
            }
            lo = -bound; hi = bound;
            nroots = scan_roots(&inner, lo, hi, 800, roots, nroots);
            say(s, "The rest can't be factored nicely, so I found those roots numerically.");
        }
        sort_roots(roots, nroots);
        if (nroots == 0) {
            s->kind = SK_NONE;
            strcpy(s->note, "No real solutions.");
            return;
        }
        s->kind = SK_LIST;
        s->n = nroots > SOL_MAX ? SOL_MAX : nroots;
        s->more = nroots > SOL_MAX;
        for (i = 0; i < s->n; i++) {
            /* exact text: rational and quadratic roots are recognised from the value */
            bool ex = false;
            real v = roots[i];
            int k;
            s->val[i] = v;
            mx_decimal(v, 6, s->dec[i], sizeof(s->dec[i]));
            for (k = 0; k < pr.nquad && !ex; k++) {
                if (fabs(pr.quad_val[k] - v) < 2e-5f * (1.0f + fabs(v))) { strcpy(s->exact[i], pr.quad_text[k]); ex = true; }
            }
            if (!ex) ex = mx_exact(v, false, s->exact[i], sizeof(s->exact[i]));
            s->is_exact[i] = ex;
        }
        return;
    }

    /* trig equations that repeat */
    if (trig) {
        real period;
        if (detect_period(&e, &period)) {
            solve_trig(&e, period, s);
            if (counter && s->kind == SK_NONE) {
                /* nothing to add */
            }
            return;
        }
    }

    /* anything else: look for roots numerically */
    nroots = scan_roots(&e, -50.0f, 50.0f, 501, roots, 0);
    sort_roots(roots, nroots);
    say(s, "Solved by searching from -50 to 50.");
    if (nroots == 0) {
        s->kind = SK_NONE;
        strcpy(s->note, "No solutions found between -50 and 50.");
        return;
    }
    s->kind = SK_LIST;
    s->n = nroots > SOL_MAX ? SOL_MAX : nroots;
    s->more = nroots > SOL_MAX;
    for (i = 0; i < s->n; i++) {
        s->val[i] = roots[i];
        mx_decimal(roots[i], 6, s->dec[i], sizeof(s->dec[i]));
        s->is_exact[i] = mx_exact(roots[i], false, s->exact[i], sizeof(s->exact[i]));
    }
}
