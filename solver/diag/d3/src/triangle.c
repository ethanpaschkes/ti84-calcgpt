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
                mx_decimal(v, fmt[1] == 'n' ? 5 : 8, buf, sizeof(buf));
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

bool tri_work(const struct tri_in *in, char *out, size_t cap)
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
            if (sy > 1.0f + 1e-9f) {
                say("\nA sine can't be more than 1: no triangle.");
                *o_ = 0;
                return false;
            }
            if (sy > 1.0f) sy = 1.0f;
            y1 = asind(sy);
            for (k = 0; k < 2; k++) {
                float yv = k == 0 ? y1 : 180.0f - y1;
                float z = 180.0f - A_ - yv;
                if ((k == 1 && fabsf(sy - 1.0f) < 1e-9f) || z <= 1e-9f) continue;
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
