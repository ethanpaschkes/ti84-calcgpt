#include "triangle.h"
#include "exact.h"
#include <math.h>
#include "mreal.h"
#include <stdarg.h>
#include <string.h>

#define PI_D 3.14159265358979323846f

static const char *const SN[3] = {"a", "b", "c"};
static const char *const AN[3] = {"A", "B", "C"};

static float sind(float d) { return sinf(d * PI_D / 180.0f); }
static float cosd(float d) { return cosf(d * PI_D / 180.0f); }
static float asind(float v) { return asinf(v) * 180.0f / PI_D; }
static float acosd(float v) { return acosf(v) * 180.0f / PI_D; }

static char *o_, *end_;

static void put(const char *s)
{
    while (*s && o_ < end_) *o_++ = *s++;
}

/* tiny printf: %s string, %n number (5 digits), %a angle (4 digits) */
static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt == '%' && (fmt[1] == 's' || fmt[1] == 'n' || fmt[1] == 'a')) {
            char buf[40];
            if (fmt[1] == 's') {
                put(va_arg(ap, const char *));
            } else {
                float v = (float)va_arg(ap, double);
                if (fmt[1] == 'a') v = floorf(v * 10.0f + 0.5f) / 10.0f; /* angles to the nearest tenth */
                mx_decimal(v, fmt[1] == 'n' ? 5 : 6, buf, sizeof(buf));
                put(buf);
            }
            fmt++;
        } else {
            char c[2];
            c[0] = *fmt;
            c[1] = 0;
            put(c);
        }
    }
    va_end(ap);
}

static void summary(const float *s, const float *a)
{
    say("\nA = %a, B = %a, C = %a", a[0], a[1], a[2]);
    say("\na = %n, b = %n, c = %n", s[0], s[1], s[2]);
    say("\nArea = (1/2) a b sin C = %n", 0.5f * s[0] * s[1] * sind(a[2]));
}

