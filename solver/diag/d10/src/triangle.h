#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "mreal.h"

#include <stdbool.h>
#include <stddef.h>

/* Solve a triangle from three known parts (at least one a side), writing the
 * method, the key steps with your numbers, and all six values. Side a is opposite
 * angle A and so on. Angles are in degrees. */
struct tri_in {
    bool has_s[3], has_a[3];
    real s[3], a[3];
};

/* Returns false (with a message in out) if the input is not a solvable triangle. */
bool tri_work(const struct tri_in *in, char *out, size_t cap);

#endif
