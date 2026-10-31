#include "probs.h"
#include <math.h>
#include <string.h>
#include "num.h"

#define PI_F 3.14159265f
#define FAIL(...) do { say_to(ans->err, ERR_MAX); say(__VA_ARGS__); return false; } while (0)

static void start(struct answer *ans, const char *rule)
{
    ans->n = 0;
    ans->err[0] = 0;
    ans->rule = rule;
    say_to(ans->steps, STEPS_MAX);
}

/* one answer line, built with say() */
static char *line(struct answer *ans)
{
    char *l = ans->line[ans->n < ANS_LINES ? ans->n++ : ANS_LINES - 1];
    say_to(l, ANS_COLS);
    return l;
}

/* say() goes back to the steps after a line was written */
static void steps(struct answer *ans)
{
    int len = (int)strlen(ans->steps);
    say_to(ans->steps + len, STEPS_MAX - len);
}

/* ---------------------------------------------------------------- quadratic */

bool quad_solve(float a, float b, float c, struct quad_out *out, struct answer *ans)
{
    float disc, root;
    start(ans, "Quadratic formula");
    if (a == 0) {
        if (b == 0) FAIL("With a = 0 and b = 0 there is no x left: %n = 0 is just true or false.", c);
        out->nreal = 1;
        out->x1 = out->x2 = -c / b;
        out->vx = out->vy = 0;
        ans->rule = "Linear (a is 0)";
        line(ans); say("x = %n", out->x1);
        steps(ans);
        say("a = 0, so this is the straight line %n x + %n = 0\n", b, c);
        say("x = -c / b = %n / %n = %n", -c, b, out->x1);
        return true;
    }
    disc = b * b - 4.0f * a * c;
    out->vx = -b / (2.0f * a);
    out->vy = a * out->vx * out->vx + b * out->vx + c;
    say("x = (-b +/- sqrt(b^2 - 4ac)) / 2a\n");
    say("b^2 - 4ac = %n - 4(%n)(%n) = %n\n", b * b, a, c, disc);
    if (disc < 0) {
        out->nreal = 0;
        out->x1 = out->vx;
        out->x2 = sqrtf(-disc) / (2.0f * a);
        if (out->x2 < 0) out->x2 = -out->x2;
        line(ans); say("No real roots");
        line(ans); say("x = %n + %ni", out->x1, out->x2);
        line(ans); say("x = %n - %ni", out->x1, out->x2);
        steps(ans);
        say("It is negative, so the square root is not a real number: the parabola never crosses the x axis.\n");
        say("Complex roots: x = %n / %n +/- sqrt(%n) / %n i = %n +/- %n i", -b, 2.0f * a, -disc, 2.0f * a, out->x1, out->x2);
    } else {
        root = sqrtf(disc);
        out->nreal = disc == 0 ? 1 : 2;
        out->x1 = (-b - root) / (2.0f * a);
        out->x2 = (-b + root) / (2.0f * a);
        if (out->x1 > out->x2) {
            float t = out->x1;
            out->x1 = out->x2;
            out->x2 = t;
        }
        if (out->nreal == 1) {
            line(ans); say("x = %n (double)", out->x1);
        } else {
            line(ans); say("x = %n", out->x1);
            line(ans); say("x = %n", out->x2);
        }
        steps(ans);
        say("sqrt(%n) = %n\n", disc, root);
        say("x = (%n +/- %n) / %n\n", -b, root, 2.0f * a);
        if (out->nreal == 1) say("The root is 0, so both answers are the same: x = %n (the parabola just touches the axis)", out->x1);
        else say("x = %n or x = %n", out->x1, out->x2);
    }
    line(ans); say("vertex x = %n", out->vx);
    line(ans); say("vertex y = %n", out->vy);
    line(ans); say("opens %s", a > 0 ? "up" : "down");
    steps(ans);
    say("\nVertex: x = -b/2a = %n, y = %n", out->vx, out->vy);
    return true;
}

/* ---------------------------------------------------------------- linear */

