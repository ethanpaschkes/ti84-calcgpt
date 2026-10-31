#ifndef PROBS_H
#define PROBS_H

#include <stdbool.h>
#include "answer.h"

/* The other guided problem types. Each fills the answer lines, the rule and the steps, or ans->err and false. No drawing. */

#define STATS_MAX 10

struct quad_out {
    int nreal;          /* 0, 1 or 2 real roots */
    float x1, x2;       /* the roots (x1 <= x2), or the real and imaginary part when nreal == 0 */
    float vx, vy;       /* the vertex */
};

/* a x^2 + b x + c = 0 */
bool quad_solve(float a, float b, float c, struct quad_out *out, struct answer *ans);

/* a x + b = c x + d; *x is the solution when there is exactly one (kind 1); kind 0 = none, kind 2 = every x */
bool lin_solve(float a, float b, float c, float d, int *kind, float *x, struct answer *ans);

struct trig_out {
    float deg;          /* the angle brought into 0 to 360 */
    float ref;          /* the reference angle */
    int quadrant;       /* 1 to 4, 0 on an axis */
    float s, c, t;      /* sin, cos, tan (tan is NaN where undefined) */
};

/* sin, cos, tan of an angle in degrees, with the exact forms the unit circle gives, and the angle in radians */
bool trig_values(float deg, struct trig_out *out, struct answer *ans);

struct stats_out {
    int n;
    float sorted[STATS_MAX];
    float mean, median, lo, hi, sum, sd, psd;   /* sd: sample (n - 1), psd: population */
    int nmodes;                                 /* 0 when every value appears once */
    float mode[2];
};

bool stats_solve(const float *x, int n, struct stats_out *out, struct answer *ans);

/* the exact unit-circle name of v ("1/2", "-sqrt3/2", ...) or NULL */
const char *exact_trig(float v);
/* an angle in degrees as a multiple of pi ("5pi/6", "pi", "0"), or NULL when it is not a neat one */
bool radians_text(float deg, char *out);

#endif
