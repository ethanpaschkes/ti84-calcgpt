#include <stdio.h>
#include <string.h>
#include "mathx.h"
#include "preset.h"
static int fails = 0;
static const char *x6 = "", *x7 = ""; /* the 7th and 8th boxes, only the triangle forms use them */
/* every line of `want` (separated by '|') must appear in the answer */
static void T(int which, const char *f0, const char *f1, const char *f2, const char *f3, const char *f4, const char *f5, bool ok, const char *want)
{
    const char *f[8] = {f0, f1, f2, f3, f4, f5, x6, x7};
    static char out[1800];
    char w[300], *tok;
    bool r;
    mx_degrees = true;
    r = presets[which].run(f, out, sizeof out);
    x6 = x7 = "";
    if (r != ok) { printf("FAIL %s: returned %d want %d\n%s\n", presets[which].name, r, ok, out); fails++; return; }
    strcpy(w, want);
    for (tok = strtok(w, "|"); tok; tok = strtok(NULL, "|")) {
        if (!strstr(out, tok)) { printf("FAIL %s: missing '%s' in:\n%s\n", presets[which].name, tok, out); fails++; }
    }
}
int main(void)
{
    /* 0 triangle, 1 right, 2 equation, 3 angle values, 4 other values, 5 convert, 6 arc, 7 polar, 8 wave, 9 quadratic */
    T(0, "7", "8", "9", "", "", "", true, "A = ");
    T(0, "7", "", "", "", "", "", false, "");
    T(0, "-3", "4", "5", "", "", "", false, "positive");
    T(1, "6", "", "10", "", "", "", true, "b = 8|B = 53.1");
    x6 = "40"; T(0, "10", "12", "", "", "", "", true, "C = 41.8|C = 138.2|two triangles fit");
    x7 = "12"; T(0, "3", "4", "", "", "", "", true, "c = 5");
    T(1, "6", "", "", "", "", "24", true, "b = 8|c = 10");
    x6 = "12"; T(1, "", "", "5", "", "", "", true, "a = 4|b = 3");
    x6 = "12"; T(1, "", "", "", "", "", "24", false, "also give two sides");
    T(1, "3", "4", "", "", "", "", true, "c = 5");
    T(1, "3", "", "", "", "", "", false, "exactly two");
    T(1, "", "", "", "30", "60", "", false, "exactly two");
    T(2, "sin", "", "", "", "1/2", "degrees", true, "x = 30|x = 150|360n");
    T(2, "cos", "2", "", "", "1", "degrees", true, "x = 60|x = 300");
    T(2, "cos", "2", "", "", "1/2", "degrees", true, "x = 75.5225");
    T(2, "sin", "", "", "", "1/2", "radians", true, "pi/6|5pi/6");
    T(2, "tan", "", "", "", "1", "degrees", true, "x = 45");
    T(2, "sin", "", "", "", "5", "degrees", true, "No solution");
    T(2, "sin", "", "", "", "", "degrees", false, "right side");
    T(2, "sin", "2", "3", "10", "1", "degrees", true, "x = 6.66667|120n");
    T(3, "150", "degrees", "", "", "", "", true, "sin = 1/2|cos = -sqrt(3)/2|tan = -sqrt(3)/3|Quadrant II|30 degrees");
    T(3, "pi/6", "radians", "", "", "", "", true, "sin = 1/2|cos = sqrt(3)/2|Quadrant I|pi/6 radians");
    T(3, "90", "degrees", "", "", "", "", true, "tan = undefined|cot = 0|on an axis");
    T(3, "", "degrees", "", "", "", "", false, "Type an angle");
    T(5, "150", "deg > rad", "", "", "", "", true, "5pi/6");
    T(5, "pi/6", "rad > deg", "", "", "", "", true, "= 30 degrees");
    T(5, "1", "rad > deg", "", "", "", "", true, "57.2958");
    T(6, "10", "60", "degrees", "", "", "", true, "10pi/3|50pi/3");
    T(6, "5", "2", "radians", "", "", "", true, "arc length s = r x theta = 10|sector area = 1/2 r^2 theta = 25");
    T(6, "0", "2", "radians", "", "", "", false, "positive");
    /* other trig values from one value and the quadrant */
    T(4, "sin", "3/5", "II", "", "", "", true, "cos = -4/5|tan = -3/4|csc = 5/3|sec = -5/4|cot = -4/3");
    T(4, "cos", "-5/13", "III", "", "", "", true, "sin = -12/13|tan = 12/5");
    T(4, "tan", "2", "III", "", "", "", true, "sin = -2sqrt(5)/5|cos = -sqrt(5)/5");
    T(4, "sec", "-3", "II", "", "", "", true, "cos = -1/3|sin = 2sqrt(2)/3");
    T(4, "sin", "3/5", "III", "", "", "", false, "quadrant III");
    T(4, "sin", "2", "I", "", "", "", false, "never bigger");
    T(4, "csc", "1/2", "I", "", "", "", false, "never between");
    T(4, "sin", "0", "I", "", "", "", false, "not 0");
    /* polar and rectangular */
    T(7, "polar > rect", "2", "60", "degrees", "", "", true, "x = r cos(theta) = 1|y = r sin(theta) = sqrt(3)");
    T(7, "rect > polar", "1", "1", "degrees", "", "", true, "r = sqrt(x^2 + y^2) = sqrt(2)|theta = 45 degrees|quadrant I");
    T(7, "rect > polar", "-1", "1", "radians", "", "", true, "3pi/4 radians|quadrant II");
    T(7, "rect > polar", "-3", "-4", "degrees", "", "", true, "r = sqrt(x^2 + y^2) = 5|quadrant III|233.13");
    T(7, "rect > polar", "0", "0", "degrees", "", "", true, "undefined");
    /* amplitude, period, shifts */
    T(8, "sin", "3", "2", "", "1", "degrees", true, "amplitude = |a| = 3|period = 360 / |b| = 180|vertical shift = d = 1|maximum = d + |a| = 4|minimum = d - |a| = -2");
    T(8, "cos", "1", "1", "60", "", "degrees", true, "phase shift = -c / b = -60|(to the left)");
    T(8, "sin", "-2", "pi", "", "", "radians", true, "period = 2pi / |b| = 2|flipped");
    T(8, "tan", "1", "2", "", "", "degrees", true, "period = 180 / |b| = 90|No amplitude");
    T(8, "sin", "", "0", "", "", "degrees", false, "not 0");
    /* quadratic */
    T(9, "1", "-5", "6", "", "", "", true, "x = 2  or  x = 3|discriminant b^2 - 4ac = 1|vertex x = -b / 2a = 5/2|vertex y = -1/4");
    T(9, "1", "0", "-2", "", "", "", true, "sqrt(2)|discriminant b^2 - 4ac = 8");
    T(9, "1", "2", "5", "", "", "", true, "No solution|discriminant b^2 - 4ac = -16");
    T(9, "0", "2", "5", "", "", "", false, "can not be 0");
    printf(fails ? "%d FAILED\n" : "preset: all passed\n", fails);
    return fails != 0;
}
