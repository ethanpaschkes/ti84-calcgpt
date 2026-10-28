#ifndef EXPR_H
#define EXPR_H

#include <stdbool.h>

/* A line with no = sign: evaluate it, or simplify it if it has x. */
struct mx_eval {
    int kind;            /* EK_ERROR, EK_NUMBER, EK_POLY, EK_FRACTION, EK_OTHER */
    char exact[120];     /* exact form (editor markup) */
    char dec[24];        /* decimal */
    bool is_exact;       /* exact differs from a plain decimal */
    char steps[400];     /* short steps */
    char note[100];
};

enum { EK_ERROR, EK_NUMBER, EK_POLY, EK_FRACTION, EK_OTHER };

void mx_evaluate(const char *text, struct mx_eval *out);

#endif
