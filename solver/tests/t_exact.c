#include <math.h>
#include <stdio.h>
#include <string.h>
#include "exact.h"
static int fails = 0;
static void X(double v, bool pi, const char *want, bool exact)
{
    char out[80];
    bool e = mx_exact(v, pi, out, sizeof out);
    if (strcmp(out, want) || e != exact) { printf("FAIL %.12g -> '%s' (%d) want '%s' (%d)\n", v, out, e, want, exact); fails++; }
}
static void D(double v, const char *want)
{
    char out[40];
    mx_decimal(v, 6, out, sizeof out);
    if (strcmp(out, want)) { printf("FAIL decimal %.12g -> '%s' want '%s'\n", v, out, want); fails++; }
}
int main(void)
{
    X(0.5, false, "[1|2]", true); X(3, false, "3", true); X(-0.75, false, "[-3|4]", true); X(19.0 / 12, false, "[19|12]", true);
    X(sqrt(2) / 2, false, "[sqrt(2)|2]", true); X(sqrt(3) / 2, false, "[sqrt(3)|2]", true); X(sqrt(8), false, "2sqrt(2)", true);
    X(-sqrt(2) / 2, false, "[-sqrt(2)|2]", true); X(sqrt(6) / 2, false, "[sqrt(6)|2]", true); X(sqrt(5), false, "sqrt(5)", true);
    X(3 * sqrt(5) / 4, false, "[3sqrt(5)|4]", true);
    X(M_PI / 6, true, "[pi|6]", true); X(3 * M_PI / 4, true, "[3pi|4]", true); X(M_PI, true, "pi", true); X(-M_PI / 3, true, "[-pi|3]", true); X(2 * M_PI, true, "2pi", true);
    X(0.7853981633974483, true, "[pi|4]", true);
    X(M_PI / 6, false, "0.523599", false);
    X((1+sqrt(5))/2, false, "[1 + sqrt(5)|2]", true); X(1-sqrt(2), false, "1 - sqrt(2)", true); X(0.7071067811865476, false, "[sqrt(2)|2]", true);
    X(1.0 / 3.0, false, "[1|3]", true); X(0.1 + 0.2, false, "[3|10]", true);
    D(0, "0"); D(1.5, "1.5"); D(0.333333333, "0.333333"); D(123456.789, "123457"); D(-2.5, "-2.5"); D(1e-5, "0.00001"); D(NAN, "undefined");
    printf(fails ? "%d FAILED\n" : "exact: all passed\n", fails);
    return fails != 0;
}
