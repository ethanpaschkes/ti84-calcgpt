#include "tri.h"
#include <math.h>
#include <string.h>
#include "num.h"

#define PI_F 3.14159265f
#define FAIL(...) do { say_to(ans->err, ERR_MAX); say(__VA_ARGS__); out->n = 0; return false; } while (0)

static const char *const SN[3] = {"a", "b", "c"};
static const char *const AN[3] = {"A", "B", "C"};

static float sind(float d) { return sinf(d * (PI_F / 180.0f)); }
static float cosd(float d) { return cosf(d * (PI_F / 180.0f)); }
static float clamp1(float v) { return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v; }
static float asind(float v) { return asinf(clamp1(v)) * (180.0f / PI_F); }
static float acosd(float v) { return acosf(clamp1(v)) * (180.0f / PI_F); }

float tri_area(const struct tri_sol *t)
{
    return 0.5f * t->s[0] * t->s[1] * sind(t->a[2]);
}

static void keep(struct tri_out *out, const float *s, const float *a)
{
    int i;
    for (i = 0; i < 3; i++) {
        out->sol[out->n].s[i] = s[i];
        out->sol[out->n].a[i] = a[i];
    }
    out->n++;
}

/* three sides: the Law of Cosines */
static bool sss(float *s, float *a, struct tri_out *out, struct answer *ans)
{
    int i, big = 0;
    for (i = 1; i < 3; i++) if (s[i] > s[big]) big = i;
    {
        int p = (big + 1) % 3, q = (big + 2) % 3;
        if (s[p] + s[q] <= s[big])
            FAIL("No triangle: %s + %s = %n is not more than %s = %n, so the two short sides can't meet.", SN[p], SN[q],
                 s[p] + s[q], SN[big], s[big]);
    }
    ans->rule = "Law of Cosines (SSS)";
    say("Law of Cosines: cos A = (b^2 + c^2 - a^2) / (2bc)");
    for (i = 0; i < 2; i++) {
        float y = s[(i + 1) % 3], z = s[(i + 2) % 3], x = s[i];
        float cv = (y * y + z * z - x * x) / (2.0f * y * z);
        a[i] = acosd(cv);
        say("\ncos %s = (%n + %n - %n) / (2 x %n x %n) = %n, so %s = %n", AN[i], y * y, z * z, x * x, y, z, cv, AN[i], a[i]);
    }
    a[2] = 180.0f - a[0] - a[1];
    say("\nAngles add to 180: C = 180 - %n - %n = %n", a[0], a[1], a[2]);
    keep(out, s, a);
    return true;
}

/* two angles and a side: the third angle, then the Law of Sines */
static bool asa(const bool *has, float *s, float *a, bool right, struct tri_out *out, struct answer *ans)
{
    int ks = has[0] ? 0 : has[1] ? 1 : 2;
    int miss = !has[3] ? 0 : !has[4] ? 1 : 2, i;
    float k, sum = 0;
    for (i = 0; i < 3; i++) if (i != miss) sum += a[i];
    if (sum >= 180.0f) FAIL("No triangle: the two angles add to %n, which is 180 or more. A triangle's three angles add to exactly 180.", sum);
    a[miss] = 180.0f - sum;
    ans->rule = right ? "Right triangle (sin, cos)" : "Law of Sines (ASA/AAS)";
    say("Angles add to 180: %s = 180 - %n - %n = %n", AN[miss], a[(miss + 1) % 3], a[(miss + 2) % 3], a[miss]);
    say("\nLaw of Sines: a/sin A = b/sin B = c/sin C");
    if (right) say("\n(in a right triangle that is SOH CAH TOA: sin A = a/c, cos A = b/c)");
    k = s[ks] / sind(a[ks]);
    for (i = 0; i < 3; i++) {
        if (i == ks) continue;
        s[i] = k * sind(a[i]);
        say("\n%s = %s sin %s / sin %s = %n x sin(%n) / sin(%n) = %n", SN[i], SN[ks], AN[i], AN[ks], s[ks], a[i], a[ks], s[i]);
    }
    keep(out, s, a);
    return true;
}

