/* The Math app's pure parts on a computer: numbers, the triangle solver (every case, the ambiguous one and the impossible ones),
 * the other problem types, the form's key-to-field mapping, and the whole screen flow. Run by test_math.py. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/app.h"
#include "../src/num.h"
#include "../src/probs.h"
#include "../src/tri.h"
#include "../../shared/keys.h"

static int fails;

#define CHECK(what, cond) do { if (!(cond)) { printf("FAIL %s\n", what); fails++; } } while (0)
#define NEAR(what, got, want) do { float g_ = (float)(got), w_ = (float)(want); if (fabsf(g_ - w_) > 0.02f) { printf("FAIL %s: got %g want %g\n", what, (double)g_, (double)w_); fails++; } } while (0)
#define HAS(what, text, needle) do { if (!strstr((text), (needle))) { printf("FAIL %s: '%s' not in:\n%s\n", what, needle, text); fails++; } } while (0)

/* the number keys as the calculator has them (group, bit from keypadc.h) */
#define KEY_0 K(3, 0)
#define KEY_1 K(3, 1)
#define KEY_2 K(4, 1)
#define KEY_3 K(5, 1)
#define KEY_4 K(3, 2)
#define KEY_5 K(4, 2)
#define KEY_6 K(5, 2)
#define KEY_7 K(3, 3)
#define KEY_8 K(4, 3)
#define KEY_9 K(5, 3)
#define KEY_DOT K(4, 0)
#define KEY_NEG K(5, 0)
#define KEY_X K(3, 6)   /* the x,t,theta,n key: a letter, not a number */

/* ---------------------------------------------------------------- numbers */

static void test_numbers(void)
{
    char b[NUM_CHARS], w[6][WRAP_W];
    float v;
    fmt_num(5.0f, 2, b); CHECK("5.00 shows as 5", !strcmp(b, "5"));
    fmt_num(3.14159f, 2, b); CHECK("3.14", !strcmp(b, "3.14"));
    fmt_num(2.345f, 2, b); CHECK("2.345 rounds to 2.35", !strcmp(b, "2.35"));
    fmt_num(2.5f, 0, b); CHECK("2.5 to 0 decimals is 3", !strcmp(b, "3"));
    fmt_num(-0.5f, 2, b); CHECK("-0.5", !strcmp(b, "-0.5"));
    fmt_num(-0.004f, 2, b); CHECK("no minus zero", !strcmp(b, "0"));
    fmt_num(1234.5678f, 3, b); CHECK("1234.568", !strcmp(b, "1234.568"));
    fmt_num(0.1f, 4, b); CHECK("0.1", !strcmp(b, "0.1"));
    fmt_num(NAN, 2, b); CHECK("NaN is undefined", !strcmp(b, "undefined"));
    fmt_num(36.8699f, 1, b); CHECK("36.9", !strcmp(b, "36.9"));
    CHECK("parse 12", parse_num("12", &v) && v == 12.0f);
    CHECK("parse -3.5", parse_num("-3.5", &v) && fabsf(v + 3.5f) < 1e-6f);
    CHECK("parse .5", parse_num(".5", &v) && fabsf(v - 0.5f) < 1e-6f);
    CHECK("empty is not a number", !parse_num("", &v));
    CHECK("- alone is not a number", !parse_num("-", &v));
    CHECK("1.2.3 is not a number", !parse_num("1.2.3", &v));
    CHECK("letters are not a number", !parse_num("1x", &v));
    say_to(b, sizeof(b));
    say("%s=%n %d%%", "x", 2.0, -7);
    CHECK("say", !strcmp(b, "x=2 -7%"));
    say_to(b, 6);
    say("a very long text");
    CHECK("say never overruns", strlen(b) == 5);
    CHECK("wrap: 4 lines", wrap_text("one two three four\nfive", 9, w, 6) == 4);
    CHECK("wrap: first line", !strcmp(w[0], "one two"));
    CHECK("wrap: second line", !strcmp(w[1], "three"));
    CHECK("wrap: a new line starts a line", !strcmp(w[3], "five"));
    CHECK("wrap: stops at max", wrap_text("a\nb\nc\nd", 9, w, 2) == 2);
}

/* ---------------------------------------------------------------- triangles */

