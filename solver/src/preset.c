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
    for (i = 0; i < 8; i++) {
        real v;
        if (!f[i][0]) continue;
        if (!val(f[i], &v) || v <= 0) {
            cat(out, cap, "Every value must be a positive number.");
            return false;
        }
        if (i < 3) { in.has_s[i] = true; in.s[i] = v; }
        else if (i < 6) { in.has_a[i - 3] = true; in.a[i - 3] = v; }
        else if (i == 6) { in.has_area = true; in.area = v; }
        else { in.has_perim = true; in.perim = v; }
    }
    return tri_work(&in, out, cap);
}

static bool run_right(const char *const *f, char *out, size_t cap)
{
    struct tri_in in;
    int i, given = 0;
    memset(&in, 0, sizeof(in));
    out[0] = 0;
    for (i = 0; i < 7; i++) {
        real v;
        if (!f[i][0]) continue;
        if (!val(f[i], &v) || v <= 0) {
            cat(out, cap, "Every value must be a positive number.");
            return false;
        }
        given++;
        if (i < 3) { in.has_s[i] = true; in.s[i] = v; }
        else if (i < 5) { in.has_a[i - 3] = true; in.a[i - 3] = v; }
        else if (i == 5) { in.has_area = true; in.area = v; }
        else { in.has_perim = true; in.perim = v; }
    }
    if (given != 2 || (!in.has_s[0] && !in.has_s[1] && !in.has_s[2] && !in.has_area && !in.has_perim)) {
        cat(out, cap, "Fill in exactly two values, at least one a side, the area or the perimeter. The right angle is C = 90.");
        return false;
    }
    in.has_a[2] = true;
    in.a[2] = 90.0f;
    return tri_work(&in, out, cap);
}

/* ---------- solving an equation and writing the steps ---------- */

/* appends the solver's answer lines (one per line) for lhs = rhs */
static bool solve_text(const char *lhs, const char *rhs, bool deg, char *out, size_t cap)
{
    static struct mx_solution sol;
    static struct hentry h;
    char tmp[LINE_LEN * 2];
    bool saved = mx_degrees;
    int i;

    mx_degrees = deg;
    mx_solve(lhs, rhs, &sol);
    memset(&h, 0, sizeof(h));
    nb_build_solution(&h, lhs, rhs, &sol);
    mx_degrees = saved;
    for (i = 0; i < h.nlines; i++) {
        plain(h.text[i], tmp, sizeof(tmp));
        cat(out, cap, "\n");
        cat(out, cap, tmp);
    }
    return sol.kind != SK_ERROR;
}

/* ---------- trig equation a f(bx + c) = d ---------- */

