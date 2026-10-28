#include <stdio.h>
#include <string.h>
#include "triangle.h"
static int fails = 0;
static void T(const char *name, struct tri_in in, const char *must)
{
    char out[2000];
    bool ok = tri_work(&in, out, sizeof out);
    int bad = must && !strstr(out, must);
    printf("--- %s %s\n%s\n\n", name, ok ? "" : "(refused)", out);
    if (bad) { printf("   ^^^ FAIL expected '%s'\n\n", must); fails++; }
}
static struct tri_in mk(const char *spec)
{
    /* spec like "a6 b8 c11" or "A53 C13 b12.8" */
    struct tri_in t;
    const char *p = spec;
    memset(&t, 0, sizeof t);
    while (*p) {
        char k = *p++;
        double v = 0, sc = 0.1;
        int dot = 0;
        while ((*p >= '0' && *p <= '9') || *p == '.') {
            if (*p == '.') dot = 1; else if (!dot) v = v * 10 + (*p - '0'); else { v += (*p - '0') * sc; sc /= 10; }
            p++;
        }
        while (*p == ' ') p++;
        if (k == 'K') { t.has_area = true; t.area = v; }
        if (k == 'P') { t.has_perim = true; t.perim = v; }
        if (k >= 'a' && k <= 'c') { t.has_s[k - 'a'] = true; t.s[k - 'a'] = v; }
        if (k >= 'A' && k <= 'C') { t.has_a[k - 'A'] = true; t.a[k - 'A'] = v; }
    }
    return t;
}
int main(void)
{
    /* area or perimeter in place of a missing part */
    T("AREA two sides: a=10 b=12 K=40 (two triangles: C = 41.8 or 138.2)", mk("a10 b12 K40"), "two triangles fit");
    T("AREA two sides: sin C = 2K/(ab)", mk("a10 b12 K40"), "C = 41.8");
    T("AREA two sides, obtuse one", mk("a10 b12 K40"), "C = 138.2");
    T("AREA right angle in the middle: a=6 b=8 K=24 (6-8-10)", mk("a6 b8 K24"), "c = 10");
    T("AREA too big for two sides", mk("a3 b4 K7"), "too big");
    T("AREA, side and adjacent angle: b=8 A=30 K=12 gives c=6", mk("b8 A30 K12"), "c = 6");
    T("AREA, side and its opposite angle: a=5 A=90 K=6 (3-4-5)", mk("a5 A90 K6"), "a = 5");
    T("AREA, two angles: A=60 B=60 K=43.30 (equilateral side 10)", mk("A60 B60 K43.3013"), "a = 10");
    T("AREA, only one other value", mk("a5 K6"), "two other values");
    T("AREA not needed with three parts", mk("a3 b4 c5 K6"), "not needed");
    T("PERIMETER, two sides: a=3 b=4 P=12", mk("a3 b4 P12"), "c = 5");
    T("PERIMETER, right triangle leg: a=3 C=90 P=12", mk("a3 C90 P12"), "c = 5");
    T("PERIMETER, right triangle hyp: c=5 C=90 P=12", mk("c5 C90 P12"), "a = 4");
    T("PERIMETER alone", mk("a3 P12"), "also give two sides");
    T("RIGHT area+leg: a=6 C=90 K=24 (b=8)", mk("a6 C90 K24"), "b = 8");
    T("RIGHT area+hyp: c=10 C=90 K=24 (6-8-10)", mk("c10 C90 K24"), "a = ");
    T("RIGHT area+angle: A=30 C=90 K=8.66 (a=2.9? legs)", mk("A30 C90 K8.66"), "B = 60");
    T("REVIEW #4 a=6 b=8 c=11, find A (32.2)", mk("a6 b8 c11"), "A = 32.2");
    T("REVIEW #5 a=6 b=8 A=15.2, find C (144.3 or 5.3)", mk("a6 b8 A15.2"), "C = 144.3");
    T("REVIEW #8 a=9 b=10 c=16 area (40.908)", mk("a9 b10 c16"), "40.908");
    T("REVIEW #10 a=6 b=8 A=20 (two triangles; c = 12.857 and 2.178)", mk("a6 b8 A20"), "12.857");
    T("REVIEW #7 forces 12 and 13 at 80 -> triangle angle 100: angle from weaker force (41.9)", mk("a12 b13 C100"), "B = 41.9");
    T("REVIEW #6 forces 9, 14 at 55 -> included 125, resultant 20.5", mk("a9 b14 C125"), "c = 20.5");
    T("CIRCUIT A=53 C=13 b=12.8 find c (3.2)", mk("A53 C13 b12.8"), "c = 3.1519");
    T("CIRCUIT C=112 b=10 a=16.2 find c (22)", mk("C112 b10 a16.2"), "c = 21.996");
    T("CIRCUIT A=65 a=9.5 b=6 find B (34.9)", mk("A65 a9.5 b6"), "B = 34.9");
    T("CIRCUIT A=30 B=65 a=8.7 find b (15.8)", mk("A30 B65 a8.7"), "b = 15.77");
    T("CIRCUIT a=7 b=7 c=12 find C (118)", mk("a7 b7 c12"), "C = 118");
    T("CIRCUIT B=35 b=7.5 c=12 obtuse C second case (113.4)", mk("B35 b7.5 c12"), "113.4");
    T("CIRCUIT freight trains 30 and 40 mph, 110 deg, after half an hour: sides 15 and 20 (30)", mk("a15 b20 C110"), "c = 28.8");
    T("bad: only angles", mk("A30 B40 C50"), NULL);
    T("no triangle", mk("a1 b1 c5"), NULL);
    printf(fails ? "%d FAILED\n" : "triangle: checks passed\n", fails);
    return fails != 0;
}
