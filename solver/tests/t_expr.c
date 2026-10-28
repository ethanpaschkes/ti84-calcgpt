#include <stdio.h>
#include <string.h>
#include "expr.h"
#include "mathx.h"
#include "proofs.h"
static int fails = 0;
static void V(const char *s, const char *want_exact, const char *want_dec)
{
    struct mx_eval e;
    mx_evaluate(s, &e);
    if (e.kind == EK_ERROR || strcmp(e.exact, want_exact) || (want_dec && strcmp(e.dec, want_dec))) {
        printf("FAIL %s\n   exact '%s' want '%s'  dec '%s' want '%s' (kind %d, note %s)\n", s, e.exact, want_exact, e.dec, want_dec ? want_dec : "(any)", e.kind, e.note);
        fails++;
    }
}
static void Pf(const char *l, const char *r, int want)
{
    const char *st = 0;
    int got = mx_find_proof(l, r, &st);
    if (got != want) { printf("FAIL proof %s = %s : got %d want %d\n", l, r, got, want); fails++; }
}
int main(void)
{
    mx_degrees = true;
    V("[3|4] + [5|6]", "[19|12]", "1.58333"); V("18/24", "[3|4]", NULL); V("sqrt(8)", "2sqrt(2)", NULL); V("sin(60)", "[sqrt(3)|2]", NULL);
    V("cos(45)", "[sqrt(2)|2]", NULL); V("2^10", "1024", NULL); V("[1|3] + [1|6]", "[1|2]", NULL); V("sqrt(2)*sqrt(8)", "4", NULL); V("[2|3]*[9|4]", "[3|2]", NULL);
    V("tan(30)", "[sqrt(3)|3]", NULL); V("100 / 7", "[100|7]", "14.2857"); V("(1+sqrt(5))/2", "[1 + sqrt(5)|2]", NULL); V("3 - 2sqrt(2)", "3 - 2sqrt(2)", NULL); V("(3 - sqrt(7))/4", "[3 - sqrt(7)|4]", NULL);
    V("[x^2 - 1|x - 1]", "x + 1", NULL); V("[x^2 - 4|x + 2]", "x - 2", NULL); V("[2x + 4|4x + 8]", "[1|2]", NULL); V("[x^2 + 3x + 2|x + 2]", "x + 1", NULL);
    V("[x^2 - 1|x^2 - 2x + 1]", "[x + 1|x - 1]", NULL); V("[2x|4x + 2]", "[x|2x + 1]", NULL);
    V("(x+1)(x-3)", "x^2 - 2x - 3", NULL); V("(x+2)^2", "x^2 + 4x + 4", NULL);
    { struct mx_eval e; mx_evaluate("sin x", &e); if (e.kind != EK_OTHER) { printf("FAIL 'sin x' should be other\n"); fails++; } }
    puts("--- proofs");
    Pf("sin^4 x - cos^4 x", "sin^2 x - cos^2 x", 1); Pf("1 - [cos^2 x|1 + sin x]", "sin x", 2); Pf("[sin x|1 + cos x]", "csc x - cot x", 3);
    Pf("[1 + tan x|sin x]", "csc x + sec x", 4); Pf("((sec x - tan x)^2 + 1)/(csc x (sec x - tan x))", "2tan x", 5);
    Pf("[1 - cos x|1 + cos x]", "(csc x - cot x)^2", 6); Pf("tan^2 x - sin^2 x", "sin^2 x tan^2 x", 7); Pf("[sin^3 x + cos^3 x|sin x + cos x]", "1 - sin x cos x", 8);
    Pf("csc x - cot x", "sin x/(1 + cos x)", 3); Pf("sin x", "cos x", 0); Pf("sin^2 x + cos^2 x", "1", 0);
    printf(fails ? "%d FAILED\n" : "expr+proofs: all passed\n", fails);
    return fails != 0;
}
