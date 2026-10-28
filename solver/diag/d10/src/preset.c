#include "preset.h"
#include <math.h>
#include "mreal.h"
#include <string.h>
#include "exact.h"
#include "expr.h"
#include "mathx.h"
#include "notebook.h"
#include "solve.h"
#include "triangle.h"

#define PI_D 3.14159265358979323846f

static void cat(char *out, size_t cap, const char *s)
{
    size_t n = strlen(out);
    while (*s && n + 1 < cap) out[n++] = *s++;
    out[n] = 0;
}

static bool needs_paren(const char *s, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        if (s[i] == ' ' || s[i] == '+' || (s[i] == '-' && i > 0)) return true;
    }
    return false;
}

/* editor markup "[a|b]" to a one-line "a/b" */
static void plain(const char *src, char *dst, size_t cap)
{
    size_t n = 0;
    while (*src && n + 8 < cap) {
        const char *bar = *src == '[' ? strchr(src, '|') : NULL;
        const char *end = bar ? strchr(bar, ']') : NULL;
        if (bar && end) {
            size_t nl = (size_t)(bar - src - 1), dl = (size_t)(end - bar - 1);
            bool np = needs_paren(src + 1, nl), dp = needs_paren(bar + 1, dl);
            if (n + nl + dl + 6 >= cap) break;
            if (np) dst[n++] = '(';
            memcpy(dst + n, src + 1, nl);
            n += nl;
            if (np) dst[n++] = ')';
            dst[n++] = '/';
            if (dp) dst[n++] = '(';
            memcpy(dst + n, bar + 1, dl);
            n += dl;
            if (dp) dst[n++] = ')';
            src = end + 1;
        } else {
            dst[n++] = *src++;
        }
    }
    dst[n] = 0;
}

/* a typed value: any expression without x, like 150, 3/4 or pi/6 */
static bool val(const char *s, real *v)
{
    struct mx_prog p;
    real r;
    if (!s[0] || !mx_compile(s, 0, &p)) return false;
    r = mx_run(&p, 0.0f);
    if (!(r == r) || fabs(r) > 1e9f) return false;
    *v = r;
    return true;
}

/* evaluate an expression and append "exact ~ decimal" */
static void add_eval(char *out, size_t cap, const char *expr)
{
    struct mx_eval ev;
    char tmp[130];
    mx_evaluate(expr, &ev);
    if (ev.kind != EK_NUMBER && ev.kind != EK_FRACTION) {
        cat(out, cap, "undefined");
        return;
    }
    plain(ev.exact, tmp, sizeof(tmp));
    cat(out, cap, tmp);
    if (ev.is_exact && !nb_is_whole(ev.exact)) {
        cat(out, cap, "  ~ ");
        cat(out, cap, ev.dec);
    }
}

static bool is_degrees(const char *unit)
{
    return strcmp(unit, "radians") != 0;
}

/* ---------- triangles ---------- */

static bool run_triangle(const char *const *f, char *out, size_t cap)
{
    struct tri_in in;
    int i;
    memset(&in, 0, sizeof(in));
    out[0] = 0;
    for (i = 0; i < 6; i++) {
        real v;
        if (!f[i][0]) continue;
        if (!val(f[i], &v) || v <= 0) {
            cat(out, cap, "Every value must be a positive number.");
            return false;
        }
        if (i < 3) { in.has_s[i] = true; in.s[i] = v; }
        else { in.has_a[i - 3] = true; in.a[i - 3] = v; }
    }
    return tri_work(&in, out, cap);
}

static bool run_right(const char *const *f, char *out, size_t cap)
{
    struct tri_in in;
    int i, given = 0, sides = 0;
    memset(&in, 0, sizeof(in));
    out[0] = 0;
    for (i = 0; i < 5; i++) {
        real v;
        if (!f[i][0]) continue;
        if (!val(f[i], &v) || v <= 0) {
            cat(out, cap, "Every value must be a positive number.");
            return false;
        }
        given++;
        if (i < 3) { in.has_s[i] = true; in.s[i] = v; sides++; }
        else { in.has_a[i - 3] = true; in.a[i - 3] = v; }
    }
    if (given != 2 || sides == 0) {
        cat(out, cap, "Fill in exactly two values, at least one a side. The right angle is C = 90.");
        return false;
    }
    in.has_a[2] = true;
    in.a[2] = 90.0f;
    return tri_work(&in, out, cap);
}

