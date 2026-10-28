#include "exact.h"
#include <stdint.h>
#include <math.h>
#include "mreal.h"
#include <string.h>

#define PI_D 3.14159265358979323846L

int32_t mx_gcd(int32_t a, int32_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) {
        int32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

bool mx_rationalize(real v, int32_t maxden, real tol, int32_t *n, int32_t *d)
{
    /* continued fraction expansion */
    int32_t h0 = 1, h1 = (int32_t)floor(v), k0 = 0, k1 = 1;
    real x = v - floor(v);
    int i;

    if (fabs(v) > 2e6L) return false;
    if (fabs(v - (real)h1) <= tol) {
        *n = h1; *d = 1;
        return true;
    }
    for (i = 0; i < 40; i++) {
        real a_f;
        int32_t a, h2, k2;
        if (fabs(x) < 1e-15L) break;
        x = 1.0L / x;
        a_f = floor(x);
        if (a_f > 1e6L) break;
        a = (int32_t)a_f;
        x -= a_f;
        h2 = a * h1 + h0;
        k2 = a * k1 + k0;
        if (k2 > maxden || k2 < 0) break;
        h0 = h1; h1 = h2; k0 = k1; k1 = k2;
        if (fabs(v - (real)h1 / (real)k1) <= tol) {
            *n = h1; *d = k1;
            return true;
        }
    }
    return false;
}

void mx_simplify_radical(int32_t N, int32_t *s, int32_t *m)
{
    int32_t f, outside = 1;
    for (f = 2; f * f <= N; f++) {
        while (N % (f * f) == 0) {
            N /= f * f;
            outside *= f;
        }
    }
    *s = outside;
    *m = N;
}

static int32_t abs_ll(int32_t v)
{
    return v < 0 ? -v : v;
}

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) *(*o)++ = *s++;
}

static void put_int(char **o, char *end, int32_t v)
{
    char tmp[24];
    int n = 0;
    if (v < 0) {
        put(o, end, "-");
        v = -v;
    }
    do {
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v);
    while (n) {
        char c[2];
        c[0] = tmp[--n];
        c[1] = 0;
        put(o, end, c);
    }
}

void mx_decimal(real v, int sig, char *out, size_t cap)
{
    char *o = out, *end = out + cap - 1;
    uint32_t ip;
    real scale = 1.0L, a;
    int32_t frac;
    int places, i, digits;

    if (v != v) { strncpy(out, "undefined", cap - 1); out[cap - 1] = 0; return; }
    if (fabs(v) < 1e-12L) { strcpy(out, "0"); return; }
    if (v < 0) {
        put(&o, end, "-");
        v = -v;
    }
    if (v >= 1e15L) { put(&o, end, "very large"); *o = 0; return; }
    a = v;
    digits = (int)floor(log10(a)) + 1;
    places = sig - digits;
    if (places < 0) places = 0;
    if (places > 9) places = 9;
    for (i = 0; i < places; i++) scale *= 10.0L;
    v = floor(v * scale + 0.5L) / scale;
    ip = (uint32_t)v;
    frac = (int32_t)((v - (real)ip) * scale + 0.5L);
    put_int(&o, end, (int32_t)ip);
    if (frac && places > 0) {
        char d[12];
        int last = places - 1;
        for (i = places - 1; i >= 0; i--) { d[i] = (char)('0' + frac % 10); frac /= 10; }
        while (last > 0 && d[last] == '0') last--;
        put(&o, end, ".");
        for (i = 0; i <= last; i++) {
            char c[2];
            c[0] = d[i];
            c[1] = 0;
            put(&o, end, c);
        }
    }
    *o = 0;
}

/* "[num|den]" or just num when den is 1 */
static void put_frac(char **o, char *end, const char *num, int32_t den)
{
    if (den == 1) {
        put(o, end, num);
        return;
    }
    put(o, end, "[");
    put(o, end, num);
    put(o, end, "|");
    put_int(o, end, den);
    put(o, end, "]");
}

