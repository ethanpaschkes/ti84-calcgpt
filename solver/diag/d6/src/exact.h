#ifndef EXACT_H
#define EXACT_H

#include <stdint.h>

#include "mreal.h"

#include <stdbool.h>
#include <stddef.h>

/* Exact forms. Results use the editor's markup: "[num|den]" is a stacked
 * fraction, and roots are written sqrt(n). */

int32_t mx_gcd(int32_t a, int32_t b);

/* The fraction n/d (d > 0, at most maxden) closest to v, if within tol. */
bool mx_rationalize(real v, int32_t maxden, real tol, int32_t *n, int32_t *d);

/* sqrt(N) = s * sqrt(m) with m squarefree. */
void mx_simplify_radical(int32_t N, int32_t *s, int32_t *m);

/* Write v as an integer, fraction, a*sqrt(n)/q, or (if allow_pi) a rational
 * multiple of pi. Returns true if it is one of those; otherwise writes a decimal
 * and returns false. */
bool mx_exact(real v, bool allow_pi, char *out, size_t cap);

/* v with up to `sig` significant digits, trailing zeros dropped. */
void mx_decimal(real v, int sig, char *out, size_t cap);

#endif
