#include "poly.h"
#include <stdint.h>
#include "exact.h"
#include <math.h>
#include "mreal.h"
#include <string.h>

/* ---------- fractions ---------- */

static struct frac fr(int32_t n, int32_t d)
{
    struct frac f;
    int32_t g;
    if (d < 0) { n = -n; d = -d; }
    g = mx_gcd(n, d);
    if (g > 1) { n /= g; d /= g; }
    f.n = n;
    f.d = d ? d : 1;
    return f;
}
static struct frac f_add(struct frac a, struct frac b) { return fr(a.n * b.d + b.n * a.d, a.d * b.d); }
static struct frac f_mul(struct frac a, struct frac b) { return fr(a.n * b.n, a.d * b.d); }
static real f_val(struct frac a) { return (real)a.n / (real)a.d; }
static bool f_zero(struct frac a) { return a.n == 0; }

/* ---------- fitting ---------- */

static void trim(struct poly *p)
{
    while (p->deg > 0 && f_zero(p->c[p->deg])) p->deg--;
}

static real poly_eval(const struct poly *p, real x)
{
    real v = 0;
    int i;
    for (i = p->deg; i >= 0; i--) v = v * x + f_val(p->c[i]);
    return v;
}

bool poly_fit(mx_fn fn, void *ctx, struct poly *out)
{
    /* solve the 7 x 7 system with x = 0..6 */
    real A[7][8];
    static const real test[] = {-3.7L, 0.37L, 2.9L, 5.5L, 7.3L, -1.2L, 10.1L, 3.3L};
    int i, j, k;

    for (i = 0; i < 7; i++) {
        real y = fn((real)i, ctx);
        real xp = 1.0L;
        if (y != y) return false;
        for (j = 0; j < 7; j++) {
            A[i][j] = xp;
            xp *= (real)i;
        }
        A[i][7] = y;
    }
    for (i = 0; i < 7; i++) {
        int piv = i;
        for (k = i + 1; k < 7; k++) if (fabs(A[k][i]) > fabs(A[piv][i])) piv = k;
        if (fabs(A[piv][i]) < 1e-12L) return false;
        if (piv != i) for (j = 0; j < 8; j++) { real t = A[i][j]; A[i][j] = A[piv][j]; A[piv][j] = t; }
        for (k = 0; k < 7; k++) {
            real f;
            if (k == i) continue;
            f = A[k][i] / A[i][i];
            for (j = i; j < 8; j++) A[k][j] -= f * A[i][j];
        }
    }
    out->deg = 6;
    for (i = 0; i < 7; i++) {
        real c = A[i][7] / A[i][i];
        int32_t n, d;
        if (fabs(c) < 1e-9L) { out->c[i] = fr(0, 1); continue; }
        if (fabs(c) > 20000.0L || !mx_rationalize(c, 60, 1e-7L * (1.0L + fabs(c)), &n, &d)) return false;
        out->c[i] = fr(n, d);
    }
    trim(out);
    {
        int32_t l = 1;
        for (i = 0; i <= out->deg; i++) {
            l = l / mx_gcd(l, out->c[i].d) * out->c[i].d;
            if (l > 5000) return false;
        }
    }
    for (i = 0; i < 8; i++) {
        real want = fn(test[i], ctx);
        real got = poly_eval(out, test[i]);
        if (want != want || fabs(want - got) > 1e-7L * (1.0L + fabs(want))) return false;
    }
    return true;
}

/* ---------- text ---------- */

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) *(*o)++ = *s++;
}

