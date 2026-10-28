#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mathx.h"
static int fails = 0;
static void E(const char *s, double x, double want)
{
    struct mx_prog p;
    double v;
    if (!mx_compile(s, 0, &p)) { printf("FAIL compile '%s'\n", s); fails++; return; }
    v = mx_run(&p, x);
    if (!(fabs(v - want) < 1e-5 * (1 + fabs(want)))) { printf("FAIL '%s' @%g = %.12g want %.12g\n", s, x, v, want); fails++; }
}
int main(void)
{
    mx_degrees = true;
    E("sin(30)", 0, 0.5); E("sin 30", 0, 0.5); E("sin x", 30, 0.5); E("sin 2x", 15, 0.5); E("2sin x", 30, 1.0);
    E("sin^2(x)+cos^2(x)", 33, 1.0); E("sin^2 x + cos^2 x", 41, 1.0); E("sin x cos x", 30, 0.5 * sqrt(3) / 2);
    E("sin x / cos x", 45, 1.0); E("tan x", 45, 1.0); E("csc x", 30, 2.0); E("sec x", 60, 2.0); E("cot x", 45, 1.0);
    E("1 + tan^2 x", 30, 1 / (cos(30 * M_PI / 180) * cos(30 * M_PI / 180)));
    E("[sin x|1+cos x]", 60, (sqrt(3) / 2) / 1.5); E("(sin x)/(1+cos x)", 60, (sqrt(3) / 2) / 1.5);
    E("x^2 - 4x + 3", 2, -1); E("[1|2]x", 4, 2); E("3(x+1)", 1, 6); E("-x^2", 3, -9); E("2^x", 3, 8);
    E("sqrt(x)", 16, 4); E("sqrt 16", 0, 4); E("abs(x-5)", 2, 3); E("ln(e)", 0, 1); E("log 1000", 0, 3); E("pi", 0, M_PI);
    E("arcsin(0.5)", 0, 30); E("acos 0.5", 0, 60); E("atan 1", 0, 45);
    E("sin x + cos x", 45, sqrt(2));
    E("(sec x - tan x)^2", 30, pow(1 / cos(30 * M_PI / 180) - tan(30 * M_PI / 180), 2));
    mx_degrees = false;
    E("sin(pi/2)", 0, 1.0); E("cos x", M_PI, -1.0); E("sin x", M_PI / 6, 0.5);
    { struct mx_prog p; if (mx_compile("sin(", 0, &p) || mx_compile("2+", 0, &p) || mx_compile("y+1", 0, &p) || mx_compile("[1|]", 0, &p)) { printf("FAIL bad input accepted\n"); fails++; } }
    printf(fails ? "%d FAILED\n" : "mathx: all passed\n", fails);
    return fails != 0;
}
