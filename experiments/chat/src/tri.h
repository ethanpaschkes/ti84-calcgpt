#ifndef TRI_H
#define TRI_H

#include <stdbool.h>
#include <stddef.h>

/* Triangle solver for messages like "sa 6 sb 8 sc 11" or "aa 53 ac 13 sb 12.8".
 * sa/sb/sc are the sides a, b, c and aa/ab/ac are the angles A, B, C (side a is
 * opposite angle A). Give three values, at least one a side. Handles SSS, SAS,
 * AAS/ASA and the ambiguous SSA case. False if the message isn't a triangle. */
bool tri_try(const char *input, char *result, size_t cap);

#endif