static void put_ll(char **o, char *end, int32_t v)
{
    char tmp[24];
    int n = 0;
    if (v < 0) { put(o, end, "-"); v = -v; }
    do { tmp[n++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v);
    while (n) { char c[2]; c[0] = tmp[--n]; c[1] = 0; put(o, end, c); }
}

void poly_text(const struct poly *p, char *out, size_t cap)
{
    char *o = out, *end = out + cap - 1;
    bool first = true;
    int i;

    for (i = p->deg; i >= 0; i--) {
        struct frac c = p->c[i];
        bool neg;
        int32_t an;
        if (f_zero(c)) continue;
        neg = c.n < 0;
        an = neg ? -c.n : c.n;
        if (first) {
            if (neg) put(&o, end, "-");
        } else {
            put(&o, end, neg ? " - " : " + ");
        }
        first = false;
        if (i == 0 || an != 1 || c.d != 1) {
            if (c.d != 1) {
                put(&o, end, "[");
                put_ll(&o, end, an);
                put(&o, end, "|");
                put_ll(&o, end, c.d);
                put(&o, end, "]");
            } else {
                put_ll(&o, end, an);
            }
        }
        if (i >= 1) put(&o, end, "x");
        if (i >= 2) { put(&o, end, "^"); put_ll(&o, end, i); }
    }
    if (first) put(&o, end, "0");
    *o = 0;
}

/* ---------- solving ---------- */

/* divide p by (x - r), r a fraction */
static void deflate(struct poly *p, struct frac r)
{
    struct frac b[POLY_MAX];
    int i;
    b[p->deg - 1] = p->c[p->deg];
    for (i = p->deg - 1; i >= 1; i--) b[i - 1] = f_add(p->c[i], f_mul(r, b[i]));
    for (i = 0; i < p->deg; i++) p->c[i] = b[i];
    p->deg--;
}

void poly_deflate_root(struct poly *p, struct frac root)
{
    deflate(p, root);
}

static int32_t lcm_den(const struct poly *p)
{
    int32_t l = 1;
    int i;
    for (i = 0; i <= p->deg; i++) {
        int32_t g = mx_gcd(l, p->c[i].d);
        l = l / g * p->c[i].d;
    }
    return l;
}

/* one rational root of p, or false */
static bool find_rational_root(const struct poly *p, struct frac *root)
{
    int32_t L = lcm_den(p), a0, an, ps[400], qs[400];
    int np = 0, nq = 0, i, j, s;
    int32_t f;

    if (p->deg < 1) return false;
    if (f_zero(p->c[0])) { *root = fr(0, 1); return true; }
    a0 = p->c[0].n * (L / p->c[0].d);
    an = p->c[p->deg].n * (L / p->c[p->deg].d);
    if (a0 < 0) a0 = -a0;
    if (an < 0) an = -an;
    if (a0 > 20000000 || an > 20000000) return false;
    for (f = 1; f * f <= a0 && np < 396; f++) {
        if (a0 % f == 0) { ps[np++] = f; if (f != a0 / f) ps[np++] = a0 / f; }
    }
    for (f = 1; f * f <= an && nq < 396; f++) {
        if (an % f == 0) { qs[nq++] = f; if (f != an / f) qs[nq++] = an / f; }
    }
    for (i = 0; i < np; i++) {
        for (j = 0; j < nq; j++) {
            for (s = -1; s <= 1; s += 2) {
                struct frac cand = fr(s * ps[i], qs[j]);
                real x = f_val(cand), v = 0, scale = 0;
                int k;
                for (k = p->deg; k >= 0; k--) {
                    v = v * x + f_val(p->c[k]);
                    scale = scale * fabs(x) + fabs(f_val(p->c[k]));
                }
                if (fabs(v) <= 1e-11L * (1.0L + scale)) {
                    *root = cand;
                    return true;
                }
            }
        }
    }
    return false;
}

static void fmt_signed_term(char **o, char *end, int32_t v)
{
    put_ll(o, end, v);
}

void poly_solve(const struct poly *p, struct poly_roots *r)
{
    struct poly q = *p;
    struct frac root;
    int i;

    memset(r, 0, sizeof(*r));
    while (q.deg >= 1 && r->nrat < 6 && find_rational_root(&q, &root)) {
        for (i = 0; i < r->nrat; i++) {
            if (r->rat[i].n == root.n && r->rat[i].d == root.d) break;
        }
        if (i == r->nrat) {
            r->rat[i] = root;
            r->mult[i] = 0;
            r->nrat++;
        }
        r->mult[i]++;
        deflate(&q, root);
    }
    r->rest = q;
    r->nquad = -1;
    if (q.deg == 2) {
        int32_t L = lcm_den(&q);
        int32_t a = q.c[2].n * (L / q.c[2].d), b = q.c[1].n * (L / q.c[1].d), c = q.c[0].n * (L / q.c[0].d);
        int32_t D = b * b - 4 * a * c;
        r->qa = a; r->qb = b; r->qc = c; r->disc = D;
        if (D < 0) {
            r->nquad = -1;
        } else if (D == 0) {
            char *o = r->quad_text[0], *end = o + sizeof(r->quad_text[0]) - 1;
            struct frac x = fr(-b, 2 * a);
            if (x.d == 1) put_ll(&o, end, x.n);
            else { put(&o, end, "["); put_ll(&o, end, x.n); put(&o, end, "|"); put_ll(&o, end, x.d); put(&o, end, "]"); }
            *o = 0;
            r->quad_val[0] = f_val(x);
            r->nquad = 1;
        } else {
            int32_t s, m, g;
            int k;
            mx_simplify_radical(D, &s, &m);
            r->nquad = 2;
            for (k = 0; k < 2; k++) {
                char *o = r->quad_text[k], *end = o + sizeof(r->quad_text[k]) - 1;
                int32_t bb = -b, ss = s, den = 2 * a;
                g = mx_gcd(mx_gcd(bb, ss), den);
                if (g < 1) g = 1;
                bb /= g; ss /= g; den /= g;
                if (den < 0) { den = -den; bb = -bb; /* flip so the denominator is positive */ }
                r->quad_val[k] = ((real)(-b) + (k == 0 ? 1.0L : -1.0L) * sqrt((real)D)) / (2.0L * (real)a);
                if (den != 1) put(&o, end, "[");
                if (bb != 0) put_ll(&o, end, bb);
                {
                    /* after dividing the denominator the sign of the radical term follows k and sign of a */
                    bool plus = (k == 0) == (a > 0);
                    if (bb != 0) put(&o, end, plus ? " + " : " - ");
                    else if (!plus) put(&o, end, "-");
                }
                if (ss != 1) put_ll(&o, end, ss);
                put(&o, end, "sqrt(");
                put_ll(&o, end, m);
                put(&o, end, ")");
                if (den != 1) { put(&o, end, "|"); put_ll(&o, end, den); put(&o, end, "]"); }
                *o = 0;
            }
            /* order the two roots from smallest to largest */
            if (r->quad_val[0] > r->quad_val[1]) {
                char tmp[40];
                real v = r->quad_val[0];
                strcpy(tmp, r->quad_text[0]);
                strcpy(r->quad_text[0], r->quad_text[1]);
                strcpy(r->quad_text[1], tmp);
                r->quad_val[0] = r->quad_val[1];
                r->quad_val[1] = v;
            }
        }
    } else if (q.deg == 0) {
        r->nquad = 0;
    }
    (void)fmt_signed_term;
}

bool poly_factor_text(const struct poly *p, const struct poly_roots *r, char *out, size_t cap)
{
    char *o = out, *end = out + cap - 1;
    int32_t qprod = 1;
    int i, k;
    bool any = false;

    if (r->nrat == 0) return false;
    for (i = 0; i < r->nrat; i++) for (k = 0; k < r->mult[i]; k++) qprod *= r->rat[i].d;
    /* p = (1/prod(q)) * prod(q x - p) * rest, so the constant in front is
       rest/prod(q) when nothing but a constant is left, otherwise 1/prod(q) */
    (void)p;
    {
        struct frac c = r->rest.deg == 0 ? fr(r->rest.c[0].n, r->rest.c[0].d * qprod) : fr(1, qprod);
        if (!(c.n == 1 && c.d == 1)) {
            if (c.n == -1 && c.d == 1) put(&o, end, "-");
            else if (c.d == 1) put_ll(&o, end, c.n);
            else { put(&o, end, "["); put_ll(&o, end, c.n); put(&o, end, "|"); put_ll(&o, end, c.d); put(&o, end, "]"); }
        }
    }
    for (i = 0; i < r->nrat; i++) {
        for (k = 0; k < r->mult[i]; k++) {
            struct frac rt = r->rat[i];
            put(&o, end, "(");
            if (rt.d != 1) put_ll(&o, end, rt.d);
            put(&o, end, "x");
            if (rt.n == 0) {
                /* just x */
            } else if (rt.n > 0) {
                put(&o, end, " - ");
                put_ll(&o, end, rt.n);
            } else {
                put(&o, end, " + ");
                put_ll(&o, end, -rt.n);
            }
            put(&o, end, ")");
            any = true;
        }
    }
    if (r->rest.deg >= 1) {
        char tmp[60];
        poly_text(&r->rest, tmp, sizeof(tmp));
        put(&o, end, "(");
        put(&o, end, tmp);
        put(&o, end, ")");
    }
    *o = 0;
    return any;
}
