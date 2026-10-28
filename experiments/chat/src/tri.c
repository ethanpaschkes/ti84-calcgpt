/* Triangle solver using the law of sines and law of cosines. */
#include "tri.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define PI_D 3.14159265358979323846

struct tri {
    double s[3]; /* sides a, b, c */
    double a[3]; /* angles A, B, C in degrees */
};

static double rad(double d) { return d * PI_D / 180.0; }
static double deg(double r) { return r * 180.0 / PI_D; }

static void put(char **o, char *end, const char *s)
{
    while (*s && *o < end) {
        *(*o)++ = *s++;
    }
}

/* Append v with up to `places` decimals, dropping trailing zeros. */
static void put_num(char **o, char *end, double v, int places)
{
    char buf[40];
    char *b = buf;
    unsigned long long ip;
    double scale = 1.0;
    long frac;
    int i;

    if (v < 0) {
        *b++ = '-';
        v = -v;
    }
    for (i = 0; i < places; i++) {
        scale *= 10.0;
    }
    v = floor(v * scale + 0.5) / scale;
    ip = (unsigned long long)v;
    frac = (long)((v - (double)ip) * scale + 0.5);
    {
        char tmp[24];
        int n = 0;
        unsigned long long t = ip;
        do {
            tmp[n++] = (char)('0' + (int)(t % 10));
            t /= 10;
        } while (t);
        while (n) *b++ = tmp[--n];
    }
    if (frac && places > 0) {
        char d[8];
        int last = places - 1;
        for (i = places - 1; i >= 0; i--) {
            d[i] = (char)('0' + frac % 10);
            frac /= 10;
        }
        while (last > 0 && d[last] == '0') last--;
        *b++ = '.';
        for (i = 0; i <= last; i++) *b++ = d[i];
    }
    *b = 0;
    put(o, end, buf);
}

static void put_solution(char **o, char *end, const struct tri *t)
{
    static const char *const an[3] = {"A", "B", "C"};
    static const char *const sn[3] = {"a", "b", "c"};
    int i;

    for (i = 0; i < 3; i++) {
        put(o, end, an[i]);
        put(o, end, " = ");
        put_num(o, end, t->a[i], 1);
        put(o, end, i < 2 ? ", " : "\n");
    }
    for (i = 0; i < 3; i++) {
        put(o, end, sn[i]);
        put(o, end, " = ");
        put_num(o, end, t->s[i], 3);
        put(o, end, i < 2 ? ", " : "\n");
    }
    put(o, end, "Area = ");
    put_num(o, end, 0.5 * t->s[0] * t->s[1] * sin(rad(t->a[2])), 3);
}

/* Law of sines once all angles and one side are known. */
static void finish_with_angles(struct tri *t, int known_side)
{
    int i;
    double k = t->s[known_side] / sin(rad(t->a[known_side]));
    for (i = 0; i < 3; i++) {
        if (i != known_side) {
            t->s[i] = k * sin(rad(t->a[i]));
        }
    }
}

static void sss(struct tri *t)
{
    int i;
    for (i = 0; i < 3; i++) {
        double x = t->s[i], y = t->s[(i + 1) % 3], z = t->s[(i + 2) % 3];
        double c = (y * y + z * z - x * x) / (2 * y * z);
        if (c > 1) c = 1;
        if (c < -1) c = -1;
        t->a[i] = deg(acos(c));
    }
}

/* Parse the message: word followed by a number. Returns the count of values read. */
static int parse(const char *s, struct tri *t, bool hs[3], bool ha[3], bool *bad)
{
    int count = 0;

    *bad = false;
    while (*s) {
        char w[3];
        int kind = -1, idx = -1;
        if ((s[0] == 's' || s[0] == 'S' || s[0] == 'a' || s[0] == 'A')
            && ((s[1] >= 'a' && s[1] <= 'c') || (s[1] >= 'A' && s[1] <= 'C'))
            && !((s[2] >= 'a' && s[2] <= 'z') || (s[2] >= 'A' && s[2] <= 'Z'))) {
            w[0] = s[0] | 0x20;
            w[1] = s[1] | 0x20;
            kind = w[0] == 's' ? 0 : 1;
            idx = w[1] - 'a';
            s += 2;
            while (*s == ' ' || *s == '=' || *s == ':') s++;
            if ((*s >= '0' && *s <= '9') || *s == '.') {
                double v = 0, scale = 0.1;
                while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
                if (*s == '.') {
                    s++;
                    while (*s >= '0' && *s <= '9') { v += (*s - '0') * scale; scale /= 10; s++; }
                }
                if (kind == 0) {
                    if (hs[idx]) *bad = true;
                    hs[idx] = true;
                    t->s[idx] = v;
                } else {
                    if (ha[idx]) *bad = true;
                    ha[idx] = true;
                    t->a[idx] = v;
                }
                count++;
                continue;
            }
            *bad = true;
            continue;
        }
        /* skip the rest of a word */
        while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) s++;
        if (*s) s++;
    }
    return count;
}

