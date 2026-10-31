#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include "answer.h"
#include "form.h"
#include "probs.h"
#include "tri.h"

/* The Math app's screens and what the keys do on each, without any drawing, so tests/math_test.c can walk the whole flow
 * (home -> Triangle -> what kind -> form -> Solve -> answers -> steps) on a computer. screens.c draws whatever is in here. */

enum { P_TRI, P_PYTH, P_QUAD, P_LIN, P_UNIT, P_STATS, P_COUNT };   /* the problem types; row P_COUNT on the home screen is Decimals */
enum { S_HOME, S_KIND, S_FORM, S_RESULT, S_STEPS, S_CHAT };
enum { APP_NONE, APP_REDRAW, APP_QUIT, APP_ASK };                  /* APP_ASK: send app->question to ChatGPT and show S_CHAT */

#define QUESTION_MAX 200

struct ptype {
    const char *title;
    const char *hint;
};

extern const struct ptype PTYPES[P_COUNT];

struct app {
    int screen;
    int home_sel;        /* 0 to P_COUNT */
    int kind_sel;        /* 0 right triangle, 1 any triangle */
    int ptype;
    bool right;          /* the triangle form is the right-triangle one */
    struct form form;
    struct answer ans;
    bool show_err;       /* ans.err is shown under the form */
    /* the numbers behind the diagram */
    struct tri_out tri;
    int which;           /* which of two SSA triangles is shown */
    struct quad_out quad;
    float qa, qb, qc;
    float la, lb, lc, ld, lx;
    int lkind;
    struct trig_out trig;
    struct stats_out stats;
    char question[QUESTION_MAX];   /* the problem in words, for ChatGPT */
};

void app_init(struct app *a);
/* a key went down; online = the box is plugged in and has a server (the ChatGPT row only works then) */
int app_key(struct app *a, int key, bool online);
/* the answer lines for the triangle shown (called again when Left/Right switches between two SSA triangles) */
void app_tri_lines(struct app *a);
/* the help line for the screen */
const char *app_help(const struct app *a, bool online);

#endif