bool lin_solve(float a, float b, float c, float d, int *kind, float *x, struct answer *ans)
{
    start(ans, "Collect x on one side");
    say("%n x + %n = %n x + %n\n", a, b, c, d);
    if (a == c) {
        if (b == d) {
            *kind = 2;
            line(ans); say("Every x works");
            steps(ans);
            say("Both sides are the same, so any x makes them equal.");
        } else {
            *kind = 0;
            line(ans); say("No solution");
            steps(ans);
            say("The x terms cancel (%n x on each side) and %n = %n is false, so no x works.", a, b, d);
        }
        return true;
    }
    *kind = 1;
    *x = (d - b) / (a - c);
    line(ans); say("x = %n", *x);
    line(ans); say("check: %n = %n", a * *x + b, c * *x + d);
    steps(ans);
    say("Take %n x from both sides: %n x + %n = %n\n", c, a - c, b, d);
    say("Take %n from both sides: %n x = %n\n", b, a - c, d - b);
    say("Divide by %n: x = %n / %n = %n\n", a - c, d - b, a - c, *x);
    say("Check: left side %n, right side %n", a * *x + b, c * *x + d);
    return true;
}

/* ---------------------------------------------------------------- unit circle */

static const struct { float v; const char *name; } EXACT[] = {
    {0.0f, "0"}, {0.5f, "1/2"}, {0.70710678f, "sqrt2/2"}, {0.86602540f, "sqrt3/2"}, {1.0f, "1"},
    {0.57735027f, "sqrt3/3"}, {1.7320508f, "sqrt3"},
};

const char *exact_trig(float v)
{
    static char buf[12];
    unsigned i;
    float av = v < 0 ? -v : v;
    for (i = 0; i < sizeof(EXACT) / sizeof(EXACT[0]); i++) {
        if (fabsf(av - EXACT[i].v) < 2e-4f) {
            buf[0] = '-';
            strcpy(buf + 1, EXACT[i].name);
            return (v < 0 && av > 1e-4f) ? buf : buf + 1;
        }
    }
    return NULL;
}

bool radians_text(float deg, char *out)
{
    int d, n;
    for (d = 1; d <= 12; d++) {
        float exact = deg * (float)d / 180.0f;
        n = (int)floorf(exact + 0.5f);
        if (fabsf(exact - (float)n) < 1e-3f) {
            char num[NUM_CHARS];
            say_to(out, NUM_CHARS);
            if (n == 0) {
                say("0");
            } else {
                if (n < 0) say("-");
                if (n != 1 && n != -1) {
                    fmt_num((float)(n < 0 ? -n : n), 0, num);
                    say("%s", num);
                }
                say("pi");
                if (d > 1) say("/%d", d);
            }
            return true;
        }
    }
    return false;
}

static void trig_line(struct answer *ans, const char *fn, float v)
{
    const char *ex = v != v ? "undefined" : exact_trig(v);
    line(ans);
    if (ex) say("%s = %s", fn, ex);
    else say("%s = %n", fn, v);
}

bool trig_values(float deg, struct trig_out *out, struct answer *ans)
{
    float d = deg, cs;
    char rad[NUM_CHARS];
    const char *sign;
    bool has_rad;
    start(ans, "Unit circle");
    while (d >= 360.0f) d -= 360.0f;
    while (d < 0) d += 360.0f;
    out->deg = d;
    out->s = sinf(d * (PI_F / 180.0f));
    out->c = cosf(d * (PI_F / 180.0f));
    if (fabsf(out->s) < 1e-6f) out->s = 0;
    if (fabsf(out->c) < 1e-6f) out->c = 0;
    out->t = out->c == 0 ? NAN : out->s / out->c;
    cs = fmodf(d, 90.0f);
    if (cs < 1e-4f || 90.0f - cs < 1e-4f) {
        out->quadrant = 0;
        out->ref = 0;
    } else {
        out->quadrant = (int)(d / 90.0f) + 1;
        out->ref = out->quadrant == 1 ? d : out->quadrant == 2 ? 180.0f - d : out->quadrant == 3 ? d - 180.0f : 360.0f - d;
    }
    if (d != deg) say("%n is the same spot as %n (whole turns of 360 taken away)\n", deg, d);
    if (out->quadrant) {
        sign = out->quadrant == 1 ? "all +" : out->quadrant == 2 ? "sin +, cos -, tan -" : out->quadrant == 3 ? "tan +, sin -, cos -" : "cos +, sin -, tan -";
        say("Quadrant %d: %s\n", out->quadrant, sign);
        say("Reference angle (to the x axis) = %n\n", out->ref);
        say("sin %n = %ssin %n, cos %n = %scos %n", d, out->s < 0 ? "-" : "", out->ref, d, out->c < 0 ? "-" : "", out->ref);
    } else {
        say("%n is on an axis: the point is (%n, %n)\n", d, out->c, out->s);
        say("sin = the y of the point, cos = the x, tan = sin/cos%s", out->c == 0 ? " (dividing by 0: undefined)" : "");
    }
    trig_line(ans, "sin", out->s);
    trig_line(ans, "cos", out->c);
    trig_line(ans, "tan", out->t);
    has_rad = radians_text(d, rad);   /* before line(): it writes into rad with say() */
    line(ans);
    if (has_rad) say("%n deg = %s rad", d, rad);
    else say("= %n rad", d * (PI_F / 180.0f));
    if (out->quadrant) {
        line(ans); say("quadrant %d", out->quadrant);
        line(ans); say("reference %n", out->ref);
    }
    return true;
}

