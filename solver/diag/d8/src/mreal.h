#ifndef MREAL_H
#define MREAL_H

/* On the calculator `double` is only 32 bits, which is too coarse for exact
 * answers, so the solver does its maths in `long double` (64 bits there). */

#include <math.h>

typedef long double real;

#define sin sinl
#define cos cosl
#define tan tanl
#define asin asinl
#define acos acosl
#define atan atanl
#define sqrt sqrtl
#define log logl
#define log10 log10l
#define exp expl
#define pow powl
#define fabs fabsl
#define floor floorl
#define fmod fmodl

#endif