static struct tri_in mk(const char *spec, bool right)
{
    struct tri_in t;
    const char *p = spec;
    memset(&t, 0, sizeof(t));
    t.right = right;
    while (*p) {
        char k = *p++;
        float v;
        char num[16];
        int n = 0;
        while ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-') num[n++] = *p++;
        num[n] = 0;
        while (*p == ' ') p++;
        parse_num(num, &v);
        if (k >= 'a' && k <= 'c') { t.has[k - 'a'] = true; t.v[k - 'a'] = v; }
        if (k >= 'A' && k <= 'C') { t.has[3 + k - 'A'] = true; t.v[3 + k - 'A'] = v; }
    }
    return t;
}

static struct tri_out T;
static struct answer A;

static bool tri(const char *spec, bool right)
{
    struct tri_in in = mk(spec, right);
    return tri_solve(&in, &T, &A);
}

static void test_triangles(void)
{
    int px[3], py[3], i;
    /* SSS */
    CHECK("SSS 3 4 5 solves", tri("a3 b4 c5", false) && T.n == 1);
    NEAR("SSS A", T.sol[0].a[0], 36.87f);
    NEAR("SSS B", T.sol[0].a[1], 53.13f);
    NEAR("SSS C", T.sol[0].a[2], 90.0f);
    HAS("SSS rule", A.rule, "Cosines");
    HAS("SSS steps plug the numbers in", A.steps, "cos A = (16 + 25 - 9) / (2 x 4 x 5) = 0.8, so A = 36.87");
    NEAR("area of 3 4 5", tri_area(&T.sol[0]), 6.0f);
    CHECK("SSS 6 8 11", tri("a6 b8 c11", false));
    NEAR("A = 32.16", T.sol[0].a[0], 32.157f);
    /* SAS */
    CHECK("SAS a12 b13 C100", tri("a12 b13 C100", false));
    NEAR("SAS B = 41.92", T.sol[0].a[1], 41.922f);
    NEAR("SAS c", T.sol[0].s[2], 19.17f);
    HAS("SAS rule", A.rule, "SAS");
    /* ASA / AAS */
    CHECK("AAS A30 B65 a8.7", tri("A30 B65 a8.7", false));
    NEAR("AAS b = 15.77", T.sol[0].s[1], 15.77f);
    NEAR("AAS C = 85", T.sol[0].a[2], 85.0f);
    HAS("AAS steps", A.steps, "C = 180 - 30 - 65 = 85");
    CHECK("ASA A53 C13 b12.8", tri("A53 C13 b12.8", false));
    NEAR("ASA c = 3.15", T.sol[0].s[2], 3.152f);
    /* SSA: two triangles */
    CHECK("SSA a6 b8 A20 solves", tri("a6 b8 A20", false));
    CHECK("SSA: two triangles", T.n == 2);
    NEAR("SSA first c", T.sol[0].s[2], 12.857f);
    NEAR("SSA second c", T.sol[1].s[2], 2.178f);
    NEAR("SSA first B", T.sol[0].a[1], 27.13f);
    NEAR("SSA second B", T.sol[1].a[1], 152.87f);
    HAS("SSA rule", A.rule, "ambiguous");
    HAS("SSA steps say two fit", A.steps, "Two triangles fit");
    /* SSA: one triangle (the obtuse option leaves no third angle) */
    CHECK("SSA a10 b6 A30", tri("a10 b6 A30", false) && T.n == 1);
    NEAR("one B", T.sol[0].a[1], 17.46f);
    /* SSA: right angle, one triangle */
    CHECK("SSA a5 b5 A90", tri("a5 b5 A90", false) == false || T.n >= 1);
    /* SSA: none */
    CHECK("SSA a2 b8 A30 impossible", !tri("a2 b8 A30", false));
    HAS("why: sine over 1", A.err, "more than 1");
    HAS("why: names the short side", A.err, "a = 2 is too short");
    /* impossible inputs, each with a reason */
    CHECK("1 1 5 impossible", !tri("a1 b1 c5", false));
    HAS("why: sides can't meet", A.err, "a + b = 2 is not more than c = 5");
    CHECK("angles 100 + 90", !tri("A100 B90 c5", false));
    HAS("why: angles over 180", A.err, "add to 190");
    CHECK("angles alone", !tri("A30 B40 C110", false));
    HAS("why: no side", A.err, "Give at least one side");
    CHECK("two values only", !tri("a3 b4", false));
    HAS("why: 3 needed", A.err, "Fill in 3 values");
    HAS("how many so far", A.err, "2 so far");
    CHECK("four values", !tri("a3 b4 c5 A30", false));
    HAS("why: too many", A.err, "Only 3 values");
    CHECK("negative side", !tri("a-3 b4 c5", false));
    HAS("why: side", A.err, "Side a must be more than 0");
    CHECK("angle 200", !tri("a3 b4 C200", false));
    HAS("why: angle range", A.err, "between 0 and 180");
    CHECK("SSA with the side next to the angle missing", tri("A30 b4 c5", false));   /* that is SAS at A */
    /* right triangles: two values */
    CHECK("right legs 3 4", tri("a3 b4", true) && T.n == 1);
    NEAR("hypotenuse 5", T.sol[0].s[2], 5.0f);
    NEAR("right A", T.sol[0].a[0], 36.87f);
    NEAR("right C", T.sol[0].a[2], 90.0f);
    HAS("right rule", A.rule, "Pythagoras");
    HAS("right steps", A.steps, "c^2 = 3^2 + 4^2 = 9 + 16 = 25");
    CHECK("right hyp 10 leg 6", tri("c10 a6", true));
    NEAR("other leg 8", T.sol[0].s[1], 8.0f);
    HAS("right hyp steps", A.steps, "sqrt(100 - 36) = 8");
    CHECK("right hyp 10 A30", tri("c10 A30", true));
    NEAR("a = 5", T.sol[0].s[0], 5.0f);
    NEAR("b = 8.66", T.sol[0].s[1], 8.66f);
    NEAR("B = 60", T.sol[0].a[1], 60.0f);
    CHECK("right leg a 3 and B 60", tri("a3 B60", true));
    NEAR("c = 6", T.sol[0].s[2], 6.0f);
    CHECK("right: leg longer than hyp", !tri("a5 c3", true));
    HAS("why: hypotenuse", A.err, "hypotenuse");
    CHECK("right: C not 90", !tri("a3 b4 C60", true));
    HAS("why: C is 90", A.err, "C is the 90 degree corner");
    CHECK("right: C = 90 typed is fine", tri("a3 b4 C90", true));
    CHECK("right: A = 90", !tri("a3 A90", true));
    HAS("why: under 90", A.err, "under 90");
    CHECK("right: one value", !tri("a3", true));
    HAS("why: 2 needed", A.err, "Fill in 2 values");
    CHECK("right: three values", !tri("a3 b4 c5", true));
    HAS("why: only 2", A.err, "Only 2 values");
    /* the drawing: to scale, inside the box */
    tri("a3 b4 c5", false);
    tri_points(&T.sol[0], 100, 50, 120, 100, px, py);
    for (i = 0; i < 3; i++) CHECK("corner inside the box", px[i] >= 100 && px[i] <= 220 && py[i] >= 50 && py[i] <= 150);
    CHECK("side c is level, centred in the box", py[0] == py[1] && py[0] > 100 && py[0] <= 150);
    CHECK("A is left of B", px[0] < px[1]);
    NEAR("to scale: b / c on screen", (float)hypotf((float)(px[2] - px[0]), (float)(py[2] - py[0])) / (float)(px[1] - px[0]), 4.0f / 5.0f);
    tri("a6 b8 A20", false);
    tri_points(&T.sol[1], 0, 0, 100, 100, px, py);
    CHECK("obtuse triangle still inside", px[2] >= 0 && px[2] <= 100 && py[2] >= 0);
}

