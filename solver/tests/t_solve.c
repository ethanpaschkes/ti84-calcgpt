#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mathx.h"
#include "solve.h"
static int fails = 0;
static void S(const char *l, const char *r, const char *want)
{
    struct mx_solution s;
    char got[300] = "";
    int i;
    mx_solve(l, r, &s);
    switch (s.kind) {
    case SK_ERROR: strcpy(got, "error"); break;
    case SK_NONE: strcpy(got, "none"); break;
    case SK_ALL: strcpy(got, "all"); break;
    default:
        for (i = 0; i < s.n; i++) { if (i) strcat(got, ", "); strcat(got, s.exact[i]); }
    }
    if (strcmp(got, want)) {
        printf("FAIL %s = %s\n   got  %s\n   want %s\n", l, r, got, want);
        fails++;
    }
}
int main(void)
{
    mx_degrees = true;
    puts("--- algebra");
    S("x^2", "4x - 3", "1, 3"); S("2x + 3", "11", "4"); S("[x+1|2]", "3", "5"); S("x^2", "-1", "none"); S("x^2 - 2", "0", "-sqrt(2), sqrt(2)");
    S("x^2 + x", "1", "[-1 - sqrt(5)|2], [-1 + sqrt(5)|2]"); S("x^3 - 6x^2 + 11x - 6", "0", "1, 2, 3"); S("2x^2 + x", "1", "-1, [1|2]");
    S("x^3", "2", "1.25992"); S("4x", "4x", "all"); S("x + 1", "x + 2", "none"); S("[x^2 - 1|x - 1]", "x + 1", "all");
    S("x(x-5)", "0", "0, 5"); S("x^2 - 6x + 9", "0", "3"); S("3x^2", "0", "0");
    puts("--- other");
    S("sqrt(x)", "x - 2", "4"); S("abs(x-1)", "3", "-2, 4"); S("2^x", "8", "3"); S("e^x", "5", "1.60944"); S("ln(x)", "2", "7.38906"); S("sin x", "x", "0");
    puts("--- trig (degrees)");
    S("sin x", "[1|2]", "30, 150"); S("2cos x", "sqrt(3)", "30, 330"); S("sin x", "cos x", "45, 225"); S("tan x", "1", "45, 225");
    S("sin 2x", "[1|2]", "15, 75, 195, 255"); S("sin^2 x", "[1|4]", "30, 150, 210, 330"); S("cos x", "-1", "180"); S("sin x", "2", "none");
    S("sin x", "-[1|2]", "210, 330"); S("2sin x + 1", "0", "210, 330"); S("tan x", "sqrt(3)", "60, 240"); S("cos 3x", "0", "30, 90, 150, 210, 270, 330");
    S("sin x", "0.3", "17.4576, 162.542");
    puts("--- identities");
    S("sin^2 x + cos^2 x", "1", "all"); S("1 + tan^2 x", "sec^2 x", "all"); S("[sin x|1 + cos x]", "csc x - cot x", "all");
    S("sin 2x", "2 sin x cos x", "all"); S("sin x", "cos x", "45, 225");
    mx_degrees = false;
    puts("--- trig (radians)");
    S("sin x", "[sqrt(3)|2]", "[pi|3], [2pi|3]"); S("cos x", "0", "[pi|2], [3pi|2]"); S("2sin x", "1", "[pi|6], [5pi|6]"); S("tan x", "1", "[pi|4], [5pi|4]");
    S("sin x", "-1", "[3pi|2]");
    printf(fails ? "%d FAILED\n" : "solve: all passed\n", fails);
    return fails != 0;
}