/* two sides and the angle between them */
static bool sas(int ai, float *s, float *a, bool right, struct tri_out *out, struct answer *ans)
{
    int p = (ai + 1) % 3, q = (ai + 2) % 3;
    int small = s[p] <= s[q] ? p : q, other;
    float x = s[p], y = s[q];
    (void)ans;
    if (right && ai == 2) {
        s[2] = sqrtf(x * x + y * y);
        ans->rule = "Pythagoras";
        say("Pythagoras: c^2 = a^2 + b^2");
        say("\nc^2 = %n^2 + %n^2 = %n + %n = %n", x, y, x * x, y * y, s[2] * s[2]);
        say("\nc = sqrt(%n) = %n", s[2] * s[2], s[2]);
        a[0] = atanf(s[0] / s[1]) * (180.0f / PI_F);
        a[1] = 90.0f - a[0];
        say("\ntan A = a/b = %n / %n = %n, so A = %n", s[0], s[1], s[0] / s[1], a[0]);
        say("\nB = 90 - A = %n", a[1]);
        keep(out, s, a);
        return true;
    }
    s[ai] = sqrtf(x * x + y * y - 2.0f * x * y * cosd(a[ai]));
    ans->rule = "Law of Cosines, then Sines (SAS)";
    say("Law of Cosines: %s^2 = %s^2 + %s^2 - 2 %s %s cos %s", SN[ai], SN[p], SN[q], SN[p], SN[q], AN[ai]);
    say("\n%s^2 = %n + %n - 2(%n)(%n)cos(%n) = %n, so %s = %n", SN[ai], x * x, y * y, x, y, a[ai], s[ai] * s[ai], SN[ai], s[ai]);
    a[small] = asind(s[small] * sind(a[ai]) / s[ai]);
    say("\nLaw of Sines: sin %s = %s sin %s / %s = %n x sin(%n) / %n = %n, so %s = %n", AN[small], SN[small], AN[ai], SN[ai],
        s[small], a[ai], s[ai], s[small] * sind(a[ai]) / s[ai], AN[small], a[small]);
    say("\n(the angle opposite the shorter side is always under 90)");
    other = 3 - ai - small;
    a[other] = 180.0f - a[ai] - a[small];
    say("\n%s = 180 - %n - %n = %n", AN[other], a[ai], a[small], a[other]);
    keep(out, s, a);
    return true;
}

/* two sides and an angle opposite one of them: the ambiguous case */
static bool ssa(int ai, int other, float *s, float *a, bool right, struct tri_out *out, struct answer *ans)
{
    int third = 3 - ai - other, k;
    float sy, y1;
    if (right) {   /* ai == 2: the hypotenuse c and one leg */
        if (s[other] >= s[2])
            FAIL("c is the hypotenuse, the longest side, so %s = %n must be less than c = %n.", SN[other], s[other], s[2]);
        ans->rule = "Pythagoras";
        s[third] = sqrtf(s[2] * s[2] - s[other] * s[other]);
        say("Pythagoras: %s = sqrt(c^2 - %s^2) = sqrt(%n - %n) = %n", SN[third], SN[other], s[2] * s[2], s[other] * s[other], s[third]);
        a[other] = asind(s[other] / s[2]);
        say("\nsin %s = %s / c = %n / %n = %n, so %s = %n", AN[other], SN[other], s[other], s[2], s[other] / s[2], AN[other], a[other]);
        a[third] = 90.0f - a[other];
        say("\n%s = 90 - %s = %n", AN[third], AN[other], a[third]);
        keep(out, s, a);
        return true;
    }
    sy = s[other] * sind(a[ai]) / s[ai];
    if (sy > 1.0f + 1e-5f)
        FAIL("No triangle: sin %s would be %n, and a sine can't be more than 1. Side %s = %n is too short to reach side %s.",
             AN[other], sy, SN[ai], s[ai], SN[other]);
    if (sy > 1.0f) sy = 1.0f;
    y1 = asind(sy);
    ans->rule = "Law of Sines (SSA, ambiguous)";
    say("Law of Sines: sin %s = %s sin %s / %s = %n x sin(%n) / %n = %n", AN[other], SN[other], AN[ai], SN[ai], s[other], a[ai], s[ai], sy);
    say("\n%s = arcsin(%n) = %n, or 180 - %n = %n (both have that sine)", AN[other], sy, y1, y1, 180.0f - y1);
    for (k = 0; k < 2; k++) {
        float yv = k == 0 ? y1 : 180.0f - y1;
        float z = 180.0f - a[ai] - yv;
        if (k == 1 && fabsf(sy - 1.0f) < 1e-5f) break;
        if (z <= 0.001f) {
            say("\n%s = %n leaves %s = 180 - %n - %n = %n: no good", AN[other], yv, AN[third], a[ai], yv, z);
            continue;
        }
        a[other] = yv;
        a[third] = z;
        s[third] = s[ai] * sind(z) / sind(a[ai]);
        say("\nTriangle %d: %s = %n, %s = 180 - %n - %n = %n, %s = %s sin %s / sin %s = %n", out->n + 1, AN[other], yv, AN[third],
            a[ai], yv, z, SN[third], SN[ai], AN[third], AN[ai], s[third]);
        keep(out, s, a);
    }
    if (out->n == 0) FAIL("No triangle: the angles would add to more than 180.");
    if (out->n == 2) say("\nTwo triangles fit these values (Left/Right shows each).");
    return true;
}

