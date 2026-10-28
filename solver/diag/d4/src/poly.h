#ifndef POLY_H
#define POLY_H

#include <stdint.h>

#include "mreal.h"

#include <stdbool.h>
#include <stddef.h>

/* Polynomials in x with fraction coefficients, found by sampling a function. */

struct frac {
    int32_t n, d; /* d > 0, lowest terms */
};

#define POLY_MAX 7 /* degree up to 6 */

struct poly {
    int deg;
    struct frac c[POLY_MAX]; /* c[i] multiplies x^i */
};

typedef real (*mx_fn)(real x, void *ctx);

/* Is fn(x) a polynomial of degree <= 6 with small fraction coefficients? */
bool poly_fit(mx_fn fn, void *ctx, struct poly *out);

/* "x^2 - 4x + 3", fractions as [1|2]x */
void poly_text(const struct poly *p, char *out, size_t cap);

struct poly_roots {
    int nrat;                 /* distinct rational roots */
    struct frac rat[6];
    int mult[6];              /* multiplicities */
    struct poly rest;         /* what is left after taking out the rational roots */
    int nquad;                /* 0, 1 or 2 exact roots of a leftover quadratic; -1 = none real */
    char quad_text[2][40];    /* their exact forms (markup) */
    real quad_val[2];
    int32_t qa, qb, qc;     /* integer quadratic a, b, c of the leftover */
    int32_t disc;           /* b^2 - 4ac of the leftover quadratic */
};

/* Find rational roots, then handle a leftover quadratic exactly. */
void poly_solve(const struct poly *p, struct poly_roots *r);

/* Divide p by (x - root) in place; root must be an exact root. */
void poly_deflate_root(struct poly *p, struct frac root);

/* "(x - 1)(2x + 3)" using the rational roots; false if nothing factors. */
bool poly_factor_text(const struct poly *p, const struct poly_roots *r, char *out, size_t cap);

#endif
