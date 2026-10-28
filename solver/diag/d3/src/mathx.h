#ifndef MATHX_H
#define MATHX_H

#include "mreal.h"

#include <stdbool.h>
#include <stdint.h>

/* Expression engine for the solver. An expression in the single variable x is
 * compiled once into a small postfix program and then run for many values of x,
 * in double precision so exact forms can be recognised afterwards.
 *
 * Understands: + - * / ^, brackets, stacked fractions [num|den], implicit
 * multiplication (2x, 3(x+1)), constants pi and e, and the functions
 * sin cos tan csc sec cot asin acos atan sqrt abs ln log exp, written like
 * sin(x), sin x, sin 2x or sin^2(x). */

#define MX_OPS 120
#define MX_CONSTS 28

struct mx_prog {
    uint8_t op[MX_OPS];
    uint8_t arg[MX_OPS];
    real k[MX_CONSTS];
    uint8_t nops, nk;
};

/* Trig functions use degrees when true, radians otherwise. */
extern bool mx_degrees;

/* The screen sets this so a int32_t search can be stopped with a key press. */
extern bool (*mx_stop_check)(void);

/* Compile s up to the character `end` ('\0' or '='). False if malformed. */
bool mx_compile(const char *s, char end, struct mx_prog *out);

/* Evaluate at x. NAN where undefined. */
real mx_run(const struct mx_prog *p, real x);

/* Does the text use a trig function (sin cos tan csc sec cot)? */
bool mx_has_trig(const char *s);

#endif
