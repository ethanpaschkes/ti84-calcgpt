#ifndef TRI_H
#define TRI_H

#include <stdbool.h>
#include "answer.h"

/* The triangle solver: sides a, b, c are opposite the angles A, B, C; angles in degrees. Any three values (one of them a side)
 * fix a triangle, except the SSA case, which can fit two. A right triangle has C = 90 built in and needs two values.
 * No drawing in here; tests/math_test.c checks every case on a computer. */

enum { T_a, T_b, T_c, T_A, T_B, T_C };   /* the six values, in the order the form shows them */

struct tri_in {
    bool has[6];
    float v[6];
    bool right;   /* C = 90, two values needed */
};

struct tri_sol {
    float s[3], a[3];   /* a b c, A B C */
};

struct tri_out {
    int n;                  /* 0 (impossible), 1, or 2 (the ambiguous SSA case) */
    struct tri_sol sol[2];
};

/* Fills out (the numbers) and ans->rule, ans->steps; on an impossible input ans->err says why and false comes back. */
bool tri_solve(const struct tri_in *in, struct tri_out *out, struct answer *ans);

/* the three corners A, B, C of the triangle drawn to scale inside the box (x0, y0, w, h), side c along the bottom */
void tri_points(const struct tri_sol *t, int x0, int y0, int w, int h, int px[3], int py[3]);

float tri_area(const struct tri_sol *t);

#endif