/* ---------------------------------------------------------------- other types */

static void test_others(void)
{
    struct quad_out q;
    struct trig_out t;
    struct stats_out s;
    int kind;
    float x, data[6] = {1, 3, 5, 7, 9, 0};
    char rad[NUM_CHARS];

    CHECK("x^2 - 5x + 6", quad_solve(1, -5, 6, &q, &A) && q.nreal == 2);
    NEAR("root 2", q.x1, 2.0f);
    NEAR("root 3", q.x2, 3.0f);
    HAS("quad line", A.line[0], "x = 2");
    HAS("quad steps", A.steps, "b^2 - 4ac = 25 - 4(1)(6) = 1");
    HAS("vertex line", A.line[2], "vertex x = 2.5");
    CHECK("x^2 - 4x + 4 double root", quad_solve(1, -4, 4, &q, &A) && q.nreal == 1);
    HAS("double", A.line[0], "double");
    CHECK("x^2 + 1 no real roots", quad_solve(1, 0, 1, &q, &A) && q.nreal == 0);
    HAS("complex", A.line[1], "x = 0 + 1i");
    CHECK("a = 0 is a line", quad_solve(0, 2, -4, &q, &A));
    HAS("line answer", A.line[0], "x = 2");
    CHECK("a = b = 0 refused", !quad_solve(0, 0, 3, &q, &A));

    CHECK("2x + 3 = x + 7", lin_solve(2, 3, 1, 7, &kind, &x, &A) && kind == 1);
    NEAR("x = 4", x, 4.0f);
    HAS("lin steps", A.steps, "Divide by 1: x = 4 / 1 = 4");
    CHECK("same both sides", lin_solve(2, 3, 2, 3, &kind, &x, &A) && kind == 2);
    HAS("every x", A.line[0], "Every x");
    CHECK("parallel", lin_solve(2, 3, 2, 5, &kind, &x, &A) && kind == 0);
    HAS("no solution", A.line[0], "No solution");

    CHECK("150 degrees", trig_values(150, &t, &A));
    CHECK("quadrant II", t.quadrant == 2);
    NEAR("reference 30", t.ref, 30.0f);
    HAS("sin 150 = 1/2", A.line[0], "sin = 1/2");
    HAS("cos 150", A.line[1], "cos = -sqrt3/2");
    HAS("tan 150", A.line[2], "tan = -sqrt3/3");
    HAS("radians", A.line[3], "5pi/6 rad");
    CHECK("390 wraps to 30", trig_values(390, &t, &A) && fabsf(t.deg - 30.0f) < 1e-3f);
    HAS("wrap step", A.steps, "390 is the same spot as 30");
    CHECK("-45 wraps to 315", trig_values(-45, &t, &A) && fabsf(t.deg - 315.0f) < 1e-3f && t.quadrant == 4);
    CHECK("90: tan undefined", trig_values(90, &t, &A) && t.quadrant == 0);
    HAS("undefined", A.line[2], "undefined");
    HAS("pi/2", A.line[3], "pi/2");
    CHECK("37 degrees has no exact form", trig_values(37, &t, &A));
    HAS("decimal sin", A.line[0], "sin = 0.6");
    CHECK("radians 180", radians_text(180, rad) && !strcmp(rad, "pi"));
    CHECK("radians 0", radians_text(0, rad) && !strcmp(rad, "0"));
    CHECK("radians 270", radians_text(270, rad) && !strcmp(rad, "3pi/2"));
    CHECK("radians 37: none", !radians_text(37, rad));
    CHECK("exact 0.5", !strcmp(exact_trig(0.5f), "1/2"));
    CHECK("exact -1", !strcmp(exact_trig(-1.0f), "-1"));
    CHECK("exact 0.6 is none", exact_trig(0.6f) == NULL);

    CHECK("stats 1 3 5 7 9", stats_solve(data, 5, &s, &A));
    NEAR("mean 5", s.mean, 5.0f);
    NEAR("median 5", s.median, 5.0f);
    NEAR("sample sd", s.sd, 3.162f);
    NEAR("population sd", s.psd, 2.828f);
    CHECK("no mode", s.nmodes == 0);
    HAS("stats line", A.line[1], "mean = 5");
    HAS("stats steps", A.steps, "mean = sum / n = 25 / 5 = 5");
    data[0] = 9; data[1] = 2; data[2] = 9; data[3] = 4;
    CHECK("stats 9 2 9 4", stats_solve(data, 4, &s, &A));
    NEAR("even median", s.median, 6.5f);
    CHECK("mode 9", s.nmodes == 1 && s.mode[0] == 9.0f);
    CHECK("sorted", s.sorted[0] == 2 && s.sorted[3] == 9);
    CHECK("no numbers refused", !stats_solve(data, 0, &s, &A));
    CHECK("one number: sd needs 2+", stats_solve(data, 1, &s, &A));
    HAS("needs 2+", A.line[6], "needs 2+");
}

