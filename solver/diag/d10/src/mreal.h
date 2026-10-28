#ifndef MREAL_H
#define MREAL_H

/* The calculator does 64-bit `long double` maths in software at about 2 ms per operation, which made
 * the solver take seconds per answer. 32-bit float is about 50 times faster, so the solver uses it
 * and every comparison below allows for roughly 6 to 7 digits of accuracy. */

#include <math.h>

typedef float real;

#define sin sinf
#define cos cosf
#define tan tanf
#define asin asinf
#define acos acosf
#define atan atanf
#define sqrt sqrtf
#define log logf
#define log10 log10f
#define exp expf
#define pow powf
#define fabs fabsf
#define floor floorf
#define fmod fmodf

#endif