bool mx_exact(real v, bool allow_pi, char *out, size_t cap)
{
    char *o = out, *end = out + cap - 1;
    real tol = 1e-9L * (fabs(v) > 1.0L ? fabs(v) : 1.0L);
    int32_t n, d, q;
    bool neg = v < 0;
    real a = fabs(v);

    if (v != v) { strncpy(out, "undefined", cap - 1); out[cap - 1] = 0; return false; }
    if (a < 1e-12L) { strcpy(out, "0"); return true; }

    /* integer or fraction */
    if (mx_rationalize(v, 1000, tol, &n, &d)) {
        char num[24];
        char *np = num;
        char *ne = num + sizeof(num) - 1;
        put_int(&np, ne, n);
        *np = 0;
        put_frac(&o, end, num, d);
        *o = 0;
        return true;
    }
    /* a * sqrt(m) / q */
    for (q = 1; q <= 60; q++) {
        real w = a * (real)q;
        real w2 = w * w;
        real Nr = floor(w2 + 0.5L);
        int32_t N = Nr < 40000.0L ? (int32_t)Nr : 0; /* keeps the whole-number maths from overflowing */
        if (N > 1 && fabs(w2 - (real)N) < 1e-8L * (1.0L + w2)) {
            int32_t s, m, g;
            char num[40];
            char *np = num, *ne = num + sizeof(num) - 1;
            mx_simplify_radical(N, &s, &m);
            g = mx_gcd(s, q);
            s /= g;
            if (neg) put(&np, ne, "-");
            if (s != 1) put_int(&np, ne, s);
            put(&np, ne, "sqrt(");
            put_int(&np, ne, m);
            put(&np, ne, ")");
            *np = 0;
            put_frac(&o, end, num, q / g);
            *o = 0;
            return true;
        }
    }
    /* (p + q sqrt(m)) / r, like the golden ratio (1 + sqrt(5))/2 */
    {
        static const int ms[] = {2, 3, 5, 6, 7, 10, 11, 13, 14, 15, 17, 19, 21, 22, 23, 29, 30};
        int32_t r;
        int mi;
        for (r = 1; r <= 12; r++) {
            for (mi = 0; mi < (int)(sizeof(ms) / sizeof(ms[0])); mi++) {
                real rt = sqrt((real)ms[mi]);
                int32_t p;
                for (p = -40; p <= 40; p++) {
                    real qd = (v * (real)r - (real)p) / rt;
                    int32_t q = (int32_t)floor(qd + 0.5L);
                    if (q != 0 && abs_ll(q) <= 20 && fabs(qd - (real)q) < 1e-8L) {
                        int32_t g = mx_gcd(mx_gcd(p, q), r);
                        char num[60];
                        char *np = num, *ne = num + sizeof(num) - 1;
                        int32_t pp = p / g, qq = q / g, rr = r / g;
                        if (pp != 0) { put_int(&np, ne, pp); put(&np, ne, qq < 0 ? " - " : " + "); }
                        else if (qq < 0) put(&np, ne, "-");
                        if (abs_ll(qq) != 1) put_int(&np, ne, abs_ll(qq));
                        put(&np, ne, "sqrt(");
                        put_int(&np, ne, ms[mi]);
                        put(&np, ne, ")");
                        *np = 0;
                        put_frac(&o, end, num, rr);
                        *o = 0;
                        return true;
                    }
                }
            }
        }
    }
    /* rational multiple of pi */
    if (allow_pi) {
        real u = v / PI_D;
        if (mx_rationalize(u, 24, 1e-9L * (fabs(u) > 1.0L ? fabs(u) : 1.0L), &n, &d) && n != 0) {
            char num[24];
            char *np = num, *ne = num + sizeof(num) - 1;
            int32_t an = n < 0 ? -n : n;
            if (n < 0) put(&np, ne, "-");
            if (an != 1) put_int(&np, ne, an);
            put(&np, ne, "pi");
            *np = 0;
            put_frac(&o, end, num, d);
            *o = 0;
            return true;
        }
    }
    mx_decimal(v, 6, out, cap);
    return false;
}