static bool run_equation(const char *const *f, char *out, size_t cap)
{
    char lhs[90], rhs[24];
    const char *a = f[1][0] ? f[1] : "1", *b = f[2][0] ? f[2] : "1", *c = f[3][0] ? f[3] : "0";
    real v;

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

    cat(out, cap, a);
    cat(out, cap, " ");
    cat(out, cap, f[0]);
    cat(out, cap, "(");
    cat(out, cap, b);
    cat(out, cap, "x + ");
    cat(out, cap, c);
    cat(out, cap, ") = ");
    cat(out, cap, f[4]);
    return solve_text(lhs, rhs, is_degrees(f[5]), out, cap);
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

/* ---------- the other trig values, from one of them and the quadrant ---------- */

/* quadrants 1 to 4 where the function has the sign of v */
static bool sign_fits(const char *fn, real v, int quad)
{
    bool pos = v > 0;
    if (!strcmp(fn, "sin") || !strcmp(fn, "csc")) return pos == (quad == 1 || quad == 2);
    if (!strcmp(fn, "cos") || !strcmp(fn, "sec")) return pos == (quad == 1 || quad == 4);
    return pos == (quad == 1 || quad == 3);
}

static bool run_other(const char *const *f, char *out, size_t cap)
{
    static const char *const names[6] = {"sin", "cos", "tan", "csc", "sec", "cot"};
    char V[40], sm[100], cm[100], s_[110], c_[110], e[240];
    int quad = (int)strlen(f[2]);
    real v;
    int i;

    out[0] = 0;
    if (!val(f[1], &v) || v == 0) {
        cat(out, cap, "Type the value of that function, not 0, like 3/5 or -2.");
        return false;
    }
    if ((!strcmp(f[0], "sin") || !strcmp(f[0], "cos")) && fabs(v) > 1.0f) {
        cat(out, cap, "sin and cos are never bigger than 1 or smaller than -1.");
        return false;
    }
    if ((!strcmp(f[0], "csc") || !strcmp(f[0], "sec")) && fabs(v) < 1.0f) {
        cat(out, cap, "csc and sec are never between -1 and 1.");
        return false;
    }
    if (!sign_fits(f[0], v, quad)) {
        cat(out, cap, "That sign does not happen in quadrant ");
        cat(out, cap, f[2]);
        cat(out, cap, ". Check the quadrant or the sign.");
        return false;
    }
    strcpy(V, "(");
    strcat(V, f[1]);
    strcat(V, ")");
    /* sizes of sin and cos, always positive; the quadrant gives their signs */
    if (!strcmp(f[0], "sin") || !strcmp(f[0], "csc")) {
        const char *s0 = !strcmp(f[0], "sin") ? V : NULL;
        char base[60];
        strcpy(base, s0 ? V : "");
        if (!s0) { strcpy(base, "(1/"); strcat(base, V); strcat(base, ")"); }
        strcpy(sm, "abs("); strcat(sm, base); strcat(sm, ")");
        strcpy(cm, "sqrt(1-"); strcat(cm, base); strcat(cm, "^2)");
    } else if (!strcmp(f[0], "cos") || !strcmp(f[0], "sec")) {
        char base[60];
        if (!strcmp(f[0], "cos")) strcpy(base, V);
        else { strcpy(base, "(1/"); strcat(base, V); strcat(base, ")"); }
        strcpy(cm, "abs("); strcat(cm, base); strcat(cm, ")");
        strcpy(sm, "sqrt(1-"); strcat(sm, base); strcat(sm, "^2)");
    } else {
        char t[60];
        if (!strcmp(f[0], "tan")) strcpy(t, V);
        else { strcpy(t, "(1/"); strcat(t, V); strcat(t, ")"); }
        strcpy(sm, "abs("); strcat(sm, t); strcat(sm, ")/sqrt(1+"); strcat(sm, t); strcat(sm, "^2)");
        strcpy(cm, "1/sqrt(1+"); strcat(cm, t); strcat(cm, "^2)");
    }
    strcpy(s_, quad >= 3 ? "-(" : "(");
    strcat(s_, sm);
    strcat(s_, ")");
    strcpy(c_, quad == 2 || quad == 3 ? "-(" : "(");
    strcat(c_, cm);
    strcat(c_, ")");

    cat(out, cap, f[0]);
    cat(out, cap, " = ");
    cat(out, cap, f[1]);
    cat(out, cap, ", angle in quadrant ");
    cat(out, cap, f[2]);
    cat(out, cap, "\nsin^2 + cos^2 = 1 gives the size of the other one;\nthe quadrant gives its sign.");
    for (i = 0; i < 6; i++) {
        if (i == 0) strcpy(e, s_);
        else if (i == 1) strcpy(e, c_);
        else if (i == 2) { strcpy(e, s_); strcat(e, "/"); strcat(e, c_); }
        else if (i == 3) { strcpy(e, "1/"); strcat(e, s_); }
        else if (i == 4) { strcpy(e, "1/"); strcat(e, c_); }
        else { strcpy(e, c_); strcat(e, "/"); strcat(e, s_); }
        cat(out, cap, "\n");
        cat(out, cap, names[i]);
        cat(out, cap, " = ");
        add_eval(out, cap, e);
    }
    return true;
}

/* ---------- polar and rectangular ---------- */

static bool run_polar(const char *const *f, char *out, size_t cap)
{
    bool deg = is_degrees(f[3]), saved = mx_degrees, to_rect = strcmp(f[0], "rect > polar") != 0;
    char e[100], tmp[32];
    real p, q;

    out[0] = 0;
    if (!val(f[1], &p) || !val(f[2], &q)) {
        cat(out, cap, "Fill in both numbers.");
        return false;
    }
    mx_degrees = deg;
    if (to_rect) {
        strcpy(e, "(");
        strcat(e, f[1]);
        strcat(e, ")*cos(");
        strcat(e, f[2]);
        strcat(e, ")");
        cat(out, cap, "x = r cos(theta) = ");
        add_eval(out, cap, e);
        strcpy(e, "(");
        strcat(e, f[1]);
        strcat(e, ")*sin(");
        strcat(e, f[2]);
        strcat(e, ")");
        cat(out, cap, "\ny = r sin(theta) = ");
        add_eval(out, cap, e);
    } else {
        real ang;
        strcpy(e, "sqrt((");
        strcat(e, f[1]);
        strcat(e, ")^2+(");
        strcat(e, f[2]);
        strcat(e, ")^2)");
        if (p == 0 && q == 0) {
            mx_degrees = saved;
            cat(out, cap, "r = 0, and the angle is undefined at the origin.");
            return true;
        }
        cat(out, cap, "r = sqrt(x^2 + y^2) = ");
        add_eval(out, cap, e);
        ang = atan2f(q, p);
        if (ang < 0) ang += 2.0f * PI_D;
        cat(out, cap, "\ntheta = angle of (x, y), from the x-axis, quadrant ");
        cat(out, cap, p > 0 && q >= 0 ? "I" : p <= 0 && q > 0 ? "II" : p < 0 && q <= 0 ? "III" : "IV");
        cat(out, cap, "\ntheta = ");
        if (deg) {
            mx_decimal(ang * 180.0f / PI_D, 6, tmp, sizeof(tmp));
            cat(out, cap, tmp);
            cat(out, cap, " degrees");
        } else {
            char ex[48];
            mx_exact(ang, true, ex, sizeof(ex));
            plain(ex, tmp, sizeof(tmp));
            cat(out, cap, tmp);
            cat(out, cap, " radians");
        }
    }
    mx_degrees = saved;
    return true;
}

/* ---------- y = a f(bx + c) + d: amplitude, period, shifts ---------- */

static bool run_wave(const char *const *f, char *out, size_t cap)
{
    bool deg = is_degrees(f[5]), saved = mx_degrees;
    bool tanlike = !strcmp(f[0], "tan") || !strcmp(f[0], "cot");
    bool recip = !strcmp(f[0], "csc") || !strcmp(f[0], "sec");
    const char *a = f[1][0] ? f[1] : "1", *b = f[2][0] ? f[2] : "1", *c = f[3][0] ? f[3] : "0", *d = f[4][0] ? f[4] : "0";
    char e[120];
    real av, bv, cv, dv;

    out[0] = 0;
    if (!val(a, &av) || !val(b, &bv) || !val(c, &cv) || !val(d, &dv) || bv == 0 || av == 0) {
        cat(out, cap, "a and b must be numbers that are not 0. c and d can be blank.");
        return false;
    }
    mx_degrees = deg;
    cat(out, cap, "y = ");
    cat(out, cap, a);
    cat(out, cap, " ");
    cat(out, cap, f[0]);
    cat(out, cap, "(");
    cat(out, cap, b);
    cat(out, cap, "x + ");
    cat(out, cap, c);
    cat(out, cap, ") + ");
    cat(out, cap, d);
    if (tanlike || recip) {
        cat(out, cap, "\nNo amplitude: this graph goes off to infinity.");
    } else {
        strcpy(e, "abs(");
        strcat(e, a);
        strcat(e, ")");
        cat(out, cap, "\namplitude = |a| = ");
        add_eval(out, cap, e);
    }
    strcpy(e, tanlike ? (deg ? "180" : "pi") : (deg ? "360" : "2*pi"));
    strcat(e, "/abs(");
    strcat(e, b);
    strcat(e, ")");
    cat(out, cap, tanlike ? (deg ? "\nperiod = 180 / |b| = " : "\nperiod = pi / |b| = ") : (deg ? "\nperiod = 360 / |b| = " : "\nperiod = 2pi / |b| = "));
    add_eval(out, cap, e);
    strcpy(e, "-(");
    strcat(e, c);
    strcat(e, ")/(");
    strcat(e, b);
    strcat(e, ")");
    cat(out, cap, "\nphase shift = -c / b = ");
    add_eval(out, cap, e);
    cat(out, cap, cv / bv < 0 ? "  (to the right)" : cv == 0 ? "" : "  (to the left)");
    cat(out, cap, "\nvertical shift = d = ");
    add_eval(out, cap, d);
    cat(out, cap, "\nmidline  y = ");
    add_eval(out, cap, d);
    if (!tanlike && !recip) {
        strcpy(e, "(");
        strcat(e, d);
        strcat(e, ")+abs(");
        strcat(e, a);
        strcat(e, ")");
        cat(out, cap, "\nmaximum = d + |a| = ");
        add_eval(out, cap, e);
        strcpy(e, "(");
        strcat(e, d);
        strcat(e, ")-abs(");
        strcat(e, a);
        strcat(e, ")");
        cat(out, cap, "\nminimum = d - |a| = ");
        add_eval(out, cap, e);
    }
    if (av < 0) cat(out, cap, "\na < 0, so the graph is flipped upside down.");
    mx_degrees = saved;
    return true;
}

/* ---------- quadratic ---------- */

static bool run_quadratic(const char *const *f, char *out, size_t cap)
{
    char lhs[70], e[120];
    real a, b, c;

    out[0] = 0;
    if (!val(f[0], &a) || !val(f[1], &b) || !val(f[2], &c) || a == 0) {
        cat(out, cap, "a, b and c must be numbers, and a can not be 0.");
        return false;
    }
    strcpy(lhs, "(");
    strcat(lhs, f[0]);
    strcat(lhs, ")x^2+(");
    strcat(lhs, f[1]);
    strcat(lhs, ")x+(");
    strcat(lhs, f[2]);
    strcat(lhs, ")");
    cat(out, cap, f[0]);
    cat(out, cap, "x^2 + ");
    cat(out, cap, f[1]);
    cat(out, cap, "x + ");
    cat(out, cap, f[2]);
    cat(out, cap, " = 0");
    solve_text(lhs, "0", true, out, cap);
    strcpy(e, "(");
    strcat(e, f[1]);
    strcat(e, ")^2-4*(");
    strcat(e, f[0]);
    strcat(e, ")*(");
    strcat(e, f[2]);
    strcat(e, ")");
    cat(out, cap, "\ndiscriminant b^2 - 4ac = ");
    add_eval(out, cap, e);
    strcpy(e, "-(");
    strcat(e, f[1]);
    strcat(e, ")/(2*(");
    strcat(e, f[0]);
    strcat(e, "))");
    cat(out, cap, "\nvertex x = -b / 2a = ");
    add_eval(out, cap, e);
    strcpy(e, "(");
    strcat(e, f[2]);
    strcat(e, ")-(");
    strcat(e, f[1]);
    strcat(e, ")^2/(4*(");
    strcat(e, f[0]);
    strcat(e, "))");
    cat(out, cap, "\nvertex y = ");
    add_eval(out, cap, e);
    return true;
}

/* ---------- the menu ---------- */

static const char *const trig_fns[] = {"sin", "cos", "tan", "csc", "sec", "cot"};
static const char *const units_dr[] = {"degrees", "radians"};
static const char *const convert_dirs[] = {"deg > rad", "rad > deg"};
static const char *const quadrants[] = {"I", "II", "III", "IV"};
static const char *const polar_dirs[] = {"polar > rect", "rect > polar"};

const struct preset presets[PRESET_COUNT] = {
    {"Triangle", "Fill any 3 values (area and perimeter count)", 8,
     {{"side a", "units", NULL, 0}, {"side b", "units", NULL, 0}, {"side c", "units", NULL, 0},
      {"angle A", "degrees", NULL, 0}, {"angle B", "degrees", NULL, 0}, {"angle C", "degrees", NULL, 0},
      {"area", "sq units", NULL, 0}, {"perimeter", "units", NULL, 0}},
     run_triangle},
    {"Right triangle", "Right angle at C. Fill any 2 values", 7,
     {{"side a", "units", NULL, 0}, {"side b", "units", NULL, 0}, {"hypotenuse c", "units", NULL, 0},
      {"angle A", "degrees", NULL, 0}, {"angle B", "degrees", NULL, 0},
      {"area", "sq units", NULL, 0}, {"perimeter", "units", NULL, 0}},
     run_right},
    {"Trig equation", "Solves  a f(bx + c) = d", 6,
     {{"function", "", trig_fns, 6}, {"a", "blank=1", NULL, 0}, {"b", "blank=1", NULL, 0},
      {"c", "blank=0", NULL, 0}, {"d (answer)", "", NULL, 0}, {"angles in", "", units_dr, 2}},
     run_equation},
    {"Angle values", "sin cos tan csc sec cot of an angle", 2,
     {{"angle", "", NULL, 0}, {"angle in", "", units_dr, 2}},
     run_angle},
    {"Other values", "Know one value and the quadrant", 3,
     {{"you know", "", trig_fns, 6}, {"its value", "", NULL, 0}, {"quadrant", "", quadrants, 4}},
     run_other},
    {"Degrees/radians", "Convert an angle", 2,
     {{"angle", "", NULL, 0}, {"convert", "", convert_dirs, 2}},
     run_convert},
    {"Arc and sector", "Arc length and sector area", 3,
     {{"radius r", "units", NULL, 0}, {"angle", "", NULL, 0}, {"angle in", "", units_dr, 2}},
     run_arc},
    {"Polar/rect", "(r, theta) and (x, y)", 4,
     {{"convert", "", polar_dirs, 2}, {"r or x", "", NULL, 0}, {"theta or y", "", NULL, 0}, {"theta in", "", units_dr, 2}},
     run_polar},
    {"Amplitude/period", "y = a f(bx + c) + d", 6,
     {{"function", "", trig_fns, 6}, {"a", "", NULL, 0}, {"b", "", NULL, 0}, {"c", "blank=0", NULL, 0},
      {"d", "blank=0", NULL, 0}, {"angles in", "", units_dr, 2}},
     run_wave},
    {"Quadratic", "a x^2 + b x + c = 0", 3,
     {{"a", "", NULL, 0}, {"b", "", NULL, 0}, {"c", "", NULL, 0}},
     run_quadratic},
};
