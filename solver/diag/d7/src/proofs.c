#include "proofs.h"
#include "mathx.h"
#include <math.h>
#include "mreal.h"
#include <string.h>

struct proof {
    const char *lhs, *rhs, *steps;
};

static const struct proof proofs[8] = {
    {"sin^4 x - cos^4 x", "sin^2 x - cos^2 x",
     "Proof 1:\nLeft = (sin^2 x - cos^2 x)(sin^2 x + cos^2 x)  [difference of squares]\n= (sin^2 x - cos^2 x)(1)  [Pythagorean identity]\n= sin^2 x - cos^2 x = right side."},
    {"1 - cos^2 x/(1 + sin x)", "sin x",
     "Proof 2:\ncos^2 x = 1 - sin^2 x = (1 - sin x)(1 + sin x)\nSo cos^2 x/(1 + sin x) = 1 - sin x\nLeft = 1 - (1 - sin x) = sin x."},
    {"sin x/(1 + cos x)", "csc x - cot x",
     "Proof 3:\nMultiply the left by (1 - cos x)/(1 - cos x):\n= sin x (1 - cos x)/(1 - cos^2 x)\n= sin x (1 - cos x)/sin^2 x = (1 - cos x)/sin x\n= 1/sin x - cos x/sin x = csc x - cot x."},
    {"(1 + tan x)/sin x", "csc x + sec x",
     "Proof 4:\nSplit it: 1/sin x + tan x/sin x\n= csc x + (sin x/cos x)(1/sin x)\n= csc x + 1/cos x = csc x + sec x."},
    {"((sec x - tan x)^2 + 1)/(csc x (sec x - tan x))", "2 tan x",
     "Proof 5:\nTop = sec^2 - 2 sec tan + tan^2 + 1. Since tan^2 + 1 = sec^2, top = 2 sec^2 - 2 sec tan = 2 sec (sec - tan).\nCancel (sec - tan): 2 sec/csc = 2 (1/cos)/(1/sin) = 2 sin/cos = 2 tan x."},
    {"(1 - cos x)/(1 + cos x)", "(csc x - cot x)^2",
     "Proof 6:\nWork on the right: (1/sin x - cos x/sin x)^2 = (1 - cos x)^2/sin^2 x\n= (1 - cos x)^2/((1 - cos x)(1 + cos x))\n= (1 - cos x)/(1 + cos x) = left side."},
    {"tan^2 x - sin^2 x", "sin^2 x tan^2 x",
     "Proof 7:\nLeft = sin^2/cos^2 - sin^2 = sin^2 (1/cos^2 - 1)\n= sin^2 ((1 - cos^2)/cos^2) = sin^2 (sin^2/cos^2)\n= sin^2 x tan^2 x."},
    {"(sin^3 x + cos^3 x)/(sin x + cos x)", "1 - sin x cos x",
     "Proof 8:\nSum of cubes: a^3 + b^3 = (a + b)(a^2 - ab + b^2)\nTop = (sin x + cos x)(sin^2 x - sin x cos x + cos^2 x)\nCancel (sin x + cos x): sin^2 + cos^2 - sin cos = 1 - sin x cos x."},
};

static bool close(real a, real b)
{
    return a == a && b == b && fabs(a - b) <= 1e-9L * (1.0L + fabs(a) + fabs(b));
}

int mx_find_proof(const char *lhs, const char *rhs, const char **steps)
{
    static const real pts[] = {0.4L, 1.9L, 7.3L, 23.7L};
    struct mx_prog L, R, pl, pr;
    int k, i;

    if (!mx_compile(lhs, 0, &L) || !mx_compile(rhs, 0, &R)) return 0;
    for (k = 0; k < 8; k++) {
        bool same = true, swapped = true;
        if (!mx_compile(proofs[k].lhs, 0, &pl) || !mx_compile(proofs[k].rhs, 0, &pr)) continue;
        for (i = 0; i < 4; i++) {
            real a = mx_run(&L, pts[i]), b = mx_run(&R, pts[i]);
            real pa = mx_run(&pl, pts[i]), pb = mx_run(&pr, pts[i]);
            if (!(close(a, pa) && close(b, pb))) same = false;
            if (!(close(a, pb) && close(b, pa))) swapped = false;
        }
        if (same || swapped) {
            *steps = proofs[k].steps;
            return k + 1;
        }
    }
    return 0;
}