/* ---------- trig equation a f(bx + c) = d ---------- */

static bool run_equation(const char *const *f, char *out, size_t cap)
{
    static struct mx_solution sol;
    static struct hentry h;
    char lhs[90], rhs[24], tmp[LINE_LEN * 2];
    bool saved = mx_degrees;
    const char *a = f[1][0] ? f[1] : "1", *b = f[2][0] ? f[2] : "1", *c = f[3][0] ? f[3] : "0";
    real v;
    int i;

    out[0] = 0;
    if (!f[4][0]) {
        cat(out, cap, "Type the number on the right side (d).");
        return false;
    }
    if (!val(a, &v) || !val(b, &v) || !val(c, &v) || !val(f[4], &v)) {
        cat(out, cap, "a, b, c and d must be numbers, like 2, 3/4 or pi/6.");
        return false;
    }
    strcpy(lhs, "(");
    strcat(lhs, a);
    strcat(lhs, ")*");
    strcat(lhs, f[0]);
    strcat(lhs, "((");
    strcat(lhs, b);
    strcat(lhs, ")*x+(");
    strcat(lhs, c);
    strcat(lhs, "))");
    strcpy(rhs, "(");
    strcat(rhs, f[4]);
    strcat(rhs, ")");

    mx_degrees = is_degrees(f[5]);
    mx_solve(lhs, rhs, &sol);
    memset(&h, 0, sizeof(h));
    nb_build_solution(&h, lhs, rhs, &sol);
    mx_degrees = saved;

    cat(out, cap, a);
    cat(out, cap, " ");
    cat(out, cap, f[0]);
    cat(out, cap, "(");
    cat(out, cap, b);
    cat(out, cap, "x + ");
    cat(out, cap, c);
    cat(out, cap, ") = ");
    cat(out, cap, f[4]);
    for (i = 0; i < h.nlines; i++) {
        plain(h.text[i], tmp, sizeof(tmp));
        cat(out, cap, "\n");
        cat(out, cap, tmp);
    }
    return sol.kind != SK_ERROR;
}

/* ---------- angle values ---------- */

static bool run_angle(const char *const *f, char *out, size_t cap)
{
    static const char *const fn[6] = {"sin", "cos", "tan", "csc", "sec", "cot"};
    bool deg = is_degrees(f[1]), saved = mx_degrees;
    real v, td;
    char expr[40], tmp[48];
    int i;

    out[0] = 0;
    if (!val(f[0], &v)) {
        cat(out, cap, "Type an angle, like 150 or pi/6.");
        return false;
    }
    cat(out, cap, "Angle = ");
    cat(out, cap, f[0]);
    cat(out, cap, deg ? " degrees" : " radians");
    mx_degrees = deg;
    for (i = 0; i < 6; i++) {
        strcpy(expr, fn[i]);
        strcat(expr, "(");
        strcat(expr, f[0]);
        strcat(expr, ")");
        cat(out, cap, "\n");
        cat(out, cap, fn[i]);
        cat(out, cap, " = ");
        add_eval(out, cap, expr);
    }
    mx_degrees = saved;

    td = deg ? v : v * 180.0f / PI_D;
    td = fmod(td, 360.0f);
    if (td < 0) td += 360.0f;
    if (fabs(td - floor(td / 90.0f + 0.5f) * 90.0f) < 1e-3f) {
        cat(out, cap, "\nThis angle is on an axis, so it has no reference angle.");
    } else {
        real ref = td < 90.0f ? td : td < 180.0f ? 180.0f - td : td < 270.0f ? td - 180.0f : 360.0f - td;
        cat(out, cap, "\nQuadrant ");
        cat(out, cap, td < 90.0f ? "I" : td < 180.0f ? "II" : td < 270.0f ? "III" : "IV");
        cat(out, cap, ", reference angle = ");
        if (deg) {
            mx_decimal(ref, 6, tmp, sizeof(tmp));
            cat(out, cap, tmp);
            cat(out, cap, " degrees");
        } else {
            char ex[48];
            mx_exact(ref * PI_D / 180.0f, true, ex, sizeof(ex));
            plain(ex, tmp, sizeof(tmp));
            cat(out, cap, tmp);
            cat(out, cap, " radians");
        }
    }
    return true;
}