bool tri_try(const char *input, char *result, size_t cap)
{
    struct tri t;
    bool hs[3] = {false, false, false}, ha[3] = {false, false, false};
    bool bad;
    int ns, na, count, i;
    char *o = result;
    char *end = result + cap - 1;

    memset(&t, 0, sizeof(t));
    count = parse(input, &t, hs, ha, &bad);
    if (count == 0) {
        return false;
    }
    ns = hs[0] + hs[1] + hs[2];
    na = ha[0] + ha[1] + ha[2];
    if (bad || count != 3 || ns == 0 || ns + na != 3) {
        put(&o, end, "Give three values, at least one a side. Sides are sa sb sc and angles are aa ab ac (a is opposite A). Example: sa 6 sb 8 sc 11");
        *o = 0;
        return true;
    }
    for (i = 0; i < 3; i++) {
        if ((hs[i] && t.s[i] <= 0) || (ha[i] && (t.a[i] <= 0 || t.a[i] >= 180))) {
            put(&o, end, "Sides must be positive and angles between 0 and 180.");
            *o = 0;
            return true;
        }
    }

    if (ns == 3) {
        if (t.s[0] + t.s[1] <= t.s[2] || t.s[1] + t.s[2] <= t.s[0] || t.s[0] + t.s[2] <= t.s[1]) {
            put(&o, end, "No such triangle exists: each side must be shorter than the other two added together.");
            *o = 0;
            return true;
        }
        sss(&t);
        put_solution(&o, end, &t);
    } else if (ns == 1) {
        /* two angles and a side */
        int known_side = hs[0] ? 0 : hs[1] ? 1 : 2;
        int missing = !ha[0] ? 0 : !ha[1] ? 1 : 2;
        if (ha[0] + ha[1] + ha[2] != 2) {
            put(&o, end, "Not enough information.");
            *o = 0;
            return true;
        }
        t.a[missing] = 180.0 - (ha[0] ? t.a[0] : 0) - (ha[1] ? t.a[1] : 0) - (ha[2] ? t.a[2] : 0);
        if (t.a[missing] <= 0) {
            put(&o, end, "No such triangle exists: the angles add up to 180 or more.");
            *o = 0;
            return true;
        }
        finish_with_angles(&t, known_side);
        put_solution(&o, end, &t);
    } else {
        /* two sides and an angle */
        int ai = ha[0] ? 0 : ha[1] ? 1 : 2;
        int s1 = (ai + 1) % 3, s2 = (ai + 2) % 3;
        if (hs[s1] && hs[s2]) {
            /* SAS: the angle is between the two sides */
            double x = t.s[s1], y = t.s[s2];
            t.s[ai] = sqrt(x * x + y * y - 2 * x * y * cos(rad(t.a[ai])));
            /* the angle opposite the shorter known side is acute */
            if (x <= y) {
                t.a[s1] = deg(asin(x * sin(rad(t.a[ai])) / t.s[ai]));
                t.a[s2] = 180.0 - t.a[ai] - t.a[s1];
            } else {
                t.a[s2] = deg(asin(y * sin(rad(t.a[ai])) / t.s[ai]));
                t.a[s1] = 180.0 - t.a[ai] - t.a[s2];
            }
            put_solution(&o, end, &t);
        } else {
            /* SSA: the angle is opposite one of the given sides */
            int known = hs[ai] ? ai : -1;
            int other = hs[s1] ? s1 : s2;
            double sin_y, y1;
            struct tri sol[2];
            int nsol = 0, k;

            if (known < 0) {
                put(&o, end, "Not enough information.");
                *o = 0;
                return true;
            }
            sin_y = t.s[other] * sin(rad(t.a[known])) / t.s[known];
            if (sin_y > 1.0 + 1e-9) {
                put(&o, end, "No such triangle exists.");
                *o = 0;
                return true;
            }
            if (sin_y > 1.0) sin_y = 1.0;
            y1 = deg(asin(sin_y));
            for (k = 0; k < 2; k++) {
                double yv = k == 0 ? y1 : 180.0 - y1;
                double z = 180.0 - t.a[known] - yv;
                int third = 3 - known - other;
                if ((k == 1 && fabs(sin_y - 1.0) < 1e-9) || z <= 1e-9) {
                    continue;
                }
                sol[nsol] = t;
                sol[nsol].a[other] = yv;
                sol[nsol].a[third] = z;
                sol[nsol].s[third] = t.s[known] * sin(rad(z)) / sin(rad(t.a[known]));
                nsol++;
            }
            if (nsol == 0) {
                put(&o, end, "No such triangle exists.");
            } else if (nsol == 1) {
                put_solution(&o, end, &sol[0]);
            } else {
                put(&o, end, "2 triangles:\n");
                for (k = 0; k < 2; k++) {
                    put(&o, end, k == 0 ? "1) " : "\n2) ");
                    put_solution(&o, end, &sol[k]);
                }
            }
        }
    }
    *o = 0;
    return true;
}
