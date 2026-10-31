#include "app.h"
#include <string.h>
#include "keys.h"
#include "num.h"

const struct ptype PTYPES[P_COUNT] = {
    {"Triangle", "find sides and angles"},
    {"Pythagorean", "a^2 + b^2 = c^2"},
    {"Quadratic", "a x^2 + b x + c = 0"},
    {"Equation", "a x + b = c x + d"},
    {"Unit circle", "sin cos tan, radians"},
    {"Stats of a list", "mean, median, spread"},
};

static const char *const L_TRI[6] = {"a", "b", "c", "A", "B", "C"};
static const char *const L_QUAD[3] = {"a", "b", "c"};
static const char *const L_LIN[4] = {"a", "b", "c", "d"};
static const char *const L_UNIT[1] = {"deg"};
static const char *const L_STATS[10] = {"x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10"};

void app_init(struct app *a)
{
    memset(a, 0, sizeof(*a));
    a->ans.rule = "";
}

static void open_form(struct app *a)
{
    switch (a->ptype) {
    case P_TRI:
        if (a->right) form_init(&a->form, "Right triangle", "Fill any 2. C is 90, c = hypotenuse", L_TRI, 5);
        else form_init(&a->form, "Any triangle", "Fill any 3, at least one a side", L_TRI, 6);
        break;
    case P_PYTH: form_init(&a->form, "Pythagorean", "Fill 2 sides. c is the hypotenuse", L_QUAD, 3); break;
    case P_QUAD: form_init(&a->form, "Quadratic", "a x^2 + b x + c = 0: fill a, b, c", L_QUAD, 3); break;
    case P_LIN: form_init(&a->form, "Equation", "a x + b = c x + d: fill all four", L_LIN, 4); break;
    case P_UNIT: form_init(&a->form, "Unit circle", "Degrees. 390 or -45 are fine too", L_UNIT, 1); break;
    default: form_init(&a->form, "Stats of a list", "Up to 10 numbers, rest stay empty", L_STATS, 10); break;
    }
    a->show_err = false;
    a->screen = S_FORM;
}

/* the problem in words, for the Ask ChatGPT row */
static void write_question(struct app *a)
{
    int i;
    float v;
    say_to(a->question, QUESTION_MAX);
    say("%s problem%s: ", PTYPES[a->ptype].title, a->ptype == P_TRI && a->right ? " (right triangle, C = 90)" : "");
    for (i = 0; i < a->form.n; i++) {
        if (!form_value(&a->form, i, &v)) continue;
        say("%s = %n, ", a->form.f[i].label, v);
    }
    say("find the rest and explain each step in a few short lines.");
}

void app_tri_lines(struct app *a)
{
    const struct tri_sol *t = &a->tri.sol[a->which];
    int i;
    a->ans.n = 0;
    for (i = 0; i < 3; i++) {
        say_to(a->ans.line[a->ans.n++], ANS_COLS);
        say("%s = %n", L_TRI[i], t->s[i]);
    }
    for (i = 0; i < 3; i++) {
        say_to(a->ans.line[a->ans.n++], ANS_COLS);
        say("%s = %n", L_TRI[i + 3], t->a[i]);
    }
    say_to(a->ans.line[a->ans.n++], ANS_COLS);
    say("Area = %n", tri_area(t));
    if (a->tri.n == 2) {
        say_to(a->ans.line[a->ans.n++], ANS_COLS);
        say("Triangle %d of 2", a->which + 1);
    }
}

/* all of the form's fields as numbers; false (with an error) when one is empty */
static bool all_values(struct app *a, float *v)
{
    int i;
    for (i = 0; i < a->form.n; i++) {
        if (!form_value(&a->form, i, &v[i])) {
            say_to(a->ans.err, ERR_MAX);
            say("Fill in %s (use 0 for a term that is not there).", a->form.f[i].label);
            return false;
        }
    }
    return true;
}