/* ---------- converting ---------- */

static bool run_convert(const char *const *f, char *out, size_t cap)
{
    bool to_rad = strcmp(f[1], "rad > deg") != 0;
    char expr[60];
    real v;

    out[0] = 0;
    if (!val(f[0], &v)) {
        cat(out, cap, "Type a number, like 150 or pi/6.");
        return false;
    }
    strcpy(expr, "(");
    strcat(expr, f[0]);
    strcat(expr, to_rad ? ")*pi/180" : ")*180/pi");
    cat(out, cap, to_rad ? "radians = degrees x pi / 180\n" : "degrees = radians x 180 / pi\n");
    cat(out, cap, f[0]);
    cat(out, cap, to_rad ? " degrees = " : " radians = ");
    add_eval(out, cap, expr);
    cat(out, cap, to_rad ? " radians" : " degrees");
    return true;
}

/* ---------- arc length and sector ---------- */

static bool run_arc(const char *const *f, char *out, size_t cap)
{
    bool deg = is_degrees(f[2]);
    char th[40], expr[100];
    real r, t;

    out[0] = 0;
    if (!val(f[0], &r) || r <= 0 || !val(f[1], &t) || t <= 0) {
        cat(out, cap, "Radius and angle must be positive numbers.");
        return false;
    }
    strcpy(th, "(");
    strcat(th, f[1]);
    strcat(th, deg ? ")*pi/180" : ")");
    cat(out, cap, "theta in radians = ");
    add_eval(out, cap, th);
    strcpy(expr, "(");
    strcat(expr, f[0]);
    strcat(expr, ")*(");
    strcat(expr, th);
    strcat(expr, ")");
    cat(out, cap, "\narc length s = r x theta = ");
    add_eval(out, cap, expr);
    strcpy(expr, "(");
    strcat(expr, f[0]);
    strcat(expr, ")^2*(");
    strcat(expr, th);
    strcat(expr, ")/2");
    cat(out, cap, "\nsector area = 1/2 r^2 theta = ");
    add_eval(out, cap, expr);
    return true;
}

/* ---------- the menu ---------- */

static const char *const trig_fns[] = {"sin", "cos", "tan", "csc", "sec", "cot"};
static const char *const units_dr[] = {"degrees", "radians"};
static const char *const convert_dirs[] = {"deg > rad", "rad > deg"};

const struct preset presets[PRESET_COUNT] = {
    {"Triangle", "Fill any 3 values, at least 1 a side", 6,
     {{"side a", "units", NULL, 0}, {"side b", "units", NULL, 0}, {"side c", "units", NULL, 0},
      {"angle A", "degrees", NULL, 0}, {"angle B", "degrees", NULL, 0}, {"angle C", "degrees", NULL, 0}},
     run_triangle},
    {"Right triangle", "Right angle at C. Fill any 2, 1 a side", 5,
     {{"side a", "units", NULL, 0}, {"side b", "units", NULL, 0}, {"hypotenuse c", "units", NULL, 0},
      {"angle A", "degrees", NULL, 0}, {"angle B", "degrees", NULL, 0}},
     run_right},
    {"Trig equation", "Solves  a f(bx + c) = d", 6,
     {{"function", "", trig_fns, 6}, {"a", "blank=1", NULL, 0}, {"b", "blank=1", NULL, 0},
      {"c", "blank=0", NULL, 0}, {"d (answer)", "", NULL, 0}, {"angles in", "", units_dr, 2}},
     run_equation},
    {"Angle values", "sin cos tan csc sec cot of an angle", 2,
     {{"angle", "", NULL, 0}, {"angle in", "", units_dr, 2}},
     run_angle},
    {"Degrees / radians", "Convert an angle", 2,
     {{"angle", "", NULL, 0}, {"convert", "", convert_dirs, 2}},
     run_convert},
    {"Arc and sector", "Arc length and sector area", 3,
     {{"radius r", "units", NULL, 0}, {"angle", "", NULL, 0}, {"angle in", "", units_dr, 2}},
     run_arc},
};