/* the case work for exactly three known parts */
static bool tri_core(const struct tri_in *in, char *out, size_t cap)
{
    float s[3], a[3];
    int ns = 0, na = 0, i;

    o_ = out;
    end_ = out + cap - 1;
    for (i = 0; i < 3; i++) {
        s[i] = in->s[i];
        a[i] = in->a[i];
        ns += in->has_s[i];
        na += in->has_a[i];
        if ((in->has_s[i] && in->s[i] <= 0) || (in->has_a[i] && (in->a[i] <= 0 || in->a[i] >= 180))) {
            say("Sides must be positive and angles between 0 and 180.");
            *o_ = 0;
            return false;
        }
    }
    if (ns == 0 || ns + na != 3) {
        say("Fill in exactly three values, at least one of them a side. Leave the rest blank.");
        *o_ = 0;
        return false;
    }

    if (ns == 3) {
        if (s[0] + s[1] <= s[2] || s[1] + s[2] <= s[0] || s[0] + s[2] <= s[1]) {
            say("No such triangle: each side must be shorter than the other two added together.");
            *o_ = 0;
            return false;
        }
        say("SSS: use the Law of Cosines.");
        for (i = 0; i < 2; i++) {
            float x = s[i], y = s[(i + 1) % 3], z = s[(i + 2) % 3];
            float c = (y * y + z * z - x * x) / (2 * y * z);
            a[i] = acosd(c);
            say("\ncos %s = (%s^2 + %s^2 - %s^2)/(2 %s %s) = (%n^2 + %n^2 - %n^2)/(2 x %n x %n)",
                AN[i], SN[(i + 1) % 3], SN[(i + 2) % 3], SN[i], SN[(i + 1) % 3], SN[(i + 2) % 3], y, z, x, y, z);
            say("\n   = %n, so %s = %a", c, AN[i], a[i]);
        }
        a[2] = 180.0f - a[0] - a[1];
        say("\nC = 180 - A - B = %a", a[2]);
        summary(s, a);
    } else if (ns == 1) {
        int ks = in->has_s[0] ? 0 : in->has_s[1] ? 1 : 2, miss = !in->has_a[0] ? 0 : !in->has_a[1] ? 1 : 2;
        float k;
        a[miss] = 180.0f - (in->has_a[0] ? a[0] : 0) - (in->has_a[1] ? a[1] : 0) - (in->has_a[2] ? a[2] : 0);
        if (a[miss] <= 0) {
            say("No such triangle: the two angles add up to 180 or more.");
            *o_ = 0;
            return false;
        }
        say("Two angles and a side: find the third angle, then use the Law of Sines.");
        say("\n%s = 180 - the other two = %a", AN[miss], a[miss]);
        k = s[ks] / sind(a[ks]);
        for (i = 0; i < 3; i++) {
            if (i == ks) continue;
            s[i] = k * sind(a[i]);
            say("\n%s = %s sin %s / sin %s = %n x sin(%a) / sin(%a) = %n", SN[i], SN[ks], AN[i], AN[ks],
                in->s[ks], a[i], a[ks], s[i]);
        }
        summary(s, a);
    } else {
        int ai = in->has_a[0] ? 0 : in->has_a[1] ? 1 : 2;
        int p = (ai + 1) % 3, q = (ai + 2) % 3;
        if (in->has_s[p] && in->has_s[q]) {
            /* SAS */
            float x = s[p], y = s[q];
            int small = x <= y ? p : q;
            float sm = s[small];
            s[ai] = sqrtf(x * x + y * y - 2 * x * y * cosd(a[ai]));
            say("SAS: use the Law of Cosines for the third side, then the Law of Sines.");
            say("\n%s^2 = %s^2 + %s^2 - 2 %s %s cos %s = %n^2 + %n^2 - 2 x %n x %n x cos(%a)",
                SN[ai], SN[p], SN[q], SN[p], SN[q], AN[ai], x, y, x, y, a[ai]);
            say("\n%s = %n", SN[ai], s[ai]);
            a[small] = asind(sm * sind(a[ai]) / s[ai]);
            say("\nsin %s = %s sin %s / %s = %n x sin(%a) / %n, so %s = %a (the angle opposite the shorter side is acute)",
                AN[small], SN[small], AN[ai], SN[ai], sm, a[ai], s[ai], AN[small], a[small]);
            a[3 - ai - small] = 180.0f - a[ai] - a[small];
            say("\n%s = 180 - %s - %s = %a", AN[3 - ai - small], AN[ai], AN[small], a[3 - ai - small]);
            summary(s, a);
        } else {
            /* SSA */
            int other = in->has_s[p] ? p : q;
            int third = 3 - ai - other;
            float sy, y1, A_ = a[ai];
            struct { float y, z, t; } sol[2];
            int nsol = 0, k;

            if (!in->has_s[ai]) {
                say("The side and angle must go together: a side next to its opposite angle (a with A, b with B, or c with C).");
                *o_ = 0;
                return false;
            }
            say("SSA (the ambiguous case): use the Law of Sines.");
            sy = s[other] * sind(A_) / s[ai];
            say("\nsin %s = %s sin %s / %s = %n x sin(%a) / %n = %n", AN[other], SN[other], AN[ai], SN[ai],
                s[other], A_, s[ai], sy);
            if (sy > 1.0f + 1e-5f) {
                say("\nA sine can't be more than 1: no triangle.");
                *o_ = 0;
                return false;
            }
            if (sy > 1.0f) sy = 1.0f;
            y1 = asind(sy);
            for (k = 0; k < 2; k++) {
                float yv = k == 0 ? y1 : 180.0f - y1;
                float z = 180.0f - A_ - yv;
                if ((k == 1 && fabsf(sy - 1.0f) < 1e-5f) || z <= 1e-5f) continue;
                sol[nsol].y = yv;
                sol[nsol].z = z;
                sol[nsol].t = s[ai] * sind(z) / sind(A_);
                nsol++;
            }
            if (nsol == 0) {
                say("\nNo triangle: the angles would add up to more than 180.");
                *o_ = 0;
                return false;
            }
            say("\n%s = arcsin(%n) = %a, or 180 - %a = %a (check that each leaves a positive third angle)", AN[other], sy, y1, y1, 180.0f - y1);
            say("\n%s triangle%s:", nsol == 2 ? "2" : "1", nsol == 2 ? "s" : "");
            for (k = 0; k < nsol; k++) {
                float aa[3], ss[3];
                aa[ai] = A_;
                aa[other] = sol[k].y;
                aa[third] = sol[k].z;
                ss[ai] = s[ai];
                ss[other] = s[other];
                ss[third] = sol[k].t;
                say("\n#%s: %s = 180 - %a - %a = %a, then %s = %s sin %s / sin %s = %n", k == 0 ? "1" : "2",
                    AN[third], A_, sol[k].y, sol[k].z, SN[third], SN[ai], AN[third], AN[ai], sol[k].t);
                say("\n   A = %a, B = %a, C = %a; a = %n, b = %n, c = %n", aa[0], aa[1], aa[2], ss[0], ss[1], ss[2]);
            }
        }
    }
    *o_ = 0;
    return true;
}

/* ---------- the area or the perimeter standing in for a missing part ---------- */

static bool fail(const char *msg)
{
    say("%s", msg);
    *o_ = 0;
    return false;
}