static bool solve(struct app *a)
{
    float v[FORM_FIELDS];
    int i;
    a->ans.err[0] = 0;
    a->which = 0;
    if (a->ptype == P_TRI || a->ptype == P_PYTH) {
        struct tri_in in;
        memset(&in, 0, sizeof(in));
        in.right = a->ptype == P_PYTH || a->right;
        for (i = 0; i < a->form.n; i++) in.has[i] = form_value(&a->form, i, &in.v[i]);
        if (!tri_solve(&in, &a->tri, &a->ans)) return false;
        app_tri_lines(a);
        return true;
    }
    if (a->ptype == P_STATS) {
        int n = 0;
        for (i = 0; i < a->form.n; i++) if (form_value(&a->form, i, &v[n])) n++;
        return stats_solve(v, n, &a->stats, &a->ans);
    }
    if (!all_values(a, v)) return false;
    if (a->ptype == P_QUAD) {
        a->qa = v[0];
        a->qb = v[1];
        a->qc = v[2];
        return quad_solve(v[0], v[1], v[2], &a->quad, &a->ans);
    }
    if (a->ptype == P_LIN) {
        a->la = v[0];
        a->lb = v[1];
        a->lc = v[2];
        a->ld = v[3];
        return lin_solve(v[0], v[1], v[2], v[3], &a->lkind, &a->lx, &a->ans);
    }
    return trig_values(v[0], &a->trig, &a->ans);
}

static int home_key(struct app *a, int key)
{
    if (key == K_UP && a->home_sel > 0) a->home_sel--;
    else if (key == K_DOWN && a->home_sel < P_COUNT) a->home_sel++;
    else if (key == K_CLEAR) return APP_QUIT;
    else if (a->home_sel == P_COUNT && key == K_LEFT && num_decimals > 0) num_decimals--;
    else if (a->home_sel == P_COUNT && key == K_RIGHT && num_decimals < 4) num_decimals++;
    else if (key == K_ENTER && a->home_sel < P_COUNT) {
        a->ptype = a->home_sel;
        if (a->ptype == P_TRI) a->screen = S_KIND;
        else open_form(a);
    } else {
        return APP_NONE;
    }
    return APP_REDRAW;
}

static int kind_key(struct app *a, int key)
{
    if ((key == K_UP || key == K_DOWN || key == K_LEFT || key == K_RIGHT)) a->kind_sel = !a->kind_sel;
    else if (key == K_CLEAR) a->screen = S_HOME;
    else if (key == K_ENTER) {
        a->right = a->kind_sel == 0;
        open_form(a);
    } else {
        return APP_NONE;
    }
    return APP_REDRAW;
}

static int form_screen_key(struct app *a, int key)
{
    int did = form_key(&a->form, key);
    if (did == FORM_BACK) {
        a->screen = a->ptype == P_TRI ? S_KIND : S_HOME;
    } else if (did == FORM_SOLVE) {
        if (solve(a)) {
            a->screen = S_RESULT;
            a->show_err = false;
        } else {
            a->show_err = true;
        }
    } else if (did == FORM_CHANGED) {
        a->show_err = false;
    } else {
        return APP_NONE;
    }
    return APP_REDRAW;
}

static int result_key(struct app *a, int key, bool online)
{
    if (key == K_ENTER) {
        open_form(a);
    } else if (key == K_CLEAR) {
        a->screen = S_FORM;
    } else if (key == K_GRAPH) {
        a->screen = a->screen == S_RESULT ? S_STEPS : S_RESULT;
    } else if ((key == K_LEFT || key == K_RIGHT) && a->tri.n == 2 && a->screen == S_RESULT && (a->ptype == P_TRI || a->ptype == P_PYTH)) {
        a->which = !a->which;
        app_tri_lines(a);
    } else if (key == K_YEQU && online && a->screen == S_RESULT) {
        write_question(a);
        a->screen = S_CHAT;
        return APP_ASK;
    } else {
        return APP_NONE;
    }
    return APP_REDRAW;
}

int app_key(struct app *a, int key, bool online)
{
    switch (a->screen) {
    case S_HOME: return home_key(a, key);
    case S_KIND: return kind_key(a, key);
    case S_FORM: return form_screen_key(a, key);
    case S_RESULT:
    case S_STEPS: return result_key(a, key, online);
    default:
        if (key == K_CLEAR) {
            a->screen = S_RESULT;
            return APP_REDRAW;
        }
        return APP_NONE;   /* scrolling in the chat is the main loop's (it owns the chat) */
    }
}

const char *app_help(const struct app *a, bool online)
{
    switch (a->screen) {
    case S_HOME: return a->home_sel == P_COUNT ? "Left/Right: decimals  Clear: exit" : "Up/Down pick  Enter open  Clear exit";
    case S_KIND: return "Up/Down pick  Enter choose  Clear back";
    case S_FORM: return a->form.cur == a->form.n ? "Enter: solve  Up: fields  Clear: back" : "0-9 . (-) type  Del erase  Enter next";
    case S_RESULT: return online ? "Enter new Graph steps Y= AI Clear back" : "Enter new  Graph steps  Clear back";
    case S_STEPS: return "Graph: diagram  Enter: new  Clear: back";
    default: return "Up/Down scroll  Clear: back to answers";
    }
}
