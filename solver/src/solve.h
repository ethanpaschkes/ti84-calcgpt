#ifndef SOLVE_H
#define SOLVE_H

#include "mreal.h"

#include <stdbool.h>

#define SOL_MAX 12

enum { SK_ERROR, SK_NONE, SK_LIST, SK_ALL };

/* Everything the screen needs to show about an equation lhs = rhs in x. */
struct mx_solution {
    int kind;                    /* SK_ERROR, SK_NONE (no solutions), SK_LIST, SK_ALL (identity) */
    int n;                       /* number of solutions listed */
    real val[SOL_MAX];
    char exact[SOL_MAX][48];     /* exact form in editor markup, or the decimal if not exact */
    bool is_exact[SOL_MAX];
    char dec[SOL_MAX][20];       /* decimal form */
    bool trig;                   /* a periodic trig equation */
    real period;               /* its period, in the current angle unit */
    int nbase;                   /* how many of the solutions are the base ones in one period */
    char general[200];           /* "x = 30 + 360n  or  x = 150 + 360n" (editor markup) */
    char steps[600];             /* short steps, '\n' separated, editor markup */
    char note[100];              /* extra remark (counterexample, range searched, ...) */
    bool more;                   /* there were more solutions than shown */
};

/* Solve lhs = rhs. Trig functions use mx_degrees. */
void mx_solve(const char *lhs, const char *rhs, struct mx_solution *out);

#endif