/* run the three-part solver on u and keep writing after what it wrote */
static bool run_core(const struct tri_in *u)
{
    char *at = o_;
    bool ok = tri_core(u, at, (size_t)(end_ - at) + 1);
    return ok;
}

bool tri_work(const struct tri_in *in, char *out, size_t cap)
{
    struct tri_in u;
    float s[3], a[3];
    int ns = 0, na = 0, i;

    if (!in->has_area && !in->has_perim) return tri_core(in, out, cap);

    o_ = out;
    end_ = out + cap - 1;
    for (i = 0; i < 3; i++) {
        s[i] = in->s[i];
        a[i] = in->a[i];
        ns += in->has_s[i];
        na += in->has_a[i];
        if ((in->has_s[i] && in->s[i] <= 0) || (in->has_a[i] && (in->a[i] <= 0 || in->a[i] >= 180))) return fail("Sides must be positive and angles between 0 and 180.");
    }
    if ((in->has_area && in->area <= 0) || (in->has_perim && in->perim <= 0)) return fail("The area and perimeter must be positive.");
    memset(&u, 0, sizeof(u));

    if (in->has_perim) {
        float P = in->perim;
        if (ns == 2) {
            int k = !in->has_s[0] ? 0 : !in->has_s[1] ? 1 : 2;
            u = *in;
            u.has_perim = u.has_area = false;
            u.s[k] = P - (s[0] * in->has_s[0] + s[1] * in->has_s[1] + s[2] * in->has_s[2]);
            u.has_s[k] = true;
            u.has_a[0] = u.has_a[1] = u.has_a[2] = false;
            if (u.s[k] <= 0) return fail("The perimeter is smaller than the two sides added together.");
            say("The perimeter is the three sides added: %s = P - (the other two) = %n - (%n + %n) = %n\n", SN[k], P,
                s[(k + 1) % 3], s[(k + 2) % 3], u.s[k]);
            return run_core(&u);
        }
        if (ns == 1 && in->has_a[2] && fabsf(a[2] - 90.0f) < 1e-3f) {
            /* right triangle, right angle at C, perimeter and one side */
            int i1 = in->has_s[0] ? 0 : in->has_s[1] ? 1 : 2;
            u.has_s[0] = u.has_s[1] = u.has_s[2] = true;
            if (i1 == 2) {
                float m = P - s[2], ab = (m * m - s[2] * s[2]) / 2.0f, disc = m * m - 4.0f * ab;
                if (m <= 0 || ab <= 0 || disc < 0) return fail("No right triangle has that perimeter and hypotenuse.");
                u.s[2] = s[2];
                u.s[0] = (m + sqrtf(disc)) / 2.0f;
                u.s[1] = (m - sqrtf(disc)) / 2.0f;
                say("Right triangle: a + b = P - c = %n and a^2 + b^2 = c^2, so a b = ((a + b)^2 - c^2)/2 = %n.\n", m, ab);
                say("a and b are the two numbers with that sum and product: (%n +/- sqrt(%n))/2.\n", m, disc);
            } else {
                float x = s[i1], c2 = ((P - x) * (P - x) + x * x) / (2.0f * (P - x));
                float y = P - x - c2;
                if (P <= x || y <= 0) return fail("No right triangle has that perimeter and leg.");
                u.s[i1] = x;
                u.s[1 - i1] = y;
                u.s[2] = c2;
                say("Right triangle: the other leg is P - %s - c and c^2 = %s^2 + (other leg)^2, so c = ((P - %s)^2 + %s^2)/(2 (P - %s)) = %n.\n",
                    SN[i1], SN[i1], SN[i1], SN[i1], SN[i1], c2);
            }
            return run_core(&u);
        }
        return fail("With the perimeter, also give two sides (or one leg or the hypotenuse of a right triangle).");
    }

    /* the area */
    if (ns + na == 3) {
        say("(The area was not needed: three other values were given.)\n");
        u = *in;
        u.has_area = false;
        return run_core(&u);
    }
    if (ns + na != 2) return fail("With the area, give two other values: two sides, a side and an angle, or two angles.");
    {
        float K = in->area;
        if (ns == 2) {
            /* two sides and the area: the angle between them */
            int k = !in->has_s[0] ? 0 : !in->has_s[1] ? 1 : 2, i1 = (k + 1) % 3, j1 = (k + 2) % 3;
            float r = 2.0f * K / (s[i1] * s[j1]), c1;
            int nsol, t;
            if (r > 1.0f + 1e-5f) return fail("No triangle: that area is too big for those two sides (the most is half their product).");
            if (r > 1.0f) r = 1.0f;
            c1 = asind(r);
            say("Area = (1/2) %s %s sin %s, so sin %s = 2 x Area/(%s %s) = 2 x %n/(%n x %n) = %n\n", SN[i1], SN[j1], AN[k], AN[k],
                SN[i1], SN[j1], K, s[i1], s[j1], r);
            nsol = fabsf(r - 1.0f) < 1e-5f ? 1 : 2;
            say("%s = arcsin(%n) = %a%s\n", AN[k], r, c1, nsol == 2 ? ", or the obtuse 180 minus that: two triangles fit" : "");
            for (t = 0; t < nsol; t++) {
                struct tri_in w;
                memset(&w, 0, sizeof(w));
                w.has_s[i1] = w.has_s[j1] = true;
                w.s[i1] = s[i1];
                w.s[j1] = s[j1];
                w.has_a[k] = true;
                w.a[k] = t == 0 ? c1 : 180.0f - c1;
                if (nsol == 2) say("\nTriangle #%s, %s = %a:\n", t == 0 ? "1" : "2", AN[k], w.a[k]);
                if (!run_core(&w)) return false;
            }
            return true;
        }
        if (ns == 1 && na == 1) {
            int i1 = in->has_s[0] ? 0 : in->has_s[1] ? 1 : 2, j1 = in->has_a[0] ? 0 : in->has_a[1] ? 1 : 2;
            if (i1 == j1) {
                /* side and its opposite angle: b c is fixed by the area, and b + c by the Law of Cosines */
                int p = (i1 + 1) % 3, q = (i1 + 2) % 3;
                float bc = 2.0f * K / sind(a[j1]), S2 = s[i1] * s[i1] + 2.0f * bc * (1.0f + cosd(a[j1]));
                float disc = S2 - 4.0f * bc;
                if (disc < 0) return fail("No triangle has that area with that side and its opposite angle.");
                u.has_s[0] = u.has_s[1] = u.has_s[2] = true;
                u.s[i1] = s[i1];
                u.s[p] = (sqrtf(S2) + sqrtf(disc)) / 2.0f;
                u.s[q] = (sqrtf(S2) - sqrtf(disc)) / 2.0f;
                say("Area = (1/2) %s %s sin %s gives %s %s = 2 x Area/sin %s = %n.\n", SN[p], SN[q], AN[i1], SN[p], SN[q], AN[i1], bc);
                say("The Law of Cosines then gives %s + %s = sqrt(%s^2 + 2 %s %s (1 + cos %s)) = %n.\n", SN[p], SN[q], SN[i1], SN[p], SN[q], AN[i1], sqrtf(S2));
                say("So %s and %s are the two numbers with that sum and product (swap them for the mirror image): %n and %n.\n", SN[p], SN[q], u.s[p], u.s[q]);
                return run_core(&u);
            } else {
                /* the angle sits between the known side and one more side: Area = (1/2) side side sin(angle) */
                int m = 3 - i1 - j1;
                float other = 2.0f * K / (s[i1] * sind(a[j1]));
                u.has_s[i1] = u.has_s[m] = true;
                u.s[i1] = s[i1];
                u.s[m] = other;
                u.has_a[j1] = true;
                u.a[j1] = a[j1];
                say("Angle %s is between sides %s and %s, so Area = (1/2) %s %s sin %s.\n", AN[j1], SN[i1], SN[m], SN[i1], SN[m], AN[j1]);
                say("%s = 2 x Area/(%s sin %s) = 2 x %n/(%n x sin(%a)) = %n\n", SN[m], SN[i1], AN[j1], K, s[i1], a[j1], other);
                return run_core(&u);
            }
        }
        {
            /* two angles and the area: Area = (1/2) a^2 sin B sin C / sin A */
            int x = in->has_a[0] ? 0 : 1, y = in->has_a[2] ? 2 : 1;
            int z = 3 - x - y;
            float third = 180.0f - a[x] - a[y], side;
            if (third <= 0) return fail("No such triangle: the two angles add up to 180 or more.");
            a[z] = third;
            side = sqrtf(2.0f * K * sind(a[0]) / (sind(a[1]) * sind(a[2])));
            u.has_s[0] = true;
            u.s[0] = side;
            u.has_a[x] = u.has_a[y] = true;
            u.a[x] = a[x];
            u.a[y] = a[y];
            say("%s = 180 - %a - %a = %a.\n", AN[z], a[x], a[y], third);
            say("Area = (1/2) a^2 sin B sin C / sin A, so a = sqrt(2 x Area x sin A/(sin B sin C)) = %n\n", side);
            return run_core(&u);
        }
    }
}