bool tri_solve(const struct tri_in *in, struct tri_out *out, struct answer *ans)
{
    bool has[6];
    float v[6], s[3], a[3];
    int i, ns = 0, na = 0, given = 0, need = in->right ? 2 : 3, ai, p, q;

    out->n = 0;
    ans->err[0] = 0;
    ans->rule = "";
    say_to(ans->steps, STEPS_MAX);
    memcpy(has, in->has, sizeof(has));
    memcpy(v, in->v, sizeof(v));
    for (i = 0; i < 6; i++) if (has[i]) given++;
    if (in->right) {
        if (has[T_C]) {
            if (fabsf(v[T_C] - 90.0f) > 0.01f) FAIL("In a right triangle C is the 90 degree corner. Leave C empty, or pick Any triangle.");
            given--;
        }
        has[T_C] = true;
        v[T_C] = 90.0f;
    }
    for (i = 0; i < 3; i++) if (has[i] && v[i] <= 0) FAIL("Side %s must be more than 0.", SN[i]);
    for (i = 3; i < 6; i++) if (has[i] && (v[i] <= 0 || v[i] >= 180)) FAIL("Angle %s must be between 0 and 180.", AN[i - 3]);
    if (in->right) for (i = 3; i < 5; i++) if (has[i] && v[i] >= 90) FAIL("Angle %s must be under 90: C is already the right angle.", AN[i - 3]);
    if (given < need) FAIL("Fill in %d values, at least one of them a side (%d so far).", need, given);
    if (given > need) FAIL("Only %d values are needed. Clear one, the rest get worked out.", need);
    for (i = 0; i < 3; i++) {
        ns += has[i];
        na += has[i + 3];
        s[i] = v[i];
        a[i] = v[i + 3];
    }
    if (ns == 0) FAIL("Angles alone don't fix the size: the same shape can be tiny or huge. Give at least one side.");
    if (ns == 3) return sss(s, a, out, ans);
    if (na == 2) return asa(has, s, a, in->right, out, ans);
    ai = has[3] ? 0 : has[4] ? 1 : 2;
    p = (ai + 1) % 3;
    q = (ai + 2) % 3;
    if (has[p] && has[q]) return sas(ai, s, a, in->right, out, ans);
    return ssa(ai, has[p] ? p : q, s, a, in->right, out, ans);
}

void tri_points(const struct tri_sol *t, int x0, int y0, int w, int h, int px[3], int py[3])
{
    float cx = t->s[1] * cosd(t->a[0]), cy = t->s[1] * sind(t->a[0]);
    float minx = cx < 0 ? cx : 0, maxx = cx > t->s[2] ? cx : t->s[2];
    float wd = maxx - minx, ht = cy, sx, sy, sc, left, bottom;
    if (wd < 1e-4f) wd = 1e-4f;
    if (ht < 1e-4f) ht = 1e-4f;
    sx = (float)w / wd;
    sy = (float)h / ht;
    sc = sx < sy ? sx : sy;
    left = (float)x0 + ((float)w - wd * sc) / 2.0f;
    bottom = (float)y0 + (float)h - ((float)h - ht * sc) / 2.0f;
    px[0] = (int)(left + (0 - minx) * sc + 0.5f);
    py[0] = (int)(bottom + 0.5f);
    px[1] = (int)(left + (t->s[2] - minx) * sc + 0.5f);
    py[1] = py[0];
    px[2] = (int)(left + (cx - minx) * sc + 0.5f);
    py[2] = (int)(bottom - cy * sc + 0.5f);
}