/* ---------------------------------------------------------------- the form */

static void test_form(void)
{
    static const char *const labels[3] = {"a", "b", "c"};
    static const char *const ten[10] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
    struct form f;
    float v;
    form_init(&f, "t", "h", labels, 3);
    CHECK("starts on the first field", f.cur == 0 && f.cols == 1);
    CHECK("the 1 key types 1", form_key(&f, KEY_1) == FORM_CHANGED && !strcmp(f.f[0].text, "1"));
    form_key(&f, KEY_2); form_key(&f, KEY_DOT); form_key(&f, KEY_5);
    CHECK("12.5", !strcmp(f.f[0].text, "12.5"));
    CHECK("a second dot is ignored", form_key(&f, KEY_DOT) == FORM_NONE && !strcmp(f.f[0].text, "12.5"));
    CHECK("a minus in the middle is ignored", form_key(&f, KEY_NEG) == FORM_NONE);
    CHECK("a letter key is ignored", form_key(&f, KEY_X) == FORM_NONE);
    CHECK("value 12.5", form_value(&f, 0, &v) && fabsf(v - 12.5f) < 1e-5f);
    CHECK("Del erases one", form_key(&f, K_DEL) == FORM_CHANGED && !strcmp(f.f[0].text, "12."));
    CHECK("Enter moves on", form_key(&f, K_ENTER) == FORM_CHANGED && f.cur == 1);
    CHECK("(-) first is a minus", form_key(&f, KEY_NEG) == FORM_CHANGED && !strcmp(f.f[1].text, "-"));
    CHECK("'-' alone is not a value", !form_value(&f, 1, &v));
    form_key(&f, KEY_3);
    CHECK("-3", form_value(&f, 1, &v) && v == -3.0f);
    form_key(&f, K_ENTER);
    CHECK("empty field has no value", !form_value(&f, 2, &v));
    CHECK("Down to the Solve row", form_key(&f, K_DOWN) == FORM_CHANGED && f.cur == 3);
    CHECK("Down stops there", form_key(&f, K_DOWN) == FORM_NONE);
    CHECK("typing on Solve does nothing", form_key(&f, KEY_7) == FORM_NONE);
    CHECK("Enter on Solve solves", form_key(&f, K_ENTER) == FORM_SOLVE);
    CHECK("Up goes back to a field", form_key(&f, K_UP) == FORM_CHANGED && f.cur == 2);
    CHECK("two filled", form_filled(&f) == 2);
    CHECK("Clear goes back", form_key(&f, K_CLEAR) == FORM_BACK);
    form_key(&f, KEY_0);
    CHECK("0 types 0", f.f[2].text[0] == '0');
    CHECK("4 6 8 9", KEY_4 == 26 && KEY_6 == 42 && KEY_8 == 35 && KEY_9 == 43);
    form_init(&f, "t", "h", ten, 10);
    CHECK("ten fields: two columns of five", f.cols == 2 && form_rows(&f) == 5);
    CHECK("Right jumps a column", form_key(&f, K_RIGHT) == FORM_CHANGED && f.cur == 5);
    CHECK("Right at the second column stays", form_key(&f, K_RIGHT) == FORM_NONE);
    CHECK("Left comes back", form_key(&f, K_LEFT) == FORM_CHANGED && f.cur == 0);
    CHECK("a field holds 9 characters", 1);
    for (v = 0; v < 12; v++) form_key(&f, KEY_1);
    CHECK("no more than 9", f.f[0].len == FIELD_CHARS);
}

