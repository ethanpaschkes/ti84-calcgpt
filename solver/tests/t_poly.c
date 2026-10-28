#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mathx.h"
#include "poly.h"
static int fails = 0;
static void P(const char *expr, const char *want_text, const char *want_roots, const char *want_factor)
{
    struct mx_prog pr;
    struct poly po;
    struct poly_roots r;
    char text[100], roots[200] = "", fac[100] = "", tmp[50];
    int i;
    if (!mx_compile(expr, 0, &pr)) { printf("FAIL compile %s\n", expr); fails++; return; }
    if (!poly_from_progs(&pr, NULL, &po)) { printf("FAIL not polynomial: %s\n", expr); fails++; return; }
    poly_text(&po, text, sizeof text);
    poly_solve(&po, &r);
    for (i = 0; i < r.nrat; i++) {
        if (r.rat[i].d == 1) snprintf(tmp, sizeof tmp, "%lld%s ", (long long)r.rat[i].n, r.mult[i] > 1 ? "^2" : "");
        else snprintf(tmp, sizeof tmp, "%lld/%lld ", (long long)r.rat[i].n, (long long)r.rat[i].d);
        strcat(roots, tmp);
    }
    for (i = 0; i < r.nquad; i++) { strcat(roots, r.quad_text[i]); strcat(roots, " "); }
    if (r.nquad < 0 && r.rest.deg == 2) strcat(roots, "norealquad ");
    if (r.rest.deg >= 3) strcat(roots, "rest3+ ");
    poly_factor_text(&po, &r, fac, sizeof fac);
    if (strcmp(text, want_text) || strcmp(roots, want_roots) || (want_factor && strcmp(fac, want_factor)))
        { printf("FAIL %s\n   text   '%s' want '%s'\n   roots  '%s' want '%s'\n   factor '%s' want '%s'\n", expr, text, want_text, roots, want_roots, fac, want_factor ? want_factor : "(any)"); fails++; }
}
int main(void)
{
    P("x^2 - 4x + 3", "x^2 - 4x + 3", "1 3 ", "(x - 1)(x - 3)");
    P("2x^2 + x - 1", "2x^2 + x - 1", "-1 1/2 ", "(x + 1)(2x - 1)");
    P("x^2 - 2", "x^2 - 2", "-sqrt(2) sqrt(2) ", NULL);
    P("x^2 - 2x - 4", "x^2 - 2x - 4", "1 - sqrt(5) 1 + sqrt(5) ", NULL);
    P("x^2 + x - 1", "x^2 + x - 1", "[-1 - sqrt(5)|2] [-1 + sqrt(5)|2] ", NULL);
    P("2x^2 - 3x - 1", "2x^2 - 3x - 1", "[3 - sqrt(17)|4] [3 + sqrt(17)|4] ", NULL);
    P("x^2 + 1", "x^2 + 1", "norealquad ", NULL);
    P("x^2 - 6x + 9", "x^2 - 6x + 9", "3^2 ", "(x - 3)(x - 3)");
    P("x^3 - 6x^2 + 11x - 6", "x^3 - 6x^2 + 11x - 6", "1 2 3 ", "(x - 1)(x - 2)(x - 3)");
    P("(x - 1)^2 (x + 2)", "x^3 - 3x + 2", "1^2 -2 ", "(x - 1)(x - 1)(x + 2)");
    P("[1|2]x^2 - 2", "[1|2]x^2 - 2", "-2 2 ", "[1|2](x + 2)(x - 2)");
    P("x^2 - x^2", "0", "", NULL);
    P("3x - 6", "3x - 6", "2 ", "3(x - 2)");
    P("x^3 - 2", "x^3 - 2", "rest3+ ", NULL);
    P("x^2 - 4", "x^2 - 4", "-2 2 ", "(x + 2)(x - 2)");
    P("4x^2 - 9", "4x^2 - 9", "-3/2 3/2 ", NULL);
    P("x(x - 5)", "x^2 - 5x", "0 5 ", NULL);
    P("(x^2 - 1)(x + 3)", "x^3 + 3x^2 - x - 3", "-1 1 -3 ", "(x + 1)(x - 1)(x + 3)");
    { struct mx_prog pr; struct poly po; const char *non[] = {"sin x", "1/x", "sqrt x", "abs x", "2^x", "x^7", "ln x"}; int i;
      for (i = 0; i < 7; i++) { mx_compile(non[i], 0, &pr); if (poly_from_progs(&pr, NULL, &po)) { printf("FAIL treated as polynomial: %s\n", non[i]); fails++; } } }
    printf(fails ? "%d FAILED\n" : "poly: all passed\n", fails);
    return fails != 0;
}