/* ---------------------------------------------------------------- stats */

bool stats_solve(const float *x, int n, struct stats_out *out, struct answer *ans)
{
    int i, j, best = 1, run;
    float sq = 0;
    start(ans, "Mean, median, mode, spread");
    if (n < 1) FAIL("Type at least one number (two or more for the spread).");
    out->n = n;
    out->sum = 0;
    for (i = 0; i < n; i++) {
        out->sorted[i] = x[i];
        out->sum += x[i];
    }
    for (i = 1; i < n; i++) {   /* insertion sort: n is at most 10 */
        float v = out->sorted[i];
        for (j = i; j > 0 && out->sorted[j - 1] > v; j--) out->sorted[j] = out->sorted[j - 1];
        out->sorted[j] = v;
    }
    out->lo = out->sorted[0];
    out->hi = out->sorted[n - 1];
    out->mean = out->sum / (float)n;
    out->median = n % 2 ? out->sorted[n / 2] : (out->sorted[n / 2 - 1] + out->sorted[n / 2]) / 2.0f;
    out->nmodes = 0;
    for (i = 0; i < n; i += run) {
        run = 1;
        while (i + run < n && out->sorted[i + run] == out->sorted[i]) run++;
        if (run > best) {
            best = run;
            out->nmodes = 0;
        }
        if (run == best && best > 1 && out->nmodes < 2) out->mode[out->nmodes++] = out->sorted[i];
    }
    for (i = 0; i < n; i++) sq += (x[i] - out->mean) * (x[i] - out->mean);
    out->psd = sqrtf(sq / (float)n);
    out->sd = n > 1 ? sqrtf(sq / (float)(n - 1)) : 0;

    line(ans); say("n = %d  sum = %n", n, out->sum);
    line(ans); say("mean = %n", out->mean);
    line(ans); say("median = %n", out->median);
    line(ans);
    if (out->nmodes == 0) say("mode: none");
    else if (out->nmodes == 1) say("mode = %n", out->mode[0]);
    else say("modes %n, %n", out->mode[0], out->mode[1]);
    line(ans); say("min %n  max %n", out->lo, out->hi);
    line(ans); say("range = %n", out->hi - out->lo);
    line(ans);
    if (n > 1) say("std dev s = %n", out->sd);
    else say("std dev: needs 2+");
    line(ans); say("pop. sd = %n", out->psd);

    steps(ans);
    say("Sorted:");
    for (i = 0; i < n; i++) say(" %n", out->sorted[i]);
    say("\nmean = sum / n = %n / %d = %n\n", out->sum, n, out->mean);
    if (n % 2) say("median = the middle value = %n\n", out->median);
    else say("median = average of the two middle values (%n + %n) / 2 = %n\n", out->sorted[n / 2 - 1], out->sorted[n / 2], out->median);
    if (out->nmodes) say("mode = the value seen most often (%d times)\n", best);
    else say("mode: every value appears once, so there is none\n");
    say("variance = sum of (x - mean)^2 / (n - 1) = %n / %d = %n\n", sq, n > 1 ? n - 1 : 1, n > 1 ? sq / (float)(n - 1) : 0);
    say("s = sqrt(variance) = %n  (divide by n instead for a whole population: %n)", out->sd, out->psd);
    return true;
}