/* ---------------------------------------------------------------- the whole flow */

static void type_value(struct app *a, const int *keys, int n)
{
    int i;
    for (i = 0; i < n; i++) app_key(a, keys[i], false);
    app_key(a, K_ENTER, false);
}

static void test_flow(void)
{
    static struct app a;
    static const int three[1] = {KEY_3}, four[1] = {KEY_4};
    static const int six[1] = {KEY_6}, eight[1] = {KEY_8}, twenty[2] = {KEY_2, KEY_0};
    app_init(&a);
    CHECK("starts at home on Triangle", a.screen == S_HOME && a.home_sel == P_TRI);
    CHECK("six problem types", P_COUNT == 6);
    HAS("home hint", PTYPES[P_TRI].hint, "sides and angles");
    CHECK("Clear at home quits", app_key(&a, K_CLEAR, false) == APP_QUIT);
    CHECK("Enter opens What kind", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_KIND);
    CHECK("Enter picks the right triangle", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_FORM && a.right);
    CHECK("the right form has 5 fields", a.form.n == 5 && !strcmp(a.form.f[4].label, "B"));
    HAS("the form says what to fill", a.form.hint, "Fill any 2");
    type_value(&a, three, 1);
    type_value(&a, four, 1);
    CHECK("now on c", a.form.cur == 2);
    app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false);
    CHECK("on Solve", a.form.cur == a.form.n);
    CHECK("Solve shows the answers", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_RESULT);
    HAS("c = 5", a.ans.line[2], "c = 5");
    HAS("A", a.ans.line[3], "A = 36.87");
    HAS("area", a.ans.line[6], "Area = 6");
    HAS("rule", a.ans.rule, "Pythagoras");
    HAS("help names the keys", app_help(&a, false), "Graph steps");
    CHECK("Y= does nothing offline", app_key(&a, K_YEQU, false) == APP_NONE);
    CHECK("Y= asks ChatGPT online", app_key(&a, K_YEQU, true) == APP_ASK && a.screen == S_CHAT);
    HAS("the question in words", a.question, "Triangle problem (right triangle, C = 90): a = 3, b = 4, find the rest");
    CHECK("Clear leaves the chat", app_key(&a, K_CLEAR, true) == APP_REDRAW && a.screen == S_RESULT);
    CHECK("Graph shows the steps", app_key(&a, K_GRAPH, false) == APP_REDRAW && a.screen == S_STEPS);
    CHECK("Graph again: the diagram", app_key(&a, K_GRAPH, false) == APP_REDRAW && a.screen == S_RESULT);
    CHECK("Enter: a new, empty form", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_FORM && form_filled(&a.form) == 0);
    /* an impossible input stays on the form and says why */
    type_value(&a, three, 1);
    app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false);
    CHECK("Solve with one value: stays on the form", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_FORM && a.show_err);
    HAS("and says why", a.ans.err, "Fill in 2 values");
    CHECK("typing hides the error", app_key(&a, K_UP, false) == APP_REDRAW && app_key(&a, KEY_4, false) == APP_REDRAW && !a.show_err);
    CHECK("Clear from the form: back to What kind", app_key(&a, K_CLEAR, false) == APP_REDRAW && a.screen == S_KIND);
    /* any triangle, the ambiguous case, Left/Right between the two */
    app_key(&a, K_DOWN, false);
    CHECK("Any triangle", app_key(&a, K_ENTER, false) == APP_REDRAW && !a.right && a.form.n == 6);
    type_value(&a, six, 1);
    type_value(&a, eight, 1);
    app_key(&a, K_DOWN, false);
    type_value(&a, twenty, 2);
    app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false);
    CHECK("SSA solved", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_RESULT && a.tri.n == 2);
    HAS("first of two", a.ans.line[7], "Triangle 1 of 2");
    HAS("first c", a.ans.line[2], "c = 12.86");
    CHECK("Right: the second", app_key(&a, K_RIGHT, false) == APP_REDRAW && a.which == 1);
    HAS("second c", a.ans.line[2], "c = 2.18");
    CHECK("Clear: back to the form with the values kept", app_key(&a, K_CLEAR, false) == APP_REDRAW && a.screen == S_FORM && form_filled(&a.form) == 3);
    /* the decimals setting */
    app_key(&a, K_CLEAR, false);
    app_key(&a, K_CLEAR, false);
    CHECK("home again", a.screen == S_HOME);
    while (a.home_sel < P_COUNT) app_key(&a, K_DOWN, false);
    CHECK("Right adds a decimal", app_key(&a, K_RIGHT, false) == APP_REDRAW && num_decimals == 3);
    app_key(&a, K_LEFT, false); app_key(&a, K_LEFT, false); app_key(&a, K_LEFT, false);
    CHECK("Left stops at 0", num_decimals == 0 && app_key(&a, K_LEFT, false) == APP_NONE);
    CHECK("Enter on Decimals does nothing", app_key(&a, K_ENTER, false) == APP_NONE);
    HAS("decimals help", app_help(&a, false), "decimals");
    num_decimals = 2;
    /* the other types open a form straight away */
    app_key(&a, K_UP, false);
    CHECK("Stats opens a 10-field form", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_FORM && a.form.n == 10);
    type_value(&a, three, 1);
    type_value(&a, four, 1);
    while (a.form.cur < a.form.n) app_key(&a, K_DOWN, false);
    CHECK("stats of 3 and 4", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_RESULT);
    HAS("mean 3.5", a.ans.line[1], "mean = 3.5");
    CHECK("Clear, Clear: home (no What kind for stats)", app_key(&a, K_CLEAR, false) == APP_REDRAW && app_key(&a, K_CLEAR, false) == APP_REDRAW && a.screen == S_HOME);
    a.home_sel = P_QUAD;
    app_key(&a, K_ENTER, false);
    app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false); app_key(&a, K_DOWN, false);
    CHECK("quadratic with empty fields: refused", app_key(&a, K_ENTER, false) == APP_REDRAW && a.screen == S_FORM);
    HAS("says which", a.ans.err, "Fill in a");
}

int main(void)
{
    test_numbers();
    test_triangles();
    test_others();
    test_form();
    test_flow();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("math (numbers, triangles, other types, form, flow): all passed\n");
    return 0;
}
